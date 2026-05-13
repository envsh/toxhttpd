#ifndef API_H
#define API_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct FriendInfo {
    int id;
    std::string name;
    std::string statusStr;
    std::string statusText;
    std::string iconUrl;
    std::string publicKey;
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
    int status = 0;           // connection status: 0=none, 1=tcp, 2=udp
    std::string statusStr;    // "none"/"tcp"/"udp"
    std::string statusText;
    std::string iconUrl;
    int role = 0;
    std::string publicKey;
    bool isSelf = false;
};

struct GroupInfo {
    int groupNumber = 0;
    std::string groupName;
    std::string chatId;
    bool isConnected = false;
    std::string statusText;
};

struct ConferenceInfo {
    int conferenceNumber = 0;
    std::string conferenceName;
    std::string chatId;
    bool isConnected = false;
    std::string statusText;
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
    
    // Group nickname
    bool setGroupSelfName(int groupId, const std::string& name);
    std::string getRandomName();
    
private:
    std::string baseUrl;

    std::string urlEncode(const std::string& str);
    std::string httpGet(const std::string& endpoint, std::map<std::string, std::string>* headers = nullptr);
    std::string httpPost(const std::string& endpoint, const std::string& postData);
};

#endif // API_H
