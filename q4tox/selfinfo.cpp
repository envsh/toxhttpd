#include "selfinfo.h"
#include "translator.h"
#include "editinfodialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>

SelfInfoWidget::SelfInfoWidget(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Avatar and name row
    QHBoxLayout* topLayout = new QHBoxLayout();
    avatarLabel = new QLabel("?");
    avatarLabel->setFixedSize(40, 40);
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setStyleSheet("border-radius: 20px; background-color: #555; color: white; font-size: 18px;");
    
    QVBoxLayout* nameLayout = new QVBoxLayout();
    nameLabel = new QLabel(_("not_set"));
    nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    
    QHBoxLayout* statusLayout = new QHBoxLayout();
    statusBadge = new QLabel(_("loading"));
    statusBadge->setStyleSheet("padding: 2px 8px; border-radius: 10px; font-size: 12px;");
    
    statusMessageLabel = new QLabel(_("no_status"));
    statusMessageLabel->setStyleSheet("color: #888; font-size: 12px;");
    
    nameLayout->addWidget(nameLabel);
    statusLayout->addWidget(statusBadge);
    statusLayout->addWidget(statusMessageLabel);
    statusLayout->addStretch();
    nameLayout->addLayout(statusLayout);
    
    topLayout->addWidget(avatarLabel);
    topLayout->addLayout(nameLayout, 1);
    topLayout->addStretch();
    
    // Address row
    QHBoxLayout* addrLayout = new QHBoxLayout();
    addressLabel = new QLabel("...");
    addressLabel->setStyleSheet("font-family: monospace; font-size: 11px;");
    addressLabel->setToolTip(_("click_to_copy"));
    copyBtn = new QPushButton(_("copy"));
    copyBtn->setFixedSize(50, 25);
    
    addrLayout->addWidget(addressLabel, 1);
    addrLayout->addWidget(copyBtn);
    
    // Action buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    editBtn = new QPushButton(_("edit_info"));
    bootstrapBtn = new QPushButton(_("connect_network"));
    btnLayout->addWidget(editBtn);
    btnLayout->addWidget(bootstrapBtn);
    
    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(addrLayout);
    mainLayout->addLayout(btnLayout);
    
    // Connect signals
    connect(editBtn, SIGNAL(clicked()), this, SLOT(onEditClicked()));
    connect(bootstrapBtn, SIGNAL(clicked()), this, SLOT(onBootstrapClicked()));
    connect(copyBtn, SIGNAL(clicked()), this, SLOT(onCopyAddress()));
}

void SelfInfoWidget::updateInfo(const QVariantMap& data) {
    currentName = data["name"].toString();
    currentStatusMessage = data["status_message"].toString();
    fullAddress = data["address"].toString();
    
    // Update avatar (show first letter of name)
    QString firstLetter = currentName.isEmpty() ? "?" : QString(currentName[0].toUpper());
    avatarLabel->setText(firstLetter);
    
    // Update name
    nameLabel->setText(currentName.isEmpty() ? _("not_set") : currentName);
    
    // Update status message
    statusMessageLabel->setText(currentStatusMessage.isEmpty() ? _("no_status") : currentStatusMessage);
    
    // Update status badge
    QString connStatus = data["connection_status"].toString();
    if (connStatus == "online" || connStatus == "tcp") {
        statusBadge->setText(_("online"));
        statusBadge->setStyleSheet("background-color: #4caf50; color: white; padding: 2px 8px; border-radius: 10px;");
    } else {
        statusBadge->setText(_("offline"));
        statusBadge->setStyleSheet("background-color: #f44336; color: white; padding: 2px 8px; border-radius: 10px;");
    }
    
    // Update address (show first 8 and last 8 chars)
    if (!fullAddress.isEmpty()) {
        QString shortAddr = fullAddress.left(8) + "..." + fullAddress.right(8);
        addressLabel->setText(shortAddr);
        addressLabel->setToolTip(fullAddress);
    }
}

void SelfInfoWidget::onEditClicked() {
    EditInfoDialog dlg(this);
    dlg.setName(currentName);
    dlg.setStatusMessage(currentStatusMessage);
    
    if (dlg.exec() == QDialog::Accepted) {
        emit editInfoRequested(dlg.getName(), dlg.getStatusMessage());
    }
}

void SelfInfoWidget::onBootstrapClicked() {
    emit bootstrapRequested();
}

void SelfInfoWidget::onCopyAddress() {
    if (!fullAddress.isEmpty()) {
        QApplication::clipboard()->setText(fullAddress);
        QMessageBox::information(this, _("copy"), _("address_copied"));
    }
}
