#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <qwidget.h>
#include <qtextedit.h>
#include <qlineedit.h>
#include <qpushbt.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qvbox.h>
#include <qhbox.h>

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget* parent = nullptr);
    
    void setHeaderText(const QString& text);
    void appendMessage(const QString& message, const QString& type, const QString& sender = QString());
    void clearMessages();
    
signals:
    void messageSent(const QString& message);
    void languageChanged(const QString& langCode);

private slots:
    void onSendClicked();
    void onLanguageChanged(int index);

private:
    QLabel* headerText;
    QComboBox* langSelector;
    QTextEdit* messageArea;
    QLineEdit* inputEdit;
    QPushButton* sendBtn;
};

#endif // CHATWIDGET_H
