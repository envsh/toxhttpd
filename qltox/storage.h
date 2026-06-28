#pragma once
#include <cstdio>
#include <sqlite3.h>
#include <qglobal.h>
#include <qmutex.h>
#include <qthread.h>
#include <qwaitcondition.h>
#include <functional>
#include <queue>
#include <memory>
#include <string>
#include <vector>
#include <chrono>

struct SlowGuard {
    const char* op;
    int thresholdMs;
    std::chrono::steady_clock::time_point start;
    SlowGuard(const char* op, int thresholdMs)
        : op(op), thresholdMs(thresholdMs)
        , start(std::chrono::steady_clock::now()) {}
    ~SlowGuard() {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (ms > thresholdMs) {
            qWarning("[SLOW] %s took %lldms (threshold %dms)",
                     op, (long long)ms, thresholdMs);
        }
    }
};

struct TimedReadGuard {
    TimedReadGuard(sqlite3* db, int timeoutMs);
    ~TimedReadGuard();
    bool timedOut() const { return m_timedOut; }
private:
    static int onProgress(void* ptr);
    sqlite3* m_db;
    std::chrono::steady_clock::time_point m_start;
    int m_timeoutMs;
    bool m_timedOut = false;
};

class SqliteStatement {
    sqlite3_stmt* m_stmt = nullptr;
public:
    SqliteStatement() = default;
    explicit SqliteStatement(sqlite3* db, const char* sql);
    ~SqliteStatement();
    SqliteStatement(SqliteStatement&&) noexcept;
    SqliteStatement& operator=(SqliteStatement&&) noexcept;
    SqliteStatement(const SqliteStatement&) = delete;
    SqliteStatement& operator=(const SqliteStatement&) = delete;

    bool bind(int idx, int val);
    bool bind(int idx, int64_t val);
    bool bind(int idx, const char* val);
    bool bind(int idx, const void* data, int len);
    bool bindNull(int idx);

    bool step();
    bool stepRow();

    int         columnInt(int col);
    int64_t     columnInt64(int col);
    const char* columnText(int col);
    int         columnBytes(int col);
    const void* columnBlob(int col);

    void reset();
    void finalize();
    bool isPrepared() const { return m_stmt != nullptr; }
};

class SqliteDb {
    sqlite3* m_db = nullptr;
public:
    SqliteDb() = default;
    explicit SqliteDb(sqlite3* db) : m_db(db) {}

    sqlite3* raw() const { return m_db; }
    bool isOpen() const { return m_db != nullptr; }

    bool exec(const char* sql);
    bool tryExec(const char* sql);
    SqliteStatement prepare(const char* sql);

    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
};

inline const char* sqliteError(sqlite3_stmt* stmt) {
    return sqlite3_errmsg(sqlite3_db_handle(stmt));
}
inline const char* sqliteError(SqliteDb& db) {
    return sqlite3_errmsg(db.raw());
}

class SqliteConnectionSafe;

class SqliteLockedDb {
    friend class SqliteConnectionSafe;
    QMutex* m_mutex = nullptr;
    SqliteDb* m_db = nullptr;
    explicit SqliteLockedDb(QMutex& mutex, SqliteDb& db)
        : m_mutex(&mutex), m_db(&db) { m_mutex->lock(); }
public:
    ~SqliteLockedDb() { if (m_mutex) { m_mutex->unlock(); m_mutex = nullptr; } }
    SqliteLockedDb(SqliteLockedDb&& other) noexcept
        : m_mutex(other.m_mutex), m_db(other.m_db) { other.m_mutex = nullptr; }
    SqliteLockedDb(const SqliteLockedDb&) = delete;
    SqliteLockedDb& operator=(const SqliteLockedDb&) = delete;

    SqliteDb* operator->() { return m_db; }
    SqliteDb& operator*() { return *m_db; }
};

class SqliteConnectionSafe {
    QMutex m_mutex;
    SqliteDb m_db;
public:
    explicit SqliteConnectionSafe(sqlite3* db) : m_db(db) {}
    SqliteLockedDb get() { return SqliteLockedDb(m_mutex, m_db); }
};

class ChannelDbSyncInterface;
class ChannelDbSyncSafeInterface;
class ChannelDbAsyncInterface;
class MessageDbSyncInterface;
class MessageDbSyncSafeInterface;
class MessageDbAsyncInterface;
class PendingDbSyncInterface;
class PendingDbSyncSafeInterface;
class PendingDbAsyncInterface;
class CacheDbSyncInterface;
class CacheDbSyncSafeInterface;
class CacheDbAsyncInterface;
class WriteQueue : public QThread {
    QMutex m_mutex;
    QWaitCondition m_cond;
    std::queue<std::function<void()>> m_queue;
    bool m_stopped = false;
protected:
    void run() override;
public:
    WriteQueue();
    ~WriteQueue();
    void post(std::function<void()> task);
    void flush();
    void stop();
};

class Storage {
public:
    static Storage& instance();

    bool init(const char* dataDir);
    void close();

    // 域类 accessors — sync（单线程/写队列线程直接调用）
    ChannelDbSyncInterface*    channelDb();
    MessageDbSyncInterface*    messageDb();
    PendingDbSyncInterface*    pendingDb();
    CacheDbSyncInterface*      cacheDb();

    // 域类 accessors — async（投递到写队列）
    ChannelDbAsyncInterface*   channelDbAsync();
    MessageDbAsyncInterface*   messageDbAsync();
    PendingDbAsyncInterface*   pendingDbAsync();
    CacheDbAsyncInterface*     cacheDbAsync();

    // 工具
    SqliteDb& msgDb()          { return m_msgDb; }
    SqliteDb& cacheDbConn()    { return m_cacheDb; }
    SqliteDb& bigCacheDb()     { return m_bigCacheDb; }

    const char* sqliteVersion() const { return m_sqliteVersion.c_str(); }
    bool hasFts5() const    { return m_hasFts5; }
    bool hasTrigram() const { return m_hasTrigram; }

private:
    Storage();
    ~Storage();
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    bool openDb(const char* path);
    bool initSyncDomains();
    bool initAsyncDomains();
    void checkFeatures();

    SqliteDb m_msgDb;
    SqliteDb m_cacheDb;
    SqliteDb m_bigCacheDb;

    std::shared_ptr<SqliteConnectionSafe> m_msgConn;
    std::shared_ptr<SqliteConnectionSafe> m_cacheConn;
    std::shared_ptr<SqliteConnectionSafe> m_bigConn;
    std::shared_ptr<WriteQueue> m_queue;

    std::shared_ptr<ChannelDbSyncSafeInterface> m_channelDb;
    std::shared_ptr<ChannelDbAsyncInterface>    m_channelDbAsync;
    std::shared_ptr<MessageDbSyncSafeInterface> m_messageDb;
    std::shared_ptr<MessageDbAsyncInterface>    m_messageDbAsync;
    std::shared_ptr<PendingDbSyncSafeInterface> m_pendingDb;
    std::shared_ptr<PendingDbAsyncInterface>    m_pendingDbAsync;
    std::shared_ptr<CacheDbSyncSafeInterface>   m_cacheDbObj;
    std::shared_ptr<CacheDbAsyncInterface>      m_cacheDbAsync;

    std::string m_sqliteVersion;
    bool m_hasFts5 = false;
    bool m_hasTrigram = false;
};

std::string mediaCacheKey(const char* prefix, const QString& mxcUrl);
