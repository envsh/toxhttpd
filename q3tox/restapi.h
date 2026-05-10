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
    std::string timestamp;  // ISO8601 format from server
};

struct PeerInfo {
    int peerNumber;
    std::string name;
};

struct GroupInfo {
    int group_number;
    std::string group_name;
    std::string chat_id;
    bool is_connected; // 新增：群组连接状态
};

struct ConferenceInfo {
    int conference_number;
    std::string conference_name;
    std::string chat_id;
    bool is_connected; // 新增：会议连接状态
};

struct HistoryMessage {
    int64_t rowid;
    std::string message;
    std::string sender_pubkey;
    uint32_t sender_number;
    std::string direction;
    std::string created_at;
};

class ToxAPI {
public:
    ToxAPI(const std::string& baseUrl = "http://localhost:8181");
    
    // Self
    bool getSelf(std::string& name, std::string& statusMsg, std::string& connStatus, std::string& address);
    bool setSelfInfo(const std::string& name, const std::string& status_message);
    // Keep old functions for compatibility (call setSelfInfo internally)
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
    bool sendGroupMessage(int groupId, const std::string& message);
    
    // Conferences
    std::vector<ConferenceInfo> getConferences();
    int createConference();
    bool leaveConference(int confId);
    bool inviteToConference(int friendId, int confId);
    bool joinConference(int friendNumber, const std::string& cookie);
    bool rejectConference(int friendNumber);
    bool ignoreConference(int friendNumber);
    
    // Groups (NGC)
    std::vector<GroupInfo> getGroups();
    int createGroup(const std::string& groupName, const std::string& creatorName, 
                   const std::string& password = "", bool isPrivate = false);
    bool leaveGroup(int groupId);
    bool inviteToGroup(int friendId, int groupId);
    bool joinGroup(int friendNumber, const std::string& chatId, 
                  const std::string& name = "", const std::string& password = "");
    bool joinGroupByChatId(const std::string& chatId,
                          const std::string& name = "", const std::string& password = "");
    
    // Events (long polling, 30s timeout)
    std::vector<Event> pollEvents(uint64_t after);
    
    // Message history
    bool getMessagesHistory(int contact_id, const std::string& contact_type,
                            std::vector<HistoryMessage>& messages);
    
    // Member lists
    std::vector<PeerInfo> getConferenceMembers(int confId);
    std::vector<PeerInfo> getGroupMembers(int groupId);
    
private:
    std::string baseUrl;

    std::string urlEncode(const std::string& str);
    std::string httpGet(const std::string& endpoint);
    std::string httpPost(const std::string& endpoint, const std::string& postData);
};

#endif // API_H
