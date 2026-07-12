#pragma once
#include "storage.h"
#include <string>
#include <vector>
#include <memory>

struct StickerPackRow {
    std::string id;
    std::string title;
    std::string author;
    std::string cover_path;
    int installed = 1;
    int position = 0;
    int64_t created_at = 0;
};

struct StickerRow {
    std::string id;
    std::string pack_id;
    std::string file_path;
    std::string emoji;
    int width = 0;
    int height = 0;
    int size = 0;
    int64_t last_used = 0;
    int position = 0;
};

class StickerDbSyncInterface {
public:
    virtual ~StickerDbSyncInterface() = default;

    // pack operations
    virtual bool add_pack(const StickerPackRow& pack) = 0;
    virtual bool delete_pack(const char* pack_id) = 0;
    virtual bool update_pack_position(const char* pack_id, int position) = 0;
    virtual std::unique_ptr<StickerPackRow> get_pack(const char* pack_id) = 0;
    virtual std::vector<StickerPackRow> list_packs(int installed = 1) = 0;

    // sticker operations
    virtual bool add_sticker(const StickerRow& sticker) = 0;
    virtual bool delete_sticker(const char* sticker_id) = 0;
    virtual bool delete_stickers_by_pack(const char* pack_id) = 0;
    virtual bool touch_sticker(const char* sticker_id, int64_t now) = 0;
    virtual std::unique_ptr<StickerRow> get_sticker(const char* sticker_id) = 0;
    virtual std::vector<StickerRow> list_stickers(const char* pack_id) = 0;
    virtual std::vector<StickerRow> list_recent_stickers(int limit = 30) = 0;
    virtual std::vector<StickerRow> search_stickers(const char* query) = 0;
    virtual int count_stickers(const char* pack_id = nullptr) = 0;

    virtual bool begin_write_transaction() = 0;
    virtual bool commit_transaction() = 0;
};

class StickerDbSyncSafeInterface {
public:
    virtual ~StickerDbSyncSafeInterface() = default;
    virtual StickerDbSyncInterface& get() = 0;
};

std::shared_ptr<StickerDbSyncSafeInterface> create_sticker_db(
    std::shared_ptr<SqliteConnectionSafe> conn);

bool init_sticker_db(SqliteDb& db);
bool drop_sticker_db(SqliteDb& db);
