#include "invitedialog.h"
#include "translator.h"
#include <qlayout.h>
#include <qapplication.h>

InviteDialog::InviteDialog(const QString& friendNumber, const QString& cookie, QWidget* parent) 
    : QDialog(parent), friendNumber(friendNumber), cookie(cookie), result(Ignore) {
    
    setCaption(_("conference.invitation_received"));
    resize(400, 200);
    
    QBoxLayout* mainLayout = new QBoxLayout(this, QBoxLayout::TopToBottom, 10, 10, 0);
    
    // 标题
    QLabel* titleLabel = new QLabel(_("conference.invitation_received"), this);
    QFont titleFont("Helvetica", 16, QFont::Bold);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);
    
    // 邀请信息
    QString infoText = _("conference.invitation_from").arg(friendNumber) + " " + _("conference.invite_message");
    QLabel* infoLabel = new QLabel(infoText, this);
    mainLayout->addWidget(infoLabel);
    
    mainLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));
    
    // 按钮行：同意（左）、拒绝（中）、忽略（右）
    QBoxLayout* btnLayout = new QBoxLayout(QBoxLayout::LeftToRight, -1, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    
    QPushButton* acceptBtn = new QPushButton(_("conference.accept"), this, "acceptBtn");
    acceptBtn->setFixedSize(80, 35);
    connect(acceptBtn, SIGNAL(clicked()), this, SLOT(onAccept()));
    btnLayout->addWidget(acceptBtn);
    
    QPushButton* rejectBtn = new QPushButton(_("conference.reject"), this, "rejectBtn");
    rejectBtn->setFixedSize(80, 35);
    connect(rejectBtn, SIGNAL(clicked()), this, SLOT(onReject()));
    btnLayout->addWidget(rejectBtn);
    
    QPushButton* ignoreBtn = new QPushButton(_("conference.ignore"), this, "ignoreBtn");
    ignoreBtn->setFixedSize(80, 35);
    connect(ignoreBtn, SIGNAL(clicked()), this, SLOT(onIgnore()));
    btnLayout->addWidget(ignoreBtn);
    
    mainLayout->addLayout(btnLayout);
}

void InviteDialog::onAccept() {
    result = Accept;
    accept();
}

void InviteDialog::onReject() {
    result = Reject;
    accept();
}

void InviteDialog::onIgnore() {
    result = Ignore;
    accept();
}
