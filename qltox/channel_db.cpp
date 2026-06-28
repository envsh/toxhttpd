#include "channel_db.h"

namespace {

class ChannelDbSync final : public ChannelDbSyncInterface {
    std::shared_ptr<SqliteConnectionSafe> m_conn;
public:
    explicit ChannelDbSync(std::shared_ptr<SqliteConnectionSafe> conn)
        : m_conn(std::move(conn)) {}

    SqliteDb& db() { return *m_conn->get(); }

    bool add_channel(const ChannelRow& row) override {
        auto stmt = db().prepare(
            "INSERT OR REPLACE INTO channels "
            "(chanid,proto_type,last_message_rowid,last_read_rowid,"
            " unread_count,pinned_order,draft_text,muted) "
            "VALUES (?1,?2,?3,?4,?5,?6,?7,?8)");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, row.chanid.c_str())) { return false; }
        if (!stmt.bind(2, row.proto_type.c_str())) { return false; }
        if (!stmt.bind(3, row.last_message_rowid)) { return false; }
        if (!stmt.bind(4, row.last_read_rowid)) { return false; }
        if (!stmt.bind(5, row.unread_count)) { return false; }
        if (!stmt.bind(6, row.pinned_order)) { return false; }
        if (!stmt.bind(7, row.draft_text.c_str())) { return false; }
        if (!stmt.bind(8, row.muted)) { return false; }
        if (!stmt.step()) {
            qWarning("ChannelDb::add_channel failed for %s", row.chanid.c_str());
            return false;
        }
        return true;
    }

    std::unique_ptr<ChannelRow> get_channel(const char* chanid) override {
        auto stmt = db().prepare(
            "SELECT chanid,proto_type,last_message_rowid,last_read_rowid,"
            "       unread_count,pinned_order,draft_text,muted "
            "FROM channels WHERE chanid=?1");
        if (!stmt.isPrepared()) { return nullptr; }
        if (!stmt.bind(1, chanid)) { return nullptr; }
        if (!stmt.stepRow()) { return nullptr; }
        auto row = std::unique_ptr<ChannelRow>(new ChannelRow());
        row->chanid = stmt.columnText(0);
        row->proto_type = stmt.columnText(1);
        row->last_message_rowid = stmt.columnInt64(2);
        row->last_read_rowid = stmt.columnInt64(3);
        row->unread_count = stmt.columnInt(4);
        row->pinned_order = stmt.columnInt(5);
        row->draft_text = stmt.columnText(6);
        row->muted = stmt.columnInt(7);
        return row;
    }

    bool update_channel(const char* chanid, const ChannelUpdate& upd) override {
        std::string sql = "UPDATE channels SET ";
        int n = 0;
        auto addField = [&](const char* name) {
            if (n++ > 0) { sql += ","; }
            char idx[8]; snprintf(idx, sizeof(idx), "?%d", n + 1);
            sql += name; sql += "="; sql += idx;
        };
        if (upd.hasProtoType)        addField("proto_type");
        if (upd.hasLastMessageRowid) addField("last_message_rowid");
        if (upd.hasLastReadRowid)    addField("last_read_rowid");
        if (upd.hasUnreadCount)      addField("unread_count");
        if (upd.hasPinnedOrder)      addField("pinned_order");
        if (upd.hasDraftText)        addField("draft_text");
        if (upd.hasMuted)            addField("muted");
        if (n == 0) { return true; }
        sql += " WHERE chanid=?1";
        auto stmt = db().prepare(sql.c_str());
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, chanid)) { return false; }
        int idx = 2;
        if (upd.hasProtoType)        { stmt.bind(idx++, upd.proto_type.c_str()); }
        if (upd.hasLastMessageRowid) { stmt.bind(idx++, upd.last_message_rowid); }
        if (upd.hasLastReadRowid)    { stmt.bind(idx++, upd.last_read_rowid); }
        if (upd.hasUnreadCount)      { stmt.bind(idx++, upd.unread_count); }
        if (upd.hasPinnedOrder)      { stmt.bind(idx++, upd.pinned_order); }
        if (upd.hasDraftText)        { stmt.bind(idx++, upd.draft_text.c_str()); }
        if (upd.hasMuted)            { stmt.bind(idx++, upd.muted); }
        return stmt.step();
    }

    bool delete_channel(const char* chanid) override {
        auto stmt = db().prepare("DELETE FROM channels WHERE chanid=?1");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, chanid)) { return false; }
        return stmt.step();
    }

    std::vector<ChannelRow> load_pinned() override {
        std::vector<ChannelRow> rows;
        auto stmt = db().prepare(
            "SELECT chanid,proto_type,last_message_rowid,last_read_rowid,"
            "       unread_count,pinned_order,draft_text,muted "
            "FROM channels WHERE pinned_order>0 ORDER BY pinned_order ASC");
        if (!stmt.isPrepared()) { return rows; }
        while (stmt.stepRow()) {
            ChannelRow row;
            row.chanid = stmt.columnText(0);
            row.proto_type = stmt.columnText(1);
            row.last_message_rowid = stmt.columnInt64(2);
            row.last_read_rowid = stmt.columnInt64(3);
            row.unread_count = stmt.columnInt(4);
            row.pinned_order = stmt.columnInt(5);
            row.draft_text = stmt.columnText(6);
            row.muted = stmt.columnInt(7);
            rows.push_back(std::move(row));
        }
        return rows;
    }

    bool add_peer(const PeerRow& row) override {
        auto stmt = db().prepare(
            "INSERT OR REPLACE INTO peers "
            "(peer_number,public_key,name,nickname,avatar_url,status_text) "
            "VALUES (?1,?2,?3,?4,?5,?6)");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, row.peer_number)) { return false; }
        if (!stmt.bind(2, row.public_key.c_str())) { return false; }
        if (!stmt.bind(3, row.name.c_str())) { return false; }
        if (!stmt.bind(4, row.nickname.c_str())) { return false; }
        if (!stmt.bind(5, row.avatar_url.c_str())) { return false; }
        if (!stmt.bind(6, row.status_text.c_str())) { return false; }
        return stmt.step();
    }

    std::unique_ptr<PeerRow> get_peer(int peer_number) override {
        auto stmt = db().prepare(
            "SELECT peer_number,public_key,name,nickname,avatar_url,status_text "
            "FROM peers WHERE peer_number=?1");
        if (!stmt.isPrepared()) { return nullptr; }
        if (!stmt.bind(1, peer_number)) { return nullptr; }
        if (!stmt.stepRow()) { return nullptr; }
        auto row = std::unique_ptr<PeerRow>(new PeerRow());
        row->peer_number = stmt.columnInt(0);
        row->public_key = stmt.columnText(1);
        row->name = stmt.columnText(2);
        row->nickname = stmt.columnText(3);
        row->avatar_url = stmt.columnText(4);
        row->status_text = stmt.columnText(5);
        return row;
    }

    bool begin_write_transaction() override { return db().beginTransaction(); }
    bool commit_transaction() override { return db().commitTransaction(); }
};

class ChannelDbSyncSafe final : public ChannelDbSyncSafeInterface {
    std::shared_ptr<SqliteConnectionSafe> m_conn;
    ChannelDbSync m_impl;
public:
    explicit ChannelDbSyncSafe(std::shared_ptr<SqliteConnectionSafe> conn)
        : m_conn(conn), m_impl(conn) {}
    ChannelDbSyncInterface& get() override { return m_impl; }
};

class ChannelDbAsync final : public ChannelDbAsyncInterface {
    std::shared_ptr<ChannelDbSyncSafeInterface> m_sync;
    std::shared_ptr<WriteQueue> m_queue;
public:
    ChannelDbAsync(std::shared_ptr<ChannelDbSyncSafeInterface> sync,
                   std::shared_ptr<WriteQueue> queue)
        : m_sync(std::move(sync)), m_queue(std::move(queue)) {}

    void post(std::function<void()> task) { m_queue->post(std::move(task)); }

    void add_channel(ChannelRow row, std::function<void(bool)> done) override {
        auto sync = m_sync;
        post([sync, row, done]() {
            bool ok = sync->get().add_channel(row);
            if (done) { done(ok); }
        });
    }

    void get_channel(std::string chanid,
                     std::function<void(std::unique_ptr<ChannelRow>)> done) override {
        auto sync = m_sync;
        post([sync, chanid, done]() {
            auto row = sync->get().get_channel(chanid.c_str());
            if (done) { done(std::move(row)); }
        });
    }

    void update_channel(std::string chanid, ChannelUpdate upd,
                        std::function<void(bool)> done) override {
        auto sync = m_sync;
        post([sync, chanid, upd,
              done]() {
            bool ok = sync->get().update_channel(chanid.c_str(), upd);
            if (done) { done(ok); }
        });
    }

    void delete_channel(std::string chanid, std::function<void(bool)> done) override {
        auto sync = m_sync;
        post([sync, chanid, done]() {
            bool ok = sync->get().delete_channel(chanid.c_str());
            if (done) { done(ok); }
        });
    }

    void load_pinned(std::function<void(std::vector<ChannelRow>)> done) override {
        auto sync = m_sync;
        post([sync, done]() {
            auto rows = sync->get().load_pinned();
            if (done) { done(std::move(rows)); }
        });
    }

    void add_peer(PeerRow row, std::function<void(bool)> done) override {
        auto sync = m_sync;
        post([sync, row, done]() {
            bool ok = sync->get().add_peer(row);
            if (done) { done(ok); }
        });
    }

    void get_peer(int peer_number,
                  std::function<void(std::unique_ptr<PeerRow>)> done) override {
        auto sync = m_sync;
        post([sync, peer_number, done]() {
            auto row = sync->get().get_peer(peer_number);
            if (done) { done(std::move(row)); }
        });
    }

    void close(std::function<void()> done) override {
        auto sync = m_sync;
        post([sync, done]() {
            if (done) { done(); }
        });
    }
};

}  // namespace

std::shared_ptr<ChannelDbSyncSafeInterface> create_channel_db(
    std::shared_ptr<SqliteConnectionSafe> conn) {
    return std::make_shared<ChannelDbSyncSafe>(std::move(conn));
}

std::shared_ptr<ChannelDbAsyncInterface> create_channel_db_async(
    std::shared_ptr<ChannelDbSyncSafeInterface> sync,
    std::shared_ptr<WriteQueue> queue) {
    return std::make_shared<ChannelDbAsync>(std::move(sync), std::move(queue));
}

bool init_channel_db(SqliteDb& db) {
    return db.exec(
        "CREATE TABLE IF NOT EXISTS channels ("
        "  chanid             TEXT PRIMARY KEY,"
        "  proto_type         TEXT DEFAULT 'tox',"
        "  last_message_rowid INTEGER NOT NULL DEFAULT 0,"
        "  last_read_rowid    INTEGER NOT NULL DEFAULT 0,"
        "  unread_count       INTEGER DEFAULT 0,"
        "  pinned_order       INTEGER DEFAULT 0,"
        "  draft_text         TEXT DEFAULT '',"
        "  muted              INTEGER DEFAULT 0,"
        "  created_at         TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")") &&
        db.exec(
        "CREATE TABLE IF NOT EXISTS peers ("
        "  peer_number  INTEGER PRIMARY KEY,"
        "  public_key   TEXT DEFAULT '',"
        "  name         TEXT DEFAULT '',"
        "  nickname     TEXT DEFAULT '',"
        "  avatar_url   TEXT DEFAULT '',"
        "  status_text  TEXT DEFAULT '',"
        "  updated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")");
}

bool drop_channel_db(SqliteDb& db) {
    return db.exec("DROP TABLE IF EXISTS channels") &&
           db.exec("DROP TABLE IF EXISTS peers");
}
