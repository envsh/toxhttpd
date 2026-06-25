#ifndef CHATVIEW_H
#define CHATVIEW_H

#include "compat34.h"
#include "floatingpill.h"
#include "StyleParams.h"
#include <string>
#include <vector>
#include <cstdint>
#include <qdatetime.h>
#include <qrect.h>
#include <qwidget.h>
#include <qpainter.h>

#if defined(QT3_BUILD)
#include <qmovie.h>
#else
#include <QMovie>
#endif

struct ChatElement {
    enum ElementType { Text, Image, File, Video, Gif, Audio };

    ElementType etype;

    // Common
    QString senderName;
    QString senderNickname;
    int     peerNumber;
    QString avatarUrl;
    QString time;
    QString ipAddress;

    // Text only
    QString messageText;
    QString category;          // "self" / "other" / "friend"

    // Translation (Text only)
    QString translatedText;
    QString translateError;
    bool showTranslation;
    bool translationInProgress;
    QRect translateBtnRect;
    QRect sourceBtnRect;

    // Shared media fields (Image / Gif / Video)
    QPixmap thumbnail;
    QPixmap scaledDisplay;   // 预缩放到显示尺寸的缓存
    int     scaledForDispW;  // 缓存对应的绘制宽度
    int     scaledForDispH;  // 缓存对应的绘制高度
    QString caption;
    QString mediaUrl;
    bool downloadFailed;
    bool downloadRequested;
    QRect thumbnailRect;
    int mediaWidth;
    QRect downloadBtnRect;
    int mediaHeight;

    // File only
    QString fileName;
    int fileSize;
    int progress;
    QString localPath;

    // Video only
    int durationSec;

    // Gif only
    QString gifPath;
    QMovie* movie;

    // Layout cache (TG-style per-element)
    short cachedWidth;
    short height;

    ChatElement()
        : etype(Text), peerNumber(-1), showTranslation(false)
        , translationInProgress(false), downloadFailed(false), downloadRequested(false)
        , mediaWidth(0), mediaHeight(0)
        , fileSize(0), progress(0), durationSec(0), movie(nullptr)
        , scaledForDispW(-1), scaledForDispH(-1), cachedWidth(-1), height(0) {}

    int calcHeight(int viewWidth, const QFontMetrics& fm, int emojiW, const QFont& baseFont);
    void paint(QPainter& p, int y, int viewWidth, bool isSelected,
               const std::vector<QRect>& selRects,
               const QFontMetrics& fm, int emojiW,
               const QFont& baseFont, const StyleParams::Palette& pal);
    void startAnimation(QWidget* parent);
    void stopAnimation();
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
    void triggerRelayout(int msgIndex = -1);
    void triggerVisibleDownloads();

    static const int kAvatarSize   = 42;
    static const int kPad          = 8;
    static const int kMsgSpacing   = 8;
    static const int kBubbleHPad   = 12;
    static const int kBubbleVPad   = 8;
    static const int kBubbleRadius = 8;

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
    void mentionClicked(const QString& username);
    void retryClicked(int msgIndex, const QString& mediaUrl);
    void downloadNeeded(int msgIndex, const QString& mediaUrl);

private slots:
    void onScrollChanged(int value);

private:
    void relayout();
    int contentWidth() const;
    int charWidth(uint32_t cp);
    void manageAnimations();
    std::pair<int,int> visibleMessageRange() const;

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

    std::vector<ChatElement> m_items;
    int m_totalHeight;
    int m_scrollPos;
    LimeScrollBar* m_vScrollBar;
    FloatingPill m_scrollDownPill;

    QFontMetrics m_fm;
    int m_emojiW;
    uint8_t m_ascW[128];
    uint8_t* m_bmpW;

};

#endif
