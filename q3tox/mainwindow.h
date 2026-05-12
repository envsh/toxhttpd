#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <map>
#include "compat34.h"
#include "selfinfo.h"
#include "contactlist.h"
#include "chatwidget.h"
#include "eventpoller.h"
#include "restapi.h"
#include "translator.h"
#include "friendinfodialog.h"
#include "memberlistdialog.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
    void customEvent(CustomEventBase* event);
    
    public slots:
    void onContactSelected(int id, const QString& type, const QString& name);
    void onMessageSent(const QString& message);
    void onLanguageChanged(const QString& langCode);
    void retranslateUi();
    void handleEvents(const EventList& events);
    void onViewInfoRequested(int id, const QString& type);
    void onDeleteOrLeaveRequested(int id, const QString& type);
    void onInviteToConferenceRequested(int friendId);
    void onInviteToGroupRequested(int friendId);
    void onGroupInviteReceived(int friendNumber, const QString& chatId);
    void onViewMembersRequested(int id, const QString& type);
    void onSwitchAccount();
    void loadMessageHistory();
    
private:
    std::string selfPubkey;  // 自己的公钥（地址前64字符）
    std::map<int, std::string> friendNameMap;  // 好友昵称映射：friend_id → name
    std::map<std::string, std::string> peerNameMap;  // 会议/群组 peer 名字缓存: "conf_N_M" / "group_N_M"
    
    QSplitter* splitter;
    QWidget* sidebarWidget;  // 左侧边栏容器
    SelfInfoWidget* selfInfoWidget;
    ContactListWidget* contactListWidget;
    ChatWidget* chatWidget;
    EventPoller* eventPoller;
    
    int currentChatId;
    QString currentChatType;
};

#endif // MAINWINDOW_H
