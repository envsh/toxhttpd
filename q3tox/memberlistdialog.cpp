#include "memberlistdialog.h"
#include "api.h"
#include "translator.h"
#include "compat34.h"
#include <qstring.h>

MemberListDialog::MemberListDialog(QWidget* parent) : QDialog(parent), listWidget(nullptr) {
    qSetWindowTitle(this, _("member_list.title"));
    resize(400, 300);
    
    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 10, 10);
    
    // 成员列表
#ifdef QT3_BUILD
    QListBox* lb = new QListBox(this);
    listWidget = lb;
    mainLayout->addWidget(lb);
#else
    QListWidget* lw = new QListWidget(this);
    listWidget = lw;
    mainLayout->addWidget(lw);
#endif
    
    // 关闭按钮
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    QPushButton* closeBtn = new QPushButton(_("buttons.close"), this);
    closeBtn->setFixedSize(80, 30);
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(onClose()));
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);
}

void MemberListDialog::setMembers(const std::vector<PeerInfo>& members) {
#ifdef QT3_BUILD
    QListBox* lb = (QListBox*)listWidget;
    lb->clear();
    for (const auto& peer : members) {
        QString text = QString("Peer %1: %2").arg(peer.peerNumber).arg(QString::fromUtf8(peer.name.c_str()));
        lb->insertItem(text);
    }
#else
    QListWidget* lw = (QListWidget*)listWidget;
    lw->clear();
    for (const auto& peer : members) {
        QString text = QString("Peer %1: %2").arg(peer.peerNumber).arg(QString::fromUtf8(peer.name.c_str()));
        lw->addItem(text);
    }
#endif
}

void MemberListDialog::setDialogTitle(const QString& title) {
    qSetWindowTitle(this, title);
}

void MemberListDialog::onClose() {
    accept();
}