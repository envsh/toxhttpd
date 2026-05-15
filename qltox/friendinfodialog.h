#ifndef FRIENDINFODIALOG_H
#define FRIENDINFODIALOG_H

#include "compat34.h"
#include "restapi.h"
#include <qstring.h>

class FriendInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit FriendInfoDialog(QWidget* parent = nullptr);
    
    void setInfo(int id, const QString& name, const QString& type,
                 const QString& status = QString(),
                 const QString& connection = QString(),
                 bool isConnected = false,
                 const QString& publicKey = QString());
    void setInfo(const FriendInfo& info);
    void setTitle(const QString& title);

private slots:
    void onClose();

private:
    QLabel* titleLabel;
    QLabel* idLabel;
    QLabel* nameLabel;
    QLabel* typeLabel;
    QLabel* statusLabel;
    QLabel* connLabel;
    QLabel* connectedLabel;
    QLabel* pkLabel;
    QLabel* ipLabel;
};

#endif // FRIENDINFODIALOG_H
