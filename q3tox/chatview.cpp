#include "chatview.h"
#include <algorithm>

#ifdef QT3_BUILD
#include <qpainter.h>
#include <qscrollbar.h>
#else
#include <QPainter>
#include <QScrollBar>
#endif

static const QColor kAvatarBg("#30363d");
static const QColor kAvatarTextColor("#8b949e");
static const QColor kBubbleBg("#21262d");
static const QColor kBubbleTextColor("#c9d1d9");
static const QColor kHeaderNameColor("#c9d1d9");
static const QColor kHeaderTimeColor("#8b949e");
static const QColor kWindowBg("#0d1117");

ChatView::ChatView(QWidget* parent)
    : QWidget(parent)
{
    m_totalHeight = 0;
    m_scrollPos = 0;

    m_vScrollBar = new QScrollBar(QScrollBar::Vertical, this);
#ifdef QT3_BUILD
    m_vScrollBar->setSteps(10, 50);
#else
    m_vScrollBar->setSingleStep(10);
    m_vScrollBar->setPageStep(50);
#endif
    connect(m_vScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onScrollChanged(int)));
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

    int bubbleTextWidth = viewWidth - 3 * kPad - kAvatarSize - 2 * kBubbleHPad;
    if (bubbleTextWidth < 50) bubbleTextWidth = 50;

    int lineCount = 0;
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
    if (lineCount < 1) lineCount = 1;

    int textHeight = lineCount * fm.lineSpacing();
    int bubbleHeight = 2 * kBubbleVPad + std::max(textHeight, fm.lineSpacing());
    int headerHeight = fm.lineSpacing() + kPad;
    int contentHeight = headerHeight + bubbleHeight;
    return std::max(contentHeight, kAvatarSize + 2 * kPad) + kMsgSpacing;
}

void ChatView::drawMessage(QPainter& p, const ChatMessage& msg, int y, int viewWidth) {
    QFont f = font();
    QFontMetrics fm(f);
    int headerH = fm.lineSpacing();

    if (msg.type == "self") {
        int ax = viewWidth - kPad - kAvatarSize;
        p.setBrush(kAvatarBg);
        p.setPen(Qt::NoPen);
        p.drawEllipse(ax, y + kPad, kAvatarSize, kAvatarSize);

        if (!msg.avatarText.isEmpty()) {
            p.setPen(kAvatarTextColor);
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
        p.setPen(kHeaderNameColor);
        int nameW = fm.width(msg.sender) + fm.width("  ");
        p.drawText(contentRight - nameW, y + kPad, nameW, headerH, Qt::AlignRight | Qt::AlignVCenter, msg.sender);

        f.setPointSize(10);
        p.setFont(f);
        p.setPen(kHeaderTimeColor);
        p.drawText(contentRight - nameW - fm.width(msg.time) - kPad, y + kPad,
                   fm.width(msg.time), headerH, Qt::AlignRight | Qt::AlignVCenter, msg.time);

        p.setFont(font());

        int bubbleX = contentRight - bubbleW;
        int bubbleY = y + kPad + headerH + kPad;
        int bubbleH = msg.height - (kPad + headerH + kPad) - kMsgSpacing;
        QRect bubbleRect(bubbleX, bubbleY, bubbleW, bubbleH);

        p.setBrush(kBubbleBg);
        p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
        p.drawRoundRect(bubbleRect, 4, 4);
#else
        p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif

        QRect textRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                       bubbleRect.width() - 2 * kBubbleHPad, bubbleRect.height() - 2 * kBubbleVPad);
        p.setPen(kBubbleTextColor);
        p.setFont(font());
#ifdef QT3_BUILD
        p.drawText(textRect, Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop, msg.messageText);
#else
        p.drawText(textRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, msg.messageText);
#endif
    } else {
        int ax = kPad;
        p.setBrush(kAvatarBg);
        p.setPen(Qt::NoPen);
        p.drawEllipse(ax, y + kPad, kAvatarSize, kAvatarSize);

        if (!msg.avatarText.isEmpty()) {
            p.setPen(kAvatarTextColor);
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
        p.setPen(kHeaderNameColor);
        p.drawText(contentX, y + kPad, contentW, headerH, Qt::AlignLeft | Qt::AlignVCenter, msg.sender);

        f.setPointSize(10);
        p.setFont(f);
        p.setPen(kHeaderTimeColor);
        int timeX = contentX + fm.width(msg.sender) + kPad;
        p.drawText(timeX, y + kPad, contentW - (timeX - contentX), headerH, Qt::AlignLeft | Qt::AlignVCenter, msg.time);

        p.setFont(font());

        int bubbleW = (contentW * 80) / 100;
        if (bubbleW < 100) bubbleW = contentW;
        int bubbleY = y + kPad + headerH + kPad;
        int bubbleH = msg.height - (kPad + headerH + kPad) - kMsgSpacing;
        if (bubbleH < 30) bubbleH = 30;
        QRect bubbleRect(contentX, bubbleY, bubbleW, bubbleH);

        p.setBrush(kBubbleBg);
        p.setPen(Qt::NoPen);
#ifdef QT3_BUILD
        p.drawRoundRect(bubbleRect, 4, 4);
#else
        p.drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);
#endif

        QRect textRect(bubbleRect.x() + kBubbleHPad, bubbleRect.y() + kBubbleVPad,
                       bubbleRect.width() - 2 * kBubbleHPad, bubbleRect.height() - 2 * kBubbleVPad);
        p.setPen(kBubbleTextColor);
        p.setFont(font());
#ifdef QT3_BUILD
        p.drawText(textRect, Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop, msg.messageText);
#else
        p.drawText(textRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, msg.messageText);
#endif
    }
}

void ChatView::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setClipRect(event->rect());
    p.fillRect(event->rect(), kWindowBg);

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
    update();
}
