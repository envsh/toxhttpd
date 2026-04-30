#include "eventpoller.h"
#include <qthread.h>

EventPoller::EventPoller(QObject* parent) : QThread((unsigned int)0), running(false), lastEventId(0), callbackFunc(nullptr), callbackData(nullptr) {
    api = new ToxAPI();
}

void EventPoller::run() {
    running = true;
    while (running) {
        // 长轮询，curl 超时 30 秒
        std::vector<Event> events = api->pollEvents(lastEventId);
        
        if (!events.empty()) {
            EventList qtEvents;
            qtEvents.resize(events.size());
            for (uint i = 0; i < events.size(); ++i) {
                qtEvents[i].id = events[i].id;
                qtEvents[i].type = events[i].type;  // std::string 直接赋值
                qtEvents[i].data = events[i].data;
                
                if (events[i].id > lastEventId) {
                    lastEventId = events[i].id;
                }
            }
            if (callbackFunc) {
                callbackFunc(qtEvents, callbackData);
            }
        } else {
            // 无事件：等待 2 秒后重试
            QThread::sleep(2);
        }
    }
}

void EventPoller::stop() {
    running = false;
    wait();
}

void EventPoller::setLastEventId(uint64_t id) {
    lastEventId = id;
}

void EventPoller::setCallback(void (*callback)(EventList, void*), void* userData) {
    callbackFunc = callback;
    callbackData = userData;
}
