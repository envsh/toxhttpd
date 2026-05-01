#include "editinfodialog.h"
#include "translator.h"
#include <QLabel>

EditInfoDialog::EditInfoDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(_("edit_info"));
    setFixedSize(300, 200);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Name
    QLabel* nameLabel = new QLabel(_("modals.labels.name"));
    nameEdit = new QLineEdit();
    mainLayout->addWidget(nameLabel);
    mainLayout->addWidget(nameEdit);
    
    // Status message
    QLabel* statusLabel = new QLabel(_("modals.labels.status_message"));
    statusEdit = new QLineEdit();
    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(statusEdit);
    
    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    saveBtn = new QPushButton(_("buttons.save"));
    cancelBtn = new QPushButton(_("buttons.cancel"));
    
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);
    
    connect(saveBtn, SIGNAL(clicked()), this, SLOT(onSaveClicked()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancelClicked()));
}

void EditInfoDialog::setName(const QString& name) {
    nameEdit->setText(name);
}

void EditInfoDialog::setStatusMessage(const QString& status) {
    statusEdit->setText(status);
}

QString EditInfoDialog::getName() const {
    return nameEdit->text();
}

QString EditInfoDialog::getStatusMessage() const {
    return statusEdit->text();
}

void EditInfoDialog::onSaveClicked() {
    accept();
}

void EditInfoDialog::onCancelClicked() {
    reject();
}
