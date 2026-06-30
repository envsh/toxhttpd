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

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    ChatWidget(QWidget* parent = 0);
    
    void setHeaderText(const QString& text);
    void appendMessage(const QString& message, const QString& type, 
                    const QString& senderName = QString(),
                    const QString& senderNickname = QString(),
                    int peerNumber = -1, const QString& time = "",
                    const QString& avatarUrl = "",
                    const QString& ipAddress = "");
    void clearMessages();
    int messageCount() const;
    ChatElement messageAt(int index) const;
    ChatElement& mutableMessageAt(int index);
    void appendMessage(const ChatElement& msg);
    void restoreMessages(const std::vector<ChatElement>& msgs);
    void triggerRelayout(int msgIndex = -1);
    void triggerVisibleDownloads();
    void repaintMessages() { messageArea->update(); }
    void retranslateUi();
    void showUnreadBanner(int count);
    
signals:
    void messageSent(const QString& message);
    void languageChanged(const QString& langCode);
    void fileSendRequested(const QString& filePath);
    void translateRequested(int msgIndex, const QString& text, const QString& targetLang);
    void translateForSendRequested(const QString& text, const QString& targetLang);
    void sourceClicked(int msgIndex);
    void retryClicked(int msgIndex, const QString& mediaUrl);
    void openFullSizeImage(int msgIndex, const QString& mediaUrl);

private slots:
    void onSendClicked();
    void onLanguageChanged(int index);
    void onThemeToggled(bool checked);
    void onStyleChanged(int index);
    void onEmojiClicked();
    void onEmojiInsert(const QString& emoji);
    void onFileClicked();
    void onFilePaste(const QString& filePath);
    void onSendEnClicked();
    void onTranslateClicked(int msgIndex);
    void onMentionClicked(const QString& username);
    void hideUnreadBanner();

public slots:
    void onTranslateResult(int msgIndex, bool success, const QString& translatedText, const QString& errorMessage);

    LoadingBar* loadingBar() { return m_loadingBar; }

private:
    LoadingBar* m_loadingBar;
    QLabel* headerText;
    QLabel* m_unreadBanner;
    QString m_baseHeader;
    void updateHeaderCount();
    QComboBox* langSelector;
    QComboBox* m_styleSelector;
    QCheckBox* themeCheckBox;
    ChatView* messageArea;
    MessageInput* inputEdit;
    EmojiPushButton* emojiBtn;
    EmojiPushButton* fileBtn;
    QPushButton* sendBtn;
    QPushButton* m_sendEnBtn;
    EmojiPicker* emojiPicker;
    std::string m_targetLang;
};

#endif // CHATWIDGET_H
