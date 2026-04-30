#include "contactlist.h"
#include "translator.h"
#include <qmessagebox.h>
#include <qlayout.h>
#include <qpushbt.h>
#include <qlineedit.h>
#include <qlistbox.h>

ContactListWidget::ContactListWidget(QWidget* parent) : QWidget(parent), currentFilter("all"), currentTab(0) {
    QBoxLayout* layout = new QBoxLayout(this, QBoxLayout::TopToBottom, 0, -1, 0);
    layout->setSpacing(2);
    layout->setMargin(8);
    
    // Tab 标签
    QBoxLayout* tabLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    QString tabNames[4];
    tabNames[0] = tr("tabs.all");
    tabNames[1] = tr("tabs.friends");
    tabNames[2] = tr("tabs.groups");
    tabNames[3] = tr("tabs.conferences");
    
    for (int i = 0; i < 4; ++i) {
        QPushButton* tab = new QPushButton(tabNames[i], this);
        tab->setToggleButton(true);
        if (i == 0) tab->setOn(true);
        connect(tab, SIGNAL(clicked()), this, SLOT(onTabClicked()));
        tabLayout->addWidget(tab);
    }
    layout->addLayout(tabLayout);
    
    // 联系人列表 - 使用 QListBox 而不是 QListView
    listBox = new QListBox(this);
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
    // 简化：通过 sender() 判断哪个按钮被点击
    updateView();
}

void ContactListWidget::onItemClicked(QListBoxItem* item) {
    int index = listBox->index(item);
    int count = 0;
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (currentFilter != "all" &&
            !(currentFilter == "friends" && c->type == "friend") &&
            !(currentFilter == "groups" && c->type == "group") &&
            !(currentFilter == "conferences" && c->type == "conference")) {
            continue;
        }
        
        if (count == index) {
            emit contactSelected(c->id, c->type);
            return;
        }
        ++count;
    }
}

void ContactListWidget::updateView() {
    listBox->clear();
    
    for (uint i = 0; i < allContacts.count(); ++i) {
        Contact* c = allContacts.at(i);
        if (currentFilter != "all" &&
            !(currentFilter == "friends" && c->type == "friend") &&
            !(currentFilter == "groups" && c->type == "group") &&
            !(currentFilter == "conferences" && c->type == "conference")) {
            continue;
        }
        
        QString emoji = (c->type == "friend") ? "👤" :
                       (c->type == "group") ? "👥" : "🎙";
        QString statusDot = (c->status == "online" || c->status == "tcp") ? "●" : "○";
        
        QString displayName = c->name.isEmpty() ? tr("no_name") : c->name;
        if (displayName.length() > 20) {
            displayName = displayName.left(20) + "...";
        }
        
        listBox->insertItem(QString("%1 %2 %3").arg(statusDot, emoji, displayName));
    }
    
    if (listBox->count() == 0) {
        listBox->insertItem(tr("no_contacts"));
    }
}
