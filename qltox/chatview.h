#ifndef CHATVIEW_H
#define CHATVIEW_H

#include "compat34.h"
#include "floatingpill.h"
#include <string>
#include <vector>
#include <cstdint>
#include <qdatetime.h>
#include <qrect.h>
#include <qwidget.h>
#include <qpainter.h>

struct ChatElement {
    QString messageText;
    QString type;
    QString senderName;
    int     peerNumber;
    QString avatarText;
    QString avatarUrl;
    QString time;
    QString ipAddress;
    int height;

    // Translation support
    QString translatedText;
    QString translateError;
    bool showTranslation;
    bool translationInProgress;
    QRect translateBtnRect;
    QRect sourceBtnRect;

    ChatElement() : height(0), peerNumber(-1), showTranslation(false), translationInProgress(false) {}
};

struct LinkSpan {
    int start;
    int end;
    QString url;
};

class LimeScrollBar;

class ChatView : public QWidget {
    Q_OBJECT
public:
    ChatView(QWidget* parent = 0);
    ~ChatView();

    void appendMessage(const ChatElement& msg);
    void restoreMessages(const std::vector<ChatElement>& msgs);
    void clearMessages();
    void scrollToBottom();
    ChatElement& messageAt(int index);
    int messageCount() const;
    void triggerRelayout();

protected:
    void paintEvent(QPaintEvent* event);
    void resizeEvent(QResizeEvent* event);
    void wheelEvent(QWheelEvent* event);
    void keyPressEvent(QKeyEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseDoubleClickEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void contextMenuEvent(QContextMenuEvent* event);

signals:
    void translateClicked(int msgIndex);
    void sourceClicked(int msgIndex);

private slots:
    void onScrollChanged(int value);

private:
    void relayout();
    int contentWidth() const;
    void drawMessage(QPainter& p, ChatElement& msg, int y, int viewWidth);
    int calcMessageHeight(const ChatElement& msg, int viewWidth);
    int charWidth(uint32_t cp);

    // Selection and link helpers
    int findMessageAtY(int y) const;
    int charPosAt(int msgIndex, int localX, int localY);
    void selectWordAt(int msgIndex, int charPos);
    void selectLineAt(int msgIndex, int charPos);
    std::vector<QRect> selectionRects(int msgIndex);
    QString selectedText() const;
    std::vector<LinkSpan> extractLinks(const QString& text);
    void copySelectedText();
    void copyFullMessage(int msgIndex);

    // Click tracking for double/triple click
    int m_clickCount;
    int m_clickMsgIndex;
    QTime m_clickTime;

    // Selection state
    int m_selMsgIndex;
    int m_selStart;
    int m_selEnd;
    bool m_selecting;

    std::vector<ChatElement> m_messages;
    int m_totalHeight;
    int m_scrollPos;
    LimeScrollBar* m_vScrollBar;
    FloatingPill m_scrollDownPill;

    QFontMetrics m_fm;
    int m_emojiW;
    uint8_t m_ascW[128];
    uint8_t* m_bmpW;

    static const int kAvatarSize = 48;
    static const int kPad = 8;
    static const int kMsgSpacing = 8;
    static const int kBubbleHPad = 12;
    static const int kBubbleVPad = 8;
    static const int kBubbleRadius = 8;
};

#endif
