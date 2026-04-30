#include "mainwindow.h"
#include <qlayout.h>
#include "selfinfo.h"
#include "contactlist.h"
#include "chatwidget.h"
#include "eventpoller.h"
#include "api.h"
#include "translator.h"
#include "invitedialog.h"
#include <qfile.h>
#include <qtextstream.h>
#include <qtextcodec.h>
#include <qtimer.h>
#include "cJSON.h"
#include <stdlib.h>
#include <qmessagebox.h>

// 读取保存的语言设置
static QString loadSavedLanguage() {
    QString home = getenv("HOME") ? getenv("HOME") : ".";
    QFile file(home + "/.q3tox_lang");
    if (file.exists() && file.open(IO_ReadOnly)) {
        QTextStream stream(&file);
        QString lang = stream.readLine().stripWhiteSpace();
        file.close();
        if (!lang.isEmpty()) return lang;
    }
    return "zh-CN"; // 默认简体
}

// 保存语言设置
static void saveLanguage(const QString& lang) {
    QString home = getenv("HOME") ? getenv("HOME") : ".";
    QFile file(home + "/.q3tox_lang");
    if (file.open(IO_WriteOnly)) {
        QTextStream stream(&file);
        stream << lang << "\n";
        file.close();
    }
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), 
    currentChatId(-1), currentChatType("") {
    qWarning("MainWindow: constructor started");
    
    // 设置窗口
    setCaption(_("app_title"));
    setGeometry(100, 100, 1100, 700);
    
    // 设置 UTF-8 编解码器
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    
    // 主分割器（左右布局）
    splitter = new QSplitter(Qt::Horizontal, this);
    
    // ===== 左侧边栏 =====
    QWidget* sidebar = new QWidget(splitter);
    QBoxLayout* sidebarLayout = new QBoxLayout(sidebar, QBoxLayout::TopToBottom, 0, -1, 0);
    sidebarLayout->setSpacing(0);
    sidebarLayout->setMargin(0);
    
    // 个人信息区
    selfInfoWidget = new SelfInfoWidget(sidebar);
    sidebarLayout->addWidget(selfInfoWidget);
    
    // 联系人列表
    contactListWidget = new ContactListWidget(sidebar);
    sidebarLayout->addWidget(contactListWidget, 1); // stretch
    
    splitter->addWidget(sidebar);
    
    // ===== 右侧聊天区 =====
    chatWidget = new ChatWidget(splitter);
    splitter->addWidget(chatWidget);
    
    // 设置分割器比例
    splitter->setResizeMode(sidebar, QSplitter::KeepSize);
    
    setCentralWidget(splitter);
    
    // 连接信号槽
    connect(contactListWidget, SIGNAL(contactSelected(int, const QString&)), 
            this, SLOT(onContactSelected(int, const QString&)));
    connect(chatWidget, SIGNAL(messageSent(const QString&)), this, SLOT(onMessageSent(const QString&)));
    connect(chatWidget, SIGNAL(languageChanged(const QString&)), 
            this, SLOT(onLanguageChanged(const QString&)));
    connect(&Translator::instance(), SIGNAL(languageChanged()), this, SLOT(updateUIStrings()));
    
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

void MainWindow::customEvent(QCustomEvent* event) {
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
            QPtrList<Contact> contacts;
            for (const auto& cd : evt->contacts) {
                Contact* c = new Contact();
                c->id = cd.id;
                c->name = QString::fromUtf8(cd.name.c_str());
                c->type = QString::fromUtf8(cd.type.c_str());
                c->status = QString::fromUtf8(cd.status.c_str());
                contacts.append(c);
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
    
    qWarning("onContactSelected: id=%d, type=%s", id, type.utf8().data());
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
    req->message = std::string(message.utf8());
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
                    
                    InviteDialog dialog(friendNumber, cookie, this);
                    dialog.exec();
                    
                    if (dialog.getResult() == InviteDialog::Accept) {
                        // ✅ 改为异步请求
                        ApiRequestEvent* req = new ApiRequestEvent(ApiJoinConference);
                        req->id = friendNumber.toInt();
                        req->message = std::string(cookie.utf8());
                        eventPoller->postApiRequest(req);
                        
                        QMessageBox::information(this, _("conference_joined"), 
                                                        _("conference_joined").arg(dialog.getCookie()));
                    } else if (dialog.getResult() == InviteDialog::Reject) {
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
                             confNumber, currentChatId, currentChatType.utf8().data(),
                             (confNumber == currentChatId && currentChatType == "conference"));
                    
                    if (confNumber == currentChatId && currentChatType == "conference") {
                        QString sender = (peerNumber >= 0) ? 
                            QString("Peer %1").arg(peerNumber) : _("conference_item");
                        qWarning("Appending conference message: %s", message.utf8().data());
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

void MainWindow::updateUIStrings() {
    setCaption(_("app_title"));
    
    // 更新子控件
    if (selfInfoWidget) selfInfoWidget->updateUIStrings();
    if (contactListWidget) contactListWidget->updateUIStrings();
    
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
