#ifndef MEMBERLISTDIALOG_H
#define MEMBERLISTDIALOG_H

#include "compat34.h"
#include <qstring.h>
#include <vector>
// #include <qobject.h>  // Qt3 需要 Q_OBJECT 宏
#include "restapi.h"  // for PeerInfo

class MemberListDialog : public QDialog {
    Q_OBJECT
public:
    explicit MemberListDialog(QWidget* parent = nullptr);
    
    void setMembers(const std::vector<PeerInfo>& members);
    void setDialogTitle(const QString& title);

private slots:
    void onClose();

private:
    void* listWidget;  // QListBox* (Qt3) or QListWidget* (Qt4)
};

#endif // MEMBERLISTDIALOG_H
