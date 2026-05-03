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

struct PeerInfo {
    int peerNumber;
    std::string name;
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
    bool leaveConference(int confId);
    bool inviteToConference(int friendId, int confId);
    bool joinConference(int friendNumber, const std::string& cookie);
    bool rejectConference(int friendNumber);
    bool ignoreConference(int friendNumber);
    
    // Groups (NGC)
    std::vector<int> getGroups();
    int createGroup(const std::string& groupName, const std::string& creatorName, 
                   const std::string& password = "", bool isPrivate = false);
    bool leaveGroup(int groupId);
    bool inviteToGroup(int friendId, int groupId);
    bool joinGroup(int friendNumber, const std::string& chatId, 
                  const std::string& name = "", const std::string& password = "");
    
    // Events (long polling, 30s timeout)
    std::vector<Event> pollEvents(uint64_t after);
    
    // Member lists
    std::vector<PeerInfo> getConferenceMembers(int confId);
    std::vector<PeerInfo> getGroupMembers(int groupId);
    
private:
    std::string baseUrl;
    
    std::string httpGet(const std::string& endpoint);
    std::string httpPost(const std::string& endpoint, const std::string& postData);
};

#endif // API_H
