#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include "compat34.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include "chatview.h"
#include "messageinput.h"
#include "emojiwidgets.h"
#include "emoji_picker.h"
#include "StyleParams.h"
#include "loadingbar.h"
#include <string>

class StickerPicker;

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    ChatWidget(QWidget* parent = 0);
    static bool s_autoTranslateArg;
    
    void setHeaderText(const QString& text);

    int messageCount() const;
    ChatElement messageAt(int index) const;
    ChatElement& mutableMessageAt(int index);
    void updateElement(int msgIndex) { messageArea->updateElement(msgIndex); }
    void relayout() { messageArea->relayout(); }
    void repaintMessages() { messageArea->update(); }
    void setBuffer(ChatHistory* hist);
    void retranslateUi();
    void showUnreadBanner(int count);
    void setAutoTranslateEnabled(bool enabled) { m_autoTranslateEnabled = enabled; }
    
signals:
    void messageSent(const QString& message);
    void languageChanged(const QString& langCode);
    void fileSendRequested(const QString& filePath);
    void translateRequested(int msgIndex, const QString& text, const QString& targetLang);
    void translateForSendRequested(const QString& text, const QString& targetLang);
    void sourceClicked(int msgIndex);
    void retryClicked(int msgIndex, const QString& mediaUrl, const QString& source);
    void openFullSizeImage(int msgIndex, const QString& mediaUrl);
    void resendMessage(int msgIndex);
    void requestRedactMessage(int msgIndex);

private slots:
    void onSendClicked();
    void onUilangChanged(int index);
    void onTranslateTolangChanged(int index);
    void onThemeToggled(bool checked);
    void onStyleChanged(int index);
    void onEmojiClicked();
    void onEmojiInsert(const QString& emoji);
    void onFileClicked();
    void onFilePaste(const QString& filePath);
    void onStickerClicked();
    void onQuickReplyClicked();
    void onSendEnClicked();
    void onTranslateClicked(int msgIndex);
    void onAutoTranslateRequested(int msgIndex, const QString& text, const QString& toLang);
    void onMentionClicked(const QString& username);
    void onReplyRequested(int msgIndex);
    void onEditRequested(int msgIndex);
    void onDeleteRequested(int msgIndex);
    void onRedactRequested(int msgIndex);
    void hideUnreadBanner();

public slots:
    void onTranslateResult(int msgIndex, bool success, const QString& translatedText, const QString& errorMessage);

    LoadingBar* loadingBar() { return m_loadingBar; }

private:
    LoadingBar* m_loadingBar;
    QLabel* headerText;
    QLabel* m_unreadBanner;
    QString m_baseHeader;
    void scrollBottomIfNeeded();
    void updateHeaderCount();
    // 若消息已缓存翻译，直接应用并返回 true（不发网络请求）
    bool applyCachedTranslation(int msgIndex, const QString& toLang);
    QComboBox* langSelector;
    QComboBox* m_styleSelector;
    QCheckBox* themeCheckBox;
    ChatView* messageArea;
    MessageInput* inputEdit;
    EmojiPushButton* emojiBtn;
    EmojiPushButton* fileBtn;
    EmojiPushButton* stickerBtn;
    EmojiPushButton* quickReplyBtn;
    QPushButton* sendBtn;
    QPushButton* m_sendEnBtn;
    EmojiPicker* emojiPicker;
    StickerPicker* m_stickerPicker = nullptr;
    bool m_autoTranslateEnabled = false;
};

#endif // CHATWIDGET_H
