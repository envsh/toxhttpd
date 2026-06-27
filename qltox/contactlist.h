#ifndef CONTACTLIST_H
#define CONTACTLIST_H

#include "compat34.h"
#include "placeholderlineedit.h"
#include "LimeScrollBar.h"
#include <qwidget.h>
#include <qpoint.h>
#ifdef QT3_BUILD
#include <qdatetime.h>
#include <qlistview.h>
#else
#include <QDateTime>
#include <QListView>
#include <QAbstractListModel>
#endif
#include <map>
#include <vector>
#include <algorithm>

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
    QString lastMessageTime;
    QDateTime lastActive;
};

typedef QPtrList<Contact> ContactList;

#ifndef QT3_BUILD
class ContactListModel;
#endif

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
    void updateContactLastMessage(int id, const QString& type, const QString& msg,
                                  const QString& timeStr = QString());
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
    void showContextMenu(QPoint pos);
    void onJoinGroupClicked();
    void onAddFriendClicked();
    void onCreateConferenceClicked();
    void onCreateGroupClicked();
    void onItemClicked(); // Qt4: QListView clicked(QModelIndex) -> reads currentIndex()
#ifdef QT3_BUILD
    void onSelectionChanged(); // Qt3: QListView selectionChanged
#endif
    
private:
    bool eventFilter(QObject* obj, QEvent* event);
public:
    void showContextMenuAt(int id, const QString& type, const QString& name, const QPoint& globalPos);
    
private:
    void findAndUpdateItem(int id, const QString& type);
    void rebuildSortFilter();
    
    void* listWidget;  // QListView* (both Qt3 and Qt4)
    LimeScrollBar* m_scrollBar;
#ifndef QT3_BUILD
    ContactListModel* m_model;
#endif
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
    std::map<std::pair<int, std::string>, QString> m_lastMessageTimes;
    
    // 搜索与排序
    PlaceholderLineEdit* searchInput;
    QLabel* countLabel;
    QPushButton* sortBtn;
    QString m_searchText;
    std::vector<QString> m_sortCriteria;
};

#endif // CONTACTLIST_H
