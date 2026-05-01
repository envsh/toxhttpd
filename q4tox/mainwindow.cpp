#include "mainwindow.h"
#include "selfinfo.h"
#include "contactlist.h"
#include "chatwidget.h"
#include "eventpoller.h"
#include "translator.h"
#include <qmessagebox.h>
#include <qtextcodec.h>
#include <qtextstream.h>
#include <qstatusbar.h>

// 读取保存的语言设置
static QString loadSavedLanguage() {
    QString home = getenv("HOME") ? getenv("HOME") : ".";
    QFile file(home + "/.q4tox_lang");
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QTextStream stream(&file);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        QString lang = stream.readLine().trimmed();
        file.close();
        if (!lang.isEmpty()) return lang;
    }
    return "zh-CN"; // 默认简体
}

// 保存语言设置
static void saveLanguage(const QString& lang) {
    QString home = getenv("HOME") ? getenv("HOME") : ".";
    QFile file(home + "/.q4tox_lang");
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream stream(&file);
        stream << lang << "\n";
        file.close();
    }
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), 
    currentChatId(-1), currentChatType("") {
    
    // 设置窗口
    setWindowTitle(_("app_title"));
    setGeometry(100, 100, 1100, 700);
    
    // 设置 UTF-8 编解码器
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    
    // 主分割器（左右布局）
    splitter = new QSplitter(Qt::Horizontal, this);
    
    // ===== 左侧边栏 =====
    QWidget* sidebar = new QWidget(splitter);
    QBoxLayout* sidebarLayout = new QBoxLayout(QBoxLayout::TopToBottom, sidebar);
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
    
    setCentralWidget(splitter);
    
    // 连接信号槽
    connect(contactListWidget, SIGNAL(contactSelected(int, const QString&)), 
            this, SLOT(onContactSelected(int, const QString&)));
    connect(chatWidget, SIGNAL(messageSent(const QString&)), this, SLOT(onMessageSent(const QString&)));
    connect(chatWidget, SIGNAL(languageChanged(const QString&)), 
            this, SLOT(onLanguageChanged(const QString&)));
    connect(&Translator::instance(), SIGNAL(languageChanged()), this, SLOT(retranslateUi()));
    
    // 事件轮询器
    eventPoller = new EventPoller(this);
    eventPoller->setReceiver(this);
    eventPoller->start();
    
    // 异步加载：发送请求事件，不阻塞
    ApiRequestEvent* req = new ApiRequestEvent(ApiLoadAllData);
    eventPoller->postApiRequest(req);
}

MainWindow::~MainWindow() {
    if (eventPoller) {
        eventPoller->stop();
        delete eventPoller;
    }
}

void MainWindow::customEvent(QEvent* event) {
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
            QList<Contact> contacts;
            for (const auto& cd : evt->contacts) {
                Contact c;
                c.id = cd.id;
                c.name = QString::fromUtf8(cd.name.c_str());
                c.type = QString::fromUtf8(cd.type.c_str());
                c.status = QString::fromUtf8(cd.status.c_str());
                contacts.append(c);
            }
            contactListWidget->setContacts(contacts);
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
    // 如果已经是当前选中的聊天对象，不重新加载
    if (id == currentChatId && type == currentChatType) {
        return;
    }
    
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
    
    // 改为异步请求
    ApiRequestEvent* req = new ApiRequestEvent(
        currentChatType == "friend" ? ApiSendFriendMessage : ApiSendConferenceMessage
    );
    req->id = currentChatId;
    req->message = std::string(message.toUtf8().constData());
    eventPoller->postApiRequest(req);
    
    // 乐观更新：先显示在界面
    chatWidget->appendMessage(message, "self");
}

void MainWindow::handleEvents(const EventList& events) {
    for (const auto& e : events) {
        QString type = QString::fromUtf8(e.type.c_str());
        
        if (type == "friend_message") {
            // 解析 friend_id 和 message
            // 这里简化处理，实际需要解析 JSON
            if (currentChatId >= 0 && currentChatType == "friend") {
                // 暂时显示
                chatWidget->appendMessage("Friend message", "other");
            }
        } else if (type == "conference_invite") {
            // 显示邀请对话框
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, _("conference.invite_message"), 
                                          _("conference.invitation_from").arg("..."),
                                          QMessageBox::Yes | QMessageBox::No | QMessageBox::Ignore);
            
            if (reply == QMessageBox::Yes) {
                ApiRequestEvent* req = new ApiRequestEvent(ApiJoinConference);
                eventPoller->postApiRequest(req);
            } else if (reply == QMessageBox::No) {
                ApiRequestEvent* req = new ApiRequestEvent(ApiRejectConference);
                eventPoller->postApiRequest(req);
            }
        }
    }
}

void MainWindow::onLanguageChanged(const QString& langCode) {
    saveLanguage(langCode);
    Translator::instance().loadLanguage(langCode);
}

void MainWindow::retranslateUi() {
    setWindowTitle(_("app_title"));
    
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

void MainWindow::onContactContextMenu(int id, const QString& type, const QPoint& pos) {
    if (type != "friend") return;
    
    currentChatId = id;
    currentChatType = type;
    
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, _("confirm"), 
                                  _("confirm_delete_friend").arg(QString::number(id)),
                                  QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        onDeleteFriend();
    }
}

void MainWindow::onDeleteFriend() {
    if (currentChatId == -1 || currentChatType != "friend") return;
    
    ApiRequestEvent* req = new ApiRequestEvent(ApiDeleteFriend);
    req->id = currentChatId;
    eventPoller->postApiRequest(req);
    
    statusBar()->showMessage(_("deleting_friend"), 3000);
}

void MainWindow::onEditInfoRequested(const QString& name, const QString& statusMessage) {
    // TODO: 通过 eventpoller 发送请求
}

void MainWindow::onBootstrapRequested() {
    // TODO: 通过 eventpoller 发送请求
    statusBar()->showMessage(_("connecting"), 3000);
}

void MainWindow::onAddFriendRequested(const QString& publicKey) {
    if (publicKey.length() != 64 && publicKey.length() != 76) {
        QMessageBox::warning(this, _("error"), _("invalid_public_key"));
        return;
    }
    
    ApiRequestEvent* req = new ApiRequestEvent(ApiAddFriend);
    req->publicKey = std::string(publicKey.toUtf8().constData());
    eventPoller->postApiRequest(req);
    
    statusBar()->showMessage(_("adding_friend"), 3000);
}

void MainWindow::onCreateConferenceRequested() {
    ApiRequestEvent* req = new ApiRequestEvent(ApiCreateConference);
    eventPoller->postApiRequest(req);
    
    statusBar()->showMessage(_("creating_conference"), 3000);
}

void MainWindow::onCreateGroupRequested() {
    ApiRequestEvent* req = new ApiRequestEvent(ApiCreateGroup);
    eventPoller->postApiRequest(req);
    
    statusBar()->showMessage(_("creating_group"), 3000);
}
