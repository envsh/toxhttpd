#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget* parent = 0);
    void setChatInfo(int id, const QString& type); // "friend", "conference", "group"
    void appendMessage(const QString& message, bool isSelf);
    
signals:
    void messageSent(const QString& message);
    void languageChanged(const QString& langCode);
    
private slots:
    void onSendClicked();
    void onLanguageChanged(int index);
    
private:
    QTextEdit* messageArea;
    QLineEdit* inputEdit;
    QPushButton* sendBtn;
    QComboBox* langSelector;
    
    int chatId;
    QString chatType;
    QString chatName;
};

#endif // CHATWIDGET_H
