#ifndef EVENTPOLLER_H
#define EVENTPOLLER_H

#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QEvent>
#include "api.h"

// Custom event type for thread communication
const int EventListReadyType = QEvent::User + 100;

class EventListEvent : public QEvent {
public:
    EventList events;
    EventListEvent(const EventList& evts) 
        : QEvent((QEvent::Type)EventListReadyType), events(evts) {}
};

enum ApiRequestType {
    ApiLoadAllData,
    ApiSendFriendMessage,
    ApiSendConferenceMessage,
    ApiJoinConference,
    ApiRejectConference,
    ApiAddFriend,
    ApiDeleteFriend,
    ApiCreateConference,
    ApiCreateGroup
};

struct ApiRequest {
    ApiRequestType type;
    int id;
    QString message;
    QString data;
};

class EventPoller : public QThread {
    Q_OBJECT
public:
    explicit EventPoller(QObject* parent = 0);
    ~EventPoller();
    
    void setReceiver(QObject* receiver) { this->receiver = receiver; }
    void postApiRequest(const ApiRequest& req);
    
public slots:
    void stop();
    
signals:
    void pollEvents(quint64 after);
    
protected:
    void run();
    
private slots:
    void onEventsReceived(const EventList& events);
    
private:
    ToxAPI* api;  // Created in run() thread
    QObject* receiver;
    bool running;
    quint64 lastEventId;
    QQueue<ApiRequest> pendingRequests;
    QMutex mutex;
    
    void processApiRequest(const ApiRequest& req);
};

#endif // EVENTPOLLER_H
