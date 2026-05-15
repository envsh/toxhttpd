#include "mainwindow.h"
#include "compat34.h"

// 获取当前时间字符串 (hh:mm:ss)
static QString getCurrentTime() {
    QTime now = QTime::currentTime();
    return now.toString("hh:mm:ss");
}

#include "selfinfo.h"
#include "contactlist.h"
#include "chatwidget.h"
#include "restapi.h"
#include "translator.h"
#include "conferenceinvitedialog.h"
#include "groupinvitedialog.h"
#include "cJSON.h"
#include "appsetup.h"
#include "placeholderlineedit.h"

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

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), 
    currentChatId(-1), currentChatType("") {
    qWarning("MainWindow: constructor started");
    
    // 设置窗口
    qSetWindowTitle(this, _("app_title"));
    setGeometry(100, 100, 1100, 700);
    
    // 设置 UTF-8 编解码器
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    
    // 主分割器（左右布局）
    splitter = new QSplitter(Qt::Horizontal, this);
    
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
    
    setCentralWidget(splitter);
    
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
    
    // 所有数据加载完成
    if (event->type() == ApiResultReadyType) {
        ApiResultEvent* e = static_cast<ApiResultEvent*>(event);
        
        if (e->type == ApiLoadAllData) {
            AllDataLoadedEvent* evt = static_cast<AllDataLoadedEvent*>(event);
            
            // 更新self信息
            selfInfoWidget->updateInfo(
                QString::fromUtf8(evt->selfName.c_str()),
                QString::fromUtf8(evt->selfStatusMsg.c_str()),
                QString::fromUtf8(evt->selfConnStatus.c_str()),
                QString::fromUtf8(evt->selfAddress.c_str()));
            
            // 保存自己的公钥（地址前64字符是公钥）
            std::string addr = evt->selfAddress;
            if (addr.length() >= 64) {
                selfPubkey = addr.substr(0, 64);
            }
            
            // 转换ContactData为Contact并更新列表
            ContactList contacts;
            qWarning("MainWindow: loading %d contacts", (int)evt->contacts.size());
            for (const auto& cd : evt->contacts) {
                Contact* c = new Contact();
                c->id = cd.id;
                c->name = QString::fromUtf8(cd.name.c_str());
                c->type = QString::fromUtf8(cd.type.c_str());
                c->status = QString::fromUtf8(cd.status.c_str());
                c->chat_id = QString::fromUtf8(cd.chatId.c_str());
                c->is_connected = cd.isConnected;
                contacts.append(c);
                qWarning("  Contact: id=%d, name='%s', type='%s', status='%s', chat_id='%s', connected=%s",
                          cd.id, cd.name.c_str(), cd.type.c_str(), cd.status.c_str(), cd.chatId.c_str(), 
                          cd.isConnected ? "true" : "false");
                
                // 写入 peerInfoMap
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
            contactListWidget->setContacts(contacts);
            
            qWarning("MainWindow: initial data load complete");
            if (ToxAPI::onLoadAllDataComplete()) {
                ToxAPI::loadAllData();
            }
            return;
        }
        
        // 消息发送结果
        if (e->type == ApiSendFriendMessage || e->type == ApiSendConferenceMessage) {
            MessageSentResultEvent* evt = static_cast<MessageSentResultEvent*>(event);
            if (!evt->success) {
                QMessageBox::warning(this, _("send_failed"), _("send_failed"));
            }
            return;
        }
        
        // 会议加入结果 → 重新加载联系人
        if (e->type == ApiJoinConference) {
            ToxAPI::loadAllData();
            return;
        }
        
        // 成员列表加载完成
        if (e->type == ApiLoadGroupMembers) {
            MembersLoadedEvent* evt = static_cast<MembersLoadedEvent*>(event);
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
            if (evt->contactId == currentChatId && evt->contactType == std::string(qToUtf8(currentChatType).data())) {
                qWarning("MessageHistoryLoaded: %d messages for %s %d", (int)evt->messages.size(), evt->contactType.c_str(), evt->contactId);
                renderHistoryMessages(evt->messages);
            }
            return;
        }
        
        // 翻译结果
        if (e->type == ApiTranslate) {
            TranslateResultEvent* tev = static_cast<TranslateResultEvent*>(event);
            chatWidget->onTranslateResult(tev->msgIndex, tev->success,
                QString::fromUtf8(tev->translatedText.data(), (int)tev->translatedText.size()),
                QString::fromUtf8(tev->errorMessage.data(), (int)tev->errorMessage.size()));
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
    chatWidget->clearMessages();
    
    // 异步加载历史消息
    ToxAPI::getMessagesHistory(id, std::string(qToUtf8(type).data()));
    
    // 异步预加载成员列表到 peerInfoMap 缓存
    if (type == "group") {
        ToxAPI::getGroupMembers(id);
    } else if (type == "conference") {
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
                    QString message = QString::fromUtf8(cJSON_GetStringValue(messageItem));
                    if (friendId == currentChatId && currentChatType == "friend") {
                        chatWidget->appendMessage(message, "other", QString(),
                                         friendId, getCurrentTime());
                    }
                    playNotifySound();
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
                    QString cookie = QString::fromUtf8(cJSON_GetStringValue(cookieItem));
                    
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
                    QString message = QString::fromUtf8(cJSON_GetStringValue(messageItem));
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
                            senderName = QString::fromUtf8(it->second.name.c_str());
                        qWarning("Appending conference message: %s", qToUtf8(message).data());
                        chatWidget->appendMessage(message, "other", senderName, peerNumber, getCurrentTime());
                    }
                    playNotifySound();
                } else {
                    qWarning("conference_message: missing confNumber or message");
                }
                cJSON_Delete(root);
            } else {
                qWarning("conference_message: failed to parse JSON: %s", e.data.c_str());
            }
        } else if (e.type == "self_connection_status") {
            ToxAPI::loadAllData();
        } else if (e.type == "group_invite") {
            cJSON* root = cJSON_Parse(e.data.c_str());
            if (root) {
                cJSON* friendNumberItem = cJSON_GetObjectItem(root, "friend_number");
                cJSON* chatIdItem = cJSON_GetObjectItem(root, "chat_id");
                if (friendNumberItem && chatIdItem) {
                    int friendNumber = friendNumberItem->valueint;
                    QString chatId = QString::fromUtf8(cJSON_GetStringValue(chatIdItem));
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
                    QString message = QString::fromUtf8(cJSON_GetStringValue(messageItem));
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
                                senderName = QString::fromUtf8(it->second.name.c_str());
                            ipAddress = QString::fromUtf8(it->second.peerIp.c_str());
                        }
                        chatWidget->appendMessage(message, "other", senderName, peerNumber, getCurrentTime(), "", "", ipAddress);
                    }
                    playNotifySound();
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
        } else if (e.type == "friend_name" || e.type == "friend_status") {
            ToxAPI::loadAllData();
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
            dialog.setInfo(id, QString::fromUtf8(info.name.c_str()), type,
                          QString::fromUtf8(info.statusText.c_str()),
                          QString::fromUtf8(info.statusStr.c_str()),
                          false,
                          QString::fromUtf8(info.publicKey.c_str()));
        } else {
            dialog.setInfo(id, _("no_name"), type);
        }
    } else if (type == "conference") {
        dialog.setTitle(_("modals.conference_info_title"));
        std::vector<ConferenceInfo> conferences = ToxAPI::getConferencesSync();
        bool isConnected = false;
        QString chatId = "";
        for (size_t i = 0; i < conferences.size(); ++i) {
            if (conferences[i].conferenceNumber == (uint32_t)id) {
                isConnected = conferences[i].isConnected;
                chatId = QString::fromUtf8(conferences[i].chatId.c_str());
                break;
            }
        }
        dialog.setInfo(id, _("conference_item") + " " + QString::number(id), type,
                       "", "", isConnected, chatId);
    } else if (type == "group") {
        dialog.setTitle(_("modals.group_info_title"));
        std::vector<GroupInfo> groups = ToxAPI::getGroupsSync();
        bool isConnected = false;
        QString chatId = "";
        for (size_t i = 0; i < groups.size(); ++i) {
            if (groups[i].groupNumber == (uint32_t)id) {
                isConnected = groups[i].isConnected;
                chatId = QString::fromUtf8(groups[i].chatId.c_str());
                break;
            }
        }
        dialog.setInfo(id, _("group_item") + " " + QString::number(id), type,
                       "", "", isConnected, chatId);
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
        _("rename.current_nick") + QString(": ") + QString::fromUtf8(displayName.c_str()), &dialog);
    layout->addWidget(currentLabel);

    // Use global nick
    QRadioButton* selfRadio = new QRadioButton(
        _("rename.use_self_nick") + QString(" (%1)").arg(QString::fromUtf8(globalName.c_str())), &dialog);
    selfRadio->setChecked(true);
    layout->addWidget(selfRadio);

    // Custom nick
    QRadioButton* customRadio = new QRadioButton(_("rename.custom_nick"), &dialog);
    layout->addWidget(customRadio);

    PlaceholderLineEdit* nameEdit = new PlaceholderLineEdit(_("rename.enter_nick"), &dialog);
    nameEdit->setText(QString::fromUtf8(currentGroupNick.c_str()));
    nameEdit->setEnabled(false);
    layout->addWidget(nameEdit);

    // Random nick
    QRadioButton* randomRadio = new QRadioButton(_("rename.random_nick"), &dialog);
    layout->addWidget(randomRadio);

    QLabel* randomLabel = new QLabel(
        QString::fromUtf8(ToxAPI::getRandomNameSync().c_str()), &dialog);
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

void MainWindow::onDeleteOrLeaveRequested(int id, const QString& type) {
    QString confirmMsg;
    if (type == "friend") {
        confirmMsg = _("confirm_delete_friend").arg(QString::number(id));
    } else if (type == "conference") {
        confirmMsg = _("confirm_leave_conference").arg(QString::number(id));
    } else {
        return;
    }
    
#ifdef QT3_BUILD
    int result = QMessageBox::question(this, _("confirm"), confirmMsg,
                                       QMessageBox::Yes, QMessageBox::No);
    if (result == 0) {
#else
    if (QMessageBox::question(this, _("confirm"), confirmMsg,
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
#endif
        bool success = false;
        
        if (type == "friend") {
            success = ToxAPI::deleteFriendSync(id);
            if (success) {
                QMessageBox::information(this, _("friend_deleted"), _("friend_deleted"));
            }
        } else if (type == "conference") {
            success = ToxAPI::leaveConferenceSync(id);
            if (success) {
                QMessageBox::information(this, _("conference_leave_success"), _("conference_leave_success"));
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
            displayName = QString::fromUtf8(conf.conferenceName.c_str());
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
            displayName = QString::fromUtf8(grp.groupName.c_str());
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
    QMessageBox::information(this, "", _("not_yet_implemented"));
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
                    senderLabel = QString::fromUtf8(it->second.name.c_str());
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
                        senderLabel = QString::fromUtf8(it->second.name.c_str());
                        avatarText = qToUpper(senderLabel.left(1));
                    }
                    ipAddress = QString::fromUtf8(it->second.peerIp.c_str());
                } else {
                    senderLabel = QString();
                    avatarText = "P";
                }
            }
        }
        
        QString timeStr = qFormatTime(QString::fromUtf8(msg.created_at.c_str()));
        
        chatWidget->appendMessage(
            QString::fromUtf8(msg.message.c_str()),
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
