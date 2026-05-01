#include "invitedialog.h"
#include "translator.h"

InviteDialog::InviteDialog(const QString& info, QWidget* parent) 
    : QDialog(parent), result(Ignore) {
    
    setWindowTitle(_("conference.invite_message"));
    setFixedSize(400, 200);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Info label
    QLabel* infoLabel = new QLabel(info);
    infoLabel->setWordWrap(true);
    infoLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(infoLabel, 1);
    
    // Buttons (order: Accept(left), Reject(middle), Ignore(right))
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    acceptBtn = new QPushButton(_("conference.accept"));
    rejectBtn = new QPushButton(_("conference.reject"));
    ignoreBtn = new QPushButton(_("conference.ignore"));
    
    btnLayout->addWidget(acceptBtn);
    btnLayout->addWidget(rejectBtn);
    btnLayout->addWidget(ignoreBtn);
    
    mainLayout->addLayout(btnLayout);
    
    connect(acceptBtn, SIGNAL(clicked()), this, SLOT(onAcceptClicked()));
    connect(rejectBtn, SIGNAL(clicked()), this, SLOT(onRejectClicked()));
    connect(ignoreBtn, SIGNAL(clicked()), this, SLOT(onIgnoreClicked()));
}

void InviteDialog::onAcceptClicked() {
    result = Accept;
    accept();
}

void InviteDialog::onRejectClicked() {
    result = Reject;
    accept();
}

void InviteDialog::onIgnoreClicked() {
    result = Ignore;
    reject();
}
