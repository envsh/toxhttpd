#include "mainwindow.h"
#include "CustomTitleBar.h"
#include "restapi.h"
#include "eventpoller.h"
#include "unknownparser.h"
#include "avatar_manager.h"
#include "translator.h"
#include "logindialog.h"
#include "conferenceinvitedialog.h"
#include "groupinvitedialog.h"
#include "friendinfodialog.h"
#include "memberlistdialog.h"
#include "storage.h"
#include "channel_db.h"
#include "cache_db.h"
#include "cJSON.h"
#include "jsonview.h"
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
#include "sharedstatusbar.h"
#include "photoviewer.h"
#include "media_shmem_cache.h"
#include <qtimer.h>
#include <qlabel.h>
#include "ConfigDialog.h"
#include <qpushbutton.h>
#include <qlineedit.h>

// 虚拟联系人 ID（使用 <-100 的负数避免与服务器 ID 及 "未选择" 哨兵值 -1 冲突）
static const int VIRTUAL_UNKNOWN_ID = -100;
static const int VIRTUAL_SYSEVENT_ID = -101;
static const int VIRTUAL_REDDIT_ID = -102;

// ── media thumbnail 辅助函数（从原始字节解码 → 缩放到显示尺寸）──
static QPixmap decodeRawToThumb(const char* data, int len, int mediaW, int mediaH, int maxW) {
    QPixmap tmp;
    tmp.loadFromData((const uchar*)data, len);
    if (tmp.isNull()) {
        std::string s(data, len);
        tmp = decodeWebP(s);
    }
    if (tmp.isNull()) return QPixmap();
    return makeScaledThumb(tmp, mediaW, mediaH, maxW);
}

// ── peer 持久化 helpers ──
// key 格式: "friend_N", "group_N_M", "conference_N_M", "unknown_*"
struct PeerKey {
    std::string chanid;
    int peerNum = 0;
    bool valid = false;
};
static PeerKey parsePeerKey(const std::string& key) {
    PeerKey ret;
    if (key.compare(0, 7, "friend_") == 0) {
        ret.chanid = "friend:" + key.substr(7);
        ret.peerNum = std::stoi(key.substr(7));
        ret.valid = true;
    } else if (key.compare(0, 6, "group_") == 0) {
        size_t us = key.find('_', 6);
        if (us == std::string::npos) { return ret; }
        ret.chanid = "group:" + key.substr(6, us - 6);
        ret.peerNum = std::stoi(key.substr(us + 1));
        ret.valid = true;
    } else if (key.compare(0, 11, "conference_") == 0) {
        size_t us = key.find('_', 11);
        if (us == std::string::npos) { return ret; }
        ret.chanid = "conference:" + key.substr(11, us - 11);
        ret.peerNum = std::stoi(key.substr(us + 1));
        ret.valid = true;
    } else if (key.compare(0, 8, "unknown_") == 0) {
        ret.chanid = "unknown:" + key.substr(8);
        ret.peerNum = 0;
        ret.valid = true;
    }
    return ret;
}

// Row→PeerInfo 转换 + 写入 peerInfoMap + 可选 avatar 下载
// 返回 map iterator，失败返回 end()
static std::map<std::string, PeerInfo>::iterator
loadRowToMap(std::map<std::string, PeerInfo>& m,
             const std::string& key,
             std::unique_ptr<PeerRow> row)
{
    if (!row) return m.end();
    PeerInfo pi;
    pi.peerNumber = row->peer_number;
    pi.publicKey  = row->public_key;
    pi.name       = row->name;
    pi.nickname   = row->nickname;
    pi.iconUrl    = row->avatar_url;
    pi.statusText = row->status_text;
    pi.statusStr  = row->status_str;
    pi.userStatus = row->user_status;
    pi.peerIp     = row->peer_ip;
    pi.role       = row->role;
    pi.roleStr    = row->role_str;
    pi.isSelf     = row->is_self;
    pi.lastSeen   = (time_t)row->last_seen;
    pi.status     = row->status;
    auto result = m.insert({key, pi});
    if (result.second && !pi.iconUrl.empty()) {
        QString mxc = qFromUtf8(pi.iconUrl);
        if (AvatarManager::inst().requestDownload(mxc)) {
            ToxAPI::downloadAvatar(pi.iconUrl);
        }
    }
    return result.first;
}

static bool addPeerToDb(const std::string& key, const PeerInfo& pi) {
    PeerKey pk = parsePeerKey(key);
    if (!pk.valid) { return false; }
    if (pi.nickname.empty() && pi.iconUrl.empty()) {
        qWarning("addPeerToDb: skip, key=%s", key.c_str());
        return false;
    }
    PeerRow row;
    row.chanid = pk.chanid;
    row.peer_number = pk.peerNum;
    row.public_key = pi.publicKey;
    row.name = pi.name;
    row.nickname = pi.nickname;
    row.avatar_url = pi.iconUrl;
    row.status_text = pi.statusText;
    row.status_str = pi.statusStr;
    row.user_status = pi.userStatus;
    row.peer_ip = pi.peerIp;
    row.role = pi.role;
    row.role_str = pi.roleStr;
    row.is_self = pi.isSelf;
    row.last_seen = (int64_t)pi.lastSeen;
    row.status = pi.status;
    auto* async = Storage::instance().channelDbAsync();
    if (async) {
        async->add_peer(std::move(row), nullptr);
    }
    return true;
}

static void updatePeerInDb(const std::string& key, const PeerInfo& pi) {
    PeerKey pk = parsePeerKey(key);
    if (!pk.valid) { return; }
    if (pk.chanid.compare(0, 8, "unknown:") == 0) { return; } // unknown 不走部分更新
    PeerRow row;
    row.chanid = pk.chanid;
    row.peer_number = pk.peerNum;
    row.public_key = pi.publicKey;
    row.name = pi.name;
    row.nickname = pi.nickname;
    row.avatar_url = pi.iconUrl;
    row.status_text = pi.statusText;
    row.status_str = pi.statusStr;
    row.user_status = pi.userStatus;
    row.peer_ip = pi.peerIp;
    row.role = pi.role;
    row.role_str = pi.roleStr;
    row.is_self = pi.isSelf;
    row.last_seen = (int64_t)pi.lastSeen;
    row.status = pi.status;
    auto* async = Storage::instance().channelDbAsync();
    if (async) {
        async->update_peer(std::move(row), nullptr);
    }
}

static PeerInfo& getOrCreatePeerEntry(std::map<std::string, PeerInfo>& m, const std::string& key) {
    auto it = m.find(key);
    if (it == m.end()) {
        it = m.insert({key, PeerInfo()}).first;
    }
    return it->second;
}

static void mergePeerInfo(PeerInfo& dst, const PeerInfo& src) {
    if (src.peerNumber != 0) dst.peerNumber = src.peerNumber;
    if (!src.name.empty()) dst.name = src.name;
    if (!src.nickname.empty()) dst.nickname = src.nickname;
    if (src.status != 0) dst.status = src.status;
    if (!src.statusStr.empty()) dst.statusStr = src.statusStr;
    if (!src.statusText.empty()) dst.statusText = src.statusText;
    if (!src.iconUrl.empty()) dst.iconUrl = src.iconUrl;
    if (src.role != 0) dst.role = src.role;
    if (!src.roleStr.empty()) dst.roleStr = src.roleStr;
    if (!src.publicKey.empty()) dst.publicKey = src.publicKey;
    if (src.isSelf) dst.isSelf = true;
    if (!src.peerIp.empty()) dst.peerIp = src.peerIp;
    if (!src.userStatus.empty()) dst.userStatus = src.userStatus;
    if (src.lastSeen != 0) dst.lastSeen = src.lastSeen;
}

static QString timenowhm() {
#ifdef QT3_BUILD
    return QTime::currentTime().toString("hh:mm");
#else
    return QTime::currentTime().toString("HH:mm");
#endif
}

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

static bool unknownShouldPlaySound(const std::string& type,
                                   const std::string& message,
                                   const std::vector<std::string>& selfNames)
{
    if (type == kUnktoxFriendType || type == kUnktoxConferenceType || type == kUnktoxGroupType)
        return true;
    if (type == kImapMailType)
        return true;

    std::string lowerMsg = message;
    for (size_t i = 0; i < lowerMsg.size(); i++) { lowerMsg[i] = tolower((unsigned char)lowerMsg[i]); }
    for (size_t ni = 0; ni < selfNames.size(); ++ni) {
        std::string lowerName = selfNames[ni];
        for (size_t i = 0; i < lowerName.size(); i++) { lowerName[i] = tolower((unsigned char)lowerName[i]); }
        if (lowerMsg.find("@" + lowerName) != std::string::npos)
            return true;
    }
    if (lowerMsg.find("funami.tech") != std::string::npos)
        return true;
    if (lowerMsg.find("P5N4wp1ga06A") != std::string::npos)
		return true;

    return false;
}

static QString formatElapsedMs(int64_t ms) {
    if (ms < 1000) {
        return QString::number((int)ms) + "ms";
    }
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

static void initDemoWidgets() {
    // Left side (addWidget)
    QLabel *leftLabel = new QLabel(qFromUtf8("Left"), SharedStatusBar::instance());
    SharedStatusBar::instance()->addWidget(leftLabel);

    QPushButton *leftBtn = new QPushButton(qFromUtf8("LBtn"), SharedStatusBar::instance());
    leftBtn->setFixedWidth(36);
    SharedStatusBar::instance()->addWidget(leftBtn);

    // Right side permanent (addPermanentWidget)
    QLabel *rightLabel = new QLabel(qFromUtf8("Right"), SharedStatusBar::instance());
    SharedStatusBar::instance()->addPermanentWidget(rightLabel);

    QPushButton *rightBtn = new QPushButton(qFromUtf8("RBtn"), SharedStatusBar::instance());
    rightBtn->setFixedWidth(36);
    SharedStatusBar::instance()->addPermanentWidget(rightBtn);

    QLineEdit *demoEdit = new QLineEdit(SharedStatusBar::instance());
    demoEdit->setFixedWidth(100);
#ifndef QT3_BUILD
    demoEdit->setPlaceholderText(qFromUtf8("input..."));
#endif
    SharedStatusBar::instance()->addPermanentWidget(demoEdit);

    SharedStatusBar::instance()->showMessage(
        qFromUtf8("Demo: 左端 widgets 隐藏中..."), 3000);
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
    setGeometry(100, 50, 1100, 680);

    // 初始化本地存储
    QString dataDir = qGetHomePath() + "/.cache/toxhttpd";
    Storage::instance().init(
#ifdef QT3_BUILD
        dataDir.utf8()
#else
        dataDir.toUtf8().constData()
#endif
    );

    // 设置 UTF-8 编解码器
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    
    // 底部状态栏浮窗
    SharedStatusBar::instance()->show();
    initDemoWidgets();
    
    QWidget* centralContainer = new QWidget(this);
    QBoxLayout* mainLayout = qNewBoxLayout(centralContainer, QBoxLayout::TopToBottom, 0, 0);

    // 主分割器（左右布局）
    splitter = new QSplitter(Qt::Horizontal, centralContainer);
    splitter->setOpaqueResize(true);
    
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
    connect(chatWidget, SIGNAL(messageSent(const QString&)), this, SLOT(onMessageSending(const QString&)));
    connect(chatWidget, SIGNAL(languageChanged(const QString&)), 
            this, SLOT(onLanguageChanged(const QString&)));
    connect(chatWidget, SIGNAL(translateRequested(int, const QString&, const QString&)),
            this, SLOT(onTranslateRequested(int, const QString&, const QString&)));
    connect(chatWidget, SIGNAL(translateForSendRequested(const QString&, const QString&)),
            this, SLOT(onTranslateForSendRequested(const QString&, const QString&)));
    connect(chatWidget, SIGNAL(sourceClicked(int)), this, SLOT(onSourceClicked(int)));
    connect(chatWidget, SIGNAL(retryClicked(int, const QString&, const QString&)), this, SLOT(onRetryClicked(int, const QString&, const QString&)));
    connect(chatWidget, SIGNAL(openFullSizeImage(int, const QString&)),
            this, SLOT(onOpenFullSizeImage(int, const QString&)));
    connect(&Translator::instance(), SIGNAL(languageChanged()), this, SLOT(retranslateUi()));
    
    // 启动事件轮询引擎
    EventPoller::start();
    ToxAPI::setEventTarget(this);
    ToxAPI::startPollEvent();
    
    // 先填充虚拟联系人，确保即使 API 未加载也能看到 Unknown/Sysevent
    {
        ContactList seedList;
        Contact* c = new Contact();
        c->id = VIRTUAL_UNKNOWN_ID; c->name = "Unknown";
        c->type = kUnknownType; c->status = "online";
        c->chat_id = ""; c->is_connected = false;
        seedList.append(c);
        c = new Contact();
        c->id = VIRTUAL_SYSEVENT_ID; c->name = "Sysevent";
        c->type = kSyseventType; c->status = "online";
        c->chat_id = ""; c->is_connected = false;
        seedList.append(c);
        c = new Contact();
        c->id = VIRTUAL_REDDIT_ID; c->name = "Reddit";
        c->type = kTopicType; c->status = "online";
        c->chat_id = ""; c->is_connected = false;
        seedList.append(c);
        contactListWidget->setContacts(seedList);
    }
    
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
    EmbeddedMenuBar::addItem(tool, qFromUtf8("统计(&T)..."), this, SLOT(onMenu1Stub()));
    EmbeddedMenuBar::addItem(tool, qFromUtf8("日志(&L)..."), this, SLOT(onMenu1Stub()));
    EmbeddedMenuBar::addItem(tool, qFromUtf8("设置(&S)...\tCtrl+,"), this, SLOT(openSettings()));

    MenuWidget34* help = mb->addMenu(qFromUtf8("帮助(&H)"));
    EmbeddedMenuBar::addItem(help, _("menu.homepage"), this, SLOT(openHomePage()));
    EmbeddedMenuBar::addItem(help, _("menu.aboutqt"), qApp, SLOT(aboutQt()));
    EmbeddedMenuBar::addItem(help, qFromUtf8("关于(&A)..."), this, SLOT(onMenu1Stub()));

    mb->finalize();
}

MainWindow::~MainWindow() {
    ToxAPI::stopPollEvent();
    EventPoller::stop();
    Storage::instance().close();
}

void MainWindow::customEvent(CustomEventBase* event) {
    // 事件轮询结果
    if (event->type() == EventListReadyType) {
        EventListEvent* e = static_cast<EventListEvent*>(event);
        handleEvents(e->events);
        return;
    }

    // 媒体下载完成
    if (event->type() == MediaDownloadReadyType) {
        MediaDownloadEvent* e = static_cast<MediaDownloadEvent*>(event);
        int cid = currentChatId;
        QString ctype = currentChatType;
        if (cid == e->chatId && ctype == qFromUtf8(e->chatType)) {
            if (e->success) {
                if (e->msgIndex >= 0 && e->msgIndex < chatWidget->messageCount()) {
                    ChatElement& el = chatWidget->mutableMessageAt(e->msgIndex);

                    {
                        // Cache raw bytes (JPEG/WebP), not QPixmap
                        MediaShmemCache::inst().putThumb(qFromUtf8(e->mxcUrl), (const char*)e->rawData.data(), e->rawData.size());
                        el.scaledDisplay = decodeRawToThumb((const char*)e->rawData.data(), e->rawData.size(),
                            el.mediaWidth, el.mediaHeight, chatWidget->width() * 70 / 100);
                    }

                    {
                        std::string key = mediaCacheKey("file", qFromUtf8(e->mxcUrl));
                        const auto& rd = e->rawData;
                        std::vector<uint8_t> data(rd.begin(), rd.end());
                        Storage::instance().cacheDbAsync()->storeMedia(
                            std::move(key), std::move(data), "", 2, nullptr);
                    }

                    el.downloadState = ChatElement::Completed;
                    chatWidget->triggerRelayout(e->msgIndex);
                }
            } else {
                qWarning("Media download failed: chat=%d/%s idx=%d err=%s",
                         e->chatId, e->chatType.c_str(), e->msgIndex, e->errorInfo.c_str());
                if (e->msgIndex >= 0 && e->msgIndex < chatWidget->messageCount()) {
                    ChatElement& el = chatWidget->mutableMessageAt(e->msgIndex);
                    el.downloadState = ChatElement::Failed;
                    el.mediaUrl = qFromUtf8(e->mxcUrl);
                    chatWidget->triggerRelayout(e->msgIndex);
                }
            }
        }
        return;
    }

    // 头像下载完成
    if (event->type() == AvatarDownloadReadyType) {
        AvatarDownloadEvent* e = static_cast<AvatarDownloadEvent*>(event);
        QString key = qFromUtf8(e->mxcUrl);
        if (e->success) {
            if (!e->pixmap.isNull()) {
                AvatarManager::inst().store(key, e->pixmap, ChatView::kAvatarSize);
                chatWidget->repaintMessages();
            } else {
                qWarning("AvatarManager: success but null pixmap for [%s]",
                         e->mxcUrl.c_str());
            }
        } else {
            qWarning("AvatarManager: download failed for [%s] reason: [%s]",
                     e->mxcUrl.c_str(), e->errorInfo.c_str());
            AvatarManager::inst().removePending(key);
        }
        return;
    }

    // 磁盘缓存加载完成（双击查看原图）
    if (event->type() == DiskLoadReadyType) {
        DiskLoadEvent* e = static_cast<DiskLoadEvent*>(event);
        if (!e->success) {
            qWarning("DiskLoad: cache miss for msg %d url=%s",
                     e->msgIndex, e->mediaUrl.c_str());
            return;
        }
        QPixmap pix;
        {
            const auto& rd = e->rawData;
            std::string rawStr(rd.begin(), rd.end());
            if (!pix.loadFromData((const uchar*)rawStr.data(), rawStr.size())) {
                if (isWebP(rawStr))
                    pix = decodeWebP(rawStr);
            }
        }
        if (pix.isNull()) { return; }
        PhotoViewer* pv = new PhotoViewer(this, pix);
        pv->show();
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
                    qFromUtf8(evt->selfName),
                    qFromUtf8(evt->selfStatusMsg),
                    qFromUtf8(evt->selfConnStatus),
                    qFromUtf8(evt->selfAddress));
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
                        auto& entry = getOrCreatePeerEntry(peerInfoMap, key);
                        entry.name = cd.name;
                        entry.peerNumber = cd.id;
                        entry.publicKey = cd.chatId;
                        entry.iconUrl = cd.iconUrl;
                        {
                            QString mxc = qFromUtf8(cd.iconUrl);
                            if (AvatarManager::inst().requestDownload(mxc)) {
                                ToxAPI::downloadAvatar(cd.iconUrl);
                            }
                        }
                        entry.isSelf = false;
                        if (cd.status == "tcp") {
                            entry.status = 1;
                            entry.statusStr = "tcp";
                        } else if (cd.status == "udp") {
                            entry.status = 2;
                            entry.statusStr = "udp";
                        } else {
                            entry.status = 0;
                            entry.statusStr = "none";
                        }
                        addPeerToDb(key, entry);
                    }
                }
                ContactList cl;
                for (const auto& cd : m_accumulatedContactData) {
                    Contact* c = new Contact();
                    c->id = cd.id;
                    c->name = qFromUtf8(cd.name);
                    c->type = qFromUtf8(cd.type);
                    c->status = qFromUtf8(cd.status);
                    c->chat_id = qFromUtf8(cd.chatId);
                    c->is_connected = cd.isConnected;
                    cl.append(c);
                }
                // 追加虚拟联系人（始终在列表底部）
                {
                    Contact* c = new Contact();
                    c->id = VIRTUAL_UNKNOWN_ID;
                    c->name = "Unknown";
                    c->type = kUnknownType;
                    c->status = "online";
                    c->chat_id = "";
                    c->is_connected = false;
                    cl.append(c);
                }
                {
                    Contact* c = new Contact();
                    c->id = VIRTUAL_SYSEVENT_ID;
                    c->name = "Sysevent";
                    c->type = kSyseventType;
                    c->status = "online";
                    c->chat_id = "";
                    c->is_connected = false;
                    cl.append(c);
                }
                // 追加固定虚拟联系人
                {
                    Contact* c = new Contact();
                    c->id = VIRTUAL_REDDIT_ID;
                    c->name = "Reddit";
                    c->type = kTopicType;
                    c->status = "online";
                    c->chat_id = "";
                    c->is_connected = false;
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
                    qFromUtf8(evt->name),
                    qFromUtf8(evt->publicKey),
                    qFromUtf8(evt->statusStr));
                std::string key = "friend_" + std::to_string(evt->friendId);
                auto& entry = getOrCreatePeerEntry(peerInfoMap, key);
                entry.name = evt->name;
                entry.peerNumber = evt->friendId;
                entry.publicKey = evt->publicKey;
                entry.iconUrl = evt->iconUrl;
                {
                    QString mxc = qFromUtf8(evt->iconUrl);
                    if (AvatarManager::inst().requestDownload(mxc)) {
                        ToxAPI::downloadAvatar(evt->iconUrl);
                    }
                }
                entry.statusText = evt->statusText;
                int s = 0;
                if (evt->statusStr == "tcp") { s = 1; }
                else if (evt->statusStr == "udp") s = 2;
                entry.status = s;
                entry.statusStr = evt->statusStr;
                entry.peerIp = evt->peerIp;
                entry.userStatus = evt->userStatus;
                entry.lastSeen = evt->lastSeen;
                addPeerToDb(key, entry);
            }
            return;
        }
        
        // 自身信息更新结果
        if (e->type == ApiGetSelf) {
            SelfInfoResultEvent* evt = static_cast<SelfInfoResultEvent*>(event);
            if (evt->success) {
                selfInfoWidget->updateInfo(
                    qFromUtf8(evt->name),
                    qFromUtf8(evt->statusMsg),
                    qFromUtf8(evt->connStatus),
                    qFromUtf8(evt->address));
                if (evt->address.length() >= 64) {
                    selfPubkey = evt->address.substr(0, 64);
                }
            }
            return;
        }
        
        // 消息发送结果
        if (e->type == ApiSendFriendMessage || e->type == ApiSendConferenceMessage || e->type == ApiSendGroupMessage || e->type == ApiSendMessage) {
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
            if (!evt->success) {
                ToastWidget::show(chatWidget, _("send_failed").arg(targetName).arg(formatElapsedMs(evt->elapsedMs)), 8000);
            }
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
                    addPeerToDb(key, m);
                    // 没有 nickname 和 avatar 的数据没必要缓存
                    if (m.nickname.empty() && m.iconUrl.empty()) {
                        continue;
                    }
                    mergePeerInfo(getOrCreatePeerEntry(peerInfoMap, key), m);
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

        // Send EN 翻译结果
        if (e->type == ApiTranslateForSend) {
            TranslateForSendResultEvent* tev = static_cast<TranslateForSendResultEvent*>(event);
            if (tev->success) {
                chatWidget->loadingBar()->showLoading(kLoadSendMsg,
                    _("sending_message"));
                std::string type = std::string(qToUtf8(currentChatType).data());
                std::string idOverride;
                if (type == kGomuksRoomType || type == kUnktoxConferenceType
                    || type == kUnktoxFriendType || type == kUnktoxGroupType) {
                    for (const auto& cd : m_accumulatedContactData) {
                        if (cd.id == currentChatId && cd.type == type) {
                            idOverride = cd.chatId;
                            break;
                        }
                    }
                }
                ToxAPI::sendMessage(currentChatId, type, tev->translatedText, idOverride);
            } else {
                chatWidget->loadingBar()->hideLoading(kLoadSendMsg);
                ToastWidget::show(chatWidget, "翻译失败", 8000);
            }
            return;
        }
    }
}

bool MainWindow::event(QEvent* event) {
    bool ret = QMainWindow::event(event);
    if (!m_firstPaintLogged && event->type() == QEvent::Paint) {
        m_paintCounter++;		
	}
    if (!m_firstPaintLogged) {
		if (event->type() == QEvent::Paint) {
			if (!qApp->hasPendingEvents() || m_paintCounter > 5) {
				m_firstPaintLogged = true;
				QTimer::singleShot(0, this, SLOT(onFirstPaintComplete()));
			}
		} else {
			if (!qApp->hasPendingEvents() && m_paintCounter > 0) {
				m_firstPaintLogged = true;
				QTimer::singleShot(0, this, SLOT(onFirstPaintComplete()));
			}			
		}
    }
    return ret;
}

void MainWindow::onFirstPaintComplete() {
    qWarning("=== UI 首次绘制完成 === pending=%s",
             qApp->hasPendingEvents() ? "true" : "false");
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
    
    // 保存当前联系人的消息到缓存（move，不复制）
    if (currentChatId != -1 && !currentChatType.isEmpty()) {
        auto key = std::make_pair(currentChatId, std::string(qToUtf8(currentChatType).data()));
        m_messageCache[key] = chatWidget->detachMessages();
    }
    
    qWarning("onContactSelected: id=%d, type=%s", id, qToUtf8(type).data());
    currentChatId = id;
    currentChatType = type;
    int prevUnread = contactListWidget->unreadCount(id, type);
    contactListWidget->resetUnread(id, type);
    
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
    } else if (type == kUnknownType) {
        emoji = EMOJI_UNKNOWN;
        headerText = emoji + " " + name;
    } else if (type == kSyseventType) {
        emoji = EMOJI_SYSEVENT;
        headerText = emoji + " " + name;
    } else if (type == kTopicType) {
        emoji = EMOJI_TOPIC;
        headerText = emoji + " " + name;
    } else if (type == kGomuksRoomType) {
        emoji = EMOJI_MATRIX;
        headerText = emoji + " " + name;
    } else if (type == kUnktoxFriendType) {
        emoji = EMOJI_FRIEND;
        headerText = emoji + " " + name;
    } else if (type == kUnktoxConferenceType) {
        emoji = EMOJI_CONFERENCE;
        headerText = emoji + " " + name;
    } else if (type == kUnktoxGroupType) {
        emoji = EMOJI_GROUP;
        headerText = emoji + " " + name;
    } else if (type == kImapMailType) {
        emoji = "E";
        headerText = emoji + " " + name;
    }
    
    chatWidget->setHeaderText(headerText);
    if (prevUnread > 0)
        chatWidget->showUnreadBanner(prevUnread);
    
    std::string typeStr = std::string(qToUtf8(type).data());
    auto key = std::make_pair(id, typeStr);
    auto cacheIt = m_messageCache.find(key);
    bool hasCache = (cacheIt != m_messageCache.end());
    
    if (hasCache) {
        // 缓存命中：恢复缓存消息，同时后台拉取刷新
        qWarning("Cache HIT for %s %d: %d msgs", typeStr.c_str(), id, (int)cacheIt->second.size());
        chatWidget->attachMessages(std::move(cacheIt->second));
        if (id >= 0 && type != kGomuksRoomType && type != kUnktoxFriendType
            && type != kUnktoxConferenceType && type != kUnktoxGroupType && type != kImapMailType
            && type != kFilesyncType && type != kClipboardType) {
            chatWidget->loadingBar()->showLoading(kLoadMessages, _("loading_messages"));
            ToxAPI::getMessagesHistory(id, typeStr);
        }
    } else {
        // 缓存未命中
        qWarning("Cache MISS for %s %d", typeStr.c_str(), id);
        chatWidget->clearMessages();
        if (id >= 0 && type != kGomuksRoomType && type != kUnktoxFriendType
            && type != kUnktoxConferenceType && type != kUnktoxGroupType && type != kImapMailType
            && type != kFilesyncType && type != kClipboardType) {
            chatWidget->loadingBar()->showLoading(kLoadMessages, _("loading_messages"));
            ToxAPI::getMessagesHistory(id, typeStr);
        }
    }
    
    // 异步预加载成员列表到 peerInfoMap 缓存（虚拟联系人跳过）
    if (id < 0) return;
    if (type == "group") {
        chatWidget->loadingBar()->showLoading(kLoadMembers, _("loading_members"));
        ToxAPI::getGroupMembers(id);
    } else if (type == "conference") {
        chatWidget->loadingBar()->showLoading(kLoadMembers, _("loading_members"));
        ToxAPI::getConferenceMembers(id);
    }
}

void MainWindow::onMessageSending(const QString& message) {
    // ── 统一发送 API ──
    // 注释掉下面这行可切回旧的三条独立端点
#define USE_UNIFIED_SEND_API
    qWarning("onMessageSending: id=%d type=%s msg=[%.60s]",
             currentChatId, qToUtf8(currentChatType).data(), qToUtf8(message).data());
    if (currentChatId == -1 || currentChatType.isEmpty()) {
        QMessageBox::warning(this, _("select_chat_first"), _("select_chat_first"));
        return;
    }

#ifdef USE_UNIFIED_SEND_API
    std::string type = std::string(qToUtf8(currentChatType).data());
    if (type.empty()) {
        qWarning("Empty chat type");
        return;
    }

    // 虚拟类型使用 chatId 字符串（如 gomuks room ID）而非 numeric contactId
    std::string idOverride;
    if (type == kGomuksRoomType || type == kUnktoxConferenceType
        || type == kUnktoxFriendType || type == kUnktoxGroupType) {
        for (const auto& cd : m_accumulatedContactData) {
            if (cd.id == currentChatId && cd.type == type) {
                idOverride = cd.chatId;
                break;
            }
        }
    }

    chatWidget->loadingBar()->showLoading(kLoadSendMsg, _("sending_message"));
    ToxAPI::sendMessage(currentChatId, type, std::string(qToUtf8(message)), idOverride);
#else
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
#endif

    // 乐观更新：先显示在界面
    chatWidget->appendMessage(message, "self", "Me", QString(), -1, getCurrentTime());
    contactListWidget->updateContactLastMessage(currentChatId, currentChatType, message, timenowhm());
}

void MainWindow::handleEvents(const EventList& events) {
    contactListWidget->beginBatch();
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
                        chatWidget->appendMessage(message, "other", QString(), QString(),
                                         friendId, getCurrentTime());
                    } else {
                        contactListWidget->incrementUnread(friendId, "friend");
                    }
                    contactListWidget->updateContactLastMessage(friendId, "friend", message, timenowhm());
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
                    
                    std::string key = "conference_" + std::to_string(confNumber) + "_" + std::to_string(peerNumber);
                    auto it = peerInfoMap.find(key);

                    if (it == peerInfoMap.end()) {
                        qWarning("conference_message: peer record load, key=%s", key.c_str());
                        PeerKey pk = parsePeerKey(key);
                        if (pk.valid) {
                            auto row = Storage::instance().channelDb()->get_chan_peer(
                                pk.chanid.c_str(), pk.peerNum);
                            it = loadRowToMap(peerInfoMap, key, std::move(row));
                        }
                        // 不创建空条目
                    } else {
                        // 第二层：key 已存在，逐个检查缺失字段
                        bool needsNick = it->second.nickname.empty();
                        bool needsAvatar = it->second.iconUrl.empty();
                        if (needsNick || needsAvatar) {
                            qWarning("conference_message: peer field fill, key=%s nick=%s avatar=%s",
                                     key.c_str(), needsNick?"miss":"hit", needsAvatar?"miss":"hit");
                            PeerKey pk = parsePeerKey(key);
                            if (pk.valid) {
                                auto row = Storage::instance().channelDb()->get_chan_peer(
                                    pk.chanid.c_str(), pk.peerNum);
                                if (row) {
                                    if (needsNick && !row->nickname.empty()) {
                                        it->second.nickname = row->nickname;
                                    }
                                    if (needsAvatar && !row->avatar_url.empty()) {
                                        it->second.iconUrl = row->avatar_url;
                                        QString mxc = qFromUtf8(it->second.iconUrl);
                                        if (AvatarManager::inst().requestDownload(mxc)) {
                                            ToxAPI::downloadAvatar(it->second.iconUrl);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (it != peerInfoMap.end()) {
                        if (peerNameItem && cJSON_IsString(peerNameItem)) {
                            it->second.name = std::string(cJSON_GetStringValue(peerNameItem));
                            it->second.peerNumber = peerNumber;
                        }
                        updatePeerInDb(key, it->second);
                    }

                    if (confNumber == currentChatId && currentChatType == "conference") {
                        QString senderName;
                        QString senderNickname;
                        if (it != peerInfoMap.end()) {
                            senderName = qFromUtf8(it->second.name);
                            if (!it->second.nickname.empty())
                                senderNickname = qFromUtf8(it->second.nickname);
                        } else if (peerNameItem && cJSON_IsString(peerNameItem)) {
                            senderName = qFromUtf8(cJSON_GetStringValue(peerNameItem));
                        }
                        chatWidget->appendMessage(message, "other", senderName, senderNickname, peerNumber, getCurrentTime());
                    } else {
                        contactListWidget->incrementUnread(confNumber, "conference");
                    }
                    contactListWidget->updateContactLastMessage(confNumber, "conference", message, timenowhm());
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
                    
                    std::string key = "group_" + std::to_string(groupNumber) + "_" + std::to_string(peerNumber);
                    auto it = peerInfoMap.find(key);

                    if (it == peerInfoMap.end()) {
                        qWarning("group_message: peer record load, key=%s", key.c_str());
                        PeerKey pk = parsePeerKey(key);
                        if (pk.valid) {
                            auto row = Storage::instance().channelDb()->get_chan_peer(
                                pk.chanid.c_str(), pk.peerNum);
                            it = loadRowToMap(peerInfoMap, key, std::move(row));
                        }
                        // 不创建空条目
                    } else {
                        // 第二层：key 已存在，逐个检查缺失字段
                        bool needsNick = it->second.nickname.empty();
                        bool needsAvatar = it->second.iconUrl.empty();
                        if (needsNick || needsAvatar) {
                            qWarning("group_message: peer field fill, key=%s nick=%s avatar=%s",
                                     key.c_str(), needsNick?"miss":"hit", needsAvatar?"miss":"hit");
                            PeerKey pk = parsePeerKey(key);
                            if (pk.valid) {
                                auto row = Storage::instance().channelDb()->get_chan_peer(
                                    pk.chanid.c_str(), pk.peerNum);
                                if (row) {
                                    if (needsNick && !row->nickname.empty()) {
                                        it->second.nickname = row->nickname;
                                    }
                                    if (needsAvatar && !row->avatar_url.empty()) {
                                        it->second.iconUrl = row->avatar_url;
                                        QString mxc = qFromUtf8(it->second.iconUrl);
                                        if (AvatarManager::inst().requestDownload(mxc)) {
                                            ToxAPI::downloadAvatar(it->second.iconUrl);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (it != peerInfoMap.end()) {
                        if (peerNameItem && cJSON_IsString(peerNameItem)) {
                            it->second.name = std::string(cJSON_GetStringValue(peerNameItem));
                            it->second.peerNumber = peerNumber;
                        }
                        updatePeerInDb(key, it->second);
                    }

                    if (groupNumber == currentChatId && currentChatType == "group") {
                        QString senderName;
                        QString senderNickname;
                        QString ipAddress;
                        if (it != peerInfoMap.end()) {
                            senderName = qFromUtf8(it->second.name);
                            ipAddress = qFromUtf8(it->second.peerIp);
                            if (!it->second.nickname.empty())
                                senderNickname = qFromUtf8(it->second.nickname);
                        } else if (peerNameItem && cJSON_IsString(peerNameItem)) {
                            senderName = qFromUtf8(cJSON_GetStringValue(peerNameItem));
                        }
                        chatWidget->appendMessage(message, "other", senderName, senderNickname, peerNumber, getCurrentTime(), "", ipAddress);
                    } else {
                        contactListWidget->incrementUnread(groupNumber, "group");
                    }
                    contactListWidget->updateContactLastMessage(groupNumber, "group", message, timenowhm());
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
                    auto it = peerInfoMap.find(key);
                    if (it == peerInfoMap.end()) {
                        PeerKey pk = parsePeerKey(key);
                        if (pk.valid) {
                            auto row = Storage::instance().channelDb()->get_chan_peer(
                                pk.chanid.c_str(), pk.peerNum);
                            it = loadRowToMap(peerInfoMap, key, std::move(row));
                        }
                    }
                    if (it != peerInfoMap.end()) {
                        it->second.name = std::string(cJSON_GetStringValue(nameItem));
                        it->second.peerNumber = peerNumberItem->valueint;
                    }
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
                    auto it = peerInfoMap.find(key);
                    if (it == peerInfoMap.end()) {
                        PeerKey pk = parsePeerKey(key);
                        if (pk.valid) {
                            auto row = Storage::instance().channelDb()->get_chan_peer(
                                pk.chanid.c_str(), pk.peerNum);
                            it = loadRowToMap(peerInfoMap, key, std::move(row));
                        }
                    }
                    if (it != peerInfoMap.end()) {
                        it->second.name = std::string(cJSON_GetStringValue(nameItem));
                        it->second.peerNumber = peerNumberItem->valueint;
                    }
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
                    auto it = peerInfoMap.find(key);
                    if (it != peerInfoMap.end()) {
                        it->second.name = newName;
                        updatePeerInDb(key, it->second);
                    }
                    contactListWidget->updateFriendName(friendId, qFromUtf8(newName));
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
                    auto it = peerInfoMap.find(key);
                    if (it != peerInfoMap.end()) {
                        it->second.status = s;
                        updatePeerInDb(key, it->second);
                    }
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
                    auto it = peerInfoMap.find(key);
                    if (it != peerInfoMap.end()) {
                        it->second.status = (statusStr == "udp") ? 2 : (statusStr == "tcp") ? 1 : 0;
                        it->second.statusStr = statusStr;
                        updatePeerInDb(key, it->second);
                    }
                    contactListWidget->updateFriendConnectionStatus(friendId, qFromUtf8(statusStr));
                }
                cJSON_Delete(root);
            }
        } else if (e.data.find("\"Type\":\"event.Evt") != std::string::npos) {
            // System event with Type field — 归入 sysevent
            {
                ChatElement msg;
                msg.messageText = qFromUtf8("[" + e.type + "]\n" + e.data);
                msg.category = "other";
                msg.senderName = "Sysevent";
                msg.peerNumber = VIRTUAL_SYSEVENT_ID;
                msg.time = getCurrentTime();
                qWarning("Cache PUSH to sysevent %d: type=%s (event.Evt)", VIRTUAL_SYSEVENT_ID, e.type.c_str());
                m_messageCache[{VIRTUAL_SYSEVENT_ID, kSyseventType}].push_back(msg);
                if (currentChatId == VIRTUAL_SYSEVENT_ID && currentChatType == kSyseventType) {
                    chatWidget->appendMessage(msg);
                } else {
                    contactListWidget->incrementUnread(VIRTUAL_SYSEVENT_ID, kSyseventType);
                }
            }
        } else if (!e.type.empty() && e.type[0] == '_') {
            // System event — 始终缓存到 Sysevent，正在查看时也追加到界面
            {
                ChatElement msg;
                if (e.type == "_server_restart") {
                    msg.messageText = "[Server restart detected]";
                } else {
                    msg.messageText = qFromUtf8("[" + e.type + "]\n" + e.data);
                }
                msg.category = "other";
                msg.senderName = "Sysevent";
                msg.peerNumber = VIRTUAL_SYSEVENT_ID;
                msg.time = getCurrentTime();
                qWarning("Cache PUSH to sysevent %d: type=%s", VIRTUAL_SYSEVENT_ID, e.type.c_str());
                m_messageCache[{VIRTUAL_SYSEVENT_ID, kSyseventType}].push_back(msg);
                if (currentChatId == VIRTUAL_SYSEVENT_ID && currentChatType == kSyseventType) {
                    chatWidget->appendMessage(msg);
                } else {
                    contactListWidget->incrementUnread(VIRTUAL_SYSEVENT_ID, kSyseventType);
                }
            }
        } else {
            ParseResult pr = UnknownParser::parse(e.type, e.data);

            // ── 更新 peers ──
            if (!pr.peers.empty()) {
                for (const auto& p : pr.peers) {
                    std::string key = "unknown_" + p.publicKey;
                    addPeerToDb(key, p);
                    // 没有 nickname 和 avatar 的数据没必要缓存
                    if (p.nickname.empty() && p.iconUrl.empty()) {
                        continue;
                    }
                    mergePeerInfo(getOrCreatePeerEntry(peerInfoMap, key), p);
                }
                for (const auto& p : pr.peers) {
                    QString mxc = qFromUtf8(p.iconUrl);
                    if (AvatarManager::inst().requestDownload(mxc)) {
                        ToxAPI::downloadAvatar(p.iconUrl);
                    }
                }
            }

            // ── 更新 contacts ──
            if (!pr.contacts.empty()) {
                for (const auto& cd : pr.contacts) {
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

                        Contact* c = new Contact();
                        c->id = cd.id;
                        c->name = qFromUtf8(cd.name);
                        c->type = qFromUtf8(cd.type);
                        c->status = qFromUtf8(cd.status);
                        c->chat_id = qFromUtf8(cd.chatId);
                        c->is_connected = cd.isConnected;
                        contactListWidget->addContact(c);
                    } else {
                        contactListWidget->updateContact(cd.id, qFromUtf8(cd.type),
                            qFromUtf8(cd.name), qFromUtf8(cd.chatId), qFromUtf8(cd.status));
                    }
                }
            }

            // ── 消息处理 ──
            if (!pr.messages.empty()) {
                for (const auto& hm : pr.messages) {
                    ChatElement msg;
                    msg.messageText = qFromUtf8(hm.message);
                    msg.category = "other";
                    msg.time = getCurrentTime();

                    QString senderLabel;
                    QString userName;
                    QString avatarMxc;
                    if (!hm.sender_pubkey.empty()) {
                        for (const auto& p : pr.peers) {
                            if (p.publicKey == hm.sender_pubkey) {
                                userName = qFromUtf8(p.name);
                                senderLabel = !p.nickname.empty()
                                    ? qFromUtf8(p.nickname) : QString();
                                avatarMxc = qFromUtf8(p.iconUrl);
                                break;
                            }
                        }
                        if (senderLabel.isEmpty()) {
                            std::string key = "unknown_" + hm.sender_pubkey;
                            auto it = peerInfoMap.find(key);
                            if (it == peerInfoMap.end()) {
                                PeerKey pk = parsePeerKey(key);
                                if (pk.valid) {
                                    auto row = Storage::instance().channelDb()->get_chan_peer(
                                        pk.chanid.c_str(), pk.peerNum);
                                    it = loadRowToMap(peerInfoMap, key, std::move(row));
                                }
                            }
                            if (it != peerInfoMap.end()) {
                                userName = qFromUtf8(it->second.name);
                                senderLabel = !it->second.nickname.empty()
                                    ? qFromUtf8(it->second.nickname) : QString();
                                avatarMxc = qFromUtf8(it->second.iconUrl);
                            }
                        }
                    }
                    msg.senderName = userName;
                    msg.senderNickname = senderLabel;
                    msg.peerNumber = (int)hm.sender_number;
                    msg.avatarUrl = avatarMxc;

                    int chatId = VIRTUAL_REDDIT_ID;
                    std::string chatType = kTopicType;
                    for (const auto& cd : pr.contacts) {
                        if (cd.chatId == hm.roomId) {
                            chatId = cd.id;
                            chatType = cd.type;
                            break;
                        }
                    }

                    if (chatType == kFilesyncType) {
                        msg.etype = ChatElement::File;
                        QString raw = qFromUtf8(hm.message);
                        int colonPos = -1;
                        for (int i = 0; i < raw.length() - 1; i++) {
                            if (raw[i] == ':' && raw[i+1] == ' ') {
                                colonPos = i; break;
                            }
                        }
                        if (colonPos >= 0) {
                            msg.messageText = raw.left(colonPos);
                            msg.fileName    = raw.mid(colonPos + 2);
                        } else {
                            msg.messageText = QString();
                            msg.fileName    = raw;
                        }
                        int slashPos = -1;
                        for (int i = 0; i < msg.fileName.length(); i++) {
                            if (msg.fileName[i] == '/') { slashPos = i; }
                        }
                        msg.caption = (slashPos >= 0)
                            ? msg.fileName.mid(slashPos + 1) : msg.fileName;
                    } else if (hm.msgtype == "image") {
                        msg.etype       = ChatElement::Image;
                        msg.caption     = qFromUtf8(hm.message);
                        msg.messageText = qFromUtf8(hm.message);
                        msg.mediaWidth  = hm.mediaWidth;
                        msg.mediaHeight = hm.mediaHeight;
                        msg.mediaUrl    = qFromUtf8(hm.mediaUrl);
                        msg.fileSize    = hm.fileSize;
                    } else if (hm.msgtype == "video") {
                        msg.etype       = ChatElement::Video;
                        msg.caption     = qFromUtf8(hm.message);
                        msg.messageText = qFromUtf8(hm.message);
                        msg.mediaWidth  = hm.mediaWidth;
                        msg.mediaHeight = hm.mediaHeight;
                        msg.durationSec = hm.duration / 1000;
                        msg.mediaUrl    = qFromUtf8(hm.mediaUrl);
                        msg.fileSize    = hm.fileSize;
                    } else if (hm.msgtype == "audio") {
                        msg.etype       = ChatElement::Audio;
                        msg.caption     = qFromUtf8(hm.message);
                        msg.messageText = qFromUtf8(hm.message);
                        msg.durationSec = hm.duration / 1000;
                        msg.mediaUrl    = qFromUtf8(hm.mediaUrl);
                        msg.fileSize    = hm.fileSize;
                    } else if (hm.msgtype == "file") {
                        msg.etype       = ChatElement::File;
                        msg.fileName    = qFromUtf8(hm.message);
                        msg.caption     = qFromUtf8(hm.message);
                        msg.mediaUrl    = qFromUtf8(hm.mediaUrl);
                        msg.fileSize    = hm.fileSize;
                    }
                
                    qWarning("Cache PUSH to %s %d: sender=%s",
                             chatType.c_str(), chatId, qToUtf8(msg.senderName).data());
                    contactListWidget->updateContactLastMessage(
                        chatId, qFromUtf8(chatType), msg.messageText, timenowhm());
                    if (currentChatId == chatId && currentChatType == qFromUtf8(chatType)) {
                        chatWidget->appendMessage(msg);
                        int newIdx = chatWidget->messageCount() - 1;
                        if (!hm.mediaUrl.empty() && hm.msgtype != "file") {
                            QString mxc = qFromUtf8(hm.mediaUrl);
                            ChatElement& el = chatWidget->mutableMessageAt(newIdx);

                            QByteArray rawBytes = MediaShmemCache::inst().getThumb(mxc);
                            if (!rawBytes.isEmpty()) {
                                el.scaledDisplay = decodeRawToThumb(rawBytes.data(), rawBytes.size(),
                                    el.mediaWidth, el.mediaHeight, chatWidget->width() * 70 / 100);
                                el.downloadState = ChatElement::Completed;
                            } else {
                                auto dbData = Storage::instance().cacheDb()->loadMedia(
                                    mediaCacheKey("file", mxc).c_str());
                                if (!dbData.empty()) {
                                    MediaShmemCache::inst().putThumb(mxc, (const char*)dbData.data(), dbData.size());
                                    el.scaledDisplay = decodeRawToThumb((const char*)dbData.data(), dbData.size(),
                                        el.mediaWidth, el.mediaHeight, chatWidget->width() * 70 / 100);
                                    el.downloadState = ChatElement::Completed;
                                } else {
                                    el.downloadState = ChatElement::InProgress;
                                    ToxAPI::downloadMedia(chatId, chatType, newIdx, hm.mediaUrl);
                                }
                            }
                        }
                    } else {
                        m_messageCache[{chatId, chatType}].push_back(msg);
                        contactListWidget->incrementUnread(chatId, qFromUtf8(chatType));
                    }

                    // 声音通知
                    if (!qIsAppActive()) {
                        std::vector<std::string> names;
                        QString self = selfInfoWidget->selfName();
                        if (!self.isEmpty())
                            names.push_back(qToUtf8(self).data());
                        if (unknownShouldPlaySound(chatType, hm.message, names))
                            playNotificationSound();
                    }
                }
            } else if (pr.handled && pr.contactName == "reddit") {
                ChatElement msg;
                msg.category = "other";
                msg.time = getCurrentTime();
                msg.messageText = pr.messageText;
                msg.senderName = pr.senderName;
                msg.peerNumber = VIRTUAL_REDDIT_ID;
                qWarning("Cache PUSH to reddit %d: sender=%s",
                         VIRTUAL_REDDIT_ID, qToUtf8(msg.senderName).data());
                m_messageCache[{VIRTUAL_REDDIT_ID, kTopicType}].push_back(msg);
                contactListWidget->updateContactLastMessage(
                    VIRTUAL_REDDIT_ID, kTopicType, msg.messageText, timenowhm());
                if (currentChatId == VIRTUAL_REDDIT_ID && currentChatType == kTopicType) {
                    chatWidget->appendMessage(msg);
                } else {
                    contactListWidget->incrementUnread(VIRTUAL_REDDIT_ID, kTopicType);
                }
            } else {
                ChatElement msg;
                msg.category = "other";
                msg.time = getCurrentTime();
                if (pr.handled) {
                    msg.messageText = pr.messageText;
                    msg.senderName = pr.senderName;
                } else {
                    msg.messageText = qFromUtf8("[" + e.type + "]\n" + e.data);
                    msg.senderName = "Unknown";
                }
                msg.peerNumber = VIRTUAL_UNKNOWN_ID;
                qWarning("Cache PUSH to unknown %d: type=%s, handled=%d, sender=%s",
                         VIRTUAL_UNKNOWN_ID, e.type.c_str(), pr.handled, qToUtf8(msg.senderName).data());
                m_messageCache[{VIRTUAL_UNKNOWN_ID, kUnknownType}].push_back(msg);
                contactListWidget->updateContactLastMessage(
                    VIRTUAL_UNKNOWN_ID, kUnknownType, msg.messageText, timenowhm());
                if (currentChatId == VIRTUAL_UNKNOWN_ID && currentChatType == kUnknownType) {
                    chatWidget->appendMessage(msg);
                } else {
                    contactListWidget->incrementUnread(VIRTUAL_UNKNOWN_ID, kUnknownType);
                }
            }
        }
    }
    contactListWidget->endBatch();
}

void MainWindow::onLanguageChanged(const QString& langCode) {
    saveLanguage(langCode);
    Translator::instance().loadLanguage(langCode);
    QtappSetup::installQtTranslations(langCode);
}

void MainWindow::retranslateUi() {
    qSetWindowTitle(this, _("app_title"));
    
    // 更新子控件
    if (selfInfoWidget) { selfInfoWidget->retranslateUi(); }
    if (contactListWidget) { contactListWidget->retranslateUi(); }
    if (chatWidget) { chatWidget->retranslateUi(); }
    
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
        } else if (currentChatType == kUnknownType) {
            headerText = QString(_("unknown")) + " " + QString::number(currentChatId);
        } else if (currentChatType == kSyseventType) {
            headerText = QString("System Events") + " " + QString::number(currentChatId);
        }
        chatWidget->setHeaderText(headerText);
    }
}

void MainWindow::onViewInfoRequested(int id, const QString& type) {
    FriendInfoDialog dialog(this);
    
    if (type == "friend") {
        std::string key = "friend_" + std::to_string(id);
        auto it = peerInfoMap.find(key);
        if (it == peerInfoMap.end()) {
            PeerKey pk = parsePeerKey(key);
            if (pk.valid) {
                auto row = Storage::instance().channelDb()->get_chan_peer(
                    pk.chanid.c_str(), pk.peerNum);
                it = loadRowToMap(peerInfoMap, key, std::move(row));
            }
        }
        if (it != peerInfoMap.end()) {
            if (it->second.statusText.empty())
                ToxAPI::lazyLoadFriendDetail(id);
            dialog.setInfo(friendInfoFromPeer(it->second, id));
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
                chatId = qFromUtf8(conferences[i].chatId);
                statusText = qFromUtf8(conferences[i].statusText);
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
                chatId = qFromUtf8(groups[i].chatId);
                statusText = qFromUtf8(groups[i].statusText);
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
        _("rename.current_nick") + QString(": ") + qFromUtf8(displayName), &dialog);
    layout->addWidget(currentLabel);

    // Use global nick
    QRadioButton* selfRadio = new QRadioButton(
        _("rename.use_self_nick") + QString(" (%1)").arg(qFromUtf8(globalName)), &dialog);
    selfRadio->setChecked(true);
    layout->addWidget(selfRadio);

    // Custom nick
    QRadioButton* customRadio = new QRadioButton(_("rename.custom_nick"), &dialog);
    layout->addWidget(customRadio);

    PlaceholderLineEdit* nameEdit = new PlaceholderLineEdit(_("rename.enter_nick"), &dialog);
    nameEdit->setText(qFromUtf8(currentGroupNick));
    nameEdit->setEnabled(false);
    layout->addWidget(nameEdit);

    // Random nick
    QRadioButton* randomRadio = new QRadioButton(_("rename.random_nick"), &dialog);
    layout->addWidget(randomRadio);

    QLabel* randomLabel = new QLabel(
        qFromUtf8(ToxAPI::getRandomNameSync()), &dialog);
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
                                  QLineEdit::Normal, qFromUtf8(currentTopic), &ok, this);
#else
    topic = QInputDialog::getText(this, _("set_topic.title"), _("set_topic.prompt"),
                                  QLineEdit::Normal, qFromUtf8(currentTopic), &ok);
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
                                  QLineEdit::Normal, qFromUtf8(currentTitle), &ok, this);
#else
    title = QInputDialog::getText(this, _("set_title.title"), _("set_title.prompt"),
                                  QLineEdit::Normal, qFromUtf8(currentTitle), &ok);
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
            displayName = qFromUtf8(conf.conferenceName);
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
        if (confId == -1) { return; }
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
            displayName = qFromUtf8(grp.groupName);
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
        bool isSelf  = (msg.sender_pubkey == selfPubkey);
        QString senderLabel;
        QString senderNickname;
        QString ipAddress;
        QString avatarUrl;
        
        if (isSelf) {
            senderLabel = "Me";
        } else {
            if (currentChatType == "friend") {
                std::string key = "friend_" + std::to_string(currentChatId);
                auto it = peerInfoMap.find(key);
                if (it == peerInfoMap.end()) {
                    PeerKey pk = parsePeerKey(key);
                    if (pk.valid) {
                        auto row = Storage::instance().channelDb()->get_chan_peer(
                            pk.chanid.c_str(), pk.peerNum);
                        it = loadRowToMap(peerInfoMap, key, std::move(row));
                    }
                }
                if (it != peerInfoMap.end()) {
                    senderLabel = qFromUtf8(it->second.name);
                    if (!it->second.nickname.empty()) {
                        QString n = qFromUtf8(it->second.nickname);
                        senderNickname = n;
                    }
                    avatarUrl = qFromUtf8(it->second.iconUrl);
                }
            } else if (currentChatType == "unknown") {
                std::string key = "unknown_" + msg.sender_pubkey;
                auto it = peerInfoMap.find(key);
                if (it == peerInfoMap.end()) {
                    PeerKey pk = parsePeerKey(key);
                    if (pk.valid) {
                        auto row = Storage::instance().channelDb()->get_chan_peer(
                            pk.chanid.c_str(), pk.peerNum);
                        it = loadRowToMap(peerInfoMap, key, std::move(row));
                    }
                }
                if (it != peerInfoMap.end()) {
                    senderLabel = qFromUtf8(it->second.name);
                    if (!it->second.nickname.empty()) {
                        QString n = qFromUtf8(it->second.nickname);
                        senderNickname = n;
                    }
                    ipAddress = qFromUtf8(it->second.peerIp);
                    avatarUrl = qFromUtf8(it->second.iconUrl);
                }
            } else {
                std::string key = std::string(qToUtf8(currentChatType).data())
                    + "_" + std::to_string(currentChatId)
                    + "_" + std::to_string(msg.sender_number);
                auto it = peerInfoMap.find(key);
                if (it == peerInfoMap.end()) {
                    PeerKey pk = parsePeerKey(key);
                    if (pk.valid) {
                        auto row = Storage::instance().channelDb()->get_chan_peer(
                            pk.chanid.c_str(), pk.peerNum);
                        it = loadRowToMap(peerInfoMap, key, std::move(row));
                    }
                }
                if (it != peerInfoMap.end()) {
                    senderLabel = qFromUtf8(it->second.name);
                    if (!it->second.nickname.empty()) {
                        QString n = qFromUtf8(it->second.nickname);
                        senderNickname = n;
                    }
                    ipAddress = qFromUtf8(it->second.peerIp);
                    avatarUrl = qFromUtf8(it->second.iconUrl);
                }
            }
        }
        
        QString timeStr = qFormatTime(qFromUtf8(msg.created_at));
        
        ChatElement el;
        el.messageText    = qFromUtf8(msg.message);
        el.category       = isSelf ? "self" : "other";
        el.senderName     = senderLabel;
        el.senderNickname = senderNickname;
        el.peerNumber     = isSelf ? -1 : (currentChatType == "friend" ? currentChatId : (int)msg.sender_number);
        el.time           = timeStr;
        el.avatarUrl      = avatarUrl;
        el.ipAddress      = ipAddress;

        if (!msg.mediaUrl.empty()) {
            el.mediaUrl    = qFromUtf8(msg.mediaUrl);
            el.mediaWidth  = msg.mediaWidth;
            el.mediaHeight = msg.mediaHeight;
            el.fileSize    = msg.fileSize;
        }

        if (msg.msgtype == "image") {
            el.etype = ChatElement::Image;
        } else if (msg.msgtype == "video") {
            el.etype       = ChatElement::Video;
            el.durationSec = msg.duration / 1000;
        } else if (msg.msgtype == "audio") {
            el.etype       = ChatElement::Audio;
            el.durationSec = msg.duration / 1000;
        } else if (msg.msgtype == "file") {
            el.etype = ChatElement::File;
        }

        chatWidget->appendMessage(el);
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

void MainWindow::onTranslateForSendRequested(const QString& text, const QString& targetLang) {
    ToxAPI::translateForSend(std::string(qToUtf8(text)), std::string(qToUtf8(targetLang)));
}

void MainWindow::onRetryClicked(int msgIndex, const QString& mediaUrl, const QString& /*source*/) {
    if (msgIndex < 0 || msgIndex >= chatWidget->messageCount()) { return; }
    if (currentChatId < 0) { return; }

    ChatElement& el = chatWidget->mutableMessageAt(msgIndex);
    QByteArray rawBytes = MediaShmemCache::inst().getThumb(mediaUrl);
    if (!rawBytes.isEmpty()) {
        el.scaledDisplay = decodeRawToThumb(rawBytes.data(), rawBytes.size(),
            el.mediaWidth, el.mediaHeight, chatWidget->width() * 70 / 100);
        el.downloadState = ChatElement::Completed;
        chatWidget->triggerRelayout(msgIndex);
        return;
    }
    auto dbData = Storage::instance().cacheDb()->loadMedia(
        mediaCacheKey("file", mediaUrl).c_str());
    if (!dbData.empty()) {
        MediaShmemCache::inst().putThumb(mediaUrl, (const char*)dbData.data(), dbData.size());
        el.scaledDisplay = decodeRawToThumb((const char*)dbData.data(), dbData.size(),
            el.mediaWidth, el.mediaHeight, chatWidget->width() * 70 / 100);
        el.downloadState = ChatElement::Completed;
        chatWidget->triggerRelayout(msgIndex);
        return;
    }

    chatWidget->triggerRelayout(msgIndex);
    el.downloadState = ChatElement::InProgress;
    ToxAPI::downloadMedia(currentChatId, std::string(qToUtf8(currentChatType).data()),
                          msgIndex, std::string(qToUtf8(mediaUrl).data()));
}

void MainWindow::onOpenFullSizeImage(int msgIndex, const QString& mediaUrl) {
    if (msgIndex < 0 || msgIndex >= chatWidget->messageCount()) { return; }
    std::string key = mediaCacheKey("file", mediaUrl);
    Storage::instance().cacheDbAsync()->loadMedia(
        key,
        [this, msgIndex, mediaUrl](std::vector<uint8_t> data, std::string) {
            auto* ev = new DiskLoadEvent();
            ev->msgIndex = msgIndex;
            ev->mediaUrl = std::string(qToUtf8(mediaUrl).data());
            if (data.empty()) {
                ev->success = false;
                ev->errorInfo = "cache miss";
            } else {
                ev->success = true;
                ev->rawData = std::move(data);
            }
            QApplication::postEvent(this, ev);
        });
}

void MainWindow::onSourceClicked(int msgIndex) {
    if (msgIndex < 0 || msgIndex >= chatWidget->messageCount()) { return; }

    ChatElement msg = chatWidget->messageAt(msgIndex);
    QDialog* dlg = new QDialog(this);
    dlg->resize(650, 520);
    qSetWindowTitle(dlg, qFromUtf8("Source"));
    QBoxLayout* lay = qNewBoxLayout(dlg, QBoxLayout::TopToBottom, 0, 4);
    qSetMargins(lay, 4, 4, 4, 4);

    JsonViewWidget* jv = new JsonViewWidget(dlg);
    jv->setJson(msg.messageText);
    lay->addWidget(jv, 1);

    QPushButton* closeBtn = new QPushButton(_("buttons.close"), dlg);
    connect(closeBtn, SIGNAL(clicked()), dlg, SLOT(accept()));
    lay->addWidget(closeBtn, 0, Qt::AlignRight);

#ifdef QT3_BUILD
    // QDialog::finished(int) 在 Qt3 不存在；dialog 是 MainWindow 子对象，自动清理
#else
    QObject::connect(dlg, SIGNAL(finished(int)), dlg, SLOT(deleteLater()));
#endif
    dlg->show();
}

void MainWindow::openSettings() {
    ConfigDialog* dlg = new ConfigDialog(qFromUtf8("设置"), this);
    dlg->setSettingsFile(ConfigDialog::defaultSettingsFile());

    CategoryPage* general = new CategoryPage(qFromUtf8("常规"), dlg);
    StringConfigItem* serverUrl = new StringConfigItem(
        "server/baseUrl", "http://localhost:8181",
        qFromUtf8("服务器地址"), general);
    general->addLabeledControl(serverUrl->label(), serverUrl->lineEdit());
    general->addStretch();
    dlg->addCategory(qFromUtf8("常规"), general);
    dlg->registerConfigItem(serverUrl);

    CategoryPage* appearance = new CategoryPage(qFromUtf8("界面"), dlg);
    QStringList langs;
    langs << qFromUtf8("zh-CN") << qFromUtf8("zh-TW") << qFromUtf8("en-US");
    SelectConfigItem* langItem = new SelectConfigItem(
        "lang/code", "zh-CN",
        qFromUtf8("语言"), langs, appearance);
    appearance->addLabeledControl(langItem->label(), langItem->comboBox());
    appearance->addStretch();
    dlg->addCategory(qFromUtf8("界面"), appearance);
    dlg->registerConfigItem(langItem);

    CategoryPage* notify = new CategoryPage(qFromUtf8("通知"), dlg);
    BoolConfigItem* msgNotif = new BoolConfigItem(
        "notify/enabled", true,
        qFromUtf8("消息通知"), notify);
    notify->addWidget(msgNotif->checkBox());
    BoolConfigItem* soundNotif = new BoolConfigItem(
        "notify/sound", true,
        qFromUtf8("声音"), notify);
    notify->addWidget(soundNotif->checkBox());
    notify->addStretch();
    dlg->addCategory(qFromUtf8("通知"), notify);
    dlg->registerConfigItem(msgNotif);
    dlg->registerConfigItem(soundNotif);

    CategoryPage* chat = new CategoryPage(qFromUtf8("聊天"), dlg);
    IntConfigItem* pageSize = new IntConfigItem(
        "chat/pageSize", 200,
        qFromUtf8("每页消息数"), 50, 500, chat);
    chat->addLabeledControl(pageSize->label(), pageSize->spinBox());
    chat->addStretch();
    dlg->addCategory(qFromUtf8("聊天"), chat);
    dlg->registerConfigItem(pageSize);

    // --- 网络 ---
    CategoryPage* network = new CategoryPage(qFromUtf8("网络"), dlg);
    IntConfigItem* timeout = new IntConfigItem(
        "server/timeout", 30,
        qFromUtf8("服务器超时(秒)"), 5, 120, network);
    network->addLabeledControl(timeout->label(), timeout->spinBox());
    IntConfigItem* retryInterval = new IntConfigItem(
        "server/retryInterval", 5,
        qFromUtf8("连接重试间隔(秒)"), 1, 60, network);
    network->addLabeledControl(retryInterval->label(), retryInterval->spinBox());
    IntConfigItem* httpParallel = new IntConfigItem(
        "http_request_parral", 1,
        qFromUtf8("HTTP请求并行数"), 1, 5, network);
    network->addLabeledControl(httpParallel->label(), httpParallel->spinBox());
    network->addStretch();
    dlg->addCategory(qFromUtf8("网络"), network);
    dlg->registerConfigItem(timeout);
    dlg->registerConfigItem(retryInterval);
    dlg->registerConfigItem(httpParallel);

    // --- 其他 ---
    CategoryPage* other = new CategoryPage(qFromUtf8("其他"), dlg);
    QStringList logLevels;
    logLevels << qFromUtf8("debug") << qFromUtf8("info")
              << qFromUtf8("warning") << qFromUtf8("error");
    SelectConfigItem* logLevel = new SelectConfigItem(
        "log/level", "info",
        qFromUtf8("日志级别"), logLevels, other);
    other->addLabeledControl(logLevel->label(), logLevel->comboBox());
    BoolConfigItem* autoConnect = new BoolConfigItem(
        "startup/autoConnect", true,
        qFromUtf8("启动时自动连接"), other);
    other->addWidget(autoConnect->checkBox());
    BoolConfigItem* sortbylastmsg = new BoolConfigItem(
        "sortbylastmsg", false,
        qFromUtf8("联系人排序最新消息优先"), other);
    other->addWidget(sortbylastmsg->checkBox());
    QStringList toLangs;
    toLangs << qFromUtf8("zh-CN") << qFromUtf8("zh-TW") << qFromUtf8("en-US");
    SelectConfigItem* translateToLang = new SelectConfigItem(
        "translate/tolang", "zh-CN",
        qFromUtf8("翻译目标语言"), toLangs, other);
    other->addLabeledControl(translateToLang->label(), translateToLang->comboBox());
    IntConfigItem* fontSize = new IntConfigItem(
        "chat/fontSize", 14,
        qFromUtf8("消息字体大小"), 8, 32, other);
    other->addLabeledControl(fontSize->label(), fontSize->spinBox());

    // ── 富媒体显示 ──
    BoolConfigItem* dispImg = new BoolConfigItem(
        "media/displayImage", true,
        qFromUtf8("显示图片"), other);
    other->addWidget(dispImg->checkBox());
    BoolConfigItem* dispFile = new BoolConfigItem(
        "media/displayFile", false,
        qFromUtf8("显示文件"), other);
    other->addWidget(dispFile->checkBox());
    BoolConfigItem* dispGif = new BoolConfigItem(
        "media/displayGif", true,
        qFromUtf8("显示GIF"), other);
    other->addWidget(dispGif->checkBox());
    BoolConfigItem* dispVid = new BoolConfigItem(
        "media/displayVideo", false,
        qFromUtf8("显示视频"), other);
    other->addWidget(dispVid->checkBox());

    // ── 富媒体自动下载 ──
    BoolConfigItem* dlImg = new BoolConfigItem(
        "media/downloadImage", true,
        qFromUtf8("自动下载图片"), other);
    other->addWidget(dlImg->checkBox());
    BoolConfigItem* dlFile = new BoolConfigItem(
        "media/downloadFile", false,
        qFromUtf8("自动下载文件"), other);
    other->addWidget(dlFile->checkBox());
    BoolConfigItem* dlGif = new BoolConfigItem(
        "media/downloadGif", true,
        qFromUtf8("自动下载GIF"), other);
    other->addWidget(dlGif->checkBox());
    BoolConfigItem* dlVid = new BoolConfigItem(
        "media/downloadVideo", false,
        qFromUtf8("自动下载视频"), other);
    other->addWidget(dlVid->checkBox());

    other->addStretch();
    dlg->addCategory(qFromUtf8("其他"), other);
    dlg->registerConfigItem(logLevel);
    dlg->registerConfigItem(autoConnect);
    dlg->registerConfigItem(sortbylastmsg);
    dlg->registerConfigItem(translateToLang);
    dlg->registerConfigItem(fontSize);
    dlg->registerConfigItem(dispImg);
    dlg->registerConfigItem(dispFile);
    dlg->registerConfigItem(dispGif);
    dlg->registerConfigItem(dispVid);
    dlg->registerConfigItem(dlImg);
    dlg->registerConfigItem(dlFile);
    dlg->registerConfigItem(dlGif);
    dlg->registerConfigItem(dlVid);

    connect(dlg, SIGNAL(settingsSaved(SettingsChangedMap)), this, SLOT(onSettingsSaved(SettingsChangedMap)));
    dlg->loadSettings();
    dlg->show();
}

void MainWindow::onSettingsSaved(const SettingsChangedMap& changed) {
    if (changed.isEmpty()) { return; }
    qWarning("Settings changed (%d keys):", changed.size());
    for (auto it = changed.begin(); it != changed.end(); ++it) {
#ifdef QT3_BUILD
        qWarning("  %s => %s", (const char*)it.key().utf8(),
                 (const char*)it.data().toString().utf8());
#else
        qWarning("  %s => %s", qPrintable(it.key()),
                 qPrintable(it.value().toString()));
#endif
    }
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
