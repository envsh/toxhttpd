#ifndef EDITINFODIALOG_H
#define EDITINFODIALOG_H

#include <qdialog.h>
#include <qvbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbt.h>
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
