#include "contactlist.h"
#include "translator.h"
#include "compat34.h"
#include "restapi.h"
#include "placeholderlineedit.h"
#include "emojiutil.h"
#include "LimeStyle.h"
#include <qmessagebox.h>
#include <algorithm>
#include <qpainter.h>
#ifdef QT3_BUILD
#include <qregion.h>
#include <qbitmap.h>
#include <qevent.h>
#else
#include <QPainterPath>
#include <QBitmap>
#include <QWheelEvent>
#endif

const char* EMOJI_FRIEND =   "👤";
const char* EMOJI_GROUP = "👥";
const char* EMOJI_CONFERENCE = "🎙";
const char* EMOJI_SYSEVENT = "⚙";
const char* EMOJI_UNKNOWN = "❓";
const char* EMOJI_TOPIC = "📌";
const char* EMOJI_MATRIX = "🧮";
const char* STATUS_ONLINE = "●";
const char* STATUS_OFFLINE = "○";

static const int kPad = 8;
static const int kRightPad = 12;
static const int kRightAreaW = 55;
static const int kDotR = 5;
static const int kAvatarSz = 36;

static uint32_t typeToEmojiCp(const QString& type) {
    if (type == "friend" || type == kUnktoxFriendType)       return 0x1F464;
    if (type == "group" || type == kUnktoxGroupType)         return 0x1F465;
    if (type == "conference" || type == kUnktoxConferenceType) return 0x1F399;
    if (type == kSyseventType)  return 0x2699;
    if (type == kUnknownType)   return 0x2753;
    if (type == kTopicType)     return 0x1F4CC;
    if (type == kFilesyncType)  return 0x1F4C1;
    if (type == kClipboardType) return 0x1F4CB;
    if (type == kGomuksRoomType) return 0x1F9EE;
    if (type == kImapMailType)  return 0x2709;
    return 0x1F464;
}

static void paintContactRow(QPainter& p, int x, int y, int w, int h,
    bool selected, const QString& type, const QString& name,
    const QString& status, bool isConnected, int unread,
    const QString& lastMessage, const QString& timeStr)
{
    if (selected) {
        QColor selBg = lerpColor(currentPalette().baseBg, currentPalette().accent, 0.25f);
        p.fillRect(x, y, w, h, selBg);
    }

    int rp = kPad;
    int cx = x + rp;
    int cy = y + (h - kDotR * 2) / 2;

    bool online = (status == "online" || status == "tcp" || status == "udp");
    if (type == "group") { online = isConnected; }
    p.save();
    p.setBrush(online ? QColor(76, 175, 80) : QColor(158, 158, 158));
    p.setPen(Qt::NoPen);
    p.drawEllipse(cx, cy, kDotR * 2, kDotR * 2);
    p.restore();
    cx += kDotR * 2 + rp;

    int avatarY = y + 6;
    uint32_t cp = typeToEmojiCp(type);
    QPixmap pm = EmojiRenderer::instance().renderEmoji(cp, kAvatarSz - 4);

#ifdef QT3_BUILD
    if (!pm.isNull()) {
        QBitmap mask(pm.width(), pm.height(), true);
        QPainter mp(&mask);
        mp.setBrush(Qt::color1);
        mp.setPen(Qt::NoPen);
        mp.drawEllipse(0, 0, pm.width(), pm.height());
        mp.end();
        pm.setMask(mask);
    }
#endif

    p.save();
    p.setBrush(QColor(224, 224, 224));
    p.setPen(Qt::NoPen);
#ifndef QT3_BUILD
    {
        QPainterPath clipPath;
        clipPath.addEllipse(cx, avatarY, kAvatarSz, kAvatarSz);
        p.setClipPath(clipPath);
#endif
        p.drawEllipse(cx, avatarY, kAvatarSz, kAvatarSz);
        if (!pm.isNull()) {
            int pmx = cx + (kAvatarSz - pm.width()) / 2;
            int pmy = avatarY + (kAvatarSz - pm.height()) / 2;
            p.drawPixmap(pmx, pmy, pm);
        }
#ifndef QT3_BUILD
    }
#endif
    p.restore();
    cx += kAvatarSz + rp;

    QFont normalFont = p.font();
    QFont boldFont = normalFont;
    boldFont.setBold(true);
    QFont smallFont = normalFont;
    if (normalFont.pointSize() > 4) smallFont.setPointSize(normalFont.pointSize() - 2);
    int lh = p.fontMetrics().lineSpacing();
    int nameW = w - (cx - x) - kRightPad - kRightAreaW;
    if (nameW < 20) { nameW = 20; }

    QString displayName = name.isEmpty() ? _("no_name") : name;
    if (p.fontMetrics().width(displayName) > nameW) {
        while (!displayName.isEmpty() && p.fontMetrics().width(displayName + "...") > nameW)
            displayName.truncate(displayName.length() - 1);
        displayName += "...";
    }

    p.setPen(currentPalette().textPrimary);
    p.setFont(boldFont);
    p.drawText(cx, y + 6, nameW, lh, Qt::AlignLeft | Qt::AlignVCenter, displayName);
    p.setFont(normalFont);

    if (!timeStr.isEmpty()) {
        p.setPen(QColor(160, 160, 160
#ifndef QT3_BUILD
            , 180
#endif
        ));
        p.setFont(smallFont);
        int tw = QFontMetrics(smallFont).width(timeStr);
        p.drawText(x + w - kRightPad - tw, y + 6, tw, lh, Qt::AlignLeft | Qt::AlignVCenter, timeStr);
        p.setFont(normalFont);
    }

    int msgY = y + 6 + lh + 1;
    QString msg = lastMessage.isEmpty() ? displayName : lastMessage;
    msg.replace('\n', ' ');

    if (!msg.isEmpty()) {
        int msgW = w - (cx - x) - kRightPad - kRightAreaW;
        if (msgW < 20) { msgW = 20; }
        p.setFont(smallFont);
        if (p.fontMetrics().width(msg) > msgW) {
            while (!msg.isEmpty() && p.fontMetrics().width(msg + "...") > msgW)
                msg.truncate(msg.length() - 1);
            msg += "...";
        }
        p.setPen(QColor(150, 150, 150
#ifndef QT3_BUILD
            , 180
#endif
        ));
        p.drawText(cx, msgY, msgW, lh, Qt::AlignLeft | Qt::AlignVCenter, msg);
        p.setFont(normalFont);
    }

    if (unread > 0) {
        QString badge = QString("(%1)").arg(unread);
        p.setPen(QColor(100, 100, 100));
        int bw = p.fontMetrics().width(badge);
        p.drawText(x + w - kRightPad - bw, msgY, bw, lh,
                   Qt::AlignLeft | Qt::AlignVCenter, badge);
    }
}

static int calcItemHeight(const QFont& f) {
    QFont sf = f;
    if (f.pointSize() > 4) sf.setPointSize(f.pointSize() - 2);
    int lh = QFontMetrics(f).lineSpacing();
    int lh2 = QFontMetrics(sf).lineSpacing();
    int textH = 6 + lh + 1 + lh2 + 6;
    int avH   = 6 + kAvatarSz + 6;
    return std::max(textH, avH);
}

// ========== ContactListData ==========

bool ContactListData::rowLess(const RowData* a, const RowData* b, const std::vector<QString>& criteria) {
    for (int i = (int)criteria.size() - 1; i >= 0; --i) {
        const QString& c = criteria[i];
        if (c == "name_asc") {
            int d = a->name.localeAwareCompare(b->name);
            if (d != 0) return d < 0;
        } else if (c == "name_desc") {
            int d = b->name.localeAwareCompare(a->name);
            if (d != 0) return d < 0;
        } else if (c == "online_first") {
            bool aOn = (a->status == "online" || a->status == "tcp");
            bool bOn = (b->status == "online" || b->status == "tcp");
            if (aOn != bOn) return aOn;
        } else if (c == "by_type") {
            int d = a->type.localeAwareCompare(b->type);
            if (d != 0) return d < 0;
        }
    }
    return false;
}

RowData* ContactListData::get(int id, const QString& type) {
    auto key = std::make_pair(id, type);
    auto it = m_map.find(key);
    return (it != m_map.end()) ? it->second.get() : nullptr;
}

RowData* ContactListData::addToEnd(std::unique_ptr<RowData> rd) {
    auto key = std::make_pair(rd->id, rd->type);
    auto it = m_map.find(key);
    if (it != m_map.end()) {
        RowData* existing = it->second.get();
        existing->name = rd->name;
        existing->nameUpper = rd->nameUpper;
        existing->status = rd->status;
        existing->chatId = rd->chatId;
        existing->isConnected = rd->isConnected;
        existing->unread = rd->unread;
        existing->lastMessage = rd->lastMessage;
        existing->timeStr = rd->timeStr;
        return existing;
    }
    RowData* ptr = rd.get();
    ptr->index = (int)m_rows.size();
    m_map.emplace(key, std::move(rd));
    m_rows.push_back(ptr);
    return ptr;
}

void ContactListData::remove(int id, const QString& type) {
    auto key = std::make_pair(id, type);
    auto it = m_map.find(key);
    if (it == m_map.end()) return;
    int idx = it->second->index;
    m_rows.erase(m_rows.begin() + idx);
    m_map.erase(it);
    updateFrom(idx);
}

void ContactListData::adjustBySort(int idx) {
    if (m_frozen) {
        m_pendingAdjust.push_back(idx);
        return;
    }
    if (m_criteria.empty()) return;
    if (idx < 0 || idx >= (int)m_rows.size()) return;

    RowData* row = m_rows[idx];

    int newIdx = idx;
    while (newIdx > 0 && rowLess(row, m_rows[newIdx - 1], m_criteria)) {
        --newIdx;
    }
    if (newIdx != idx) {
        std::rotate(m_rows.begin() + newIdx, m_rows.begin() + idx, m_rows.begin() + idx + 1);
        updateFrom(newIdx);
        return;
    }

    newIdx = idx;
    while (newIdx < (int)m_rows.size() - 1 && rowLess(m_rows[newIdx + 1], row, m_criteria)) {
        ++newIdx;
    }
    if (newIdx != idx) {
        std::rotate(m_rows.begin() + idx, m_rows.begin() + idx + 1, m_rows.begin() + newIdx + 1);
        updateFrom(idx);
    }
}

void ContactListData::sort(const std::vector<QString>& criteria) {
    m_criteria = criteria;
    std::stable_sort(m_rows.begin(), m_rows.end(),
        [this](RowData* a, RowData* b) { return rowLess(a, b, m_criteria); });
    updateFrom(0);
}

void ContactListData::freeze() {
    m_frozen = true;
}

void ContactListData::unfreeze(const std::vector<QString>& criteria) {
    m_frozen = false;
    if (m_pendingAdjust.size() <= 1) {
        for (int idx : m_pendingAdjust) {
            adjustBySort(idx);
        }
    } else {
        sort(criteria);
    }
    m_pendingAdjust.clear();
}

void ContactListData::clear() {
    m_map.clear();
    m_rows.clear();
    m_pendingAdjust.clear();
    m_frozen = false;
}

void ContactListData::updateFrom(int startIdx) {
    for (int i = startIdx; i < (int)m_rows.size(); ++i) {
        m_rows[i]->index = i;
    }
}

// ========== ContactListView ==========

ContactListView::ContactListView(ContactListWidget* widget)
    : QWidget(widget), m_widget(widget), m_scrollBar(nullptr)
{
    setMouseTracking(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ContactListView::setSelectedIndex(int idx) {
    m_selIdx = idx;
    update();
}

void ContactListView::setScrollY(int y) {
    m_scrollY = y;
}

int ContactListView::totalHeight() const {
    return m_widget->itemHeight() * m_widget->m_list.size();
}

int ContactListView::yToRow(int y) const {
    int h = m_widget->itemHeight();
    if (h <= 0) return 0;
    int row = y / h;
    if (row >= m_widget->m_list.size()) row = m_widget->m_list.size() - 1;
    if (row < 0) row = 0;
    return row;
}

void ContactListView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    int h = m_widget->itemHeight();
    if (h <= 0) return;
    int totalH = totalHeight();
    int viewH = height();
    if (totalH <= viewH) {
        m_scrollY = 0;
    } else if (m_scrollY > totalH - viewH) {
        m_scrollY = totalH - viewH;
    }

    int firstRow = m_scrollY / h;
    int lastRow = (m_scrollY + viewH - 1) / h;
    int sz = m_widget->m_list.size();
    if (lastRow >= sz) lastRow = sz - 1;

    for (int i = firstRow; i <= lastRow; ++i) {
        RowData* rd = m_widget->m_list.at(i);
        if (!rd || !m_widget->matchesFilter(*rd)) continue;
        int y = i * h - m_scrollY;
        paintContactRow(p, 0, y, width(), h,
                        i == m_selIdx, rd->type, rd->name, rd->status,
                        rd->isConnected, rd->unread, rd->lastMessage, rd->timeStr);
    }
}

void ContactListView::mousePressEvent(QMouseEvent* e) {
    int h = m_widget->itemHeight();
    if (h <= 0) return;
    int row = (e->pos().y() + m_scrollY) / h;
    if (row < 0 || row >= m_widget->m_list.size()) return;
    RowData* rd = m_widget->m_list.at(row);
    if (!rd || !m_widget->matchesFilter(*rd)) return;
    m_selIdx = row;
    update();
    if (e->button() == Qt::RightButton) {
        m_widget->showContextMenuAt(rd->id, rd->type, rd->name, e->globalPos());
    } else {
        m_widget->handleContactSelected(rd->id, rd->type, rd->name);
    }
}

void ContactListView::wheelEvent(QWheelEvent* e) {
    int h = m_widget->itemHeight();
    if (h <= 0) return;
    int step = h * 5;
    int newY = m_scrollY + (e->delta() > 0 ? -step : step);
    int totalH = totalHeight();
    int viewH = height();
    int maxScroll = totalH > viewH ? totalH - viewH : 0;
    m_scrollY = std::max(0, std::min(newY, maxScroll));
    if (m_scrollBar) {
        m_scrollBar->setValue(m_scrollY);
    }
    update();
}

void ContactListView::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    m_widget->updateScrollBar();
}

// ========== ContactListWidget ==========

ContactListWidget::ContactListWidget(QWidget* parent)
    : QWidget(parent), contextItemId(-1), contextItemType(""), m_scrollBar(nullptr)
{
    QBoxLayout* layout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 4, 1);
    qSetMargins(layout, 4, 2, 4, 2);

    QBoxLayout* searchRow = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    countLabel = new QLabel("0", this);
    searchRow->addWidget(countLabel);
    searchRow->addSpacing(4);
    searchInput = new PlaceholderLineEdit(_("placeholders.search_contact"), this);
    connect(searchInput, SIGNAL(textChanged(const QString&)), this, SLOT(onSearchTextChanged(const QString&)));
    searchRow->addWidget(searchInput, 1);
    sortBtn = new QPushButton(_("sort.button"), this);
    connect(sortBtn, SIGNAL(clicked()), this, SLOT(onSortMenuClicked()));
    searchRow->addWidget(sortBtn);
    layout->addLayout(searchRow);

    m_sortCriteria.push_back("online_first");

    m_itemHeight = calcItemHeight(font());

    m_view = new ContactListView(this);
    m_scrollBar = new LimeScrollBar(Qt::Vertical, this);
    m_view->setScrollBar(m_scrollBar);
    connect(m_scrollBar, SIGNAL(valueChanged(int)), this, SLOT(onScrollChanged(int)));

    QBoxLayout* listRow = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    listRow->addWidget(m_view, 1);
    listRow->addWidget(m_scrollBar);
    layout->addLayout(listRow, 1);

    QBoxLayout* addLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    addInput = new PlaceholderLineEdit(_("placeholders.add_friend"), this);
    addLayout->addWidget(addInput, 1);
    addBtn = new QPushButton(_("buttons.add"), this);
    addBtn->setFixedHeight(24);
    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAddFriendClicked()));
    addLayout->addWidget(addBtn);
    layout->addLayout(addLayout);

    QBoxLayout* joinGroupLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    joinGroupInput = new PlaceholderLineEdit(_("placeholders.join_group"), this);
    joinGroupLayout->addWidget(joinGroupInput, 1);
    joinGroupBtn = new QPushButton(_("buttons.join_group"), this);
    joinGroupBtn->setFixedHeight(24);
    connect(joinGroupBtn, SIGNAL(clicked()), this, SLOT(onJoinGroupClicked()));
    joinGroupLayout->addWidget(joinGroupBtn);
    layout->addLayout(joinGroupLayout);

    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    confBtn = new QPushButton(_("buttons.create_conference"), this);
    confBtn->setFixedHeight(24);
    connect(confBtn, SIGNAL(clicked()), this, SLOT(onCreateConferenceClicked()));
    btnLayout->addWidget(confBtn);
    groupBtn = new QPushButton(_("buttons.create_group"), this);
    groupBtn->setFixedHeight(24);
    connect(groupBtn, SIGNAL(clicked()), this, SLOT(onCreateGroupClicked()));
    btnLayout->addWidget(groupBtn);
    layout->addLayout(btnLayout);
}

void ContactListWidget::setContacts(ContactList& contacts) {
    m_list.clear();
    for (uint i = 0; i < contacts.count(); ++i) {
        Contact* c = contacts.at(i);
        auto rd = std::unique_ptr<RowData>(new RowData());
        rd->id = c->id;
        rd->type = c->type;
        rd->name = c->name;
        rd->nameUpper = qToUpper(c->name);
        rd->status = c->status;
        rd->chatId = c->chat_id;
        rd->isConnected = c->is_connected;
        auto key = std::make_pair(c->id, std::string(qToUtf8(c->type).data()));
        auto uit = m_unreadCounts.find(key);
        rd->unread = (uit != m_unreadCounts.end()) ? uit->second : 0;
        auto lit = m_lastMessages.find(key);
        rd->lastMessage = (lit != m_lastMessages.end()) ? lit->second : c->lastMessage;
        auto tit = m_lastMessageTimes.find(key);
        rd->timeStr = (tit != m_lastMessageTimes.end()) ? tit->second : c->lastMessageTime;
        m_list.addToEnd(std::move(rd));
        delete c;
    }
    m_list.sort(m_sortCriteria);
    refreshView();
}

void ContactListWidget::clear() {
    m_list.clear();
    m_view->setSelectedIndex(-1);
    refreshView();
}

void ContactListWidget::updateFriendName(int friendId, const QString& newName) {
    RowData* rd = m_list.get(friendId, "friend");
    if (!rd) return;
    rd->name = newName;
    rd->nameUpper = qToUpper(newName);
    if (!m_batchLevel) {
        m_list.adjustBySort(rd->index);
        refreshView();
    }
}

void ContactListWidget::updateFriendConnectionStatus(int friendId, const QString& newStatus) {
    RowData* rd = m_list.get(friendId, "friend");
    if (!rd) return;
    rd->status = newStatus;
    if (!m_batchLevel) {
        m_list.adjustBySort(rd->index);
        refreshView();
    }
}

void ContactListWidget::updateContact(int id, const QString& type, const QString& name,
                                       const QString& chatId, const QString& status) {
    RowData* rd = m_list.get(id, type);
    if (!rd) return;
    if (!name.isEmpty()) {
        rd->name = name;
        rd->nameUpper = qToUpper(name);
    }
    if (!chatId.isEmpty()) rd->chatId = chatId;
    if (!status.isEmpty()) rd->status = status;
    if (!m_batchLevel) {
        m_list.adjustBySort(rd->index);
        refreshView();
    }
}

void ContactListWidget::addContact(Contact* c) {
    auto key = std::make_pair(c->id, std::string(qToUtf8(c->type).data()));
    auto uit = m_unreadCounts.find(key);
    int unread = (uit != m_unreadCounts.end()) ? uit->second : 0;
    auto lit = m_lastMessages.find(key);
    QString lastMsg = (lit != m_lastMessages.end()) ? lit->second : c->lastMessage;
    auto tit = m_lastMessageTimes.find(key);
    QString lastTime = (tit != m_lastMessageTimes.end()) ? tit->second : c->lastMessageTime;

    auto rd = std::unique_ptr<RowData>(new RowData());
    rd->id = c->id;
    rd->type = c->type;
    rd->name = c->name;
    rd->nameUpper = qToUpper(c->name);
    rd->status = c->status;
    rd->chatId = c->chat_id;
    rd->isConnected = c->is_connected;
    rd->unread = unread;
    rd->lastMessage = lastMsg;
    rd->timeStr = lastTime;

    RowData* result = m_list.addToEnd(std::move(rd));
    delete c;

    if (!m_batchLevel) {
        m_list.adjustBySort(result->index);
        refreshView();
    }
}

void ContactListWidget::removeContact(int id, const QString& type) {
    RowData* rd = m_list.get(id, type);
    if (!rd) return;
    int idx = rd->index;

    m_list.remove(id, type);

    if (idx == m_view->selectedIndex()) {
        int newSel = std::min(idx, m_list.size() - 1);
        m_view->setSelectedIndex(newSel);
    }

    if (!m_batchLevel) refreshView();
}

bool ContactListWidget::isFriendLoaded(int friendId) {
    for (int i = 0; i < m_list.size(); ++i) {
        RowData* rd = m_list.at(i);
        if (rd->id == friendId && rd->type == "friend") {
            return !rd->chatId.isEmpty();
        }
    }
    return false;
}

void ContactListWidget::updateContactLastMessage(int id, const QString& type, const QString& msg,
                                                  const QString& timeStr) {
    auto key = std::make_pair(id, std::string(qToUtf8(type).data()));
    m_lastMessages[key] = msg;
    m_lastMessageTimes[key] = timeStr;
    RowData* rd = m_list.get(id, type);
    if (!rd) return;
    rd->lastMessage = msg;
    rd->timeStr = timeStr;
    if (!m_batchLevel) refreshView();
}

void ContactListWidget::incrementUnread(int id, const QString& type, int count) {
    auto key = std::make_pair(id, std::string(qToUtf8(type).data()));
    m_unreadCounts[key] += count;
    RowData* rd = m_list.get(id, type);
    if (!rd) return;
    rd->unread = m_unreadCounts[key];
    if (!m_batchLevel) refreshView();
}

void ContactListWidget::resetUnread(int id, const QString& type) {
    auto key = std::make_pair(id, std::string(qToUtf8(type).data()));
    m_unreadCounts[key] = 0;
    RowData* rd = m_list.get(id, type);
    if (!rd) return;
    rd->unread = 0;
    if (!m_batchLevel) refreshView();
}

int ContactListWidget::unreadCount(int id, const QString& type) const {
    auto key = std::make_pair(id, std::string(qToUtf8(type).data()));
    auto it = m_unreadCounts.find(key);
    return (it != m_unreadCounts.end()) ? it->second : 0;
}

void ContactListWidget::beginBatch() {
    ++m_batchLevel;
    m_list.freeze();
}

void ContactListWidget::endBatch() {
    if (--m_batchLevel <= 0) {
        m_batchLevel = 0;
        m_list.unfreeze(m_sortCriteria);
        refreshView();
    }
}

void ContactListWidget::setViewScrollY(int y) {
    int totalH = m_list.size() * m_itemHeight;
    int viewH = m_view->height();
    int maxScroll = totalH > viewH ? totalH - viewH : 0;
    m_view->setScrollY(std::max(0, std::min(y, maxScroll)));
    m_scrollBar->blockSignals(true);
    m_scrollBar->setValue(m_view->scrollY());
    m_scrollBar->blockSignals(false);
    m_view->update();
}

void ContactListWidget::updateScrollBar() {
    int totalH = m_list.size() * m_itemHeight;
    int viewH = m_view->height();
    int maxScroll = totalH > viewH ? totalH - viewH : 0;
    m_scrollBar->setRange(0, maxScroll);
    m_scrollBar->setPageStep(viewH);
}

void ContactListWidget::refreshView() {
    resolveSelection();
    updateScrollBar();
    int visible = 0;
    if (m_searchText.isEmpty()) {
        visible = m_list.size();
    } else {
        for (int i = 0; i < m_list.size(); ++i) {
            if (matchesFilter(*m_list.at(i))) ++visible;
        }
    }
    countLabel->setText(QString::number(visible));
    m_view->update();
}

void ContactListWidget::resolveSelection() {
    int oldSel = m_view->selectedIndex();
    if (oldSel < 0 || oldSel >= m_list.size()) return;
    RowData* rd = m_list.at(oldSel);
    if (!rd) { m_view->setSelectedIndex(-1); return; }
    int id = rd->id;
    QString type = rd->type;
    RowData* newRd = m_list.get(id, type);
    m_view->setSelectedIndex(newRd ? newRd->index : -1);
}

void ContactListWidget::onSearchTextChanged(const QString& text) {
    m_searchText = text;
    refreshView();
}

void ContactListWidget::onSortMenuClicked() {
#ifdef QT3_BUILD
    QPopupMenu menu(this);
#else
    QMenu menu(this);
#endif
    menu.setMinimumWidth(200);

    struct SortItem { const char* key; const char* labelKey; };
    SortItem items[] = {
        {"name_asc",    "sort.name_asc"},
        {"name_desc",   "sort.name_desc"},
        {"online_first","sort.online_first"},
        {"by_type",     "sort.by_type"},
    };
    const int itemCount = sizeof(items) / sizeof(items[0]);

#ifdef QT3_BUILD
    for (int i = 0; i < itemCount; ++i) {
        QString label = _(items[i].labelKey);
        int id = menu.insertItem(label, i);
        bool checked = false;
        for (uint j = 0; j < m_sortCriteria.size(); ++j) {
            if (m_sortCriteria[j] == items[i].key) { checked = true; break; }
        }
        menu.setItemChecked(id, checked);
    }
    menu.setCheckable(true);
    int choice = menu.exec(sortBtn->mapToGlobal(QPoint(0, sortBtn->height())));
    if (choice < 0 || choice >= itemCount) { return; }

    QString key = items[choice].key;
    {
        auto it = m_sortCriteria.begin();
        for (; it != m_sortCriteria.end(); ++it) {
            if (*it == key) { break; }
        }
        if (it != m_sortCriteria.end()) {
            m_sortCriteria.erase(it);
        } else {
            m_sortCriteria.push_back(key);
        }
    }
#else
    for (int i = 0; i < itemCount; ++i) {
        QAction* action = menu.addAction(_(items[i].labelKey));
        action->setCheckable(true);
        bool checked = false;
        for (uint j = 0; j < m_sortCriteria.size(); ++j) {
            if (m_sortCriteria[j] == items[i].key) { checked = true; break; }
        }
        action->setChecked(checked);
        action->setData(QString(items[i].key));
    }
    QAction* selected = menu.exec(sortBtn->mapToGlobal(QPoint(0, sortBtn->height())));
    if (!selected) { return; }

    QString key = selected->data().toString();
    {
        auto it = m_sortCriteria.begin();
        for (; it != m_sortCriteria.end(); ++it) {
            if (*it == key) { break; }
        }
        if (it != m_sortCriteria.end()) {
            m_sortCriteria.erase(it);
        } else {
            m_sortCriteria.push_back(key);
        }
    }
#endif

    m_list.sort(m_sortCriteria);
    refreshView();
}

void ContactListWidget::onScrollChanged(int value) {
    if (m_view->scrollY() != value) {
        m_view->setScrollY(value);
        m_view->update();
    }
}

void ContactListWidget::retranslateUi() {
    if (searchInput) searchInput->setPlaceholderText(_("placeholders.search_contact"));
    if (sortBtn) sortBtn->setText(_("sort.button"));
    if (addInput) addInput->setPlaceholderText(_("placeholders.add_friend"));
    if (joinGroupInput) joinGroupInput->setPlaceholderText(_("placeholders.join_group"));
    if (joinGroupBtn) joinGroupBtn->setText(_("buttons.join_group"));
    if (addBtn) addBtn->setText(_("buttons.add"));
    if (confBtn) confBtn->setText(_("buttons.create_conference"));
    if (groupBtn) groupBtn->setText(_("buttons.create_group"));
    refreshView();
}

void ContactListWidget::showContextMenuAt(int id, const QString& type, const QString& name, const QPoint& globalPos) {
    contextItemId = id;
    contextItemType = type;

#ifdef QT3_BUILD
    QPopupMenu menu(0);
#else
    QMenu menu(this);
#endif

#ifdef QT3_BUILD
    menu.insertItem(_("context_menu.view_info"), 0);
#else
    QAction* viewInfoAction = menu.addAction(_("context_menu.view_info"));
#endif

    if (type == "friend") {
#ifdef QT3_BUILD
        menu.insertSeparator();
        menu.insertItem(_("context_menu.invite_to_conference"), 1);
        menu.insertItem(_("context_menu.invite_to_group"), 2);
        menu.insertSeparator();
        menu.insertItem(_("context_menu.delete_friend"), 3);
        int choice = menu.exec(globalPos);
        if (choice == 0) { emit viewInfoRequested(id, type); }
        else if (choice == 1) emit inviteToConferenceRequested(id);
        else if (choice == 2) emit inviteToGroupRequested(id);
        else if (choice == 3) emit deleteOrLeaveRequested(id, type);
#else
        menu.addSeparator();
        QAction* inviteConfAction = menu.addAction(_("context_menu.invite_to_conference"));
        QAction* inviteGroupAction = menu.addAction(_("context_menu.invite_to_group"));
        menu.addSeparator();
        QAction* deleteAction = menu.addAction(_("context_menu.delete_friend"));
        QAction* selected = menu.exec(globalPos);
        if (selected == viewInfoAction) { emit viewInfoRequested(id, type); }
        else if (selected == inviteConfAction) emit inviteToConferenceRequested(id);
        else if (selected == inviteGroupAction) emit inviteToGroupRequested(id);
        else if (selected == deleteAction) emit deleteOrLeaveRequested(id, type);
#endif
    } else if (type == "conference") {
#ifdef QT3_BUILD
        menu.insertItem(_("context_menu.view_members"), 1);
        menu.insertItem(_("context_menu.set_title"), 2);
        menu.insertSeparator();
        menu.insertItem(_("context_menu.leave_conference"), 3);
        int choice = menu.exec(globalPos);
        if (choice == 0) { emit viewInfoRequested(id, type); }
        else if (choice == 1) emit viewMembersRequested(id, type);
        else if (choice == 2) emit setConferenceTitleRequested(id);
        else if (choice == 3) emit deleteOrLeaveRequested(id, type);
#else
        QAction* viewMembersAction = menu.addAction(_("context_menu.view_members"));
        QAction* setTitleAction = menu.addAction(_("context_menu.set_title"));
        menu.addSeparator();
        QAction* leaveAction = menu.addAction(_("context_menu.leave_conference"));
        QAction* selected = menu.exec(globalPos);
        if (selected == viewInfoAction) { emit viewInfoRequested(id, type); }
        else if (selected == viewMembersAction) emit viewMembersRequested(id, type);
        else if (selected == setTitleAction) emit setConferenceTitleRequested(id);
        else if (selected == leaveAction) emit deleteOrLeaveRequested(id, type);
#endif
    } else if (type == "group") {
#ifdef QT3_BUILD
        menu.insertItem(_("context_menu.view_members"), 1);
        menu.insertItem(_("context_menu.rename_nick"), 2);
        menu.insertItem(_("context_menu.set_topic"), 3);
        menu.insertSeparator();
        menu.insertItem(_("context_menu.leave_group"), 4);
        int choice = menu.exec(globalPos);
        if (choice == 0) { emit viewInfoRequested(id, type); }
        else if (choice == 1) emit viewMembersRequested(id, type);
        else if (choice == 2) emit renameNickRequested(id, name);
        else if (choice == 3) emit setGroupTopicRequested(id);
        else if (choice == 4) emit deleteOrLeaveRequested(id, type);
#else
        QAction* viewMembersAction = menu.addAction(_("context_menu.view_members"));
        QAction* renameAction = menu.addAction(_("context_menu.rename_nick"));
        QAction* setTopicAction = menu.addAction(_("context_menu.set_topic"));
        menu.addSeparator();
        QAction* leaveAction = menu.addAction(_("context_menu.leave_group"));
        QAction* selected = menu.exec(globalPos);
        if (selected == viewInfoAction) { emit viewInfoRequested(id, type); }
        else if (selected == viewMembersAction) emit viewMembersRequested(id, type);
        else if (selected == renameAction) emit renameNickRequested(id, name);
        else if (selected == setTopicAction) emit setGroupTopicRequested(id);
        else if (selected == leaveAction) emit deleteOrLeaveRequested(id, type);
#endif
    } else {
#ifdef QT3_BUILD
        int choice = menu.exec(globalPos);
        if (choice == 0) { emit viewInfoRequested(id, type); }
#else
        QAction* selected = menu.exec(globalPos);
        if (selected == viewInfoAction) { emit viewInfoRequested(id, type); }
#endif
    }
}

void ContactListWidget::onJoinGroupClicked() {
    QString chatId = qTrim(joinGroupInput->text());
    if (chatId.isEmpty()) {
        QMessageBox::warning(this, _("warning"), _("please_enter_chat_id"));
        return;
    }
    bool success = ToxAPI::joinGroupByChatIdSync(qToUtf8(chatId).data(), "", "");
    if (success) {
        QMessageBox::information(this, _("group.joined"),
                                  _A("group.joined", QStringList() << chatId));
        joinGroupInput->clear();
    } else {
        QMessageBox::warning(this, _("group.join_failed"),
                              _("group.join_failed"));
    }
}

void ContactListWidget::onAddFriendClicked() {
    QString pubkey = qTrim(addInput->text());
    if (pubkey.isEmpty()) {
        QMessageBox::warning(this, _("warning"), _("add_friend_prompt"));
        return;
    }
    int len = pubkey.length();
    if (len != 64 && len != 76) {
        QMessageBox::warning(this, _("warning"), _("add_friend_prompt"));
        return;
    }
    int friendId = ToxAPI::addFriendSync(qToUtf8(pubkey).data());
    if (friendId >= 0) {
        QMessageBox::information(this, _("add_friend_success"),
                                  _("add_friend_success"));
        addInput->clear();
    } else {
        QMessageBox::warning(this, _("add_friend_failed"),
                              _("add_friend_failed"));
    }
}

void ContactListWidget::onCreateConferenceClicked() {
    int confId = ToxAPI::createConferenceSync();
    if (confId >= 0) {
        QMessageBox::information(this, _("conference_created"),
                                  _("conference_created"));
    } else {
        QMessageBox::warning(this, _("conference_create_failed"),
                              _("conference_create_failed"));
    }
}

void ContactListWidget::onCreateGroupClicked() {
    int groupId = ToxAPI::createGroupSync("NewGroup", "me", "", false);
    if (groupId >= 0) {
        QMessageBox::information(this, _("group_created"),
                                  _("group_created"));
    } else {
        QMessageBox::warning(this, _("group_create_failed"),
                              _("group_create_failed"));
    }
}
