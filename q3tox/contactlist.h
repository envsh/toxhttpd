#ifndef CONTACTLIST_H
#define CONTACTLIST_H

#include "compat34.h"

struct Contact {
    int id;
    QString name;
    QString type; // "friend", "group", "conference"
    QString status; // "online", "offline", "tcp"
};

typedef QPtrList<Contact> ContactList;

class ContactListWidget : public QWidget {
    Q_OBJECT
public:
    explicit ContactListWidget(QWidget* parent = 0);
    
    void setContacts(const ContactList& contacts);
    void clear();
    void retranslateUi();
    
signals:
    void contactSelected(int id, const QString& type);
    
private slots:
    void onTabClicked();
    void onItemClicked();  // Qt3: QListBox selectionChanged -> call this
                             // Qt4: QListWidget itemClicked -> call this
    void onSelectionChanged(); // Qt3 only: QListBox selectionChanged
    
private:
    void updateView_v3();
    void updateView_v4();
    void setTabFilter(int index);
    
    void* listWidget;  // QListBox* (Qt3) or QListWidget* (Qt4)
    ContactList allContacts;
    QString currentFilter;
    int currentTab;
    QLineEdit* addInput;
    QPushButton* addBtn;        // 添加好友按钮
    QPushButton* confBtn;       // 创建会议按钮
    QPushButton* groupBtn;      // 创建群组按钮
    
    // Tab 按钮和过滤器映射
    QPushButton* tabButtons[4];
    static const char* tabFilters[4];
    static const char* tabNames[4];
};

#endif // CONTACTLIST_H
