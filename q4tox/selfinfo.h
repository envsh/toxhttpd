#ifndef SELFINFO_H
#define SELFINFO_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVariantMap>

class SelfInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit SelfInfoWidget(QWidget* parent = 0);
    void updateInfo(const QString& name, const QString& statusMsg, 
                   const QString& connStatus, const QString& address);
    void retranslateUi();
    
signals:
    void editInfoRequested(const QString& name, const QString& statusMessage);
    void bootstrapRequested();
    
private slots:
    void onEditClicked();
    void onBootstrapClicked();
    void onCopyAddress();
    
private:
    QLabel* avatarLabel;
    QLabel* nameLabel;
    QLabel* statusBadge;
    QLabel* statusMessageLabel;
    QLabel* addressLabel;
    QPushButton* copyBtn;
    QPushButton* editBtn;
    QPushButton* bootstrapBtn;
    
    QString fullAddress;
    QString currentName;
    QString currentStatusMessage;
    QString currentConnStatus;
};

#endif // SELFINFO_H
