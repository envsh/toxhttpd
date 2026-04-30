#include "contactlist.h"
#include "translator.h"
#include <qmessagebox.h>
#include <qlayout.h>
#include <qpushbt.h>
#include <qlineedit.h>
#include <qlistbox.h>

// 静态数组定义
const char* ContactListWidget::tabFilters[4] = {"all", "friend", "group", "conference"};
const char* ContactListWidget::tabNames[4] = {"tabs.all", "tabs.friends", "tabs.groups", "tabs.conferences"};

ContactListWidget::ContactListWidget(QWidget* parent) : QWidget(parent), currentFilter("all"), currentTab(0) {
    QBoxLayout* layout = new QBoxLayout(this, QBoxLayout::TopToBottom, 0, -1, 0);
    layout->setSpacing(2);
    layout->setMargin(8);
    
    // Tab 标签
    QBoxLayout* tabLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    
    for (int i = 0; i < 4; ++i) {
        QPushButton* tab = new QPushButton(tr(tabNames[i]), this);
        tab->setToggleButton(true);
        if (i == 0) tab->setOn(true);
        tabButtons[i] = tab;
        connect(tab, SIGNAL(clicked()), this, SLOT(onTabClicked()));
        tabLayout->addWidget(tab);
    }
    layout->addLayout(tabLayout);
    
    // 联系人列表 - 使用 QListBox 而不是 QListView
    listBox = new QListBox(this);
    listBox->setSelectionMode(QListBox::Single);
    connect(listBox, SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));
    layout->addWidget(listBox, 1); // stretch
    
    // 底部添加好友区域
    QBoxLayout* addLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    addInput = new QLineEdit(this);
    addInput->setText(tr("placeholders.add_friend"));
    addLayout->addWidget(addInput, 1);
    
    QPushButton* addBtn = new QPushButton(tr("buttons.add"), this);
    addLayout->addWidget(addBtn);
    layout->addLayout(addLayout);
    
    // 创建按钮行
    QBoxLayout* btnLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    QPushButton* confBtn = new QPushButton("🎥 " + tr("buttons.create_conference"), this);
    btnLayout->addWidget(confBtn);
    
    QPushButton* groupBtn = new QPushButton("👥 " + tr("buttons.create_group"), this);
    btnLayout->addWidget(groupBtn);
    layout->addLayout(btnLayout);
}

void ContactListWidget::setContacts(const QPtrList<Contact>& contacts) {
    allContacts = contacts;
    updateView();
}

void ContactListWidget::clear() {
    allContacts.clear();
    listBox->clear();
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
        tabButtons[i]->setOn(i == index);
    }
    
    updateView();
}

void ContactListWidget::onItemClicked(QListBoxItem* item) {
    int index = listBox->index(item);
    int count = 0;
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        
        // 使用相同的过滤逻辑（单数形式）
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
}

void ContactListWidget::onSelectionChanged() {
    qWarning("onSelectionChanged called");
    QListBoxItem* item = listBox->selectedItem();
    if (!item) {
        qWarning("onSelectionChanged: no item selected");
        return;
    }
    
    int index = listBox->index(item);
    int count = 0;
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        
        if (currentFilter != "all") {
            if (currentFilter == "friend" && c->type != "friend") continue;
            if (currentFilter == "group" && c->type != "group") continue;
            if (currentFilter == "conference" && c->type != "conference") continue;
        }
        
        if (count == index) {
            qWarning("Emitting contactSelected: id=%d, type=%s", c->id, c->type.utf8().data());
            emit contactSelected(c->id, c->type);
            return;
        }
        ++count;
    }
}

void ContactListWidget::updateView() {
    // 保存当前选中的联系人信息
    int selectedId = -1;
    QString selectedType;
    QListBoxItem* selItem = listBox->selectedItem();
    if (selItem) {
        int index = listBox->index(selItem);
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
    
    listBox->clear();
    
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
        
        QString displayName = c->name.isEmpty() ? tr("no_name") : c->name;
        if (displayName.length() > 20) {
            displayName = displayName.left(20) + "...";
        }
        
        listBox->insertItem(QString("%1 %2 %3").arg(statusDot, emoji, displayName));
        
        if (c->id == selectedId && c->type == selectedType) {
            targetIndex = newIndex;
        }
        ++newIndex;
    }
    
    if (listBox->count() == 0) {
        listBox->insertItem(tr("no_contacts"));
    } else if (targetIndex >= 0) {
        listBox->setSelected(targetIndex, TRUE);
    }
}
