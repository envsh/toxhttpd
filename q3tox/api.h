#ifndef API_H
#define API_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct FriendInfo {
    int id;
    std::string name;
    std::string status_message;
    std::string status;
    std::string connection_status;
    std::string public_key;
};

struct Event {
    uint64_t id;
    std::string type;
    std::string data;
};

class ToxAPI {
public:
    ToxAPI(const std::string& baseUrl = "http://localhost:8181");
    
    // Self
    bool getSelf(std::string& name, std::string& statusMsg, std::string& connStatus, std::string& address);
    bool setSelfName(const std::string& name);
    bool setSelfStatus(const std::string& status);
    
    // Friends
    std::vector<int> getFriends();
    bool getFriendInfo(int friendId, FriendInfo& info);
    int addFriend(const std::string& publicKey);
    bool deleteFriend(int friendId);
    
    // Messages
    bool sendFriendMessage(int friendId, const std::string& message);
    bool sendConferenceMessage(int conferenceId, const std::string& message);
    
    // Conferences
    std::vector<int> getConferences();
    int createConference();
    bool joinConference(int friendNumber, const std::string& cookie);
    bool rejectConference(int friendNumber);
    bool ignoreConference(int friendNumber);
    
    // Events (long polling, 30s timeout)
    std::vector<Event> pollEvents(uint64_t after);
    
private:
    std::string baseUrl;
    
    std::string httpGet(const std::string& endpoint);
    std::string httpPost(const std::string& endpoint, const std::string& postData);
};

#endif // API_H
