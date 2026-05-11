#include "chatview.h"
#include "LimeScrollBar.h"
#include "LimeStyle.h"
#ifdef EMOJI_RENDER_QT34
#include "emojiutil.h"
#endif
#include <algorithm>

#ifdef QT3_BUILD
#include <qpainter.h>
#else
#include <QPainter>
#include <QWheelEvent>
#include <QKeyEvent>
#endif

ChatView::ChatView(QWidget* parent)
    : QWidget(parent)
{
    m_totalHeight = 0;
    m_scrollPos = 0;

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
}

void ChatView::appendMessage(const ChatMessage& msg) {
    m_messages.push_back(msg);
    relayout();
    scrollToBottom();
}

void ChatView::clearMessages() {
    m_messages.clear();
    m_totalHeight = 0;
    m_scrollPos = 0;
    m_vScrollBar->setRange(0, 0);
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

int ChatView::contentWidth() const {
    int sbw = m_vScrollBar->sizeHint().width();
    return width() - sbw;
}

void ChatView::relayout() {
    int w = contentWidth();
    if (w <= 0) w = 400;

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

int ChatView::calcMessageHeight(const ChatMessage& msg, int viewWidth) const {
    QFont f = font();
    QFontMetrics fm(f);

    if (viewWidth <= 0) viewWidth = 400;

    int contentW = viewWidth - 3 * kPad - kAvatarSize;
    int bubbleW = (contentW * 80) / 100;
    if (bubbleW < 100) bubbleW = contentW;
    int bubbleTextWidth = bubbleW - 2 * kBubbleHPad;
    if (bubbleTextWidth < 20) bubbleTextWidth = 20;

    int lineCount = 0;
#ifdef EMOJI_RENDER_QT34
    auto cps = toCodepoints(msg.messageText);
    int tLen = (int)cps.size();
    int pos = 0;
    while (pos < tLen) {
        if (cps[pos] == '\n') { lineCount++; pos++; continue; }
        int lineWidth = 0, lastSpace = -1, end = pos;
        while (end < tLen && cps[end] != '\n') {
            int cw = isEmojiChar(cps[end]) ? emojiCharWidth(fm) : fm.width(QChar((ushort)cps[end]));
            lineWidth += cw;
            if (cps[end] == ' ') lastSpace = end;
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
    int textLen = msg.messageText.length();
    int pos = 0;
    while (pos < textLen) {
        if (msg.messageText[pos] == '\n') {
            lineCount++;
            pos++;
            continue;
        }
        int lineWidth = 0;
        int lastSpace = -1;
        int end = pos;
        while (end < textLen && msg.messageText[end] != '\n') {
            lineWidth += fm.width(msg.messageText[end]);
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
    if (lineCount < 1) lineCount = 1;

    int textHeight = lineCount * fm.lineSpacing();
    int bubbleHeight = 2 * kBubbleVPad + std::max(textHeight, fm.lineSpacing());
    int headerHeight = fm.lineSpacing() + kPad;
    int contentHeight = kPad + headerHeight + bubbleHeight;
    int avatarTotal = kPad + kAvatarSize;
    return std::max(contentHeight, avatarTotal) + kMsgSpacing;
}

void ChatView::drawMessage(QPainter& p, const ChatMessage& msg, int y, int viewWidth) {
    QFont f = font();
    QFontMetrics fm(f);
    int headerH = fm.lineSpacing();

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

        f.setPointSize(11);
        p.setFont(f);
        p.setPen(currentPalette().textPrimary);
        int nameW = fm.width(msg.sender) + fm.width("  ");
        p.drawText(contentRight - nameW, y + kPad, nameW, headerH, Qt::AlignRight | Qt::AlignVCenter, msg.sender);

        f.setPointSize(10);
        p.setFont(f);
        p.setPen(currentPalette().textMuted);
        p.drawText(contentRight - nameW - fm.width(msg.time) - kPad, y + kPad,
                   fm.width(msg.time), headerH, Qt::AlignRight | Qt::AlignVCenter, msg.time);

        p.setFont(font());

        int bubbleX = contentRight - bubbleW;
        int bubbleY = y + kPad + headerH + kPad;
        int bubbleH = msg.height - (kPad + headerH + kPad) - kMsgSpacing;
        QRect bubbleRect(bubbleX, bubbleY, bubbleW, bubbleH);

        p.setBrush(currentPalette().baseBg);
        p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
        p.drawRoundRect(bubbleRect, 4, 4);
#else
        p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif

        QRect textRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                       bubbleRect.width() - 2 * kBubbleHPad, bubbleRect.height() - 2 * kBubbleVPad);
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

        f.setPointSize(11);
        p.setFont(f);
        p.setPen(currentPalette().textPrimary);
        p.drawText(contentX, y + kPad, contentW, headerH, Qt::AlignLeft | Qt::AlignVCenter, msg.sender);

        f.setPointSize(10);
        p.setFont(f);
        p.setPen(currentPalette().textMuted);
        int timeX = contentX + fm.width(msg.sender) + kPad;
        p.drawText(timeX, y + kPad, contentW - (timeX - contentX), headerH, Qt::AlignLeft | Qt::AlignVCenter, msg.time);

        p.setFont(font());

        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) bubbleW = contentW;
        int bubbleY = y + kPad + headerH + kPad;
        int bubbleH = msg.height - (kPad + headerH + kPad) - kMsgSpacing;
        if (bubbleH < 30) bubbleH = 30;
        QRect bubbleRect(contentX, bubbleY, bubbleW, bubbleH);

        p.setBrush(currentPalette().baseBg);
        p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
        p.drawRoundRect(bubbleRect, 4, 4);
#else
        p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif

        QRect textRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                       bubbleRect.width() - 2 * kBubbleHPad, bubbleRect.height() - 2 * kBubbleVPad);
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
}

void ChatView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    int sbw = m_vScrollBar->sizeHint().width();
    m_vScrollBar->setGeometry(width() - sbw, 0, sbw, height());
    relayout();
}

void ChatView::onScrollChanged(int value) {
    m_scrollPos = value;
    m_vScrollBar->showTemporarily();
    update();
}
