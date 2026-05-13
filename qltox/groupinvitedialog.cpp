#include "groupinvitedialog.h"
#include "translator.h"
#include "compat34.h"

GroupInviteDialog::GroupInviteDialog(const QString& friendNumber, const QString& chatId, QWidget* parent) 
    : QDialog(parent), friendNumber(friendNumber), chatId(chatId), password(""), result(Reject) {
    
    qSetWindowTitle(this, _("group.invitation_received"));
    resize(400, 250);
    
    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 10, 10);
    
    // 标题
    QLabel* titleLabel = new QLabel(_("group.invitation_received"), this);
    QFont titleFont("Helvetica", 16, QFont::Bold);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);
    
    // 邀请信息
    QString infoText = _("group.invitation_from").arg(friendNumber) + "\n" + 
                     _("group.invite_chat_id") + ": " + chatId;
    QLabel* infoLabel = new QLabel(infoText, this);
    mainLayout->addWidget(infoLabel);
    
    // 密码输入（可选）
    QLabel* passwordLabel = new QLabel(_("group.password_optional"), this);
    mainLayout->addWidget(passwordLabel);
    
    passwordEdit = new QLineEdit(this);
    passwordEdit->setEchoMode(QLineEdit::Password);
#ifdef QT3_BUILD
    passwordEdit->setText(_("placeholders.group_password"));
#else
    passwordEdit->setPlaceholderText(_("placeholders.group_password"));
#endif
    mainLayout->addWidget(passwordEdit);
    
    mainLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));
    
    // 按钮行：接受（左）、拒绝（右）
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    
    QPushButton* acceptBtn = new QPushButton(_("group.accept"), this);
    acceptBtn->setFixedSize(80, 35);
    connect(acceptBtn, SIGNAL(clicked()), this, SLOT(onAccept()));
    btnLayout->addWidget(acceptBtn);
    
    QPushButton* rejectBtn = new QPushButton(_("group.reject"), this);
    rejectBtn->setFixedSize(80, 35);
    connect(rejectBtn, SIGNAL(clicked()), this, SLOT(onReject()));
    btnLayout->addWidget(rejectBtn);
    
    mainLayout->addLayout(btnLayout);
}

void GroupInviteDialog::onAccept() {
    password = passwordEdit->text();
    result = Accept;
    accept();
}

void GroupInviteDialog::onReject() {
    result = Reject;
    accept();
}
