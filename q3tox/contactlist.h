#ifndef CONTACTLIST_H
#define CONTACTLIST_H

#include <qwidget.h>
#include <qlistbox.h>
#include <qpushbt.h>
#include <qlayout.h>
#include <qstring.h>
#include <qlineedit.h>

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
    
    void setContacts(const QPtrList<Contact>& contacts);
    void clear();
    
signals:
    void contactSelected(int id, const QString& type);
    
private slots:
    void onTabClicked();
    void onItemClicked(QListBoxItem* item);
    
private:
    void updateView();
    
    QListBox* listBox;
    QPtrList<Contact> allContacts;
    QString currentFilter;
    int currentTab;
    QLineEdit* addInput;
};

#endif // CONTACTLIST_H
