#pragma once
#include <sqlite3.h>
#include <string>

class Storage {
public:
    static Storage& instance();

    bool init(const char* dataDir);
    void close();
    bool isReady() const { return m_ready; }

    const char* sqliteVersion() const { return m_sqliteVersion.c_str(); }
    bool hasFts5() const { return m_hasFts5; }
    bool hasTrigram() const { return m_hasTrigram; }

private:
    Storage() = default;
    ~Storage();
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    bool openDb(sqlite3** db, const char* path);
    bool setupMessageDb();
    bool setupCacheDb();
    bool setupBigCacheDb();
    void checkFeatures();

    void exec(const char* sql);
    void execDb(sqlite3* db, const char* sql);
    bool tryExecDb(sqlite3* db, const char* sql);

    sqlite3* m_msgDb = nullptr;
    sqlite3* m_cacheDb = nullptr;
    sqlite3* m_bigCacheDb = nullptr;
    bool m_ready = false;

    std::string m_sqliteVersion;
    bool m_hasFts5 = false;
    bool m_hasTrigram = false;
};
