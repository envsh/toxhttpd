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
#include <qheader.h>
#else
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

static const int kPad = 8;
static const int kRightPad = 12;
static const int kRightAreaW = 55;  // time + unread + right margin reserved
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

    int rp = kPad;

    int cx = x + rp;
    int cy = y + (h - kDotR * 2) / 2;

    // 2. Status dot（不变）
    bool online = (status == "online" || status == "tcp" || status == "udp");
    if (type == "group") { online = isConnected; }
    p.save();
    p.setBrush(online ? QColor(76, 175, 80) : QColor(158, 158, 158));
    p.setPen(Qt::NoPen);
    p.drawEllipse(cx, cy, kDotR * 2, kDotR * 2);
    p.restore();
    cx += kDotR * 2 + rp;

    // 3. 圆形头像（仿 chatview 风格）
    int avatarY = y + 6;
    uint32_t cp = typeToEmojiCp(type);
    QPixmap pm = EmojiRenderer::instance().renderEmoji(cp, kAvatarSz - 4);

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

    // 4. Line 1: Name（bold）+ time（right）
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

    // Time: right-aligned on line 1
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

    // 5. Line 2: Last message（略透明）+ unread（right）
    int msgY = y + 6 + lh + 1;
    QString msg = lastMessage.isEmpty() ? displayName : lastMessage;
    msg.replace('\n', ' ');

    if (!msg.isEmpty()) {
        int msgW = w - (cx - x) - kRightPad - kRightAreaW;

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
        p.drawText(x + w - kRightPad - bw, msgY, bw, lh,
                   Qt::AlignLeft | Qt::AlignVCenter, badge);
    }
}

// ---- 动态行高计算（共用，无 kRowH 硬编码）----
static int calcItemHeight(const QFont& f) {
    QFont sf = f;
    if (f.pointSize() > 4) sf.setPointSize(f.pointSize() - 2);
    int lh = QFontMetrics(f).lineSpacing();
    int lh2 = QFontMetrics(sf).lineSpacing();
    int textH = 6 + lh + 1 + lh2 + 6;
    int avH   = 6 + kAvatarSz + 6;
    return std::max(textH, avH);
}

// ---- 排序比较所需的多条件向量（文件静态，供 compare() 访问）----
static std::vector<QString>* g_sortCriteriaPtr = nullptr;

#ifdef QT3_BUILD
// ---- Qt3: ContactListViewItem ----

class ContactListViewItem : public QListViewItem {
public:
    ContactListViewItem(QListView* parent, int id, const QString& type,
                        const QString& name, const QString& status,
                        bool isConnected, int unread,
                        const QString& lastMessage, const QString& timeStr);
    void paintCell(QPainter* p, const QColorGroup& cg, int col, int width, int align);
    void setup();
    int compare(QListViewItem* other, int col, bool ascending) const;

    void updateData(int unread, const QString& lastMessage, const QString& timeStr);
    void updateContact(const QString& name, const QString& status, bool isConnected);

    int itemId() const { return m_id; }
    const QString& itemType() const { return m_type; }
    const QString& itemName() const { return m_name; }
    const QString& itemStatus() const { return m_status; }
private:
    int m_id, m_unread;
    QString m_type, m_name, m_status, m_lastMessage, m_timeStr;
    bool m_isConnected;
};

ContactListViewItem::ContactListViewItem(QListView* parent, int id, const QString& type,
    const QString& name, const QString& status,
    bool isConnected, int unread,
    const QString& lastMessage, const QString& timeStr)
    : QListViewItem(parent), m_id(id), m_unread(unread),
      m_type(type), m_name(name), m_status(status),
      m_isConnected(isConnected),
      m_lastMessage(lastMessage), m_timeStr(timeStr)
{
}

void ContactListViewItem::setup() {
    QListViewItem::setup();
    if (listView()) setHeight(calcItemHeight(listView()->font()));
}

void ContactListViewItem::paintCell(QPainter* p, const QColorGroup&, int, int width, int) {
    paintContactRow(*p, 0, 0, width, height(),
                    isSelected(), m_type, m_name, m_status, m_isConnected, m_unread,
                    m_lastMessage, m_timeStr);
}

int ContactListViewItem::compare(QListViewItem* other, int, bool) const {
    const ContactListViewItem* o = static_cast<const ContactListViewItem*>(other);
    if (!g_sortCriteriaPtr) { return 0; }
    const std::vector<QString>& criteria = *g_sortCriteriaPtr;
    for (int i = (int)criteria.size() - 1; i >= 0; --i) {
        const QString& c = criteria[i];
        if (c == "name_asc") {
            int d = m_name.localeAwareCompare(o->m_name);
            if (d != 0) return d;
        } else if (c == "name_desc") {
            int d = o->m_name.localeAwareCompare(m_name);
            if (d != 0) return d;
        } else if (c == "online_first") {
            bool aOn = (m_status == "online" || m_status == "tcp");
            bool bOn = (o->m_status == "online" || o->m_status == "tcp");
            if (aOn != bOn) { return aOn ? -1 : 1; }
        } else if (c == "by_type") {
            int d = m_type.localeAwareCompare(o->m_type);
            if (d != 0) return d;
        }
    }
    return 0;
}

void ContactListViewItem::updateData(int unread, const QString& lastMessage, const QString& timeStr) {
    m_unread = unread;
    m_lastMessage = lastMessage;
    m_timeStr = timeStr;
    setText(0, m_name);
}

void ContactListViewItem::updateContact(const QString& name, const QString& status, bool isConnected) {
    m_name = name;
    m_status = status;
    m_isConnected = isConnected;
    setText(0, m_name);
}

// ---- Qt3: ContactListView（右键菜单由 contentsMousePressEvent 直接处理）----

class ContactListView : public QListView {
public:
    ContactListView(ContactListWidget* w) : QListView(w), m_widget(w) {
        addColumn("", 1);  // single column, Manual mode; resizeEvent fixes width
        header()->hide();
        setRootIsDecorated(false);
        setSorting(-1);    // manual sort via sort()
    }
protected:
    void resizeEvent(QResizeEvent* e) {
        QListView::resizeEvent(e);
        setColumnWidth(0, viewport()->width());
    }
    void contentsMousePressEvent(QMouseEvent* e) {
        QListView::contentsMousePressEvent(e);
        if (e->button() == Qt::RightButton) {
            QListViewItem* qitem = itemAt(e->pos());
            if (!qitem) { return; }
            ContactListViewItem* item = dynamic_cast<ContactListViewItem*>(qitem);
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
// ---- Qt4: ContactListModel ----

class ContactListModel : public QAbstractListModel {
    Q_OBJECT
public:
    struct Item {
        Contact* contact;
        int unread;
        QString lastMessage;
        QString lastMessageTime;
    };

    ContactListModel(QObject* parent = 0) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex& idx) const;

    void setContacts(const ContactList& contacts,
                     const std::map<std::pair<int,std::string>,int>& unreadCounts,
                     const std::map<std::pair<int,std::string>,QString>& lastMessages,
                     const std::map<std::pair<int,std::string>,QString>& lastMessageTimes);
    void addContact(Contact* c, int unread,
                    const QString& lastMsg, const QString& lastTime);
    void removeContact(int id, const QString& type);
    void updateContact(int id, const QString& type, int unread,
                       const QString& lastMsg, const QString& lastTime);
    void applyFilter(const QString& filterText);
    void applySort(const std::vector<QString>& criteria);
    QModelIndex findIndex(int id, const QString& type) const;

    const QList<Item>& items() const { return m_visible; }

private:
    QList<Item> m_allItems;
    QList<Item> m_visible;
    QString m_filterText;
    std::vector<QString> m_sortCriteria;

    bool matchesFilter(const Item& item) const;
    void rebuildVisibleList();
    void doSortItems(QList<Item>& items);
};

int ContactListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) { return 0; }
    return m_visible.size();
}

QVariant ContactListModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || idx.row() < 0 || idx.row() >= m_visible.size())
        return QVariant();
    const Item& item = m_visible[idx.row()];
    if (role == Qt::UserRole)     return item.contact->id;
    if (role == Qt::UserRole + 1) return item.contact->type;
    if (role == Qt::UserRole + 2) return item.contact->name;
    if (role == Qt::UserRole + 3) return item.unread;
    if (role == Qt::UserRole + 4) return item.contact->is_connected;
    if (role == Qt::UserRole + 5) return item.contact->status;
    if (role == Qt::UserRole + 6) return item.lastMessage;
    if (role == Qt::UserRole + 7) return item.lastMessageTime;
    return QVariant();
}

Qt::ItemFlags ContactListModel::flags(const QModelIndex& idx) const {
    if (!idx.isValid()) { return Qt::NoItemFlags; }
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

bool ContactListModel::matchesFilter(const Item& item) const {
    if (m_filterText.isEmpty()) { return true; }
    return qToUpper(item.contact->name).contains(qToUpper(m_filterText));
}

void ContactListModel::doSortItems(QList<Item>& items) {
    std::stable_sort(items.begin(), items.end(),
        [this](const Item& a, const Item& b) {
            for (int i = (int)m_sortCriteria.size() - 1; i >= 0; --i) {
                const QString& c = m_sortCriteria[i];
                if (c == "name_asc") {
                    int d = a.contact->name.localeAwareCompare(b.contact->name);
                    if (d != 0) { return d < 0; }
                } else if (c == "name_desc") {
                    int d = b.contact->name.localeAwareCompare(a.contact->name);
                    if (d != 0) { return d < 0; }
                } else if (c == "online_first") {
                    bool aOn = (a.contact->status == "online" || a.contact->status == "tcp");
                    bool bOn = (b.contact->status == "online" || b.contact->status == "tcp");
                    if (aOn != bOn) { return aOn; }
                } else if (c == "by_type") {
                    int d = a.contact->type.localeAwareCompare(b.contact->type);
                    if (d != 0) { return d < 0; }
                }
            }
            return a.contact->lastActive > b.contact->lastActive;
        });
}

void ContactListModel::rebuildVisibleList() {
    m_visible.clear();
    for (int i = 0; i < m_allItems.size(); ++i) {
        if (matchesFilter(m_allItems[i])) {
            m_visible.append(m_allItems[i]);
        }
    }
    doSortItems(m_visible);
}

void ContactListModel::setContacts(const ContactList& contacts,
    const std::map<std::pair<int,std::string>,int>& unreadCounts,
    const std::map<std::pair<int,std::string>,QString>& lastMessages,
    const std::map<std::pair<int,std::string>,QString>& lastMessageTimes)
{
    beginResetModel();
    m_allItems.clear();
    for (uint i = 0; i < contacts.count(); ++i) {
        Contact* c = contacts.at(i);
        auto key = std::make_pair(c->id, std::string(qToUtf8(c->type).data()));
        Item item;
        item.contact = c;
        auto uit = unreadCounts.find(key);
        item.unread = (uit != unreadCounts.end()) ? uit->second : 0;
        auto lit = lastMessages.find(key);
        item.lastMessage = (lit != lastMessages.end()) ? lit->second : QString();
        auto tit = lastMessageTimes.find(key);
        item.lastMessageTime = (tit != lastMessageTimes.end()) ? tit->second : QString();
        m_allItems.append(item);
    }
    rebuildVisibleList();
    endResetModel();
}

void ContactListModel::addContact(Contact* c, int unread,
                                   const QString& lastMsg, const QString& lastTime) {
    Item item;
    item.contact = c;
    item.unread = unread;
    item.lastMessage = lastMsg;
    item.lastMessageTime = lastTime;
    m_allItems.append(item);
    if (!matchesFilter(item)) { return; }
    int row = m_visible.size();
    beginInsertRows(QModelIndex(), row, row);
    m_visible.append(item);
    endInsertRows();
    doSortItems(m_visible);
    // After sort, the item may have moved. Signal full layout change.
    emit layoutChanged();
}

void ContactListModel::removeContact(int id, const QString& type) {
    for (int i = 0; i < m_allItems.size(); ++i) {
        if (m_allItems[i].contact->id == id && m_allItems[i].contact->type == type) {
            m_allItems.removeAt(i);
            break;
        }
    }
    for (int i = 0; i < m_visible.size(); ++i) {
        if (m_visible[i].contact->id == id && m_visible[i].contact->type == type) {
            beginRemoveRows(QModelIndex(), i, i);
            m_visible.removeAt(i);
            endRemoveRows();
            return;
        }
    }
}

void ContactListModel::updateContact(int id, const QString& type, int unread,
                                      const QString& lastMsg, const QString& lastTime) {
    for (int i = 0; i < m_allItems.size(); ++i) {
        if (m_allItems[i].contact->id == id && m_allItems[i].contact->type == type) {
            m_allItems[i].unread = unread;
            m_allItems[i].lastMessage = lastMsg;
            m_allItems[i].lastMessageTime = lastTime;
            break;
        }
    }
    for (int i = 0; i < m_visible.size(); ++i) {
        if (m_visible[i].contact->id == id && m_visible[i].contact->type == type) {
            m_visible[i].unread = unread;
            m_visible[i].lastMessage = lastMsg;
            m_visible[i].lastMessageTime = lastTime;
            emit dataChanged(index(i), index(i));
            return;
        }
    }
}

void ContactListModel::applyFilter(const QString& filterText) {
    m_filterText = filterText;
    beginResetModel();
    rebuildVisibleList();
    endResetModel();
}

void ContactListModel::applySort(const std::vector<QString>& criteria) {
    m_sortCriteria = criteria;
    beginResetModel();
    doSortItems(m_visible);
    endResetModel();
}

QModelIndex ContactListModel::findIndex(int id, const QString& type) const {
    for (int i = 0; i < m_visible.size(); ++i) {
        if (m_visible[i].contact->id == id && m_visible[i].contact->type == type) {
            return index(i);
        }
    }
    return QModelIndex();
}

// ---- Qt4: ContactListDelegate ----

class ContactListDelegate : public QStyledItemDelegate {
public:
    ContactListDelegate(QObject* parent = 0) : QStyledItemDelegate(parent) {}
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const;
    QSize sizeHint(const QStyleOptionViewItem& opt, const QModelIndex&) const {
        QFontMetrics fm(opt.font);
        int iconArea = kPad + kDotR*2 + kPad + kAvatarSz + kPad;
        int minW = iconArea + fm.width("W") * 3 + kRightAreaW + kRightPad;
        return QSize(minW, calcItemHeight(opt.font));
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
    {
        ContactListView* lv = new ContactListView(this);
        listWidget = lv;
        lv->setSelectionMode(QListView::Single);
        connect(lv, SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));
        lv->installEventFilter(this);
    }
#else
    {
        QListView* lv = new QListView(this);
        listWidget = lv;
        m_model = new ContactListModel(this);
        lv->setModel(m_model);
        lv->setSelectionMode(QAbstractItemView::SingleSelection);
        connect(lv, SIGNAL(clicked(const QModelIndex&)), this, SLOT(onItemClicked()));
        lv->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(lv, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(QPoint)));
        m_scrollBar = new LimeScrollBar(Qt::Vertical, lv);
        lv->setVerticalScrollBar(m_scrollBar);
        lv->setItemDelegate(new ContactListDelegate(lv));
    }
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
#ifdef QT3_BUILD
    {
        QListView* lv = static_cast<QListView*>(listWidget);
        lv->clear();
        g_sortCriteriaPtr = &m_sortCriteria;
        for (uint i = 0; i < allContacts.count(); ++i) {
            Contact* c = allContacts.at(i);
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
            new ContactListViewItem(lv, c->id, c->type, c->name, c->status,
                                    c->is_connected, unread, lastMsg, lastTime);
        }
    }
#else
    m_model->setContacts(allContacts, m_unreadCounts, m_lastMessages, m_lastMessageTimes);
#endif
    // Apply search and sort
    rebuildSortFilter();
}

void ContactListWidget::clear() {
#ifdef QT3_BUILD
    static_cast<QListView*>(listWidget)->clear();
#else
    m_model->setContacts(ContactList(), m_unreadCounts, m_lastMessages, m_lastMessageTimes);
#endif
    allContacts.clear();
}

void ContactListWidget::updateFriendName(int friendId, const QString& newName) {
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (c->id == friendId && c->type == "friend") {
            c->name = newName;
            break;
        }
    }
    findAndUpdateItem(friendId, "friend");
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
    findAndUpdateItem(id, type);
}

void ContactListWidget::addContact(Contact* c) {
    auto key = std::make_pair(c->id, std::string(qToUtf8(c->type).data()));
    auto uit = m_unreadCounts.find(key);
    int unread = (uit != m_unreadCounts.end()) ? uit->second : 0;
    auto lit = m_lastMessages.find(key);
    QString lastMsg = (lit != m_lastMessages.end()) ? lit->second : c->lastMessage;
    auto tit = m_lastMessageTimes.find(key);
    QString lastTime = (tit != m_lastMessageTimes.end()) ? tit->second : c->lastMessageTime;
#ifdef QT3_BUILD
    {
        QListView* lv = static_cast<QListView*>(listWidget);
        g_sortCriteriaPtr = &m_sortCriteria;
        new ContactListViewItem(lv, c->id, c->type,
            c->name, c->status, c->is_connected, unread, lastMsg, lastTime);
    }
#else
    m_model->addContact(c, unread, lastMsg, lastTime);
#endif
    allContacts.append(c);
}

void ContactListWidget::removeContact(int id, const QString& type) {
    // Remove from model/view first (before deleting Contact*)
#ifdef QT3_BUILD
    {
        QListView* lv = static_cast<QListView*>(listWidget);
        for (QListViewItem* item = lv->firstChild(); item; item = item->nextSibling()) {
            ContactListViewItem* ci = dynamic_cast<ContactListViewItem*>(item);
            if (ci && ci->itemId() == id && ci->itemType() == type) {
                delete ci;
                break;
            }
        }
    }
#else
    m_model->removeContact(id, type);
#endif
    // Then remove from allContacts
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
    findAndUpdateItem(id, type);
}

void ContactListWidget::resetUnread(int id, const QString& type) {
    auto key = std::make_pair(id, std::string(qToUtf8(type).data()));
    m_unreadCounts[key] = 0;
    findAndUpdateItem(id, type);
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
    findAndUpdateItem(friendId, "friend");
}

void ContactListWidget::updateContactLastMessage(int id, const QString& type, const QString& msg,
                                                  const QString& timeStr) {
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
    findAndUpdateItem(id, type);
}

void ContactListWidget::onSearchTextChanged(const QString& text) {
    m_searchText = text;
    rebuildSortFilter();
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
    
    rebuildSortFilter();
}

#ifdef QT3_BUILD
void ContactListWidget::onSelectionChanged() {
    QListView* lv = static_cast<QListView*>(listWidget);
    ContactListViewItem* citem = static_cast<ContactListViewItem*>(lv->selectedItem());
    if (!citem) { return; }
    emit contactSelected(citem->itemId(), citem->itemType(), citem->itemName());
}
#endif

void ContactListWidget::onItemClicked() {
#ifndef QT3_BUILD
    QListView* lv = static_cast<QListView*>(listWidget);
    QModelIndex index = lv->currentIndex();
    if (!index.isValid()) { return; }
    int id = index.data(Qt::UserRole).toInt();
    QString type = index.data(Qt::UserRole + 1).toString();
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

void ContactListWidget::findAndUpdateItem(int id, const QString& type) {
    auto key = std::make_pair(id, std::string(qToUtf8(type).data()));
    auto uit = m_unreadCounts.find(key);
    int unread = (uit != m_unreadCounts.end()) ? uit->second : 0;
    auto lit = m_lastMessages.find(key);
    QString lastMsg = (lit != m_lastMessages.end()) ? lit->second : QString();
    auto tit = m_lastMessageTimes.find(key);
    QString lastTime = (tit != m_lastMessageTimes.end()) ? tit->second : QString();
#ifdef QT3_BUILD
    QListView* lv = static_cast<QListView*>(listWidget);
    for (QListViewItem* item = lv->firstChild(); item; item = item->nextSibling()) {
        ContactListViewItem* ci = dynamic_cast<ContactListViewItem*>(item);
        if (ci && ci->itemId() == id && ci->itemType() == type) {
            // Update contact-level fields from allContacts
            for (uint i = 0; i < allContacts.count(); ++i) {
                Contact* c = allContacts.at(i);
                if (c->id == id && c->type == type) {
                    ci->updateContact(c->name, c->status, c->is_connected);
                    ci->updateData(unread, lastMsg, lastTime);
                    break;
                }
            }
            break;
        }
    }
#else
    m_model->updateContact(id, type, unread, lastMsg, lastTime);
#endif
}

void ContactListWidget::rebuildSortFilter() {
#ifdef QT3_BUILD
    QListView* lv = static_cast<QListView*>(listWidget);
    g_sortCriteriaPtr = &m_sortCriteria;

    // Apply search visibility
    for (QListViewItem* item = lv->firstChild(); item; item = item->nextSibling()) {
        ContactListViewItem* ci = dynamic_cast<ContactListViewItem*>(item);
        if (!ci) { continue; }
        if (m_searchText.isEmpty()) {
            item->setVisible(true);
        } else {
            item->setVisible(qToUpper(ci->itemName()).contains(qToUpper(m_searchText)));
        }
    }

    // Update count label: count visible items
    int visibleCount = 0;
    for (QListViewItem* item = lv->firstChild(); item; item = item->nextSibling()) {
        if (item->isVisible()) { ++visibleCount; }
    }
    countLabel->setText(QString::number(visibleCount));
#else
    m_model->applyFilter(m_searchText);
    m_model->applySort(m_sortCriteria);
#endif
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
    rebuildSortFilter();
}

#ifndef QT3_BUILD
// Qt4: 右键菜单
void ContactListWidget::showContextMenu(QPoint pos) {
    QListView* lv = static_cast<QListView*>(listWidget);
    QModelIndex idx = lv->indexAt(pos);
    if (!idx.isValid()) { return; }
    int id = idx.data(Qt::UserRole).toInt();
    QString type = idx.data(Qt::UserRole + 1).toString();
    QString name = idx.data(Qt::UserRole + 2).toString();
    QPoint globalPos = lv->mapToGlobal(pos);
    showContextMenuAt(id, type, name, globalPos);
}

bool ContactListWidget::eventFilter(QObject*, QEvent*) {
    return false;
}
#else
// Qt3: 事件过滤器—透传（右键由 ContactListView 处理）
bool ContactListWidget::eventFilter(QObject*, QEvent*) {
    return false;
}

void ContactListWidget::showContextMenu(QPoint) {
    // Qt3 通过 ContactListView 处理，此函数不使用
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

#ifndef QT3_BUILD
#include "contactlist.moc"
#endif
