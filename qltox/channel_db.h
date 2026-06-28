#pragma once
#include "storage.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct ChannelRow {
    std::string chanid;
    std::string proto_type = "tox";
    int64_t last_message_rowid = 0;
    int64_t last_read_rowid = 0;
    int unread_count = 0;
    int pinned_order = 0;
    std::string draft_text;
    int muted = 0;
};

struct ChannelUpdate {
    bool hasProtoType = false;          std::string proto_type;
    bool hasLastMessageRowid = false;   int64_t last_message_rowid;
    bool hasLastReadRowid = false;      int64_t last_read_rowid;
    bool hasUnreadCount = false;        int unread_count;
    bool hasPinnedOrder = false;        int pinned_order;
    bool hasDraftText = false;          std::string draft_text;
    bool hasMuted = false;              int muted;
};

struct PeerRow {
    int peer_number;
    std::string public_key;
    std::string name;
    std::string nickname;
    std::string avatar_url;
    std::string status_text;
};

class ChannelDbSyncInterface {
public:
    virtual ~ChannelDbSyncInterface() = default;

    virtual bool add_channel(const ChannelRow& row) = 0;
    virtual std::unique_ptr<ChannelRow> get_channel(const char* chanid) = 0;
    virtual bool update_channel(const char* chanid, const ChannelUpdate& upd) = 0;
    virtual bool delete_channel(const char* chanid) = 0;
    virtual std::vector<ChannelRow> load_pinned() = 0;

    virtual bool add_peer(const PeerRow& row) = 0;
    virtual std::unique_ptr<PeerRow> get_peer(int peer_number) = 0;

    virtual bool begin_write_transaction() = 0;
    virtual bool commit_transaction() = 0;
};

class ChannelDbSyncSafeInterface {
public:
    virtual ~ChannelDbSyncSafeInterface() = default;
    virtual ChannelDbSyncInterface& get() = 0;
};

class WriteQueue;

class ChannelDbAsyncInterface {
public:
    virtual ~ChannelDbAsyncInterface() = default;
    virtual void add_channel(ChannelRow row, std::function<void(bool)> done) = 0;
    virtual void get_channel(std::string chanid,
                             std::function<void(std::unique_ptr<ChannelRow>)> done) = 0;
    virtual void update_channel(std::string chanid, ChannelUpdate upd,
                                std::function<void(bool)> done) = 0;
    virtual void delete_channel(std::string chanid, std::function<void(bool)> done) = 0;
    virtual void load_pinned(std::function<void(std::vector<ChannelRow>)> done) = 0;
    virtual void add_peer(PeerRow row, std::function<void(bool)> done) = 0;
    virtual void get_peer(int peer_number,
                          std::function<void(std::unique_ptr<PeerRow>)> done) = 0;
    virtual void close(std::function<void()> done) = 0;
};

std::shared_ptr<ChannelDbSyncSafeInterface> create_channel_db(
    std::shared_ptr<SqliteConnectionSafe> conn);

std::shared_ptr<ChannelDbAsyncInterface> create_channel_db_async(
    std::shared_ptr<ChannelDbSyncSafeInterface> sync,
    std::shared_ptr<WriteQueue> queue);

bool init_channel_db(SqliteDb& db);
bool drop_channel_db(SqliteDb& db);
