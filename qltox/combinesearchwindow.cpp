#include "combinesearchwindow.h"
#include "translator.h"
#include "compat34.h"
#include "placeholderlineedit.h"
#include "storage.h"
#include "channel_db.h"
#include "message_db.h"
#include "eventpoller.h"
#include <qlayout.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qstringlist.h>
#ifdef QT3_BUILD
#include <qevent.h>
#else
#include <QKeyEvent>
#endif
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

static const int VIRTUAL_SEARCH_UNKNOWN_ID = -100;
static const int VIRTUAL_SEARCH_SYSEVENT_ID = -101;
static const int VIRTUAL_SEARCH_REDDIT_ID = -102;
static const int VIRTUAL_SEARCH_BOOKMARK_ID = -103;

namespace {

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

QLabel* sectionTitle(const QString& text, QWidget* host) {
    QLabel* l = new QLabel(text, host);
    QFont f = l->font();
    f.setBold(true);
    l->setFont(f);
#ifdef QT3_BUILD
    l->setPaletteForegroundColor(QColor(0x40, 0x40, 0x40));
#else
    l->setStyleSheet("color:#404040");
#endif
    return l;
}

QString typeLabel(const std::string& type) {
    if (type == "friend") { return _("friend"); }
    if (type == "conference") { return _("conference_item"); }
    if (type == "group") { return _("tabs.groups"); }
    if (type == kUnknownType) { return qFromUtf8("未知"); }
    if (type == kSyseventType) { return qFromUtf8("系统事件"); }
    if (type == kTopicType) { return qFromUtf8("主题"); }
    if (type == kBookmarkType) { return qFromUtf8("书签"); }
    return qFromUtf8(type.c_str());
}

QString trimStr(const QString& s) {
#ifdef QT3_BUILD
    return s.stripWhiteSpace();
#else
    return s.trimmed();
#endif
}

struct VirtualSeed {
    int id;
    const char* name;
    const char* type;
};

const VirtualSeed kVirtualSeeds[] = {
    { VIRTUAL_SEARCH_UNKNOWN_ID,  "Unknown",  kUnknownType },
    { VIRTUAL_SEARCH_SYSEVENT_ID, "Sysevent", kSyseventType },
    { VIRTUAL_SEARCH_REDDIT_ID,   "Reddit",   kTopicType },
    { VIRTUAL_SEARCH_BOOKMARK_ID, "Bookmark", kBookmarkType },
};

}  // namespace

CombineSearch::CombineSearch(QWidget* parent)
    : QDialog(parent
#ifdef QT3_BUILD
          , nullptr, false, WDestructiveClose
#endif
          ) {
#ifndef QT3_BUILD
    setAttribute(Qt::WA_DeleteOnClose);
#endif
    qSetWindowTitle(this, _("combine_search.title"));
    resize(600, 500);
    setMinimumSize(600, 400);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setMargin(8);
    root->setSpacing(6);

    QHBoxLayout* searchRow = new QHBoxLayout;
    m_input = new PlaceholderLineEdit(_("combine_search.placeholder"), this);
    connect(m_input, SIGNAL(returnPressed()), this, SLOT(runSearch()));
    m_searchBtn = new QPushButton(_("combine_search.button"), this);
    connect(m_searchBtn, SIGNAL(clicked()), this, SLOT(runSearch()));
    searchRow->addWidget(m_input, 1);
    searchRow->addWidget(m_searchBtn);
    root->addLayout(searchRow);

    QHBoxLayout* tabRow = new QHBoxLayout;
    tabRow->setSpacing(0);
    m_tabContacts = new QPushButton(_("combine_search.tabs.contacts"), this);
    m_tabMessages = new QPushButton(_("combine_search.tabs.messages"), this);
    QPushButton* tabs[2] = { m_tabContacts, m_tabMessages };
    for (int i = 0; i < 2; ++i) {
        qSetCheckable(tabs[i], true);
        tabs[i]->setFixedHeight(26);
        connect(tabs[i], SIGNAL(clicked()), this, SLOT(onTabClicked()));
        tabRow->addWidget(tabs[i]);
        m_tabButtons.push_back(tabs[i]);
    }
    tabRow->addStretch(1);
    qSetChecked(m_tabContacts, true);
    root->addLayout(tabRow);

    m_stack = new StackedWidget(this);
    QWidget* contactsPage = new QWidget(m_stack);
    QWidget* messagesPage = new QWidget(m_stack);

    QWidget* inner = nullptr;
    ScrollArea* scroll = makeScrollArea(contactsPage, inner);
    (new QVBoxLayout(inner))->addStretch(1);
    (new QVBoxLayout(contactsPage))->addWidget(scroll);
    m_contactsInner = inner;
    m_pages.push_back(contactsPage);
    m_stack->addWidget(contactsPage);

    inner = nullptr;
    scroll = makeScrollArea(messagesPage, inner);
    (new QVBoxLayout(inner))->addStretch(1);
    (new QVBoxLayout(messagesPage))->addWidget(scroll);
    m_messagesInner = inner;
    m_pages.push_back(messagesPage);
    m_stack->addWidget(messagesPage);

    root->addWidget(m_stack, 10);
    qStackSetCurrent(m_stack, contactsPage);

    QHBoxLayout* pagerRow = new QHBoxLayout;
    m_firstBtn = new QPushButton(_("combine_search.page_first"), this);
    m_prevBtn = new QPushButton(_("combine_search.page_prev"), this);
    m_nextBtn = new QPushButton(_("combine_search.page_next"), this);
    m_lastBtn = new QPushButton(_("combine_search.page_last"), this);
    connect(m_firstBtn, SIGNAL(clicked()), this, SLOT(goFirst()));
    connect(m_prevBtn, SIGNAL(clicked()), this, SLOT(goPrev()));
    connect(m_nextBtn, SIGNAL(clicked()), this, SLOT(goNext()));
    connect(m_lastBtn, SIGNAL(clicked()), this, SLOT(goLast()));
    m_pageLabel = new QLabel(this);
    pagerRow->addStretch(1);
    pagerRow->addWidget(m_firstBtn);
    pagerRow->addWidget(m_prevBtn);
    pagerRow->addWidget(m_pageLabel);
    pagerRow->addWidget(m_nextBtn);
    pagerRow->addWidget(m_lastBtn);
    pagerRow->addStretch(1);
    root->addLayout(pagerRow);

    m_firstBtn->setEnabled(false);
    m_prevBtn->setEnabled(false);
    showPage(0, 0);
}

void CombineSearch::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QDialog::keyPressEvent(e);
}

int CombineSearch::activeTab() const {
#ifdef QT3_BUILD
    return m_tabMessages->isOn() ? 1 : 0;
#else
    return m_tabMessages->isChecked() ? 1 : 0;
#endif
}

void CombineSearch::onTabClicked() {
    int idx = activeTab();
    qStackSetCurrent(m_stack, m_pages[idx]);
    showPage(idx, m_curPage[idx]);
}

int CombineSearch::rowTotal(int tab) const {
    const std::vector<RowInfo>& rows = (tab == 0) ? m_contactRows : m_messageRows;
    if (rows.empty()) { return 1; }
    return ((int)rows.size() + 49) / 50;
}

void CombineSearch::renderSlice(QWidget* inner, const std::vector<RowInfo>& rows, int page,
                                const QString& emptyText) {
    QVBoxLayout* lay = static_cast<QVBoxLayout*>(inner->layout());
    clearLayout(lay);

    int total = rows.empty() ? 1 : ((int)rows.size() + 49) / 50;
    if (page < 0) { page = 0; }
    if (page >= total) { page = total - 1; }

    if (rows.empty()) {
        lay->addWidget(new QLabel(emptyText, inner));
        lay->addStretch(1);
    } else {
        int off = page * 50;
        int end = std::min(off + 50, (int)rows.size());
        for (int i = off; i < end; ++i) {
            lay->addWidget(makeRow(rows[i].title, rows[i].detail, inner));
        }
        lay->addStretch(1);
    }

    m_pageLabel->setText(_A("combine_search.page_indicator",
                            QStringList() << QString::number(page + 1)
                                          << QString::number(total)));
    m_firstBtn->setEnabled(page > 0);
    m_prevBtn->setEnabled(page > 0);
    m_nextBtn->setEnabled(page < total - 1);
    m_lastBtn->setEnabled(page < total - 1);
}

void CombineSearch::showPage(int tab, int page) {
    if (tab == 0) {
        renderSlice(m_contactsInner, m_contactRows, page, _("combine_search.no_contacts"));
    } else {
        renderSlice(m_messagesInner, m_messageRows, page, _("combine_search.no_messages"));
    }
}

void CombineSearch::goFirst() {
    int tab = activeTab();
    m_curPage[tab] = 0;
    showPage(tab, 0);
}

void CombineSearch::goPrev() {
    int tab = activeTab();
    if (m_curPage[tab] <= 0) { return; }
    --m_curPage[tab];
    showPage(tab, m_curPage[tab]);
}

void CombineSearch::goNext() {
    int tab = activeTab();
    if (m_curPage[tab] + 1 >= rowTotal(tab)) { return; }
    ++m_curPage[tab];
    showPage(tab, m_curPage[tab]);
}

void CombineSearch::goLast() {
    int tab = activeTab();
    m_curPage[tab] = rowTotal(tab) - 1;
    showPage(tab, m_curPage[tab]);
}

QWidget* CombineSearch::makeRow(const QString& title, const QString& detail, QWidget* host) {
    QWidget* row = new QWidget(host);
    QVBoxLayout* lay = new QVBoxLayout(row);
    lay->setMargin(6);
    lay->setSpacing(2);

    QLabel* t = new QLabel(qElideChars(title, 80, ElideRight), row);
    QFont f = t->font();
    f.setBold(true);
    t->setFont(f);

    QLabel* d = new QLabel(qElideChars(detail, 120, ElideRight), row);
#ifdef QT3_BUILD
    d->setPaletteForegroundColor(QColor(0x70, 0x70, 0x70));
#else
    d->setStyleSheet("color:#707070");
#endif

    lay->addWidget(t);
    lay->addWidget(d);
    return row;
}

void CombineSearch::clearLayout(QLayout* lay) {
    if (!lay) { return; }
#ifdef QT3_BUILD
    QLayoutIterator it = lay->iterator();
    while (it.current()) {
        QLayoutItem* cur = it.current();
        QWidget* w = cur->widget();
        it.deleteCurrent();
        if (w) { delete w; }
    }
#else
    QLayoutItem* it;
    while ((it = lay->takeAt(0))) {
        if (it->widget()) { delete it->widget(); }
        delete it;
    }
#endif
}

void CombineSearch::runSearch() {
    QString query = trimStr(m_input->text());
    QString upperQ = qToUpper(query);
    m_contacts.clear();
    m_messages.clear();

    std::map<std::pair<int, std::string>, QString> nameMap;
    if (!query.isEmpty()) {
        {
            auto channels = Storage::instance().channelDb()->load_all_channels();
            for (const auto& row : channels) {
                auto sep = row.chanid.rfind('_');
                if (sep == std::string::npos) { continue; }
                std::string type = row.chanid.substr(0, sep);
                int id = 0;
                try {
                    id = std::stoi(row.chanid.substr(sep + 1));
                } catch (...) {
                    continue;
                }
                QString name = qFromUtf8(row.name.c_str());
                nameMap[std::make_pair(id, type)] = name;
                if (qToUpper(name).contains(upperQ)) {
                    ContactHit h;
                    h.id = id;
                    h.type = type;
                    h.name = name;
                    h.typeLabel = typeLabel(type);
                    m_contacts.push_back(h);
                }
            }
        }
        for (const VirtualSeed& s : kVirtualSeeds) {
            QString name = qFromUtf8(s.name);
            if (qToUpper(name).contains(upperQ)) {
                ContactHit h;
                h.id = s.id;
                h.type = s.type;
                h.name = name;
                h.typeLabel = typeLabel(s.type);
                m_contacts.push_back(h);
            }
        }

        std::vector<int64_t> ids = Storage::instance().messageDb()->search_messages(
            qToUtf8(query).data(), 100);
        for (int64_t rowid : ids) {
            std::unique_ptr<MessageRow> row = Storage::instance().messageDb()->get_message(rowid);
            if (!row) { continue; }
            auto sep = row->chanid.rfind('_');
            if (sep == std::string::npos) { continue; }
            std::string type = row->chanid.substr(0, sep);
            int id = 0;
            try {
                id = std::stoi(row->chanid.substr(sep + 1));
            } catch (...) {
                continue;
            }
            MessageHit h;
            h.id = id;
            h.type = type;
            std::map<std::pair<int, std::string>, QString>::const_iterator it =
                nameMap.find(std::make_pair(id, type));
            h.chanName = (it != nameMap.end()) ? it->second : qFromUtf8(row->chanid.c_str());
            h.sender = qFromUtf8(row->sender_name.c_str());
            if (h.sender.isEmpty()) { h.sender = qFromUtf8("未知"); }
            h.body = qFromUtf8(row->data.c_str());
            h.time = qFromUtf8(row->time_text.c_str());
            m_messages.push_back(h);
        }
    }

    m_contactRows.clear();
    for (const auto& c : m_contacts) {
        RowInfo r;
        r.title = c.name;
        r.detail = c.typeLabel;
        m_contactRows.push_back(r);
    }
    m_messageRows.clear();
    for (const auto& m : m_messages) {
        RowInfo r;
        r.title = qFromUtf8("[") + m.sender + qFromUtf8("] ") + m.body;
        r.detail = m.chanName + qFromUtf8(" · ") + m.time;
        m_messageRows.push_back(r);
    }

    m_curPage[0] = 0;
    m_curPage[1] = 0;
    showPage(activeTab(), 0);
}