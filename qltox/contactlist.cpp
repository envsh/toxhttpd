#include "contactlist.h"
#include "translator.h"
#include "compat34.h"
#include "restapi.h"
#include "placeholderlineedit.h"
#include <qmessagebox.h>
#include <algorithm>
#ifdef QT3_BUILD
#include <qlistbox.h>
#else
#include <qlistwidget.h>
#endif

// Emoji 和状态点常量
#if QT_VERSION >= 0x050000
const char* EMOJI_FRIEND = "👤";
const char* EMOJI_GROUP = "👥";
const char* EMOJI_CONFERENCE = "🎙";
const char* EMOJI_SYSEVENT = "⚙";
const char* EMOJI_UNKNOWN = "❓";
const char* EMOJI_TOPIC = "📌";
const char* EMOJI_MATRIX = "🧮";
const char* STATUS_ONLINE = "●";
const char* STATUS_OFFLINE = "○";
#else
// qt3/qt4 not support emoji
const char* EMOJI_FRIEND = "F";
const char* EMOJI_GROUP = "G";
const char* EMOJI_CONFERENCE = "C";
const char* EMOJI_SYSEVENT = "S";
const char* EMOJI_UNKNOWN = "?";
const char* EMOJI_TOPIC = "T";
const char* EMOJI_MATRIX = "M";
const char* STATUS_ONLINE = "O";
const char* STATUS_OFFLINE = "N";
#endif

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
    listWidget = new QListBox(this);
    ((QListBox*)listWidget)->setSelectionMode(QListBox::Single);
    connect(((QListBox*)listWidget), SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));
    ((QListBox*)listWidget)->installEventFilter(this);
#else
    listWidget = new QListWidget(this);
    ((QListWidget*)listWidget)->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(((QListWidget*)listWidget), SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(onItemClicked()));
    ((QListWidget*)listWidget)->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(((QListWidget*)listWidget), SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
    m_scrollBar = new LimeScrollBar(Qt::Vertical, (QListWidget*)listWidget);
    ((QListWidget*)listWidget)->setVerticalScrollBar(m_scrollBar);
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
    QListBoxItem* item = lb->selectedItem();
    if (!item) { return; }
    int index = lb->index(item);
    int count = 0;
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (!m_searchText.isEmpty() && !qToUpper(c->name).contains(qToUpper(m_searchText))) { continue; }
        if (count == index) {
            emit contactSelected(c->id, c->type, c->name);
            return;
        }
        ++count;
    }
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
        int index = lb->index(selItem);
        if (index >= 0 && (uint)index < visible.size()) {
            selectedId = visible[index]->id;
            selectedType = visible[index]->type;
        }
    }
    
    // 保存滚动位置（顶部可见项索引）
    int scrollIndex = -1;
    int scrollTopIdx = lb->topItem();
    if (scrollTopIdx >= 0 && (uint)scrollTopIdx < visible.size()) {
        scrollIndex = scrollTopIdx;
    }
    
    lb->clear();
    
    int newIndex = 0;
    int targetIndex = -1;
    for (uint i = 0; i < visible.size(); ++i) {
        Contact* c = visible[i];
        
        QString emoji;
        if (c->type == "friend")       emoji = EMOJI_FRIEND;
        else if (c->type == "group")   emoji = EMOJI_GROUP;
        else if (c->type == "conference") emoji = EMOJI_CONFERENCE;
        else if (c->type == "sysevent")   emoji = EMOJI_SYSEVENT;
        else if (c->type == "unknown")    emoji = EMOJI_UNKNOWN;
        else if (c->type == "topic")      emoji = EMOJI_TOPIC;
        else if (c->type == "gomuks_room") emoji = EMOJI_MATRIX;
        else if (c->type == "unktox_friend") emoji = EMOJI_FRIEND;
        else if (c->type == "unktox_conference") emoji = EMOJI_CONFERENCE;
        else if (c->type == "unktox_group") emoji = EMOJI_GROUP;
        else if (c->type == "imap_mail") emoji = "E";
        else emoji = EMOJI_CONFERENCE;
        // 群组使用真实连接状态，好友使用原有逻辑，会议保持硬编码
        QString statusDot;
        if (c->type == "group") {
            statusDot = c->is_connected ? STATUS_ONLINE : STATUS_OFFLINE;
        } else {
            statusDot = (c->status == "online" || c->status == "tcp" || c->status == "udp") ? STATUS_ONLINE : STATUS_OFFLINE;
        }
        
        QString displayName = c->name.isEmpty() ? _("no_name") : c->name;
        // 对于群组和会议，如果名称为空，使用降级策略（已在eventpoller中处理）
        if (displayName.length() > 20) {
            displayName = displayName.left(20) + "...";
        }
        
        // emoji和名字之间加空格（web端使用CSS margin-right，这里用字符串空格）
        QString itemText = QString("%1 %2  %3").arg(statusDot, emoji, displayName);
        auto key = std::make_pair(c->id, std::string(qToUtf8(c->type).data()));
        auto uit = m_unreadCounts.find(key);
        if (uit != m_unreadCounts.end() && uit->second > 0) {
            itemText += QString("  (%1)").arg(uit->second);
        }
        lb->insertItem(itemText);
        
        if (c->id == selectedId && c->type == selectedType) {
            targetIndex = newIndex;
        }
        ++newIndex;
    }
    
    if (lb->count() == 0) {
        lb->insertItem(_("no_contacts"));
    } else if (targetIndex >= 0) {
        lb->setSelected(targetIndex, TRUE);
    }
    
    // 恢复滚动位置
    if (scrollIndex >= 0 && scrollIndex < (int)lb->count()) {
        lb->setTopItem(scrollIndex);
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
    
    int scrollPos = lw->verticalScrollBar()->value();
    
    lw->clear();
    for (uint i = 0; i < visible.size(); ++i) {
        Contact* c = visible[i];
        
        QString emoji;
        if (c->type == "friend")       emoji = EMOJI_FRIEND;
        else if (c->type == "group")   emoji = EMOJI_GROUP;
        else if (c->type == "conference") emoji = EMOJI_CONFERENCE;
        else if (c->type == "sysevent")   emoji = EMOJI_SYSEVENT;
        else if (c->type == "unknown")    emoji = EMOJI_UNKNOWN;
        else if (c->type == "topic")      emoji = EMOJI_TOPIC;
        else if (c->type == "gomuks_room") emoji = EMOJI_MATRIX;
        else if (c->type == "unktox_friend") emoji = EMOJI_FRIEND;
        else if (c->type == "unktox_conference") emoji = EMOJI_CONFERENCE;
        else if (c->type == "unktox_group") emoji = EMOJI_GROUP;
        else if (c->type == "imap_mail") emoji = "E";
        else emoji = EMOJI_CONFERENCE;
        QString statusDot = (c->status == "online" || c->status == "tcp" || c->status == "udp") ? STATUS_ONLINE : STATUS_OFFLINE;
        
        QString displayName = c->name.isEmpty() ? _("no_name") : c->name;
        // 对于群组和会议，如果名称为空，使用降级策略（已在eventpoller中处理）
        if (displayName.length() > 20) {
            displayName = displayName.left(20) + "...";
        }
        
        // emoji和名字之间加空格
        QString itemText = QString("%1 %2  %3").arg(statusDot, emoji, displayName);
        auto key = std::make_pair(c->id, std::string(qToUtf8(c->type).data()));
        auto uit = m_unreadCounts.find(key);
        if (uit != m_unreadCounts.end() && uit->second > 0) {
            itemText += QString("  (%1)").arg(uit->second);
        }
        QListWidgetItem* item = new QListWidgetItem(itemText);
        item->setData(Qt::UserRole, c->id);
        item->setData(Qt::UserRole + 1, c->type);
        lw->addItem(item);
        
        if (c->id == selectedId && c->type == selectedType) {
            item->setSelected(true);
        }
    }
    
    if (lw->count() == 0) {
        lw->addItem(new QListWidgetItem(_("no_contacts")));
    }
    
    lw->verticalScrollBar()->setValue(scrollPos);
#endif
}

void ContactListWidget::sortVisible(std::vector<Contact*>& visible) {
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
    QPoint globalPos = lw->mapToGlobal(pos);
    showContextMenuAt(id, type, item->text(), globalPos);
}

bool ContactListWidget::eventFilter(QObject*, QEvent*) {
    return false;
}
#else
// Qt3: 事件过滤器处理右键
bool ContactListWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == (QListBox*)listWidget && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            QListBox* lb = (QListBox*)listWidget;
            QListBoxItem* item = lb->itemAt(mouseEvent->pos());
            if (!item) { return false; }
            int index = lb->index(item);
            // 查找对应的联系人
            int count = 0;
            for (uint i = 0; i < allContacts.count(); ++i) {
                Contact* c = allContacts.at(i);
                if (!m_searchText.isEmpty() && !qToUpper(c->name).contains(qToUpper(m_searchText))) { continue; }
                if (count == index) {
                    contextItemId = c->id;
                    contextItemType = c->type;
                    QPoint globalPos = lb->viewport()->mapToGlobal(mouseEvent->pos());
                    showContextMenuAt(c->id, c->type, c->name, globalPos);
                    return true;
                }
                ++count;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ContactListWidget::showContextMenu(QPoint) {
    // Qt3 通过 eventFilter 处理，此函数不使用
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
