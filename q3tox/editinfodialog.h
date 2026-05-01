#ifndef EDITINFODIALOG_H
#define EDITINFODIALOG_H_

#include "compat34.h"
#include <qstring.h>

class EditInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditInfoDialog(QWidget* parent = nullptr);
    
    QString getName() const;
    QString getStatusMessage() const;
    
private slots:
    void onSave();
    void onCancel();
    
private:
    QLineEdit* nameEdit;
    QLineEdit* statusEdit;
};

#endif // EDITINFODIALOG_H
