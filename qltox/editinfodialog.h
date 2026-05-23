#ifndef EDITINFODIALOG_H
#define EDITINFODIALOG_H

#include "compat34.h"
#include <qstring.h>
#include <qdialog.h>
#include <qlineedit.h>

class EditInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditInfoDialog(const QString& initialName = "", 
                           const QString& initialStatus = "", 
                           QWidget* parent = nullptr);
    
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
