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
    void onRenameNickRequested(int groupId, const QString& groupName);
    void onSwitchAccount();
    void loadMessageHistory();
    void onTranslateRequested(int msgIndex, const QString& text, const QString& targetLang);
    
private:
    std::string selfPubkey;  // 自己的公钥（地址前64字符）
    std::map<std::string, PeerInfo> peerInfoMap;  // 会议/群组 peer info 缓存: "conf_N_M" / "group_N_M"
    
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
