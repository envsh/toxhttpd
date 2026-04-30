#ifndef EVENTPOLLER_H
#define EVENTPOLLER_H

#include <qthread.h>
#include <qstring.h>
#include <qobject.h>
#include <vector>
#include "api.h"

typedef std::vector<Event> EventList;

class EventPoller : public QThread {
public:
    explicit EventPoller(QObject* parent = nullptr);
    void run();
    void stop();
    
    void setLastEventId(uint64_t id);
    
    // 改用普通函数指针回调代替信号槽
    void setCallback(void (*callback)(const EventList&, void*), void* userData);
    
private:
    bool running;
    uint64_t lastEventId;
    ToxAPI* api;
    void (*callbackFunc)(const EventList&, void*);
    void* callbackData;
};

#endif // EVENTPOLLER_H
