#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget* parent = 0);
    void setHeaderText(const QString& text);
    void clearMessages();
    void appendMessage(const QString& message, const QString& sender);
    void retranslateUi();
    
signals:
    void messageSent(const QString& message);
    void languageChanged(const QString& langCode);
    void darkModeToggled(bool dark);
    
private slots:
    void onSendClicked();
    void onLanguageChanged(int index);
    void onDarkModeClicked();
    
private:
    QTextEdit* messageArea;
    QLineEdit* inputEdit;
    QPushButton* sendBtn;
    QComboBox* langSelector;
    QPushButton* darkModeBtn;
    QLabel* headerText;
    
    int chatId;
    QString chatType;
};

#endif // CHATWIDGET_H
