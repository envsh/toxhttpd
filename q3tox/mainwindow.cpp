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
#include <cJSON.h>
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

// 静态回调函数
void MainWindow::onEventsReceivedStatic(EventList events, void* userData) {
    MainWindow* self = static_cast<MainWindow*>(userData);
    if (self) {
        self->handleEvents(events);
    }
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), 
    currentChatId(-1), currentChatType("") {
    // 设置窗口
    setCaption(tr("app_title"));
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
    eventPoller->setCallback(MainWindow::onEventsReceivedStatic, this);
    eventPoller->start();
    
    // 初始加载数据
    loadSelfInfo();
    loadContacts();
}

MainWindow::~MainWindow() {
    if (eventPoller) {
        eventPoller->stop();
        delete eventPoller;
    }
}

void MainWindow::loadSelfInfo() {
    ToxAPI api;
    std::string name, statusMsg, connStatus, address;
    if (api.getSelf(name, statusMsg, connStatus, address)) {
        selfInfoWidget->updateInfo(QString::fromUtf8(name.c_str()), 
                                  QString::fromUtf8(statusMsg.c_str()),
                                  QString::fromUtf8(connStatus.c_str()),
                                  QString::fromUtf8(address.c_str()));
    }
}

void MainWindow::loadContacts() {
    ToxAPI api;
    QPtrList<Contact> contacts;
    
    // 加载好友
    std::vector<int> friends = api.getFriends();
    for (int id : friends) {
        FriendInfo info;
        if (api.getFriendInfo(id, info)) {
            Contact* c = new Contact;
            c->id = id;
            c->name = QString::fromUtf8(info.name.c_str());
            c->type = "friend";
            c->status = QString::fromUtf8(info.connection_status.c_str());
            contacts.append(c);
        }
    }
    
    // 加载会议
    std::vector<int> conferences = api.getConferences();
    for (int id : conferences) {
        Contact* c = new Contact;
        c->id = id;
        c->name = tr("conference_item") + " " + QString::number(id);
        c->type = "conference";
        c->status = "online"; // 会议默认为在线
        contacts.append(c);
    }
    
    contactListWidget->setContacts(contacts);
}

void MainWindow::onContactSelected(int id, const QString& type) {
    currentChatId = id;
    currentChatType = type;
    
    QString headerText;
    if (type == "friend") {
        headerText = tr("chat_with_friend").arg(QString::number(id));
    } else if (type == "group") {
        headerText = tr("group") + " " + QString::number(id);
    } else if (type == "conference") {
        headerText = tr("conference_item") + " " + QString::number(id);
    }
    
    chatWidget->setHeaderText(headerText);
    chatWidget->clearMessages();
}

void MainWindow::onMessageSent(const QString& message) {
    if (currentChatId == -1 || currentChatType.isEmpty()) {
        QMessageBox::warning(this, tr("select_chat_first"), tr("select_chat_first"));
        return;
    }
    
    ToxAPI api;
    bool success = false;
    
    if (currentChatType == "friend") {
        success = api.sendFriendMessage(currentChatId, std::string(message.utf8()));
    } else if (currentChatType == "conference") {
        // TODO: 实现发送会议消息
    }
    
    if (success) {
        chatWidget->appendMessage(message, "self");
    } else {
        QMessageBox::warning(this, tr("send_failed"), tr("send_failed"));
    }
}

void MainWindow::handleEvents(const QArray<Event>& events) {
    for (int i = 0; i < (int)events.size(); ++i) {
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
                                                 tr("friend_label").arg(QString::number(friendId)));
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
                    
                    ToxAPI api;
                    if (dialog.getResult() == InviteDialog::Accept) {
                        api.joinConference(friendNumber.toInt(), std::string(cookie.utf8()));
                        QMessageBox::information(this, tr("conference_joined"), 
                                                tr("conference_joined").arg(dialog.getCookie()));
                        loadContacts();
                    } else if (dialog.getResult() == InviteDialog::Reject) {
                        api.rejectConference(friendNumber.toInt());
                    }
                }
                cJSON_Delete(root);
            }
        } else if (e.type == "self_connection_status") {
            loadSelfInfo();
        } else if (e.type == "friend_name" || e.type == "friend_status") {
            loadContacts();
        }
    }
}

void MainWindow::onLanguageChanged(const QString& langCode) {
    saveLanguage(langCode);
    Translator::instance().loadLanguage(langCode);
}

void MainWindow::updateUIStrings() {
    setCaption(tr("app_title"));
    
    if (currentChatId == -1) {
        chatWidget->setHeaderText(tr("select_chat_object"));
    } else {
        QString headerText;
        if (currentChatType == "friend") {
            headerText = tr("chat_with_friend").arg(QString::number(currentChatId));
        } else if (currentChatType == "group") {
            headerText = tr("group") + " " + QString::number(currentChatId);
        } else if (currentChatType == "conference") {
            headerText = tr("conference_item") + " " + QString::number(currentChatId);
        }
        chatWidget->setHeaderText(headerText);
    }
    
    loadContacts();
}
