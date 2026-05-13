#ifndef GROUPINVITEDIALOG_H
#define GROUPINVITEDIALOG_H

#include "compat34.h"
#include <qstring.h>

class GroupInviteDialog : public QDialog {
    Q_OBJECT
public:
    explicit GroupInviteDialog(const QString& friendNumber, const QString& chatId, QWidget* parent = nullptr);
    
    enum Result {
        Accept,
        Reject
    };
    
    Result getResult() const { return result; }
    QString getFriendNumber() const { return friendNumber; }
    QString getChatId() const { return chatId; }
    QString getPassword() const { return password; }
    
private slots:
    void onAccept();
    void onReject();
    
private:
    QString friendNumber;
    QString chatId;
    QString password;
    Result result;
    QLineEdit* passwordEdit;
};

#endif // GROUPINVITEDIALOG_H
