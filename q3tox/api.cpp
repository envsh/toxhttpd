#include "api.h"
#include "apilog.h"
#include "cJSON.h"
#include <curl/curl.h>
#include <sstream>

// cURL 回调函数
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), realsize);
    return realsize;
}

// 执行 HTTP GET 请求
std::string ToxAPI::httpGet(const std::string& endpoint) {
    CURL* curl = curl_easy_init();
    std::string response;
    std::string url = baseUrl + endpoint;
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30秒超时
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            ALOG_ERROR("cURL GET error:", curl_easy_strerror(res));
            response.clear();
        } else {
            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            ALOG_INFO("HTTP GET response, code:", httpCode, "body length:", response.length());
        }
        curl_easy_cleanup(curl);
    }
    return response;
}

// 执行 HTTP POST 请求
std::string ToxAPI::httpPost(const std::string& endpoint, const std::string& postData) {
    CURL* curl = curl_easy_init();
    std::string response;
    std::string url = baseUrl + endpoint;
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            ALOG_ERROR("cURL POST error:", curl_easy_strerror(res));
            response.clear();
        } else {
            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            ALOG_INFO("HTTP POST response, code:", httpCode, "body length:", response.length());
        }
        curl_easy_cleanup(curl);
    }
    return response;
}

// URL encode string using curl_easy_escape
std::string ToxAPI::urlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (!curl) return str;
    char* encoded = curl_easy_escape(curl, str.c_str(), (int)str.length());
    std::string result = encoded ? encoded : str;
    if (encoded) curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

ToxAPI::ToxAPI(const std::string& baseUrl) : baseUrl(baseUrl) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

bool ToxAPI::getSelf(std::string& name, std::string& statusMsg, std::string& connStatus, std::string& address) {
    std::string response = httpGet("/api/self");
    if (response.empty()) return false;
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return false;
    
    cJSON* nameItem = cJSON_GetObjectItem(root, "name");
    if (nameItem && cJSON_IsString(nameItem)) {
        name = std::string(cJSON_GetStringValue(nameItem));
    } else {
        name = "";
    }
    
    cJSON* statusItem = cJSON_GetObjectItem(root, "status_message");
    if (statusItem && cJSON_IsString(statusItem)) {
        statusMsg = std::string(cJSON_GetStringValue(statusItem));
    } else {
        statusMsg = "";
    }
    
    cJSON* connItem = cJSON_GetObjectItem(root, "connection_status");
    if (connItem && cJSON_IsString(connItem)) {
        connStatus = std::string(cJSON_GetStringValue(connItem));
    } else {
        connStatus = "offline";
    }
    
    cJSON* addrItem = cJSON_GetObjectItem(root, "address");
    if (addrItem && cJSON_IsString(addrItem)) {
        address = std::string(cJSON_GetStringValue(addrItem));
    } else {
        address = "";
    }
    
    cJSON_Delete(root);
    return true;
}

bool ToxAPI::setSelfInfo(const std::string& name, const std::string& status_message) {
    std::string postData;
    bool hasParams = false;
    
    if (!name.empty()) {
        postData += "name=" + urlEncode(name);
        hasParams = true;
    }
    if (!status_message.empty()) {
        if (hasParams) postData += "&";
        postData += "status_message=" + urlEncode(status_message);
        hasParams = true;
    }
    
    if (!hasParams) return true; // No params, return directly
    
    std::string response = httpPost("/api/self", postData);
    return !response.empty();
}

bool ToxAPI::setSelfName(const std::string& name) {
    return setSelfInfo(name, "");
}

bool ToxAPI::setSelfStatus(const std::string& status) {
    return setSelfInfo("", status);
}

std::vector<int> ToxAPI::getFriends() {
    std::vector<int> friends;
    std::string response = httpGet("/api/friends");
    if (response.empty()) return friends;

    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return friends;

    cJSON* friendsItem = cJSON_GetObjectItem(root, "friends");
    if (friendsItem && cJSON_IsArray(friendsItem)) {
        int count = cJSON_GetArraySize(friendsItem);
        for (int i = 0; i < count; ++i) {
            cJSON* item = cJSON_GetArrayItem(friendsItem, i);
            if (item) friends.push_back(item->valueint);
        }
    }

    cJSON_Delete(root);
    return friends;
}

bool ToxAPI::getFriendInfo(int friendId, FriendInfo& info) {
    std::string postData = "friend_id=" + std::to_string(friendId);
    std::string response = httpPost("/api/friend", postData);
    if (response.empty()) return false;
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return false;
    
    info.id = friendId;
    
    cJSON* nameItem = cJSON_GetObjectItem(root, "name");
    if (nameItem && cJSON_IsString(nameItem)) {
        info.name = std::string(cJSON_GetStringValue(nameItem));
    } else {
        info.name = "";
    }
    
    cJSON* statusItem = cJSON_GetObjectItem(root, "status_message");
    if (statusItem && cJSON_IsString(statusItem)) {
        info.status = std::string(cJSON_GetStringValue(statusItem));
    } else {
        info.status = "";
    }
    
    cJSON* connItem = cJSON_GetObjectItem(root, "connection_status");
    if (connItem && cJSON_IsString(connItem)) {
        info.connection_status = std::string(cJSON_GetStringValue(connItem));
    } else {
        info.connection_status = "offline";
    }
    
    cJSON* pkItem = cJSON_GetObjectItem(root, "public_key");
    if (pkItem && cJSON_IsString(pkItem)) {
        info.public_key = std::string(cJSON_GetStringValue(pkItem));
    } else {
        info.public_key = "";
    }
    
    cJSON_Delete(root);
    return true;
}

int ToxAPI::addFriend(const std::string& publicKey) {
    std::string postData = "public_key=" + publicKey;
    std::string response = httpPost("/api/friends", postData);
    if (response.empty()) return -1;
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return -1;
    
    cJSON* idItem = cJSON_GetObjectItem(root, "friend_id");
    int id = idItem ? idItem->valueint : -1;
    
    cJSON_Delete(root);
    return id;
}

bool ToxAPI::deleteFriend(int friendId) {
    std::string postData = "friend_id=" + std::to_string(friendId);
    std::string response = httpPost("/api/friend_delete", postData);
    return !response.empty();
}

bool ToxAPI::leaveConference(int confId) {
    std::string postData = "conference_id=" + std::to_string(confId);
    std::string response = httpPost("/api/conference_delete", postData);
    return !response.empty();
}

bool ToxAPI::inviteToConference(int friendId, int confId) {
    std::string postData = "friend_id=" + std::to_string(friendId) + "&conference_id=" + std::to_string(confId);
    std::string response = httpPost("/api/conference_invite", postData);
    return !response.empty();
}

bool ToxAPI::sendFriendMessage(int friendId, const std::string& message) {
    std::string postData = "friend_id=" + std::to_string(friendId) + "&message=" + urlEncode(message);
    std::string response = httpPost("/api/messages", postData);
    return !response.empty();
}

bool ToxAPI::sendConferenceMessage(int conferenceId, const std::string& message) {
    std::string postData = "conference_id=" + std::to_string(conferenceId) + "&message=" + urlEncode(message);
    std::string response = httpPost("/api/conference_messages", postData);
    return !response.empty();
}

bool ToxAPI::sendGroupMessage(int groupId, const std::string& message) {
    std::string postData = "group_number=" + std::to_string(groupId) + "&message=" + urlEncode(message);
    std::string response = httpPost("/api/group_messages", postData);
    return !response.empty();
}

std::vector<ConferenceInfo> ToxAPI::getConferences() {
    std::vector<ConferenceInfo> conferences;
    std::string response = httpGet("/api/conferences");
    if (response.empty()) return conferences;
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return conferences;
    
    cJSON* conferencesItem = cJSON_GetObjectItem(root, "conferences");
    if (conferencesItem && cJSON_IsArray(conferencesItem)) {
        int count = cJSON_GetArraySize(conferencesItem);
        for (int i = 0; i < count; ++i) {
            cJSON* item = cJSON_GetArrayItem(conferencesItem, i);
            if (!item) continue;
            
            ConferenceInfo info;
            cJSON* numItem = cJSON_GetObjectItem(item, "conference_number");
            if (numItem) info.conference_number = numItem->valueint;
            
            cJSON* nameItem = cJSON_GetObjectItem(item, "conference_name");
            if (nameItem && cJSON_IsString(nameItem)) {
                info.conference_name = std::string(cJSON_GetStringValue(nameItem));
            }
            
            cJSON* chatIdItem = cJSON_GetObjectItem(item, "chat_id");
            if (chatIdItem && cJSON_IsString(chatIdItem)) {
                info.chat_id = std::string(cJSON_GetStringValue(chatIdItem));
            }
            
            conferences.push_back(info);
        }
    }
    
    cJSON_Delete(root);
    return conferences;
}

int ToxAPI::createConference() {
    std::string response = httpPost("/api/conferences", "");
    if (response.empty()) return -1;
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return -1;
    
    cJSON* idItem = cJSON_GetObjectItem(root, "conference_id");
    int id = idItem ? idItem->valueint : -1;
    
    cJSON_Delete(root);
    return id;
}

bool ToxAPI::joinConference(int friendNumber, const std::string& cookie) {
    std::string postData = "friend_number=" + std::to_string(friendNumber) + "&cookie=" + cookie;
    std::string response = httpPost("/api/conferences/join", postData);
    return !response.empty();
}

bool ToxAPI::rejectConference(int friendNumber) {
    std::string postData = "friend_number=" + std::to_string(friendNumber);
    std::string response = httpPost("/api/conferences/reject", postData);
    return !response.empty();
}

bool ToxAPI::ignoreConference(int friendNumber) {
    std::string postData = "friend_number=" + std::to_string(friendNumber);
    std::string response = httpPost("/api/conferences/ignore", postData);
    return !response.empty();
}

std::vector<Event> ToxAPI::pollEvents(uint64_t after) {
    std::vector<Event> events;
    std::string endpoint = "/api/events?after=" + std::to_string(after);
    
    std::string response = httpGet(endpoint);
    if (response.empty()) return events;
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return events;
    
    if (cJSON_IsArray(root)) {
        int count = cJSON_GetArraySize(root);
        for (int i = 0; i < count; ++i) {
            cJSON* item = cJSON_GetArrayItem(root, i);
            if (!item) continue;
            
            Event event;
            cJSON* idItem = cJSON_GetObjectItem(item, "event_id");
            event.id = idItem ? idItem->valueint : 0;
            
            cJSON* typeItem = cJSON_GetObjectItem(item, "event_type");
            const char* typeStr = typeItem ? cJSON_GetStringValue(typeItem) : "";
            event.type = typeStr ? typeStr : "";
            
            cJSON* dataItem = cJSON_GetObjectItem(item, "data");
            const char* dataStr = dataItem ? cJSON_GetStringValue(dataItem) : "";
            event.data = dataStr ? dataStr : "";
            
            cJSON* timestampItem = cJSON_GetObjectItem(item, "timestamp");
            const char* timestampStr = timestampItem ? cJSON_GetStringValue(timestampItem) : "";
            event.timestamp = timestampStr ? timestampStr : "";
            
            events.push_back(event);
        }
    }
    
    cJSON_Delete(root);
    return events;
}

// ===== Groups (NGC) =====

std::vector<GroupInfo> ToxAPI::getGroups() {
    std::vector<GroupInfo> groups;
    std::string response = httpGet("/api/groups");
    if (response.empty()) return groups;
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return groups;
    
    cJSON* groupsItem = cJSON_GetObjectItem(root, "groups");
    if (groupsItem && cJSON_IsArray(groupsItem)) {
        int count = cJSON_GetArraySize(groupsItem);
        for (int i = 0; i < count; ++i) {
            cJSON* item = cJSON_GetArrayItem(groupsItem, i);
            if (!item) continue;
            
            GroupInfo info;
            cJSON* numItem = cJSON_GetObjectItem(item, "group_number");
            if (numItem) info.group_number = numItem->valueint;
            
            cJSON* nameItem = cJSON_GetObjectItem(item, "group_name");
            if (nameItem && cJSON_IsString(nameItem)) {
                info.group_name = std::string(cJSON_GetStringValue(nameItem));
            }
            
            cJSON* chatIdItem = cJSON_GetObjectItem(item, "chat_id");
            if (chatIdItem && cJSON_IsString(chatIdItem)) {
                info.chat_id = std::string(cJSON_GetStringValue(chatIdItem));
            }
            
            // 解析 is_connected 字段
            cJSON* connectedItem = cJSON_GetObjectItem(item, "is_connected");
            if (connectedItem) {
                info.is_connected = (connectedItem->valueint == 1);
            } else {
                info.is_connected = false;
            }
            
            groups.push_back(info);
        }
    }
    
    cJSON_Delete(root);
    return groups;
}

int ToxAPI::createGroup(const std::string& groupName, const std::string& creatorName, 
                       const std::string& password, bool isPrivate) {
    std::string postData = "group_name=" + groupName + "&name=" + creatorName;
    if (!password.empty()) {
        postData += "&password=" + password;
    }
    postData += "&privacy=" + std::string(isPrivate ? "private" : "public");
    
    std::string response = httpPost("/api/groups", postData);
    if (response.empty()) return -1;
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return -1;
    
    cJSON* idItem = cJSON_GetObjectItem(root, "group_id");
    int id = idItem ? idItem->valueint : -1;
    
    cJSON_Delete(root);
    return id;
}

bool ToxAPI::leaveGroup(int groupId) {
    std::string postData = "group_id=" + std::to_string(groupId);
    std::string response = httpPost("/api/groups/leave", postData);
    return !response.empty();
}

bool ToxAPI::inviteToGroup(int friendId, int groupId) {
    std::string postData = "friend_id=" + std::to_string(friendId) + "&group_id=" + std::to_string(groupId);
    std::string response = httpPost("/api/groups/invite", postData);
    return !response.empty();
}

bool ToxAPI::joinGroup(int friendNumber, const std::string& chatId, 
                       const std::string& name, const std::string& password) {
    std::string postData = "friend_number=" + std::to_string(friendNumber) + "&chat_id=" + chatId;
    if (!name.empty()) {
        postData += "&name=" + name;
    }
    if (!password.empty()) {
        postData += "&password=" + password;
    }
    std::string response = httpPost("/api/groups/join", postData);
    return !response.empty();
}

std::vector<PeerInfo> ToxAPI::getConferenceMembers(int confId) {
    std::vector<PeerInfo> members;
    std::string endpoint = "/api/conference/members?conference_id=" + std::to_string(confId);
    std::string response = httpGet(endpoint);
    if (response.empty()) return members;
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return members;
    
    cJSON* membersItem = cJSON_GetObjectItem(root, "members");
    if (membersItem && cJSON_IsArray(membersItem)) {
        int count = cJSON_GetArraySize(membersItem);
        for (int i = 0; i < count; ++i) {
            cJSON* memberItem = cJSON_GetArrayItem(membersItem, i);
            if (!memberItem) continue;
            
            PeerInfo info;
            cJSON* peerNumberItem = cJSON_GetObjectItem(memberItem, "peer_number");
            info.peerNumber = peerNumberItem ? peerNumberItem->valueint : -1;
            
            cJSON* nameItem = cJSON_GetObjectItem(memberItem, "name");
            if (nameItem && cJSON_IsString(nameItem)) {
                info.name = std::string(cJSON_GetStringValue(nameItem));
            } else {
                info.name = "Unknown";
            }
            
            members.push_back(info);
        }
    }
    
    cJSON_Delete(root);
    return members;
}

std::vector<PeerInfo> ToxAPI::getGroupMembers(int groupId) {
    std::vector<PeerInfo> members;
    std::string endpoint = "/api/group/members?group_number=" + std::to_string(groupId);
    std::string response = httpGet(endpoint);
    if (response.empty()) return members;
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return members;
    
    cJSON* membersItem = cJSON_GetObjectItem(root, "members");
    if (membersItem && cJSON_IsArray(membersItem)) {
        int count = cJSON_GetArraySize(membersItem);
        for (int i = 0; i < count; ++i) {
            cJSON* memberItem = cJSON_GetArrayItem(membersItem, i);
            if (!memberItem) continue;
            
            PeerInfo info;
            cJSON* peerNumberItem = cJSON_GetObjectItem(memberItem, "peer_number");
            info.peerNumber = peerNumberItem ? peerNumberItem->valueint : -1;
            
            cJSON* nameItem = cJSON_GetObjectItem(memberItem, "name");
            if (nameItem && cJSON_IsString(nameItem)) {
                info.name = std::string(cJSON_GetStringValue(nameItem));
            } else {
                info.name = "Unknown";
            }
            
            members.push_back(info);
        }
    }
    
    cJSON_Delete(root);
    return members;
}

// ===== Message History =====

bool ToxAPI::getMessagesHistory(int contact_id, const std::string& contact_type,
                                 std::vector<HistoryMessage>& messages) {
    messages.clear();
    
    std::string url = "/api/messages/history?contact_id=" + std::to_string(contact_id)
                   + "&contact_type=" + contact_type;
    std::string response = httpGet(url);
    if (response.empty()) return false;
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return false;
    
    cJSON* msgsItem = cJSON_GetObjectItem(root, "messages");
    if (!msgsItem || !cJSON_IsArray(msgsItem)) {
        cJSON_Delete(root);
        return false;
    }
    
    int msgCount = cJSON_GetArraySize(msgsItem);
    for (int i = 0; i < msgCount; ++i) {
        cJSON* msg = cJSON_GetArrayItem(msgsItem, i);
        if (!msg) continue;
        
        HistoryMessage hm;
        
        cJSON* item = cJSON_GetObjectItem(msg, "rowid");
        if (item) hm.rowid = (int64_t)(item->valuedouble);
        
        item = cJSON_GetObjectItem(msg, "message");
        if (item && cJSON_IsString(item)) {
            hm.message = cJSON_GetStringValue(item) ?: "";
        }
        
        item = cJSON_GetObjectItem(msg, "sender_pubkey");
        if (item && cJSON_IsString(item)) {
            hm.sender_pubkey = cJSON_GetStringValue(item) ?: "";
        }
        
        item = cJSON_GetObjectItem(msg, "sender_number");
        if (item) hm.sender_number = (uint32_t)(item->valuedouble);
        
        item = cJSON_GetObjectItem(msg, "direction");
        if (item && cJSON_IsString(item)) {
            hm.direction = cJSON_GetStringValue(item) ?: "";
        }
        
        item = cJSON_GetObjectItem(msg, "created_at");
        if (item && cJSON_IsString(item)) {
            hm.created_at = cJSON_GetStringValue(item) ?: "";
        }
        
        messages.push_back(hm);
    }
    
    cJSON_Delete(root);
    return true;
}
