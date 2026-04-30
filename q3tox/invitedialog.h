#ifndef INVITEDIALOG_H
#define INVITEDIALOG_H

#include <qdialog.h>
#include <qvbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qpushbt.h>
#include <qstring.h>

class InviteDialog : public QDialog {
    Q_OBJECT
public:
    explicit InviteDialog(const QString& friendNumber, const QString& cookie, QWidget* parent = nullptr);
    
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

#endif // INVITEDIALOG_H
