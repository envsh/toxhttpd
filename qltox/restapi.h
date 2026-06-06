#ifndef API_H
#define API_H

#include <string>
#include <vector>
#include <cstdint>
#include "eventpoller.h"

struct FriendInfo {
    int id;
    std::string name;
    std::string statusStr;
    std::string userStatus;
    std::string statusText;
    std::string iconUrl;
    std::string publicKey;
    std::string peerIp;
    uint64_t lastSeen = 0;
};

FriendInfo friendInfoFromPeer(const PeerInfo& peer, int id);

struct GroupInfo {
    int groupNumber = 0;
    std::string groupName;
    std::string chatId;
    bool isConnected = false;
    std::string statusText;
    int memberCount = 0;
};

struct ConferenceInfo {
    int conferenceNumber = 0;
    std::string conferenceName;
    std::string chatId;
    bool isConnected = false;
    std::string statusText;
    int memberCount = 0;
};

class ToxAPI {
public:
    static void setEventTarget(QObject* target);
    static void setBaseUrl(const std::string& url);
    static void resetLastEventId();

    static void startPollEvent();
    static void stopPollEvent();
    static void loadAllData();
    static bool onLoadAllDataComplete();

    static void getSelf();
    static void getFriends();
    static void sendFriendMessage(int friendId, const std::string& message);
    static void sendConferenceMessage(int conferenceId, const std::string& message);
    static void sendGroupMessage(int groupId, const std::string& message);
    static void addFriend(const std::string& publicKey);
    static void deleteFriend(int friendId);
    static void getGroupMembers(int groupId);
    static void getConferenceMembers(int confId);
    static void getMessagesHistory(int contactId, const std::string& contactType);
    static void joinConference(int friendNumber, const std::string& cookie);
    static void rejectConference(int friendNumber);
    static void ignoreConference(int friendNumber);
    static void createConference();
    static void leaveConference(int confId);
    static void inviteToConference(int friendId, int confId);
    static void createGroup(const std::string& groupName, const std::string& creatorName,
                           const std::string& password = "", bool isPrivate = false);
    static void leaveGroup(int groupId);
    static void inviteToGroup(int friendId, int groupId);
    static void joinGroup(int friendNumber, const std::string& chatId,
                         const std::string& name = "", const std::string& password = "");
    static void joinGroupByChatId(const std::string& chatId,
                                  const std::string& name = "", const std::string& password = "");
    static void setGroupSelfName(int groupId, const std::string& name);
    static void getRandomName();
    static void setSelfInfo(const std::string& name, const std::string& statusMessage);
    static void translate(const std::string& text, const std::string& toLang, int msgIndex);
    static void lazyLoadFriendDetail(int friendId);
    static std::string urlEncode(const std::string& str);

    // Sync helper methods for dialog contexts
    static std::vector<GroupInfo> getGroupsSync();
    static std::vector<ConferenceInfo> getConferencesSync();
    static std::vector<PeerInfo> getConferenceMembersSync(int confId);
    static std::vector<PeerInfo> getGroupMembersSync(int groupId);
    static std::string getRandomNameSync();
    static int addFriendSync(const std::string& publicKey);
    static int createConferenceSync();
    static int createGroupSync(const std::string& groupName, const std::string& creatorName,
                               const std::string& password = "", bool isPrivate = false);
    static bool deleteFriendSync(int friendId);
    static bool leaveConferenceSync(int confId);
    static bool leaveGroupSync(int groupId);
    static bool setGroupTopicSync(int groupId, const std::string& topic);
    static bool setConferenceTitleSync(int conferenceId, const std::string& title);
    static void setGroupTopic(int groupId, const std::string& topic);
    static void setConferenceTitle(int conferenceId, const std::string& title);
    static bool setSelfInfoSync(const std::string& name, const std::string& statusMessage);
    static bool joinGroupSync(int friendNumber, const std::string& chatId,
                              const std::string& name = "", const std::string& password = "");
    static bool inviteToConferenceSync(int friendId, int confId);
    static bool inviteToGroupSync(int friendId, int groupId);
    static bool setGroupSelfNameSync(int groupId, const std::string& name);
    static bool joinGroupByChatIdSync(const std::string& chatId,
                                      const std::string& name = "", const std::string& password = "");

private:
    struct ApiCtx {
        int type;
        int id = 0;
        std::string str1;
        std::string str2;
        int n1 = 0;
        int step = 0;
        ApiCtx() {}
        ApiCtx(int t) : type(t) {}
        ApiCtx(int t, int i) : type(t), id(i) {}
        ApiCtx(int t, int i, const std::string& s) : type(t), id(i), str1(s) {}
        ApiCtx(int t, int i, const std::string& s1, const std::string& s2) : type(t), id(i), str1(s1), str2(s2) {}
        ApiCtx(int t, int i, const std::string& s1, const std::string& s2, int n) : type(t), id(i), str1(s1), str2(s2), n1(n) {}
        void* ptr = nullptr;
    };
    static void onHttpDone(const HttpResponse& resp, void* udata);
    static void dispatchResult(ApiCtx* ctx, const HttpResponse& resp);
    static std::string buildUrl(const std::string& endpoint);
    static void pollEvents();
    static void request(ApiRequestType type, const HttpRequest& req);
    static void request(const HttpRequest& req, ApiCtx* ctx);
    static bool syncRequest(const std::string& endpoint,
                            const std::string& method,
                            std::string& outBody,
                            const std::string& data = "",
                            int timeoutSec = 35);

    static QObject* s_target;
    static std::string s_baseUrl;
    static uint64_t s_lastEventId;
    static bool s_pollRunning;
    static bool s_loadingAllData;
    static bool s_reloadPending;
};

#endif
