#ifndef CHATVIEW_H
#define CHATVIEW_H

#include "compat34.h"
#include <string>
#include <vector>
#include <qdatetime.h>
#include <qrect.h>

struct ChatMessage {
    QString messageText;
    QString type;
    QString senderName;
    int     peerNumber;
    QString avatarText;
    QString avatarUrl;
    QString time;
    int height;

    // Translation support
    QString translatedText;
    QString translateError;
    bool showTranslation;
    bool translationInProgress;
    QRect translateBtnRect;

    ChatMessage() : height(0), peerNumber(-1), showTranslation(false), translationInProgress(false) {}
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

    void appendMessage(const ChatMessage& msg);
    void clearMessages();
    void scrollToBottom();
    ChatMessage& messageAt(int index);
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

private slots:
    void onScrollChanged(int value);

private:
    void relayout();
    int contentWidth() const;
    void drawMessage(QPainter& p, ChatMessage& msg, int y, int viewWidth);
    int calcMessageHeight(const ChatMessage& msg, int viewWidth) const;

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

    std::vector<ChatMessage> m_messages;
    int m_totalHeight;
    int m_scrollPos;
    LimeScrollBar* m_vScrollBar;

    static const int kAvatarSize = 48;
    static const int kPad = 8;
    static const int kMsgSpacing = 8;
    static const int kBubbleHPad = 12;
    static const int kBubbleVPad = 8;
    static const int kBubbleRadius = 8;
};

#endif
