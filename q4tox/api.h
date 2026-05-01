#ifndef API_H
#define API_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QString>
#include <QVariantMap>

// Forward declaration for cJSON
struct cJSON;

struct FriendInfo {
    int id;
    QString name;
    QString statusMessage;
    QString status;
    QString connectionStatus;
    QString publicKey;
};

struct Event {
    quint64 id;
    QString type;
    QString data; // JSON string
};

typedef QList<Event> EventList;

class ToxAPI : public QObject {
    Q_OBJECT
public:
    explicit ToxAPI(QObject* parent = 0, const QString& baseUrl = "http://localhost:8181");
    
    // Self info
    void getSelf();
    void setSelfName(const QString& name);
    void setSelfStatus(const QString& statusMessage);
    
    // Friends
    void getFriends();
    void getFriendInfo(int friendId);
    void addFriend(const QString& publicKey);
    void deleteFriend(int friendId);
    
    // Messages
    void sendFriendMessage(int friendId, const QString& message);
    
    // Conferences
    void getConferences();
    void createConference();
    void joinConference(int friendNumber, const QString& cookie);
    void rejectConference(int friendNumber);
    void ignoreConference(int friendNumber);
    void sendConferenceMessage(int conferenceId, const QString& message);
    
    // Groups (if supported)
    void getGroups();
    void createGroup();
    void sendGroupMessage(int groupId, const QString& message);
    
    // Events
    void pollEvents(quint64 after = 0);
    
    // Bootstrap
    void bootstrap();

signals:
    // Self
    void selfLoaded(const QVariantMap& data);
    void selfUpdated();
    
    // Friends
    void friendsLoaded(const QList<int>& friendIds);
    void friendInfoLoaded(const FriendInfo& info);
    void friendAdded(int friendId);
    void friendDeleted(int friendId);
    
    // Messages
    void messageSent(bool success);
    void messageReceived(int friendId, const QString& message);
    
    // Conferences
    void conferencesLoaded(const QList<int>& conferenceIds);
    void conferenceCreated(int conferenceId);
    void conferenceJoined(int conferenceId);
    void conferenceMessageReceived(int conferenceId, int peerNumber, const QString& message);
    void conferenceInvited(int friendNumber, const QString& cookie);
    
    // Groups
    void groupsLoaded(const QList<int>& groupIds);
    
    // Events
    void eventsReceived(const EventList& events);
    
    // Error
    void errorOccurred(const QString& error);

public slots:
    // Parse JSON helper (used by other classes)
    QVariantMap parseJsonString(const QString& jsonStr);

private slots:
    void onReplyFinished();

private:
    QNetworkAccessManager* manager;
    QString baseUrl;
    
    // Parse JSON response using cJSON
    QVariantMap parseJsonResponse(const QByteArray& data);
    EventList parseEvents(const QByteArray& data);
    
    // Make HTTP request
    void get(const QString& endpoint, const QString& callbackName);
    void post(const QString& endpoint, const QString& postData, const QString& callbackName);
};

#endif // API_H
