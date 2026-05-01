#ifndef EDITINFODIALOG_H
#define EDITINFODIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

class EditInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditInfoDialog(QWidget* parent = 0);
    
    void setName(const QString& name);
    void setStatusMessage(const QString& status);
    
    QString getName() const;
    QString getStatusMessage() const;
    
private slots:
    void onSaveClicked();
    void onCancelClicked();
    
private:
    QLineEdit* nameEdit;
    QLineEdit* statusEdit;
    QPushButton* saveBtn;
    QPushButton* cancelBtn;
};

#endif // EDITINFODIALOG_H
