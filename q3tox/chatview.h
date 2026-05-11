#ifndef CHATVIEW_H
#define CHATVIEW_H

#include "compat34.h"
#include <string>
#include <vector>

struct ChatMessage {
    QString messageText;
    QString type;
    QString sender;
    QString avatarText;
    QString time;
    int height;

    ChatMessage() : height(0) {}
};

class LimeScrollBar;

class ChatView : public QWidget {
    Q_OBJECT
public:
    ChatView(QWidget* parent = 0);

    void appendMessage(const ChatMessage& msg);
    void clearMessages();
    void scrollToBottom();

protected:
    void paintEvent(QPaintEvent* event);
    void resizeEvent(QResizeEvent* event);
    void wheelEvent(QWheelEvent* event);
    void keyPressEvent(QKeyEvent* event);

private slots:
    void onScrollChanged(int value);

private:
    void relayout();
    int contentWidth() const;
    void drawMessage(QPainter& p, const ChatMessage& msg, int y, int viewWidth);
    int calcMessageHeight(const ChatMessage& msg, int viewWidth) const;

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
