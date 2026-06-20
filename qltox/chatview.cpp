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



// Helper to open URL across Qt3/Qt4
/*
 static void qOpenUrl(const QString& url) {
#ifdef QT3_BUILD
    // In Qt3, use system() to open URL via xdg-open
    QString cmd = QString("xdg-open \"%1\"").arg(url);
    system(cmd.local8Bit().data());
#else
    QDesktopServices::openUrl(QUrl(url));
#endif
}
*/

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
    m_messages = msgs;
    relayout();
    scrollToBottom();
}

void ChatView::appendMessage(const ChatElement& msg) {
    int curVal = m_vScrollBar->value();
#ifdef QT3_BUILD
    int maxVal = m_vScrollBar->maxValue();
#else
    int maxVal = m_vScrollBar->maximum();
#endif
    bool atBottom = (maxVal - curVal) <= 20;

    m_messages.push_back(msg);
    relayout();
    if (atBottom) {
        scrollToBottom();
    } else {
        m_scrollDownPill.setCount(m_scrollDownPill.count() + 1);
    }
}

void ChatView::clearMessages() {
    m_messages.clear();
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
    return m_messages[index];
}

int ChatView::messageCount() const {
    return (int)m_messages.size();
}

void ChatView::triggerRelayout() {
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
    for (size_t i = 0; i < m_messages.size(); ++i) {
        m_messages[i].height = calcMessageHeight(m_messages[i], w);
        m_totalHeight += m_messages[i].height;
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

int ChatView::calcMessageHeight(const ChatElement& msg, int viewWidth) {
    const QFontMetrics& fm = m_fm;
    if (viewWidth <= 0) { viewWidth = 400; }

    int contentW = viewWidth - 3 * kPad - kAvatarSize;
    int bubbleW = (contentW * 80) / 100;
    if (bubbleW < 100) { bubbleW = contentW; }
    int bubbleTextWidth = bubbleW - 2 * kBubbleHPad;
    if (bubbleTextWidth < 20) { bubbleTextWidth = 20; }

    int lineCount = 0;
#ifdef EMOJI_RENDER_QT34
    auto cps = toCodepoints(msg.messageText);
    int tLen = (int)cps.size();
    int pos = 0;
    while (pos < tLen) {
        if (cps[pos] == '\n') { lineCount++; pos++; continue; }
        int lineWidth = 0, lastSpace = -1, end = pos;
        while (end < tLen && cps[end] != '\n') {
            int cw = isEmojiChar(cps[end]) ? m_emojiW : charWidth(cps[end]);
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
    for (int i = 0; i < textLen; ) {
        if (msg.messageText[i] == '\n') {
            lineCount++;
            i++;
            continue;
        }
        int lineWidth = 0;
        int lastSpace = -1;
        int end = i;
        while (end < textLen && msg.messageText[end] != '\n') {
            int cw = m_fm.width(msg.messageText[end].unicode());
            if (msg.messageText[end].isSpace()) lastSpace = end;
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
#endif
    if (lineCount < 1) { lineCount = 1; }

    int textHeight = lineCount * fm.lineSpacing();
    int minBubbleH = 2 * kBubbleVPad + fm.lineSpacing();
    int bubbleHeight = 2 * kBubbleVPad + std::max(textHeight, fm.lineSpacing());
    if (bubbleHeight < minBubbleH) { bubbleHeight = minBubbleH; }

    // Account for translated text
    if (msg.showTranslation && !msg.translatedText.isEmpty()) {
        int transLineCount = 0;
        int tLen = msg.translatedText.length();
        int tPos = 0;
        while (tPos < tLen) {
            if (msg.translatedText[tPos] == '\n') { transLineCount++; tPos++; continue; }
            int lineWidth = 0;
            int lastSpace = -1;
            int end = tPos;
            while (end < tLen && msg.translatedText[end] != '\n') {
                lineWidth += fm.width(msg.translatedText[end]);
                if (msg.translatedText[end].isSpace()) lastSpace = end;
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
    for (size_t i = 0; i < m_messages.size(); ++i) {
        int h = m_messages[i].height;
        if (y >= curY && y < curY + h) { return (int)i; }
        curY += h;
    }
    return -1;
}

// Get character position in a message from local coordinates
int ChatView::charPosAt(int msgIndex, int localX, int localY) {
    if (msgIndex < 0 || msgIndex >= (int)m_messages.size()) return -1;
    const ChatElement& msg = m_messages[msgIndex];
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
    if (msg.type == "self") {
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

    const ChatElement& msg = m_messages[msgIndex];
    int textLen = msg.messageText.length();
    if (start >= textLen || end <= 0) { return rects; }

    // Compute text rectangle
    QFont f = font();
    QFontMetrics fm(f);
    int viewW = contentWidth();
    int headerH = fm.lineSpacing();

    QRect textRect;
    if (msg.type == "self") {
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
    if (m_selMsgIndex < 0 || m_selMsgIndex >= (int)m_messages.size()) return QString();
    int start = std::min(m_selStart, m_selEnd);
    int end = std::max(m_selStart, m_selEnd);
    if (start == end) { return QString(); }
    return m_messages[m_selMsgIndex].messageText.mid(start, end - start);
}

void ChatView::selectWordAt(int msgIndex, int charPos) {
    if (msgIndex < 0 || msgIndex >= (int)m_messages.size()) return;
    const QString& text = m_messages[msgIndex].messageText;
    int start, end;
    wordBoundaries(text, charPos, start, end);
    m_selMsgIndex = msgIndex;
    m_selStart = start;
    m_selEnd = end;
    m_selecting = false;
    update();
}

void ChatView::selectLineAt(int msgIndex, int charPos) {
    if (msgIndex < 0 || msgIndex >= (int)m_messages.size()) return;
    const QString& text = m_messages[msgIndex].messageText;
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
    if (msgIndex >= 0 && msgIndex < (int)m_messages.size()) {
        QApplication::clipboard()->setText(m_messages[msgIndex].messageText);
    }
}

void ChatView::drawMessage(QPainter& p, ChatElement& msg, int y, int viewWidth) {
    QFont f = font();
    QFontMetrics fm(f);
    int headerH = fm.lineSpacing();
    QRect textRect;
    QRect bubbleRect;

    // Header button constants
    const int hdrBtnSize = 18;
    const int hdrBtnGap = 4;
    int hdrBtnCnt = 2;
    int hdrBtnAreaW = hdrBtnCnt * hdrBtnSize + (hdrBtnCnt - 1) * hdrBtnGap;

    if (msg.type == "self") {
        int ax = viewWidth - kPad - kAvatarSize;
        p.setBrush(currentPalette().surfaceBg);
        p.setPen(Qt::NoPen);
        p.drawEllipse(ax, y + kPad, kAvatarSize, kAvatarSize);

        if (!msg.avatarText.isEmpty()) {
            p.setPen(currentPalette().textMuted);
            f.setPointSize(18);
            f.setBold(true);
            p.setFont(f);
            p.drawText(ax, y + kPad, kAvatarSize, kAvatarSize, Qt::AlignCenter, qToUpper(msg.avatarText.left(1)));
            p.setFont(font());
        }

        int contentRight = viewWidth - 2 * kPad - kAvatarSize;
        int contentLeft = kPad;
        int bubbleMaxW = contentRight - contentLeft;
        int bubbleW = std::min(bubbleMaxW, (bubbleMaxW * 80) / 100);

        // Header action buttons
        int hdrTextRight = contentRight - hdrBtnAreaW - kPad / 2;

        f.setPointSize(11);
        p.setFont(f);
        p.setPen(currentPalette().textMuted);
        QString displayName;
        if (!msg.senderName.isEmpty())
            displayName = msg.senderName;
        else if (msg.peerNumber >= 0)
            displayName = QString("Peer %1").arg(msg.peerNumber);
        else
            displayName = "?";
        int nameW = fm.width(displayName) + fm.width("  ");
        p.drawText(hdrTextRight - nameW, y + kPad, nameW, headerH, Qt::AlignRight | Qt::AlignVCenter, displayName);

        int ipW = 0;
        if (!msg.ipAddress.isEmpty()) {
            f.setPointSize(10);
            p.setFont(f);
            p.setPen(QColor(130, 140, 150));
            ipW = fm.width(msg.ipAddress);
            p.drawText(hdrTextRight - nameW - ipW - kPad/2, y + kPad,
                       ipW, headerH, Qt::AlignLeft | Qt::AlignVCenter, msg.ipAddress);
        }

        f.setPointSize(10);
        p.setFont(f);
        p.setPen(currentPalette().textMuted);
        p.drawText(hdrTextRight - nameW - ipW - kPad/2 - fm.width(msg.time) - kPad/2, y + kPad,
                   fm.width(msg.time), headerH, Qt::AlignRight | Qt::AlignVCenter, msg.time);

        p.setFont(font());

        // Draw header buttons
        {
            int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
            int btnX0 = contentRight - hdrBtnAreaW;
            msg.sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
            msg.translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
            p.setPen(currentPalette().textMuted);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(msg.sourceBtnRect);
            p.drawText(msg.sourceBtnRect, Qt::AlignCenter, qFromUtf8("📋"));
            p.drawEllipse(msg.translateBtnRect);
            p.drawText(msg.translateBtnRect, Qt::AlignCenter, qFromUtf8("🌐"));
        }

        int bubbleX = contentRight - bubbleW;
        int bubbleY = y + kPad + headerH + kPad;
        int bubbleH = msg.height - (kPad + headerH + kPad) - kMsgSpacing;
        bubbleRect = QRect(bubbleX, bubbleY, bubbleW, bubbleH);
        textRect = QRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                         bubbleRect.width() - 2 * kBubbleHPad, bubbleRect.height() - 2 * kBubbleVPad);

        p.setBrush(currentPalette().baseBg);
        p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
        p.drawRoundRect(bubbleRect, 4, 4);
#else
        p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
    } else {
        int ax = kPad;
        p.setBrush(currentPalette().surfaceBg);
        p.setPen(Qt::NoPen);
        p.drawEllipse(ax, y + kPad, kAvatarSize, kAvatarSize);

        if (!msg.avatarText.isEmpty()) {
            p.setPen(currentPalette().textMuted);
            f.setPointSize(18);
            f.setBold(true);
            p.setFont(f);
            p.drawText(ax, y + kPad, kAvatarSize, kAvatarSize, Qt::AlignCenter, qToUpper(msg.avatarText.left(1)));
            p.setFont(font());
        }

        int contentX = 2 * kPad + kAvatarSize;
        int contentW = viewWidth - kPad - contentX;
        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) { bubbleW = contentW; }
        int bubbleRight = contentX + bubbleW;

        // Header action buttons
        int hdrTextRight = bubbleRight - hdrBtnAreaW - kPad / 2;

        f.setPointSize(11);
        p.setFont(f);
        p.setPen(currentPalette().textMuted);
        QString displayName;
        if (!msg.senderName.isEmpty())
            displayName = msg.senderName;
        else if (msg.peerNumber >= 0)
            displayName = QString("Peer %1").arg(msg.peerNumber);
        else
            displayName = "?";
        int maxNameW = hdrTextRight - contentX;
        if (maxNameW < 20) { maxNameW = 20; }
        p.drawText(contentX, y + kPad, maxNameW, headerH, Qt::AlignLeft | Qt::AlignVCenter, displayName);

        f.setPointSize(10);
        p.setFont(f);
        int ipEnd = contentX + fm.width(displayName) + kPad;
        if (!msg.ipAddress.isEmpty()) {
            p.setPen(QColor(130, 140, 150));
            int ipW = fm.width(msg.ipAddress);
            int ipMax = hdrTextRight - ipEnd;
            if (ipMax > ipW) { ipMax = ipW; }
            if (ipMax > 0) {
                p.drawText(ipEnd, y + kPad, ipMax, headerH,
                           Qt::AlignLeft | Qt::AlignVCenter, msg.ipAddress);
            }
            ipEnd += fm.width(msg.ipAddress) + kPad/2;
        }

        p.setPen(currentPalette().textMuted);
        int timeX = ipEnd;
        int timeMaxW = hdrTextRight - timeX;
        if (timeMaxW > 0) {
            p.drawText(timeX, y + kPad, timeMaxW, headerH, Qt::AlignLeft | Qt::AlignVCenter, msg.time);
        }

        p.setFont(font());

        // Draw header buttons
        {
            int btnY = y + kPad + (headerH - hdrBtnSize) / 2;
            int btnX0 = bubbleRight - hdrBtnAreaW;
            msg.sourceBtnRect = QRect(btnX0, btnY, hdrBtnSize, hdrBtnSize);
            msg.translateBtnRect = QRect(btnX0 + hdrBtnSize + hdrBtnGap, btnY, hdrBtnSize, hdrBtnSize);
            p.setPen(currentPalette().textMuted);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(msg.sourceBtnRect);
            p.drawText(msg.sourceBtnRect, Qt::AlignCenter, qFromUtf8("📋"));
            p.drawEllipse(msg.translateBtnRect);
            p.drawText(msg.translateBtnRect, Qt::AlignCenter, qFromUtf8("🌐"));
        }

        int bubbleY = y + kPad + headerH + kPad;
        int bubbleH = msg.height - (kPad + headerH + kPad) - kMsgSpacing;
        if (bubbleH < 30) { bubbleH = 30; }
        bubbleRect = QRect(contentX, bubbleY, bubbleW, bubbleH);
        textRect = QRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                         bubbleRect.width() - 2 * kBubbleHPad, bubbleRect.height() - 2 * kBubbleVPad);

        p.setBrush(currentPalette().baseBg);
        p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
        p.drawRoundRect(bubbleRect, 4, 4);
#else
        p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif
    }

    // Selection highlight
    if (m_selMsgIndex >= 0 && m_selMsgIndex < (int)m_messages.size()) {
        auto& selMsg = m_messages[m_selMsgIndex];
        if (&msg == &selMsg) {
            std::vector<QRect> selRects = selectionRects(m_selMsgIndex);
            QColor selColor = lerpColor(currentPalette().baseBg, currentPalette().accent, 0.25f);
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
    }

    // Draw text
    p.setPen(currentPalette().textPrimary);
    p.setFont(font());
#ifdef EMOJI_RENDER_QT34
    EmojiRenderer::instance().drawText(p, textRect, msg.messageText);
#else
#ifdef QT3_BUILD
    p.drawText(textRect, Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop, msg.messageText);
#else
    p.drawText(textRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, msg.messageText);
#endif
#endif

    // Translated text
    if (msg.showTranslation && !msg.translatedText.isEmpty()) {
        // Calculate original text line count to find where text ends
        int origLineCount = 0;
        int textW = textRect.width();
        if (textW < 20) { textW = 20; }
#ifdef EMOJI_RENDER_QT34
        auto cps = toCodepoints(msg.messageText);
        int tLen = (int)cps.size();
        int pos = 0;
        while (pos < tLen) {
            if (cps[pos] == '\n') { origLineCount++; pos++; continue; }
            int lineWidth = 0, lastSpace = -1, end = pos;
            while (end < tLen && cps[end] != '\n') {
            int cw = isEmojiChar(cps[end]) ? m_emojiW : charWidth(cps[end]);
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
        int tLen = msg.messageText.length();
        int pos = 0;
        while (pos < tLen) {
            if (msg.messageText[pos] == '\n') { origLineCount++; pos++; continue; }
            int lineWidth = 0, lastSpace = -1, end = pos;
            while (end < tLen && msg.messageText[end] != '\n') {
                lineWidth += fm.width(msg.messageText[end]);
                if (msg.messageText[end].isSpace()) lastSpace = end;
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
            p.setPen(currentPalette().textMuted);
            f.setItalic(true);
            p.setFont(f);
#ifdef EMOJI_RENDER_QT34
            EmojiRenderer::instance().drawText(p, transRect, msg.translatedText);
#else
#  ifdef QT3_BUILD
            p.drawText(transRect, Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop, msg.translatedText);
#  else
            p.drawText(transRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, msg.translatedText);
#  endif
#endif
            p.setFont(font());
        }
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
        if (msgIndex >= 0 && msgIndex < (int)m_messages.size()) {
            // Check translate button click
            if (m_messages[msgIndex].translateBtnRect.contains(event->pos())) {
                if (!m_messages[msgIndex].translationInProgress) {
                    emit translateClicked(msgIndex);
                }
                return;
            }

            // Check source button click
            if (m_messages[msgIndex].sourceBtnRect.contains(event->pos())) {
                emit sourceClicked(msgIndex);
                return;
            }

            // Compute local Y relative to message
            int curY = kPad - m_scrollPos;
            for (int i = 0; i < msgIndex; i++) { curY += m_messages[i].height; }
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
                auto links = extractLinks(m_messages[msgIndex].messageText);
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
    if (m_selecting && m_selMsgIndex >= 0 && m_selMsgIndex < (int)m_messages.size()) {
        int msgIndex = findMessageAtY(event->y());
        if (msgIndex == m_selMsgIndex) {
            int msgY = kPad - m_scrollPos;
            for (int i = 0; i < msgIndex; i++) { msgY += m_messages[i].height; }
            int localY = event->y() - msgY;
            int localX = event->x();
            int charPos = charPosAt(msgIndex, localX, localY);
            if (charPos >= 0) {
                m_selEnd = charPos;
                update(0, msgY, width(), m_messages[m_selMsgIndex].height);
            }
        }
    }
    // Set cursor based on whether over URL
    int msgIndex = findMessageAtY(event->y());
    if (msgIndex >= 0 && msgIndex < (int)m_messages.size()) {
        // Check header action buttons first
        if (m_messages[msgIndex].translateBtnRect.contains(event->pos())) {
            QString tip = m_messages[msgIndex].translateError.isEmpty()
                ? qFromUtf8("Translate")
                : m_messages[msgIndex].translateError;
            showTempTooltip(this, m_messages[msgIndex].translateBtnRect, tip);
            setCursor(QCursor(Qt::PointingHandCursor));
            QWidget::mouseMoveEvent(event);
            return;
        }
        if (m_messages[msgIndex].sourceBtnRect.contains(event->pos())) {
            showTempTooltip(this, m_messages[msgIndex].sourceBtnRect, qFromUtf8("Source"));
            setCursor(QCursor(Qt::PointingHandCursor));
            QWidget::mouseMoveEvent(event);
            return;
        }
        // ... compute charPos for link detection
        int curY = kPad - m_scrollPos;
        for (int i = 0; i < msgIndex; i++) { curY += m_messages[i].height; }
        int localY = event->y() - curY;
        int localX = event->x();
        int charPos = charPosAt(msgIndex, localX, localY);
        if (charPos >= 0) {
            auto links = extractLinks(m_messages[msgIndex].messageText);
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
            for (int i = 0; i < msgIndex; i++) { msgY += m_messages[i].height; }
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
        m_selEnd = m_messages.empty() ? 0 : m_messages.back().messageText.length();
        update();
    }
#else
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == copyMsgAction) {
        copyFullMessage(msgIndex);
    } else if (chosen == selectAllAction) {
        m_selMsgIndex = 0;
        m_selStart = 0;
        m_selEnd = m_messages.empty() ? 0 : m_messages.back().messageText.length();
        update();
    }
#endif
}

void ChatView::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setClipRect(event->rect());
    p.fillRect(event->rect(), currentPalette().windowBg);

    int viewW = contentWidth();
    int vpH = height();

    int y = kPad - m_scrollPos;
    for (size_t i = 0; i < m_messages.size(); ++i) {
        int h = m_messages[i].height;
        if (y + h >= 0 && y <= vpH) {
            drawMessage(p, m_messages[i], y, viewW);
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
