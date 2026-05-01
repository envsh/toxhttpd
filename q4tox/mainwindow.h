#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QListWidgetItem>
#include "eventpoller.h"

class SelfInfoWidget;
class ContactListWidget;
class ChatWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    MainWindow(QWidget* parent = 0);
    ~MainWindow();
    
protected:
    void customEvent(QEvent* event);
    
private slots:
    void onContactSelected(int id, const QString& type);
    void onMessageSent(const QString& message);
    void onLanguageChanged(const QString& langCode);
    void retranslateUi();
    
    // Contact context menu
    void onContactContextMenu(int id, const QString& type, const QPoint& pos);
    void onDeleteFriend();
    
    // Info save
    void onEditInfoRequested(const QString& name, const QString& statusMessage);
    void onBootstrapRequested();
    
    // Contact actions
    void onAddFriendRequested(const QString& publicKey);
    void onCreateConferenceRequested();
    void onCreateGroupRequested();
    
private:
    void handleEvents(const std::vector<Event>& events);
    
    QSplitter* splitter;
    SelfInfoWidget* selfInfoWidget;
    ContactListWidget* contactListWidget;
    ChatWidget* chatWidget;
    EventPoller* eventPoller;
    
    // Current chat state
    int currentChatId;
    QString currentChatType;
};

#endif // MAINWINDOW_H
