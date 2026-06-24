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
#include <qlistbox.h>
#include <qregion.h>
#include <qbitmap.h>
#else
#include <qlistwidget.h>
#include <QStyledItemDelegate>
#include <QPainterPath>
#include <QBitmap>
#endif

// Emoji 和状态点常量（所有构建统一使用 UTF-8 emoji）
const char* EMOJI_FRIEND = "👤";
const char* EMOJI_GROUP = "👥";
const char* EMOJI_CONFERENCE = "🎙";
const char* EMOJI_SYSEVENT = "⚙";
const char* EMOJI_UNKNOWN = "❓";
const char* EMOJI_TOPIC = "📌";
const char* EMOJI_MATRIX = "🧮";
const char* STATUS_ONLINE = "●";
const char* STATUS_OFFLINE = "○";

// ---- 公共辅助函数 ----

static int kRowH() { return 54; }
static int kPad() { return 8; }
static int kDotR() { return 5; }
static int kAvatarSz() { return 36; }

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

// 公共绘制函数（Qt3/Qt4 共用）
static void paintContactRow(QPainter& p, int x, int y, int w, int h,
    bool selected, const QString& type, const QString& name,
    const QString& status, bool isConnected, int unread,
    const QString& lastMessage, const QString& timeStr)
{
    if (selected) {
        QColor selBg = lerpColor(currentPalette().baseBg, currentPalette().accent, 0.25f);
        p.fillRect(x, y, w, h, selBg);
    }

    int rh = kRowH();
    int rp = kPad();

    int cx = x + rp;
    int cy = y + (h - kDotR() * 2) / 2;

    // 2. Status dot（不变）
    bool online = (status == "online" || status == "tcp" || status == "udp");
    if (type == "group") { online = isConnected; }
    p.save();
    p.setBrush(online ? QColor(76, 175, 80) : QColor(158, 158, 158));
    p.setPen(Qt::NoPen);
    p.drawEllipse(cx, cy, kDotR() * 2, kDotR() * 2);
    p.restore();
    cx += kDotR() * 2 + rp;

    // 3. 圆形头像（仿 chatview 风格）
    int avatarY = y + 6;
    uint32_t cp = typeToEmojiCp(type);
    QPixmap pm = EmojiRenderer::instance().renderEmoji(cp, kAvatarSz() - 4);

#ifdef QT3_BUILD
    // Qt3: 用 QPixmap::setMask 做圆形 clip（X11 不支持非矩形 clip region）
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

    // 圆形背景（Qt4 用 QPainterPath clip，Qt3 用 QPixmap::setMask）
    p.save();
    p.setBrush(QColor(224, 224, 224));
    p.setPen(Qt::NoPen);
#ifndef QT3_BUILD
    {
        QPainterPath clipPath;
        clipPath.addEllipse(cx, avatarY, kAvatarSz(), kAvatarSz());
        p.setClipPath(clipPath);
#endif
        p.drawEllipse(cx, avatarY, kAvatarSz(), kAvatarSz());

        if (!pm.isNull()) {
            int pmx = cx + (kAvatarSz() - pm.width()) / 2;
            int pmy = avatarY + (kAvatarSz() - pm.height()) / 2;
            p.drawPixmap(pmx, pmy, pm);
        }
#ifndef QT3_BUILD
    }
#endif
    p.restore();
    cx += kAvatarSz() + rp;

    // 4. Line 1: Name（bold）+ time（right）
    QFont normalFont = p.font();
    QFont boldFont = normalFont;
    boldFont.setBold(true);
    QFont smallFont = normalFont;
    if (normalFont.pointSize() > 4) smallFont.setPointSize(normalFont.pointSize() - 2);
    int lh = p.fontMetrics().lineSpacing();
    int rightAreaW = 55; // time + unread + right margin conservative area
    int nameW = w - (cx - x) - rp - rightAreaW;
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

    // Time: right-aligned on line 1
    if (!timeStr.isEmpty()) {
        p.setPen(QColor(160, 160, 160
#ifndef QT3_BUILD
            , 180
#endif
        ));
        int tw = p.fontMetrics().width(timeStr);
        p.drawText(x + w - rp - tw, y + 6, tw, lh, Qt::AlignLeft | Qt::AlignVCenter, timeStr);
    }

    // 5. Line 2: Last message（略透明）+ unread（right）
    int msgY = y + 6 + lh + 1;
    QString msg = lastMessage.isEmpty() ? displayName : lastMessage;
    msg.replace('\n', ' ');

    if (!msg.isEmpty()) {
        int msgW = w - (cx - x) - rp - rightAreaW;

        // qWarning("paintContactRow: id=%s name=[%s] lastMsg=[%s] msg=[%s] w=%d h=%d msgY=%d msgW=%d lh=%d cx=%d",
        //          qToUtf8(type).data(), qToUtf8(displayName).data(),
        //          qToUtf8(lastMessage).data(), qToUtf8(msg).data(),
        //          w, h, msgY, msgW, lh, cx);
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
        p.setFont(smallFont);
        p.drawText(cx, msgY, msgW, lh, Qt::AlignLeft | Qt::AlignVCenter, msg);
        p.setFont(normalFont);
    }

    // Unread badge: right-aligned on line 2
    if (unread > 0) {
        QString badge = QString("(%1)").arg(unread);
        p.setPen(QColor(100, 100, 100));
        int bw = p.fontMetrics().width(badge);
        p.drawText(x + w - rp - bw, msgY, bw, lh,
                   Qt::AlignLeft | Qt::AlignVCenter, badge);
    }
}

#ifdef QT3_BUILD
// ---- Qt3: ContactListItem ----

class ContactListItem : public QListBoxItem {
public:
    ContactListItem(QListBox* lb, int id, const QString& type,
                    const QString& name, const QString& status,
                    bool isConnected, int unread,
                    const QString& lastMessage, const QString& timeStr);
    void paint(QPainter* p);
    int height(const QListBox*) const { return 54; }
    int itemId() const { return m_id; }
    const QString& itemType() const { return m_type; }
    const QString& itemName() const { return m_name; }
private:
    int m_id, m_unread;
    QString m_type, m_name, m_status, m_lastMessage, m_timeStr;
    bool m_isConnected;
    QListBox* m_lb;
};

ContactListItem::ContactListItem(QListBox* lb, int id, const QString& type,
    const QString& name, const QString& status,
    bool isConnected, int unread,
    const QString& lastMessage, const QString& timeStr)
    : QListBoxItem(lb), m_id(id), m_unread(unread),
      m_type(type), m_name(name), m_status(status),
      m_isConnected(isConnected), m_lb(lb),
      m_lastMessage(lastMessage), m_timeStr(timeStr) {}

void ContactListItem::paint(QPainter* p) {
    bool sel = m_lb->isSelected(this);
    paintContactRow(*p, 0, 0, m_lb->width(), kRowH(),
                    sel, m_type, m_name, m_status, m_isConnected, m_unread,
                    m_lastMessage, m_timeStr);
}

// ---- Qt3: ContactListBox（右键菜单由 contentsMousePressEvent 直接处理）----

class ContactListBox : public QListBox {
public:
    ContactListBox(ContactListWidget* w) : QListBox(w), m_widget(w) {}
protected:
    void contentsMousePressEvent(QMouseEvent* e) {
        QListBox::contentsMousePressEvent(e);
        if (e->button() == Qt::RightButton) {
            QListBoxItem* qitem = itemAt(e->pos());
            if (!qitem) { return; }
            ContactListItem* item = dynamic_cast<ContactListItem*>(qitem);
            if (!item) { e->accept(); return; }
            m_widget->showContextMenuAt(
                item->itemId(), item->itemType(), item->itemName(),
                e->globalPos());
            e->accept();
        }
    }
    void contentsContextMenuEvent(QContextMenuEvent* e) {
        e->accept();
    }
private:
    ContactListWidget* m_widget;
};

#else
// ---- Qt4: ContactListDelegate ----

class ContactListDelegate : public QStyledItemDelegate {
public:
    ContactListDelegate(QObject* parent = 0) : QStyledItemDelegate(parent) {}
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
        return QSize(100, 54);
    }
};

void ContactListDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt,
                                 const QModelIndex& idx) const {
    QString type = idx.data(Qt::UserRole + 1).toString();
    QString name = idx.data(Qt::UserRole + 2).toString();
    int unread = idx.data(Qt::UserRole + 3).toInt();
    bool conn  = idx.data(Qt::UserRole + 4).toBool();
    QString status = idx.data(Qt::UserRole + 5).toString();
    QString lastMessage = idx.data(Qt::UserRole + 6).toString();
    QString timeStr = idx.data(Qt::UserRole + 7).toString();
    bool sel = (opt.state & QStyle::State_Selected);
    paintContactRow(*p, opt.rect.x(), opt.rect.y(), opt.rect.width(), opt.rect.height(),
                    sel, type, name, status, conn, unread, lastMessage, timeStr);
}
#endif

// ---- ContactListWidget 实现 ----

ContactListWidget::ContactListWidget(QWidget* parent) : QWidget(parent), contextItemId(-1), contextItemType(""), m_scrollBar(nullptr) {
    QBoxLayout* layout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 4, 1);
    qSetMargins(layout, 4, 2, 4, 2);
    
    // 搜索行：计数 + 搜索框 + 排序按钮
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
    
    // 默认排序：在线优先
    m_sortCriteria.push_back("online_first");
    
    // 联系人列表
#ifdef QT3_BUILD
    listWidget = new ContactListBox(this);
    ((ContactListBox*)listWidget)->setSelectionMode(QListBox::Single);
    connect(((ContactListBox*)listWidget), SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));
    ((QListBox*)listWidget)->installEventFilter(this);
#else
    listWidget = new QListWidget(this);
    ((QListWidget*)listWidget)->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(((QListWidget*)listWidget), SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(onItemClicked()));
    ((QListWidget*)listWidget)->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(((QListWidget*)listWidget), SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
    m_scrollBar = new LimeScrollBar(Qt::Vertical, (QListWidget*)listWidget);
    ((QListWidget*)listWidget)->setVerticalScrollBar(m_scrollBar);
    ((QListWidget*)listWidget)->setItemDelegate(new ContactListDelegate((QListWidget*)listWidget));
#endif
    layout->addWidget((QWidget*)listWidget, 1); // stretch
    
    // 底部添加好友区域
    QBoxLayout* addLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    addInput = new PlaceholderLineEdit(_("placeholders.add_friend"), this);
    addLayout->addWidget(addInput, 1);
    
    addBtn = new QPushButton(_("buttons.add"), this);
    addBtn->setFixedHeight(24);
    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAddFriendClicked()));
    addLayout->addWidget(addBtn);
    layout->addLayout(addLayout);
    
    // 加入群组区域（第2行，参照 web 端 index.html:44-47）
    QBoxLayout* joinGroupLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    joinGroupInput = new PlaceholderLineEdit(_("placeholders.join_group"), this);
    joinGroupLayout->addWidget(joinGroupInput, 1);

    joinGroupBtn = new QPushButton(_("buttons.join_group"), this);
    joinGroupBtn->setFixedHeight(24);
    connect(joinGroupBtn, SIGNAL(clicked()), this, SLOT(onJoinGroupClicked()));
    joinGroupLayout->addWidget(joinGroupBtn);
    layout->addLayout(joinGroupLayout);
    
    // 创建按钮行（第3行）
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

void ContactListWidget::setContacts(const ContactList& contacts) {
    for (uint i = 0; i < allContacts.count(); ++i)
        delete allContacts.at(i);
    allContacts.clear();
    allContacts = contacts;
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
}

void ContactListWidget::clear() {
    allContacts.clear();
#ifdef QT3_BUILD
    ((QListBox*)listWidget)->clear();
#else
    ((QListWidget*)listWidget)->clear();
#endif
}

void ContactListWidget::updateFriendName(int friendId, const QString& newName) {
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (c->id == friendId && c->type == "friend") {
            c->name = newName;
            break;
        }
    }
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
}

void ContactListWidget::updateContact(int id, const QString& type, const QString& name,
                                       const QString& chatId, const QString& status) {
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (c->id == id && c->type == type) {
            if (!name.isEmpty()) c->name = name;
            if (!chatId.isEmpty()) c->chat_id = chatId;
            if (!status.isEmpty()) c->status = status;
            break;
        }
    }
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
}

void ContactListWidget::addContact(Contact* c) {
    allContacts.append(c);
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
}

void ContactListWidget::removeContact(int id, const QString& type) {
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (c->id == id && c->type == type) {
#ifdef QT3_BUILD
            allContacts.remove(i);
#else
            allContacts.removeAt(i);
#endif
            delete c;
            break;
        }
    }
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
}

bool ContactListWidget::isFriendLoaded(int friendId) {
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (c->id == friendId && c->type == "friend") {
            return !c->chat_id.isEmpty();
        }
    }
    return false;
}

void ContactListWidget::incrementUnread(int id, const QString& type, int count) {
    auto key = std::make_pair(id, std::string(qToUtf8(type).data()));
    m_unreadCounts[key] += count;
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
}

void ContactListWidget::resetUnread(int id, const QString& type) {
    auto key = std::make_pair(id, std::string(qToUtf8(type).data()));
    m_unreadCounts[key] = 0;
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
}

int ContactListWidget::unreadCount(int id, const QString& type) const {
    auto key = std::make_pair(id, std::string(qToUtf8(type).data()));
    auto it = m_unreadCounts.find(key);
    if (it != m_unreadCounts.end()) {
        return it->second;
    }
    return 0;
}

void ContactListWidget::updateFriendConnectionStatus(int friendId, const QString& newStatus) {
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (c->id == friendId && c->type == "friend") {
            c->status = newStatus;
            break;
        }
    }
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
}

void ContactListWidget::updateContactLastMessage(int id, const QString& type, const QString& msg,
                                                  const QString& timeStr) {
    // qWarning("updateContactLastMessage: id=%d type=%s msg=[%s]",
    //          id, qToUtf8(type).data(), qToUtf8(msg).data());
    auto key = std::make_pair(id, std::string(qToUtf8(type).data()));
    m_lastMessages[key] = msg;
    m_lastMessageTimes[key] = timeStr;
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (c->id == id && c->type == type) {
            c->lastMessage = msg;
            c->lastActive = QDateTime::currentDateTime();
            c->lastMessageTime = timeStr;
            break;
        }
    }
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
}

void ContactListWidget::onSearchTextChanged(const QString& text) {
    m_searchText = text;
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
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
    
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
}

void ContactListWidget::onSelectionChanged() {
#ifdef QT3_BUILD
    QListBox* lb = (QListBox*)listWidget;
    ContactListItem* citem = (ContactListItem*)lb->selectedItem();
    if (!citem) { return; }
    emit contactSelected(citem->itemId(), citem->itemType(), citem->itemName());
#endif
}

void ContactListWidget::onItemClicked() {
#ifdef QT3_BUILD
    // Qt3: onSelectionChanged handles it
#else
    // Qt4: get current item
    QListWidget* lw = (QListWidget*)listWidget;
    QListWidgetItem* item = lw->currentItem();
    if (!item) { return; }
    int id = item->data(Qt::UserRole).toInt();
    QString type = item->data(Qt::UserRole + 1).toString();
    // find name
    QString name;
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (c->id == id && c->type == type) {
            name = c->name;
            break;
        }
    }
    emit contactSelected(id, type, name);
#endif
}

void ContactListWidget::updateView_v3() {
#ifdef QT3_BUILD
    QListBox* lb = (QListBox*)listWidget;
    
    // 收集并过滤联系人
    std::vector<Contact*> visible;
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (!m_searchText.isEmpty() && !qToUpper(c->name).contains(qToUpper(m_searchText))) { continue; }
        visible.push_back(c);
    }
    sortVisible(visible);
    
    // 更新计数标签
    countLabel->setText(QString::number(visible.size()));
    
    // 保存当前选中的联系人信息
    int selectedId = -1;
    QString selectedType;
    QListBoxItem* selItem = lb->selectedItem();
    if (selItem) {
        ContactListItem* citem = (ContactListItem*)selItem;
        selectedId = citem->itemId();
        selectedType = citem->itemType();
    }
    
    // 保存滚动位置（顶部可见项的联系人 ID，而非索引）
    int scrollId = -1;
    QString scrollType;
    int scrollTopIdx = lb->topItem();
    if (scrollTopIdx >= 0 && (uint)scrollTopIdx < lb->count()) {
        ContactListItem* topCItem = (ContactListItem*)lb->item(scrollTopIdx);
        if (topCItem) {
            scrollId = topCItem->itemId();
            scrollType = topCItem->itemType();
        }
    }
    
    lb->clear();
    
    ContactListItem* targetItem = NULL;
    for (uint i = 0; i < visible.size(); ++i) {
        Contact* c = visible[i];
        
        auto key = std::make_pair(c->id, std::string(qToUtf8(c->type).data()));
        auto uit = m_unreadCounts.find(key);
        int unread = (uit != m_unreadCounts.end()) ? uit->second : 0;

        QString lastMsg = c->lastMessage;
        if (lastMsg.isEmpty()) {
            // qWarning("updateView_v3: id=%d type=%s name=[%s] c->lastMessage EMPTY, trying m_lastMessages",
            //          c->id, qToUtf8(c->type).data(), qToUtf8(c->name).data());
            auto lit = m_lastMessages.find(key);
            if (lit != m_lastMessages.end()) { lastMsg = lit->second; }
        }

        QString lastTime = c->lastMessageTime;
        if (lastTime.isEmpty()) {
            auto tit = m_lastMessageTimes.find(key);
            if (tit != m_lastMessageTimes.end()) { lastTime = tit->second; }
        }

        // qWarning("updateView_v3: id=%d type=%s name=[%s] lastMsg=[%s]",
        //          c->id, qToUtf8(c->type).data(), qToUtf8(c->name).data(),
        //          qToUtf8(lastMsg).data());

        ContactListItem* item = new ContactListItem(lb, c->id, c->type,
            c->name, c->status, c->is_connected, unread,
            lastMsg, lastTime);
        
        if (c->id == selectedId && c->type == selectedType) {
            targetItem = item;
        }
    }
    
    if (lb->count() == 0) {
        lb->insertItem(_("no_contacts"));
    } else if (targetItem) {
        lb->setSelected(targetItem, TRUE);
    }
    
    // 恢复滚动位置
    if (scrollId >= 0) {
        for (uint i = 0; i < lb->count(); ++i) {
            ContactListItem* item = (ContactListItem*)lb->item(i);
            if (item && item->itemId() == scrollId && item->itemType() == scrollType) {
                lb->setTopItem(i);
                break;
            }
        }
    }
    // 确保选中项可见
    {
        ContactListItem* selItem = (ContactListItem*)lb->selectedItem();
        if (selItem) {
            int selIdx = lb->index(selItem);
            int topIdx = lb->topItem();
            int visRows = lb->height() / kRowH();
            if (visRows < 2) visRows = 2;
            if (selIdx < topIdx)
                lb->setTopItem(selIdx);
            else if (selIdx >= topIdx + visRows)
                lb->setTopItem(selIdx - visRows + 1);
        }
    }
#endif
}

void ContactListWidget::updateView_v4() {
#ifndef QT3_BUILD
    QListWidget* lw = (QListWidget*)listWidget;
    
    // 收集并过滤联系人
    std::vector<Contact*> visible;
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (!m_searchText.isEmpty() && !qToUpper(c->name).contains(qToUpper(m_searchText))) { continue; }
        visible.push_back(c);
    }
    sortVisible(visible);
    
    // 更新计数标签
    countLabel->setText(QString::number(visible.size()));
    
    int selectedId = -1;
    QString selectedType;
    QListWidgetItem* selItem = lw->currentItem();
    if (selItem) {
        selectedId = selItem->data(Qt::UserRole).toInt();
        selectedType = selItem->data(Qt::UserRole + 1).toString();
    }
    
    // 保存滚动位置（顶部可见项的联系人 ID，而非像素偏移）
    int scrollId = -1;
    QString scrollType;
    int scrollTopIdx = lw->verticalScrollBar()->value() / kRowH();
    if (scrollTopIdx >= 0 && (uint)scrollTopIdx < lw->count()) {
        QListWidgetItem* topItem = lw->item(scrollTopIdx);
        if (topItem) {
            scrollId = topItem->data(Qt::UserRole).toInt();
            scrollType = topItem->data(Qt::UserRole + 1).toString();
        }
    }
    
    lw->clear();
    for (uint i = 0; i < visible.size(); ++i) {
        Contact* c = visible[i];
        
        auto key = std::make_pair(c->id, std::string(qToUtf8(c->type).data()));
        auto uit = m_unreadCounts.find(key);
        int unread = (uit != m_unreadCounts.end()) ? uit->second : 0;

        QString lastMsg = c->lastMessage;
        if (lastMsg.isEmpty()) {
            auto lit = m_lastMessages.find(key);
            if (lit != m_lastMessages.end()) { lastMsg = lit->second; }
        }

        QString lastTime = c->lastMessageTime;
        if (lastTime.isEmpty()) {
            auto tit = m_lastMessageTimes.find(key);
            if (tit != m_lastMessageTimes.end()) { lastTime = tit->second; }
        }

        QListWidgetItem* item = new QListWidgetItem();
        item->setData(Qt::UserRole,     c->id);
        item->setData(Qt::UserRole + 1, c->type);
        item->setData(Qt::UserRole + 2, c->name);
        item->setData(Qt::UserRole + 3, unread);
        item->setData(Qt::UserRole + 4, c->is_connected);
        item->setData(Qt::UserRole + 5, c->status);
        item->setData(Qt::UserRole + 6, lastMsg);
        item->setData(Qt::UserRole + 7, lastTime);
        lw->addItem(item);
        
        if (c->id == selectedId && c->type == selectedType) {
            item->setSelected(true);
        }
    }
    
    if (lw->count() == 0) {
        lw->addItem(new QListWidgetItem(_("no_contacts")));
    }
    
    // 恢复滚动位置（按联系人 ID 找新位置）
    if (scrollId >= 0) {
        for (uint i = 0; i < lw->count(); ++i) {
            QListWidgetItem* item = lw->item(i);
            if (item && item->data(Qt::UserRole).toInt() == scrollId
                    && item->data(Qt::UserRole+1).toString() == scrollType) {
                lw->verticalScrollBar()->setValue(i * kRowH());
                break;
            }
        }
    }
    // 确保选中项可见
    {
        QList<QListWidgetItem*> selItems = lw->selectedItems();
        if (!selItems.isEmpty()) {
            lw->scrollToItem(selItems.first(), QAbstractItemView::EnsureVisible);
        }
    }
#endif
}

void ContactListWidget::sortVisible(std::vector<Contact*>& visible) {
    // 最高优先级：按 lastActive 降序，最近消息的联系人置顶
    std::stable_sort(visible.begin(), visible.end(), [](Contact* a, Contact* b) {
        return a->lastActive > b->lastActive;
    });
    // 反向迭代：低优先级先排，高优先级后排
    for (int i = (int)m_sortCriteria.size() - 1; i >= 0; --i) {
        const QString& criterion = m_sortCriteria[i];
        if (criterion == "name_asc") {
            std::stable_sort(visible.begin(), visible.end(), [](Contact* a, Contact* b) {
                return qToUpper(a->name) < qToUpper(b->name);
            });
        } else if (criterion == "name_desc") {
            std::stable_sort(visible.begin(), visible.end(), [](Contact* a, Contact* b) {
                return qToUpper(a->name) > qToUpper(b->name);
            });
        } else if (criterion == "online_first") {
            std::stable_sort(visible.begin(), visible.end(), [](Contact* a, Contact* b) {
                bool aOnline = (a->status == "online" || a->status == "tcp" || a->status == "udp");
                bool bOnline = (b->status == "online" || b->status == "tcp" || b->status == "udp");
                return aOnline && !bOnline;
            });
        } else if (criterion == "by_type") {
            std::stable_sort(visible.begin(), visible.end(), [](Contact* a, Contact* b) {
                return a->type < b->type;
            });
        }
    }
}

void ContactListWidget::retranslateUi() {
    // 更新搜索框
    if (searchInput) { searchInput->setPlaceholderText(_("placeholders.search_contact")); }
    if (sortBtn) { sortBtn->setText(_("sort.button")); }
    
    // 更新添加好友输入框
    if (addInput) { addInput->setPlaceholderText(_("placeholders.add_friend")); }
    
    // 更新加入群组输入框
    if (joinGroupInput) { joinGroupInput->setPlaceholderText(_("placeholders.join_group")); }
    if (joinGroupBtn) { joinGroupBtn->setText(_("buttons.join_group")); }
    
    // 更新底部按钮
    if (addBtn) { addBtn->setText(_("buttons.add")); }
    if (confBtn) { confBtn->setText(_("buttons.create_conference")); }
    if (groupBtn) { groupBtn->setText(_("buttons.create_group")); }
    
    // 重新更新视图
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
}

#ifndef QT3_BUILD
// Qt4: 右键菜单
void ContactListWidget::showContextMenu(QPoint pos) {
    QListWidget* lw = (QListWidget*)listWidget;
    QListWidgetItem* item = lw->itemAt(pos);
    if (!item) { return; }
    
    int id = item->data(Qt::UserRole).toInt();
    QString type = item->data(Qt::UserRole + 1).toString();
    QString name = item->data(Qt::UserRole + 2).toString();
    QPoint globalPos = lw->mapToGlobal(pos);
    showContextMenuAt(id, type, name, globalPos);
}

bool ContactListWidget::eventFilter(QObject*, QEvent*) {
    return false;
}
#else
// Qt3: 事件过滤器—透传（右键由 ContactListBox 处理）
bool ContactListWidget::eventFilter(QObject*, QEvent*) {
    return false;
}

void ContactListWidget::showContextMenu(QPoint) {
    // Qt3 通过 ContactListBox 处理，此函数不使用
}
#endif

// 共用：显示右键菜单
void ContactListWidget::showContextMenuAt(int id, const QString& type, const QString& name, const QPoint& globalPos) {
    contextItemId = id;
    contextItemType = type;
    
#ifdef QT3_BUILD
    QPopupMenu menu(0);
#else
    QMenu menu(this);
#endif
    
    // 查看信息
#ifdef QT3_BUILD
    menu.insertItem(_("context_menu.view_info"), 0);
#else
    QAction* viewInfoAction = menu.addAction(_("context_menu.view_info"));
#endif
    
    if (type == "friend") {
        // 好友：邀请进会议、邀请进群组，删除在最下
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
        // 会议：查看成员、设置标题、离开会议
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
        // 群组：查看成员、修改昵称、设置主题、离开群组
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
        // 其他类型（unknown, sysevent, topic 等）：只有查看信息
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
    // 验证长度：64 字符公钥 或 76 字符地址
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
