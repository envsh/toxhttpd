#include "storage.h"
#include <qglobal.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>

typedef std::chrono::steady_clock::time_point TimePoint;

static long long elapsedMs(TimePoint start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
}

static void mkdirRecursive(const char *path) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static void qExec(sqlite3 *db, const char *sql) {
    char *err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        qFatal("SQL error: %s\nSQL: %s", err ? err : "unknown", sql);
    }
}

static bool qTryExec(sqlite3 *db, const char *sql) {
    char *err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        qWarning("SQL (fallible): %s", err ? err : "unknown");
        sqlite3_free(err);
        return false;
    }
    return true;
}

Storage &Storage::instance() {
    static Storage s;
    return s;
}

Storage::~Storage() { close(); }

static std::string pathFor(const char *dir, const char *name) {
    std::string s(dir);
    s += '/';
    s += name;
    return s;
}

bool Storage::init(const char *dataDir) {
    auto totalStart = std::chrono::steady_clock::now();

    mkdirRecursive(dataDir);

    std::string msgPath = pathFor(dataDir, "message.db");
    std::string cachePath = pathFor(dataDir, "cache.db");
    std::string bigCachePath = pathFor(dataDir, "big_cache.db");

    if (!openDb(&m_msgDb, msgPath.c_str()))      return false;
    if (!openDb(&m_cacheDb, cachePath.c_str()))   return false;
    if (!openDb(&m_bigCacheDb, bigCachePath.c_str())) return false;

    if (!setupMessageDb()) return false;
    if (!setupCacheDb()) return false;
    if (!setupBigCacheDb()) return false;

    checkFeatures();

    m_ready = true;
    qDebug("Storage init complete in %lldms", elapsedMs(totalStart));
    return true;
}

void Storage::close() {
    if (m_msgDb)     sqlite3_close(m_msgDb);
    if (m_cacheDb)   sqlite3_close(m_cacheDb);
    if (m_bigCacheDb) sqlite3_close(m_bigCacheDb);
    m_msgDb = m_cacheDb = m_bigCacheDb = nullptr;
    m_ready = false;
}

static const char *basenamePtr(const char *path) {
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

bool Storage::openDb(sqlite3 **db, const char *path) {
    auto start = std::chrono::steady_clock::now();

    int rc = sqlite3_open_v2(path, db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        qWarning("DB open failed [%s]: %s", path, sqlite3_errmsg(*db));
        return false;
    }

    qExec(*db, "PRAGMA journal_mode=WAL");
    qExec(*db, "PRAGMA synchronous=NORMAL");
    qExec(*db, "PRAGMA page_size=4096");
    qExec(*db, "PRAGMA cache_size=-64000");
    qExec(*db, "PRAGMA temp_store=MEMORY");
    qExec(*db, "PRAGMA mmap_size=268435456");
    qExec(*db, "PRAGMA busy_timeout=5000");
    qExec(*db, "PRAGMA foreign_keys=ON");

    qDebug("DB opened [%s] in %lldms", basenamePtr(path), elapsedMs(start));
    return true;
}

bool Storage::setupMessageDb() {
    auto start = std::chrono::steady_clock::now();

    exec("CREATE TABLE IF NOT EXISTS channels ("
         "  chanid             TEXT PRIMARY KEY,"
         "  proto_type         TEXT DEFAULT 'tox',"
         "  last_message_rowid INTEGER NOT NULL DEFAULT 0,"
         "  last_read_rowid    INTEGER NOT NULL DEFAULT 0,"
         "  unread_count       INTEGER DEFAULT 0,"
         "  pinned_order       INTEGER DEFAULT 0,"
         "  draft_text         TEXT DEFAULT '',"
         "  muted              INTEGER DEFAULT 0,"
         "  created_at         TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
         ")");

    exec("CREATE TABLE IF NOT EXISTS messages ("
         "  rowid       INTEGER PRIMARY KEY AUTOINCREMENT,"
         "  event_id    TEXT UNIQUE,"
         "  chanid      TEXT NOT NULL REFERENCES channels(chanid),"
         "  data        TEXT NOT NULL,"
         "  etype       INTEGER DEFAULT 0,"
         "  sender_name TEXT DEFAULT '',"
         "  sender_nick TEXT DEFAULT '',"
         "  peer_number INTEGER DEFAULT -1,"
         "  sender_pubkey TEXT DEFAULT '',"
         "  signature   TEXT DEFAULT '',"
         "  avatar_url  TEXT DEFAULT '',"
         "  time_text   TEXT DEFAULT '',"
         "  ip_address  TEXT DEFAULT '',"
         "  category    TEXT DEFAULT '',"
         "  caption     TEXT DEFAULT '',"
         "  media_url   TEXT,"
         "  media_mime  TEXT,"
         "  media_width INTEGER DEFAULT 0,"
         "  media_height INTEGER DEFAULT 0,"
         "  file_name   TEXT,"
         "  file_size   INTEGER DEFAULT 0,"
         "  duration_sec INTEGER DEFAULT 0,"
         "  local_path  TEXT,"
         "  gif_path    TEXT DEFAULT '',"
         "  thumbnail_key TEXT,"
         "  cache_tag   INTEGER DEFAULT 0,"
         "  send_state  INTEGER DEFAULT 0,"
         "  reply_to_rowid INTEGER DEFAULT 0,"
         "  edited      INTEGER DEFAULT 0,"
         "  deleted_at  TIMESTAMP,"
         "  forwarded_from TEXT DEFAULT '',"
         "  mention     INTEGER DEFAULT 0,"
         "  created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
         ")");

    exec("CREATE INDEX IF NOT EXISTS idx_messages_chanid"
         "  ON messages(chanid, rowid DESC)");
    exec("CREATE INDEX IF NOT EXISTS idx_messages_media_url"
         "  ON messages(media_url) WHERE media_url IS NOT NULL");
    exec("CREATE INDEX IF NOT EXISTS idx_messages_send_state"
         "  ON messages(chanid, send_state)");

    exec("CREATE TABLE IF NOT EXISTS peers ("
         "  peer_number  INTEGER PRIMARY KEY,"
         "  public_key   TEXT DEFAULT '',"
         "  name         TEXT DEFAULT '',"
         "  nickname     TEXT DEFAULT '',"
         "  avatar_url   TEXT DEFAULT '',"
         "  status_text  TEXT DEFAULT '',"
         "  updated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
         ")");

    exec("CREATE TABLE IF NOT EXISTS bookmarks ("
         "  id             INTEGER PRIMARY KEY AUTOINCREMENT,"
         "  message_rowid  INTEGER NOT NULL,"
         "  chanid         TEXT NOT NULL REFERENCES channels(chanid),"
         "  note           TEXT DEFAULT '',"
         "  tag            TEXT DEFAULT '',"
         "  created_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
         "  UNIQUE(message_rowid)"
         ")");

    exec("CREATE TABLE IF NOT EXISTS pending_messages ("
         "  id             INTEGER PRIMARY KEY AUTOINCREMENT,"
         "  chanid         TEXT NOT NULL,"
         "  peer_number    INTEGER DEFAULT -1,"
         "  data           TEXT NOT NULL,"
         "  etype          INTEGER DEFAULT 0,"
         "  message_text   TEXT DEFAULT '',"
         "  media_url      TEXT DEFAULT '',"
         "  file_name      TEXT DEFAULT '',"
         "  file_size      INTEGER DEFAULT 0,"
         "  retry_count    INTEGER DEFAULT 0,"
         "  max_retries    INTEGER DEFAULT 3,"
         "  last_error     TEXT DEFAULT '',"
         "  status         INTEGER DEFAULT 0,"
         "  created_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
         ")");
    exec("CREATE INDEX IF NOT EXISTS idx_pending_status"
         "  ON pending_messages(status)");

    exec("CREATE TABLE IF NOT EXISTS reactions ("
         "  message_rowid  INTEGER NOT NULL REFERENCES messages(rowid),"
         "  emoji          TEXT NOT NULL,"
         "  sender_name    TEXT DEFAULT '',"
         "  created_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
         "  PRIMARY KEY (message_rowid, emoji, sender_name)"
         ")");
    exec("CREATE INDEX IF NOT EXISTS idx_reactions_message"
         "  ON reactions(message_rowid)");

    exec("CREATE TABLE IF NOT EXISTS translations ("
         "  message_rowid    INTEGER NOT NULL REFERENCES messages(rowid) ON DELETE CASCADE,"
         "  target_lang      TEXT NOT NULL,"
         "  translated_text  TEXT NOT NULL,"
         "  translated_entities TEXT,"
         "  source_lang      TEXT,"
         "  provider         TEXT DEFAULT 'builtin',"
         "  created_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
         "  PRIMARY KEY (message_rowid, target_lang)"
         ")");

    if (!tryExecDb(m_msgDb,
            "CREATE VIRTUAL TABLE IF NOT EXISTS messages_fts USING fts5("
            "  content,"
            "  tokenize='trigram case_sensitive 0',"
            "  content='messages',"
            "  content_rowid='rowid')")) {
        qWarning("Trigram not available, falling back to unicode61");
        exec("CREATE VIRTUAL TABLE IF NOT EXISTS messages_fts USING fts5("
             "  content,"
             "  tokenize='unicode61',"
             "  content='messages',"
             "  content_rowid='rowid')");
    }

    exec("CREATE TRIGGER IF NOT EXISTS messages_fts_insert"
         " AFTER INSERT ON messages BEGIN"
         "  INSERT INTO messages_fts(rowid, content)"
         "  VALUES (NEW.rowid, NEW.data);"
         " END");
    exec("CREATE TRIGGER IF NOT EXISTS messages_fts_delete"
         " AFTER DELETE ON messages BEGIN"
         "  INSERT INTO messages_fts(messages_fts, rowid, content)"
         "  VALUES('delete', OLD.rowid, OLD.data);"
         " END");
    exec("CREATE TRIGGER IF NOT EXISTS messages_fts_update"
         " AFTER UPDATE ON messages BEGIN"
         "  INSERT INTO messages_fts(messages_fts, rowid, content)"
         "  VALUES('delete', OLD.rowid, OLD.data);"
         "  INSERT INTO messages_fts(rowid, content)"
         "  VALUES (NEW.rowid, NEW.data);"
         " END");

    exec("CREATE TABLE IF NOT EXISTS schema_version ("
         "  version     INTEGER PRIMARY KEY,"
         "  applied_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
         ")");
    exec("INSERT OR IGNORE INTO schema_version(version) VALUES(3)");

    qDebug("message.db tables created in %lldms (8 tables, 5 indexes, 3 triggers)",
           elapsedMs(start));
    return true;
}

bool Storage::setupCacheDb() {
    auto start = std::chrono::steady_clock::now();

    execDb(m_cacheDb, "CREATE TABLE IF NOT EXISTS cache ("
            "  key         TEXT PRIMARY KEY,"
            "  data        BLOB NOT NULL,"
            "  mime_type   TEXT DEFAULT '',"
            "  tag         INTEGER DEFAULT 0,"
            "  access_time INTEGER NOT NULL,"
            "  size        INTEGER NOT NULL,"
            "  created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")");

    execDb(m_cacheDb, "CREATE TABLE IF NOT EXISTS file_refs ("
            "  key         TEXT PRIMARY KEY,"
            "  file_path   TEXT NOT NULL,"
            "  mime_type   TEXT DEFAULT '',"
            "  tag         INTEGER DEFAULT 0,"
            "  access_time INTEGER NOT NULL,"
            "  size        INTEGER NOT NULL,"
            "  created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")");

    execDb(m_cacheDb, "CREATE INDEX IF NOT EXISTS idx_cache_tag ON cache(tag)");
    execDb(m_cacheDb, "CREATE INDEX IF NOT EXISTS idx_cache_access ON cache(access_time)");
    execDb(m_cacheDb, "CREATE INDEX IF NOT EXISTS idx_filerefs_tag ON file_refs(tag)");
    execDb(m_cacheDb, "CREATE INDEX IF NOT EXISTS idx_filerefs_access ON file_refs(access_time)");

    qDebug("cache.db tables created in %lldms (2 tables, 4 indexes)", elapsedMs(start));
    return true;
}

bool Storage::setupBigCacheDb() {
    auto start = std::chrono::steady_clock::now();

    execDb(m_bigCacheDb, "CREATE TABLE IF NOT EXISTS big_cache ("
            "  key         TEXT PRIMARY KEY,"
            "  file_path   TEXT NOT NULL,"
            "  mime_type   TEXT DEFAULT '',"
            "  tag         INTEGER DEFAULT 0,"
            "  access_time INTEGER NOT NULL,"
            "  size        INTEGER NOT NULL,"
            "  created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")");

    execDb(m_bigCacheDb, "CREATE INDEX IF NOT EXISTS idx_bigcache_tag ON big_cache(tag)");
    execDb(m_bigCacheDb, "CREATE INDEX IF NOT EXISTS idx_bigcache_access ON big_cache(access_time)");

    qDebug("big_cache.db tables created in %lldms (1 table, 2 indexes)", elapsedMs(start));
    return true;
}

void Storage::checkFeatures() {
    m_sqliteVersion = sqlite3_libversion();
    qDebug("SQLite version: %s", m_sqliteVersion.c_str());

    m_hasFts5 = qTryExec(m_msgDb,
        "CREATE VIRTUAL TABLE IF NOT EXISTS _t_fts5 USING fts5(c)");
    qExec(m_msgDb, "DROP TABLE IF EXISTS _t_fts5");
    qDebug("FTS5: %s", m_hasFts5 ? "OK" : "NOT AVAILABLE");

    m_hasTrigram = qTryExec(m_msgDb,
        "CREATE VIRTUAL TABLE IF NOT EXISTS _t_tri USING fts5(c, tokenize='trigram')");
    qExec(m_msgDb, "DROP TABLE IF EXISTS _t_tri");
    qDebug("Trigram: %s", m_hasTrigram ? "OK" : "NOT AVAILABLE (CJK search degraded)");
}

void Storage::exec(const char *sql) {
    qExec(m_msgDb, sql);
}

void Storage::execDb(sqlite3 *db, const char *sql) {
    qExec(db, sql);
}

bool Storage::tryExecDb(sqlite3 *db, const char *sql) {
    return qTryExec(db, sql);
}
