#include "contactlist.h"
#include "translator.h"
#include <QMenu>
#include <QListWidgetItem>
#include <QLabel>
#include <QHBoxLayout>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>

ContactListWidget::ContactListWidget(QWidget* parent) 
    : QWidget(parent), currentFilter("all") {
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Tab buttons
    QHBoxLayout* tabLayout = new QHBoxLayout();
    QStringList tabs;
    tabs << "all" << "friends" << "groups" << "conferences";
    QStringList tabNames;
    tabNames << _("tabs.all") << _("tabs.friends") << _("tabs.groups") << _("tabs.conferences");
    
    for (int i = 0; i < tabs.size(); ++i) {
        QPushButton* btn = new QPushButton(tabNames[i]);
        btn->setProperty("tabType", tabs[i]);
        btn->setCheckable(true);
        if (tabs[i] == "all") btn->setChecked(true);
        
        connect(btn, SIGNAL(clicked()), this, SLOT(onTabClicked()));
        tabButtons.append(btn);
        tabLayout->addWidget(btn);
    }
    mainLayout->addLayout(tabLayout);
    
    // Contact list
    listWidget = new QListWidget(this);
    listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    
    connect(listWidget, SIGNAL(itemClicked(QListWidgetItem*)), 
            this, SLOT(onItemClicked(QListWidgetItem*)));
    connect(listWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)), 
            this, SLOT(onItemDoubleClicked(QListWidgetItem*)));
    connect(listWidget, SIGNAL(customContextMenuRequested(QPoint)), 
            this, SLOT(showContextMenu(QPoint)));
    
    mainLayout->addWidget(listWidget, 1);
    
    // Bottom area: Add friend
    QHBoxLayout* addLayout = new QHBoxLayout();
    addFriendEdit = new QLineEdit();
    addFriendEdit->setPlaceholderText(_("input_public_key"));
    QPushButton* addFriendBtn = new QPushButton(_("add_friend"));
    addLayout->addWidget(addFriendEdit, 1);
    addLayout->addWidget(addFriendBtn);
    mainLayout->addLayout(addLayout);
    
    connect(addFriendBtn, SIGNAL(clicked()), this, SLOT(onAddFriendClicked()));
    
    // Bottom area: Create buttons (conference left, group right)
    QHBoxLayout* createLayout = new QHBoxLayout();
    QPushButton* confBtn = new QPushButton(_("create_conference"));
    QPushButton* groupBtn = new QPushButton(_("create_group"));
    createLayout->addWidget(confBtn);
    createLayout->addWidget(groupBtn);
    mainLayout->addLayout(createLayout);
    
    connect(confBtn, SIGNAL(clicked()), this, SIGNAL(createConferenceRequested()));
    connect(groupBtn, SIGNAL(clicked()), this, SIGNAL(createGroupRequested()));
}

void ContactListWidget::setContacts(const QList<Contact>& contacts) {
    allContacts = contacts;
    clearContacts();
    
    foreach (const Contact& c, contacts) {
        if (currentFilter != "all") {
            if (currentFilter == "friends" && c.type != "friend") continue;
            if (currentFilter == "groups" && c.type != "group") continue;
            if (currentFilter == "conferences" && c.type != "conference") continue;
        }
        
        addContact(c);
    }
}

void ContactListWidget::addContact(const Contact& contact) {
    QListWidgetItem* item = new QListWidgetItem(listWidget);
    item->setData(Qt::UserRole, contact.id);
    item->setData(Qt::UserRole + 1, contact.type);
    
    // Create widget for item
    QWidget* widget = new QWidget(listWidget);
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(5, 5, 5, 5);
    
    // Status dot
    QLabel* statusDot = new QLabel();
    statusDot->setFixedSize(10, 10);
    QString color = (contact.status == "online" || contact.status == "tcp") ? "green" : "red";
    statusDot->setStyleSheet(QString("background-color: %1; border-radius: 5px;").arg(color));
    
    // Type icon
    QLabel* iconLabel = new QLabel();
    if (contact.type == "friend") iconLabel->setText("👤");
    else if (contact.type == "group") iconLabel->setText("👥");
    else if (contact.type == "conference") iconLabel->setText("🎙");
    
    // Name
    QLabel* nameLabel = new QLabel(contact.name.isEmpty() ? QString::number(contact.id) : contact.name);
    
    layout->addWidget(statusDot);
    layout->addWidget(iconLabel);
    layout->addWidget(nameLabel, 1);
    
    widget->setLayout(layout);
    item->setSizeHint(widget->sizeHint());
    listWidget->setItemWidget(item, widget);
}

void ContactListWidget::updateContact(const Contact& contact) {
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem* item = listWidget->item(i);
        if (item->data(Qt::UserRole).toInt() == contact.id) {
            // Update widget
            QWidget* widget = listWidget->itemWidget(item);
            if (widget) {
                QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(widget->layout());
                if (layout && layout->count() >= 3) {
                    QLabel* nameLabel = qobject_cast<QLabel*>(layout->itemAt(2)->widget());
                    if (nameLabel) {
                        nameLabel->setText(contact.name.isEmpty() ? QString::number(contact.id) : contact.name);
                    }
                }
            }
            break;
        }
    }
}

void ContactListWidget::clearContacts() {
    listWidget->clear();
}

void ContactListWidget::setFilter(const QString& filter) {
    currentFilter = filter;
    setContacts(allContacts);
}

void ContactListWidget::onItemClicked(QListWidgetItem* item) {
    int id = item->data(Qt::UserRole).toInt();
    QString type = item->data(Qt::UserRole + 1).toString();
    emit contactSelected(id, type);
}

void ContactListWidget::onItemDoubleClicked(QListWidgetItem* item) {
    // Show contact info
}

void ContactListWidget::onTabClicked() {
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    
    QString tabType = btn->property("tabType").toString();
    
    // Update button states
    foreach (QPushButton* b, tabButtons) {
        b->setChecked(b == btn);
    }
    
    setFilter(tabType);
}

void ContactListWidget::onAddFriendClicked() {
    QString publicKey = addFriendEdit->text().trimmed();
    if (publicKey.isEmpty()) return;
    
    emit addFriendRequested(publicKey);
    addFriendEdit->clear();
}

void ContactListWidget::showContextMenu(const QPoint& pos) {
    QListWidgetItem* item = listWidget->itemAt(pos);
    if (!item) return;
    
    int id = item->data(Qt::UserRole).toInt();
    QString type = item->data(Qt::UserRole + 1).toString();
    
    QMenu contextMenu(this);
    QAction* infoAction = contextMenu.addAction(_("friend_menu.info"));
    QAction* deleteAction = contextMenu.addAction(_("friend_menu.delete"));
    deleteAction->setIcon(QIcon::fromTheme("edit-delete"));
    
    QAction* selectedAction = contextMenu.exec(listWidget->mapToGlobal(pos));
    
    if (selectedAction == infoAction) {
        // Show info dialog
    } else if (selectedAction == deleteAction) {
        emit contactContextMenu(id, type, pos);
    }
}
