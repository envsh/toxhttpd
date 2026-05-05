#ifndef EVENTPOLLER_H
#define EVENTPOLLER_H

#include "compat34.h"
#include <vector>
#include <queue>
#include <string>
#include "api.h"

typedef std::vector<Event> EventList;

// 事件类型常量
const int EventListReadyType = QEvent::User + 100;
const int ApiRequestEventType = QEvent::User + 101;
const int ApiResultReadyType = QEvent::User + 102;

// API请求类型
enum ApiRequestType {
    ApiLoadAllData,        // 加载所有初始数据（self + contacts）
    ApiSendFriendMessage,
    ApiSendConferenceMessage,
    ApiJoinConference,
    ApiRejectConference,
    ApiAddFriend
};

// 事件轮询结果
class EventListEvent : public CustomEventBase {
public:
#ifdef QT3_BUILD
    EventListEvent(const EventList& evts) : QCustomEvent(EventListReadyType), events(evts) {}
#else
    EventListEvent(const EventList& evts) : QEvent((QEvent::Type)EventListReadyType), events(evts) {}
#endif
    EventList events;
};

// API请求事件
class ApiRequestEvent : public CustomEventBase {
public:
#ifdef QT3_BUILD
    ApiRequestEvent(ApiRequestType t) : QCustomEvent(ApiRequestEventType), type(t) {}
#else
    ApiRequestEvent(ApiRequestType t) : QEvent((QEvent::Type)ApiRequestEventType), type(t) {}
#endif
    ApiRequestType type;
    // 请求参数
    int id;
    std::string message;
    std::string publicKey;
};

// API结果事件基类
class ApiResultEvent : public CustomEventBase {
public:
#ifdef QT3_BUILD
    ApiResultEvent(ApiRequestType t) : QCustomEvent(ApiResultReadyType), type(t) {}
#else
    ApiResultEvent(ApiRequestType t) : QEvent((QEvent::Type)ApiResultReadyType), type(t) {}
#endif
    ApiRequestType type;
};

// 可跨线程传递的联系人数据
struct ContactData {
    int id;
    std::string name;
    std::string type;
    std::string status;
    std::string chat_id;  // public key
    bool is_connected;     // 群组/会议连接状态
    
    ContactData() : id(-1), is_connected(false) {}
};

// 所有数据加载完成事件
class AllDataLoadedEvent : public ApiResultEvent {
public:
#ifdef QT3_BUILD
    AllDataLoadedEvent() : ApiResultEvent(ApiLoadAllData), success(true) {}
#else
    AllDataLoadedEvent() : ApiResultEvent(ApiLoadAllData), success(true) {}
#endif
    bool success;
    // Self info
    std::string selfName, selfStatusMsg, selfConnStatus, selfAddress;
    
    // Contacts
    std::vector<ContactData> contacts;
};

// 消息发送结果事件
class MessageSentResultEvent : public ApiResultEvent {
public:
#ifdef QT3_BUILD
    MessageSentResultEvent() : ApiResultEvent(ApiSendFriendMessage), success(false) {}
#else
    MessageSentResultEvent() : ApiResultEvent(ApiSendFriendMessage), success(false) {}
#endif
    bool success;
    std::string message;  // 用于乐观更新
    int chatId;
    std::string chatType;
};

// 会议操作结果事件
class ConferenceResultEvent : public ApiResultEvent {
public:
#ifdef QT3_BUILD
    ConferenceResultEvent() : ApiResultEvent(ApiJoinConference), success(false) {}
#else
    ConferenceResultEvent() : ApiResultEvent(ApiJoinConference), success(false) {}
#endif
    bool success;
    int conferenceId;
};

class EventPoller : public QThread {
public:
    explicit EventPoller(QObject* parent = nullptr);
    void run();
    void stop();
    
    void setLastEventId(uint64_t id);
    void setReceiver(QObject* recv) { receiver = recv; }
    
    // API请求接口（主线程调用）
    void postApiRequest(ApiRequestEvent* req);
    
private:
    void processApiRequest(ApiRequestEvent* req);
    
    bool running;
    uint64_t lastEventId;
    ToxAPI* api;
    QObject* receiver;
    std::queue<ApiRequestEvent*> pendingRequests;
};

#endif // EVENTPOLLER_H
