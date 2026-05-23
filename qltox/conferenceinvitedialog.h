#ifndef CONFERENCEINVITEDIALOG_H
#define CONFERENCEINVITEDIALOG_H

#include "compat34.h"
#include <qstring.h>
#include <qdialog.h>

class ConferenceInviteDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConferenceInviteDialog(const QString& friendNumber, const QString& cookie, QWidget* parent = nullptr);
    
    enum Result {
        Accept,
        Reject,
        Ignore
    };
    
    Result getResult() const { return result; }
    QString getFriendNumber() const { return friendNumber; }
    QString getCookie() const { return cookie; }
    
private slots:
    void onAccept();
    void onReject();
    void onIgnore();
    
private:
    QString friendNumber;
    QString cookie;
    Result result;
};

#endif // CONFERENCEINVITEDIALOG_H
