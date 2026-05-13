#include "eventpoller.h"
#include "compat34.h"
#include "apilog.h"
#include <queue>

EventPoller::EventPoller(QObject* parent) :
#ifdef QT3_BUILD
    QThread((unsigned int)0),
#else
    QThread(parent),
#endif
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
        
        // 检查是否有特殊重启事件
        std::vector<Event> normalEvents;
        bool restartDetected = false;
        
        for (const auto& e : events) {
            if (e.type == "_server_restart") {
                restartDetected = true;
            } else {
                normalEvents.push_back(e);
            }
        }
        
        // 处理重启检测
        if (restartDetected) {
            ALOG_WARN("EventPoller: Server restart detected, resetting lastEventId from", lastEventId, "to 0");
            lastEventId = 0;
            // 只重置不刷新
        }
        
        if (!normalEvents.empty()) {
            if (receiver) {
                EventListEvent* event = new EventListEvent(normalEvents);
                QApplication::postEvent(receiver, event);
            }
            // 更新lastEventId
            for (const auto& e : normalEvents) {
                if (e.id > lastEventId) {
                    lastEventId = e.id;
                }
            }
        } else if (!restartDetected) {
            // 无事件且无重启：等待2秒后重试
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
                    cd.status = info.statusStr;
                    cd.chatId = info.publicKey;
                    cd.iconUrl = info.iconUrl;
                    result->contacts.push_back(cd);
                }
            }
            
            // 3. 加载群组列表（使用新的API返回结构）
            std::vector<GroupInfo> groups = api->getGroups();
            for (const auto& grp : groups) {
                ContactData cd;
                cd.id = grp.groupNumber;
                cd.chatId = grp.chatId;
                cd.isConnected = grp.isConnected;
                // 使用群组名称，为空时使用降级策略
                if (!grp.groupName.empty()) {
                    cd.name = grp.groupName;
                } else {
                    // 名称为空：显示 "number - chat_id前7位"
                    std::string displayName = std::to_string(grp.groupNumber);
                    if (!grp.chatId.empty()) {
                        displayName += " - " + grp.chatId.substr(0, 7);
                    }
                    cd.name = displayName;
                }
                cd.type = "group";
                // 根据真实连接状态设置 status
                cd.status = cd.isConnected ? "online" : "offline";
                cd.statusText = grp.statusText;
                result->contacts.push_back(cd);
            }
            
            // 4. 加载会议列表（使用新的API返回结构）
            std::vector<ConferenceInfo> conferences = api->getConferences();
            for (const auto& conf : conferences) {
                ContactData cd;
                cd.id = conf.conferenceNumber;
                cd.chatId = conf.chatId;
                cd.isConnected = conf.isConnected;
                // 使用会议名称，为空时使用降级策略
                if (!conf.conferenceName.empty()) {
                    cd.name = conf.conferenceName;
                } else {
                    // 名称为空：显示 "number - chat_id前7位"
                    std::string displayName = std::to_string(conf.conferenceNumber);
                    if (!conf.chatId.empty()) {
                        displayName += " - " + conf.chatId.substr(0, 7);
                    }
                    cd.name = displayName;
                }
                cd.type = "conference";
                // 会议使用真实连接状态
                cd.status = cd.isConnected ? "online" : "offline";
                cd.statusText = conf.statusText;
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
        case ApiSendGroupMessage: {
            MessageSentResultEvent* result = new MessageSentResultEvent();
            result->chatId = req->id;
            result->chatType = "group";
            result->success = api->sendGroupMessage(req->id, req->message);
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
        case ApiTranslate: {
            TranslateRequestEvent* treq = static_cast<TranslateRequestEvent*>(req);
            TranslateResultEvent* result = new TranslateResultEvent();
            result->msgIndex = treq->msgIndex;
            TranslateApiResult tr = api->translate(treq->text, treq->targetLang);
            result->success = tr.success;
            result->translatedText = tr.translatedText;
            result->errorMessage = tr.errorMessage;
            QApplication::postEvent(receiver, result);
            break;
        }
        // ... 处理其他请求类型
    }
}
