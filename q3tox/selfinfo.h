#ifndef SELFINFO_H
#define SELFINFO_H

#include <qwidget.h>
#include <qlabel.h>
#include <qpushbt.h>
#include <qvbox.h>
#include <qhbox.h>
#include <qstring.h>

class SelfInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit SelfInfoWidget(QWidget* parent = 0);
    
    void updateInfo(const QString& name, const QString& statusMsg,
                    const QString& connStatus, const QString& address);
    
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
    
    QString selfAddress;
};

#endif // SELFINFO_H
