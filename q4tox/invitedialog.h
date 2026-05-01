#ifndef INVITEDIALOG_H
#define INVITEDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

class InviteDialog : public QDialog {
    Q_OBJECT
public:
    enum Result {
        Accept,
        Reject,
        Ignore
    };
    
    explicit InviteDialog(const QString& info, QWidget* parent = 0);
    Result getResult() const { return result; }
    
private slots:
    void onAcceptClicked();
    void onRejectClicked();
    void onIgnoreClicked();
    
private:
    QPushButton* acceptBtn;
    QPushButton* rejectBtn;
    QPushButton* ignoreBtn;
    Result result;
};

#endif // INVITEDIALOG_H
