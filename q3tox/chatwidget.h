#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include "compat34.h"

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    ChatWidget(QWidget* parent = 0);
    
    void setHeaderText(const QString& text);
    void appendMessage(const QString& message, const QString& type, const QString& sender = QString::null);
    void clearMessages();
    void retranslateUi();
    
signals:
    void messageSent(const QString& message);
    void languageChanged(const QString& langCode);

private slots:
    void onSendClicked();
    void onLanguageChanged(int index);
    void onThemeToggled(bool checked);

private:
    QLabel* headerText;
    QComboBox* langSelector;
    QCheckBox* themeCheckBox;
    QTextEdit* messageArea;
    QLineEdit* inputEdit;
    QPushButton* sendBtn;
};

#endif // CHATWIDGET_H
