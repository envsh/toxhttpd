#include "cache_db.h"
#include <ctime>

namespace {

class CacheDbSync final : public CacheDbSyncInterface {
    std::shared_ptr<SqliteConnectionSafe> m_cacheConn;
    std::shared_ptr<SqliteConnectionSafe> m_bigConn;
public:
    CacheDbSync(std::shared_ptr<SqliteConnectionSafe> cacheConn,
                std::shared_ptr<SqliteConnectionSafe> bigConn)
        : m_cacheConn(std::move(cacheConn)), m_bigConn(std::move(bigConn)) {}

    SqliteDb& cacheDb() { return *m_cacheConn->get(); }
    SqliteDb& bigDb() { return *m_bigConn->get(); }

    bool put(const char* key, const void* data, size_t size,
             const char* mime, int tag) override {
        SlowGuard _w("cache::put", 60);
        auto stmt = cacheDb().prepare(
            "INSERT OR REPLACE INTO cache "
            "(key,data,mime_type,tag,access_time,size) "
            "VALUES (?1,?2,?3,?4,?5,?6)");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, key)) { return false; }
        if (!stmt.bind(2, data, (int)size)) { return false; }
        if (!stmt.bind(3, mime)) { return false; }
        if (!stmt.bind(4, tag)) { return false; }
        if (!stmt.bind(5, (int64_t)std::time(nullptr))) { return false; }
        if (!stmt.bind(6, (int64_t)size)) { return false; }
        if (!stmt.step()) {
            qWarning("CacheDb::put failed for %s: %s",
                     key, sqliteError(cacheDb()));
            return false;
        }
        return true;
    }

    std::vector<uint8_t> get(const char* key, std::string* out_mime) override {
        SlowGuard _w("cache::get", 30);
        TimedReadGuard _t(cacheDb().raw(), 50);
        std::vector<uint8_t> result;
        auto stmt = cacheDb().prepare(
            "SELECT data,mime_type FROM cache WHERE key=?1");
        if (!stmt.isPrepared()) { return result; }
        if (!stmt.bind(1, key)) { return result; }
        if (!stmt.stepRow()) {
            if (_t.timedOut()) { qWarning("cache::get timed out for '%s'", key); }
            return result;
        }

        int bytes = stmt.columnBytes(0);
        const void* blob = stmt.columnBlob(0);
        if (blob && bytes > 0) {
            const uint8_t* p = (const uint8_t*)blob;
            result.assign(p, p + bytes);
        }
        if (out_mime) { *out_mime = stmt.columnText(1); }

        // 更新 access_time
        auto upd = cacheDb().prepare(
            "UPDATE cache SET access_time=?1 WHERE key=?2");
        if (upd.isPrepared()) {
            upd.bind(1, (int64_t)std::time(nullptr));
            upd.bind(2, key);
            upd.step();
        }
        return result;
    }

    bool exists(const char* key) override {
        auto stmt = cacheDb().prepare("SELECT 1 FROM cache WHERE key=?1");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, key)) { return false; }
        return stmt.stepRow();
    }

    bool remove(const char* key) override {
        auto stmt = cacheDb().prepare("DELETE FROM cache WHERE key=?1");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, key)) { return false; }
        return stmt.step();
    }

    bool clear_by_tag(int tag) override {
        if (tag == 0) {
            return cacheDb().exec("DELETE FROM cache") &&
                   cacheDb().exec("DELETE FROM file_refs") &&
                   bigDb().exec("DELETE FROM big_cache");
        }
        auto stmt = cacheDb().prepare("DELETE FROM cache WHERE tag=?1");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, tag)) { return false; }
        stmt.step();
        auto stmt2 = cacheDb().prepare("DELETE FROM file_refs WHERE tag=?1");
        if (!stmt2.isPrepared()) { return false; }
        stmt2.bind(1, tag);
        stmt2.step();
        auto stmt3 = bigDb().prepare("DELETE FROM big_cache WHERE tag=?1");
        if (!stmt3.isPrepared()) { return false; }
        stmt3.bind(1, tag);
        return stmt3.step();
    }

    int64_t total_cache_size() override {
        auto stmt = cacheDb().prepare("SELECT COALESCE(SUM(size),0) FROM cache");
        if (!stmt.isPrepared()) { return 0; }
        if (!stmt.stepRow()) { return 0; }
        int64_t s1 = stmt.columnInt64(0);
        auto stmt2 = cacheDb().prepare("SELECT COALESCE(SUM(size),0) FROM file_refs");
        if (!stmt2.isPrepared()) { return s1; }
        if (!stmt2.stepRow()) { return s1; }
        return s1 + stmt2.columnInt64(0);
    }

    bool put_ref(const char* key, const char* file_path,
                 const char* mime, int tag, int64_t size) override {
        SlowGuard _w("cache::put_ref", 60);
        auto stmt = cacheDb().prepare(
            "INSERT OR REPLACE INTO file_refs "
            "(key,file_path,mime_type,tag,access_time,size) "
            "VALUES (?1,?2,?3,?4,?5,?6)");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, key)) { return false; }
        if (!stmt.bind(2, file_path)) { return false; }
        if (!stmt.bind(3, mime)) { return false; }
        if (!stmt.bind(4, tag)) { return false; }
        if (!stmt.bind(5, (int64_t)std::time(nullptr))) { return false; }
        if (!stmt.bind(6, size)) { return false; }
        return stmt.step();
    }

    std::string get_ref_path(const char* key) override {
        SlowGuard _w("cache::get_ref", 30);
        TimedReadGuard _t(cacheDb().raw(), 50);
        auto stmt = cacheDb().prepare(
            "SELECT file_path FROM file_refs WHERE key=?1");
        if (!stmt.isPrepared()) { return {}; }
        if (!stmt.bind(1, key)) { return {}; }
        if (!stmt.stepRow()) {
            if (_t.timedOut()) { qWarning("cache::get_ref timed out for '%s'", key); }
            return {};
        }
        // 更新 access_time
        auto upd = cacheDb().prepare(
            "UPDATE file_refs SET access_time=?1 WHERE key=?2");
        if (upd.isPrepared()) {
            upd.bind(1, (int64_t)std::time(nullptr));
            upd.bind(2, key);
            upd.step();
        }
        return stmt.columnText(0);
    }

    bool remove_ref(const char* key) override {
        auto stmt = cacheDb().prepare("DELETE FROM file_refs WHERE key=?1");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, key)) { return false; }
        return stmt.step();
    }

    bool put_big_ref(const char* key, const char* file_path,
                     const char* mime, int tag, int64_t size) override {
        SlowGuard _w("cache::put_big", 60);
        auto stmt = bigDb().prepare(
            "INSERT OR REPLACE INTO big_cache "
            "(key,file_path,mime_type,tag,access_time,size) "
            "VALUES (?1,?2,?3,?4,?5,?6)");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, key)) { return false; }
        if (!stmt.bind(2, file_path)) { return false; }
        if (!stmt.bind(3, mime)) { return false; }
        if (!stmt.bind(4, tag)) { return false; }
        if (!stmt.bind(5, (int64_t)std::time(nullptr))) { return false; }
        if (!stmt.bind(6, size)) { return false; }
        return stmt.step();
    }

    std::string get_big_path(const char* key) override {
        SlowGuard _w("cache::get_big", 30);
        TimedReadGuard _t(bigDb().raw(), 50);
        auto stmt = bigDb().prepare(
            "SELECT file_path FROM big_cache WHERE key=?1");
        if (!stmt.isPrepared()) { return {}; }
        if (!stmt.bind(1, key)) { return {}; }
        if (!stmt.stepRow()) {
            if (_t.timedOut()) { qWarning("cache::get_big timed out for '%s'", key); }
            return {};
        }
        auto upd = bigDb().prepare(
            "UPDATE big_cache SET access_time=?1 WHERE key=?2");
        if (upd.isPrepared()) {
            upd.bind(1, (int64_t)std::time(nullptr));
            upd.bind(2, key);
            upd.step();
        }
        return stmt.columnText(0);
    }

    bool remove_big_ref(const char* key) override {
        auto stmt = bigDb().prepare("DELETE FROM big_cache WHERE key=?1");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, key)) { return false; }
        return stmt.step();
    }

    int64_t total_big_cache_size() override {
        auto stmt = bigDb().prepare("SELECT COALESCE(SUM(size),0) FROM big_cache");
        if (!stmt.isPrepared()) { return 0; }
        if (!stmt.stepRow()) { return 0; }
        return stmt.columnInt64(0);
    }

    bool evict(int64_t target_size) override {
        SlowGuard _w("cache::evict", 500);
        int64_t now = (int64_t)std::time(nullptr);
        int64_t total = total_cache_size();

        if (total > target_size) {
            // 删除超 30 天未访问且单文件 > 2MB 的
            std::string sql =
                "DELETE FROM cache WHERE access_time < " +
                std::to_string(now - 30 * 86400) + " AND size > 2097152";
            cacheDb().exec(sql.c_str());
            // 删除最久未访问的
            auto stmt = cacheDb().prepare(
                "DELETE FROM cache WHERE rowid IN ("
                "  SELECT rowid FROM cache"
                "  ORDER BY access_time ASC LIMIT 100)");
            if (stmt.isPrepared()) { stmt.step(); }
        }

        int64_t bigTotal = total_big_cache_size();
        if (bigTotal > target_size) {
            auto stmt = bigDb().prepare(
                "SELECT key,file_path FROM big_cache"
                "  WHERE access_time < ?1"
                "  ORDER BY access_time ASC LIMIT 100");
            if (!stmt.isPrepared()) { return false; }
            if (!stmt.bind(1, now - 7 * 86400)) { return false; }
            while (stmt.stepRow()) {
                std::string path = stmt.columnText(1);
                if (!path.empty()) {
                    remove(path.c_str());  // 删文件
                }
                remove_big_ref(stmt.columnText(0));
            }
        }
        return true;
    }

    bool vacuum() override {
        return cacheDb().exec("VACUUM") && bigDb().exec("VACUUM");
    }

    bool begin_write_transaction() override {
        return cacheDb().beginTransaction() && bigDb().beginTransaction();
    }
    bool commit_transaction() override {
        return cacheDb().commitTransaction() && bigDb().commitTransaction();
    }
};

class CacheDbSyncSafe final : public CacheDbSyncSafeInterface {
    std::shared_ptr<SqliteConnectionSafe> m_cacheConn;
    std::shared_ptr<SqliteConnectionSafe> m_bigConn;
    CacheDbSync m_impl;
public:
    CacheDbSyncSafe(std::shared_ptr<SqliteConnectionSafe> cacheConn,
                    std::shared_ptr<SqliteConnectionSafe> bigConn)
        : m_cacheConn(cacheConn), m_bigConn(bigConn), m_impl(cacheConn, bigConn) {}
    CacheDbSyncInterface& get() override { return m_impl; }
};

class CacheDbAsync final : public CacheDbAsyncInterface {
    std::shared_ptr<CacheDbSyncSafeInterface> m_sync;
    std::shared_ptr<WriteQueue> m_queue;
public:
    CacheDbAsync(std::shared_ptr<CacheDbSyncSafeInterface> sync,
                 std::shared_ptr<WriteQueue> queue)
        : m_sync(std::move(sync)), m_queue(std::move(queue)) {}

    void post(std::function<void()> task) { m_queue->post(std::move(task)); }

    void put(std::string key, std::vector<uint8_t> data,
             std::string mime, int tag,
             std::function<void(bool)> done) override {
        auto sync = m_sync;
        post([sync, key, data,
              mime, tag, done]() {
            bool ok = sync->get().put(key.c_str(), data.data(), data.size(),
                                      mime.c_str(), tag);
            if (done) { done(ok); }
        });
    }

    void get(std::string key,
             std::function<void(std::vector<uint8_t>, std::string)> done) override {
        auto sync = m_sync;
        post([sync, key, done]() {
            std::string mime;
            auto data = sync->get().get(key.c_str(), &mime);
            if (done) { done(std::move(data), std::move(mime)); }
        });
    }

    void exists(std::string key, std::function<void(bool)> done) override {
        auto sync = m_sync;
        post([sync, key, done]() {
            if (done) { done(sync->get().exists(key.c_str())); }
        });
    }

    void remove(std::string key, std::function<void(bool)> done) override {
        auto sync = m_sync;
        post([sync, key, done]() {
            if (done) { done(sync->get().remove(key.c_str())); }
        });
    }

    void clear_by_tag(int tag, std::function<void(bool)> done) override {
        auto sync = m_sync;
        post([sync, tag, done]() {
            if (done) { done(sync->get().clear_by_tag(tag)); }
        });
    }

    void put_ref(std::string key, std::string file_path,
                 std::string mime, int tag, int64_t size,
                 std::function<void(bool)> done) override {
        auto sync = m_sync;
        post([sync, key, file_path, mime, tag, size, done]() {
            bool ok = sync->get().put_ref(key.c_str(), file_path.c_str(),
                                          mime.c_str(), tag, size);
            if (done) { done(ok); }
        });
    }

    void get_ref_path(std::string key,
                      std::function<void(std::string)> done) override {
        auto sync = m_sync;
        post([sync, key, done]() {
            auto path = sync->get().get_ref_path(key.c_str());
            if (done) { done(std::move(path)); }
        });
    }

    void put_big_ref(std::string key, std::string file_path,
                     std::string mime, int tag, int64_t size,
                     std::function<void(bool)> done) override {
        auto sync = m_sync;
        post([sync, key, file_path, mime, tag, size, done]() {
            bool ok = sync->get().put_big_ref(key.c_str(), file_path.c_str(),
                                              mime.c_str(), tag, size);
            if (done) { done(ok); }
        });
    }

    void get_big_path(std::string key,
                      std::function<void(std::string)> done) override {
        auto sync = m_sync;
        post([sync, key, done]() {
            auto path = sync->get().get_big_path(key.c_str());
            if (done) { done(std::move(path)); }
        });
    }

    void evict(int64_t target_size, std::function<void(bool)> done) override {
        auto sync = m_sync;
        post([sync, target_size, done]() {
            if (done) { done(sync->get().evict(target_size)); }
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

std::shared_ptr<CacheDbSyncSafeInterface> create_cache_db(
    std::shared_ptr<SqliteConnectionSafe> cache_conn,
    std::shared_ptr<SqliteConnectionSafe> big_cache_conn) {
    return std::make_shared<CacheDbSyncSafe>(std::move(cache_conn),
                                             std::move(big_cache_conn));
}

std::shared_ptr<CacheDbAsyncInterface> create_cache_db_async(
    std::shared_ptr<CacheDbSyncSafeInterface> sync,
    std::shared_ptr<WriteQueue> queue) {
    return std::make_shared<CacheDbAsync>(std::move(sync), std::move(queue));
}

bool init_cache_db(SqliteDb& db) {
    if (!db.exec(
        "CREATE TABLE IF NOT EXISTS cache ("
        "  key         TEXT PRIMARY KEY,"
        "  data        BLOB NOT NULL,"
        "  mime_type   TEXT DEFAULT '',"
        "  tag         INTEGER DEFAULT 0,"
        "  access_time INTEGER NOT NULL,"
        "  size        INTEGER NOT NULL,"
        "  created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")")) { return false; }

    if (!db.exec(
        "CREATE TABLE IF NOT EXISTS file_refs ("
        "  key         TEXT PRIMARY KEY,"
        "  file_path   TEXT NOT NULL,"
        "  mime_type   TEXT DEFAULT '',"
        "  tag         INTEGER DEFAULT 0,"
        "  access_time INTEGER NOT NULL,"
        "  size        INTEGER NOT NULL,"
        "  created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")")) { return false; }

    db.exec("CREATE INDEX IF NOT EXISTS idx_cache_tag ON cache(tag)");
    db.exec("CREATE INDEX IF NOT EXISTS idx_cache_access ON cache(access_time)");
    db.exec("CREATE INDEX IF NOT EXISTS idx_filerefs_tag ON file_refs(tag)");
    db.exec("CREATE INDEX IF NOT EXISTS idx_filerefs_access ON file_refs(access_time)");
    return true;
}

bool init_big_cache_db(SqliteDb& db) {
    if (!db.exec(
        "CREATE TABLE IF NOT EXISTS big_cache ("
        "  key         TEXT PRIMARY KEY,"
        "  file_path   TEXT NOT NULL,"
        "  mime_type   TEXT DEFAULT '',"
        "  tag         INTEGER DEFAULT 0,"
        "  access_time INTEGER NOT NULL,"
        "  size        INTEGER NOT NULL,"
        "  created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")")) { return false; }

    db.exec("CREATE INDEX IF NOT EXISTS idx_bigcache_tag ON big_cache(tag)");
    db.exec("CREATE INDEX IF NOT EXISTS idx_bigcache_access ON big_cache(access_time)");
    return true;
}

bool drop_cache_db(SqliteDb& db) {
    return db.exec("DROP TABLE IF EXISTS cache") &&
           db.exec("DROP TABLE IF EXISTS file_refs");
}

bool drop_big_cache_db(SqliteDb& db) {
    return db.exec("DROP TABLE IF EXISTS big_cache");
}
