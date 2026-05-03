#ifndef CONTACTLIST_H
#define CONTACTLIST_H

#include "compat34.h"
#include "placeholderlineedit.h"

struct Contact {
    int id;
    QString name;
    QString type; // "friend", "group", "conference"
    QString status; // "online", "offline", "tcp"
};

#ifdef QT3_BUILD
typedef QPtrList<Contact> ContactList; // QPtrList<Contact> stores Contact*
#else
typedef QList<Contact*> ContactList; // Qt4: QList<Contact*> to match Qt3's pointer list
#endif

class ContactListWidget : public QWidget {
    Q_OBJECT
public:
    explicit ContactListWidget(QWidget* parent = 0);
    
    void setContacts(const ContactList& contacts);
    void clear();
    void retranslateUi();
    
    signals:
    void contactSelected(int id, const QString& type);
    void viewInfoRequested(int id, const QString& type);
    void deleteOrLeaveRequested(int id, const QString& type);
    void inviteToConferenceRequested(int friendId);
    void inviteToGroupRequested(int friendId);
    void viewMembersRequested(int id, const QString& type);
    
private slots:
    void onTabClicked();
    void onItemClicked();  // Qt3: QListBox selectionChanged -> call this
                               // Qt4: QListWidget itemClicked -> call this
    void onSelectionChanged(); // Qt3 only: QListBox selectionChanged
    void showContextMenu(QPoint pos); // Qt4: right-click menu
    void onJoinGroupClicked();
    
private:
#ifdef QT3_BUILD
    bool eventFilter(QObject* obj, QEvent* event); // Qt3: handle right-click
#endif
    void showContextMenuAt(int id, const QString& type, const QPoint& globalPos);
    
private:
    void updateView_v3();
    void updateView_v4();
    void setTabFilter(int index);
    
    void* listWidget;  // QListBox* (Qt3) or QListWidget* (Qt4)
    ContactList allContacts;
    QString currentFilter;
    int currentTab;
    int contextItemId;           // 右键选中的联系人ID
    QString contextItemType;     // 右键选中的联系人类型
     PlaceholderLineEdit* addInput;
    QPushButton* addBtn;        // 添加好友按钮
    QPushButton* confBtn;       // 创建会议按钮
    QPushButton* groupBtn;      // 创建群组按钮
    PlaceholderLineEdit* joinGroupInput;  // 加入群组输入框
    QPushButton* joinGroupBtn;  // 加入群组按钮
    
    // Tab 按钮和过滤器映射
    QPushButton* tabButtons[4];
    static const char* tabFilters[4];
    static const char* tabNames[4];
};

#endif // CONTACTLIST_H
