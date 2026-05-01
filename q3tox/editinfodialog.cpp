#include "editinfodialog.h"
#include "translator.h"
#include "compat34.h"

EditInfoDialog::EditInfoDialog(QWidget* parent) : QDialog(parent) {
    qSetWindowTitle(this, _("modals.edit_info_title"));
    resize(400, 200);
    
    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 10, 10);
    
    // 名称输入
    QBoxLayout* nameLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    QLabel* nameLabel = new QLabel(_("modals.labels.name"), this);
    nameLabel->setFixedWidth(80);
    nameLayout->addWidget(nameLabel);
    nameEdit = new QLineEdit(this);
    nameLayout->addWidget(nameEdit, 1);
    mainLayout->addLayout(nameLayout);
    
    // 状态消息输入
    QBoxLayout* statusLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    QLabel* statusLabel = new QLabel(_("modals.labels.status_message"), this);
    statusLabel->setFixedWidth(80);
    statusLayout->addWidget(statusLabel);
    statusEdit = new QLineEdit(this);
    statusLayout->addWidget(statusEdit, 1);
    mainLayout->addLayout(statusLayout);
    
    // 按钮
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    
    QPushButton* saveBtn = new QPushButton(_("buttons.save"), this);
    saveBtn->setFixedSize(80, 30);
    connect(saveBtn, SIGNAL(clicked()), this, SLOT(onSave()));
    btnLayout->addWidget(saveBtn);
    
    QPushButton* cancelBtn = new QPushButton(_("buttons.cancel"), this);
    cancelBtn->setFixedSize(80, 30);
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
    btnLayout->addWidget(cancelBtn);
    
    mainLayout->addLayout(btnLayout);
}

QString EditInfoDialog::getName() const {
    return qTrim(nameEdit->text());
}

QString EditInfoDialog::getStatusMessage() const {
    return qTrim(statusEdit->text());
}

void EditInfoDialog::onSave() {
    accept();
}

void EditInfoDialog::onCancel() {
    reject();
}
