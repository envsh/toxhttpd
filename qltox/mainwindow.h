#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <vector>
#include <utility>
#include "compat34.h"
#include <qmainwindow.h>
#include <qsplitter.h>
#include <qwidget.h>
#include "selfinfo.h"
#include "contactlist.h"
#include "chatwidget.h"
#include "chatbuffer.h"
#include <qtimer.h>
#include "desktoplyrics.h"
#include "restapi.h"
#include "translator.h"
#include "FramelessHelper.h"
#include "friendinfodialog.h"
#include "screenshotmanager.h"
#include "screenshotpreview.h"
#include "memberlistdialog.h"
#include "ConfigDialog.h"
#include "sleepblocker.h"
#include "plugin_loader.h"
#include "EmbeddedMenuBar.h"
#ifdef QT3_BUILD
#include <qmap.h>
#else
#include <QMap>
#endif

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
    void customEvent(CustomEventBase* event);
    
protected:
    bool event(QEvent* event) override;
protected slots:
    void onFirstPaintComplete();
    void initLoadPlugins();
    
    public slots:
    void onContactSelected(int id, const QString& type, const QString& name);
    void onMessageSending(const QString& message);
    void onFileSendRequested(const QString& filePath);
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
    void clearLyricsHint();
    void showClockOnLyrics();
    void loadMessageHistory();
    void onTranslateRequested(int msgIndex, const QString& text, const QString& targetLang);
    void onTranslateForSendRequested(const QString& text, const QString& targetLang);
    void onSourceClicked(int msgIndex);
    void onRetryClicked(int msgIndex, const QString& mediaUrl, const QString& source);
    void onResendMessage(int msgIndex);
    void onOpenFullSizeImage(int msgIndex, const QString& mediaUrl);
    void renderHistoryMessages(const std::vector<HistoryMessage>& messages);
    void openSettings();
    void openStickerManager();
    void onMenu1Stub();
    void onMenu2Stub();
    void onAboutApp();
    void onScreenshotRequested();
    void onScreenshotReady(const QString& filePath);
    void onScreenshotCancelled();
    void openHomePage();
    void onSettingsSaved(const SettingsChangedMap& changed);
    void onEtappActivated(int index);
    void onEtappCloseAll();
    void openPluginManager();
    
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

    bool m_firstPaintLogged = false;
    int m_paintCounter = 0;

    // 增量累积的 contacts（值类型，避免指针所有权混乱）
    std::vector<ContactData> m_accumulatedContactData;

    // 消息缓存：每个联系人的 ChatHistory，ChatBuffer 统一管理
    ChatBuffer m_chatbuf;

    ScreenshotManager* m_screenshotMgr;
    DesktopLyrics* m_lyrics;
    QTimer* m_msgTimer;
    QTimer* m_clockTimer;
    bool m_hintActive;
    QColor m_savedPlayedColor;
    SleepBlocker* m_sleepBlocker;
    MenuWidget34* m_etappsMenu;

#ifdef QT3_BUILD
    QMap<int, int> m_etappItemToIndex;
#endif
};

#endif // MAINWINDOW_H
