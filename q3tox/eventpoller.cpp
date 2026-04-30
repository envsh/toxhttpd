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
            EventList qtEvents = events;  // 直接用 std::vector，让编译器处理拷贝
            if (callbackFunc) {
                callbackFunc(qtEvents, callbackData);
            }
            // 更新 lastEventId
            for (const auto& e : events) {
                if (e.id > lastEventId) {
                    lastEventId = e.id;
                }
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

void EventPoller::setCallback(void (*callback)(const EventList&, void*), void* userData) {
    callbackFunc = callback;
    callbackData = userData;
}
