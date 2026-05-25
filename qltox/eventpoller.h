#ifndef EVENTPOLLER_H
#define EVENTPOLLER_H

#include "compat34.h"
#include <vector>
#include <string>
#include <map>
#include <cstdint>
#include <curl/curl.h>
#include <qthread.h>
#include <qmutex.h>

// ── 事件类型常量 ──
const EventType34 EventListReadyType = toEventType34(QEvent::User + 100);
const EventType34 ApiResultReadyType = toEventType34(QEvent::User + 102);

// ── API 请求类型 ──
enum ApiRequestType {
    ApiPollEvents,
    ApiGetSelf,
    ApiGetFriends,
    ApiLoadGroupMembers,
    ApiLoadMessageHistory,
    ApiSendFriendMessage,
    ApiSendConferenceMessage,
    ApiSendGroupMessage,
    ApiAddFriend,
    ApiJoinConference,
    ApiTranslate,
    ApiSetSelfInfo,
    ApiDeleteFriend,
    ApiCreateConference,
    ApiLeaveConference,
    ApiInviteToConference,
    ApiRejectConference,
    ApiIgnoreConference,
    ApiCreateGroup,
    ApiLeaveGroup,
    ApiInviteToGroup,
    ApiJoinGroup,
    ApiJoinGroupByChatId,
    ApiSetGroupSelfName,
    ApiSetGroupTopic,
    ApiSetConferenceTitle,
    ApiGetRandomName,
    ApiLoadAllData,
};

// ── 数据类型（事件契约）──

struct Event {
    uint64_t id;
    std::string type;
    std::string data;
    std::string timestamp;
};

struct PeerInfo {
    int peerNumber;
    std::string name;
    int status = 0;
    std::string statusStr;
    std::string statusText;
    std::string iconUrl;
    int role = 0;
    std::string roleStr;
    std::string publicKey;
    bool isSelf = false;
    std::string peerIp;
};

struct HistoryMessage {
    int64_t rowid;
    std::string message;
    std::string sender_pubkey;
    uint32_t sender_number;
    std::string direction;
    std::string created_at;
};

struct TranslateApiResult {
    bool success = false;
    std::string translatedText;
    std::string errorMessage;
};

struct ContactData {
    int id;
    std::string name;
    std::string type;
    std::string status;
    std::string chatId;
    bool isConnected;
    std::string iconUrl;
    std::string statusText;
    ContactData() : id(-1), isConnected(false) {}
};

typedef std::vector<Event> EventList;

// ── CustomEvent 基类 ──

class EventListEvent : public CustomEventBase {
public:
    EventListEvent(const EventList& evts) : CustomEventBase(EventListReadyType), events(evts) {}
    EventList events;
};

class ApiResultEvent : public CustomEventBase {
public:
    ApiResultEvent(ApiRequestType t) : CustomEventBase(ApiResultReadyType), type(t) {}
    ApiRequestType type;
};

class SelfInfoResultEvent : public ApiResultEvent {
public:
    SelfInfoResultEvent() : ApiResultEvent(ApiGetSelf) {}
    bool success = false;
    std::string name, statusMsg, connStatus, address;
};

class FriendsResultEvent : public ApiResultEvent {
public:
    FriendsResultEvent() : ApiResultEvent(ApiGetFriends) {}
    std::vector<int> friendIds;
};

class MessageSentResultEvent : public ApiResultEvent {
public:
    MessageSentResultEvent(ApiRequestType t) : ApiResultEvent(t), success(false) {}
    bool success;
    std::string message;
    int chatId;
    std::string chatType;
};

class MembersLoadedEvent : public ApiResultEvent {
public:
    MembersLoadedEvent() : ApiResultEvent(ApiLoadGroupMembers) {}
    int contactId = 0;
    std::string contactType;
    std::vector<PeerInfo> members;
};

class MessageHistoryLoadedEvent : public ApiResultEvent {
public:
    MessageHistoryLoadedEvent() : ApiResultEvent(ApiLoadMessageHistory) {}
    int contactId = 0;
    std::string contactType;
    std::vector<HistoryMessage> messages;
};

class TranslateResultEvent : public ApiResultEvent {
public:
    TranslateResultEvent() : ApiResultEvent(ApiTranslate) {}
    int msgIndex = 0;
    bool success = false;
    std::string translatedText;
    std::string errorMessage;
};

class AllDataLoadedEvent : public ApiResultEvent {
public:
    AllDataLoadedEvent() : ApiResultEvent(ApiLoadAllData) {}
    bool success = false;
    std::string selfName, selfStatusMsg, selfConnStatus, selfAddress;
    std::vector<ContactData> contacts;
};

// ── curl_multi HTTP 引擎 ──

struct HttpResponse {
    int httpCode = 0;
    std::string curlErrStr;
    std::string body;
    std::map<std::string, std::string> headers;
};

struct HttpCtx {
    std::string urlStr;
    std::string postData;
    std::string body;
    std::map<std::string, std::string> headers;
    void (*done)(const HttpResponse& resp, void* udata);
    void* udata;
};

class EventPoller : public QThread {
public:
    static void start();
    static void stop();
    static void addRequest(const std::string& url,
                           const std::string& method,
                           const std::string& data,
                           void (*done)(const HttpResponse& resp, void* udata),
                           void* udata = nullptr,
                           int timeoutSec = 35);

private:
    EventPoller();
    void run();
    static EventPoller* s_instance;
    bool running;
    CURLM* multi;
    QMutex multiMutex;
};

#endif
