#ifndef CONTACTLIST_H
#define CONTACTLIST_H

#include "compat34.h"
#include "placeholderlineedit.h"
#include "LimeScrollBar.h"
#include <qwidget.h>
#include <qpoint.h>

// Emoji constants defined in contactlist.cpp
extern const char* EMOJI_FRIEND;
extern const char* EMOJI_GROUP;
extern const char* EMOJI_CONFERENCE;

struct Contact {
    int id;
    QString name;
    QString type; // "friend", "group", "conference"
    QString status; // "online", "offline", "tcp"
    QString chat_id; // public key
    bool is_connected; // 群组/会议连接状态
};

typedef QPtrList<Contact> ContactList;

class ContactListWidget : public QWidget {
    Q_OBJECT
public:
    explicit ContactListWidget(QWidget* parent = 0);
    
    void setContacts(const ContactList& contacts);
    void clear();
    void updateFriendName(int friendId, const QString& newName);
    void updateFriendConnectionStatus(int friendId, const QString& newStatus);
    void updateContact(int id, const QString& type, const QString& name,
                       const QString& chatId, const QString& status);
    bool isFriendLoaded(int friendId);
    void retranslateUi();
    
    signals:
    void contactSelected(int id, const QString& type, const QString& name);
    void viewInfoRequested(int id, const QString& type);
    void deleteOrLeaveRequested(int id, const QString& type);
    void inviteToConferenceRequested(int friendId);
    void inviteToGroupRequested(int friendId);
    void viewMembersRequested(int id, const QString& type);
    void renameNickRequested(int groupId, const QString& groupName);
    void setGroupTopicRequested(int groupId);
    void setConferenceTitleRequested(int conferenceId);
    
private slots:
    void onTabClicked();
    void onItemClicked();  // Qt3: QListBox selectionChanged -> call this
                               // Qt4: QListWidget itemClicked -> call this
    void onSelectionChanged(); // Qt3 only: QListBox selectionChanged
    void showContextMenu(QPoint pos); // Qt4: right-click menu
    void onJoinGroupClicked();
    void onAddFriendClicked();
    void onCreateConferenceClicked();
    void onCreateGroupClicked();
    
private:
    bool eventFilter(QObject* obj, QEvent* event);
    void showContextMenuAt(int id, const QString& type, const QString& name, const QPoint& globalPos);
    
private:
    void updateView_v3();
    void updateView_v4();
    void setTabFilter(int index);
    
    void* listWidget;  // QListBox* (Qt3) or QListWidget* (Qt4)
    LimeScrollBar* m_scrollBar;
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
