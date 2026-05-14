#include "restapi.h"
#include "apilog.h"
#include "cJSON.h"
#include <curl/curl.h>
#include <cstdlib>
#include <cctype>

// ── Static members ──
QObject* ToxAPI::s_target = nullptr;
std::string ToxAPI::s_baseUrl = "http://localhost:8181";
uint64_t ToxAPI::s_lastEventId = 0;
bool ToxAPI::s_pollRunning = false;

// ── Helpers ──

static std::string jsonStr(cJSON* item) {
    return (item && cJSON_IsString(item)) ? std::string(cJSON_GetStringValue(item)) : "";
}

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// ── Sync helper write callback ──
static size_t syncWriteCb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

// ── LoadAllData chain ──
struct LoadChain {
    AllDataLoadedEvent* result;
    int step = 0;
    std::vector<int> friendIds;
    int friendIdx = 0;
    LoadChain() : result(new AllDataLoadedEvent()) {}
};

// ── Private ──

void ToxAPI::request(ApiRequestType type, const std::string& endpoint,
                      const std::string& method, const std::string& data,
                      ApiCtx* ctx, int timeoutSec) {
    std::string url = buildUrl(endpoint);
    if (!ctx) ctx = new ApiCtx(type);
    EventPoller::addRequest(url, method, data, onHttpDone, ctx, timeoutSec);
}

void ToxAPI::onHttpDone(int httpCode, const std::string& body,
                         const std::map<std::string, std::string>* headers,
                         void* udata) {
    auto* ctx = static_cast<ApiCtx*>(udata);
    dispatchResult(ctx, httpCode, body, headers);
    delete ctx;
}

// ── Public API ──

void ToxAPI::setEventTarget(QObject* target) { s_target = target; }
void ToxAPI::setBaseUrl(const std::string& url) { s_baseUrl = url; }

void ToxAPI::startPollEvent() {
    s_pollRunning = true;
    s_lastEventId = 0;
    pollEvents();
}

void ToxAPI::stopPollEvent() {
    s_pollRunning = false;
}

void ToxAPI::pollEvents() {
    EventPoller::addRequest(
        buildUrl("/api/events?after=" + std::to_string(s_lastEventId)),
        "GET", "", onHttpDone, new ApiCtx(ApiPollEvents), 35);
}

void ToxAPI::loadAllData() {
    auto* chain = new LoadChain();
    auto* ctx = new ApiCtx(ApiLoadAllData);
    ctx->ptr = chain;
    EventPoller::addRequest(buildUrl("/api/self"), "GET", "", onHttpDone, ctx, 35);
}

void ToxAPI::getSelf() {
    request(ApiGetSelf, "/api/self", "GET");
}

void ToxAPI::getFriends() {
    request(ApiGetFriends, "/api/friends", "GET");
}

void ToxAPI::sendFriendMessage(int friendId, const std::string& message) {
    auto* ctx = new ApiCtx(ApiSendFriendMessage, friendId, message);
    request(ApiSendFriendMessage, "/api/messages", "POST",
            "friend_id=" + std::to_string(friendId) + "&message=" + urlEncode(message), ctx);
}

void ToxAPI::sendConferenceMessage(int conferenceId, const std::string& message) {
    auto* ctx = new ApiCtx(ApiSendConferenceMessage, conferenceId, message);
    request(ApiSendConferenceMessage, "/api/conference_messages", "POST",
            "conference_id=" + std::to_string(conferenceId) + "&message=" + urlEncode(message), ctx);
}

void ToxAPI::sendGroupMessage(int groupId, const std::string& message) {
    auto* ctx = new ApiCtx(ApiSendGroupMessage, groupId, message);
    request(ApiSendGroupMessage, "/api/group_messages", "POST",
            "group_number=" + std::to_string(groupId) + "&message=" + urlEncode(message), ctx);
}

void ToxAPI::addFriend(const std::string& publicKey) {
    request(ApiAddFriend, "/api/friends", "POST",
            "public_key=" + urlEncode(publicKey));
}

void ToxAPI::deleteFriend(int friendId) {
    request(ApiDeleteFriend, "/api/friend_delete", "POST",
            "friend_id=" + std::to_string(friendId));
}

void ToxAPI::getGroupMembers(int groupId) {
    auto* ctx = new ApiCtx(ApiLoadGroupMembers, groupId, "group");
    request(ApiLoadGroupMembers, "/api/group/members?group_number="
            + std::to_string(groupId), "GET", "", ctx);
}

void ToxAPI::getConferenceMembers(int confId) {
    auto* ctx = new ApiCtx(ApiLoadGroupMembers, confId, "conference");
    request(ApiLoadGroupMembers, "/api/conference/members?conference_id="
            + std::to_string(confId), "GET", "", ctx);
}

void ToxAPI::getMessagesHistory(int contactId, const std::string& contactType) {
    auto* ctx = new ApiCtx(ApiLoadMessageHistory, contactId, contactType);
    request(ApiLoadMessageHistory,
            "/api/messages/history?contact_id=" + std::to_string(contactId)
            + "&contact_type=" + contactType, "GET", "", ctx);
}

void ToxAPI::joinConference(int friendNumber, const std::string& cookie) {
    request(ApiJoinConference, "/api/conferences/join", "POST",
            "friend_number=" + std::to_string(friendNumber) + "&cookie=" + cookie);
}

void ToxAPI::rejectConference(int friendNumber) {
    request(ApiRejectConference, "/api/conferences/reject", "POST",
            "friend_number=" + std::to_string(friendNumber));
}

void ToxAPI::ignoreConference(int friendNumber) {
    request(ApiIgnoreConference, "/api/conferences/ignore", "POST",
            "friend_number=" + std::to_string(friendNumber));
}

void ToxAPI::createConference() {
    request(ApiCreateConference, "/api/conferences", "POST");
}

void ToxAPI::leaveConference(int confId) {
    request(ApiLeaveConference, "/api/conference_delete", "POST",
            "conference_id=" + std::to_string(confId));
}

void ToxAPI::inviteToConference(int friendId, int confId) {
    request(ApiInviteToConference, "/api/conference_invite", "POST",
            "friend_id=" + std::to_string(friendId) + "&conference_id=" + std::to_string(confId));
}

void ToxAPI::createGroup(const std::string& groupName, const std::string& creatorName,
                          const std::string& password, bool isPrivate) {
    std::string data = "group_name=" + urlEncode(groupName) + "&name=" + urlEncode(creatorName);
    if (!password.empty()) data += "&password=" + urlEncode(password);
    if (isPrivate) data += "&privacy_state=private";
    request(ApiCreateGroup, "/api/groups", "POST", data);
}

void ToxAPI::leaveGroup(int groupId) {
    request(ApiLeaveGroup, "/api/groups/leave", "POST",
            "group_id=" + std::to_string(groupId));
}

void ToxAPI::inviteToGroup(int friendId, int groupId) {
    request(ApiInviteToGroup, "/api/groups/invite", "POST",
            "friend_id=" + std::to_string(friendId) + "&group_id=" + std::to_string(groupId));
}

void ToxAPI::joinGroup(int friendNumber, const std::string& chatId,
                        const std::string& name, const std::string& password) {
    std::string data = "friend_number=" + std::to_string(friendNumber) + "&chat_id=" + chatId;
    if (!name.empty()) data += "&name=" + urlEncode(name);
    if (!password.empty()) data += "&password=" + urlEncode(password);
    request(ApiJoinGroup, "/api/groups/join", "POST", data);
}

void ToxAPI::joinGroupByChatId(const std::string& chatId,
                                const std::string& name, const std::string& password) {
    std::string data = "chat_id=" + chatId;
    if (!name.empty()) data += "&name=" + urlEncode(name);
    if (!password.empty()) data += "&password=" + urlEncode(password);
    request(ApiJoinGroupByChatId, "/api/groups/join", "POST", data);
}

void ToxAPI::setGroupSelfName(int groupId, const std::string& name) {
    request(ApiSetGroupSelfName, "/api/groups/set-name", "POST",
            "group_number=" + std::to_string(groupId) + "&name=" + urlEncode(name));
}

void ToxAPI::getRandomName() {
    request(ApiGetRandomName, "/api/random-name", "GET");
}

void ToxAPI::setSelfInfo(const std::string& name, const std::string& statusMessage) {
    std::string data;
    bool hasParams = false;
    if (!name.empty()) {
        data += "name=" + urlEncode(name);
        hasParams = true;
    }
    if (!statusMessage.empty()) {
        if (hasParams) data += "&";
        data += "status_message=" + urlEncode(statusMessage);
        hasParams = true;
    }
    if (!hasParams) return;
    request(ApiSetSelfInfo, "/api/self", "POST", data);
}

void ToxAPI::translate(const std::string& text, const std::string& toLang, int msgIndex) {
    auto* ctx = new ApiCtx(ApiTranslate, msgIndex, text, toLang);
    std::string postData = "{\"text\":\"" + jsonEscape(text) + "\",\"to\":\"" + toLang + "\"}";
    request(ApiTranslate, "/api/translate", "POST", postData, ctx);
}

std::string ToxAPI::urlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (!curl) return str;
    char* encoded = curl_easy_escape(curl, str.c_str(), (int)str.length());
    std::string result = encoded ? encoded : str;
    if (encoded) curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

std::string ToxAPI::buildUrl(const std::string& endpoint) {
    return s_baseUrl + endpoint;
}

// ── Sync helpers ──

bool ToxAPI::syncRequest(const std::string& endpoint,
                          const std::string& method,
                          std::string& outBody,
                          const std::string& data,
                          int timeoutSec) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    std::string url = buildUrl(endpoint);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, syncWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeoutSec);
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)data.size());
    }
    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    return (httpCode == 200 && !outBody.empty());
}

bool ToxAPI::getFriendInfo(int id, FriendInfo& info) {
    std::string body;
    if (!syncRequest("/api/friend", "POST", body,
                     "friend_id=" + std::to_string(id)))
        return false;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) return false;
    info.id = id;
    info.name = jsonStr(cJSON_GetObjectItem(root, "name"));
    info.publicKey = jsonStr(cJSON_GetObjectItem(root, "publicKey"));
    info.statusStr = jsonStr(cJSON_GetObjectItem(root, "statusStr"));
    info.statusText = jsonStr(cJSON_GetObjectItem(root, "statusText"));
    info.iconUrl = jsonStr(cJSON_GetObjectItem(root, "iconUrl"));
    cJSON_Delete(root);
    return true;
}

std::vector<GroupInfo> ToxAPI::getGroupsSync() {
    std::vector<GroupInfo> groups;
    std::string body;
    if (!syncRequest("/api/groups", "GET", body))
        return groups;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) return groups;
    cJSON* arr = cJSON_GetObjectItem(root, "groups");
    if (arr && cJSON_IsArray(arr)) {
        int n = cJSON_GetArraySize(arr);
        for (int i = 0; i < n; ++i) {
            cJSON* item = cJSON_GetArrayItem(arr, i);
            if (!item) continue;
            GroupInfo g;
            cJSON* v = cJSON_GetObjectItem(item, "groupNumber");
            if (v) g.groupNumber = v->valueint;
            g.groupName = jsonStr(cJSON_GetObjectItem(item, "groupName"));
            g.chatId = jsonStr(cJSON_GetObjectItem(item, "chatId"));
            v = cJSON_GetObjectItem(item, "isConnected");
            g.isConnected = v ? (v->valueint == 1) : false;
            g.statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
            groups.push_back(g);
        }
    }
    cJSON_Delete(root);
    return groups;
}

std::vector<ConferenceInfo> ToxAPI::getConferencesSync() {
    std::vector<ConferenceInfo> conferences;
    std::string body;
    if (!syncRequest("/api/conferences", "GET", body))
        return conferences;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) return conferences;
    cJSON* arr = cJSON_GetObjectItem(root, "conferences");
    if (arr && cJSON_IsArray(arr)) {
        int n = cJSON_GetArraySize(arr);
        for (int i = 0; i < n; ++i) {
            cJSON* item = cJSON_GetArrayItem(arr, i);
            if (!item) continue;
            ConferenceInfo c;
            cJSON* v = cJSON_GetObjectItem(item, "conferenceNumber");
            if (v) c.conferenceNumber = v->valueint;
            c.conferenceName = jsonStr(cJSON_GetObjectItem(item, "conferenceName"));
            c.chatId = jsonStr(cJSON_GetObjectItem(item, "chatId"));
            v = cJSON_GetObjectItem(item, "isConnected");
            c.isConnected = v ? (v->valueint == 1) : false;
            c.statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
            conferences.push_back(c);
        }
    }
    cJSON_Delete(root);
    return conferences;
}

static std::vector<PeerInfo> parseMembersResponse(const std::string& body) {
    std::vector<PeerInfo> members;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) return members;
    cJSON* selfPpItem = cJSON_GetObjectItem(root, "selfPeerNumber");
    int selfPeerNumber = selfPpItem ? selfPpItem->valueint : -1;
    cJSON* membersItem = cJSON_GetObjectItem(root, "members");
    if (membersItem && cJSON_IsArray(membersItem)) {
        int n = cJSON_GetArraySize(membersItem);
        for (int i = 0; i < n; ++i) {
            cJSON* item = cJSON_GetArrayItem(membersItem, i);
            if (!item) continue;
            PeerInfo info;
            cJSON* v;
            v = cJSON_GetObjectItem(item, "peerNumber");
            if (v) info.peerNumber = v->valueint;
            info.name = jsonStr(cJSON_GetObjectItem(item, "name"));
            v = cJSON_GetObjectItem(item, "status");
            if (v) info.status = v->valueint;
            info.statusStr = jsonStr(cJSON_GetObjectItem(item, "statusStr"));
            info.statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
            info.iconUrl = jsonStr(cJSON_GetObjectItem(item, "iconUrl"));
            v = cJSON_GetObjectItem(item, "role");
            if (v) info.role = v->valueint;
            info.roleStr = jsonStr(cJSON_GetObjectItem(item, "roleStr"));
            info.publicKey = jsonStr(cJSON_GetObjectItem(item, "publicKey"));
            info.peerIp = jsonStr(cJSON_GetObjectItem(item, "peerIp"));
            info.isSelf = (info.peerNumber == selfPeerNumber);
            members.push_back(info);
        }
    }
    cJSON_Delete(root);
    return members;
}

std::vector<PeerInfo> ToxAPI::getConferenceMembersSync(int confId) {
    std::string body;
    if (!syncRequest("/api/conference/members?conference_id=" + std::to_string(confId), "GET", body))
        return std::vector<PeerInfo>();
    return parseMembersResponse(body);
}

std::vector<PeerInfo> ToxAPI::getGroupMembersSync(int groupId) {
    std::string body;
    if (!syncRequest("/api/group/members?group_number=" + std::to_string(groupId), "GET", body))
        return std::vector<PeerInfo>();
    return parseMembersResponse(body);
}

std::string ToxAPI::getRandomNameSync() {
    std::string body;
    if (!syncRequest("/api/random-name", "GET", body))
        return "";
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) return "";
    std::string name = jsonStr(cJSON_GetObjectItem(root, "name"));
    cJSON_Delete(root);
    return name;
}

int ToxAPI::addFriendSync(const std::string& publicKey) {
    std::string body;
    if (!syncRequest("/api/friends", "POST", body,
                     "public_key=" + urlEncode(publicKey)))
        return -1;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) return -1;
    cJSON* v = cJSON_GetObjectItem(root, "friend_id");
    int id = v ? v->valueint : -1;
    cJSON_Delete(root);
    return id;
}

int ToxAPI::createConferenceSync() {
    std::string body;
    if (!syncRequest("/api/conferences", "POST", body))
        return -1;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) return -1;
    cJSON* v = cJSON_GetObjectItem(root, "conference_id");
    int id = v ? v->valueint : -1;
    cJSON_Delete(root);
    return id;
}

int ToxAPI::createGroupSync(const std::string& groupName, const std::string& creatorName,
                             const std::string& password, bool isPrivate) {
    std::string data = "group_name=" + urlEncode(groupName) + "&name=" + urlEncode(creatorName);
    if (!password.empty()) data += "&password=" + urlEncode(password);
    if (isPrivate) data += "&privacy_state=private";
    std::string body;
    if (!syncRequest("/api/groups", "POST", body, data))
        return -1;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) return -1;
    cJSON* v = cJSON_GetObjectItem(root, "group_number");
    int id = v ? v->valueint : -1;
    cJSON_Delete(root);
    return id;
}

bool ToxAPI::deleteFriendSync(int friendId) {
    std::string body;
    return syncRequest("/api/friend_delete", "POST", body,
                       "friend_id=" + std::to_string(friendId));
}

bool ToxAPI::leaveConferenceSync(int confId) {
    std::string body;
    return syncRequest("/api/conference_delete", "POST", body,
                       "conference_id=" + std::to_string(confId));
}

bool ToxAPI::setSelfInfoSync(const std::string& name, const std::string& statusMessage) {
    std::string data;
    bool hasParams = false;
    if (!name.empty()) {
        data += "name=" + urlEncode(name);
        hasParams = true;
    }
    if (!statusMessage.empty()) {
        if (hasParams) data += "&";
        data += "status_message=" + urlEncode(statusMessage);
        hasParams = true;
    }
    if (!hasParams) return true;
    std::string body;
    return syncRequest("/api/self", "POST", body, data);
}

bool ToxAPI::getSelfSync(std::string& name, std::string& statusMsg,
                          std::string& connStatus, std::string& address) {
    std::string body;
    if (!syncRequest("/api/self", "GET", body))
        return false;
    cJSON* root = cJSON_Parse(body.c_str());
    if (!root) return false;
    name = jsonStr(cJSON_GetObjectItem(root, "name"));
    statusMsg = jsonStr(cJSON_GetObjectItem(root, "status_message"));
    connStatus = jsonStr(cJSON_GetObjectItem(root, "connection_status"));
    address = jsonStr(cJSON_GetObjectItem(root, "address"));
    cJSON_Delete(root);
    return true;
}

bool ToxAPI::joinGroupSync(int friendNumber, const std::string& chatId,
                            const std::string& name, const std::string& password) {
    std::string data = "friend_number=" + std::to_string(friendNumber) + "&chat_id=" + chatId;
    if (!name.empty()) data += "&name=" + urlEncode(name);
    if (!password.empty()) data += "&password=" + urlEncode(password);
    std::string body;
    return syncRequest("/api/groups/join", "POST", body, data);
}

bool ToxAPI::inviteToConferenceSync(int friendId, int confId) {
    std::string body;
    return syncRequest("/api/conference_invite", "POST", body,
                       "friend_id=" + std::to_string(friendId)
                       + "&conference_id=" + std::to_string(confId));
}

bool ToxAPI::inviteToGroupSync(int friendId, int groupId) {
    std::string body;
    return syncRequest("/api/groups/invite", "POST", body,
                       "friend_id=" + std::to_string(friendId)
                       + "&group_id=" + std::to_string(groupId));
}

bool ToxAPI::setGroupSelfNameSync(int groupId, const std::string& name) {
    std::string body;
    return syncRequest("/api/groups/set-name", "POST", body,
                       "group_number=" + std::to_string(groupId)
                       + "&name=" + urlEncode(name));
}

bool ToxAPI::joinGroupByChatIdSync(const std::string& chatId,
                                    const std::string& name, const std::string& password) {
    std::string data = "chat_id=" + chatId;
    if (!name.empty()) data += "&name=" + urlEncode(name);
    if (!password.empty()) data += "&password=" + urlEncode(password);
    std::string body;
    return syncRequest("/api/groups/join", "POST", body, data);
}

// ── dispatchResult ──

void ToxAPI::dispatchResult(ApiCtx* ctx, int httpCode, const std::string& body,
                             const std::map<std::string, std::string>* headers) {
    if (!s_target) return;
    int type = ctx->type;

    switch (type) {

    case ApiPollEvents: {
        bool restartDetected = false;
        auto it = headers->find("x-server-next-id");
        if (it != headers->end()) {
            char* endptr = nullptr;
            uint64_t serverNextId = std::strtoull(it->second.c_str(), &endptr, 10);
            if (endptr != it->second.c_str() && serverNextId <= s_lastEventId) {
                restartDetected = true;
                s_lastEventId = 0;
            }
        }

        if (httpCode != 200 || body.empty()) {
            if (s_pollRunning)
                EventPoller::addRequest(
                    buildUrl("/api/events?after=" + std::to_string(s_lastEventId)),
                    "GET", "", onHttpDone, new ApiCtx(ApiPollEvents), 35);
            break;
        }

        std::vector<Event> events;
        if (restartDetected) {
            Event e;
            e.id = 0; e.type = "_server_restart"; e.data = ""; e.timestamp = "";
            events.push_back(e);
        }

        cJSON* root = cJSON_Parse(body.c_str());
        if (root && cJSON_IsArray(root)) {
            int count = cJSON_GetArraySize(root);
            for (int i = 0; i < count; ++i) {
                cJSON* item = cJSON_GetArrayItem(root, i);
                if (!item) continue;
                Event e;
                cJSON* v = cJSON_GetObjectItem(item, "event_id");
                e.id = v ? (uint64_t)v->valuedouble : 0;
                e.type = jsonStr(cJSON_GetObjectItem(item, "event_type"));
                e.data = jsonStr(cJSON_GetObjectItem(item, "data"));
                e.timestamp = jsonStr(cJSON_GetObjectItem(item, "timestamp"));
                events.push_back(e);
                if (e.id > s_lastEventId) s_lastEventId = e.id;
            }
        }
        cJSON_Delete(root);

        QApplication::postEvent(s_target, new EventListEvent(events));

        if (s_pollRunning)
            EventPoller::addRequest(
                buildUrl("/api/events?after=" + std::to_string(s_lastEventId)),
                "GET", "", onHttpDone, new ApiCtx(ApiPollEvents), 35);
        break;
    }

    case ApiGetSelf: {
        auto* ev = new SelfInfoResultEvent();
        if (httpCode != 200 || body.empty()) {
            ev->success = false;
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(body.c_str());
        if (!root) { ev->success = false; QApplication::postEvent(s_target, ev); break; }
        ev->success = true;
        ev->name = jsonStr(cJSON_GetObjectItem(root, "name"));
        ev->statusMsg = jsonStr(cJSON_GetObjectItem(root, "status_message"));
        ev->connStatus = jsonStr(cJSON_GetObjectItem(root, "connection_status"));
        ev->address = jsonStr(cJSON_GetObjectItem(root, "address"));
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiGetFriends: {
        auto* ev = new FriendsResultEvent();
        if (httpCode != 200 || body.empty()) {
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(body.c_str());
        if (!root) { QApplication::postEvent(s_target, ev); break; }
        cJSON* arr = cJSON_GetObjectItem(root, "friends");
        if (arr && cJSON_IsArray(arr)) {
            int n = cJSON_GetArraySize(arr);
            for (int i = 0; i < n; ++i) {
                cJSON* item = cJSON_GetArrayItem(arr, i);
                if (item) ev->friendIds.push_back(item->valueint);
            }
        }
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiSendFriendMessage:
    case ApiSendConferenceMessage:
    case ApiSendGroupMessage: {
        auto* ev = new MessageSentResultEvent((ApiRequestType)type);
        ev->success = (httpCode == 200 && !body.empty());
        ev->chatId = ctx->id;
        ev->message = ctx->str1;
        switch (ctx->type) {
            case ApiSendFriendMessage: ev->chatType = "friend"; break;
            case ApiSendConferenceMessage: ev->chatType = "conference"; break;
            case ApiSendGroupMessage: ev->chatType = "group"; break;
        }
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiAddFriend:
    case ApiDeleteFriend:
    case ApiCreateConference:
    case ApiLeaveConference:
    case ApiInviteToConference:
    case ApiRejectConference:
    case ApiIgnoreConference:
    case ApiCreateGroup:
    case ApiLeaveGroup:
    case ApiInviteToGroup:
    case ApiJoinGroup:
    case ApiJoinGroupByChatId:
    case ApiSetGroupSelfName:
    case ApiSetSelfInfo:
        break; // fire-and-forget

    case ApiJoinConference: {
        auto* ev = new ApiResultEvent(ApiJoinConference);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiGetRandomName: {
        auto* ev = new ApiResultEvent(ApiGetRandomName);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiLoadGroupMembers: {
        auto* ev = new MembersLoadedEvent();
        ev->contactId = ctx->id;
        ev->contactType = ctx->str1;
        if (httpCode != 200 || body.empty()) {
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(body.c_str());
        if (!root) { QApplication::postEvent(s_target, ev); break; }

        cJSON* selfPpItem = cJSON_GetObjectItem(root, "selfPeerNumber");
        int selfPeerNumber = selfPpItem ? selfPpItem->valueint : -1;

        cJSON* membersItem = cJSON_GetObjectItem(root, "members");
        if (membersItem && cJSON_IsArray(membersItem)) {
            int n = cJSON_GetArraySize(membersItem);
            for (int i = 0; i < n; ++i) {
                cJSON* item = cJSON_GetArrayItem(membersItem, i);
                if (!item) continue;
                PeerInfo info;
                cJSON* v;
                v = cJSON_GetObjectItem(item, "peerNumber");
                if (v) info.peerNumber = v->valueint;
                info.name = jsonStr(cJSON_GetObjectItem(item, "name"));
                v = cJSON_GetObjectItem(item, "status");
                if (v) info.status = v->valueint;
                info.statusStr = jsonStr(cJSON_GetObjectItem(item, "statusStr"));
                info.statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
                info.iconUrl = jsonStr(cJSON_GetObjectItem(item, "iconUrl"));
                v = cJSON_GetObjectItem(item, "role");
                if (v) info.role = v->valueint;
                info.roleStr = jsonStr(cJSON_GetObjectItem(item, "roleStr"));
                info.publicKey = jsonStr(cJSON_GetObjectItem(item, "publicKey"));
                info.peerIp = jsonStr(cJSON_GetObjectItem(item, "peerIp"));
                info.isSelf = (info.peerNumber == selfPeerNumber);
                ev->members.push_back(info);
            }
        }
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiLoadMessageHistory: {
        auto* ev = new MessageHistoryLoadedEvent();
        ev->contactId = ctx->id;
        ev->contactType = ctx->str1;
        if (httpCode != 200 || body.empty()) {
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(body.c_str());
        if (!root) { QApplication::postEvent(s_target, ev); break; }
        cJSON* msgsItem = cJSON_GetObjectItem(root, "messages");
        if (msgsItem && cJSON_IsArray(msgsItem)) {
            int n = cJSON_GetArraySize(msgsItem);
            for (int i = 0; i < n; ++i) {
                cJSON* msg = cJSON_GetArrayItem(msgsItem, i);
                if (!msg) continue;
                HistoryMessage hm;
                cJSON* v = cJSON_GetObjectItem(msg, "rowid");
                if (v) hm.rowid = (int64_t)v->valuedouble;
                hm.message = jsonStr(cJSON_GetObjectItem(msg, "message"));
                hm.sender_pubkey = jsonStr(cJSON_GetObjectItem(msg, "sender_pubkey"));
                v = cJSON_GetObjectItem(msg, "sender_number");
                if (v) hm.sender_number = (uint32_t)v->valuedouble;
                hm.direction = jsonStr(cJSON_GetObjectItem(msg, "direction"));
                hm.created_at = jsonStr(cJSON_GetObjectItem(msg, "created_at"));
                ev->messages.push_back(hm);
            }
        }
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiTranslate: {
        auto* ev = new TranslateResultEvent();
        ev->msgIndex = ctx->id;
        if (httpCode != 200 || body.empty()) {
            ev->errorMessage = "NETWORK_ERROR: cannot connect to server";
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* root = cJSON_Parse(body.c_str());
        if (!root) {
            ev->errorMessage = "PARSE_ERROR: invalid server response";
            QApplication::postEvent(s_target, ev);
            break;
        }
        cJSON* t = cJSON_GetObjectItem(root, "translated_text");
        if (t && cJSON_IsString(t)) {
            ev->success = true;
            ev->translatedText = cJSON_GetStringValue(t);
        } else {
            std::string err = jsonStr(cJSON_GetObjectItem(root, "error"));
            std::string code = jsonStr(cJSON_GetObjectItem(root, "code"));
            ev->errorMessage = (code.empty() ? "UNKNOWN" : code) + ": " + (err.empty() ? "translate failed" : err);
        }
        cJSON_Delete(root);
        QApplication::postEvent(s_target, ev);
        break;
    }

    case ApiLoadAllData: {
        auto* chain = static_cast<LoadChain*>(ctx->ptr);
        if (!chain) break;

        switch (chain->step) {
        case 0: { // self
            if (httpCode == 200 && !body.empty()) {
                cJSON* root = cJSON_Parse(body.c_str());
                if (root) {
                    chain->result->selfName = jsonStr(cJSON_GetObjectItem(root, "name"));
                    chain->result->selfStatusMsg = jsonStr(cJSON_GetObjectItem(root, "status_message"));
                    chain->result->selfConnStatus = jsonStr(cJSON_GetObjectItem(root, "connection_status"));
                    chain->result->selfAddress = jsonStr(cJSON_GetObjectItem(root, "address"));
                    cJSON_Delete(root);
                }
            }
            chain->step = 1;
            auto* next = new ApiCtx(ApiLoadAllData); next->ptr = chain;
            EventPoller::addRequest(buildUrl("/api/friends"), "GET", "", onHttpDone, next, 35);
            break;
        }
        case 1: { // friend ids
            if (httpCode == 200 && !body.empty()) {
                cJSON* root = cJSON_Parse(body.c_str());
                if (root) {
                    cJSON* arr = cJSON_GetObjectItem(root, "friends");
                    if (arr && cJSON_IsArray(arr)) {
                        int n = cJSON_GetArraySize(arr);
                        for (int i = 0; i < n; ++i)
                            chain->friendIds.push_back(cJSON_GetArrayItem(arr, i)->valueint);
                    }
                    cJSON_Delete(root);
                }
            }
            chain->step = 2;
            chain->friendIdx = 0;
            if (chain->friendIds.empty()) chain->step = 3; // skip to groups
            auto* next = new ApiCtx(ApiLoadAllData); next->ptr = chain;
            if (chain->step == 3)
                EventPoller::addRequest(buildUrl("/api/groups"), "GET", "", onHttpDone, next, 35);
            else
                EventPoller::addRequest(buildUrl("/api/friend"), "POST",
                    "friend_id=" + std::to_string(chain->friendIds[0]), onHttpDone, next, 35);
            break;
        }
        case 2: { // friend info
            if (httpCode == 200 && !body.empty()) {
                cJSON* root = cJSON_Parse(body.c_str());
                if (root) {
                    ContactData cd;
                    cd.id = chain->friendIds[chain->friendIdx];
                    cd.type = "friend";
                    cd.name = jsonStr(cJSON_GetObjectItem(root, "name"));
                    cd.status = jsonStr(cJSON_GetObjectItem(root, "statusStr"));
                    cd.chatId = jsonStr(cJSON_GetObjectItem(root, "publicKey"));
                    cd.iconUrl = jsonStr(cJSON_GetObjectItem(root, "iconUrl"));
                    cd.statusText = jsonStr(cJSON_GetObjectItem(root, "statusText"));
                    chain->result->contacts.push_back(cd);
                    cJSON_Delete(root);
                }
            }
            chain->friendIdx++;
            auto* next = new ApiCtx(ApiLoadAllData); next->ptr = chain;
            if ((size_t)chain->friendIdx >= chain->friendIds.size()) {
                chain->step = 3;
                EventPoller::addRequest(buildUrl("/api/groups"), "GET", "", onHttpDone, next, 35);
            } else {
                EventPoller::addRequest(buildUrl("/api/friend"), "POST",
                    "friend_id=" + std::to_string(chain->friendIds[chain->friendIdx]),
                    onHttpDone, next, 35);
            }
            break;
        }
        case 3: { // groups
            if (httpCode == 200 && !body.empty()) {
                cJSON* root = cJSON_Parse(body.c_str());
                if (root) {
                    cJSON* arr = cJSON_GetObjectItem(root, "groups");
                    if (arr && cJSON_IsArray(arr)) {
                        int n = cJSON_GetArraySize(arr);
                        for (int i = 0; i < n; ++i) {
                            cJSON* item = cJSON_GetArrayItem(arr, i);
                            if (!item) continue;
                            ContactData cd;
                            cJSON* v = cJSON_GetObjectItem(item, "groupNumber");
                            if (v) cd.id = v->valueint;
                            cd.name = jsonStr(cJSON_GetObjectItem(item, "groupName"));
                            cd.chatId = jsonStr(cJSON_GetObjectItem(item, "chatId"));
                            v = cJSON_GetObjectItem(item, "isConnected");
                            cd.isConnected = v ? (v->valueint == 1) : false;
                            cd.statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
                            cd.type = "group";
                            chain->result->contacts.push_back(cd);
                        }
                    }
                    cJSON_Delete(root);
                }
            }
            chain->step = 4;
            auto* next = new ApiCtx(ApiLoadAllData); next->ptr = chain;
            EventPoller::addRequest(buildUrl("/api/conferences"), "GET", "", onHttpDone, next, 35);
            break;
        }
        case 4: { // conferences → done
            if (httpCode == 200 && !body.empty()) {
                cJSON* root = cJSON_Parse(body.c_str());
                if (root) {
                    cJSON* arr = cJSON_GetObjectItem(root, "conferences");
                    if (arr && cJSON_IsArray(arr)) {
                        int n = cJSON_GetArraySize(arr);
                        for (int i = 0; i < n; ++i) {
                            cJSON* item = cJSON_GetArrayItem(arr, i);
                            if (!item) continue;
                            ContactData cd;
                            cJSON* v = cJSON_GetObjectItem(item, "conferenceNumber");
                            if (v) cd.id = v->valueint;
                            cd.name = jsonStr(cJSON_GetObjectItem(item, "conferenceName"));
                            cd.chatId = jsonStr(cJSON_GetObjectItem(item, "chatId"));
                            v = cJSON_GetObjectItem(item, "isConnected");
                            cd.isConnected = v ? (v->valueint == 1) : false;
                            cd.statusText = jsonStr(cJSON_GetObjectItem(item, "statusText"));
                            cd.type = "conference";
                            chain->result->contacts.push_back(cd);
                        }
                    }
                    cJSON_Delete(root);
                }
            }
            chain->result->success = true;
            QApplication::postEvent(s_target, chain->result);
            delete chain;
            break;
        }
        }
        break;
    }

    }
}
