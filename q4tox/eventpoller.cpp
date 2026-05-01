#include "eventpoller.h"
#include <qapplication.h>
#include <qmutex.h>
#include <queue>

EventPoller::EventPoller(QObject* parent) : QThread((unsigned int)0), 
    running(false), lastEventId(0), receiver(nullptr) {
    api = new ToxAPI();
}

void EventPoller::run() {
    running = true;
    while (running) {
        // 处理待处理的API请求
        while (!pendingRequests.empty()) {
            ApiRequestEvent* req = pendingRequests.front();
            pendingRequests.pop();
            processApiRequest(req);
            delete req;
        }
        
        // 事件轮询
        std::vector<Event> events = api->pollEvents(lastEventId);
        
        if (!events.empty()) {
            if (receiver) {
                EventListEvent* event = new EventListEvent(events);
                QApplication::postEvent(receiver, event);
            }
            // 更新lastEventId
            for (const auto& e : events) {
                if (e.id > lastEventId) {
                    lastEventId = e.id;
                }
            }
        } else {
            // 无事件：等待2秒后重试
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

void EventPoller::postApiRequest(ApiRequestEvent* req) {
    pendingRequests.push(req);
}

void EventPoller::processApiRequest(ApiRequestEvent* req) {
    if (!req || !receiver) return;
    
    switch (req->type) {
        case ApiLoadAllData: {
            AllDataLoadedEvent* result = new AllDataLoadedEvent();
            
            // 1. 加载self info
            result->success = api->getSelf(result->selfName, 
                                            result->selfStatusMsg,
                                            result->selfConnStatus,
                                            result->selfAddress);
            
            // 2. 加载好友列表和详情
            std::vector<int> friends = api->getFriends();
            for (int id : friends) {
                FriendInfo info;
                if (api->getFriendInfo(id, info)) {
                    ContactData cd;
                    cd.id = id;
                    cd.name = info.name;
                    cd.type = "friend";
                    cd.status = info.connection_status;
                    result->contacts.push_back(cd);
                }
            }
            
            // 3. 加载会议列表
            std::vector<int> conferences = api->getConferences();
            for (int id : conferences) {
                ContactData cd;
                cd.id = id;
                cd.name = "conference_item " + std::to_string(id);
                cd.type = "conference";
                cd.status = "online";
                result->contacts.push_back(cd);
            }
            
            // 一次性发送所有结果
            QApplication::postEvent(receiver, result);
            break;
        }
        case ApiSendFriendMessage: {
            MessageSentResultEvent* result = new MessageSentResultEvent();
            result->chatId = req->id;
            result->chatType = "friend";
            result->success = api->sendFriendMessage(req->id, req->message);
            result->message = req->message;
            QApplication::postEvent(receiver, result);
            break;
        }
        case ApiSendConferenceMessage: {
            MessageSentResultEvent* result = new MessageSentResultEvent();
            result->chatId = req->id;
            result->chatType = "conference";
            result->success = api->sendConferenceMessage(req->id, req->message);
            result->message = req->message;
            QApplication::postEvent(receiver, result);
            break;
        }
        case ApiJoinConference: {
            ConferenceResultEvent* result = new ConferenceResultEvent();
            // 需要从请求中获取friendNumber和cookie
            // 简化：假设req中有这些字段
            result->success = api->joinConference(req->id, req->message);
            QApplication::postEvent(receiver, result);
            break;
        }
        // ... 处理其他请求类型
    }
}
