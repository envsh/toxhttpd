#include "mainwindow.h"
#include "selfinfo.h"
#include "contactlist.h"
#include "chatwidget.h"
#include "translator.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QFile>
#include <QTextStream>
#include <QTextCodec>
#include <QDir>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent) 
    : QMainWindow(parent), currentChatId(-1) {
    
    // Set window properties
    setWindowTitle(_("app_title"));
    setMinimumSize(800, 600);
    
    // Create central widget and layout
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QHBoxLayout* mainLayout = new QHBoxLayout(central);
    
    // Create splitter
    splitter = new QSplitter(Qt::Horizontal, central);
    mainLayout->addWidget(splitter);
    
    // Create left sidebar
    QWidget* sidebar = new QWidget(splitter);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    
    selfInfoWidget = new SelfInfoWidget(sidebar);
    contactListWidget = new ContactListWidget(sidebar);
    
    sidebarLayout->addWidget(selfInfoWidget);
    sidebarLayout->addWidget(contactListWidget, 1); // stretch
    
    // Create right chat area
    chatWidget = new ChatWidget(splitter);
    
    splitter->addWidget(sidebar);
    splitter->addWidget(chatWidget);
    splitter->setStretchFactor(0, 0); // sidebar fixed
    splitter->setStretchFactor(1, 1); // chat area stretch
    
    // Create API and event poller
    api = new ToxAPI(this);
    eventPoller = new EventPoller(this);
    eventPoller->setReceiver(this);
    
    // Connect signals/slots (Qt4 style)
    connect(contactListWidget, SIGNAL(contactSelected(int, QString)), 
            this, SLOT(onContactSelected(int, QString)));
    connect(contactListWidget, SIGNAL(addFriendRequested(QString)), 
            this, SLOT(onAddFriendRequested(QString)));
    connect(contactListWidget, SIGNAL(createConferenceRequested()), 
            this, SLOT(onCreateConferenceRequested()));
    connect(contactListWidget, SIGNAL(createGroupRequested()), 
            this, SLOT(onCreateGroupRequested()));
    connect(chatWidget, SIGNAL(messageSent(QString)), 
            this, SLOT(onMessageSent(QString)));
    connect(chatWidget, SIGNAL(languageChanged(QString)), 
            this, SLOT(onLanguageChanged(QString)));
    connect(selfInfoWidget, SIGNAL(editInfoRequested(QString, QString)), 
            this, SLOT(onEditInfoRequested(QString, QString)));
    connect(selfInfoWidget, SIGNAL(bootstrapRequested()), 
            this, SLOT(onBootstrapRequested()));
    
    // API signals
    connect(api, SIGNAL(selfLoaded(QVariantMap)), 
            this, SLOT(onSelfLoaded(QVariantMap)));
    connect(api, SIGNAL(friendsLoaded(QList<int>)), 
            this, SLOT(onFriendsLoaded(QList<int>)));
    connect(api, SIGNAL(conferencesLoaded(QList<int>)), 
            this, SLOT(onConferencesLoaded(QList<int>)));
    connect(api, SIGNAL(eventsReceived(EventList)), 
            this, SLOT(onEventsReceived(EventList)));
    connect(api, SIGNAL(messageReceived(int, QString)), 
            this, SLOT(onMessageReceived(int, QString)));
    connect(api, SIGNAL(conferenceMessageReceived(int, int, QString)), 
            this, SLOT(onConferenceMessageReceived(int, int, QString)));
    connect(api, SIGNAL(conferenceInvited(int, QString)), 
            this, SLOT(onConferenceInvited(int, QString)));
    connect(api, SIGNAL(errorOccurred(QString)), 
            this, SLOT(onErrorOccurred(QString)));
    
    // Translator
    connect(&Translator::instance(), SIGNAL(languageChanged()), 
            this, SLOT(retranslateUi()));
    
    // Start event poller
    eventPoller->start();
    
    // Load initial data
    ApiRequest req;
    req.type = ApiLoadAllData;
    eventPoller->postApiRequest(req);
}

MainWindow::~MainWindow() {
    eventPoller->stop();
    eventPoller->wait();
}

void MainWindow::customEvent(QEvent* event) {
    if (event->type() == EventListReadyType) {
        EventListEvent* e = static_cast<EventListEvent*>(event);
        handleEvents(e->events);
    }
}

void MainWindow::onContactSelected(int id, const QString& type) {
    currentChatId = id;
    currentChatType = type;
    chatWidget->setChatInfo(id, type);
}

void MainWindow::onMessageSent(const QString& message) {
    if (currentChatId == -1) return;
    
    ApiRequest req;
    req.id = currentChatId;
    req.message = message;
    
    if (currentChatType == "friend") {
        req.type = ApiSendFriendMessage;
        chatWidget->appendMessage(message, true); // self
    } else if (currentChatType == "conference") {
        req.type = ApiSendConferenceMessage;
        chatWidget->appendMessage(message, true);
    }
    
    eventPoller->postApiRequest(req);
}

void MainWindow::onLanguageChanged(const QString& langCode) {
    saveLanguage(langCode);
    Translator::instance().loadLanguage(langCode);
}

void MainWindow::retranslateUi() {
    setWindowTitle(_("app_title"));
    // TODO: retranslate other widgets
}

void MainWindow::onSelfLoaded(const QVariantMap& data) {
    selfData = data;
    selfInfoWidget->updateInfo(data);
}

void MainWindow::onFriendInfoLoaded(const FriendInfo& info) {
    Contact contact;
    contact.id = info.id;
    contact.name = info.name;
    contact.type = "friend";
    contact.status = info.connectionStatus;
    contactListWidget->addContact(contact);
}

void MainWindow::onFriendsLoaded(const QList<int>& friendIds) {
    // Load each friend's info
    foreach (int id, friendIds) {
        loadContactInfo(id, "friend");
    }
}

void MainWindow::onConferencesLoaded(const QList<int>& conferenceIds) {
    foreach (int id, conferenceIds) {
        loadContactInfo(id, "conference");
    }
}

void MainWindow::onEventsReceived(const EventList& events) {
    handleEvents(events);
}

void MainWindow::handleEvents(const EventList& events) {
    foreach (const Event& e, events) {
        if (e.type == "friend_message") {
            QVariantMap data = api->parseJsonString(e.data);
            int friendId = data["friend_id"].toInt();
            QString message = data["message"].toString();
            
            if (currentChatId == friendId && currentChatType == "friend") {
                chatWidget->appendMessage(message, false);
            }
        } else if (e.type == "conference_invite") {
            QVariantMap data = api->parseJsonString(e.data);
            int friendNumber = data["friend_number"].toInt();
            QString cookie = data["cookie"].toString();
            onConferenceInvited(friendNumber, cookie);
        } else if (e.type == "conference_message") {
            QVariantMap data = api->parseJsonString(e.data);
            int confId = data["conference_number"].toInt();
            QString message = data["message"].toString();
            
            if (currentChatId == confId && currentChatType == "conference") {
                chatWidget->appendMessage(message, false);
            }
        }
    }
}

void MainWindow::onMessageReceived(int friendId, const QString& message) {
    if (currentChatId == friendId) {
        chatWidget->appendMessage(message, false);
    }
}

void MainWindow::onConferenceMessageReceived(int conferenceId, int peerNumber, const QString& message) {
    if (currentChatId == conferenceId) {
        chatWidget->appendMessage(message, false);
    }
}

void MainWindow::onConferenceInvited(int friendNumber, const QString& cookie) {
    // Show invite dialog
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, _("conference.invite_message"), 
                                  _("conference.invitation_from").arg(QString::number(friendNumber)),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Ignore);
    
    ApiRequest req;
    req.id = friendNumber;
    req.data = cookie;
    
    if (reply == QMessageBox::Yes) {
        req.type = ApiJoinConference;
    } else if (reply == QMessageBox::No) {
        req.type = ApiRejectConference;
    } else {
        return; // Ignore
    }
    
    eventPoller->postApiRequest(req);
}

void MainWindow::onErrorOccurred(const QString& error) {
    statusBar()->showMessage(error, 5000);
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
    
    ApiRequest req;
    req.type = ApiDeleteFriend;
    req.id = currentChatId;
    eventPoller->postApiRequest(req);
    
    statusBar()->showMessage(_("deleting_friend"), 3000);
}

void MainWindow::loadContactInfo(int id, const QString& type) {
    if (type == "friend") {
        api->getFriendInfo(id);
    } else if (type == "conference") {
        // For conference, we might need different API
        // For now, just add to contact list with basic info
        Contact contact;
        contact.id = id;
        contact.name = QString("Conference %1").arg(id);
        contact.type = type;
        contact.status = "online";
        contactListWidget->addContact(contact);
    }
}

void MainWindow::saveLanguage(const QString& lang) {
    QFile file(QDir::homePath() + "/.q4tox_lang");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        stream << lang << "\n";
        file.close();
    }
}

QString MainWindow::loadSavedLanguage() {
    QFile file(QDir::homePath() + "/.q4tox_lang");
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        QString lang = stream.readLine().trimmed();
        file.close();
        if (!lang.isEmpty()) return lang;
    }
    return "zh-CN";
}

void MainWindow::onEditInfoRequested(const QString& name, const QString& statusMessage) {
    if (!name.isEmpty()) {
        api->setSelfName(name);
    }
    if (!statusMessage.isEmpty()) {
        api->setSelfStatus(statusMessage);
    }
}

void MainWindow::onBootstrapRequested() {
    api->bootstrap();
    statusBar()->showMessage(_("connecting"), 3000);
}

void MainWindow::onAddFriendRequested(const QString& publicKey) {
    if (publicKey.length() != 64 && publicKey.length() != 76) {
        QMessageBox::warning(this, _("error"), _("invalid_public_key"));
        return;
    }
    
    ApiRequest req;
    req.type = ApiAddFriend;
    req.message = publicKey;
    eventPoller->postApiRequest(req);
    
    statusBar()->showMessage(_("adding_friend"), 3000);
}

void MainWindow::onCreateConferenceRequested() {
    ApiRequest req;
    req.type = ApiCreateConference;
    eventPoller->postApiRequest(req);
    
    statusBar()->showMessage(_("creating_conference"), 3000);
}

void MainWindow::onCreateGroupRequested() {
    ApiRequest req;
    req.type = ApiCreateGroup;
    eventPoller->postApiRequest(req);
    
    statusBar()->showMessage(_("creating_group"), 3000);
}
