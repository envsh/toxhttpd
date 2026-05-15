#include "friendinfodialog.h"
#include "translator.h"
#include "compat34.h"

FriendInfoDialog::FriendInfoDialog(QWidget* parent) : QDialog(parent) {
    qSetWindowTitle(this, _("modals.friend_info_title"));
    resize(400, 250);
    
    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 10, 10);
    
    // 标题
    titleLabel = new QLabel(this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);
    
    // ID
    QBoxLayout* idLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    QLabel* idTitle = new QLabel(_("modals.labels.friend_id"), this);
    idTitle->setFixedWidth(80);
    idLayout->addWidget(idTitle);
    idLabel = new QLabel(this);
    idLayout->addWidget(idLabel, 1);
    mainLayout->addLayout(idLayout);
    
    // 名称
    QBoxLayout* nameLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    QLabel* nameTitle = new QLabel(_("modals.labels.name"), this);
    nameTitle->setFixedWidth(80);
    nameLayout->addWidget(nameTitle);
    nameLabel = new QLabel(this);
    nameLayout->addWidget(nameLabel, 1);
    mainLayout->addLayout(nameLayout);
    
    // 类型
    QBoxLayout* typeLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    QLabel* typeTitle = new QLabel(_("modals.labels.type"), this);
    typeTitle->setFixedWidth(80);
    typeLayout->addWidget(typeTitle);
    typeLabel = new QLabel(this);
    typeLayout->addWidget(typeLabel, 1);
    mainLayout->addLayout(typeLayout);
    
    // 状态
    QBoxLayout* statusLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    QLabel* statusTitle = new QLabel(_("modals.labels.status"), this);
    statusTitle->setFixedWidth(80);
    statusLayout->addWidget(statusTitle);
    statusLabel = new QLabel(this);
    statusLayout->addWidget(statusLabel, 1);
    mainLayout->addLayout(statusLayout);
    
    // 连接状态
    QBoxLayout* connLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    QLabel* connTitle = new QLabel(_("modals.labels.connection"), this);
    connTitle->setFixedWidth(80);
    connLayout->addWidget(connTitle);
    connLabel = new QLabel(this);
    connLayout->addWidget(connLabel, 1);
    mainLayout->addLayout(connLayout);
    
    // 连接状态（在线/离线）
    QBoxLayout* connectedLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    QLabel* connectedTitle = new QLabel(_("modals.labels.type"), this);  // 复用 type 标签显示"连接状态"
    connectedTitle->setFixedWidth(80);
    connectedLayout->addWidget(connectedTitle);
    connectedLabel = new QLabel(this);
    connectedLayout->addWidget(connectedLabel, 1);
    mainLayout->addLayout(connectedLayout);
    
    // IP
    QBoxLayout* ipLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    QLabel* ipTitle = new QLabel(_("modals.labels.ip"), this);
    ipTitle->setFixedWidth(80);
    ipLayout->addWidget(ipTitle);
    ipLabel = new QLabel(this);
    ipLayout->addWidget(ipLabel, 1);
    mainLayout->addLayout(ipLayout);

    // 公钥
    QBoxLayout* pkLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    QLabel* pkTitle = new QLabel(_("modals.labels.public_key"), this);
    pkTitle->setFixedWidth(80);
    pkLayout->addWidget(pkTitle);
    pkLabel = new QLabel(this);
#ifdef QT3_BUILD
    // Qt3: use break lines manually or use QTextEdit
    pkLabel->setTextFormat(Qt::PlainText);
#else
    pkLabel->setWordWrap(true);
#endif
    pkLayout->addWidget(pkLabel, 1);
    mainLayout->addLayout(pkLayout);
    
    // 关闭按钮
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    QPushButton* closeBtn = new QPushButton(_("buttons.close"), this);
    closeBtn->setFixedSize(80, 30);
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(onClose()));
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);
}

void FriendInfoDialog::setInfo(int id, const QString& name, const QString& type,
                                 const QString& status, const QString& connection,
                                 bool isConnected, const QString& publicKey) {
    idLabel->setText(QString::number(id));
    nameLabel->setText(name.isEmpty() ? _("no_name") : name);
    typeLabel->setText(type == "friend" ? _("friend") : 
                      type == "conference" ? _("conference_item") : _("group"));
    statusLabel->setText(status.isEmpty() ? _("no_status") : status);
    connLabel->setText(connection.isEmpty() ? _("statuses.offline") : connection);
    
    // 设置连接状态（在线/离线）
    if (type == "friend") {
        connectedLabel->setText(connection.isEmpty() ? _("statuses.offline") : connection);
    } else {
        connectedLabel->setText(isConnected ? _("statuses.online") : _("statuses.offline"));
    }
    
    pkLabel->setText(publicKey.isEmpty() ? _("no_status") : publicKey);
}

void FriendInfoDialog::setInfo(const FriendInfo& info) {
    setInfo(info.id, qFromUtf8(info.name.c_str()), "friend",
            qFromUtf8(info.statusText.c_str()),
            qFromUtf8(info.statusStr.c_str()),
            false,
            qFromUtf8(info.publicKey.c_str()));
    ipLabel->setText(info.peerIp.empty() ? "-" : qFromUtf8(info.peerIp.c_str()));
}

void FriendInfoDialog::setTitle(const QString& title) {
    titleLabel->setText(title);
    qSetWindowTitle(this, title);
}

void FriendInfoDialog::onClose() {
    accept();
}
