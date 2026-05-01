#ifndef CONTACTLIST_H
#define CONTACTLIST_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

struct Contact {
    int id;
    QString name;
    QString type; // "friend", "group", "conference"
    QString status; // "online", "offline", "tcp"
};

class ContactListWidget : public QWidget {
    Q_OBJECT
public:
    explicit ContactListWidget(QWidget* parent = 0);
    
public slots:
    void setContacts(const QList<Contact>& contacts);
    void addContact(const Contact& contact);
    void updateContact(const Contact& contact);
    void clearContacts();
    void setFilter(const QString& filter); // "all", "friends", "groups", "conferences"
    void retranslateUi();
    
signals:
    void contactSelected(int id, const QString& type);
    void contactContextMenu(int id, const QString& type, const QPoint& pos);
    void addFriendRequested(const QString& publicKey);
    void createConferenceRequested();
    void createGroupRequested();
    
private slots:
    void onItemClicked(QListWidgetItem* item);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onTabClicked();
    void showContextMenu(const QPoint& pos);
    void onAddFriendClicked();
    
private:
    QListWidget* listWidget;
    QLineEdit* addFriendEdit;
    QPushButton* addFriendBtn;
    QPushButton* confBtn;
    QPushButton* groupBtn;
    QList<Contact> allContacts;
    QString currentFilter;
    QList<QPushButton*> tabButtons;
};

#endif // CONTACTLIST_H
