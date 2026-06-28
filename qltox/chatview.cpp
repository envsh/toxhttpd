#include "identicon.h"
#include "avatar_manager.h"
#include "chatview.h"
#include "photoviewer.h"
#include "LimeScrollBar.h"
#include "LimeStyle.h"
#ifdef EMOJI_RENDER_QT34
#include "emojiutil.h"
#endif
#include <algorithm>
#include <cstdlib>
#include "translator.h"

// ── Media display sizing ──
static const int kMaxMediaDim = 260;
static const int kMinMediaH = 50;

#ifdef QT3_BUILD
#include <qpainter.h>
#include <qimage.h>
#include <qprocess.h>
#include <qregexp.h>
#include <qstringlist.h>
#include <qcursor.h>
#include <qclipboard.h>
#else
#include <QPainter>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QClipboard>
#include <QMenu>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QRegExp>
#include <QToolTip>
#endif

// ───── WebP fallback decode (dlopen libwebp) ─────
#include <dlfcn.h>
#include <cstdint>

static bool isWebP(const std::string& d) {
    return d.size() >= 12 &&
           d[0]=='R'&&d[1]=='I'&&d[2]=='F'&&d[3]=='F' &&
           d[8]=='W'&&d[9]=='E'&&d[10]=='B'&&d[11]=='P';
}

static QPixmap decodeWebP(const std::string& data) {
    if (!isWebP(data) || data.size() < 12) return QPixmap();
    void* lib = dlopen("libwebp.so", RTLD_LAZY);
    if (!lib) { qWarning("dlopen libwebp: %s", dlerror()); return QPixmap(); }
    using GetInfo  = int (*)(const uint8_t*, size_t, int*, int*);
    using DecodeRGB = uint8_t* (*)(const uint8_t*, size_t, int*, int*);
    auto gi   = (GetInfo)dlsym(lib, "WebPGetInfo");
    auto drgb = (DecodeRGB)dlsym(lib, "WebPDecodeRGB");
    if (!gi || !drgb) { dlclose(lib); return QPixmap(); }
    int w=0, imgH=0;
    if (!gi((const uint8_t*)data.data(), data.size(), &w, &imgH) || w<=0 || imgH<=0)
        { dlclose(lib); return QPixmap(); }
    uint8_t* rgb = drgb((const uint8_t*)data.data(), data.size(), &w, &imgH);
    if (!rgb) { dlclose(lib); return QPixmap(); }
    // Qt3/Qt4 no Format_RGB888; expand 24-bit RGB → 32-bit XRGB
#ifdef QT3_BUILD
    QImage img(w, imgH, 32);
#else
    QImage img(w, imgH, QImage::Format_RGB32);
#endif
    for (int y = 0; y < imgH; y++) {
        uint32_t* dst = (uint32_t*)img.scanLine(y);
        const uint8_t* src = rgb + y * w * 3;
        for (int x = 0; x < w; x++) {
            dst[x] = 0xFF000000 | (src[0] << 16) | (src[1] << 8) | src[2];
            src += 3;
        }
    }
    free(rgb); dlclose(lib);
#ifdef QT3_BUILD
    return QPixmap(img);
#else
    return QPixmap::fromImage(img);
#endif
}

#ifdef EMOJI_RENDER_QT34
// ───── Emoji icon helpers ─────
static uint32_t fileIconForName(const QString& name) {
    QString ext;
    int dot = -1;
    for (int i = 0; i < name.length(); i++) {
        if (name[i] == '.') { dot = i; }
    }
    if (dot >= 0) {
        for (int i = dot + 1; i < name.length(); i++) {
#ifdef QT3_BUILD
            ext += name[i].lower();
#else
            ext += name[i].toLower();
#endif
        }
    }
    if (ext == "jpg" || ext == "jpeg" || ext == "png" ||
        ext == "gif" || ext == "webp" || ext == "svg")
        return 0x1F5BC;   // 🖼
    if (ext == "mp3" || ext == "wav" || ext == "flac" ||
        ext == "ogg" || ext == "m4a" || ext == "wma")
        return 0x1F3B5;   // 🎵
    if (ext == "mp4" || ext == "mkv" || ext == "avi" ||
        ext == "mov" || ext == "webm")
        return 0x1F3AC;   // 🎬
    if (ext == "zip" || ext == "tar" || ext == "gz" ||
        ext == "bz2" || ext == "xz" || ext == "7z" || ext == "rar")
        return 0x1F4E6;   // 📦
    if (ext == "pdf")          return 0x1F4D5;   // 📕
    if (ext == "doc" || ext == "docx")  return 0x1F4DD;   // 📝
    if (ext == "xls" || ext == "xlsx")  return 0x1F4CA;   // 📊
    if (ext == "txt" || ext == "json" || ext == "xml" ||
        ext == "html" || ext == "js" || ext == "ts" ||
        ext == "css" || ext == "py" || ext == "go")
        return 0x1F4C4;   // 📄
    return 0x1F4C4;       // 📄 default
}

static void drawEmojiIcon(QPainter& p, const QRect& rect, uint32_t cp, const char* fallback) {
    QPixmap pm = EmojiRenderer::instance().renderEmoji(cp, rect.height() - 2);
    if (!pm.isNull()) {
        int cx = rect.x() + (rect.width() - pm.width()) / 2;
        int cy = rect.y() + (rect.height() - pm.height()) / 2;
        p.drawPixmap(cx, cy, pm);
    } else {
        p.drawText(rect, Qt::AlignCenter, qFromUtf8(fallback));
    }
}

// ───── Download button helper ─────
static QString formatFileSize(int bytes) {
    if (bytes <= 0) return QString();
    if (bytes < 1024) return QString::number(bytes) + qFromUtf8(" B");
    double kb = bytes / 1024.0;
    if (kb < 1024.0) {
        return QString::number(kb, 'f', 1) + qFromUtf8(" KB");
    }
    double mb = kb / 1024.0;
    return QString::number(mb, 'f', 1) + qFromUtf8(" MB");
}

struct DownloadBarInfo {
    QRect downloadBtn;
    QRect retryBtn;
};

static DownloadBarInfo paintDownloadStatusBar(QPainter& p, const QRect& parentRect,
    const QFont& baseFont, const StyleParams::Palette& pal,
    ChatElement::DownloadState state, int fileSize)
{
    const int kPad = 8, btnH = 26, retryBtnH = 22;
    QFont bf = baseFont; bf.setPointSize(10); bf.setBold(true);
    QFontMetrics bfm(bf);

    // ── Build text for each element ──
    QString statusText;
    QColor statusColor = pal.textMuted;
    if (state == ChatElement::NotRequested) {
        statusText = qFromUtf8("待下载");
    } else if (state == ChatElement::Completed) {
        statusText = qFromUtf8("✓ 已下载");
        statusColor = QColor(80, 180, 80);
    } else if (state == ChatElement::InProgress) {
        statusText = qFromUtf8("⏳ 下载中");
    } else if (state == ChatElement::Failed) {
        statusText = qFromUtf8("✗ 失败");
        statusColor = QColor(200, 50, 50);
    }

    QString dlText = qFromUtf8("⬇ 下载");
    if (fileSize > 0) {
        QString sz = formatFileSize(fileSize);
        if (!sz.isEmpty()) { dlText += qFromUtf8(" ") + sz; }
    }

    QString errText;
    QString retryText;
    if (state == ChatElement::Failed) {
        errText = qFromUtf8("⚠ 下载失败");
        retryText = qFromUtf8("↻ 重试");
    }

    // ── Compute widths ──
    int statusW = statusText.isEmpty() ? 0 : bfm.width(statusText) + 8;
    int dlW     = bfm.width(dlText) + 20;
    int retryW  = retryText.isEmpty() ? 0 : bfm.width(retryText) + 16;
    int errW    = errText.isEmpty() ? 0 : bfm.width(errText);

    // ── Right-to-left layout ──
    int cursor = parentRect.right() - kPad;
    int barY   = parentRect.bottom() - btnH - 4;

    // 1. Status text (rightmost)
    if (!statusText.isEmpty()) {
        cursor -= statusW;
        QRect sr(cursor, barY, statusW, btnH);
        p.setPen(statusColor);
        p.setFont(baseFont);
        p.drawText(sr, Qt::AlignLeft | Qt::AlignVCenter, statusText);
        cursor -= kPad;
    }

    // 2. Download button
    cursor -= dlW;
    QRect dlBtnR(cursor, barY, dlW, btnH);
    p.setBrush(QColor(50, 50, 50));
    p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
    p.drawRoundRect(dlBtnR, 6, 6);
#else
    p.drawRoundedRect(dlBtnR, 6, 6);
#endif
    p.setPen(Qt::white);
    p.setFont(bf);
    p.drawText(dlBtnR, Qt::AlignCenter, dlText);
    cursor -= kPad;

    // 3. Retry button (only on failure)
    QRect retBtnR;
    if (!retryText.isEmpty()) {
        cursor -= retryW;
        int ry = barY + (btnH - retryBtnH) / 2;
        retBtnR = QRect(cursor, ry, retryW, retryBtnH);
        p.setBrush(QColor(50, 50, 50));
        p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
        p.drawRoundRect(retBtnR, 4, 3);
#else
        p.drawRoundedRect(retBtnR, 3, 3);
#endif
        p.setPen(Qt::white);
        QFont rf = baseFont; rf.setPointSize(9); p.setFont(rf);
        p.drawText(retBtnR, Qt::AlignCenter, retryText);
        cursor -= kPad;
    }

    // 4. Error text (only on failure)
    if (!errText.isEmpty()) {
        cursor -= errW;
        p.setPen(QColor(200, 50, 50));
        QFont ef = baseFont; ef.setPointSize(10); p.setFont(ef);
        p.drawText(cursor, barY, errW, btnH,
                   Qt::AlignLeft | Qt::AlignVCenter, errText);
    }

    p.setFont(baseFont);

    DownloadBarInfo info;
    info.downloadBtn = dlBtnR;
    info.retryBtn    = retBtnR;
    return info;
}

// ───── Media rendering helpers ─────

static int paintThumbnail(QPainter& p, const QRect& imgRect,
    const QPixmap& thumb, int mediaW, int mediaH,
    const QFont& baseFont, const QFontMetrics& fm,
    const StyleParams::Palette& pal,
    ChatElement::DownloadState state,
    QRect* downloadBtnOut = nullptr, QRect* retryBtnOut = nullptr,
    int fileSize = 0)
{
    int maxW = imgRect.width(), maxH = imgRect.height();
    int dw, dh;
    if (mediaW > 0 && mediaH > 0) {
        double ratio = (double)mediaH / mediaW;
        dw = std::min(maxW, kMaxMediaDim);
        dh = (int)(dw * ratio);
        if (dh > kMaxMediaDim) { dh = kMaxMediaDim; dw = (int)(kMaxMediaDim / ratio); }
        if (dh < kMinMediaH)   { dh = kMinMediaH;   dw = std::min((int)(kMinMediaH / ratio), kMaxMediaDim); }
    } else {
        dw = std::min(maxW, kMaxMediaDim);
        dh = 200;
    }
    if (!thumb.isNull()) {
        if (thumb.width() == dw && thumb.height() == dh) {
            int ox = imgRect.x() + (maxW - dw) / 2;
            int oy = imgRect.y() + (maxH - dh) / 2;
            p.drawPixmap(ox, oy, thumb);
        } else {
#ifdef QT3_BUILD
        QImage img = thumb.convertToImage();
        QImage scaledImg = img.scale(dw, dh, QImage::ScaleMax);
        QPixmap scaled;
        scaled.convertFromImage(scaledImg);
#else
        QPixmap scaled = thumb.scaled(dw, dh, Qt::KeepAspectRatio, Qt::SmoothTransformation);
#endif
        int ox = imgRect.x() + (maxW - scaled.width()) / 2;
        int oy = imgRect.y() + (maxH - scaled.height()) / 2;
        p.drawPixmap(ox, oy, scaled);
        }
    } else {
        p.setBrush(pal.hoverBg);
        p.setPen(Qt::NoPen);
        int px = imgRect.x() + (maxW - dw) / 2;
        int py = imgRect.y() + (maxH - dh) / 2;
        QRect pr(px, py, dw, dh);
#ifdef QT3_BUILD
        p.drawRoundRect(pr, 4, 4);
#else
        p.drawRoundedRect(pr, 4, 4);
#endif
        p.setPen(pal.textMuted);
        QFont f = baseFont; f.setPointSize(11); p.setFont(f);
        QString dimText = (mediaW > 0 && mediaH > 0)
            ? QString("%1 × %2").arg(mediaW).arg(mediaH) : "?";
        p.drawText(pr, Qt::AlignCenter, dimText);
        p.setFont(baseFont);

    }
    // ── 状态栏（右下角右对齐）──
    DownloadBarInfo bi = paintDownloadStatusBar(p, imgRect, baseFont, pal,
                                                state, fileSize);
    if (downloadBtnOut) { *downloadBtnOut = bi.downloadBtn; }
    if (retryBtnOut)    { *retryBtnOut    = bi.retryBtn; }
    return dh;
}

static void paintMediaContent(QPainter& p, const QRect& bubbleRect,
    ChatElement::ElementType etype,
    const QPixmap& thumbnail, const QString& caption,
    int mediaWidth, int mediaHeight, int durationSec,
    QMovie* movie,
    const QFont& baseFont, const QFontMetrics& fm,
    int emojiW, const StyleParams::Palette& pal,
    ChatElement::DownloadState state,
    QRect* downloadBtnOut = nullptr, QRect* retryBtnOut = nullptr,
    int fileSize = 0)
{
    const int kBubbleHPad = 12, kBubbleVPad = 8, kPad = 8;
    QRect imgRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                  bubbleRect.width() - 2*kBubbleHPad, bubbleRect.height() - 2*kBubbleVPad);

    // Thumbnail / GIF frame
    QPixmap frame;
    if (etype == ChatElement::Gif) {
#ifndef QT3_BUILD
        if (movie && movie->isValid())
            frame = movie->currentPixmap();
#endif
    }
    const QPixmap& src = !frame.isNull() ? frame : thumbnail;
    int imgDispH = paintThumbnail(p, imgRect, src, mediaWidth, mediaHeight, baseFont, fm, pal, state, downloadBtnOut, retryBtnOut, fileSize);

    // GIF badge
    if (etype == ChatElement::Gif) {
        int bw = 30, bh = 16;
        QRect badgeR(imgRect.x() + 4, imgRect.y() + 4, bw, bh);
#ifdef QT3_BUILD
        p.setBrush(QColor(50, 50, 50));
#else
        p.setBrush(QColor(0,0,0,180));
#endif
        p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
        p.drawRoundRect(badgeR, 4, 3);
#else
        p.drawRoundedRect(badgeR, 3, 3);
#endif
        p.setPen(Qt::white);
        QFont bf = baseFont; bf.setPointSize(9); bf.setBold(true); p.setFont(bf);
        p.drawText(badgeR, Qt::AlignCenter, qFromUtf8("GIF"));
        p.setFont(baseFont);
    }

    // Video overlay
    if (etype == ChatElement::Video) {
        int btnSize = 40;
        QRect playRect(imgRect.center().x() - btnSize/2,
                       imgRect.center().y() - btnSize/2,
                       btnSize, btnSize);
#ifdef QT3_BUILD
        p.setBrush(QColor(40, 40, 40));
#else
        p.setBrush(QColor(0,0,0,160));
#endif
        p.setPen(Qt::NoPen);
        p.drawEllipse(playRect);
        // Play triangle via lines + flood fill
        {
#ifdef QT3_BUILD
            QPointArray pts(3);
            pts.setPoint(0, playRect.left() + 14, playRect.top() + 10);
            pts.setPoint(1, playRect.left() + 14, playRect.bottom() - 10);
            pts.setPoint(2, playRect.right() - 10, playRect.center().y());
#else
            QPolygon pts(3);
            pts.setPoint(0, playRect.left() + 14, playRect.top() + 10);
            pts.setPoint(1, playRect.left() + 14, playRect.bottom() - 10);
            pts.setPoint(2, playRect.right() - 10, playRect.center().y());
#endif
            p.setBrush(Qt::white);
            p.setPen(Qt::NoPen);
            p.drawPolygon(pts);
        }

        if (durationSec > 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d:%02d", durationSec / 60, durationSec % 60);
            QString dur = qFromUtf8(buf);
            int bw = fm.width(dur) + 12, bh = fm.lineSpacing() + 4;
            QRect badgeR(imgRect.right() - bw - 4, imgRect.bottom() - bh - 4, bw, bh);
#ifdef QT3_BUILD
            p.setBrush(QColor(50, 50, 50));
#else
            p.setBrush(QColor(0,0,0,180));
#endif
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(badgeR, 4, 3);
#else
            p.drawRoundedRect(badgeR, 3, 3);
#endif
            p.setPen(Qt::white);
            QFont bf = baseFont; bf.setPointSize(10); p.setFont(bf);
            p.drawText(badgeR, Qt::AlignCenter, dur);
            p.setFont(baseFont);
        }
    }

    // Caption
    if (!caption.isEmpty()) {
        int captionY = imgRect.y() + imgDispH + kPad/2;
        p.setPen(pal.textMuted);
        QFont cf = baseFont; cf.setPointSize(10); p.setFont(cf);
        p.drawText(bubbleRect.x() + kBubbleHPad, captionY,
                   bubbleRect.width() - 2*kBubbleHPad, fm.lineSpacing(),
                   Qt::AlignLeft | Qt::AlignVCenter, caption);
        p.setFont(baseFont);
    }
}

static void paintAudioContent(QPainter& p, const QRect& bubbleRect,
    const QString& caption, int durationSec,
    const QFont& baseFont, const QFontMetrics& fm,
    const StyleParams::Palette& pal)
{
    const int kBubbleHPad = 12, kBubbleVPad = 8, kPad = 8;
    int iconSize = 48;
    int iconX = bubbleRect.x() + kBubbleHPad;
    int iconY = bubbleRect.y() + kBubbleVPad;

    p.setBrush(pal.surfaceBg);
    p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
    p.drawRoundRect(iconX, iconY, iconSize, iconSize, 8, 8);
#else
    p.drawRoundedRect(iconX, iconY, iconSize, iconSize, 8, 8);
#endif
    uint32_t cp = fileIconForName(caption);
    QPixmap pm = EmojiRenderer::instance().renderEmoji(cp, iconSize - 4);
    if (!pm.isNull()) {
        int cx = iconX + (iconSize - pm.width()) / 2;
        int cy = iconY + (iconSize - pm.height()) / 2;
        p.drawPixmap(cx, cy, pm);
    } else {
        QFont f = baseFont; f.setPointSize(24); p.setFont(f);
        p.setPen(pal.textMuted);
        p.drawText(iconX, iconY, iconSize, iconSize, Qt::AlignCenter, qFromUtf8("🎵"));
        p.setFont(baseFont);
    }

    int textX = iconX + iconSize + kPad;
    int textW = bubbleRect.right() - kBubbleHPad - textX;
    if (textW < 20) { textW = 20; }
    if (!caption.isEmpty()) {
        p.setPen(pal.textPrimary);
        QFont f = baseFont; f.setPointSize(12); p.setFont(f);
        p.drawText(textX, iconY, textW, fm.lineSpacing(),
                   Qt::AlignLeft | Qt::AlignVCenter, caption);
        p.setFont(baseFont);
    }
    if (durationSec > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d", durationSec / 60, durationSec % 60);
        QString dur = qFromUtf8(buf);
        p.setPen(pal.textMuted);
        QFont f = baseFont; f.setPointSize(10); p.setFont(f);
        p.drawText(textX, iconY + fm.lineSpacing() + kPad/2,
                   textW, fm.lineSpacing(),
                   Qt::AlignLeft | Qt::AlignVCenter, dur);
        p.setFont(baseFont);
    }
}
#endif

// ───── ChatElement methods ─────

int ChatElement::calcHeight(int viewWidth, const QFontMetrics& fm, int emojiW, const QFont& baseFont) {
    switch (etype) {
    case Text: {
        if (viewWidth <= 0) { viewWidth = 400; }

        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;
        int kMsgSpacing = ChatView::kMsgSpacing;

        int contentW = viewWidth - 3 * kPad - kAvatarSize;
        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) { bubbleW = contentW; }
        int bubbleTextWidth = bubbleW - 2 * kBubbleHPad;
        if (bubbleTextWidth < 20) { bubbleTextWidth = 20; }

        int lineCount = 0;
#ifdef EMOJI_RENDER_QT34
        auto cps = toCodepoints(messageText);
        int tLen = (int)cps.size();
        int pos = 0;
        while (pos < tLen) {
            if (cps[pos] == '\n') { lineCount++; pos++; continue; }
            int lineWidth = 0, lastSpace = -1, end = pos;
            while (end < tLen && cps[end] != '\n') {
                int cw = isEmojiChar(cps[end]) ? emojiW : fm.width(QChar(cps[end]));
                lineWidth += cw;
                if (cps[end] == ' ') { lastSpace = end; }
                if (lineWidth >= bubbleTextWidth) {
                    if (lastSpace > pos && end - pos > 10) {
                        end = lastSpace + 1;
                    }
                    break;
                }
                end++;
            }
            lineCount++;
            pos = end;
        }
#else
        int textLen = messageText.length();
        for (int i = 0; i < textLen; ) {
            if (messageText[i] == '\n') {
                lineCount++;
                i++;
                continue;
            }
            int lineWidth = 0;
            int lastSpace = -1;
            int end = i;
            while (end < textLen && messageText[end] != '\n') {
                lineWidth += fm.width(messageText[end]);
                if (messageText[end].isSpace()) lastSpace = end;
                if (lineWidth >= bubbleTextWidth) {
                    if (lastSpace > i && end - i > 10) {
                        end = lastSpace + 1;
                    }
                    break;
                }
                end++;
            }
            lineCount++;
            i = end;
        }
#endif
        if (lineCount < 1) { lineCount = 1; }

        int textHeight = lineCount * fm.lineSpacing();
        int minBubbleH = 2 * kBubbleVPad + fm.lineSpacing();
        int bubbleHeight = 2 * kBubbleVPad + std::max(textHeight, fm.lineSpacing());
        if (bubbleHeight < minBubbleH) { bubbleHeight = minBubbleH; }

        if (showTranslation && !translatedText.isEmpty()) {
            QFont sfmFont = baseFont;
            int pt = std::max(sfmFont.pointSize() - 2, 8);
            sfmFont.setPointSize(pt);
            QFontMetrics sfm(sfmFont);

            int transLineCount = 0;
            int tLen2 = translatedText.length();
            int tPos = 0;
            while (tPos < tLen2) {
                if (translatedText[tPos] == '\n') { transLineCount++; tPos++; continue; }
                int lineWidth = 0;
                int lastSpace = -1;
                int end = tPos;
                while (end < tLen2 && translatedText[end] != '\n') {
                    lineWidth += sfm.width(translatedText[end]);
                    if (translatedText[end].isSpace()) lastSpace = end;
                    if (lineWidth >= bubbleTextWidth) {
                        if (lastSpace > tPos && end - tPos > 10) {
                            end = lastSpace + 1;
                        }
                        break;
                    }
                    end++;
                }
                transLineCount++;
                tPos = end;
            }
            if (transLineCount < 1) { transLineCount = 1; }
            bubbleHeight += kBubbleVPad / 2 + transLineCount * sfm.lineSpacing() + kBubbleVPad + 7;
        }

        int headerHeight = fm.lineSpacing() + kPad;
        int contentHeight = kPad + headerHeight + bubbleHeight;
        int avatarTotal = kPad + kAvatarSize;
        return std::max(contentHeight, avatarTotal) + kMsgSpacing;
    }
    case Image:
    case Gif:
    case Video: {
        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;

        int contentW = viewWidth - 3 * kPad - kAvatarSize;
        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) { bubbleW = contentW; }
        int imgMaxW = bubbleW - 2 * kBubbleHPad;
        int imgDispW, imgDispH;
        if (mediaWidth > 0 && mediaHeight > 0) {
            double ratio = (double)mediaHeight / mediaWidth;
            imgDispW = std::min(imgMaxW, kMaxMediaDim);
            imgDispH = (int)(imgDispW * ratio);
            if (imgDispH > kMaxMediaDim) {
                imgDispH = kMaxMediaDim;
                imgDispW = (int)(kMaxMediaDim / ratio);
            }
            if (imgDispH < kMinMediaH) {
                imgDispH = kMinMediaH;
                imgDispW = std::min((int)(kMinMediaH / ratio), kMaxMediaDim);
            }
        } else {
            imgDispW = std::min(imgMaxW, kMaxMediaDim);
            imgDispH = 200;
        }
        int captionH = caption.isEmpty() ? 0 : fm.lineSpacing() + kPad/2;
        int kDLBtnH = 30;
        int bubbleH = 2 * kBubbleVPad + imgDispH + captionH + kDLBtnH;
        int headerHeight = fm.lineSpacing() + kPad;
        int contentHeight = kPad + headerHeight + bubbleH;
        int avatarTotal = kPad + kAvatarSize;
        return std::max(contentHeight, avatarTotal) + 8;
    }
    case Audio: {
        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;

        int contentW = viewWidth - 3 * kPad - kAvatarSize;
        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) { bubbleW = contentW; }
        int iconSize = 48;
        int textW = bubbleW - 2 * kBubbleHPad - iconSize - kPad;
        if (textW < 20) { textW = 20; }
        int nameLines = 0;
        QString displayText = caption;
        {
            int tLen = displayText.length(), pos = 0;
            while (pos < tLen) {
                if (displayText[pos] == '\n') { nameLines++; pos++; continue; }
                int lineW = 0, lastSpace = -1, end = pos;
                while (end < tLen && displayText[end] != '\n') {
                    lineW += fm.width(displayText[end]);
                    if (displayText[end] == ' ') { lastSpace = end; }
                    if (lineW >= textW) {
                        if (lastSpace > pos && end - pos > 3) { end = lastSpace + 1; }
                        break;
                    }
                    end++;
                }
                nameLines++; pos = end;
            }
        }
        if (nameLines < 1) { nameLines = 1; }
        int textH = nameLines * fm.lineSpacing() + fm.lineSpacing() + kPad/2;
        int kDLBtnH = 30;
        int bubbleH = 2 * kBubbleVPad + std::max(textH + kPad, iconSize) + kDLBtnH;
        if (bubbleH < 2 * kBubbleVPad + iconSize + kDLBtnH) { bubbleH = 2 * kBubbleVPad + iconSize + kDLBtnH; }
        int headerHeight = fm.lineSpacing() + kPad;
        int contentHeight = kPad + headerHeight + bubbleH;
        int avatarTotal = kPad + kAvatarSize;
        return std::max(contentHeight, avatarTotal) + 8;
    }
    case File: {
        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;
        int kMsgSpacing = ChatView::kMsgSpacing;

        if (viewWidth <= 0) { viewWidth = 400; }

        int contentW = viewWidth - 3 * kPad - kAvatarSize;
        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) { bubbleW = contentW; }
        int innerW = bubbleW - 2 * kBubbleHPad;

        int iconSize = 48;
        int textW = innerW - iconSize - ChatView::kPad;
        if (textW < 20) { textW = 20; }

        int nameLines = 0;
        QString displayText = !caption.isEmpty() ? caption
                             : !fileName.isEmpty() ? fileName
                             : messageText;
        {
            int tLen = displayText.length();
            int pos = 0;
            while (pos < tLen) {
                if (displayText[pos] == '\n') { nameLines++; pos++; continue; }
                int lineWidth = 0, lastSpace = -1, end = pos;
                while (end < tLen && displayText[end] != '\n') {
                    lineWidth += fm.width(displayText[end]);
                    if (displayText[end] == ' ') { lastSpace = end; }
                    if (lineWidth >= textW) {
                        if (lastSpace > pos && end - pos > 3) {
                            end = lastSpace + 1;
                        }
                        break;
                    }
                    end++;
                }
                nameLines++;
                pos = end;
            }
        }
        if (nameLines < 1) { nameLines = 1; }

        int subLines = messageText.isEmpty() ? 0 : 1;
        int textHeight = nameLines * fm.lineSpacing()
                       + (subLines > 0 ? kPad/2 + subLines * fm.lineSpacing() : 0);

        int kDLBtnH = 30;
        int bubbleHeight = 2 * kBubbleVPad
                         + std::max(textHeight + kPad, iconSize) + kDLBtnH;
        if (bubbleHeight < 2 * kBubbleVPad + iconSize + kDLBtnH) {
            bubbleHeight = 2 * kBubbleVPad + iconSize + kDLBtnH;
        }

        int headerHeight = fm.lineSpacing() + kPad;
        int contentHeight = kPad + headerHeight + bubbleHeight;
        int avatarTotal = kPad + kAvatarSize;
        return std::max(contentHeight, avatarTotal) + kMsgSpacing;
    }
    }
    return 0;
}

void ChatElement::paint(QPainter& p, int y, int viewWidth, bool isSelected,
                        const std::vector<QRect>& selRects,
                        const QFontMetrics& fm, int emojiW,
                        const QFont& baseFont, const StyleParams::Palette& pal) {
    switch (etype) {
    case Text: {
        QFont f = baseFont;
        int headerH = fm.lineSpacing();
        QRect textRect;
        QRect bubbleRect;

        const int hdrBtnSize = 18;
        const int hdrBtnGap = 4;
        int hdrBtnCnt = 2;
        int hdrBtnAreaW = hdrBtnCnt * hdrBtnSize + (hdrBtnCnt - 1) * hdrBtnGap;

        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;
        int kBubbleRadius = ChatView::kBubbleRadius;
        int kMsgSpacing = ChatView::kMsgSpacing;

        if (category == "self") {
            int ax = viewWidth - kPad - kAvatarSize;
            QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
            p.drawPixmap(ax, y + kPad, av);

            int contentRight = viewWidth - 2 * kPad - kAvatarSize;
            int contentLeft = kPad;
            int bubbleMaxW = contentRight - contentLeft;
            int bubbleW = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);

            int hdrTextRight = contentRight - hdrBtnAreaW - kPad / 2;

            f.setPointSize(11);
            p.setFont(f);
            p.setPen(pal.textMuted);
            QString displayName;
            if (!senderNickname.isEmpty())
                displayName = senderNickname;
            else if (!senderName.isEmpty())
                displayName = senderName;
            else if (peerNumber >= 0)
                displayName = QString("Peer %1").arg(peerNumber);
            else
                displayName = "?";
            displayName = qElideChars(displayName, 23, ElideMiddle);
            int nameW = fm.width(displayName) + fm.width("  ");
            p.drawText(hdrTextRight - nameW, y + kPad, nameW, headerH, Qt::AlignRight | Qt::AlignVCenter, displayName);

            int ipW = 0;
            if (!ipAddress.isEmpty()) {
                f.setPointSize(10);
                p.setFont(f);
                p.setPen(QColor(130, 140, 150));
                ipW = fm.width(ipAddress);
                p.drawText(hdrTextRight - nameW - ipW - kPad/2, y + kPad,
                           ipW, headerH, Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
            }

            f.setPointSize(10);
            p.setFont(f);
            p.setPen(pal.textMuted);
            p.drawText(hdrTextRight - nameW - ipW - kPad/2 - fm.width(time) - kPad/2, y + kPad,
                       fm.width(time), headerH, Qt::AlignRight | Qt::AlignVCenter, time);

            p.setFont(baseFont);

            if (etype != ChatElement::File) {
                int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                int btnX0 = contentRight - hdrBtnAreaW;
                sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                p.setPen(pal.textMuted);
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(sourceBtnRect);
                drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                p.drawEllipse(translateBtnRect);
                drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
            }

            int bubbleX = contentRight - bubbleW;
            int bubbleY = y + kPad + headerH + kPad;
            int bubbleH = height - (kPad + headerH + kPad) - kMsgSpacing;
            bubbleRect = QRect(bubbleX, bubbleY, bubbleW, bubbleH);
            textRect = QRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                             bubbleRect.width() - 2 * kBubbleHPad, bubbleRect.height() - 2 * kBubbleVPad);

            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
        } else {
            int ax = kPad;
            QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
            p.drawPixmap(ax, y + kPad, av);

            int contentX = 2 * kPad + kAvatarSize;
            int contentW = viewWidth - kPad - contentX;
            int bubbleW = (contentW * 80) / 100;
            if (bubbleW < 100) { bubbleW = contentW; }
            int bubbleRight = contentX + bubbleW;

            int hdrTextRight = bubbleRight - hdrBtnAreaW - kPad / 2;

            f.setPointSize(11);
            p.setFont(f);
            p.setPen(pal.textMuted);
            QString displayName;
            if (!senderNickname.isEmpty())
                displayName = senderNickname;
            else if (!senderName.isEmpty())
                displayName = senderName;
            else if (peerNumber >= 0)
                displayName = QString("Peer %1").arg(peerNumber);
            else
                displayName = "?";
            displayName = qElideChars(displayName, 23, ElideMiddle);
            int maxNameW = hdrTextRight - contentX;
            if (maxNameW < 20) { maxNameW = 20; }
            p.drawText(contentX, y + kPad, maxNameW, headerH, Qt::AlignLeft | Qt::AlignVCenter, displayName);

            f.setPointSize(10);
            p.setFont(f);
            int ipEnd = contentX + fm.width(displayName) + kPad;
            if (!ipAddress.isEmpty()) {
                p.setPen(QColor(130, 140, 150));
                int ipW = fm.width(ipAddress);
                int ipMax = hdrTextRight - ipEnd;
                if (ipMax > ipW) { ipMax = ipW; }
                if (ipMax > 0) {
                    p.drawText(ipEnd, y + kPad, ipMax, headerH,
                               Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
                }
                ipEnd += fm.width(ipAddress) + kPad/2;
            }

            p.setPen(pal.textMuted);
            int timeX = ipEnd;
            int timeMaxW = hdrTextRight - timeX;
            if (timeMaxW > 0) {
                p.drawText(timeX, y + kPad, timeMaxW, headerH, Qt::AlignLeft | Qt::AlignVCenter, time);
            }

            p.setFont(baseFont);

            if (etype != ChatElement::File) {
                int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                int btnX0 = bubbleRight - hdrBtnAreaW;
                sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                p.setPen(pal.textMuted);
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(sourceBtnRect);
                drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                p.drawEllipse(translateBtnRect);
                drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
            }

            int bubbleY = y + kPad + headerH + kPad;
            int bubbleH = height - (kPad + headerH + kPad) - kMsgSpacing;
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(contentX, bubbleY, bubbleW, bubbleH);
            textRect = QRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                             bubbleRect.width() - 2 * kBubbleHPad, bubbleRect.height() - 2 * kBubbleVPad);

            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
        }

        // Selection highlight
        if (isSelected && !selRects.empty()) {
            QColor selColor = lerpColor(pal.baseBg, pal.accent, 0.25f);
            p.setPen(Qt::NoPen);
            p.setBrush(selColor);
            for (size_t ri = 0; ri < selRects.size(); ++ri) {
                QRect r = selRects[ri];
#ifdef QT3_BUILD
                r.moveBy(0, y);
#else
                r.translate(0, y);
#endif
                p.drawRect(r);
            }
        }

        // Draw text
        p.setPen(pal.textPrimary);
        p.setFont(baseFont);
#ifdef EMOJI_RENDER_QT34
        EmojiRenderer::instance().drawText(p, textRect, messageText);
#else
#ifdef QT3_BUILD
        p.drawText(textRect, Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop, messageText);
#else
        p.drawText(textRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, messageText);
#endif
#endif

        // Translated text
        if (showTranslation && !translatedText.isEmpty()) {
            int origLineCount = 0;
            int textW = textRect.width();
            if (textW < 20) { textW = 20; }
#ifdef EMOJI_RENDER_QT34
            auto cps = toCodepoints(messageText);
            int tLen = (int)cps.size();
            int pos = 0;
            while (pos < tLen) {
                if (cps[pos] == '\n') { origLineCount++; pos++; continue; }
                int lineWidth = 0, lastSpace = -1, end = pos;
                while (end < tLen && cps[end] != '\n') {
                    int cw = isEmojiChar(cps[end]) ? emojiW : fm.width(QChar(cps[end]));
                    lineWidth += cw;
                    if (cps[end] == ' ') { lastSpace = end; }
                    if (lineWidth >= textW) {
                        if (lastSpace > pos && end - pos > 10) {
                            end = lastSpace + 1;
                        }
                        break;
                    }
                    end++;
                }
                origLineCount++;
                pos = end;
            }
#else
            int tLen3 = messageText.length();
            int pos = 0;
            while (pos < tLen3) {
                if (messageText[pos] == '\n') { origLineCount++; pos++; continue; }
                int lineWidth = 0, lastSpace = -1, end = pos;
                while (end < tLen3 && messageText[end] != '\n') {
                    lineWidth += fm.width(messageText[end]);
                    if (messageText[end].isSpace()) lastSpace = end;
                    if (lineWidth >= textW) {
                        if (lastSpace > pos && end - pos > 10) {
                            end = lastSpace + 1;
                        }
                        break;
                    }
                    end++;
                }
                origLineCount++;
                pos = end;
            }
#endif
            if (origLineCount < 1) { origLineCount = 1; }

            int origTextEndY = textRect.y() + origLineCount * fm.lineSpacing();
            int transY = origTextEndY + kBubbleVPad / 2;
            QRect transRect(textRect.x(), transY,
                            textRect.width(), bubbleRect.bottom() - kBubbleVPad - transY);
            if (transRect.height() > 0) {
                p.setPen(pal.textMuted);
                QFont smallFont = baseFont;
                int pt2 = std::max(smallFont.pointSize() - 2, 8);
                smallFont.setPointSize(pt2);
                p.setFont(smallFont);
                QString displayText = QString::fromUtf8("🌐 ") + translatedText;
#ifdef EMOJI_RENDER_QT34
                EmojiRenderer::instance().drawText(p, transRect, displayText);
#else
#  ifdef QT3_BUILD
                p.drawText(transRect, Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop, displayText);
#  else
                p.drawText(transRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, displayText);
#  endif
#endif
                p.setFont(baseFont);
            }
        }
        break;
    }
    case Image:
    case Video:
    case Gif: {
        QFont f = baseFont;
        int headerH = fm.lineSpacing();
        QRect bubbleRect;

        const int hdrBtnSize = 18, hdrBtnGap = 4;
        int hdrBtnAreaW = 2 * hdrBtnSize + hdrBtnGap;

        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;
        int kBubbleRadius = ChatView::kBubbleRadius;
        int kMsgSpacing = ChatView::kMsgSpacing;

        if (category == "self") {
            int ax = viewWidth - kPad - kAvatarSize;
            QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
            p.drawPixmap(ax, y + kPad, av);
            int contentRight = viewWidth - 2 * kPad - kAvatarSize;
            int contentLeft = kPad;
            int bubbleMaxW = contentRight - contentLeft;
            int bubbleW = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);
            int hdrTextRight = contentRight - kPad / 2;

            f.setPointSize(11); p.setFont(f);
            p.setPen(pal.textMuted);
            QString dname = !senderNickname.isEmpty() ? senderNickname
                          : !senderName.isEmpty() ? senderName
                          : (peerNumber >= 0 ? QString("Peer %1").arg(peerNumber) : "?");
            dname = qElideChars(dname, 23, ElideMiddle);
            int nameW = fm.width(dname);
            p.drawText(hdrTextRight - nameW, y + kPad, nameW, headerH,
                       Qt::AlignRight | Qt::AlignVCenter, dname);
            int ipW = 0;
            if (!ipAddress.isEmpty()) {
                f.setPointSize(10); p.setFont(f);
                p.setPen(QColor(130, 140, 150));
                ipW = fm.width(ipAddress);
                p.drawText(hdrTextRight - nameW - ipW - kPad/2, y + kPad,
                           ipW, headerH, Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
            }
            f.setPointSize(10); p.setFont(f);
            p.setPen(pal.textMuted);
            p.drawText(hdrTextRight - nameW - ipW - kPad/2 - fm.width(time) - kPad/2,
                       y + kPad, fm.width(time), headerH,
                       Qt::AlignRight | Qt::AlignVCenter, time);
            p.setFont(baseFont);

            if (etype != ChatElement::File) {
                int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                int btnX0 = contentRight - hdrBtnAreaW;
                sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                p.setPen(pal.textMuted); p.setBrush(Qt::NoBrush);
                p.drawEllipse(sourceBtnRect);
                drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                p.drawEllipse(translateBtnRect);
                drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
            }

            int bubbleX = contentRight - bubbleW;
            int bubbleY = y + kPad + headerH + kPad;
            int bubbleH = height - (kPad + headerH + kPad) - kMsgSpacing;
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(bubbleX, bubbleY, bubbleW, bubbleH);
            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
        } else {
            int ax = kPad;
            QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
            p.drawPixmap(ax, y + kPad, av);
            int contentX = 2 * kPad + kAvatarSize;
            int contentW = viewWidth - kPad - contentX;
            int bubbleW = (contentW * 80) / 100;
            if (bubbleW < 100) { bubbleW = contentW; }
            int bubbleRight = contentX + bubbleW;
            int hdrTextRight = bubbleRight - kPad / 2;

            f.setPointSize(11); p.setFont(f);
            p.setPen(pal.textMuted);
            QString dname = !senderNickname.isEmpty() ? senderNickname
                          : !senderName.isEmpty() ? senderName
                          : (peerNumber >= 0 ? QString("Peer %1").arg(peerNumber) : "?");
            dname = qElideChars(dname, 23, ElideMiddle);
            int maxNameW = hdrTextRight - contentX;
            if (maxNameW < 20) { maxNameW = 20; }
            p.drawText(contentX, y + kPad, maxNameW, headerH,
                       Qt::AlignLeft | Qt::AlignVCenter, dname);
            f.setPointSize(10); p.setFont(f);
            int ipEnd = contentX + fm.width(dname) + kPad;
            if (!ipAddress.isEmpty()) {
                p.setPen(QColor(130, 140, 150));
                int ipW = fm.width(ipAddress);
                int ipMax = hdrTextRight - ipEnd;
                if (ipMax > ipW) { ipMax = ipW; }
                if (ipMax > 0) {
                    p.drawText(ipEnd, y + kPad, ipMax, headerH,
                               Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
                }
                ipEnd += fm.width(ipAddress) + kPad/2;
            }
            p.setPen(pal.textMuted);
            int timeMaxW = hdrTextRight - ipEnd;
            if (timeMaxW > 0) {
                p.drawText(ipEnd, y + kPad, timeMaxW, headerH,
                           Qt::AlignLeft | Qt::AlignVCenter, time);
            }
            p.setFont(baseFont);

            if (etype != ChatElement::File) {
                int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                int btnX0 = bubbleRight - hdrBtnAreaW;
                sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                p.setPen(pal.textMuted); p.setBrush(Qt::NoBrush);
                p.drawEllipse(sourceBtnRect);
                drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                p.drawEllipse(translateBtnRect);
                drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
            }

            int bubbleY = y + kPad + headerH + kPad;
            int bubbleH = height - (kPad + headerH + kPad) - kMsgSpacing;
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(contentX, bubbleY, bubbleW, bubbleH);
            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
        }

        // Selection highlight
        if (isSelected && !selRects.empty()) {
            QColor selColor = lerpColor(pal.baseBg, pal.accent, 0.25f);
            p.setPen(Qt::NoPen);
            p.setBrush(selColor);
            for (size_t ri = 0; ri < selRects.size(); ++ri) {
                QRect r = selRects[ri];
#ifdef QT3_BUILD
                r.moveBy(0, y);
#else
                r.translate(0, y);
#endif
                p.drawRect(r);
            }
        }

        {
            int kBubbleHPad = 12, kBubbleVPad = 8;
            thumbnailRect = QRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                                  bubbleRect.width() - 2*kBubbleHPad, bubbleRect.height() - 2*kBubbleVPad);
        }
        // WebP fallback: decode raw bytes to thumbnail
        if (thumbnail.isNull() && !rawFileData.empty() && isWebP(rawFileData)) {
            QPixmap wp = decodeWebP(rawFileData);
            if (!wp.isNull()) {
                mediaWidth = wp.width();
                mediaHeight = wp.height();
#ifdef QT3_BUILD
                {
                    QImage tmpImg = wp.convertToImage();
                    QImage scaledImg = tmpImg.smoothScale(thumbnailRect.width(), thumbnailRect.height(), QImage::ScaleMax);
                    thumbnail.convertFromImage(scaledImg);
                }
#else
                thumbnail = wp.scaled(thumbnailRect.width(), thumbnailRect.height(),
                                      Qt::KeepAspectRatio, Qt::SmoothTransformation);
#endif
            }
        }
        // 预缩放缓存：避免每帧重新缩放全分辨率图片
        QPixmap displayPixmap = thumbnail;
        if (!thumbnail.isNull() && mediaWidth > 0 && mediaHeight > 0) {
            int maxW = thumbnailRect.width();
            // dw/dh 计算必须与 paintThumbnail() 一致，否则每帧 rescale
            int dw, dh;
            if (mediaWidth > 0 && mediaHeight > 0) {
                double ratio = (double)mediaHeight / mediaWidth;
                dw = std::min(maxW, kMaxMediaDim);
                dh = (int)(dw * ratio);
                if (dh > kMaxMediaDim) { dh = kMaxMediaDim; dw = (int)(kMaxMediaDim / ratio); }
                if (dh < kMinMediaH)   { dh = kMinMediaH;   dw = std::min((int)(kMinMediaH / ratio), kMaxMediaDim); }
            } else {
                dw = std::min(maxW, kMaxMediaDim);
                dh = 200;
            }
            if (dw != scaledForDispW || dh != scaledForDispH) {
#ifdef QT3_BUILD
                {
                    QImage img = thumbnail.convertToImage();
                    QImage scaledImg = img.smoothScale(dw, dh, QImage::ScaleMin);
                    scaledDisplay.convertFromImage(scaledImg);
                }
#else
                scaledDisplay = thumbnail.scaled(dw, dh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
#endif
                scaledForDispW = dw;
                scaledForDispH = dh;
            }
            displayPixmap = scaledDisplay;
        }
        paintMediaContent(p, bubbleRect, etype, displayPixmap, caption,
                          mediaWidth, mediaHeight, durationSec, movie,
                          baseFont, fm, emojiW, pal, downloadState,
                          &downloadBtnRect, &retryBtnRect, fileSize);
        break;
    }
    case Audio: {
        QFont f = baseFont;
        int headerH = fm.lineSpacing();
        QRect bubbleRect;

        const int hdrBtnSize = 18, hdrBtnGap = 4;
        int hdrBtnAreaW = 2 * hdrBtnSize + hdrBtnGap;

        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;
        int kBubbleRadius = ChatView::kBubbleRadius;
        int kMsgSpacing = ChatView::kMsgSpacing;

        if (category == "self") {
            int ax = viewWidth - kPad - kAvatarSize;
            QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
            p.drawPixmap(ax, y + kPad, av);
            int contentRight = viewWidth - 2 * kPad - kAvatarSize;
            int contentLeft = kPad;
            int bubbleMaxW = contentRight - contentLeft;
            int bubbleW = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);
            int hdrTextRight = contentRight - kPad / 2;

            f.setPointSize(11); p.setFont(f);
            p.setPen(pal.textMuted);
            QString dname = !senderNickname.isEmpty() ? senderNickname
                          : !senderName.isEmpty() ? senderName
                          : (peerNumber >= 0 ? QString("Peer %1").arg(peerNumber) : "?");
            dname = qElideChars(dname, 23, ElideMiddle);
            int nameW = fm.width(dname);
            p.drawText(hdrTextRight - nameW, y + kPad, nameW, headerH,
                       Qt::AlignRight | Qt::AlignVCenter, dname);
            int ipW = 0;
            if (!ipAddress.isEmpty()) {
                f.setPointSize(10); p.setFont(f);
                p.setPen(QColor(130, 140, 150));
                ipW = fm.width(ipAddress);
                p.drawText(hdrTextRight - nameW - ipW - kPad/2, y + kPad,
                           ipW, headerH, Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
            }
            f.setPointSize(10); p.setFont(f);
            p.setPen(pal.textMuted);
            p.drawText(hdrTextRight - nameW - ipW - kPad/2 - fm.width(time) - kPad/2,
                       y + kPad, fm.width(time), headerH,
                       Qt::AlignRight | Qt::AlignVCenter, time);
            p.setFont(baseFont);

            if (etype != ChatElement::File) {
                int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                int btnX0 = contentRight - hdrBtnAreaW;
                sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                p.setPen(pal.textMuted); p.setBrush(Qt::NoBrush);
                p.drawEllipse(sourceBtnRect);
                drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                p.drawEllipse(translateBtnRect);
                drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
            }

            int bubbleX = contentRight - bubbleW;
            int bubbleY = y + kPad + headerH + kPad;
            int bubbleH = height - (kPad + headerH + kPad) - kMsgSpacing;
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(bubbleX, bubbleY, bubbleW, bubbleH);
            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
        } else {
            int ax = kPad;
            QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
            p.drawPixmap(ax, y + kPad, av);
            int contentX = 2 * kPad + kAvatarSize;
            int contentW = viewWidth - kPad - contentX;
            int bubbleW = (contentW * 80) / 100;
            if (bubbleW < 100) { bubbleW = contentW; }
            int bubbleRight = contentX + bubbleW;
            int hdrTextRight = bubbleRight - kPad / 2;

            f.setPointSize(11); p.setFont(f);
            p.setPen(pal.textMuted);
            QString dname = !senderNickname.isEmpty() ? senderNickname
                          : !senderName.isEmpty() ? senderName
                          : (peerNumber >= 0 ? QString("Peer %1").arg(peerNumber) : "?");
            dname = qElideChars(dname, 23, ElideMiddle);
            int maxNameW = hdrTextRight - contentX;
            if (maxNameW < 20) { maxNameW = 20; }
            p.drawText(contentX, y + kPad, maxNameW, headerH,
                       Qt::AlignLeft | Qt::AlignVCenter, dname);
            f.setPointSize(10); p.setFont(f);
            int ipEnd = contentX + fm.width(dname) + kPad;
            if (!ipAddress.isEmpty()) {
                p.setPen(QColor(130, 140, 150));
                int ipW = fm.width(ipAddress);
                int ipMax = hdrTextRight - ipEnd;
                if (ipMax > ipW) { ipMax = ipW; }
                if (ipMax > 0) {
                    p.drawText(ipEnd, y + kPad, ipMax, headerH,
                               Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
                }
                ipEnd += fm.width(ipAddress) + kPad/2;
            }
            p.setPen(pal.textMuted);
            int timeMaxW = hdrTextRight - ipEnd;
            if (timeMaxW > 0) {
                p.drawText(ipEnd, y + kPad, timeMaxW, headerH,
                           Qt::AlignLeft | Qt::AlignVCenter, time);
            }
            p.setFont(baseFont);

            if (etype != ChatElement::File) {
                int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                int btnX0 = bubbleRight - hdrBtnAreaW;
                sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                p.setPen(pal.textMuted); p.setBrush(Qt::NoBrush);
                p.drawEllipse(sourceBtnRect);
                drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                p.drawEllipse(translateBtnRect);
                drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
            }

            int bubbleY = y + kPad + headerH + kPad;
            int bubbleH = height - (kPad + headerH + kPad) - kMsgSpacing;
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(contentX, bubbleY, bubbleW, bubbleH);
            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
        }

        if (isSelected && !selRects.empty()) {
            QColor selColor = lerpColor(pal.baseBg, pal.accent, 0.25f);
            p.setPen(Qt::NoPen);
            p.setBrush(selColor);
            for (size_t ri = 0; ri < selRects.size(); ++ri) {
                QRect r = selRects[ri];
#ifdef QT3_BUILD
                r.moveBy(0, y);
#else
                r.translate(0, y);
#endif
                p.drawRect(r);
            }
        }

        paintAudioContent(p, bubbleRect, caption, durationSec, baseFont, fm, pal);
        {
            DownloadBarInfo bi = paintDownloadStatusBar(p, bubbleRect, baseFont, pal,
                downloadState, fileSize);
            downloadBtnRect = bi.downloadBtn;
            retryBtnRect    = bi.retryBtn;
        }
        break;
    }
    case File: {
        QFont f = baseFont;
        int headerH = fm.lineSpacing();
        QRect bubbleRect;

        const int hdrBtnSize = 18;
        const int hdrBtnGap = 4;
        int hdrBtnAreaW = 2 * hdrBtnSize + hdrBtnGap;

        int kAvatarSize = ChatView::kAvatarSize;
        int kPad = ChatView::kPad;
        int kBubbleHPad = ChatView::kBubbleHPad;
        int kBubbleVPad = ChatView::kBubbleVPad;
        int kBubbleRadius = ChatView::kBubbleRadius;
        int kMsgSpacing = ChatView::kMsgSpacing;

        if (category == "self") {
            int ax = viewWidth - kPad - kAvatarSize;
            QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
            p.drawPixmap(ax, y + kPad, av);

            int contentRight = viewWidth - 2 * kPad - kAvatarSize;
            int contentLeft = kPad;
            int bubbleMaxW = contentRight - contentLeft;
            int bubbleW = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);
            int hdrTextRight = contentRight - kPad / 2;

            // sender name
            f.setPointSize(11);
            p.setFont(f);
            p.setPen(pal.textMuted);
            QString dname = !senderNickname.isEmpty() ? senderNickname
                          : !senderName.isEmpty() ? senderName
                          : (peerNumber >= 0 ? QString("Peer %1").arg(peerNumber) : "?");
            dname = qElideChars(dname, 23, ElideMiddle);
            int nameW = fm.width(dname);
            p.drawText(hdrTextRight - nameW, y + kPad, nameW, headerH,
                       Qt::AlignRight | Qt::AlignVCenter, dname);

            // IP
            int ipW = 0;
            if (!ipAddress.isEmpty()) {
                f.setPointSize(10); p.setFont(f);
                p.setPen(QColor(130, 140, 150));
                ipW = fm.width(ipAddress);
                p.drawText(hdrTextRight - nameW - ipW - kPad/2, y + kPad,
                           ipW, headerH, Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
            }
            // time
            f.setPointSize(10); p.setFont(f);
            p.setPen(pal.textMuted);
            p.drawText(hdrTextRight - nameW - ipW - kPad/2 - fm.width(time) - kPad/2,
                       y + kPad, fm.width(time), headerH,
                       Qt::AlignRight | Qt::AlignVCenter, time);
            p.setFont(baseFont);

            // header buttons
            if (etype != ChatElement::File) {
                int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                int btnX0 = contentRight - hdrBtnAreaW;
                sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                p.setPen(pal.textMuted); p.setBrush(Qt::NoBrush);
                p.drawEllipse(sourceBtnRect);
                drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                p.drawEllipse(translateBtnRect);
                drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
            }

            // bubble background
            int bubbleX = contentRight - bubbleW;
            int bubbleY = y + kPad + headerH + kPad;
            int bubbleH = height - (kPad + headerH + kPad) - kMsgSpacing;
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(bubbleX, bubbleY, bubbleW, bubbleH);
            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif

            // ── file content ──
            int iconSize = 48;
            int iconX = bubbleRect.x() + kBubbleHPad;
            int iconY = bubbleRect.y() + kBubbleVPad;
            p.setBrush(pal.surfaceBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(iconX, iconY, iconSize, iconSize, 8, 8);
#else
            p.drawRoundedRect(iconX, iconY, iconSize, iconSize, 8, 8);
#endif
            uint32_t fileCp = fileIconForName(!caption.isEmpty() ? caption : fileName);
            QPixmap pm = EmojiRenderer::instance().renderEmoji(fileCp, iconSize - 4);
            if (!pm.isNull()) {
                int cx = iconX + (iconSize - pm.width()) / 2;
                int cy = iconY + (iconSize - pm.height()) / 2;
                p.drawPixmap(cx, cy, pm);
            } else {
                f.setPointSize(24);
                p.setFont(f);
                p.setPen(pal.textMuted);
                p.drawText(iconX, iconY, iconSize, iconSize, Qt::AlignCenter, qFromUtf8("📄"));
                p.setFont(baseFont);
            }

            int textX = iconX + iconSize + kPad;
            int textW = bubbleRect.right() - kBubbleHPad - textX;
            if (textW < 20) { textW = 20; }
            QString displayName = !caption.isEmpty() ? caption
                                : !fileName.isEmpty() ? fileName
                                : messageText;
            if (!displayName.isEmpty()) {
                p.setPen(pal.textPrimary);
                f.setPointSize(12);
                p.setFont(f);
#ifdef QT3_BUILD
                // Qt3 手动 elide
                QString elide = displayName;
                if (fm.width(elide) > textW) {
                    for (int ei = elide.length(); ei > 0; ei--) {
                        if (fm.width(elide.left(ei) + "...") <= textW) {
                            elide = elide.left(ei) + "..."; break;
                        }
                    }
                    if (fm.width(elide) > textW) { elide = "..."; }
                }
                p.drawText(textX, iconY, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter, elide);
#else
                p.drawText(textX, iconY, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter,
                           fm.elidedText(displayName, Qt::ElideRight, textW));
#endif
                p.setFont(baseFont);
            }
            if (!messageText.isEmpty() && messageText != displayName) {
                p.setPen(pal.textMuted);
                f.setPointSize(10);
                p.setFont(f);
                p.drawText(textX, iconY + fm.lineSpacing() + kPad/2, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter, messageText);
                p.setFont(baseFont);
            }
        } else {
            int ax = kPad;
            QPixmap av = AvatarManager::inst().get(avatarUrl, senderName, peerNumber, kAvatarSize);
            p.drawPixmap(ax, y + kPad, av);

            int contentX = 2 * kPad + kAvatarSize;
            int contentW = viewWidth - kPad - contentX;
            int bubbleW = (contentW * 80) / 100;
            if (bubbleW < 100) { bubbleW = contentW; }
            int bubbleRight = contentX + bubbleW;
            int hdrTextRight = bubbleRight - kPad / 2;

            // sender name
            f.setPointSize(11); p.setFont(f);
            p.setPen(pal.textMuted);
            QString dname = !senderNickname.isEmpty() ? senderNickname
                          : !senderName.isEmpty() ? senderName
                          : (peerNumber >= 0 ? QString("Peer %1").arg(peerNumber) : "?");
            dname = qElideChars(dname, 23, ElideMiddle);
            int maxNameW = hdrTextRight - contentX;
            if (maxNameW < 20) { maxNameW = 20; }
            p.drawText(contentX, y + kPad, maxNameW, headerH,
                       Qt::AlignLeft | Qt::AlignVCenter, dname);

            // IP
            f.setPointSize(10); p.setFont(f);
            int ipEnd = contentX + fm.width(dname) + kPad;
            if (!ipAddress.isEmpty()) {
                p.setPen(QColor(130, 140, 150));
                int ipW = fm.width(ipAddress);
                int ipMax = hdrTextRight - ipEnd;
                if (ipMax > ipW) { ipMax = ipW; }
                if (ipMax > 0) {
                    p.drawText(ipEnd, y + kPad, ipMax, headerH,
                               Qt::AlignLeft | Qt::AlignVCenter, ipAddress);
                }
                ipEnd += fm.width(ipAddress) + kPad/2;
            }
            // time
            p.setPen(pal.textMuted);
            int timeMaxW = hdrTextRight - ipEnd;
            if (timeMaxW > 0) {
                p.drawText(ipEnd, y + kPad, timeMaxW, headerH,
                           Qt::AlignLeft | Qt::AlignVCenter, time);
            }
            p.setFont(baseFont);

            // header buttons
            if (etype != ChatElement::File) {
                int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
                int btnX0 = bubbleRight - hdrBtnAreaW;
                sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
                translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
                p.setPen(pal.textMuted); p.setBrush(Qt::NoBrush);
                p.drawEllipse(sourceBtnRect);
                drawEmojiIcon(p, sourceBtnRect, 0x1F4CB, "📋");
                p.drawEllipse(translateBtnRect);
                drawEmojiIcon(p, translateBtnRect, 0x1F310, "🌐");
            }

            // bubble background
            int bubbleY = y + kPad + headerH + kPad;
            int bubbleH = height - (kPad + headerH + kPad) - kMsgSpacing;
            if (bubbleH < 30) { bubbleH = 30; }
            bubbleRect = QRect(contentX, bubbleY, bubbleW, bubbleH);
            p.setBrush(pal.baseBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(bubbleRect, 4, 4);
#else
            p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif

            // ── file content ──
            int iconSize = 48;
            int iconX = bubbleRect.x() + kBubbleHPad;
            int iconY = bubbleRect.y() + kBubbleVPad;
            p.setBrush(pal.surfaceBg);
            p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
            p.drawRoundRect(iconX, iconY, iconSize, iconSize, 8, 8);
#else
            p.drawRoundedRect(iconX, iconY, iconSize, iconSize, 8, 8);
#endif
            uint32_t fileCp = fileIconForName(!caption.isEmpty() ? caption : fileName);
            QPixmap pm = EmojiRenderer::instance().renderEmoji(fileCp, iconSize - 4);
            if (!pm.isNull()) {
                int cx = iconX + (iconSize - pm.width()) / 2;
                int cy = iconY + (iconSize - pm.height()) / 2;
                p.drawPixmap(cx, cy, pm);
            } else {
                f.setPointSize(24);
                p.setFont(f);
                p.setPen(pal.textMuted);
                p.drawText(iconX, iconY, iconSize, iconSize, Qt::AlignCenter, qFromUtf8("📄"));
                p.setFont(baseFont);
            }

            int textX = iconX + iconSize + kPad;
            int textW = bubbleRect.right() - kBubbleHPad - textX;
            if (textW < 20) { textW = 20; }
            QString displayName = !caption.isEmpty() ? caption
                                : !fileName.isEmpty() ? fileName
                                : messageText;
            if (!displayName.isEmpty()) {
                p.setPen(pal.textPrimary);
                f.setPointSize(12);
                p.setFont(f);
#ifdef QT3_BUILD
                QString elide = displayName;
                if (fm.width(elide) > textW) {
                    for (int ei = elide.length(); ei > 0; ei--) {
                        if (fm.width(elide.left(ei) + "...") <= textW) {
                            elide = elide.left(ei) + "..."; break;
                        }
                    }
                    if (fm.width(elide) > textW) { elide = "..."; }
                }
                p.drawText(textX, iconY, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter, elide);
#else
                p.drawText(textX, iconY, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter,
                           fm.elidedText(displayName, Qt::ElideRight, textW));
#endif
                p.setFont(baseFont);
            }
            if (!messageText.isEmpty() && messageText != displayName) {
                p.setPen(pal.textMuted);
                f.setPointSize(10);
                p.setFont(f);
                p.drawText(textX, iconY + fm.lineSpacing() + kPad/2, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter, messageText);
                p.setFont(baseFont);
            }
        }

        // ── selection highlight ──
        if (isSelected && !selRects.empty()) {
            QColor selColor = lerpColor(pal.baseBg, pal.accent, 0.25f);
            p.setPen(Qt::NoPen);
            p.setBrush(selColor);
            for (size_t ri = 0; ri < selRects.size(); ++ri) {
                QRect r = selRects[ri];
#ifdef QT3_BUILD
                r.moveBy(0, y);
#else
                r.translate(0, y);
#endif
                p.drawRect(r);
            }
        }
        // 下载按钮
        {
            DownloadBarInfo bi = paintDownloadStatusBar(p, bubbleRect, baseFont, pal,
                downloadState, fileSize);
            downloadBtnRect = bi.downloadBtn;
            retryBtnRect    = bi.retryBtn;
        }
        break;
    }
    }
}

void ChatElement::startAnimation(QWidget* parent, int msgIndex) {
    if (etype != Gif || movie) { return; }
#ifndef QT3_BUILD
    movie = new QMovie(gifPath);
    {
        auto* slot = new LambdaSlot(movie, [parent, msgIndex]() {
            static_cast<ChatView*>(parent)->onGifFrameUpdated(msgIndex);
        });
        QObject::connect(movie, SIGNAL(updated(const QRect&)), slot, SLOT(call()));
    }
    movie->start();
#else
    Q_UNUSED(parent);
    Q_UNUSED(msgIndex);
    // Qt3 QMovie API differs; GIF animation deferred to Phase 4
#endif
}

void ChatElement::stopAnimation() {
    if (!movie) { return; }
#ifndef QT3_BUILD
    movie->stop();
#endif
    delete movie;
    movie = nullptr;
}

// ───── ChatView ─────

// Block index helpers — binary-search accelerated Y-position lookup.
void ChatView::rebuildBlocks() {
    m_blocks.clear();
    int absY = kPad;
    for (size_t i = 0; i < m_items.size(); i += kBlockSize) {
        m_blocks.push_back({absY});
        size_t end = std::min(i + kBlockSize, m_items.size());
        for (size_t j = i; j < end; ++j) {
            absY += m_items[j].height;
        }
    }
}

int ChatView::blockForIndex(int msgIndex) const {
    if (m_blocks.empty() || msgIndex < 0) { return -1; }
    int lo = 0, hi = (int)m_blocks.size();
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (mid * kBlockSize <= msgIndex) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo - 1;
}

int ChatView::msgAbsY(int msgIndex) const {
    if (msgIndex < 0 || msgIndex >= (int)m_items.size()) { return -1; }
    int blockIdx = blockForIndex(msgIndex);
    if (blockIdx < 0) { return kPad; }
    int absY = m_blocks[blockIdx].cumulativeHeight;
    int blockStart = blockIdx * kBlockSize;
    int blockEnd = std::min(blockStart + kBlockSize, (int)m_items.size());
    for (int i = blockStart; i < msgIndex && i < blockEnd; ++i) {
        absY += m_items[i].height;
    }
    return absY;
}

int ChatView::findByAbsY(int absY) const {
    if (m_blocks.empty()) { return -1; }
    if (absY < kPad) { return 0; }
    if (absY >= m_totalHeight) { return -1; }
    int lo = 0, hi = (int)m_blocks.size();
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (m_blocks[mid].cumulativeHeight <= absY) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    int blockIdx = lo - 1;
    int absPos = m_blocks[blockIdx].cumulativeHeight;
    int start = blockIdx * kBlockSize;
    int end = std::min(start + kBlockSize, (int)m_items.size());
    for (int i = start; i < end; ++i) {
        int h = m_items[i].height;
        if (absY >= absPos && absY < absPos + h) { return i; }
        absPos += h;
    }
    return end - 1;
}

// 刷新策略：updateFull() 用于可视结构变化全量重绘，
// updateRect() 用于脏矩形可精确定位的局部刷新。
void ChatView::updateFull() {
    QWidget::update();
}

QRect ChatView::messageRect(int msgIndex) const {
    if (msgIndex < 0 || msgIndex >= (int)m_items.size()) { return QRect(); }
    int y = msgAbsY(msgIndex) - m_scrollPos;
    return QRect(0, y, contentWidth(), m_items[msgIndex].height);
}

void ChatView::updateRect(const QRect& r) {
    if (r.isEmpty()) {
        qWarning("updateRect: empty rect (w=%d h=%d) — should use updateFull()",
                 r.width(), r.height());
        return;
    }
    QWidget::update(r);
}

ChatView::ChatView(QWidget* parent)
    : QWidget(parent)
    , m_clickCount(0), m_clickMsgIndex(-1)
    , m_selMsgIndex(-1), m_selStart(0), m_selEnd(0), m_selecting(false)
    , m_fm(font()), m_emojiW(0), m_bmpW(NULL)
{
    m_totalHeight = 0;
    m_scrollPos = 0;
    for (int i = 0; i < 128; i++) {
        m_ascW[i] = (uint8_t)std::min(m_fm.width(QChar(i)), 255);
    }
    m_emojiW = emojiCharWidth(m_fm);

    m_vScrollBar = new LimeScrollBar(Qt::Vertical, this);
#ifdef QT3_BUILD
    m_vScrollBar->setSteps(10, 50);
#else
    m_vScrollBar->setSingleStep(10);
    m_vScrollBar->setPageStep(50);
#endif
    connect(m_vScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onScrollChanged(int)));

#ifdef QT3_BUILD
    setFocusPolicy(QWidget::StrongFocus);
#else
    setFocusPolicy(Qt::StrongFocus);
#endif
    setMouseTracking(true);
    m_scrollDownPill.setCallback([this]() {
        m_scrollDownPill.setCount(0);
        scrollToBottom();
    });
#ifdef QT3_BUILD
    setWFlags(getWFlags() | WNoAutoErase);
#else
    setAttribute(Qt::WA_OpaquePaintEvent);
#endif

    m_animTimer = new QTimer(this);
    QObject::connect(m_animTimer, SIGNAL(timeout()), this, SLOT(onAnimTick()));
    m_animTimer->start(200);
}

ChatView::~ChatView() {
    delete[] m_bmpW;
}

void ChatView::restoreMessages(const std::vector<ChatElement>& msgs) {
    m_scrollDownPill.setCount(0);
    m_items = msgs;
    m_gifFrameUpdated.assign(msgs.size(), 0);
    relayout();
    scrollToBottom();
}

void ChatView::appendMessage(const ChatElement& msg) {
    // Block index maintenance: start a new block when the last one is full.
    if (m_blocks.empty()) {
        m_blocks.push_back({kPad});
    } else {
        int lastBlockStart = ((int)m_blocks.size() - 1) * kBlockSize;
        int lastBlockCount = (int)m_items.size() - lastBlockStart;
        if (lastBlockCount >= kBlockSize) {
            m_blocks.push_back({m_totalHeight});
        }
    }

    int curVal = m_vScrollBar->value();
#ifdef QT3_BUILD
    int oldMax = m_vScrollBar->maxValue();
#else
    int oldMax = m_vScrollBar->maximum();
#endif
    bool atBottom = (oldMax - curVal) <= 20;

    int w = contentWidth();
    if (w <= 0) { w = 400; }
    m_items.push_back(msg);
    m_gifFrameUpdated.push_back(0);
    ChatElement& el = m_items.back();
    el.height = el.calcHeight(w, m_fm, m_emojiW, font());
    el.cachedWidth = (short)w;
    m_totalHeight += el.height;

    int vpH = height();
    int maxScroll = std::max(0, m_totalHeight - vpH);
    m_vScrollBar->setRange(0, maxScroll);
    m_vScrollBar->setPageStep(vpH);

    if (atBottom) {
        m_scrollPos = maxScroll;
        m_vScrollBar->blockSignals(true);
        m_vScrollBar->setValue(maxScroll);
        m_vScrollBar->blockSignals(false);
        m_vScrollBar->showTemporarily();
        triggerVisibleDownloads();
        updateFull();
    } else {
        m_scrollDownPill.setCount(m_scrollDownPill.count() + 1);
        updateRect(QRect(width() - 150, height() - 40, 150, 40));
    }
}

void ChatView::clearMessages() {
    for (auto& el : m_items) { el.stopAnimation(); }
    m_items.clear();
    m_gifFrameUpdated.clear();
    m_blocks.clear();
    m_totalHeight = 0;
    m_scrollPos = 0;
    m_vScrollBar->setRange(0, 0);
    m_selMsgIndex = -1;
    m_selStart = m_selEnd = 0;
    m_scrollDownPill.setCount(0);
    updateFull();
}

void ChatView::scrollToBottom() {
#ifdef QT3_BUILD
    int maxScroll = m_vScrollBar->maxValue();
#else
    int maxScroll = m_vScrollBar->maximum();
#endif
    m_vScrollBar->setValue(maxScroll);
}

ChatElement& ChatView::messageAt(int index) {
    return m_items[index];
}

int ChatView::messageCount() const {
    return (int)m_items.size();
}

void ChatView::triggerRelayout(int msgIndex) {
    if (msgIndex >= 0 && msgIndex < (int)m_items.size()) {
        m_items[msgIndex].cachedWidth = -1;
        m_items[msgIndex].height = 0;
    } else {
        for (auto& el : m_items) {
            el.cachedWidth = -1;
            el.height = 0;
        }
    }
    relayout();
}

int ChatView::contentWidth() const {
    int sbw = m_vScrollBar->sizeHint().width();
    return width() - sbw;
}

void ChatView::relayout() {
    int w = contentWidth();
    if (w <= 0) { w = 400; }

    m_totalHeight = kPad;
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].cachedWidth != w) {
            m_items[i].height = m_items[i].calcHeight(w, m_fm, m_emojiW, font());
            m_items[i].cachedWidth = (short)w;
            m_items[i].scaledForDispW = -1;
        }
        m_totalHeight += m_items[i].height;
    }

    int vpH = height();
    int maxScroll = std::max(0, m_totalHeight - vpH);
#ifdef QT3_BUILD
    int oldMax = m_vScrollBar->maxValue();
    int oldVal = m_vScrollBar->value();
#else
    int oldMax = m_vScrollBar->maximum();
    int oldVal = m_vScrollBar->value();
#endif
    rebuildBlocks();
    m_vScrollBar->setRange(0, maxScroll);
    m_vScrollBar->setPageStep(vpH);
    if (oldVal == oldMax && oldMax > 0) {
        m_vScrollBar->setValue(maxScroll);
    }
    updateFull();
}

int ChatView::charWidth(uint32_t cp) {
    if (cp < 128) return m_ascW[cp];
    if (cp < 0x10000) {
        if (!m_bmpW) m_bmpW = new uint8_t[65536]();
        if (!m_bmpW[cp]) {
            m_bmpW[cp] = (uint8_t)std::min(m_fm.width(QChar((ushort)cp)), 255);
        }
        return m_bmpW[cp];
    }
    return m_fm.width(QChar((ushort)cp));
}

// Extract URLs from text
std::vector<LinkSpan> ChatView::extractLinks(const QString& text) {
    std::vector<LinkSpan> spans;
#ifdef QT3_BUILD
    QRegExp urlRe("https?://[^\\s<>\"']+", false);
#else
    QRegExp urlRe("https?://[^\\s<>\"']+", Qt::CaseInsensitive);
#endif
    int pos = 0;
    while (true) {
#ifdef QT3_BUILD
        pos = urlRe.search(text, pos);
#else
        pos = urlRe.indexIn(text, pos);
#endif
        if (pos == -1) { break; }
        int len = urlRe.matchedLength();
        QString url = text.mid(pos, len);
        while (len > 0 && (url.right(1) == "." || url.right(1) == "," ||
               url.right(1) == ";" || url.right(1) == ":" ||
               url.right(1) == "!" || url.right(1) == "?" ||
               url.right(1) == ")" || url.right(1) == "]" ||
               url.right(1) == "}" || url.right(1) == ">")) {
            len--;
            url = url.left(len);
        }
        LinkSpan span;
        span.start = pos;
        span.end = pos + len;
        span.url = url;
        spans.push_back(span);
        pos += len;
    }
    return spans;
}

// Find word boundaries around a character position
static void wordBoundaries(const QString& text, int pos, int& start, int& end) {
    int len = text.length();
    if (len == 0) { start = end = 0; return; }
    start = pos;
    end = pos;
    if (pos < 0) { pos = 0; }
    if (pos >= len) { pos = len - 1; }
    // If at whitespace, move to nearest non-space
    while (start >= 0 && text[start].isSpace()) start--;
    while (end < len && text[end].isSpace()) end++;
    if (start < 0) { start = end = 0; return; }
    if (end >= len) { end = len; }
    // Expand left
    while (start > 0 && !text[start - 1].isSpace()) start--;
    // Expand right
    while (end < len && !text[end].isSpace()) end++;
}

// Find line boundaries around a character position
static void lineBoundaries(const QString& text, int pos, int& start, int& end) {
    int len = text.length();
    if (len == 0) { start = end = 0; return; }
    start = pos;
    end = pos;
    if (pos < 0) { pos = 0; }
    if (pos >= len) { pos = len - 1; }
    // Expand left to line start
    while (start > 0 && text[start - 1] != '\n') { start--; }
    // Expand right to line end
    while (end < len && text[end] != '\n') { end++; }
}

// Find which message is at given y coordinate (relative to widget top)
int ChatView::findMessageAtY(int y) const {
    return findByAbsY(m_scrollPos + y);
}

// Get character position in a message from local coordinates
int ChatView::charPosAt(int msgIndex, int localX, int localY) {
    if (msgIndex < 0 || msgIndex >= (int)m_items.size()) return -1;
    const ChatElement& msg = m_items[msgIndex];
    const QString& text = msg.messageText;
    int textLen = text.length();
    if (textLen == 0) { return 0; }

    QFont f = font();
    QFontMetrics fm(f);
    int viewW = contentWidth();
    int headerH = fm.lineSpacing();

    // Compute bubble text width (same as calcMessageHeight)
    int contentW = viewW - 3 * kPad - kAvatarSize;
    int bubbleW = (contentW * 80) / 100;
    if (bubbleW < 100) { bubbleW = contentW; }
    int bubbleTextWidth = bubbleW - 2 * kBubbleHPad;
    if (bubbleTextWidth < 20) { bubbleTextWidth = 20; }

    // Compute text area position
    int areaX, areaY;
    if (msg.category == "self") {
        int contentRight = viewW - 2 * kPad - kAvatarSize;
        int contentLeft = kPad;
        int bubbleMaxW = contentRight - contentLeft;
        int bubbleW2 = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);
        int bubbleX = contentRight - bubbleW2;
        areaX = bubbleX + kBubbleHPad;
    } else {
        int contentX = 2 * kPad + kAvatarSize;
        int contentW2 = viewW - kPad - contentX;
        int bubbleW3 = (contentW2 * 80) / 100;
        if (bubbleW3 < 100) { bubbleW3 = contentW2; }
        areaX = contentX + kBubbleHPad;
    }
    areaY = kPad + headerH + kPad + kBubbleVPad;

    localX -= areaX;
    localY -= areaY;
    if (localX < 0) { localX = 0; }
    if (localY < 0) { return -1; }

    // Compute line breaks
    std::vector<int> lineStarts;
    lineStarts.push_back(0);
    int pos = 0;
    while (pos < textLen) {
        if (text[pos] == '\n') {
            pos++;
            lineStarts.push_back(pos);
            continue;
        }
        int lineWidth = 0, lastSpace = -1, end = pos;
        while (end < textLen && text[end] != '\n') {
            lineWidth += fm.width(text[end]);
            if (text[end].isSpace()) lastSpace = end;
            if (lineWidth >= bubbleTextWidth) {
                if (lastSpace > pos && end - pos > 10) {
                    end = lastSpace + 1;
                }
                break;
            }
            end++;
        }
        pos = end;
        if (pos < textLen) { lineStarts.push_back(pos); }
    }

    int lineHeight = fm.lineSpacing();
    int lineIndex = localY / lineHeight;
    if (lineIndex >= (int)lineStarts.size())
        lineIndex = (int)lineStarts.size() - 1;
    if (lineIndex < 0) { lineIndex = 0; }

    int lineStart = lineStarts[lineIndex];
    int lineEnd = (lineIndex + 1 < (int)lineStarts.size()) ?
                  lineStarts[lineIndex + 1] : textLen;

    // Find character at x position
    int xOffset = 0;
    for (int i = lineStart; i < lineEnd; i++) {
        int chW = fm.width(text[i]);
        if (localX <= xOffset + chW / 2) {
            return i;
        }
        xOffset += chW;
    }
    return lineEnd - 1;
}

// Get rectangles for selection in a message
std::vector<QRect> ChatView::selectionRects(int msgIndex) {
    std::vector<QRect> rects;
    if (msgIndex != m_selMsgIndex) { return rects; }
    int start = std::min(m_selStart, m_selEnd);
    int end = std::max(m_selStart, m_selEnd);
    if (start == end) { return rects; }

    const ChatElement& msg = m_items[msgIndex];
    int textLen = msg.messageText.length();
    if (start >= textLen || end <= 0) { return rects; }

    // Compute text rectangle
    QFont f = font();
    QFontMetrics fm(f);
    int viewW = contentWidth();
    int headerH = fm.lineSpacing();

    QRect textRect;
    if (msg.category == "self") {
        int contentRight = viewW - 2 * kPad - kAvatarSize;
        int contentLeft = kPad;
        int bubbleMaxW = contentRight - contentLeft;
        int bubbleW = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);
        int bubbleX = contentRight - bubbleW;
        int bubbleY = kPad + headerH + kPad; // relative to message top
        textRect = QRect(bubbleX + kBubbleHPad, bubbleY + kBubbleVPad,
                         bubbleW - 2 * kBubbleHPad, msg.height - (2*kPad + headerH + kMsgSpacing) - 2*kBubbleVPad);
    } else {
        int contentX = 2 * kPad + kAvatarSize;
        int contentW = viewW - kPad - contentX;
        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) { bubbleW = contentW; }
        textRect = QRect(contentX + kBubbleHPad, kPad + headerH + kPad + kBubbleVPad,
                         bubbleW - 2 * kBubbleHPad, msg.height - (2*kPad + headerH + kMsgSpacing) - 2*kBubbleVPad);
    }

    // Get line breaks
    int contentW = viewW - 3 * kPad - kAvatarSize;
    int bubbleW2 = (contentW * 80) / 100;
    if (bubbleW2 < 100) { bubbleW2 = contentW; }
    int bubbleTextWidth = bubbleW2 - 2 * kBubbleHPad;
    if (bubbleTextWidth < 20) { bubbleTextWidth = 20; }

    // Get line breaks using word-wrap
    std::vector<int> lineStarts;
    lineStarts.push_back(0);
    int cpos = 0;
    while (cpos < textLen) {
        if (msg.messageText[cpos] == '\n') {
            cpos++;
            lineStarts.push_back(cpos);
            continue;
        }
        int lineWidth = 0, lastSpace = -1, end = cpos;
        while (end < textLen && msg.messageText[end] != '\n') {
            lineWidth += fm.width(msg.messageText[end]);
            if (msg.messageText[end].isSpace()) lastSpace = end;
            if (lineWidth >= bubbleTextWidth) {
                if (lastSpace > cpos && end - cpos > 10) {
                    end = lastSpace + 1;
                }
                break;
            }
            end++;
        }
        cpos = end;
        if (cpos < textLen) { lineStarts.push_back(cpos); }
    }

    int lineHeight = fm.lineSpacing();
    for (size_t li = 0; li < lineStarts.size(); li++) {
        int lineStart = lineStarts[li];
        int lineEnd = (li+1 < lineStarts.size()) ? lineStarts[li+1] : textLen;
        int selStartInLine = std::max(start, lineStart);
        int selEndInLine = std::min(end, lineEnd);
        if (selStartInLine < selEndInLine) {
            int x1 = 0, x2 = 0;
            for (int i = lineStart; i < selStartInLine; i++) {
                x1 += fm.width(msg.messageText[i]);
            }
            for (int i = lineStart; i < selEndInLine; i++) {
                x2 += fm.width(msg.messageText[i]);
            }
            QRect selRect(textRect.x() + x1, textRect.y() + li * lineHeight, x2 - x1, lineHeight);
            rects.push_back(selRect);
        }
    }
    return rects;
}

QString ChatView::selectedText() const {
    if (m_selMsgIndex < 0 || m_selMsgIndex >= (int)m_items.size()) return QString();
    int start = std::min(m_selStart, m_selEnd);
    int end = std::max(m_selStart, m_selEnd);
    if (start == end) { return QString(); }
    return m_items[m_selMsgIndex].messageText.mid(start, end - start);
}

void ChatView::selectWordAt(int msgIndex, int charPos) {
    if (msgIndex < 0 || msgIndex >= (int)m_items.size()) return;
    int oldIdx = m_selMsgIndex;
    const QString& text = m_items[msgIndex].messageText;
    int start, end;
    wordBoundaries(text, charPos, start, end);
    m_selMsgIndex = msgIndex;
    m_selStart = start;
    m_selEnd = end;
    m_selecting = false;
    QRect dirty = messageRect(oldIdx);
#ifdef QT3_BUILD
    dirty = dirty.unite(messageRect(msgIndex));
#else
    dirty = dirty.united(messageRect(msgIndex));
#endif
    if (!dirty.isEmpty()) updateRect(dirty);
}

void ChatView::selectLineAt(int msgIndex, int charPos) {
    if (msgIndex < 0 || msgIndex >= (int)m_items.size()) return;
    int oldIdx = m_selMsgIndex;
    const QString& text = m_items[msgIndex].messageText;
    int start, end;
    lineBoundaries(text, charPos, start, end);
    m_selMsgIndex = msgIndex;
    m_selStart = start;
    m_selEnd = end;
    m_selecting = false;
    QRect dirty = messageRect(oldIdx);
#ifdef QT3_BUILD
    dirty = dirty.unite(messageRect(msgIndex));
#else
    dirty = dirty.united(messageRect(msgIndex));
#endif
    if (!dirty.isEmpty()) updateRect(dirty);
}

void ChatView::copySelectedText() {
    QString text = selectedText();
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
    }
}

void ChatView::copyFullMessage(int msgIndex) {
    if (msgIndex >= 0 && msgIndex < (int)m_items.size()) {
        QApplication::clipboard()->setText(m_items[msgIndex].messageText);
    }
}

void ChatView::wheelEvent(QWheelEvent* event) {
#ifdef QT3_BUILD
    int step = m_vScrollBar->lineStep();
#else
    int step = m_vScrollBar->singleStep();
#endif
    int newVal = m_vScrollBar->value() + (event->delta() > 0 ? -step * 5 : step * 5);
    m_vScrollBar->setValue(newVal);
    event->accept();
}

void ChatView::keyPressEvent(QKeyEvent* event) {
    int val = m_vScrollBar->value();
    // Ctrl+C to copy selected text
#ifdef QT3_BUILD
    if (event->key() == Qt::Key_C && (event->state() & Qt::ControlButton)) {
#else
    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
#endif
        copySelectedText();
        return;
    }
    switch (event->key()) {
#ifdef QT3_BUILD
    case Qt::Key_PageUp:     val -= m_vScrollBar->pageStep(); break;
    case Qt::Key_PageDown:   val += m_vScrollBar->pageStep(); break;
    case Qt::Key_Up:         val -= m_vScrollBar->lineStep(); break;
    case Qt::Key_Down:       val += m_vScrollBar->lineStep(); break;
    case Qt::Key_Home:       val  = m_vScrollBar->minValue(); break;
    case Qt::Key_End:        val  = m_vScrollBar->maxValue(); break;
#else
    case Qt::Key_PageUp:     val -= m_vScrollBar->pageStep(); break;
    case Qt::Key_PageDown:   val += m_vScrollBar->pageStep(); break;
    case Qt::Key_Up:         val -= m_vScrollBar->singleStep(); break;
    case Qt::Key_Down:       val += m_vScrollBar->singleStep(); break;
    case Qt::Key_Home:       val  = m_vScrollBar->minimum(); break;
    case Qt::Key_End:        val  = m_vScrollBar->maximum(); break;
#endif
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    m_vScrollBar->setValue(val);
    event->accept();
}

void ChatView::mousePressEvent(QMouseEvent* event) {
    if (m_scrollDownPill.handleClick(event->pos())) {
        return;
    }
    if (event->button() == Qt::LeftButton) {
        int msgIndex = findMessageAtY(event->y());
        if (msgIndex >= 0 && msgIndex < (int)m_items.size()) {
            // Check translate button click
            if (m_items[msgIndex].translateBtnRect.contains(event->pos())) {
                if (!m_items[msgIndex].translationInProgress) {
                    emit translateClicked(msgIndex);
                }
                return;
            }

            // Check source button click
            if (m_items[msgIndex].sourceBtnRect.contains(event->pos())) {
                emit sourceClicked(msgIndex);
                return;
            }

            // Check download button click (handled in release, prevent text selection)
            if (!m_items[msgIndex].downloadBtnRect.isNull() &&
                m_items[msgIndex].downloadBtnRect.contains(event->pos())) {
                return;
            }
            // Check retry button click
            if (!m_items[msgIndex].retryBtnRect.isNull() &&
                m_items[msgIndex].retryBtnRect.contains(event->pos())) {
                return;
            }

            // Compute local Y relative to message
            int curY = kPad - m_scrollPos;
            for (int i = 0; i < msgIndex; i++) { curY += m_items[i].height; }
            int localY = event->y() - curY;
            int localX = event->x();
            int charPos = charPosAt(msgIndex, localX, localY);
            if (charPos >= 0) {
                // Triple-click detection
                QTime now = QTime::currentTime();
                if (msgIndex == m_clickMsgIndex && m_clickCount == 2 &&
                    m_clickTime.msecsTo(now) <= QApplication::doubleClickInterval()) {
                    selectLineAt(msgIndex, charPos);
                    m_clickCount = 0;
                    return;
                }
                m_clickCount = 1;
                m_clickMsgIndex = msgIndex;
                m_clickTime = now;

                // Check if clicked on a URL
                auto links = extractLinks(m_items[msgIndex].messageText);
                for (const LinkSpan& link : links) {
                    qWarning("  PRESS link [%d,%d): %s",
                             link.start, link.end, qToUtf8(link.url).data());
                    if (charPos >= link.start && charPos < link.end) {
                        qOpenUrl(link.url);
                        return;
                    }
                }
                // Start selection
                int oldIdx = m_selMsgIndex;
                m_selMsgIndex = msgIndex;
                m_selStart = charPos;
                m_selEnd = charPos;
                m_selecting = true;
                updateRect(messageRect(oldIdx));
            }
        } else {
            // Click outside messages, clear selection
            if (m_selMsgIndex != -1) {
                int oldIdx = m_selMsgIndex;
                m_selMsgIndex = -1;
                updateRect(messageRect(oldIdx));
            }
            m_clickCount = 0;
        }
    }
    QWidget::mousePressEvent(event);
}

void ChatView::mouseMoveEvent(QMouseEvent* event) {
    bool overPill = m_scrollDownPill.count() > 0
        && m_scrollDownPill.rect().contains(event->pos());
    if (overPill != m_scrollDownPill.isHovered()) {
        m_scrollDownPill.setHovered(overPill);
        updateRect(m_scrollDownPill.rect());
    }
    if (overPill) {
        setCursor(QCursor(Qt::PointingHandCursor));
        QWidget::mouseMoveEvent(event);
        return;
    }
    if (m_selecting && m_selMsgIndex >= 0 && m_selMsgIndex < (int)m_items.size()) {
        int msgIndex = findMessageAtY(event->y());
        if (msgIndex == m_selMsgIndex) {
            int msgY = msgAbsY(msgIndex) - m_scrollPos;
            int localY = event->y() - msgY;
            int localX = event->x();
            int charPos = charPosAt(msgIndex, localX, localY);
            if (charPos >= 0) {
                m_selEnd = charPos;
                updateRect(QRect(0, msgY, width(), m_items[m_selMsgIndex].height));
            }
        }
    }
    // Set cursor based on whether over URL
    int msgIndex = findMessageAtY(event->y());
    if (msgIndex >= 0 && msgIndex < (int)m_items.size()) {
        // Check header action buttons first
        if (m_items[msgIndex].translateBtnRect.contains(event->pos())) {
            QString tip = m_items[msgIndex].translateError.isEmpty()
                ? qFromUtf8("Translate")
                : m_items[msgIndex].translateError;
            showTempTooltip(this, m_items[msgIndex].translateBtnRect, tip);
            setCursor(QCursor(Qt::PointingHandCursor));
            QWidget::mouseMoveEvent(event);
            return;
        }
        if (m_items[msgIndex].sourceBtnRect.contains(event->pos())) {
            showTempTooltip(this, m_items[msgIndex].sourceBtnRect, qFromUtf8("Source"));
            setCursor(QCursor(Qt::PointingHandCursor));
            QWidget::mouseMoveEvent(event);
            return;
        }
        // Nickname tooltip: 显示 nickname 时，hover 名字区域显示 username
        {
            const auto& item = m_items[msgIndex];
            if (!item.senderNickname.isEmpty() && !item.senderName.isEmpty()
                && item.senderNickname != item.senderName) {
                int msgY = msgAbsY(msgIndex) - m_scrollPos;
                QFont nf;
                nf.setPointSize(11);
                QFontMetrics nfm(nf);
                int headerH = nfm.lineSpacing();
                QRect nameRect(kPad, msgY + kPad, width() - 2*kPad, headerH);
                if (nameRect.contains(event->pos())) {
                    showTempTooltip(this, nameRect, item.senderName);
                }
            }
        }

        // ... compute charPos for link detection
        int curY = msgAbsY(msgIndex) - m_scrollPos;
        int localY = event->y() - curY;
        int localX = event->x();
        int charPos = charPosAt(msgIndex, localX, localY);
        if (charPos >= 0) {
            auto links = extractLinks(m_items[msgIndex].messageText);
            for (const LinkSpan& link : links) {
                if (charPos >= link.start && charPos < link.end) {
#ifdef QT3_BUILD
                    setCursor(QCursor(Qt::PointingHandCursor));
#else
                    setCursor(QCursor(Qt::PointingHandCursor));
#endif
                    QWidget::mouseMoveEvent(event);
                    return;
                }
            }
        }
        // Over text area, set I-beam cursor
#ifdef QT3_BUILD
        setCursor(QCursor(Qt::IbeamCursor));
#else
        setCursor(QCursor(Qt::IBeamCursor));
#endif
    } else {
        setCursor(QCursor(Qt::ArrowCursor));
    }

    QWidget::mouseMoveEvent(event);
}

void ChatView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int msgIndex = findMessageAtY(event->y());
        if (msgIndex >= 0 && msgIndex < (int)m_items.size()) {
            ChatElement& el = m_items[msgIndex];
            // Download button (always)
            if (!el.downloadBtnRect.isNull() &&
                el.downloadBtnRect.contains(event->pos())) {
                emit retryClicked(msgIndex, el.mediaUrl);
                return;
            }
            // Retry button (only when failed)
            if (!el.retryBtnRect.isNull() &&
                el.retryBtnRect.contains(event->pos())) {
                emit retryClicked(msgIndex, el.mediaUrl);
                return;
            }
        }
        if (m_selecting) {
            m_selecting = false;
            // If start and end are same, clear selection
            if (m_selStart == m_selEnd) {
                int oldIdx = m_selMsgIndex;
                m_selMsgIndex = -1;
                updateRect(messageRect(oldIdx));
            }
        }
        // Retry via status bar button replaces old thumbnail-area retry
    }
    QWidget::mouseReleaseEvent(event);
}

void ChatView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int msgIndex = findMessageAtY(event->y());
        if (msgIndex >= 0 && msgIndex < (int)m_items.size()) {
            // 双击下载/重试按钮时不打开 PhotoViewer、不进入选择
            if (!m_items[msgIndex].downloadBtnRect.isNull() &&
                m_items[msgIndex].downloadBtnRect.contains(event->pos())) {
                return;
            }
            if (!m_items[msgIndex].retryBtnRect.isNull() &&
                m_items[msgIndex].retryBtnRect.contains(event->pos())) {
                return;
            }
            // 双击媒体缩略图 → 打开 PhotoViewer
            if ((m_items[msgIndex].etype == ChatElement::Image ||
                 m_items[msgIndex].etype == ChatElement::Video ||
                 m_items[msgIndex].etype == ChatElement::Gif) &&
                (!m_items[msgIndex].thumbnail.isNull() || !m_items[msgIndex].rawFileData.empty()) &&
                m_items[msgIndex].thumbnailRect.contains(event->pos())) {
                QPixmap fullPix = m_items[msgIndex].thumbnail;
                if (fullPix.isNull() && isWebP(m_items[msgIndex].rawFileData))
                    fullPix = decodeWebP(m_items[msgIndex].rawFileData);
                if (fullPix.isNull()) return;
                PhotoViewer* pv = new PhotoViewer(this, fullPix);
                pv->show();
                return;
            }
        }
        if (msgIndex >= 0) {
            int msgY = kPad - m_scrollPos;
            for (int i = 0; i < msgIndex; i++) { msgY += m_items[i].height; }
            int localY = event->y() - msgY;
            int localX = event->x();
            int charPos = charPosAt(msgIndex, localX, localY);
            if (charPos >= 0) {
                selectWordAt(msgIndex, charPos);
                m_clickCount = 2;
                m_clickMsgIndex = msgIndex;
                m_clickTime = QTime::currentTime();
                return;
            }
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ChatView::contextMenuEvent(QContextMenuEvent* event) {
    int msgIndex = findMessageAtY(event->y());
    // 检测是否在名字区域上
    bool onName = false;
    QString displayName;
    if (msgIndex >= 0 && msgIndex < (int)m_items.size()) {
        const ChatElement& item = m_items[msgIndex];
        displayName = item.senderNickname.isEmpty() ? item.senderName : item.senderNickname;
        int msgY = kPad - m_scrollPos;
        for (int i = 0; i < msgIndex; i++) { msgY += m_items[i].height; }
        QFont nf;
        nf.setPointSize(11);
        QFontMetrics nfm(nf);
        int headerH = nfm.lineSpacing();
        QRect nameRect(kPad, msgY + kPad, width() - 2*kPad, headerH);
        onName = nameRect.contains(event->pos());
    }
#ifdef QT3_BUILD
    QPopupMenu menu(this);
#else
    QMenu menu(this);
#endif
    // Copy full message
#ifdef QT3_BUILD
    int copyMsgId = menu.insertItem(_("context.copy_message"));
    int selectAllId = menu.insertItem(_("context.select_all"));
    int copyNickId = -1, mentionId = -1;
    if (onName) {
        copyNickId = menu.insertItem(qFromUtf8("复制昵称"));
        mentionId = menu.insertItem(qFromUtf8("@ TA"));
    }
#else
    QAction* copyMsgAction = menu.addAction(_("context.copy_message"));
    QAction* selectAllAction = menu.addAction(_("context.select_all"));
    QAction* copyNickAction = nullptr;
    QAction* mentionAction = nullptr;
    if (onName) {
        copyNickAction = menu.addAction(qFromUtf8("复制昵称"));
        mentionAction = menu.addAction(qFromUtf8("@ TA"));
    }
#endif
#ifdef QT3_BUILD
    int choice = menu.exec(event->globalPos());
    if (choice == copyMsgId) {
        copyFullMessage(msgIndex);
    } else if (choice == selectAllId) {
        // Select all text in all messages
        m_selMsgIndex = 0;
        m_selStart = 0;
        m_selEnd = m_items.empty() ? 0 : m_items.back().messageText.length();
        updateFull();
    } else if (choice == copyNickId) {
        QApplication::clipboard()->setText(displayName);
    } else if (choice == mentionId) {
        emit mentionClicked(m_items[msgIndex].senderName);
    }
#else
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == copyMsgAction) {
        copyFullMessage(msgIndex);
    } else if (chosen == selectAllAction) {
        m_selMsgIndex = 0;
        m_selStart = 0;
        m_selEnd = m_items.empty() ? 0 : m_items.back().messageText.length();
        updateFull();
    } else if (chosen == copyNickAction) {
        QApplication::clipboard()->setText(displayName);
    } else if (chosen == mentionAction) {
        emit mentionClicked(m_items[msgIndex].senderName);
    }
#endif
}

void ChatView::manageAnimations() {
    int viewBottom = m_scrollPos + height();
    int first = findByAbsY(m_scrollPos);
    if (first < 0) { first = 0; }
    int absY = msgAbsY(first);
    int y = absY - m_scrollPos;
    for (size_t i = first; i < m_items.size(); ++i) {
        int h = m_items[i].height;
        bool visible = (absY + h > m_scrollPos) && (absY < viewBottom);
        if (m_items[i].etype == ChatElement::Gif) {
            if (visible) {
                if (!m_items[i].movie) {
                    m_items[i].startAnimation(this, i);
                }
                if (m_items[i].movie && i < m_gifFrameUpdated.size() && m_gifFrameUpdated[i]) {
                    m_gifFrameUpdated[i] = 0;
                    update(QRect(0, y, width(), h));
                }
            } else {
                m_items[i].stopAnimation();
            }
        }
        if (absY > viewBottom) { break; }
        absY += h;
        y += h;
    }
}

void ChatView::onAnimTick() {
    manageAnimations();
}

void ChatView::onGifFrameUpdated(int msgIndex) {
    if (msgIndex >= 0 && msgIndex < (int)m_gifFrameUpdated.size()) {
        m_gifFrameUpdated[msgIndex] = 1;
    }
}

std::pair<int,int> ChatView::visibleMessageRange() const {
    int top = m_scrollPos;
    int bottom = top + height();
    int first = findByAbsY(top);
    if (first < 0) { return {-1, -1}; }
    int absY = msgAbsY(first);
    int last = first;
    while (last + 1 < (int)m_items.size() && absY + m_items[last].height < bottom) {
        absY += m_items[last].height;
        last++;
    }
    return {first, last};
}

void ChatView::triggerVisibleDownloads() {
    if (m_items.empty()) {
        qWarning("[VisibleTrigger] empty items, skipping");
        return;
    }
    auto range = visibleMessageRange();
    int first = range.first, last = range.second;
    if (first < 0) { return; }
    qWarning("[VisibleTrigger] visible range: %d ~ %d", first, last);
    for (int i = first; i <= last; i++) {
        ChatElement& el = m_items[i];
        if (!el.downloadRequested && el.downloadState == ChatElement::NotRequested
            && el.thumbnail.isNull() && !el.mediaUrl.isEmpty()) {
            el.downloadRequested = true;
            qWarning("[VisibleTrigger] would download idx=%d type=%d url=%s",
                     i, (int)el.etype, qToUtf8(el.mediaUrl).data());
        }
    }
}

void ChatView::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setClipRect(event->rect());
    p.fillRect(event->rect(), currentPalette().windowBg);

    int viewW = contentWidth();
    int vpH = height();

    std::vector<QRect> selRects;
    if (m_selMsgIndex >= 0 && m_selMsgIndex < (int)m_items.size()) {
        selRects = selectionRects(m_selMsgIndex);
    }

    int first = findByAbsY(m_scrollPos);
    if (first < 0) { first = 0; }
    int absY = msgAbsY(first);
    int y = absY - m_scrollPos;
    for (size_t i = first; i < m_items.size(); ++i) {
        int h = m_items[i].height;
        if (y + h >= 0 && y <= vpH) {
            m_items[i].paint(p, y, viewW, ((int)i == m_selMsgIndex), selRects,
                             m_fm, m_emojiW, font(), currentPalette());
        }
        if (y > vpH) { break; }
        y += h;
        absY += h;
    }
    m_scrollDownPill.paint(p, rect(), currentPalette().windowBg, currentPalette().textPrimary);
}

void ChatView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    int sbw = m_vScrollBar->sizeHint().width();
    m_vScrollBar->setGeometry(width() - sbw, 0, sbw, height());
    relayout();
    triggerVisibleDownloads();
}

void ChatView::onScrollChanged(int value) {
    int delta = m_scrollPos - value;
    m_scrollPos = value;
    QWidget::scroll(0, delta, rect());
#ifdef QT3_BUILD
    int maxVal = m_vScrollBar->maxValue();
#else
    int maxVal = m_vScrollBar->maximum();
#endif
    if (maxVal - value <= 20 && m_scrollDownPill.count() > 0) {
        m_scrollDownPill.setCount(0);
        updateRect(m_scrollDownPill.rect());
    }
    m_vScrollBar->showTemporarily();
    triggerVisibleDownloads();
}
