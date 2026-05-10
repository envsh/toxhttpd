#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include "compat34.h"
#include "chatview.h"
#include "messageinput.h"

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    ChatWidget(QWidget* parent = 0);
    
    void setHeaderText(const QString& text);
    void appendMessage(const QString& message, const QString& type, 
                    const QString& sender = QString::null, const QString& time = "",
                    const QString& avatarText = "");
    void clearMessages();
    void retranslateUi();
    
signals:
    void messageSent(const QString& message);
    void languageChanged(const QString& langCode);
    void fileSendRequested(const QString& filePath);

private slots:
    void onSendClicked();
    void onLanguageChanged(int index);
    void onThemeToggled(bool checked);
    void onEmojiClicked();
    void onFileClicked();
    void onFilePaste(const QString& filePath);

private:
    QLabel* headerText;
    QComboBox* langSelector;
    QCheckBox* themeCheckBox;
    ChatView* messageArea;
    MessageInput* inputEdit;
    QPushButton* emojiBtn;
    QPushButton* fileBtn;
    QPushButton* sendBtn;
};

#endif // CHATWIDGET_H
