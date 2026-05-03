#include "mainwindow.h"
#include "compat34.h"
#include "selfinfo.h"
#include "contactlist.h"
#include "chatwidget.h"
#include "eventpoller.h"
#include "api.h"
#include "translator.h"
#include "conferenceinvitedialog.h"
#include "groupinvitedialog.h"
#include "cJSON.h"

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
    connect(contactListWidget, SIGNAL(contactSelected(int, const QString&)), 
            this, SLOT(onContactSelected(int, const QString&)));
    connect(contactListWidget, SIGNAL(viewInfoRequested(int, const QString&)),
            this, SLOT(onViewInfoRequested(int, const QString&)));
    connect(contactListWidget, SIGNAL(deleteOrLeaveRequested(int, const QString&)),
            this, SLOT(onDeleteOrLeaveRequested(int, const QString&)));
    connect(contactListWidget, SIGNAL(inviteToConferenceRequested(int)),
            this, SLOT(onInviteToConferenceRequested(int)));
    connect(chatWidget, SIGNAL(messageSent(const QString&)), this, SLOT(onMessageSent(const QString&)));
    connect(chatWidget, SIGNAL(languageChanged(const QString&)), 
            this, SLOT(onLanguageChanged(const QString&)));
    connect(&Translator::instance(), SIGNAL(languageChanged()), this, SLOT(retranslateUi()));
    
    // 事件轮询器
    eventPoller = new EventPoller(this);
    eventPoller->setReceiver(this);
    eventPoller->start();
    
    // ✅ 异步加载：发送请求事件，不阻塞
    qWarning("MainWindow: requesting initial data load (async)");
    ApiRequestEvent* req = new ApiRequestEvent(ApiLoadAllData);
    eventPoller->postApiRequest(req);
}

MainWindow::~MainWindow() {
    if (eventPoller) {
        eventPoller->stop();
        delete eventPoller;
    }
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
            
            // 转换ContactData为Contact并更新列表
            ContactList contacts;
            qWarning("MainWindow: loading %d contacts", (int)evt->contacts.size());
            for (const auto& cd : evt->contacts) {
                Contact* c = new Contact();
                c->id = cd.id;
                c->name = QString::fromUtf8(cd.name.c_str());
                c->type = QString::fromUtf8(cd.type.c_str());
                c->status = QString::fromUtf8(cd.status.c_str());
                contacts.append(c);
                qWarning("  Contact: id=%d, name='%s', type='%s', status='%s'",
                          cd.id, cd.name.c_str(), cd.type.c_str(), cd.status.c_str());
            }
            contactListWidget->setContacts(contacts);
            
            qWarning("MainWindow: initial data load complete");
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
        
        // 会议操作结果
        if (e->type == ApiJoinConference) {
            ConferenceResultEvent* evt = static_cast<ConferenceResultEvent*>(event);
            if (evt->success) {
                // 重新加载联系人
                ApiRequestEvent* req = new ApiRequestEvent(ApiLoadAllData);
                eventPoller->postApiRequest(req);
            }
            return;
        }
    }
}

void MainWindow::onContactSelected(int id, const QString& type) {
    // 如果已经是当前选中的聊天对象，不重新加载（避免清空消息）
    if (id == currentChatId && type == currentChatType) {
        qWarning("onContactSelected: same contact, ignoring");
        return;
    }
    
    qWarning("onContactSelected: id=%d, type=%s", id, qToUtf8(type).data());
    currentChatId = id;
    currentChatType = type;
    
    QString headerText;
    if (type == "friend") {
        headerText = _("chat_with_friend").arg(QString::number(id));
    } else if (type == "group") {
        headerText = _("group") + " " + QString::number(id);
    } else if (type == "conference") {
        headerText = _("conference_item") + " " + QString::number(id);
    }
    
    chatWidget->setHeaderText(headerText);
    chatWidget->clearMessages();
}

void MainWindow::onMessageSent(const QString& message) {
    if (currentChatId == -1 || currentChatType.isEmpty()) {
        QMessageBox::warning(this, _("select_chat_first"), _("select_chat_first"));
        return;
    }
    
    // ✅ 改为异步请求
    ApiRequestEvent* req = new ApiRequestEvent(
        currentChatType == "friend" ? ApiSendFriendMessage : ApiSendConferenceMessage
    );
    req->id = currentChatId;
    req->message = std::string(qToUtf8(message));
    eventPoller->postApiRequest(req);
    
    // 乐观更新：先显示在界面
    chatWidget->appendMessage(message, "self");
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
                        chatWidget->appendMessage(message, "other", 
                                         _("friend_label").arg(QString::number(friendId)));
                    }
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
                        // ✅ 改为异步请求
                        ApiRequestEvent* req = new ApiRequestEvent(ApiJoinConference);
                        req->id = friendNumber.toInt();
                        req->message = std::string(qToUtf8(cookie));
                        eventPoller->postApiRequest(req);
                        
                        QMessageBox::information(this, _("conference_joined"), 
                                                        _("conference_joined").arg(dialog.getCookie()));
                    } else if (dialog.getResult() == ConferenceInviteDialog::Reject) {
                        // ✅ 改为异步请求
                        ApiRequestEvent* req = new ApiRequestEvent(ApiRejectConference);
                        req->id = friendNumber.toInt();
                        eventPoller->postApiRequest(req);
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
                
                if (confNumberItem && messageItem) {
                    int confNumber = confNumberItem->valueint;
                    QString message = QString::fromUtf8(cJSON_GetStringValue(messageItem));
                    int peerNumber = peerNumberItem ? peerNumberItem->valueint : -1;
                    
                    qWarning("confNumber=%d, currentChatId=%d, currentChatType=%s, match=%d", 
                             confNumber, currentChatId, qToUtf8(currentChatType).data(),
                             (confNumber == currentChatId && currentChatType == "conference"));
                    
                    if (confNumber == currentChatId && currentChatType == "conference") {
                        QString sender = (peerNumber >= 0) ? 
                            QString("Peer %1").arg(peerNumber) : _("conference_item");
                        qWarning("Appending conference message: %s", qToUtf8(message).data());
                        chatWidget->appendMessage(message, "other", sender);
                    }
                } else {
                    qWarning("conference_message: missing confNumber or message");
                }
                cJSON_Delete(root);
            } else {
                qWarning("conference_message: failed to parse JSON: %s", e.data.c_str());
            }
        } else if (e.type == "self_connection_status") {
            // ✅ 改为异步请求
            ApiRequestEvent* req = new ApiRequestEvent(ApiLoadAllData);
            eventPoller->postApiRequest(req);
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
                
                if (groupNumberItem && messageItem) {
                    int groupNumber = groupNumberItem->valueint;
                    QString message = QString::fromUtf8(cJSON_GetStringValue(messageItem));
                    int peerNumber = peerNumberItem ? peerNumberItem->valueint : -1;
                    
                    if (groupNumber == currentChatId && currentChatType == "group") {
                        QString sender = (peerNumber >= 0) ? 
                            QString("Peer %1").arg(peerNumber) : _("group_item");
                        chatWidget->appendMessage(message, "other", sender);
                    }
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "friend_name" || e.type == "friend_status") {
            // ✅ 异步重新加载（直接发送请求，避免SLOT问题）
            ApiRequestEvent* req = new ApiRequestEvent(ApiLoadAllData);
            eventPoller->postApiRequest(req);
        }
    }
}

void MainWindow::onLanguageChanged(const QString& langCode) {
    saveLanguage(langCode);
    Translator::instance().loadLanguage(langCode);
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
        ToxAPI api;
        FriendInfo info;
        if (api.getFriendInfo(id, info)) {
            dialog.setInfo(id, QString::fromUtf8(info.name.c_str()), type,
                          QString::fromUtf8(info.status.c_str()),
                          QString::fromUtf8(info.connection_status.c_str()),
                          QString::fromUtf8(info.public_key.c_str()));
        } else {
            dialog.setInfo(id, _("no_name"), type);
        }
    } else if (type == "conference") {
        dialog.setInfo(id, _("conference_item") + " " + QString::number(id), type);
    }
    
    dialog.exec();
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
        ToxAPI api;
        bool success = false;
        
        if (type == "friend") {
            success = api.deleteFriend(id);
            if (success) {
                QMessageBox::information(this, _("friend_deleted"), _("friend_deleted"));
            }
        } else if (type == "conference") {
            success = api.leaveConference(id);
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
            ApiRequestEvent* req = new ApiRequestEvent(ApiLoadAllData);
            eventPoller->postApiRequest(req);
        }
    }
}

void MainWindow::onInviteToConferenceRequested(int friendId) {
    // 获取会议列表
    ToxAPI api;
    std::vector<int> conferences = api.getConferences();
    
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
    for (uint i = 0; i < conferences.size(); ++i) {
#ifdef QT3_BUILD
        confCombo->insertItem(QString(_("conference_item")) + " " + QString::number(conferences[i]));
#else
        confCombo->insertItem(i, QString(_("conference_item")) + " " + QString::number(conferences[i]));
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
#ifdef QT3_BUILD
        int confId = conferences[confCombo->currentItem()];
#else
        int confId = conferences[confCombo->currentIndex()];
#endif
        bool success = api.inviteToConference(friendId, confId);
        if (success) {
            QMessageBox::information(this, _("invite_success"), _("invite_success"));
        } else {
            QMessageBox::warning(this, _("invite_failed"), _("invite_failed"));
        }
    }
}

void MainWindow::onInviteToGroupRequested(int friendId) {
    // 获取群组列表
    ToxAPI api;
    std::vector<int> groups = api.getGroups();
    
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
    for (uint i = 0; i < groups.size(); ++i) {
#ifdef QT3_BUILD
        groupCombo->insertItem(QString(_("group_item")) + " " + QString::number(groups[i]));
#else
        groupCombo->insertItem(i, QString(_("group_item")) + " " + QString::number(groups[i]));
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
#ifdef QT3_BUILD
        int groupId = groups[groupCombo->currentItem()];
#else
        int groupId = groups[groupCombo->currentIndex()];
#endif
        bool success = api.inviteToGroup(friendId, groupId);
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
            // 接受群组邀请
            ToxAPI api;
            bool success = api.joinGroup(friendNumber, qToUtf8(chatId).data(), 
                                         "", qToUtf8(dialog.getPassword()).data());
            if (success) {
                QMessageBox::information(this, _("group_joined"), 
                                        _("group_joined").arg(chatId));
                // 重新加载联系人
                ApiRequestEvent* req = new ApiRequestEvent(ApiLoadAllData);
                eventPoller->postApiRequest(req);
            } else {
                QMessageBox::warning(this, _("group_join_failed"), _("group_join_failed"));
            }
        } else if (dialog.getResult() == GroupInviteDialog::Reject) {
            // 拒绝群组邀请（直接忽略，无后端API）
            QMessageBox::information(this, _("group_rejected"), _("group_rejected"));
        }
    }
}
