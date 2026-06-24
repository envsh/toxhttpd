#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <map>
#include <vector>
#include <utility>
#include "compat34.h"
#include <qmainwindow.h>
#include <qsplitter.h>
#include <qwidget.h>
#include "selfinfo.h"
#include "contactlist.h"
#include "chatwidget.h"
#include "restapi.h"
#include "translator.h"
#include "FramelessHelper.h"
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
    void onMessageSending(const QString& message);
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
    void onSetGroupTopicRequested(int groupId);
    void onSetConferenceTitleRequested(int conferenceId);
    void onSwitchAccount();
    void loadMessageHistory();
    void onTranslateRequested(int msgIndex, const QString& text, const QString& targetLang);
    void onSourceClicked(int msgIndex);
    void onRetryClicked(int msgIndex, const QString& mediaUrl);
    void renderHistoryMessages(const std::vector<HistoryMessage>& messages);
    void openSettings();
    void onMenu1Stub();
    void onMenu2Stub();
    void openHomePage();
    
private:
    FramelessHelper* framelessHelper;
    std::string selfPubkey;  // 自己的公钥（地址前64字符）
    std::map<std::string, PeerInfo> peerInfoMap;  // 会议/群组 peer info 缓存: "conf_N_M" / "group_N_M"
    
    QSplitter* splitter;
    QWidget* sidebarWidget;  // 左侧边栏容器
    SelfInfoWidget* selfInfoWidget;
    ContactListWidget* contactListWidget;
    ChatWidget* chatWidget;
    
    int currentChatId;
    QString currentChatType;

    // loading ID 常量
    static const int kLoadAll = 1;
    static const int kLoadFriend = 2;
    static const int kLoadMessages = 3;
    static const int kLoadMembers = 4;
    static const int kLoadSendMsg = 5;

    // 增量累积的 contacts（值类型，避免指针所有权混乱）
    std::vector<ContactData> m_accumulatedContactData;

    // 消息缓存：key=(contactId, contactType)，切换联系人时暂存/恢复
    std::map<std::pair<int, std::string>, std::vector<ChatElement>> m_messageCache;
};

#endif // MAINWINDOW_H
