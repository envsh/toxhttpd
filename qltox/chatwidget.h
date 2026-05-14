#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include "compat34.h"
#include "chatview.h"
#include "messageinput.h"
#include "emojiwidgets.h"
#include "emoji_picker.h"
#include "StyleParams.h"
#include <string>

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    ChatWidget(QWidget* parent = 0);
    
    void setHeaderText(const QString& text);
    void appendMessage(const QString& message, const QString& type, 
                    const QString& senderName = QString(), int peerNumber = -1,
                    const QString& time = "", const QString& avatarText = "",
                    const QString& avatarUrl = "");
    void clearMessages();
    void retranslateUi();
    
signals:
    void messageSent(const QString& message);
    void languageChanged(const QString& langCode);
    void fileSendRequested(const QString& filePath);
    void translateRequested(int msgIndex, const QString& text, const QString& targetLang);

private slots:
    void onSendClicked();
    void onLanguageChanged(int index);
    void onThemeToggled(bool checked);
    void onStyleChanged(int index);
    void onEmojiClicked();
    void onEmojiInsert(const QString& emoji);
    void onFileClicked();
    void onFilePaste(const QString& filePath);
    void onTranslateClicked(int msgIndex);

public slots:
    void onTranslateResult(int msgIndex, bool success, const QString& translatedText, const QString& errorMessage);

private:
    QLabel* headerText;
    QComboBox* langSelector;
    QComboBox* m_styleSelector;
    QCheckBox* themeCheckBox;
    ChatView* messageArea;
    MessageInput* inputEdit;
    EmojiPushButton* emojiBtn;
    EmojiPushButton* fileBtn;
    QPushButton* sendBtn;
    EmojiPicker* emojiPicker;
    std::string m_targetLang;
};

#endif // CHATWIDGET_H
