#ifndef SELFINFO_H
#define SELFINFO_H

#include "compat34.h"
#include <qstring.h>

class SelfInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit SelfInfoWidget(QWidget* parent = 0);
    
    void updateInfo(const QString& name, const QString& statusMsg,
                    const QString& connStatus, const QString& address);
    void retranslateUi();
    
private slots:
    void onEditInfo();
    void onBootstrap();
    void onShowQRCode();
    void onCopyAddress();
    
private:
    QLabel* avatarLabel;
    QLabel* nameLabel;
    QLabel* statusBadge;
    QLabel* statusMsgLabel;
    QLabel* addressLabel;
    QPushButton* copyBtn;
    QPushButton* editBtn;
    QPushButton* connectBtn;
    QPushButton* qrBtn;

    QString selfAddress;
};

#endif // SELFINFO_H
