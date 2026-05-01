#include "api.h"
#include "cJSON.h"
#include <curl/curl.h>
#include <sstream>
#include <iostream>

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
            std::cerr << "cURL GET error: " << curl_easy_strerror(res) << std::endl;
            response.clear();
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
            std::cerr << "cURL POST error: " << curl_easy_strerror(res) << std::endl;
            response.clear();
        }
        curl_easy_cleanup(curl);
    }
    return response;
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

bool ToxAPI::setSelfName(const std::string& name) {
    std::string postData = "name=" + name;
    std::string response = httpPost("/api/self/name", postData);
    return !response.empty();
}

bool ToxAPI::setSelfStatus(const std::string& status) {
    std::string postData = "status_message=" + status;
    std::string response = httpPost("/api/self/status", postData);
    return !response.empty();
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
    std::string postData = "friend_id=" + std::to_string(friendId) + "&message=" + message;
    std::string response = httpPost("/api/messages", postData);
    return !response.empty();
}

bool ToxAPI::sendConferenceMessage(int conferenceId, const std::string& message) {
    std::string postData = "conference_id=" + std::to_string(conferenceId) + "&message=" + message;
    std::string response = httpPost("/api/conference_messages", postData);
    return !response.empty();
}

std::vector<int> ToxAPI::getConferences() {
    std::vector<int> conferences;
    std::string response = httpGet("/api/conferences");
    if (response.empty()) return conferences;
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (!root) return conferences;
    
    cJSON* conferencesItem = cJSON_GetObjectItem(root, "conferences");
    if (conferencesItem && cJSON_IsArray(conferencesItem)) {
        int count = cJSON_GetArraySize(conferencesItem);
        for (int i = 0; i < count; ++i) {
            cJSON* item = cJSON_GetArrayItem(conferencesItem, i);
            if (item) conferences.push_back(item->valueint);
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
            
            events.push_back(event);
        }
    }
    
    cJSON_Delete(root);
    return events;
}
