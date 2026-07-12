#include "sticker_db.h"
#include <cstdio>

namespace {

class StickerDbSync final : public StickerDbSyncInterface {
    std::shared_ptr<SqliteConnectionSafe> m_conn;
public:
    explicit StickerDbSync(std::shared_ptr<SqliteConnectionSafe> conn)
        : m_conn(std::move(conn)) {}

    bool add_pack(const StickerPackRow& pack) override {
        auto _ = m_conn->get();
        auto stmt = _->prepare(
            "INSERT OR REPLACE INTO sticker_packs "
            "(id,title,author,cover_path,installed,position,created_at) "
            "VALUES (?1,?2,?3,?4,?5,?6,COALESCE(?7,strftime('%s','now')))");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, pack.id.c_str())) { return false; }
        if (!stmt.bind(2, pack.title.c_str())) { return false; }
        if (!stmt.bind(3, pack.author.c_str())) { return false; }
        if (!stmt.bind(4, pack.cover_path.c_str())) { return false; }
        if (!stmt.bind(5, pack.installed)) { return false; }
        if (!stmt.bind(6, pack.position)) { return false; }
        if (!stmt.bind(7, pack.created_at)) { return false; }
        if (!stmt.step()) {
            qWarning("StickerDb::add_pack failed for %s", pack.id.c_str());
            return false;
        }
        return true;
    }

    bool delete_pack(const char* pack_id) override {
        auto _ = m_conn->get();
        auto stmt = _->prepare(
            "DELETE FROM sticker_packs WHERE id=?1");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, pack_id)) { return false; }
        return stmt.step();
    }

    bool update_pack_position(const char* pack_id, int position) override {
        auto _ = m_conn->get();
        auto stmt = _->prepare(
            "UPDATE sticker_packs SET position=?2 WHERE id=?1");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, pack_id)) { return false; }
        if (!stmt.bind(2, position)) { return false; }
        return stmt.step();
    }

    std::unique_ptr<StickerPackRow> get_pack(const char* pack_id) override {
        auto _ = m_conn->get();
        auto stmt = _->prepare(
            "SELECT id,title,author,cover_path,installed,position,created_at "
            "FROM sticker_packs WHERE id=?1");
        if (!stmt.isPrepared()) { return nullptr; }
        if (!stmt.bind(1, pack_id)) { return nullptr; }
        if (!stmt.stepRow()) { return nullptr; }
        auto row = std::unique_ptr<StickerPackRow>(new StickerPackRow());
        row->id = stmt.columnText(0);
        row->title = stmt.columnText(1);
        row->author = stmt.columnText(2);
        row->cover_path = stmt.columnText(3);
        row->installed = stmt.columnInt(4);
        row->position = stmt.columnInt(5);
        row->created_at = stmt.columnInt64(6);
        return row;
    }

    std::vector<StickerPackRow> list_packs(int installed) override {
        std::vector<StickerPackRow> rows;
        auto _ = m_conn->get();
        auto stmt = _->prepare(
            "SELECT id,title,author,cover_path,installed,position,created_at "
            "FROM sticker_packs WHERE installed=?1 ORDER BY position ASC, created_at ASC");
        if (!stmt.isPrepared()) { return rows; }
        if (!stmt.bind(1, installed)) { return rows; }
        while (stmt.stepRow()) {
            StickerPackRow row;
            row.id = stmt.columnText(0);
            row.title = stmt.columnText(1);
            row.author = stmt.columnText(2);
            row.cover_path = stmt.columnText(3);
            row.installed = stmt.columnInt(4);
            row.position = stmt.columnInt(5);
            row.created_at = stmt.columnInt64(6);
            rows.push_back(std::move(row));
        }
        return rows;
    }

    bool add_sticker(const StickerRow& sticker) override {
        auto _ = m_conn->get();
        auto stmt = _->prepare(
            "INSERT OR REPLACE INTO stickers "
            "(id,pack_id,file_path,emoji,width,height,size,last_used,position) "
            "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9)");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, sticker.id.c_str())) { return false; }
        if (!stmt.bind(2, sticker.pack_id.c_str())) { return false; }
        if (!stmt.bind(3, sticker.file_path.c_str())) { return false; }
        if (!stmt.bind(4, sticker.emoji.c_str())) { return false; }
        if (!stmt.bind(5, sticker.width)) { return false; }
        if (!stmt.bind(6, sticker.height)) { return false; }
        if (!stmt.bind(7, sticker.size)) { return false; }
        if (!stmt.bind(8, sticker.last_used)) { return false; }
        if (!stmt.bind(9, sticker.position)) { return false; }
        if (!stmt.step()) {
            qWarning("StickerDb::add_sticker failed for %s", sticker.id.c_str());
            return false;
        }
        return true;
    }

    bool delete_sticker(const char* sticker_id) override {
        auto _ = m_conn->get();
        auto stmt = _->prepare("DELETE FROM stickers WHERE id=?1");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, sticker_id)) { return false; }
        return stmt.step();
    }

    bool delete_stickers_by_pack(const char* pack_id) override {
        auto _ = m_conn->get();
        auto stmt = _->prepare("DELETE FROM stickers WHERE pack_id=?1");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, pack_id)) { return false; }
        return stmt.step();
    }

    bool touch_sticker(const char* sticker_id, int64_t now) override {
        auto _ = m_conn->get();
        auto stmt = _->prepare(
            "UPDATE stickers SET last_used=?2 WHERE id=?1");
        if (!stmt.isPrepared()) { return false; }
        if (!stmt.bind(1, sticker_id)) { return false; }
        if (!stmt.bind(2, now)) { return false; }
        return stmt.step();
    }

    std::unique_ptr<StickerRow> get_sticker(const char* sticker_id) override {
        auto _ = m_conn->get();
        auto stmt = _->prepare(
            "SELECT id,pack_id,file_path,emoji,width,height,size,last_used,position "
            "FROM stickers WHERE id=?1");
        if (!stmt.isPrepared()) { return nullptr; }
        if (!stmt.bind(1, sticker_id)) { return nullptr; }
        if (!stmt.stepRow()) { return nullptr; }
        auto row = std::unique_ptr<StickerRow>(new StickerRow());
        row->id = stmt.columnText(0);
        row->pack_id = stmt.columnText(1);
        row->file_path = stmt.columnText(2);
        row->emoji = stmt.columnText(3);
        row->width = stmt.columnInt(4);
        row->height = stmt.columnInt(5);
        row->size = stmt.columnInt(6);
        row->last_used = stmt.columnInt64(7);
        row->position = stmt.columnInt(8);
        return row;
    }

    std::vector<StickerRow> list_stickers(const char* pack_id) override {
        std::vector<StickerRow> rows;
        auto _ = m_conn->get();
        auto stmt = _->prepare(
            "SELECT id,pack_id,file_path,emoji,width,height,size,last_used,position "
            "FROM stickers WHERE pack_id=?1 ORDER BY position ASC");
        if (!stmt.isPrepared()) { return rows; }
        if (!stmt.bind(1, pack_id)) { return rows; }
        while (stmt.stepRow()) {
            StickerRow row;
            row.id = stmt.columnText(0);
            row.pack_id = stmt.columnText(1);
            row.file_path = stmt.columnText(2);
            row.emoji = stmt.columnText(3);
            row.width = stmt.columnInt(4);
            row.height = stmt.columnInt(5);
            row.size = stmt.columnInt(6);
            row.last_used = stmt.columnInt64(7);
            row.position = stmt.columnInt(8);
            rows.push_back(std::move(row));
        }
        return rows;
    }

    std::vector<StickerRow> list_recent_stickers(int limit) override {
        std::vector<StickerRow> rows;
        auto _ = m_conn->get();
        auto stmt = _->prepare(
            "SELECT s.id,s.pack_id,s.file_path,s.emoji,s.width,s.height,"
            "       s.size,s.last_used,s.position "
            "FROM stickers s "
            "WHERE s.last_used > 0 "
            "ORDER BY s.last_used DESC LIMIT ?1");
        if (!stmt.isPrepared()) { return rows; }
        if (!stmt.bind(1, limit)) { return rows; }
        while (stmt.stepRow()) {
            StickerRow row;
            row.id = stmt.columnText(0);
            row.pack_id = stmt.columnText(1);
            row.file_path = stmt.columnText(2);
            row.emoji = stmt.columnText(3);
            row.width = stmt.columnInt(4);
            row.height = stmt.columnInt(5);
            row.size = stmt.columnInt(6);
            row.last_used = stmt.columnInt64(7);
            row.position = stmt.columnInt(8);
            rows.push_back(std::move(row));
        }
        return rows;
    }

    std::vector<StickerRow> search_stickers(const char* query) override {
        std::vector<StickerRow> rows;
        auto _ = m_conn->get();
        auto stmt = _->prepare(
            "SELECT s.id,s.pack_id,s.file_path,s.emoji,s.width,s.height,"
            "       s.size,s.last_used,s.position "
            "FROM stickers s "
            "JOIN sticker_packs p ON s.pack_id = p.id "
            "WHERE s.emoji LIKE ?1 OR p.title LIKE ?1 "
            "ORDER BY s.last_used DESC LIMIT 50");
        if (!stmt.isPrepared()) { return rows; }
        std::string pattern = "%";
        pattern += query;
        pattern += "%";
        if (!stmt.bind(1, pattern.c_str())) { return rows; }
        while (stmt.stepRow()) {
            StickerRow row;
            row.id = stmt.columnText(0);
            row.pack_id = stmt.columnText(1);
            row.file_path = stmt.columnText(2);
            row.emoji = stmt.columnText(3);
            row.width = stmt.columnInt(4);
            row.height = stmt.columnInt(5);
            row.size = stmt.columnInt(6);
            row.last_used = stmt.columnInt64(7);
            row.position = stmt.columnInt(8);
            rows.push_back(std::move(row));
        }
        return rows;
    }

    int count_stickers(const char* pack_id) override {
        auto _ = m_conn->get();
        if (pack_id) {
            auto stmt = _->prepare(
                "SELECT COUNT(*) FROM stickers WHERE pack_id=?1");
            if (!stmt.isPrepared()) { return 0; }
            if (!stmt.bind(1, pack_id)) { return 0; }
            if (!stmt.stepRow()) { return 0; }
            return stmt.columnInt(0);
        } else {
            auto stmt = _->prepare("SELECT COUNT(*) FROM stickers");
            if (!stmt.isPrepared()) { return 0; }
            if (!stmt.stepRow()) { return 0; }
            return stmt.columnInt(0);
        }
    }

    bool begin_write_transaction() override {
        auto _ = m_conn->get();
        return _->exec("BEGIN IMMEDIATE");
    }

    bool commit_transaction() override {
        auto _ = m_conn->get();
        return _->exec("COMMIT");
    }
};

class StickerDbSyncSafe final : public StickerDbSyncSafeInterface {
    std::shared_ptr<SqliteConnectionSafe> m_conn;
    StickerDbSync m_sync;
public:
    explicit StickerDbSyncSafe(std::shared_ptr<SqliteConnectionSafe> conn)
        : m_conn(std::move(conn)), m_sync(m_conn) {}
    StickerDbSyncInterface& get() override { return m_sync; }
};

} // anonymous namespace

std::shared_ptr<StickerDbSyncSafeInterface> create_sticker_db(
    std::shared_ptr<SqliteConnectionSafe> conn) {
    return std::make_shared<StickerDbSyncSafe>(std::move(conn));
}

bool init_sticker_db(SqliteDb& db) {
    if (!db.exec(
        "CREATE TABLE IF NOT EXISTS sticker_packs ("
        "  id             TEXT PRIMARY KEY,"
        "  title          TEXT NOT NULL,"
        "  author         TEXT DEFAULT '',"
        "  cover_path     TEXT DEFAULT '',"
        "  installed      INTEGER DEFAULT 1,"
        "  position       INTEGER DEFAULT 0,"
        "  created_at     INTEGER DEFAULT (strftime('%s','now'))"
        ")")) { return false; }
    if (!db.exec(
        "CREATE TABLE IF NOT EXISTS stickers ("
        "  id             TEXT PRIMARY KEY,"
        "  pack_id        TEXT NOT NULL REFERENCES sticker_packs(id) ON DELETE CASCADE,"
        "  file_path      TEXT NOT NULL,"
        "  emoji          TEXT DEFAULT '',"
        "  width          INTEGER DEFAULT 0,"
        "  height         INTEGER DEFAULT 0,"
        "  size           INTEGER DEFAULT 0,"
        "  last_used      INTEGER DEFAULT 0,"
        "  position       INTEGER DEFAULT 0"
        ")")) { return false; }
    db.exec("CREATE INDEX IF NOT EXISTS idx_stickers_pack ON stickers(pack_id)");
    db.exec("CREATE INDEX IF NOT EXISTS idx_stickers_recent ON stickers(last_used DESC)");
    db.exec("PRAGMA foreign_keys = ON");
    return true;
}

bool drop_sticker_db(SqliteDb& db) {
    if (!db.exec("DROP TABLE IF EXISTS stickers")) { return false; }
    if (!db.exec("DROP TABLE IF EXISTS sticker_packs")) { return false; }
    return true;
}
