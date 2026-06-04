#include "mainwindow.h"
#include "CustomTitleBar.h"
#include "restapi.h"
#include "eventpoller.h"
#include "translator.h"
#include "logindialog.h"
#include "conferenceinvitedialog.h"
#include "groupinvitedialog.h"
#include "friendinfodialog.h"
#include "memberlistdialog.h"
#include "cJSON.h"
#include "appsetup.h"
#include <qmessagebox.h>
#include <qtextcodec.h>
#include <qtextstream.h>
#include <qradiobutton.h>
#include <qinputdialog.h>
#include "placeholderlineedit.h"
#include "sound.h"
#include <qfile.h>
#include "toastwidget.h"

// 读取保存的语言设置
static QString loadSavedLanguage() {
    QString home = qGetHomePath();
    QFile file(home + "/.q3tox_lang");
    if (qOpenReadOnly(file)) {
        QTextStream stream(&file);
        QString lang = qTrim(stream.readLine());
        file.close();
        if (!lang.isEmpty()) return lang;
    }
    return "zh-CN"; // 默认简体
}

static void playNotificationSound() {
    const char* paths[] = {
        "sound/notification.s16le.pcm",
        "web/sound/notification.s16le.pcm",
        "../web/sound/notification.s16le.pcm"
    };
    for (int i = 0; i < 3; i++) {
        if (QFile::exists(paths[i])) {
            playSoundNopcm(paths[i]);
            return;
        }
    }
}

static QString formatElapsedMs(int64_t ms) {
    if (ms < 1000)
        return QString::number((int)ms) + "ms";
    return QString::number((int)(ms / 1000)) + "." + QString::number((int)((ms % 1000) / 100)) + "s";
}

// 保存语言设置
static void saveLanguage(const QString& lang) {
    QString home = qGetHomePath();
    QFile file(home + "/.q3tox_lang");
    if (qOpenWriteOnly(file)) {
        QTextStream stream(&file);
        stream << lang << "\n";
        file.close();
    }
}

MainWindow::MainWindow(QWidget* parent) 
    : QMainWindow(parent
#ifdef QT3_BUILD
        , "mainwindow", Qt::WType_TopLevel | Qt::WStyle_Customize | Qt::WSubWindow | Qt::WStyle_MinMax | Qt::WStyle_SysMenu | Qt::WStyle_NoBorder
#else
        , Qt::FramelessWindowHint
#endif
    ), 
    currentChatId(-1), currentChatType("") {
    qWarning("MainWindow: constructor started");
    
    // 设置窗口
    qSetWindowTitle(this, _("app_title"));
    setGeometry(100, 100, 1100, 700);
    
    // 设置 UTF-8 编解码器
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    
    QWidget* centralContainer = new QWidget(this);
    QBoxLayout* mainLayout = qNewBoxLayout(centralContainer, QBoxLayout::TopToBottom, 0, 0);

    // 主分割器（左右布局）
    splitter = new QSplitter(Qt::Horizontal, centralContainer);
    
    // ===== 左侧边栏 =====
    QWidget* sidebar = new QWidget(splitter);
    QBoxLayout* sidebarLayout = qNewBoxLayout(sidebar, QBoxLayout::TopToBottom, 0, 0);
    sidebarLayout->setSpacing(0);
    qSetMargins(sidebarLayout, 0, 0, 0, 0);
    
    // 个人信息区
    selfInfoWidget = new SelfInfoWidget(sidebar);
    sidebarLayout->addWidget(selfInfoWidget);
    
    // 联系人列表
    contactListWidget = new ContactListWidget(sidebar);
    sidebarLayout->addWidget(contactListWidget, 1); // stretch
    
    splitter->addWidget(sidebar);
    sidebarWidget = sidebar;  // 保存 sidebar 指针
    
    // ===== 右侧聊天区 =====
    chatWidget = new ChatWidget(splitter);
    splitter->addWidget(chatWidget);
    
    // 设置 splitter 比例：左 1/3 (367px)，右 2/3 (733px)
    // 窗口固定 1100px，所以直接算像素值
    int leftWidth = 367;   // 1100 / 3 ≈ 367
    int rightWidth = 733;  // 1100 - 367 = 733
    
#ifdef QT3_BUILD
    QValueList<int> sizes;
    sizes << leftWidth << rightWidth;
    splitter->setSizes(sizes);
#else
    QList<int> sizes;
    sizes << leftWidth << rightWidth;
    splitter->setSizes(sizes);
#endif

    CustomTitleBar* titleBar = new CustomTitleBar(centralContainer);
    mainLayout->addWidget(titleBar, 0);
    mainLayout->addWidget(splitter, 1);
    setCentralWidget(centralContainer);

    // 连接信号槽
    connect(selfInfoWidget, SIGNAL(switchAccountRequested()),
            this, SLOT(onSwitchAccount()));
    connect(contactListWidget, SIGNAL(contactSelected(int, const QString&, const QString&)), 
            this, SLOT(onContactSelected(int, const QString&, const QString&)));
    connect(contactListWidget, SIGNAL(viewInfoRequested(int, const QString&)),
            this, SLOT(onViewInfoRequested(int, const QString&)));
    connect(contactListWidget, SIGNAL(deleteOrLeaveRequested(int, const QString&)),
            this, SLOT(onDeleteOrLeaveRequested(int, const QString&)));
    connect(contactListWidget, SIGNAL(viewMembersRequested(int, const QString&)),
            this, SLOT(onViewMembersRequested(int, const QString&)));
    connect(contactListWidget, SIGNAL(renameNickRequested(int, const QString&)),
            this, SLOT(onRenameNickRequested(int, const QString&)));
    connect(contactListWidget, SIGNAL(setGroupTopicRequested(int)),
            this, SLOT(onSetGroupTopicRequested(int)));
    connect(contactListWidget, SIGNAL(setConferenceTitleRequested(int)),
            this, SLOT(onSetConferenceTitleRequested(int)));
    connect(contactListWidget, SIGNAL(inviteToConferenceRequested(int)),
            this, SLOT(onInviteToConferenceRequested(int)));
    connect(chatWidget, SIGNAL(messageSent(const QString&)), this, SLOT(onMessageSent(const QString&)));
    connect(chatWidget, SIGNAL(languageChanged(const QString&)), 
            this, SLOT(onLanguageChanged(const QString&)));
    connect(chatWidget, SIGNAL(translateRequested(int, const QString&, const QString&)),
            this, SLOT(onTranslateRequested(int, const QString&, const QString&)));
    connect(&Translator::instance(), SIGNAL(languageChanged()), this, SLOT(retranslateUi()));
    
    // 启动事件轮询引擎
    EventPoller::start();
    ToxAPI::setEventTarget(this);
    ToxAPI::startPollEvent();
    
    // 异步加载初始数据
    qWarning("MainWindow: requesting initial data load (async)");
    ToxAPI::loadAllData();
    chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
    
    // 设置 frameless 窗口
    framelessHelper = new FramelessHelper(this);
    framelessHelper->setup(this);
    titleBar->connectFramelessHelper(framelessHelper);

    // ≡ 应用菜单：切换 menubar 显隐
    QObject::connect(titleBar, SIGNAL(appMenuClicked()), titleBar, SLOT(toggleMenu()));

    EmbeddedMenuBar* mb = titleBar->menuBar();

    MenuWidget34* file = mb->addMenu(qFromUtf8("文件(&F)"));
    EmbeddedMenuBar::addItem(file, qFromUtf8("新建\tCtrl+N"), this, SLOT(onMenu1Stub()));
    EmbeddedMenuBar::addSeparator(file);
    EmbeddedMenuBar::addItem(file, qFromUtf8("退出\tCtrl+Q"), this, SLOT(close()));

    MenuWidget34* edit = mb->addMenu(qFromUtf8("编辑(&E)"));
    EmbeddedMenuBar::addItem(edit, qFromUtf8("撤销\tCtrl+Z"), this, SLOT(onMenu1Stub()));
    EmbeddedMenuBar::addItem(edit, qFromUtf8("重做\tCtrl+Shift+Z"), this, SLOT(onMenu1Stub()));

    MenuWidget34* tool = mb->addMenu(qFromUtf8("工具(&T)"));
    EmbeddedMenuBar::addItem(tool, qFromUtf8("设置(&S)...\tCtrl+,"), this, SLOT(onMenu1Stub()));

    MenuWidget34* help = mb->addMenu(qFromUtf8("帮助(&H)"));
    EmbeddedMenuBar::addItem(help, _("menu.homepage"), this, SLOT(openHomePage()));
    EmbeddedMenuBar::addItem(help, _("menu.aboutqt"), qApp, SLOT(aboutQt()));
    EmbeddedMenuBar::addItem(help, qFromUtf8("关于(&A)..."), this, SLOT(onMenu1Stub()));

    mb->finalize();
}

MainWindow::~MainWindow() {
    ToxAPI::stopPollEvent();
    EventPoller::stop();
}

void MainWindow::customEvent(CustomEventBase* event) {
    // 事件轮询结果
    if (event->type() == EventListReadyType) {
        EventListEvent* e = static_cast<EventListEvent*>(event);
        handleEvents(e->events);
        return;
    }
    
    // 数据加载完成
    if (event->type() == ApiResultReadyType) {
        ApiResultEvent* e = static_cast<ApiResultEvent*>(event);
        
        // 增量数据到达（逐步展示）
        if (e->type == ApiLoadPartialData) {
            PartialDataEvent* evt = static_cast<PartialDataEvent*>(event);
            
            if (evt->loadedMask & PartialDataEvent::kSelf) {
                selfInfoWidget->updateInfo(
                    qFromUtf8(evt->selfName.c_str()),
                    qFromUtf8(evt->selfStatusMsg.c_str()),
                    qFromUtf8(evt->selfConnStatus.c_str()),
                    qFromUtf8(evt->selfAddress.c_str()));
                std::string addr = evt->selfAddress;
                if (addr.length() >= 64) {
                    selfPubkey = addr.substr(0, 64);
                }
            }
            
            if (evt->loadedMask & PartialDataEvent::kContacts) {
                for (const auto& cd : evt->contacts) {
                    // 按 (id, type) 匹配，原地更新已有条目（如好友占位→真实数据）
                    bool updated = false;
                    for (auto& existing : m_accumulatedContactData) {
                        if (existing.id == cd.id && existing.type == cd.type) {
                            existing = cd;
                            updated = true;
                            break;
                        }
                    }
                    if (!updated) {
                        m_accumulatedContactData.push_back(cd);
                    }
                    
                    if (cd.type == "friend") {
                        std::string key = "friend_" + std::to_string(cd.id);
                        peerInfoMap[key].name = cd.name;
                        peerInfoMap[key].peerNumber = cd.id;
                        peerInfoMap[key].publicKey = cd.chatId;
                        peerInfoMap[key].iconUrl = cd.iconUrl;
                        peerInfoMap[key].isSelf = false;
                        if (cd.status == "tcp") {
                            peerInfoMap[key].status = 1;
                            peerInfoMap[key].statusStr = "tcp";
                        } else if (cd.status == "udp") {
                            peerInfoMap[key].status = 2;
                            peerInfoMap[key].statusStr = "udp";
                        } else {
                            peerInfoMap[key].status = 0;
                            peerInfoMap[key].statusStr = "none";
                        }
                    }
                }
                ContactList cl;
                for (const auto& cd : m_accumulatedContactData) {
                    Contact* c = new Contact();
                    c->id = cd.id;
                    c->name = qFromUtf8(cd.name.c_str());
                    c->type = qFromUtf8(cd.type.c_str());
                    c->status = qFromUtf8(cd.status.c_str());
                    c->chat_id = qFromUtf8(cd.chatId.c_str());
                    c->is_connected = cd.isConnected;
                    cl.append(c);
                }
                contactListWidget->setContacts(cl);
            }
            return;
        }
        
        if (e->type == ApiLoadAllData) {
            qWarning("MainWindow: initial data load complete");
            chatWidget->loadingBar()->hideLoading(kLoadAll);
            if (ToxAPI::onLoadAllDataComplete()) {
                chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
                ToxAPI::loadAllData();
            }
            return;
        }

        // 懒加载好友详情结果
        if (e->type == ApiLoadFriendDetail) {
            FriendDetailEvent* evt = static_cast<FriendDetailEvent*>(event);
            chatWidget->loadingBar()->hideLoading(kLoadFriend);
            if (evt->success) {
                contactListWidget->updateContact(
                    evt->friendId, "friend",
                    qFromUtf8(evt->name.c_str()),
                    qFromUtf8(evt->publicKey.c_str()),
                    qFromUtf8(evt->statusStr.c_str()));
                std::string key = "friend_" + std::to_string(evt->friendId);
                peerInfoMap[key].name = evt->name;
                peerInfoMap[key].peerNumber = evt->friendId;
                peerInfoMap[key].publicKey = evt->publicKey;
                peerInfoMap[key].iconUrl = evt->iconUrl;
                int s = 0;
                if (evt->statusStr == "tcp") s = 1;
                else if (evt->statusStr == "udp") s = 2;
                peerInfoMap[key].status = s;
                peerInfoMap[key].statusStr = evt->statusStr;
            }
            return;
        }
        
        // 消息发送结果
        if (e->type == ApiSendFriendMessage || e->type == ApiSendConferenceMessage || e->type == ApiSendGroupMessage) {
            MessageSentResultEvent* evt = static_cast<MessageSentResultEvent*>(event);
            chatWidget->loadingBar()->hideLoading(kLoadSendMsg);
            QString targetName;
            for (const auto& cd : m_accumulatedContactData) {
                if (cd.id == evt->chatId && cd.type == evt->chatType) {
                    targetName = qFromUtf8(cd.name);
                    break;
                }
            }
            if (targetName.isEmpty())
                targetName = qFromUtf8(evt->chatType) + " " + QString::number(evt->chatId);
            if (!evt->success)
                ToastWidget::show(chatWidget, _("send_failed").arg(targetName).arg(formatElapsedMs(evt->elapsedMs)), 8000);
            else
                ToastWidget::show(chatWidget, _("send_success").arg(targetName).arg(formatElapsedMs(evt->elapsedMs)), 2000);
            return;
        }
        
        // 会议加入结果 → 重新加载联系人
        if (e->type == ApiJoinConference) {
            chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
            ToxAPI::loadAllData();
            return;
        }
        
        // 成员列表加载完成
        if (e->type == ApiLoadGroupMembers) {
            MembersLoadedEvent* evt = static_cast<MembersLoadedEvent*>(event);
            chatWidget->loadingBar()->hideLoading(kLoadMembers);
            if (evt->contactId == currentChatId && evt->contactType == std::string(qToUtf8(currentChatType).data())) {
                qWarning("MembersLoaded: %d members for %s %d", (int)evt->members.size(), evt->contactType.c_str(), evt->contactId);
                for (const auto& m : evt->members) {
                    std::string key = evt->contactType + "_" + std::to_string(evt->contactId) + "_" + std::to_string(m.peerNumber);
                    peerInfoMap[key] = m;
                }
            }
            return;
        }
        
        // 历史消息加载完成
        if (e->type == ApiLoadMessageHistory) {
            MessageHistoryLoadedEvent* evt = static_cast<MessageHistoryLoadedEvent*>(event);
            chatWidget->loadingBar()->hideLoading(kLoadMessages);
            if (!evt->success) {
                ToastWidget::show(chatWidget, _("load_messages_failed").arg(formatElapsedMs(evt->elapsedMs)), 8000);
                return;
            }
            if (evt->messages.empty()) {
                ToastWidget::show(chatWidget, _("load_messages_empty").arg(formatElapsedMs(evt->elapsedMs)), 2000);
                return;
            }
            if (evt->contactId == currentChatId && evt->contactType == std::string(qToUtf8(currentChatType).data())) {
                qWarning("MessageHistoryLoaded: %d messages for %s %d", (int)evt->messages.size(), evt->contactType.c_str(), evt->contactId);
                chatWidget->clearMessages();
                renderHistoryMessages(evt->messages);
            }
            return;
        }
        
        // 翻译结果
        if (e->type == ApiTranslate) {
            TranslateResultEvent* tev = static_cast<TranslateResultEvent*>(event);
            chatWidget->onTranslateResult(tev->msgIndex, tev->success,
                qFromUtf8(tev->translatedText.data(), (int)tev->translatedText.size()),
                qFromUtf8(tev->errorMessage.data(), (int)tev->errorMessage.size()));
            return;
        }
    }
}

void MainWindow::onContactSelected(int id, const QString& type, const QString& name) {
    // 如果已经是当前选中的聊天对象，不重新加载（避免清空消息）
    if (id == currentChatId && type == currentChatType) {
        qWarning("onContactSelected: same contact, ignoring");
        return;
    }
    
    // 好友未加载详情 → 懒加载
    if (type == "friend" && !contactListWidget->isFriendLoaded(id)) {
        qWarning("onContactSelected: friend %d not loaded, lazy loading", id);
        currentChatId = -1; // reset so next click won't be ignored
        chatWidget->loadingBar()->showLoading(kLoadFriend, _("loading_contact"));
        ToxAPI::lazyLoadFriendDetail(id);
        return;
    }
    
    // 保存当前联系人的消息到缓存
    if (currentChatId != -1 && !currentChatType.isEmpty()) {
        auto key = std::make_pair(currentChatId, std::string(qToUtf8(currentChatType).data()));
        std::vector<ChatMessage> msgs;
        int n = chatWidget->messageCount();
        for (int i = 0; i < n; ++i)
            msgs.push_back(chatWidget->messageAt(i));
        m_messageCache[key] = std::move(msgs);
    }
    
    qWarning("onContactSelected: id=%d, type=%s", id, qToUtf8(type).data());
    currentChatId = id;
    currentChatType = type;
    
    QString headerText;
    QString emoji;
    if (type == "friend") {
        emoji = EMOJI_FRIEND;
        headerText = emoji + " " + name;
    } else if (type == "group") {
        emoji = EMOJI_GROUP;
        headerText = emoji + " " + name;
    } else if (type == "conference") {
        emoji = EMOJI_CONFERENCE;
        headerText = emoji + " " + name;
    }
    
    chatWidget->setHeaderText(headerText);
    
    std::string typeStr = std::string(qToUtf8(type).data());
    auto key = std::make_pair(id, typeStr);
    auto cacheIt = m_messageCache.find(key);
    bool hasCache = (cacheIt != m_messageCache.end());
    
    if (hasCache) {
        // 缓存命中：恢复缓存消息，同时后台拉取刷新
        chatWidget->clearMessages();
        for (const auto& msg : cacheIt->second)
            chatWidget->appendMessage(msg);
        chatWidget->loadingBar()->showLoading(kLoadMessages, _("loading_messages"));
        ToxAPI::getMessagesHistory(id, typeStr);
    } else {
        // 缓存未命中：显示 loading 并拉取
        chatWidget->clearMessages();
        chatWidget->loadingBar()->showLoading(kLoadMessages, _("loading_messages"));
        ToxAPI::getMessagesHistory(id, typeStr);
    }
    
    // 异步预加载成员列表到 peerInfoMap 缓存
    if (type == "group") {
        chatWidget->loadingBar()->showLoading(kLoadMembers, _("loading_members"));
        ToxAPI::getGroupMembers(id);
    } else if (type == "conference") {
        chatWidget->loadingBar()->showLoading(kLoadMembers, _("loading_members"));
        ToxAPI::getConferenceMembers(id);
    }
}

void MainWindow::onMessageSent(const QString& message) {
    if (currentChatId == -1 || currentChatType.isEmpty()) {
        QMessageBox::warning(this, _("select_chat_first"), _("select_chat_first"));
        return;
    }
    
    // ✅ 改为异步请求
    ApiRequestType reqType;
    if (currentChatType == "friend") {
        reqType = ApiSendFriendMessage;
    } else if (currentChatType == "group") {
        reqType = ApiSendGroupMessage;
    } else if (currentChatType == "conference") {
        reqType = ApiSendConferenceMessage;
    } else {
        qWarning("Unknown chat type: %s", qToUtf8(currentChatType).data());
        return;
    }
    
    chatWidget->loadingBar()->showLoading(kLoadSendMsg, _("sending_message"));
    if (reqType == ApiSendFriendMessage) {
        ToxAPI::sendFriendMessage(currentChatId, std::string(qToUtf8(message)));
    } else if (reqType == ApiSendConferenceMessage) {
        ToxAPI::sendConferenceMessage(currentChatId, std::string(qToUtf8(message)));
    } else if (reqType == ApiSendGroupMessage) {
        ToxAPI::sendGroupMessage(currentChatId, std::string(qToUtf8(message)));
    }
    
    // 乐观更新：先显示在界面
    chatWidget->appendMessage(message, "self", "Me", -1, getCurrentTime());
}

void MainWindow::handleEvents(const EventList& events) {
    for (size_t i = 0; i < events.size(); ++i) {
        const Event& e = events[i];
        qWarning("Event: %s", e.type.c_str());
        
        if (e.type == "friend_message") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* friendIdItem = cJSON_GetObjectItem(root, "friend_id");
                cJSON* messageItem = cJSON_GetObjectItem(root, "message");
                if (friendIdItem && messageItem) {
                    int friendId = friendIdItem->valueint;
                    QString message = qFromUtf8(cJSON_GetStringValue(messageItem));
                    if (friendId == currentChatId && currentChatType == "friend") {
                        chatWidget->appendMessage(message, "other", QString(),
                                         friendId, getCurrentTime());
                    }
                    if (!qIsAppActive())
                        playNotificationSound();
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "conference_invite") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* friendNumberItem = cJSON_GetObjectItem(root, "friend_number");
                cJSON* cookieItem = cJSON_GetObjectItem(root, "cookie");
                if (friendNumberItem && cookieItem) {
                    QString friendNumber = QString::number(friendNumberItem->valueint);
                    QString cookie = qFromUtf8(cJSON_GetStringValue(cookieItem));
                    
                     ConferenceInviteDialog dialog(friendNumber, cookie, this);
                      dialog.exec();
                      
                      if (dialog.getResult() == ConferenceInviteDialog::Accept) {
                         ToxAPI::joinConference(friendNumber.toInt(), std::string(qToUtf8(cookie)));
                         
                         QMessageBox::information(this, _("conference_joined"), 
                                                         _("conference_joined").arg(dialog.getCookie()));
                     } else if (dialog.getResult() == ConferenceInviteDialog::Reject) {
                         ToxAPI::rejectConference(friendNumber.toInt());
                     }
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "conference_message") {
            qWarning("Processing conference_message event");
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* confNumberItem = cJSON_GetObjectItem(root, "conference_number");
                cJSON* messageItem = cJSON_GetObjectItem(root, "message");
                cJSON* peerNumberItem = cJSON_GetObjectItem(root, "peer_number");
                cJSON* peerNameItem = cJSON_GetObjectItem(root, "peer_name");
                
                if (confNumberItem && messageItem) {
                    int confNumber = confNumberItem->valueint;
                    QString message = qFromUtf8(cJSON_GetStringValue(messageItem));
                    int peerNumber = peerNumberItem ? peerNumberItem->valueint : -1;
                    
                    // 更新 peer info 缓存
                    if (peerNameItem && cJSON_IsString(peerNameItem)) {
                        std::string key = "conference_" + std::to_string(confNumber) + "_" + std::to_string(peerNumber);
                        peerInfoMap[key].name = std::string(cJSON_GetStringValue(peerNameItem));
                        peerInfoMap[key].peerNumber = peerNumber;
                    }
                    
                    qWarning("confNumber=%d, currentChatId=%d, currentChatType=%s, match=%d", 
                             confNumber, currentChatId, qToUtf8(currentChatType).data(),
                             (confNumber == currentChatId && currentChatType == "conference"));
                    
                    if (confNumber == currentChatId && currentChatType == "conference") {
                        // 查询缓存获取 peer 名字
                        std::string key = "conference_" + std::to_string(confNumber) + "_" + std::to_string(peerNumber);
                        auto it = peerInfoMap.find(key);
                        QString senderName;
                        if (it != peerInfoMap.end() && !it->second.name.empty())
                            senderName = qFromUtf8(it->second.name.c_str());
                        qWarning("Appending conference message: %s", qToUtf8(message).data());
                        chatWidget->appendMessage(message, "other", senderName, peerNumber, getCurrentTime());
                    }
                    if (!qIsAppActive())
                        playNotificationSound();
                } else {
                    qWarning("conference_message: missing confNumber or message");
                }
                cJSON_Delete(root);
            } else {
                qWarning("conference_message: failed to parse JSON: %s", e.data.c_str());
            }
        } else if (e.type == "self_connection_status") {
            chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
            ToxAPI::loadAllData();
        } else if (e.type == "group_invite") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* friendNumberItem = cJSON_GetObjectItem(root, "friend_number");
                cJSON* chatIdItem = cJSON_GetObjectItem(root, "chat_id");
                if (friendNumberItem && chatIdItem) {
                    int friendNumber = friendNumberItem->valueint;
                    QString chatId = qFromUtf8(cJSON_GetStringValue(chatIdItem));
                    onGroupInviteReceived(friendNumber, chatId);
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "group_message") {
            qWarning("Processing group_message event");
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* groupNumberItem = cJSON_GetObjectItem(root, "group_number");
                cJSON* messageItem = cJSON_GetObjectItem(root, "message");
                cJSON* peerNumberItem = cJSON_GetObjectItem(root, "peer_number");
                cJSON* peerNameItem = cJSON_GetObjectItem(root, "peer_name");
                
                if (groupNumberItem && messageItem) {
                    int groupNumber = groupNumberItem->valueint;
                    QString message = qFromUtf8(cJSON_GetStringValue(messageItem));
                    int peerNumber = peerNumberItem ? peerNumberItem->valueint : -1;
                    
                    // 更新 peer info 缓存
                    if (peerNameItem && cJSON_IsString(peerNameItem)) {
                        std::string key = "group_" + std::to_string(groupNumber) + "_" + std::to_string(peerNumber);
                        peerInfoMap[key].name = std::string(cJSON_GetStringValue(peerNameItem));
                        peerInfoMap[key].peerNumber = peerNumber;
                    }
                    
                    if (groupNumber == currentChatId && currentChatType == "group") {
                        // 查询缓存获取 peer 名字和 IP
                        std::string key = "group_" + std::to_string(groupNumber) + "_" + std::to_string(peerNumber);
                        auto it = peerInfoMap.find(key);
                        QString senderName;
                        QString ipAddress;
                        if (it != peerInfoMap.end()) {
                            if (!it->second.name.empty())
                                senderName = qFromUtf8(it->second.name.c_str());
                            ipAddress = qFromUtf8(it->second.peerIp.c_str());
                        }
                        chatWidget->appendMessage(message, "other", senderName, peerNumber, getCurrentTime(), "", "", ipAddress);
                    }
                    if (!qIsAppActive())
                        playNotificationSound();
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "conference_peer_name") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* confNumberItem = cJSON_GetObjectItem(root, "conference_number");
                cJSON* peerNumberItem = cJSON_GetObjectItem(root, "peer_number");
                cJSON* nameItem = cJSON_GetObjectItem(root, "name");
                if (confNumberItem && peerNumberItem && nameItem && cJSON_IsString(nameItem)) {
                    std::string key = "conference_" + std::to_string(confNumberItem->valueint)
                        + "_" + std::to_string(peerNumberItem->valueint);
                    peerInfoMap[key].name = std::string(cJSON_GetStringValue(nameItem));
                    peerInfoMap[key].peerNumber = peerNumberItem->valueint;
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "group_peer_name") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* groupNumberItem = cJSON_GetObjectItem(root, "group_number");
                cJSON* peerNumberItem = cJSON_GetObjectItem(root, "peer_number");
                cJSON* nameItem = cJSON_GetObjectItem(root, "name");
                if (groupNumberItem && peerNumberItem && nameItem && cJSON_IsString(nameItem)) {
                    std::string key = "group_" + std::to_string(groupNumberItem->valueint)
                        + "_" + std::to_string(peerNumberItem->valueint);
                    peerInfoMap[key].name = std::string(cJSON_GetStringValue(nameItem));
                    peerInfoMap[key].peerNumber = peerNumberItem->valueint;
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "friend_name") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* fid = cJSON_GetObjectItem(root, "friend_id");
                cJSON* nameItem = cJSON_GetObjectItem(root, "name");
                if (fid && nameItem && cJSON_IsString(nameItem)) {
                    int friendId = fid->valueint;
                    std::string newName = cJSON_GetStringValue(nameItem);
                    std::string key = "friend_" + std::to_string(friendId);
                    peerInfoMap[key].name = newName;
                    contactListWidget->updateFriendName(friendId, qFromUtf8(newName.c_str()));
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "friend_status") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* fid = cJSON_GetObjectItem(root, "friend_id");
                cJSON* statusItem = cJSON_GetObjectItem(root, "status");
                if (fid && statusItem) {
                    int friendId = fid->valueint;
                    int s = statusItem->valueint;
                    std::string key = "friend_" + std::to_string(friendId);
                    peerInfoMap[key].status = s;
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "friend_connection_status") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* fid = cJSON_GetObjectItem(root, "friend_id");
                cJSON* statusItem = cJSON_GetObjectItem(root, "status");
                if (fid && statusItem && cJSON_IsString(statusItem)) {
                    int friendId = fid->valueint;
                    std::string statusStr = cJSON_GetStringValue(statusItem);
                    std::string key = "friend_" + std::to_string(friendId);
                    peerInfoMap[key].status = (statusStr == "udp") ? 2 : (statusStr == "tcp") ? 1 : 0;
                    peerInfoMap[key].statusStr = statusStr;
                    contactListWidget->updateFriendConnectionStatus(friendId, qFromUtf8(statusStr.c_str()));
                }
                cJSON_Delete(root);
            }
        }
    }
}

void MainWindow::onLanguageChanged(const QString& langCode) {
    saveLanguage(langCode);
    Translator::instance().loadLanguage(langCode);
    QtappSetup::installQtTranslations(langCode);
}

void MainWindow::retranslateUi() {
    qSetWindowTitle(this, _("app_title"));
    
    // 更新子控件
    if (selfInfoWidget) selfInfoWidget->retranslateUi();
    if (contactListWidget) contactListWidget->retranslateUi();
    if (chatWidget) chatWidget->retranslateUi();
    
    if (currentChatId == -1) {
        chatWidget->setHeaderText(_("select_chat_object"));
    } else {
        QString headerText;
        if (currentChatType == "friend") {
            headerText = _("chat_with_friend").arg(QString::number(currentChatId));
        } else if (currentChatType == "group") {
            headerText = _("group") + " " + QString::number(currentChatId);
        } else if (currentChatType == "conference") {
            headerText = _("conference_item") + " " + QString::number(currentChatId);
        }
        chatWidget->setHeaderText(headerText);
    }
}

void MainWindow::onViewInfoRequested(int id, const QString& type) {
    FriendInfoDialog dialog(this);
    
    if (type == "friend") {
        FriendInfo info;
        if (ToxAPI::getFriendInfo(id, info)) {
            dialog.setInfo(info);
        } else {
            dialog.setInfo(id, _("no_name"), type);
        }
    } else if (type == "conference") {
        dialog.setTitle(_("modals.conference_info_title"));
        std::vector<ConferenceInfo> conferences = ToxAPI::getConferencesSync();
        bool isConnected = false;
        QString chatId = "";
        QString statusText = "";
        int memberCount = 0;
        for (size_t i = 0; i < conferences.size(); ++i) {
            if (conferences[i].conferenceNumber == (uint32_t)id) {
                isConnected = conferences[i].isConnected;
                chatId = qFromUtf8(conferences[i].chatId.c_str());
                statusText = qFromUtf8(conferences[i].statusText.c_str());
                memberCount = conferences[i].memberCount;
                break;
            }
        }
        dialog.setInfo(id, _("conference_item") + " " + QString::number(id), type,
                       statusText, QString(), "", isConnected, chatId);
        dialog.setPeerCount(memberCount);
    } else if (type == "group") {
        dialog.setTitle(_("modals.group_info_title"));
        std::vector<GroupInfo> groups = ToxAPI::getGroupsSync();
        bool isConnected = false;
        QString chatId = "";
        QString statusText = "";
        int memberCount = 0;
        for (size_t i = 0; i < groups.size(); ++i) {
            if (groups[i].groupNumber == (uint32_t)id) {
                isConnected = groups[i].isConnected;
                chatId = qFromUtf8(groups[i].chatId.c_str());
                statusText = qFromUtf8(groups[i].statusText.c_str());
                memberCount = groups[i].memberCount;
                break;
            }
        }
        dialog.setInfo(id, _("group_item") + " " + QString::number(id), type,
                       statusText, QString(), "", isConnected, chatId);
        dialog.setPeerCount(memberCount);
    }
    
    dialog.exec();
}

void MainWindow::onViewMembersRequested(int id, const QString& type) {
    std::vector<PeerInfo> members;
    QString title;
    
    if (type == "conference") {
        members = ToxAPI::getConferenceMembersSync(id);
        title = _("member_list.title.conference").arg(QString::number(id));
    } else if (type == "group") {
        members = ToxAPI::getGroupMembersSync(id);
        title = _("member_list.title.group").arg(QString::number(id));
    } else {
        return;
    }
    
    MemberListDialog dialog(this);
    dialog.setDialogTitle(title);
    dialog.setMembers(members);
    dialog.exec();
}

void MainWindow::onRenameNickRequested(int groupId, const QString& groupName) {
    std::string globalName = qToUtf8(selfInfoWidget->selfName()).data();

    // Scan peerInfoMap for own peer in this group
    std::string currentGroupNick;
    std::string prefix = "group_" + std::to_string(groupId) + "_";
    for (const auto& pair : peerInfoMap) {
        if (pair.second.isSelf && pair.first.find(prefix) == 0) {
            currentGroupNick = pair.second.name;
            break;
        }
    }
    if (currentGroupNick.empty()) {
        currentGroupNick = globalName;
    }

    QDialog dialog(this);
    qSetWindowTitle(&dialog, _("rename.title") + QString(" - ") + groupName);
    dialog.resize(380, 220);

    QBoxLayout* layout = qNewBoxLayout(&dialog, QBoxLayout::TopToBottom, 10, 10);

    // Current nickname (from cache or fallback to global)
    std::string displayName = currentGroupNick.empty() ? "--" : currentGroupNick;
    QLabel* currentLabel = new QLabel(
        _("rename.current_nick") + QString(": ") + qFromUtf8(displayName.c_str()), &dialog);
    layout->addWidget(currentLabel);

    // Use global nick
    QRadioButton* selfRadio = new QRadioButton(
        _("rename.use_self_nick") + QString(" (%1)").arg(qFromUtf8(globalName.c_str())), &dialog);
    selfRadio->setChecked(true);
    layout->addWidget(selfRadio);

    // Custom nick
    QRadioButton* customRadio = new QRadioButton(_("rename.custom_nick"), &dialog);
    layout->addWidget(customRadio);

    PlaceholderLineEdit* nameEdit = new PlaceholderLineEdit(_("rename.enter_nick"), &dialog);
    nameEdit->setText(qFromUtf8(currentGroupNick.c_str()));
    nameEdit->setEnabled(false);
    layout->addWidget(nameEdit);

    // Random nick
    QRadioButton* randomRadio = new QRadioButton(_("rename.random_nick"), &dialog);
    layout->addWidget(randomRadio);

    QLabel* randomLabel = new QLabel(
        qFromUtf8(ToxAPI::getRandomNameSync().c_str()), &dialog);
    layout->addWidget(randomLabel);

    // Buttons
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));

    QPushButton* confirmBtn = new QPushButton(_("rename.confirm"), &dialog);
    QPushButton* cancelBtn = new QPushButton(_("buttons.cancel"), &dialog);
    btnLayout->addWidget(confirmBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    QObject::connect(confirmBtn, SIGNAL(clicked()), &dialog, SLOT(accept()));
    QObject::connect(cancelBtn, SIGNAL(clicked()), &dialog, SLOT(reject()));

    if (dialog.exec() == QDialog::Accepted) {
        std::string name;
        if (selfRadio->isChecked()) {
            name = globalName;
        } else if (customRadio->isChecked()) {
            name = qToUtf8(nameEdit->text()).data();
            if (name.empty()) {
                QMessageBox::warning(this, _("rename.failed"), _("rename.name_empty"));
                return;
            }
        } else if (randomRadio->isChecked()) {
            name = qToUtf8(randomLabel->text()).data();
            if (name.empty()) {
                QMessageBox::warning(this, _("rename.failed"), _("rename.name_empty"));
                return;
            }
        }
        if (!name.empty()) {
            ToxAPI::setGroupSelfNameSync(groupId, name);
        }
    }
}

void MainWindow::onSetGroupTopicRequested(int groupId) {
    auto groups = ToxAPI::getGroupsSync();
    std::string currentTopic;
    for (const auto& g : groups) {
        if (g.groupNumber == groupId) {
            currentTopic = g.statusText;
            break;
        }
    }

    bool ok = false;
    QString topic;
#ifdef QT3_BUILD
    topic = QInputDialog::getText(_("set_topic.title"), _("set_topic.prompt"),
                                  QLineEdit::Normal, qFromUtf8(currentTopic.c_str()), &ok, this);
#else
    topic = QInputDialog::getText(this, _("set_topic.title"), _("set_topic.prompt"),
                                  QLineEdit::Normal, qFromUtf8(currentTopic.c_str()), &ok);
#endif
    if (ok && !topic.isEmpty()) {
        if (ToxAPI::setGroupTopicSync(groupId, qToUtf8(topic).data())) {
            chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
            ToxAPI::loadAllData();
        } else {
            QMessageBox::warning(this, _("error"), _("set_topic.failed"));
        }
    }
}

void MainWindow::onSetConferenceTitleRequested(int conferenceId) {
    auto conferences = ToxAPI::getConferencesSync();
    std::string currentTitle;
    for (const auto& c : conferences) {
        if (c.conferenceNumber == conferenceId) {
            currentTitle = c.statusText;
            break;
        }
    }

    bool ok = false;
    QString title;
#ifdef QT3_BUILD
    title = QInputDialog::getText(_("set_title.title"), _("set_title.prompt"),
                                  QLineEdit::Normal, qFromUtf8(currentTitle.c_str()), &ok, this);
#else
    title = QInputDialog::getText(this, _("set_title.title"), _("set_title.prompt"),
                                  QLineEdit::Normal, qFromUtf8(currentTitle.c_str()), &ok);
#endif
    if (ok && !title.isEmpty()) {
        if (ToxAPI::setConferenceTitleSync(conferenceId, qToUtf8(title).data())) {
            chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
            ToxAPI::loadAllData();
        } else {
            QMessageBox::warning(this, _("error"), _("set_title.failed"));
        }
    }
}

void MainWindow::onDeleteOrLeaveRequested(int id, const QString& type) {
    QString confirmMsg;
    if (type == "friend") {
        confirmMsg = _("confirm_delete_friend").arg(QString::number(id));
    } else if (type == "conference") {
        confirmMsg = _("confirm_leave_conference").arg(QString::number(id));
    } else if (type == "group") {
        confirmMsg = _("confirm_leave_group").arg(QString::number(id));
    } else {
        return;
    }
    
#ifdef QT3_BUILD
    int result = QMessageBox::question(this, _("confirm"), confirmMsg,
                                       QMessageBox::Yes, QMessageBox::No);
    if (result == QMessageBox::Yes) {
#else
    if (QMessageBox::question(this, _("confirm"), confirmMsg,
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
#endif
        bool success = false;
        
        if (type == "friend") {
            success = ToxAPI::deleteFriendSync(id);
            if (success) {
                QMessageBox::information(this, _("friend_deleted"), _("friend_deleted"));
            } else {
                QMessageBox::warning(this, _("error"), _("delete_failed"));
            }
        } else if (type == "conference") {
            success = ToxAPI::leaveConferenceSync(id);
            if (success) {
                QMessageBox::information(this, _("conference_leave_success"), _("conference_leave_success"));
            }
        } else if (type == "group") {
            success = ToxAPI::leaveGroupSync(id);
            if (success) {
                QMessageBox::information(this, _("group_leave_success"), _("group_leave_success"));
            }
        }
        
        if (success) {
            // 如果是当前聊天对象，清空聊天区
            if (id == currentChatId && type == currentChatType) {
                currentChatId = -1;
                currentChatType = "";
                chatWidget->setHeaderText(_("select_chat_object"));
                chatWidget->clearMessages();
            }
            // 重新加载联系人列表
            chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
            ToxAPI::loadAllData();
        }
    }
}

void MainWindow::onInviteToConferenceRequested(int friendId) {
    // 获取会议列表
    std::vector<ConferenceInfo> conferences = ToxAPI::getConferencesSync();
    
    if (conferences.empty()) {
        QMessageBox::warning(this, _("no_conference"), _("no_conference"));
        return;
    }
    
    // 创建选择对话框
    QDialog dialog(this);
    qSetWindowTitle(&dialog, _("select_conference"));
    dialog.resize(300, 150);
    
    QBoxLayout* layout = qNewBoxLayout(&dialog, QBoxLayout::TopToBottom, 10, 10);
    
    QLabel* label = new QLabel(_("select_conference"), &dialog);
    layout->addWidget(label);
    
    QComboBox* confCombo = new QComboBox(&dialog);
    // Qt3: 用单独的数组存储ID
    std::vector<int> confIds;
    for (uint i = 0; i < conferences.size(); ++i) {
        const auto& conf = conferences[i];
        QString displayName;
        if (!conf.conferenceName.empty()) {
            displayName = qFromUtf8(conf.conferenceName.c_str());
        } else {
            displayName = QString(_("conference_item")) + " " + QString::number(conf.conferenceNumber);
        }
#ifdef QT3_BUILD
        confCombo->insertItem(displayName);
        confIds.push_back(conf.conferenceNumber);
#else
        confCombo->addItem(displayName, QVariant(conf.conferenceNumber));
#endif
    }
    layout->addWidget(confCombo);
    
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    
    QPushButton* inviteBtn = new QPushButton(_("buttons.add"), &dialog);
    connect(inviteBtn, SIGNAL(clicked()), &dialog, SLOT(accept()));
    btnLayout->addWidget(inviteBtn);
    
    QPushButton* cancelBtn = new QPushButton(_("buttons.cancel"), &dialog);
    connect(cancelBtn, SIGNAL(clicked()), &dialog, SLOT(reject()));
    btnLayout->addWidget(cancelBtn);
    
    layout->addLayout(btnLayout);
    
    if (dialog.exec() == QDialog::Accepted) {
        int confId = -1;
#ifdef QT3_BUILD
        // Qt3: 从单独的ID数组获取
        int idx = confCombo->currentItem();
        if (idx >= 0 && idx < (int)confIds.size()) {
            confId = confIds[idx];
        }
#else
        // Qt4: 从 QVariant 获取存储的ID
        QVariant data = confCombo->itemData(confCombo->currentIndex());
        confId = data.toInt();
#endif
        if (confId == -1) return;
        bool success = ToxAPI::inviteToConferenceSync(friendId, confId);
        if (success) {
            QMessageBox::information(this, _("invite_success"), _("invite_success"));
        } else {
            QMessageBox::warning(this, _("invite_failed"), _("invite_failed"));
        }
    }
}

void MainWindow::onInviteToGroupRequested(int friendId) {
    // 获取群组列表
    std::vector<GroupInfo> groups = ToxAPI::getGroupsSync();
    
    if (groups.empty()) {
        QMessageBox::warning(this, _("no_group"), _("no_group"));
        return;
    }
    
    // 创建选择对话框
    QDialog dialog(this);
    qSetWindowTitle(&dialog, _("select_group"));
    dialog.resize(300, 150);
    
    QBoxLayout* layout = qNewBoxLayout(&dialog, QBoxLayout::TopToBottom, 10, 10);
    
    QLabel* label = new QLabel(_("select_group"), &dialog);
    layout->addWidget(label);
    
    QComboBox* groupCombo = new QComboBox(&dialog);
    // Qt3: 用单独的数组存储ID
    std::vector<int> groupIds;
    for (uint i = 0; i < groups.size(); ++i) {
        const auto& grp = groups[i];
        QString displayName;
        if (!grp.groupName.empty()) {
            displayName = qFromUtf8(grp.groupName.c_str());
        } else {
            displayName = QString(_("group_item")) + " " + QString::number(grp.groupNumber);
        }
#ifdef QT3_BUILD
        groupCombo->insertItem(displayName);
        groupIds.push_back(grp.groupNumber);
#else
        groupCombo->addItem(displayName, QVariant(grp.groupNumber));
#endif
    }
    layout->addWidget(groupCombo);
    
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    
    QPushButton* inviteBtn = new QPushButton(_("buttons.add"), &dialog);
    connect(inviteBtn, SIGNAL(clicked()), &dialog, SLOT(accept()));
    btnLayout->addWidget(inviteBtn);
    
    QPushButton* cancelBtn = new QPushButton(_("buttons.cancel"), &dialog);
    connect(cancelBtn, SIGNAL(clicked()), &dialog, SLOT(reject()));
    btnLayout->addWidget(cancelBtn);
    
    layout->addLayout(btnLayout);
    
    if (dialog.exec() == QDialog::Accepted) {
        int groupId = -1;
#ifdef QT3_BUILD
        // Qt3: 从单独的ID数组获取
        int idx = groupCombo->currentItem();
        if (idx >= 0 && idx < (int)groupIds.size()) {
            groupId = groupIds[idx];
        }
#else
        // Qt4: 从 QVariant 获取存储的ID
        QVariant data = groupCombo->itemData(groupCombo->currentIndex());
        groupId = data.toInt();
#endif
        bool success = ToxAPI::inviteToGroupSync(friendId, groupId);
        if (success) {
            QMessageBox::information(this, _("invite_success"), _("invite_success"));
        } else {
            QMessageBox::warning(this, _("invite_failed"), _("invite_failed"));
        }
    }
}

void MainWindow::onGroupInviteReceived(int friendNumber, const QString& chatId) {
    GroupInviteDialog dialog(QString::number(friendNumber), chatId, this);
    if (dialog.exec() == QDialog::Accepted) {
        if (dialog.getResult() == GroupInviteDialog::Accept) {
            bool success = ToxAPI::joinGroupSync(friendNumber, qToUtf8(chatId).data(),
                                                  "", qToUtf8(dialog.getPassword()).data());
            if (success) {
                QMessageBox::information(this, _("group.joined"),
                                        _A("group.joined", QStringList() << chatId));
                chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
                ToxAPI::loadAllData();
            } else {
                QMessageBox::warning(this, _("group.join_failed"), _("group.join_failed"));
            }
        } else if (dialog.getResult() == GroupInviteDialog::Reject) {
            QMessageBox::information(this, _("group_rejected"), _("group_rejected"));
        }
    }
}

void MainWindow::onSwitchAccount() {
    ToxAPI::stopPollEvent();

    LoginDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        ToxAPI::startPollEvent();
        chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
        ToxAPI::loadAllData();
        return;
    }

    ToxAPI::setBaseUrl(dialog.selectedUrl());
    ToxAPI::resetLastEventId();

    // 清空 UI 状态
    currentChatId = -1;
    currentChatType = "";
    selfPubkey.clear();
    peerInfoMap.clear();
    contactListWidget->clear();
    chatWidget->clearMessages();
    chatWidget->setHeaderText("");

    // 重新加载数据
    ToxAPI::startPollEvent();
    chatWidget->loadingBar()->showLoading(kLoadAll, _("loading_data"));
    ToxAPI::loadAllData();
}

void MainWindow::renderHistoryMessages(const std::vector<HistoryMessage>& messages) {
    if (currentChatId == -1 || currentChatType.isEmpty()) return;
    
    for (const auto& msg : messages) {
        bool isSelf = (msg.sender_pubkey == selfPubkey);
        QString senderLabel;
        QString avatarText;
        QString ipAddress;
        
        if (isSelf) {
            senderLabel = "Me";
            avatarText = "M";
        } else {
            if (currentChatType == "friend") {
                std::string key = "friend_" + std::to_string(currentChatId);
                auto it = peerInfoMap.find(key);
                if (it != peerInfoMap.end() && !it->second.name.empty()) {
                    senderLabel = qFromUtf8(it->second.name.c_str());
                    avatarText = qToUpper(senderLabel.left(1));
                } else {
                    senderLabel = QString();
                    avatarText = "F";
                }
            } else {
                std::string key = std::string(qToUtf8(currentChatType).data())
                    + "_" + std::to_string(currentChatId)
                    + "_" + std::to_string(msg.sender_number);
                auto it = peerInfoMap.find(key);
                if (it != peerInfoMap.end()) {
                    if (!it->second.name.empty()) {
                        senderLabel = qFromUtf8(it->second.name.c_str());
                        avatarText = qToUpper(senderLabel.left(1));
                    }
                    ipAddress = qFromUtf8(it->second.peerIp.c_str());
                } else {
                    senderLabel = QString();
                    avatarText = "P";
                }
            }
        }
        
        QString timeStr = qFormatTime(qFromUtf8(msg.created_at.c_str()));
        
        chatWidget->appendMessage(
            qFromUtf8(msg.message.c_str()),
            isSelf ? "self" : "other",
            senderLabel,
            isSelf ? -1 : (currentChatType == "friend" ? currentChatId : (int)msg.sender_number),
            timeStr,
            avatarText,
            "",
            ipAddress
        );
    }
}

void MainWindow::loadMessageHistory() {
    if (currentChatId == -1 || currentChatType.isEmpty()) {
        qWarning("loadMessageHistory: no contact selected");
        return;
    }
    
    ToxAPI::getMessagesHistory(currentChatId, std::string(qToUtf8(currentChatType).data()));
}

void MainWindow::onTranslateRequested(int msgIndex, const QString& text, const QString& targetLang) {
    ToxAPI::translate(std::string(qToUtf8(text)), std::string(qToUtf8(targetLang)), msgIndex);
}

void MainWindow::onMenu1Stub() {
    qWarning("onMenu1Stub: not implemented yet");
}

void MainWindow::onMenu2Stub() {
    qWarning("onMenu2Stub: not implemented yet");
}

void MainWindow::openHomePage() {
    qOpenUrl("https://github.com/envsh/toxhttpd");
}
