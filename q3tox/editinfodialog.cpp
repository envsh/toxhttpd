#include "editinfodialog.h"
#include "translator.h"
#include <qlayout.h>
#include <qapplication.h>

EditInfoDialog::EditInfoDialog(QWidget* parent) : QDialog(parent) {
    setCaption(tr("modals.edit_info_title"));
    resize(400, 200);
    
    QBoxLayout* mainLayout = new QBoxLayout(this, QBoxLayout::TopToBottom, 10, 10, 0);
    
    // 名称输入
    QBoxLayout* nameLayout = new QBoxLayout(QBoxLayout::LeftToRight, -1, 0);
    QLabel* nameLabel = new QLabel(tr("modals.labels.name"), this);
    nameLabel->setFixedWidth(80);
    nameLayout->addWidget(nameLabel);
    nameEdit = new QLineEdit(this, "nameEdit");
    nameLayout->addWidget(nameEdit, 1);
    mainLayout->addLayout(nameLayout);
    
    // 状态消息输入
    QBoxLayout* statusLayout = new QBoxLayout(QBoxLayout::LeftToRight, -1, 0);
    QLabel* statusLabel = new QLabel(tr("modals.labels.status_message"), this);
    statusLabel->setFixedWidth(80);
    statusLayout->addWidget(statusLabel);
    statusEdit = new QLineEdit(this, "statusEdit");
    statusLayout->addWidget(statusEdit, 1);
    mainLayout->addLayout(statusLayout);
    
    // 按钮
    QBoxLayout* btnLayout = new QBoxLayout(QBoxLayout::LeftToRight, -1, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    
    QPushButton* saveBtn = new QPushButton(tr("buttons.save"), this, "saveBtn");
    saveBtn->setFixedSize(80, 30);
    connect(saveBtn, SIGNAL(clicked()), this, SLOT(onSave()));
    btnLayout->addWidget(saveBtn);
    
    QPushButton* cancelBtn = new QPushButton(tr("buttons.cancel"), this, "cancelBtn");
    cancelBtn->setFixedSize(80, 30);
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
    btnLayout->addWidget(cancelBtn);
    
    mainLayout->addLayout(btnLayout);
}

QString EditInfoDialog::getName() const {
    return nameEdit->text().stripWhiteSpace();
}

QString EditInfoDialog::getStatusMessage() const {
    return statusEdit->text().stripWhiteSpace();
}

void EditInfoDialog::onSave() {
    accept();
}

void EditInfoDialog::onCancel() {
    reject();
}
