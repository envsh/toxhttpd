#include "chatview.h"
#include "LimeScrollBar.h"
#include "LimeStyle.h"
#ifdef EMOJI_RENDER_QT34
#include "emojiutil.h"
#endif
#include <algorithm>
#include <cstdlib>
#include "translator.h"
#ifdef QT3_BUILD
#include <qpainter.h>
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
#endif

// ───── ChatElement methods ─────

int ChatElement::calcHeight(int viewWidth, const QFontMetrics& fm, int emojiW) {
    switch (etype) {
    case Text: {
        if (viewWidth <= 0) { viewWidth = 400; }

        int kAvatarSize = 48;
        int kPad = 8;
        int kBubbleHPad = 12;
        int kBubbleVPad = 8;
        int kMsgSpacing = 8;

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
            int transLineCount = 0;
            int tLen2 = translatedText.length();
            int tPos = 0;
            while (tPos < tLen2) {
                if (translatedText[tPos] == '\n') { transLineCount++; tPos++; continue; }
                int lineWidth = 0;
                int lastSpace = -1;
                int end = tPos;
                while (end < tLen2 && translatedText[end] != '\n') {
                    lineWidth += fm.width(translatedText[end]);
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
            bubbleHeight += kBubbleVPad / 2 + transLineCount * fm.lineSpacing() + kBubbleVPad;
        }

        int headerHeight = fm.lineSpacing() + kPad;
        int contentHeight = kPad + headerHeight + bubbleHeight;
        int avatarTotal = kPad + kAvatarSize;
        return std::max(contentHeight, avatarTotal) + kMsgSpacing;
    }
    case Image:
    case Gif:
    case Video:
        return 0;
    case File: {
        int kAvatarSize = 48;
        int kPad = 8;
        int kBubbleHPad = 12;
        int kBubbleVPad = 8;
        int kMsgSpacing = 8;

        if (viewWidth <= 0) { viewWidth = 400; }

        int contentW = viewWidth - 3 * kPad - kAvatarSize;
        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) { bubbleW = contentW; }
        int innerW = bubbleW - 2 * kBubbleHPad;

        int iconSize = 48;
        int textW = innerW - iconSize - kPad;
        if (textW < 20) { textW = 20; }

        int nameLines = 0;
        QString displayText = !caption.isEmpty() ? caption : fileName;
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

        int bubbleHeight = 2 * kBubbleVPad
                         + std::max(textHeight + kPad, iconSize);
        if (bubbleHeight < 2 * kBubbleVPad + iconSize) {
            bubbleHeight = 2 * kBubbleVPad + iconSize;
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

        int kAvatarSize = 48;
        int kPad = 8;
        int kBubbleHPad = 12;
        int kBubbleVPad = 8;
        int kBubbleRadius = 8;
        int kMsgSpacing = 8;

        if (category == "self") {
            int ax = viewWidth - kPad - kAvatarSize;
            p.setBrush(pal.surfaceBg);
            p.setPen(Qt::NoPen);
            p.drawEllipse(ax, y + kPad, kAvatarSize, kAvatarSize);

            if (!avatarText.isEmpty()) {
                p.setPen(pal.textMuted);
                f.setPointSize(18);
                f.setBold(true);
                p.setFont(f);
                p.drawText(ax, y + kPad, kAvatarSize, kAvatarSize, Qt::AlignCenter, qToUpper(avatarText.left(1)));
                p.setFont(baseFont);
            }

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
            p.setBrush(pal.surfaceBg);
            p.setPen(Qt::NoPen);
            p.drawEllipse(ax, y + kPad, kAvatarSize, kAvatarSize);

            if (!avatarText.isEmpty()) {
                p.setPen(pal.textMuted);
                f.setPointSize(18);
                f.setBold(true);
                p.setFont(f);
                p.drawText(ax, y + kPad, kAvatarSize, kAvatarSize, Qt::AlignCenter, qToUpper(avatarText.left(1)));
                p.setFont(baseFont);
            }

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
                f.setItalic(true);
                p.setFont(f);
#ifdef EMOJI_RENDER_QT34
                EmojiRenderer::instance().drawText(p, transRect, translatedText);
#else
#  ifdef QT3_BUILD
                p.drawText(transRect, Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop, translatedText);
#  else
                p.drawText(transRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, translatedText);
#  endif
#endif
                p.setFont(baseFont);
            }
        }
        break;
    }
    case Image:
    case Gif:
    case Video:
        break;
    case File: {
        QFont f = baseFont;
        int headerH = fm.lineSpacing();
        QRect bubbleRect;

        int kAvatarSize = 48;
        int kPad = 8;
        int kBubbleHPad = 12;
        int kBubbleVPad = 8;
        int kBubbleRadius = 8;
        int kMsgSpacing = 8;

        const int hdrBtnSize = 18;
        const int hdrBtnGap = 4;
        int hdrBtnAreaW = 2 * hdrBtnSize + hdrBtnGap;

        if (category == "self") {
            // ── self ──
            int ax = viewWidth - kPad - kAvatarSize;
            p.setBrush(pal.surfaceBg);
            p.setPen(Qt::NoPen);
            p.drawEllipse(ax, y + kPad, kAvatarSize, kAvatarSize);
            if (!avatarText.isEmpty()) {
                p.setPen(pal.textMuted);
                f.setPointSize(18);
                f.setBold(true);
                p.setFont(f);
                p.drawText(ax, y + kPad, kAvatarSize, kAvatarSize,
                           Qt::AlignCenter, qToUpper(avatarText.left(1)));
                p.setFont(baseFont);
            }

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
            QString displayName = !caption.isEmpty() ? caption : fileName;
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
            if (!messageText.isEmpty()) {
                p.setPen(pal.textMuted);
                f.setPointSize(10);
                p.setFont(f);
                p.drawText(textX, iconY + fm.lineSpacing() + kPad/2, textW, fm.lineSpacing(),
                           Qt::AlignLeft | Qt::AlignVCenter, messageText);
                p.setFont(baseFont);
            }
        } else {
            // ── other ──
            int ax = kPad;
            p.setBrush(pal.surfaceBg);
            p.setPen(Qt::NoPen);
            p.drawEllipse(ax, y + kPad, kAvatarSize, kAvatarSize);
            if (!avatarText.isEmpty()) {
                p.setPen(pal.textMuted);
                f.setPointSize(18); f.setBold(true); p.setFont(f);
                p.drawText(ax, y + kPad, kAvatarSize, kAvatarSize,
                           Qt::AlignCenter, qToUpper(avatarText.left(1)));
                p.setFont(baseFont);
            }

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
            QString displayName = !caption.isEmpty() ? caption : fileName;
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
            if (!messageText.isEmpty()) {
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
        break;
    }
    }
}

void ChatElement::startAnimation(QWidget* parent) {
    if (etype != Gif || movie) { return; }
#ifndef QT3_BUILD
    movie = new QMovie(gifPath);
    QObject::connect(movie, SIGNAL(updated(const QRect&)), parent, SLOT(update()));
    movie->start();
#else
    Q_UNUSED(parent);
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
}

ChatView::~ChatView() {
    delete[] m_bmpW;
}

void ChatView::restoreMessages(const std::vector<ChatElement>& msgs) {
    m_scrollDownPill.setCount(0);
    m_items = msgs;
    relayout();
    scrollToBottom();
}

void ChatView::appendMessage(const ChatElement& msg) {
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
    ChatElement& el = m_items.back();
    el.height = el.calcHeight(w, m_fm, m_emojiW);
    el.cachedWidth = (short)w;
    m_totalHeight += el.height;

    int vpH = height();
    int maxScroll = std::max(0, m_totalHeight - vpH);
    m_vScrollBar->setRange(0, maxScroll);
    m_vScrollBar->setPageStep(vpH);
    if (curVal == oldMax && oldMax > 0) {
        m_vScrollBar->setValue(maxScroll);
    }

    if (atBottom) {
        scrollToBottom();
    } else {
        m_scrollDownPill.setCount(m_scrollDownPill.count() + 1);
    }
    update();
}

void ChatView::clearMessages() {
    m_items.clear();
    m_totalHeight = 0;
    m_scrollPos = 0;
    m_vScrollBar->setRange(0, 0);
    m_selMsgIndex = -1;
    m_selStart = m_selEnd = 0;
    m_scrollDownPill.setCount(0);
    update();
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
            m_items[i].height = m_items[i].calcHeight(w, m_fm, m_emojiW);
            m_items[i].cachedWidth = (short)w;
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
    m_vScrollBar->setRange(0, maxScroll);
    m_vScrollBar->setPageStep(vpH);
    if (oldVal == oldMax && oldMax > 0) {
        m_vScrollBar->setValue(maxScroll);
    }
    update();
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
    int curY = kPad - m_scrollPos;
    for (size_t i = 0; i < m_items.size(); ++i) {
        int h = m_items[i].height;
        if (y >= curY && y < curY + h) { return (int)i; }
        curY += h;
    }
    return -1;
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
    const QString& text = m_items[msgIndex].messageText;
    int start, end;
    wordBoundaries(text, charPos, start, end);
    m_selMsgIndex = msgIndex;
    m_selStart = start;
    m_selEnd = end;
    m_selecting = false;
    update();
}

void ChatView::selectLineAt(int msgIndex, int charPos) {
    if (msgIndex < 0 || msgIndex >= (int)m_items.size()) return;
    const QString& text = m_items[msgIndex].messageText;
    int start, end;
    lineBoundaries(text, charPos, start, end);
    m_selMsgIndex = msgIndex;
    m_selStart = start;
    m_selEnd = end;
    m_selecting = false;
    update();
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
                m_selMsgIndex = msgIndex;
                m_selStart = charPos;
                m_selEnd = charPos;
                m_selecting = true;
                update();
            }
        } else {
            // Click outside messages, clear selection
            if (m_selMsgIndex != -1) {
                m_selMsgIndex = -1;
                update();
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
        update();
    }
    if (overPill) {
        setCursor(QCursor(Qt::PointingHandCursor));
        QWidget::mouseMoveEvent(event);
        return;
    }
    if (m_selecting && m_selMsgIndex >= 0 && m_selMsgIndex < (int)m_items.size()) {
        int msgIndex = findMessageAtY(event->y());
        if (msgIndex == m_selMsgIndex) {
            int msgY = kPad - m_scrollPos;
            for (int i = 0; i < msgIndex; i++) { msgY += m_items[i].height; }
            int localY = event->y() - msgY;
            int localX = event->x();
            int charPos = charPosAt(msgIndex, localX, localY);
            if (charPos >= 0) {
                m_selEnd = charPos;
                update(0, msgY, width(), m_items[m_selMsgIndex].height);
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
                int msgY = kPad - m_scrollPos;
                for (int i = 0; i < msgIndex; i++) { msgY += m_items[i].height; }
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
        int curY = kPad - m_scrollPos;
        for (int i = 0; i < msgIndex; i++) { curY += m_items[i].height; }
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
        if (m_selecting) {
            m_selecting = false;
            // If start and end are same, clear selection
            if (m_selStart == m_selEnd) {
                m_selMsgIndex = -1;
                update();
            }
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void ChatView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        int msgIndex = findMessageAtY(event->y());
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
#ifdef QT3_BUILD
    QPopupMenu menu(this);
#else
    QMenu menu(this);
#endif
    // Copy full message
#ifdef QT3_BUILD
    int copyMsgId = menu.insertItem(_("context.copy_message"));
    int selectAllId = menu.insertItem(_("context.select_all"));
#else
    QAction* copyMsgAction = menu.addAction(_("context.copy_message"));
    QAction* selectAllAction = menu.addAction(_("context.select_all"));
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
        update();
    }
#else
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == copyMsgAction) {
        copyFullMessage(msgIndex);
    } else if (chosen == selectAllAction) {
        m_selMsgIndex = 0;
        m_selStart = 0;
        m_selEnd = m_items.empty() ? 0 : m_items.back().messageText.length();
        update();
    }
#endif
}

void ChatView::manageAnimations() {
    int viewTop = m_scrollPos;
    int viewBottom = m_scrollPos + height();
    int y = kPad - m_scrollPos;
    for (size_t i = 0; i < m_items.size(); ++i) {
        int h = m_items[i].height;
        bool visible = (y + h > kPad - m_scrollPos) && (y < viewBottom);
        if (m_items[i].etype == ChatElement::Gif) {
            if (visible) {
                if (!m_items[i].movie) {
                    m_items[i].startAnimation(this);
                }
            } else {
                m_items[i].stopAnimation();
            }
        }
        y += h;
    }
}

void ChatView::paintEvent(QPaintEvent* event) {
    manageAnimations();
    QPainter p(this);
    p.setClipRect(event->rect());
    p.fillRect(event->rect(), currentPalette().windowBg);

    int viewW = contentWidth();
    int vpH = height();

    std::vector<QRect> selRects;
    if (m_selMsgIndex >= 0 && m_selMsgIndex < (int)m_items.size()) {
        selRects = selectionRects(m_selMsgIndex);
    }

    int y = kPad - m_scrollPos;
    for (size_t i = 0; i < m_items.size(); ++i) {
        int h = m_items[i].height;
        if (y + h >= 0 && y <= vpH) {
            m_items[i].paint(p, y, viewW, ((int)i == m_selMsgIndex), selRects,
                             m_fm, m_emojiW, font(), currentPalette());
        }
        y += h;
    }
    m_scrollDownPill.paint(p, rect(), currentPalette().windowBg, currentPalette().textPrimary);
}

void ChatView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    int sbw = m_vScrollBar->sizeHint().width();
    m_vScrollBar->setGeometry(width() - sbw, 0, sbw, height());
    relayout();
}

void ChatView::onScrollChanged(int value) {
    m_scrollPos = value;
#ifdef QT3_BUILD
    int maxVal = m_vScrollBar->maxValue();
#else
    int maxVal = m_vScrollBar->maximum();
#endif
    if (maxVal - value <= 20 && m_scrollDownPill.count() > 0) {
        m_scrollDownPill.setCount(0);
    }
    m_vScrollBar->showTemporarily();
    update();
}
