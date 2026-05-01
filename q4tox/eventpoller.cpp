#include "eventpoller.h"
#include <QCoreApplication>
#include <QDebug>

EventPoller::EventPoller(QObject* parent) 
    : QThread(parent), api(0), receiver(0), running(false), lastEventId(0) {
}

EventPoller::~EventPoller() {
    stop();
    wait();
    // api is deleted in run() thread
}

void EventPoller::stop() {
    running = false;
}

void EventPoller::postApiRequest(const ApiRequest& req) {
    QMutexLocker locker(&mutex);
    pendingRequests.enqueue(req);
}

void EventPoller::run() {
    // Create API in this thread
    api = new ToxAPI();
    connect(api, SIGNAL(eventsReceived(EventList)), 
            this, SLOT(onEventsReceived(EventList)));
    
    running = true;
    lastEventId = 0;
    
    while (running) {
        // Process pending API requests
        mutex.lock();
        while (!pendingRequests.isEmpty()) {
            ApiRequest req = pendingRequests.dequeue();
            mutex.unlock();
            processApiRequest(req);
            mutex.lock();
        }
        mutex.unlock();
        
        // Poll for events
        if (api) {
            api->pollEvents(lastEventId);
        }
        
        // In real implementation, eventsReceived will update lastEventId
        // For now, sleep and continue polling
        if (running) {
            QThread::sleep(2);
        }
    }
    
    // Cleanup API in this thread
    delete api;
    api = 0;
}

void EventPoller::processApiRequest(const ApiRequest& req) {
    if (!api) return;
    
    switch (req.type) {
        case ApiLoadAllData:
            api->getSelf();
            api->getFriends();
            api->getConferences();
            break;
        case ApiSendFriendMessage:
            api->sendFriendMessage(req.id, req.message);
            break;
        case ApiSendConferenceMessage:
            api->sendConferenceMessage(req.id, req.message);
            break;
        case ApiJoinConference:
            api->joinConference(req.id, req.data);
            break;
        case ApiRejectConference:
            api->rejectConference(req.id);
            break;
        case ApiAddFriend:
            api->addFriend(req.message);
            break;
        case ApiDeleteFriend:
            api->deleteFriend(req.id);
            break;
        case ApiCreateConference:
            api->createConference();
            break;
        case ApiCreateGroup:
            api->createGroup();
            break;
    }
}

void EventPoller::onEventsReceived(const EventList& events) {
    if (events.isEmpty()) return;
    
    // Update lastEventId
    foreach (const Event& e, events) {
        if (e.id > lastEventId) {
            lastEventId = e.id;
        }
    }
    
    // Post event to receiver in main thread
    if (receiver) {
        EventListEvent* event = new EventListEvent(events);
        QCoreApplication::postEvent(receiver, event);
    }
}
