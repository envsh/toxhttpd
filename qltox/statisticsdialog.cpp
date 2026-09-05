#include "statisticsdialog.h"
#include "storage.h"
#include "message_db.h"
#include "channel_db.h"
#include "pending_db.h"
#include "sticker_db.h"
#include "cache_db.h"

#include <qapplication.h>
#include <qclipboard.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <sys/stat.h>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int64_t fileSize(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) { return -1; }
    return (int64_t)st.st_size;
}

std::string groupNum(int64_t v) {
    char buf[32];
    snprintf(buf, sizeof buf, "%lld", (long long)v);
    std::string s(buf);
    int start = (s[0] == '-') ? 1 : 0;
    std::string out;
    int n = (int)s.size();
    for (int i = 0; i < n; ++i) {
        out += s[i];
        if (i >= start && i < n - 1 && (n - 1 - i) % 3 == 0) { out += ','; }
    }
    return out;
}

QString fmtBytes(int64_t b) {
    if (b < 0) { return QString("-"); }
    double v = (double)b;
    QString name;
    if (v < 1024.0) {
        name = qFromUtf8(groupNum(b).c_str()) + qFromUtf8(" B");
        return name;
    }
    static const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    int u = 0;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; ++u; }
    QString s;
    s += QString::number(v, 'f', 1);
    s += ' ';
    s += qFromUtf8(units[u]);
    s += "  (";
    s += qFromUtf8(groupNum(b).c_str());
    s += qFromUtf8(" 字节)");
    return s;
}

QString itemLine(const QString& name, const QString& value) {
    return name + qFromUtf8(": ") + value;
}

QString statValue(int i, const std::vector<int64_t>& vals) {
    if (i == 0 || i == 1) { return fmtBytes(vals[i]); }
    if (vals[i] < 0) { return QString("-"); }
    return qFromUtf8(groupNum(vals[i]).c_str());
}

QString wrapText(const QString& in, int maxCol) {
    if (in.isEmpty()) { return in; }
    QString out;
    int col = 0;
    for (int i = 0; i < in.length(); ++i) {
        QChar c = in.at(i);
        if (c == '\n') {
            out += c;
            col = 0;
            continue;
        }
        if (col >= maxCol) {
            if (c == ' ') { continue; }
            out += '\n';
            col = 0;
        }
        out += c;
        ++col;
    }
    return out;
}

ScrollArea* makeScrollArea(QWidget* page, QWidget*& inner) {
#ifdef QT3_BUILD
    QScrollView* scroll = new QScrollView(page);
#else
    QScrollArea* scroll = new QScrollArea(page);
#endif
    inner = new QWidget(scroll);
#ifdef QT3_BUILD
    scroll->addChild(inner);
    scroll->setResizePolicy(QScrollView::AutoOne);
#else
    scroll->setWidget(inner);
    scroll->setWidgetResizable(true);
#endif
    return scroll;
}

}  // namespace

StatisticsDialog::StatisticsDialog(QWidget* parent) : QDialog(parent) {
    qSetWindowTitle(this, qFromUtf8("统计"));
    resize(560, 480);

    QVBoxLayout* root = new QVBoxLayout(this);

    m_tabBar = new QWidget(this);
    QHBoxLayout* barLay = new QHBoxLayout(m_tabBar);
    barLay->setMargin(0);
    barLay->setSpacing(0);

    QString titles[3];
    titles[0] = qFromUtf8("总览");
    titles[1] = qFromUtf8("消息");
    titles[2] = qFromUtf8("缓存");

    for (int i = 0; i < 3; ++i) {
        QPushButton* btn = new QPushButton(titles[i], m_tabBar);
        btn->setFixedHeight(24);
        btn->setFlat(true);
        connect(btn, SIGNAL(clicked()), this, SLOT(onTabClicked()));
        barLay->addWidget(btn);
        m_tabButtons.push_back(btn);
    }
    barLay->addStretch(1);

    m_pageStack = new StackedWidget(this);

    QWidget* overview = new QWidget(m_pageStack);
    {
        QWidget* inner = nullptr;
        ScrollArea* scroll = makeScrollArea(overview, inner);
        buildOverviewPage(inner);
        (new QVBoxLayout(overview))->addWidget(scroll);
    }
    m_pageStack->addWidget(overview);
    m_pages.push_back(overview);

    for (int i = 1; i < 3; ++i) {
        QWidget* page = new QWidget(m_pageStack);
        QWidget* inner = nullptr;
        ScrollArea* scroll = makeScrollArea(page, inner);
        (new QVBoxLayout(inner))->addWidget(new QLabel(qFromUtf8("（待定）"), inner));
        (new QVBoxLayout(page))->addWidget(scroll);
        m_pageStack->addWidget(page);
        m_pages.push_back(page);
    }

    root->addWidget(m_tabBar);
    root->addWidget(m_pageStack, 10);

    qStackSetCurrent(m_pageStack, m_pages[0]);
}

void StatisticsDialog::buildOverviewPage(QWidget* inner) {
    QVBoxLayout* lay = new QVBoxLayout(inner);

    QHBoxLayout* dirRow = new QHBoxLayout;
    dirRow->addWidget(new QLabel(qFromUtf8("数据目录:"), inner));
    m_dirLabel = new QLabel(inner);
    qSetLabelSelectable(m_dirLabel);
    dirRow->addWidget(m_dirLabel, 1);
    lay->addLayout(dirRow);

    QHBoxLayout* btnRow = new QHBoxLayout;
    QPushButton* openBtn = new QPushButton(qFromUtf8("打开目录"), inner);
    connect(openBtn, SIGNAL(clicked()), this, SLOT(openDataDir()));
    QPushButton* calcBtn = new QPushButton(qFromUtf8("计算"), inner);
    connect(calcBtn, SIGNAL(clicked()), this, SLOT(refreshStats()));
    QPushButton* copyBtn = new QPushButton(qFromUtf8("复制结果"), inner);
    connect(copyBtn, SIGNAL(clicked()), this, SLOT(copyStats()));
    btnRow->addWidget(openBtn);
    btnRow->addWidget(calcBtn);
    btnRow->addWidget(copyBtn);
    btnRow->addStretch(1);
    lay->addLayout(btnRow);

    m_resultBox = new QWidget(inner);
    buildResultGrid(m_resultBox);
    lay->addWidget(m_resultBox, 1);

    m_dirLabel->setText(wrapText(dataDirText(), 90));
}

void StatisticsDialog::buildResultGrid(QWidget* host) {
    QGridLayout* grid = new QGridLayout(host);
    grid->setSpacing(6);
#ifdef QT3_BUILD
    grid->setColStretch(1, 1);
#else
    grid->setColumnStretch(1, 1);
#endif

    int row = 0;
    auto addHeader = [&](const QString& text) {
        QLabel* h = new QLabel(text, host);
        QFont f = h->font();
        f.setBold(true);
        h->setFont(f);
#ifdef QT3_BUILD
        grid->addMultiCellWidget(h, row, row, 0, 1);
#else
        grid->addWidget(h, row, 0, 1, 2);
#endif
        ++row;
    };
    auto addNameRow = [&](const QString& name) {
        QLabel* nl = new QLabel(name, host);
        nl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(nl, row, 0);
        QLabel* vl = new QLabel("-", host);
        vl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(vl, row, 1);
        m_valueLabels.push_back(vl);
        ++row;
    };

    addHeader(qFromUtf8("文件大小"));
    addNameRow(qFromUtf8("message.db"));
    addNameRow(qFromUtf8("cache.db"));

    addHeader(qFromUtf8("各表记录数 (message.db)"));
    addNameRow(qFromUtf8("messages"));
    addNameRow(qFromUtf8("reactions"));
    addNameRow(qFromUtf8("translations"));
    addNameRow(qFromUtf8("bookmarks"));
    addNameRow(qFromUtf8("channels"));
    addNameRow(qFromUtf8("peers"));
    addNameRow(qFromUtf8("未读消息合计"));
    addNameRow(qFromUtf8("pending_messages"));
    addNameRow(qFromUtf8("sticker_packs"));
    addNameRow(qFromUtf8("stickers"));

    addHeader(qFromUtf8("各表记录数 (cache.db)"));
    addNameRow(qFromUtf8("cache"));
    addNameRow(qFromUtf8("file_refs"));
}

QString StatisticsDialog::dataDirText() const {
    std::string dir = Storage::instance().dataDir();
    if (dir.empty()) { return qFromUtf8("(Storage 未初始化)"); }
    return qFromUtf8(dir.c_str());
}

void StatisticsDialog::openDataDir() {
    std::string dir = Storage::instance().dataDir();
    if (dir.empty()) { return; }
    qOpenUrl(QString("file://") + qFromUtf8(dir.c_str()));
}

void StatisticsDialog::copyStats() {
    QApplication::clipboard()->setText(m_resultText);
}

void StatisticsDialog::onTabClicked() {
    QPushButton* btn = (QPushButton*)sender();
    for (size_t i = 0; i < m_tabButtons.size(); ++i) {
        if (m_tabButtons[i] == btn) {
            qStackSetCurrent(m_pageStack, m_pages[i]);
            break;
        }
    }
}

void StatisticsDialog::refreshStats() {
    QString names[14];
    names[0] = qFromUtf8("message.db");
    names[1] = qFromUtf8("cache.db");
    names[2] = qFromUtf8("messages");
    names[3] = qFromUtf8("reactions");
    names[4] = qFromUtf8("translations");
    names[5] = qFromUtf8("bookmarks");
    names[6] = qFromUtf8("channels");
    names[7] = qFromUtf8("peers");
    names[8] = qFromUtf8("未读消息合计");
    names[9] = qFromUtf8("pending_messages");
    names[10] = qFromUtf8("sticker_packs");
    names[11] = qFromUtf8("stickers");
    names[12] = qFromUtf8("cache");
    names[13] = qFromUtf8("file_refs");

    std::vector<int64_t> vals(14, -1);
    std::string dir = Storage::instance().dataDir();

    if (!dir.empty()) {
        vals[0] = fileSize(dir + "/message.db");
        vals[1] = fileSize(dir + "/cache.db");
        if (MessageDbSyncInterface* db = Storage::instance().messageDb()) {
            vals[2] = db->countMessages();
            vals[3] = db->countReactions();
            vals[4] = db->countTranslations();
            vals[5] = db->countBookmarks();
        }
        if (ChannelDbSyncInterface* db = Storage::instance().channelDb()) {
            vals[6] = db->countChannels();
            vals[7] = db->countPeers();
            vals[8] = db->totalUnread();
        }
        if (PendingDbSyncInterface* db = Storage::instance().pendingDb()) {
            vals[9] = db->countPending();
        }
        if (StickerDbSyncInterface* db = Storage::instance().stickerDb()) {
            vals[10] = db->countPacks();
            vals[11] = db->count_stickers(nullptr);
        }
        if (CacheDbSyncInterface* db = Storage::instance().cacheDb()) {
            vals[12] = db->countCache();
            vals[13] = db->countFileRefs();
        }
    }

    QString t;
    if (dir.empty()) {
        t = qFromUtf8("(Storage 未初始化)");
    } else {
        t += qFromUtf8("文件大小:\n");
        t += itemLine(names[0], statValue(0, vals)) + "\n";
        t += itemLine(names[1], statValue(1, vals)) + "\n";
        t += qFromUtf8("\n各表记录数 (message.db):\n");
        for (int i = 2; i < 12; ++i) {
            t += itemLine(names[i], statValue(i, vals)) + "\n";
        }
        t += qFromUtf8("\n各表记录数 (cache.db):\n");
        for (int i = 12; i < 14; ++i) {
            t += itemLine(names[i], statValue(i, vals)) + "\n";
        }
    }
    m_resultText = t;

    for (int i = 0; i < 14; ++i) {
        m_valueLabels[i]->setText(dir.empty() ? QString("-") : statValue(i, vals));
    }
}