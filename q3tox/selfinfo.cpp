#include "selfinfo.h"
#include "translator.h"
#include "editinfodialog.h"
#include "api.h"
#include "compat34.h"

SelfInfoWidget::SelfInfoWidget(QWidget* parent) : QWidget(parent), selfAddress("") {
    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 12, 10);
    qSetMargins(mainLayout, 12, 12, 12, 12);
    
    // 头部区域：头像 + 信息
    QBoxLayout* headerLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    
    // 头像
    avatarLabel = new QLabel("?", this);
    avatarLabel->setFixedSize(40, 40);
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setPalette(QPalette(QColor("#21262d")));
    avatarLabel->setFrameStyle(QFrame::Box | QFrame::Raised);
    avatarLabel->setLineWidth(2);
    headerLayout->addWidget(avatarLabel);
    
    // 信息内容
    QBoxLayout* infoLayout = qNewBoxLayout(nullptr, QBoxLayout::TopToBottom, 0, 2);
    
    // 名称行：名称 + 状态标识
    QBoxLayout* nameRowLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    nameLabel = new QLabel(_("no_name"), this);
    nameLabel->setFont(QFont("Helvetica", 16, QFont::Bold));
    nameRowLayout->addWidget(nameLabel, 1);
    
    statusBadge = new QLabel(_("statuses.offline"), this);
    statusBadge->setAlignment(Qt::AlignCenter);
    statusBadge->setFixedSize(60, 20);
    statusBadge->setPalette(QPalette(QColor("#482121")));
    statusBadge->setFrameStyle(QFrame::Box | QFrame::Raised);
    nameRowLayout->addWidget(statusBadge);
    infoLayout->addLayout(nameRowLayout);
    
    // 状态消息
    statusMsgLabel = new QLabel(_("no_status"), this);
    statusMsgLabel->setFont(QFont("Helvetica", 12));
    statusMsgLabel->setPalette(QPalette(QColor("#6e7681")));
    infoLayout->addWidget(statusMsgLabel);
    
    headerLayout->addLayout(infoLayout, 1);
    mainLayout->addLayout(headerLayout);
    
    // 地址行
    QBoxLayout* addrLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    addressLabel = new QLabel("...", this);
    addressLabel->setFont(QFont("Monospace", 11));
    addressLabel->setPalette(QPalette(QColor("#6e7681")));
    addrLayout->addWidget(addressLabel, 1);
    
    copyBtn = new QPushButton(_("buttons.copy"), this);
    copyBtn->setFixedSize(50, 25);
    connect(copyBtn, SIGNAL(clicked()), this, SLOT(onCopyAddress()));
    addrLayout->addWidget(copyBtn);
    mainLayout->addLayout(addrLayout);
    
    // 操作按钮
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    editBtn = new QPushButton(_("buttons.edit_info"), this);
    connect(editBtn, SIGNAL(clicked()), this, SLOT(onEditInfo()));
    btnLayout->addWidget(editBtn);
    
    connectBtn = new QPushButton(_("buttons.connect_network"), this);
    connect(connectBtn, SIGNAL(clicked()), this, SLOT(onBootstrap()));
    btnLayout->addWidget(connectBtn);
    
    qrBtn = new QPushButton(_("buttons.qrcode"), this);
    connect(qrBtn, SIGNAL(clicked()), this, SLOT(onShowQRCode()));
    btnLayout->addWidget(qrBtn);
    
    mainLayout->addLayout(btnLayout);
}

void SelfInfoWidget::updateInfo(const QString& name, const QString& statusMsg,
                               const QString& connStatus, const QString& address) {
    // 更新头像
    QString displayName = name.isEmpty() ? _("no_name") : name;
    QString initial = qToUpper(displayName.left(1));
    avatarLabel->setText(initial);
    
    if (!name.isEmpty()) {
        avatarLabel->setPalette(QPalette(QColor("#1c3a5f")));
    }
    
    // 更新名称
    nameLabel->setText(displayName);
    
    // 更新状态标识
    QString statusText = (connStatus == "offline") ? _("statuses.offline") :
                        (connStatus == "tcp") ? _("statuses.tcp") :
                        _("statuses.udp");
    statusBadge->setText(statusText);
    
    if (connStatus == "offline") {
        statusBadge->setPalette(QPalette(QColor("#482121")));
    } else {
        statusBadge->setPalette(QPalette(QColor("#1a4731")));
    }
    
    // 更新状态消息
    statusMsgLabel->setText(statusMsg.isEmpty() ? _("no_status") : statusMsg);
    
    // 更新地址
    selfAddress = address;
    if (address.length() > 20) {
        QString shortAddr = address.left(8) + "..." + address.right(8);
        addressLabel->setText(shortAddr);
    } else {
        addressLabel->setText(address);
    }
    // 设置工具提示
    qSetToolTip(addressLabel, address);
}

void SelfInfoWidget::onEditInfo() {
    EditInfoDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        ToxAPI api;
        QString name = dialog.getName();
        QString status = dialog.getStatusMessage();
        
        if (!name.isEmpty()) {
            api.setSelfName(std::string(qToUtf8(name).data()));
        }
        if (!status.isEmpty()) {
            api.setSelfStatus(std::string(qToUtf8(status).data()));
        }
        
        // 重新加载信息
        std::string name2, statusMsg, connStatus, address;
        if (api.getSelf(name2, statusMsg, connStatus, address)) {
            updateInfo(QString::fromUtf8(name2.c_str()), 
                      QString::fromUtf8(statusMsg.c_str()), 
                      QString::fromUtf8(connStatus.c_str()),
                      QString::fromUtf8(address.c_str()));
        }
    }
}

void SelfInfoWidget::onBootstrap() {
    ToxAPI api;
    QMessageBox::information(this, "Bootstrap", _("connecting_network"));
    
    // 重新加载信息以更新状态
    std::string name, statusMsg, connStatus, address;
    if (api.getSelf(name, statusMsg, connStatus, address)) {
        updateInfo(QString::fromUtf8(name.c_str()), 
                  QString::fromUtf8(statusMsg.c_str()), 
                  QString::fromUtf8(connStatus.c_str()),
                  QString::fromUtf8(address.c_str()));
    }
}

void SelfInfoWidget::onShowQRCode() {
    if (selfAddress.isEmpty()) {
        QMessageBox::warning(this, "QR Code", _("please_wait"));
        return;
    }
    
    // Qt3 没有QUrl，简单进行URL编码
    QString encodedAddr = selfAddress;
    encodedAddr.replace("%", "%25");
    encodedAddr.replace(" ", "%20");
    encodedAddr.replace("!", "%21");
    encodedAddr.replace("\"", "%22");
    encodedAddr.replace("#", "%23");
    encodedAddr.replace("$", "%24");
    encodedAddr.replace("&", "%26");
    encodedAddr.replace("'", "%27");
    encodedAddr.replace("(", "%28");
    encodedAddr.replace(")", "%29");
    encodedAddr.replace("*", "%2A");
    encodedAddr.replace("+", "%2B");
    encodedAddr.replace(",", "%2C");
    encodedAddr.replace("/", "%2F");
    encodedAddr.replace(":", "%3A");
    encodedAddr.replace(";", "%3B");
    encodedAddr.replace("=", "%3D");
    encodedAddr.replace("?", "%3F");
    encodedAddr.replace("@", "%40");
    encodedAddr.replace("[", "%5B");
    encodedAddr.replace("]", "%5D");
    
    QString url = QString("https://api.qrserver.com/v1/create-qr-code/?size=200x200&data=%1")
                  .arg(encodedAddr);
    
    QMessageBox::information(this, "QR Code", _("tox_id_copied") + "\n\nURL: " + url);
}

void SelfInfoWidget::retranslateUi() {
    // 更新按钮文字
    if (editBtn) editBtn->setText(_("buttons.edit_info"));
    if (connectBtn) connectBtn->setText(_("buttons.connect_network"));
    if (qrBtn) qrBtn->setText(_("buttons.qrcode"));
    if (copyBtn) copyBtn->setText(_("buttons.copy"));
    
    // 更新状态标签（如果当前显示的是默认值）
    QString currentStatus = statusBadge->text();
    if (currentStatus == "离线" || currentStatus == "TCP" || currentStatus == "UDP" ||
        currentStatus == "Offline" || currentStatus == "TCP" || currentStatus == "UDP") {
        // 重新获取状态并更新
        // 这里简化：只更新默认状态文字
    }
}

void SelfInfoWidget::onCopyAddress() {
    if (!selfAddress.isEmpty()) {
        QApplication::clipboard()->setText(selfAddress);
        QMessageBox::information(this, "Copy", _("tox_id_copied"));
    }
}
