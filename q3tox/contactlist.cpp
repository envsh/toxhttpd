#include "contactlist.h"
#include "translator.h"
#include "compat34.h"

#ifdef QT3_BUILD
#include <qpopupmenu.h>
#else
#include <QMenu>
#endif

// 静态数组定义
const char* ContactListWidget::tabFilters[4] = {"all", "friend", "group", "conference"};
const char* ContactListWidget::tabNames[4] = {"tabs.all", "tabs.friends", "tabs.groups", "tabs.conferences"};

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
    addInput = new QLineEdit(this);
    addInput->setText(_("placeholders.add_friend"));
    addLayout->addWidget(addInput, 1);
    
    addBtn = new QPushButton(_("buttons.add"), this);
    addLayout->addWidget(addBtn);
    layout->addLayout(addLayout);
    
    // 创建按钮行
    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    confBtn = new QPushButton(_("buttons.create_conference"), this);
    btnLayout->addWidget(confBtn);
    
    groupBtn = new QPushButton(_("buttons.create_group"), this);
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
        
        QString emoji = (c->type == "friend") ? "👤" :
                       (c->type == "group") ? "👥" : "🎙";
        QString statusDot = (c->status == "online" || c->status == "tcp") ? "●" : "○";
        
        QString displayName = c->name.isEmpty() ? _("no_name") : c->name;
        if (displayName.length() > 20) {
            displayName = displayName.left(20) + "...";
        }
        
        lb->insertItem(QString("%1 %2 %3").arg(statusDot, emoji, displayName));
        
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
        
        QString emoji = (c->type == "friend") ? "👤" :
                       (c->type == "group") ? "👥" : "🎙";
        QString statusDot = (c->status == "online" || c->status == "tcp") ? "●" : "○";
        
        QString displayName = c->name.isEmpty() ? _("no_name") : c->name;
        if (displayName.length() > 20) {
            displayName = displayName.left(20) + "...";
        }
        
        QListWidgetItem* item = new QListWidgetItem(
            QString("%1 %2 %3").arg(statusDot, emoji, displayName)
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
    if (addInput) {
        QString text = addInput->text();
        if (text == "输入 Tox ID 添加好友" ||
            text == "Enter Tox ID to add friend" ||
            text == "輸入 Tox ID 添加好友") {
            addInput->setText(_("placeholders.add_friend"));
        }
    }
    
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
        // 好友：删除、邀请进会议
#ifdef QT3_BUILD
        menu.insertItem(_("context_menu.delete_friend"), 1);
        menu.insertSeparator();
        menu.insertItem(_("invite_to_conference"), 2);
        int choice = menu.exec(globalPos);
        if (choice == 0) emit viewInfoRequested(id, type);
        else if (choice == 1) emit deleteOrLeaveRequested(id, type);
        else if (choice == 2) emit inviteToConferenceRequested(id);
#else
        menu.addSeparator();
        QAction* deleteAction = menu.addAction(_("context_menu.delete_friend"));
        QAction* inviteAction = menu.addAction(_("invite_to_conference"));
        QAction* selected = menu.exec(globalPos);
        if (selected == viewInfoAction) emit viewInfoRequested(id, type);
        else if (selected == deleteAction) emit deleteOrLeaveRequested(id, type);
        else if (selected == inviteAction) emit inviteToConferenceRequested(id);
#endif
    } else if (type == "conference") {
        // 会议：离开
#ifdef QT3_BUILD
        menu.insertItem(_("context_menu.leave_conference"), 1);
        int choice = menu.exec(globalPos);
        if (choice == 0) emit viewInfoRequested(id, type);
        else if (choice == 1) emit deleteOrLeaveRequested(id, type);
#else
        menu.addSeparator();
        QAction* leaveAction = menu.addAction(_("context_menu.leave_conference"));
        QAction* selected = menu.exec(globalPos);
        if (selected == viewInfoAction) emit viewInfoRequested(id, type);
        else if (selected == leaveAction) emit deleteOrLeaveRequested(id, type);
#endif
    } else {
        // 群组：暂不支持操作
        menu.exec(globalPos);
    }
}
