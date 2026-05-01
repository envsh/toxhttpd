#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QListWidgetItem>
#include "api.h"
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
    
    // API slots
    void onSelfLoaded(const QVariantMap& data);
    void onFriendsLoaded(const QList<int>& friendIds);
    void onFriendInfoLoaded(const FriendInfo& info);
    void onConferencesLoaded(const QList<int>& conferenceIds);
    void onEventsReceived(const EventList& events);
    void onMessageReceived(int friendId, const QString& message);
    void onConferenceMessageReceived(int conferenceId, int peerNumber, const QString& message);
    void onConferenceInvited(int friendNumber, const QString& cookie);
    void onErrorOccurred(const QString& error);
    
    // Info save
    void onEditInfoRequested(const QString& name, const QString& statusMessage);
    void onBootstrapRequested();
    
    // Contact actions
    void onAddFriendRequested(const QString& publicKey);
    void onCreateConferenceRequested();
    void onCreateGroupRequested();
    
private:
    void handleEvents(const EventList& events);
    void loadContactInfo(int id, const QString& type);
    void saveLanguage(const QString& lang);
    QString loadSavedLanguage();
    
    QSplitter* splitter;
    SelfInfoWidget* selfInfoWidget;
    ContactListWidget* contactListWidget;
    ChatWidget* chatWidget;
    EventPoller* eventPoller;
    ToxAPI* api;
    
    // Current chat state
    int currentChatId;
    QString currentChatType;
    QVariantMap selfData;
};

#endif // MAINWINDOW_H
