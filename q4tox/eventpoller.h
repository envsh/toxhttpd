#ifndef EVENTPOLLER_H
#define EVENTPOLLER_H

#include <qthread.h>
#include <qobject.h>
#include <qevent.h>
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
    ApiAddFriend,
    ApiDeleteFriend,
    ApiCreateConference,
    ApiCreateGroup
};

// 事件轮询结果
class EventListEvent : public QEvent {
public:
    EventListEvent(const EventList& evts) : QEvent((QEvent::Type)EventListReadyType), events(evts) {}
    EventList events;
};

// API请求事件
class ApiRequestEvent : public QEvent {
public:
    ApiRequestEvent(ApiRequestType t) : QEvent((QEvent::Type)ApiRequestEventType), type(t) {}
    
    ApiRequestType type;
    // 请求参数
    int id;
    std::string message;
    std::string publicKey;
};

// API结果事件基类
class ApiResultEvent : public QEvent {
public:
    ApiResultEvent(ApiRequestType t) : QEvent((QEvent::Type)ApiResultReadyType), type(t) {}
    ApiRequestType type;
};

// 可跨线程传递的联系人数据
struct ContactData {
    int id;
    std::string name;
    std::string type;
    std::string status;
    
    ContactData() : id(-1) {}
};

// 所有数据加载完成事件
class AllDataLoadedEvent : public ApiResultEvent {
public:
    AllDataLoadedEvent() : ApiResultEvent(ApiLoadAllData), success(true) {}
    
    bool success;
    // Self info
    std::string selfName, selfStatusMsg, selfConnStatus, selfAddress;
    
    // Contacts
    std::vector<ContactData> contacts;
};

// 消息发送结果事件
class MessageSentResultEvent : public ApiResultEvent {
public:
    MessageSentResultEvent() : ApiResultEvent(ApiSendFriendMessage), success(false) {}
    bool success;
    std::string message;  // 用于乐观更新
    int chatId;
    std::string chatType;
};

// 会议操作结果事件
class ConferenceResultEvent : public ApiResultEvent {
public:
    ConferenceResultEvent() : ApiResultEvent(ApiJoinConference), success(false) {}
    bool success;
    int conferenceId;
};

class EventPoller : public QThread {
    Q_OBJECT
public:
    explicit EventPoller(QObject* parent = 0);
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
