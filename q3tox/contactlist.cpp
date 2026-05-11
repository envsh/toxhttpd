#include "contactlist.h"
#include "translator.h"
#include "compat34.h"
#include "restapi.h"
#include "placeholderlineedit.h"

// 静态数组定义
const char* ContactListWidget::tabFilters[4] = {"all", "friend", "group", "conference"};
const char* ContactListWidget::tabNames[4] = {"tabs.all", "tabs.friends", "tabs.groups", "tabs.conferences"};

// Emoji 和状态点常量
#if QT_VERSION >= 0x050000
const char* EMOJI_FRIEND = "👤";
const char* EMOJI_GROUP = "👥";
const char* EMOJI_CONFERENCE = "🎙";
const char* STATUS_ONLINE = "●";
const char* STATUS_OFFLINE = "○";
#else
// qt3/qt4 not support emoji
const char* EMOJI_FRIEND = "F";
const char* EMOJI_GROUP = "G";
const char* EMOJI_CONFERENCE = "C";
const char* STATUS_ONLINE = "O";
const char* STATUS_OFFLINE = "N";
#endif

ContactListWidget::ContactListWidget(QWidget* parent) : QWidget(parent), currentFilter("all"), currentTab(0), contextItemId(-1), contextItemType("") {
    QBoxLayout* layout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 8, 2);
    qSetMargins(layout, 8, 8, 8, 8);
    
    // Tab 标签
    QBoxLayout* tabLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    
    for (int i = 0; i < 4; ++i) {
        QPushButton* tab = new QPushButton(_(tabNames[i]), this);
        qSetCheckable(tab, true);
        if (i == 0) qSetChecked(tab, true);
        tabButtons[i] = tab;
        connect(tab, SIGNAL(clicked()), this, SLOT(onTabClicked()));
        tabLayout->addWidget(tab);
    }
    layout->addLayout(tabLayout);
    
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
#endif
    layout->addWidget((QWidget*)listWidget, 1); // stretch
    
    // 底部添加好友区域
    QBoxLayout* addLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    addInput = new PlaceholderLineEdit(_("placeholders.add_friend"), this);
    addLayout->addWidget(addInput, 1);
    
    addBtn = new QPushButton(_("buttons.add"), this);
    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAddFriendClicked()));
    addLayout->addWidget(addBtn);
    layout->addLayout(addLayout);
    
    // 加入群组区域（第2行，参照 web 端 index.html:44-47）
    QBoxLayout* joinGroupLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    joinGroupInput = new PlaceholderLineEdit(_("placeholders.join_group"), this);
    joinGroupLayout->addWidget(joinGroupInput, 1);

    joinGroupBtn = new QPushButton(_("buttons.join_group"), this);
    connect(joinGroupBtn, SIGNAL(clicked()), this, SLOT(onJoinGroupClicked()));
    joinGroupLayout->addWidget(joinGroupBtn);
    layout->addLayout(joinGroupLayout);
    
    // 创建按钮行（第3行）
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    confBtn = new QPushButton(_("buttons.create_conference"), this);
    connect(confBtn, SIGNAL(clicked()), this, SLOT(onCreateConferenceClicked()));
    btnLayout->addWidget(confBtn);
    
    groupBtn = new QPushButton(_("buttons.create_group"), this);
    connect(groupBtn, SIGNAL(clicked()), this, SLOT(onCreateGroupClicked()));
    btnLayout->addWidget(groupBtn);
    layout->addLayout(btnLayout);
}

void ContactListWidget::setContacts(const ContactList& contacts) {
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

void ContactListWidget::onTabClicked() {
    QPushButton* senderBtn = (QPushButton*) sender();
    if (!senderBtn) return;
    
    // 查找是哪个按钮被点击
    for (int i = 0; i < 4; ++i) {
        if (tabButtons[i] == senderBtn) {
            setTabFilter(i);
            break;
        }
    }
}

void ContactListWidget::setTabFilter(int index) {
    if (index < 0 || index >= 4) return;
    
    currentFilter = tabFilters[index];
    currentTab = index;
    
    // 更新按钮状态
    for (int i = 0; i < 4; ++i) {
        qSetChecked(tabButtons[i], i == index);
    }
    
    updateView_v3();
#ifndef QT3_BUILD
    updateView_v4();
#endif
}

void ContactListWidget::onSelectionChanged() {
#ifdef QT3_BUILD
    QListBox* lb = (QListBox*)listWidget;
    QListBoxItem* item = lb->selectedItem();
    if (!item) return;
    int index = lb->index(item);
    int count = 0;
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (currentFilter != "all") {
            if (currentFilter == "friend" && c->type != "friend") continue;
            if (currentFilter == "group" && c->type != "group") continue;
            if (currentFilter == "conference" && c->type != "conference") continue;
        }
        if (count == index) {
            emit contactSelected(c->id, c->type);
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
    if (!item) return;
    int id = item->data(Qt::UserRole).toInt();
    QString type = item->data(Qt::UserRole + 1).toString();
    emit contactSelected(id, type);
#endif
}

void ContactListWidget::updateView_v3() {
#ifdef QT3_BUILD
    QListBox* lb = (QListBox*)listWidget;
    
    // 保存当前选中的联系人信息
    int selectedId = -1;
    QString selectedType;
    QListBoxItem* selItem = lb->selectedItem();
    if (selItem) {
        int index = lb->index(selItem);
        int count = 0;
        for (uint i = 0; i < allContacts.count(); ++i) {
            Contact* c = allContacts.at(i);
            if (currentFilter != "all") {
                if (currentFilter == "friend" && c->type != "friend") continue;
                if (currentFilter == "group" && c->type != "group") continue;
                if (currentFilter == "conference" && c->type != "conference") continue;
            }
            if (count == index) {
                selectedId = c->id;
                selectedType = c->type;
                break;
            }
            ++count;
        }
    }
    
    lb->clear();
    
    int newIndex = 0;
    int targetIndex = -1;
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (currentFilter != "all") {
            if (currentFilter == "friend" && c->type != "friend") continue;
            if (currentFilter == "group" && c->type != "group") continue;
            if (currentFilter == "conference" && c->type != "conference") continue;
        }
        
        QString emoji = (c->type == "friend") ? EMOJI_FRIEND :
                       (c->type == "group") ? EMOJI_GROUP : EMOJI_CONFERENCE;
        // 群组使用真实连接状态，好友使用原有逻辑，会议保持硬编码
        QString statusDot;
        if (c->type == "group") {
            statusDot = c->is_connected ? STATUS_ONLINE : STATUS_OFFLINE;
        } else {
            statusDot = (c->status == "online" || c->status == "tcp") ? STATUS_ONLINE : STATUS_OFFLINE;
        }
        
        QString displayName = c->name.isEmpty() ? _("no_name") : c->name;
        // 对于群组和会议，如果名称为空，使用降级策略（已在eventpoller中处理）
        if (displayName.length() > 20) {
            displayName = displayName.left(20) + "...";
        }
        
        // emoji和名字之间加空格（web端使用CSS margin-right，这里用字符串空格）
        lb->insertItem(QString("%1 %2  %3").arg(statusDot, emoji, displayName));
        
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
#endif
}

void ContactListWidget::updateView_v4() {
#ifndef QT3_BUILD
    QListWidget* lw = (QListWidget*)listWidget;
    
    int selectedId = -1;
    QString selectedType;
    QListWidgetItem* selItem = lw->currentItem();
    if (selItem) {
        selectedId = selItem->data(Qt::UserRole).toInt();
        selectedType = selItem->data(Qt::UserRole + 1).toString();
    }
    
    lw->clear();
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (currentFilter != "all") {
            if (currentFilter == "friend" && c->type != "friend") continue;
            if (currentFilter == "group" && c->type != "group") continue;
            if (currentFilter == "conference" && c->type != "conference") continue;
        }
        
        QString emoji = (c->type == "friend") ? EMOJI_FRIEND :
                       (c->type == "group") ? EMOJI_GROUP : EMOJI_CONFERENCE;
        QString statusDot = (c->status == "online" || c->status == "tcp") ? STATUS_ONLINE : STATUS_OFFLINE;
        
        QString displayName = c->name.isEmpty() ? _("no_name") : c->name;
        // 对于群组和会议，如果名称为空，使用降级策略（已在eventpoller中处理）
        if (displayName.length() > 20) {
            displayName = displayName.left(20) + "...";
        }
        
        // emoji和名字之间加空格
        QListWidgetItem* item = new QListWidgetItem(
            QString("%1 %2  %3").arg(statusDot, emoji, displayName)
        );
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
#endif
}

void ContactListWidget::retranslateUi() {
    // 更新Tab按钮文字
    for (int i = 0; i < 4; ++i) {
        if (tabButtons[i]) {
            tabButtons[i]->setText(_(tabNames[i]));
        }
    }
    
    // 更新添加好友输入框
    if (addInput) addInput->setPlaceholderText(_("placeholders.add_friend"));
    
    // 更新加入群组输入框
    if (joinGroupInput) joinGroupInput->setPlaceholderText(_("placeholders.join_group"));
    if (joinGroupBtn) joinGroupBtn->setText(_("buttons.join_group"));
    
    // 更新底部按钮
    if (addBtn) addBtn->setText(_("buttons.add"));
    if (confBtn) confBtn->setText(_("buttons.create_conference"));
    if (groupBtn) groupBtn->setText(_("buttons.create_group"));
    
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
    if (!item) return;
    
    int id = item->data(Qt::UserRole).toInt();
    QString type = item->data(Qt::UserRole + 1).toString();
    QPoint globalPos = lw->mapToGlobal(pos);
    showContextMenuAt(id, type, globalPos);
}
#else
// Qt3: 事件过滤器处理右键
bool ContactListWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == (QListBox*)listWidget && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            QListBox* lb = (QListBox*)listWidget;
            int index = lb->index(lb->selectedItem());
            // 查找对应的联系人
            int count = 0;
            for (uint i = 0; i < allContacts.count(); ++i) {
                Contact* c = allContacts.at(i);
                if (currentFilter != "all") {
                    if (currentFilter == "friend" && c->type != "friend") continue;
                    if (currentFilter == "group" && c->type != "group") continue;
                    if (currentFilter == "conference" && c->type != "conference") continue;
                }
                if (count == index) {
                    contextItemId = c->id;
                    contextItemType = c->type;
                    QPoint globalPos = ((QListBox*)listWidget)->mapToGlobal(mouseEvent->pos());
                    showContextMenuAt(c->id, c->type, globalPos);
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
void ContactListWidget::showContextMenuAt(int id, const QString& type, const QPoint& globalPos) {
    contextItemId = id;
    contextItemType = type;
    
#ifdef QT3_BUILD
    QPopupMenu menu(this);
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
        // 好友：删除、邀请进会议、邀请进群组
#ifdef QT3_BUILD
        menu.insertItem(_("context_menu.delete_friend"), 1);
        menu.insertSeparator();
        menu.insertItem(_("context_menu.invite_to_conference"), 2);
        menu.insertItem(_("context_menu.invite_to_group"), 3);
        int choice = menu.exec(globalPos);
        if (choice == 0) emit viewInfoRequested(id, type);
        else if (choice == 1) emit deleteOrLeaveRequested(id, type);
        else if (choice == 2) emit inviteToConferenceRequested(id);
        else if (choice == 3) emit inviteToGroupRequested(id);
#else
        menu.addSeparator();
        QAction* deleteAction = menu.addAction(_("context_menu.delete_friend"));
        QAction* inviteConfAction = menu.addAction(_("context_menu.invite_to_conference"));
        QAction* inviteGroupAction = menu.addAction(_("context_menu.invite_to_group"));
        QAction* selected = menu.exec(globalPos);
        if (selected == viewInfoAction) emit viewInfoRequested(id, type);
        else if (selected == deleteAction) emit deleteOrLeaveRequested(id, type);
        else if (selected == inviteConfAction) emit inviteToConferenceRequested(id);
        else if (selected == inviteGroupAction) emit inviteToGroupRequested(id);
#endif
    } else if (type == "conference") {
        // 会议：查看成员、离开会议（离开放在最后）
#ifdef QT3_BUILD
        menu.insertItem(_("context_menu.view_members"), 1);
        menu.insertSeparator();
        menu.insertItem(_("context_menu.leave_conference"), 2);
        int choice = menu.exec(globalPos);
        if (choice == 0) emit viewInfoRequested(id, type);
        else if (choice == 1) emit viewMembersRequested(id, type);
        else if (choice == 2) emit deleteOrLeaveRequested(id, type);
#else
        QAction* viewMembersAction = menu.addAction(_("context_menu.view_members"));
        menu.addSeparator();
        QAction* leaveAction = menu.addAction(_("context_menu.leave_conference"));
        QAction* selected = menu.exec(globalPos);
        if (selected == viewInfoAction) emit viewInfoRequested(id, type);
        else if (selected == viewMembersAction) emit viewMembersRequested(id, type);
        else if (selected == leaveAction) emit deleteOrLeaveRequested(id, type);
#endif
    } else if (type == "group") {
        // 群组：查看成员、离开群组（离开放在最后）
#ifdef QT3_BUILD
        menu.insertItem(_("context_menu.view_members"), 1);
        menu.insertSeparator();
        menu.insertItem(_("context_menu.leave_group"), 2);
        int choice = menu.exec(globalPos);
        if (choice == 0) emit viewInfoRequested(id, type);
        else if (choice == 1) emit viewMembersRequested(id, type);
        else if (choice == 2) emit deleteOrLeaveRequested(id, type);
#else
        QAction* viewMembersAction = menu.addAction(_("context_menu.view_members"));
        menu.addSeparator();
        QAction* leaveAction = menu.addAction(_("context_menu.leave_group"));
        QAction* selected = menu.exec(globalPos);
        if (selected == viewInfoAction) emit viewInfoRequested(id, type);
        else if (selected == viewMembersAction) emit viewMembersRequested(id, type);
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

    ToxAPI api;
    bool success = api.joinGroupByChatId(qToUtf8(chatId).data(), "", "");

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

    ToxAPI api;
    int friendId = api.addFriend(qToUtf8(pubkey).data());
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
    ToxAPI api;
    int confId = api.createConference();
    if (confId >= 0) {
        QMessageBox::information(this, _("conference_created"),
                                  _("conference_created"));
    } else {
        QMessageBox::warning(this, _("conference_create_failed"),
                              _("conference_create_failed"));
    }
}

void ContactListWidget::onCreateGroupClicked() {
    ToxAPI api;
    int groupId = api.createGroup("NewGroup", "me", "", false);
    if (groupId >= 0) {
        QMessageBox::information(this, _("group_created"),
                                  _("group_created"));
    } else {
        QMessageBox::warning(this, _("group_create_failed"),
                              _("group_create_failed"));
    }
}
