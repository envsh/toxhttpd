#ifndef CONTACTLIST_H
#define CONTACTLIST_H

#include "compat34.h"
#include "placeholderlineedit.h"
#include "LimeScrollBar.h"
#include <qwidget.h>
#include <qpoint.h>
#include <map>
#include <vector>

// Emoji constants defined in contactlist.cpp
extern const char* EMOJI_FRIEND;
extern const char* EMOJI_GROUP;
extern const char* EMOJI_CONFERENCE;
extern const char* EMOJI_SYSEVENT;
extern const char* EMOJI_UNKNOWN;
extern const char* EMOJI_TOPIC;
extern const char* EMOJI_MATRIX;

struct Contact {
    int id;
    QString name;
    QString type; // "friend", "group", "conference"
    QString status; // "online", "offline", "tcp"
    QString chat_id; // public key
    bool is_connected; // 群组/会议连接状态
    QString lastMessage;
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
    void addContact(Contact* c);
    void removeContact(int id, const QString& type);
    void updateContactLastMessage(int id, const QString& type, const QString& msg);
    bool isFriendLoaded(int friendId);
    void retranslateUi();
    
    // 未读消息数
    void incrementUnread(int id, const QString& type, int count = 1);
    void resetUnread(int id, const QString& type);
    int unreadCount(int id, const QString& type) const;
    
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
    void onSearchTextChanged(const QString& text);
    void onSortMenuClicked();
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
public:
    void showContextMenuAt(int id, const QString& type, const QString& name, const QPoint& globalPos);
    
private:
    void updateView_v3();
    void updateView_v4();
    void sortVisible(std::vector<Contact*>& visible);
    
    void* listWidget;  // QListBox* (Qt3) or QListWidget* (Qt4)
    LimeScrollBar* m_scrollBar;
    ContactList allContacts;
    int contextItemId;           // 右键选中的联系人ID
    QString contextItemType;     // 右键选中的联系人类型
    PlaceholderLineEdit* addInput;
    QPushButton* addBtn;        // 添加好友按钮
    QPushButton* confBtn;       // 创建会议按钮
    QPushButton* groupBtn;      // 创建群组按钮
    PlaceholderLineEdit* joinGroupInput;  // 加入群组输入框
    QPushButton* joinGroupBtn;  // 加入群组按钮
    
    // 未读消息数 map: key=(id, type)
    std::map<std::pair<int, std::string>, int> m_unreadCounts;
    std::map<std::pair<int, std::string>, QString> m_lastMessages;
    
    // 搜索与排序
    PlaceholderLineEdit* searchInput;
    QLabel* countLabel;
    QPushButton* sortBtn;
    QString m_searchText;
    std::vector<QString> m_sortCriteria;
};

#endif // CONTACTLIST_H
