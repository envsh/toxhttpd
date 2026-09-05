#include "translation_cache.h"
#include "storage.h"
#include "message_db.h"

namespace {

// 真实实现：查询走同步接口，写入走异步接口（POST 到 WriteQueue，不阻塞主线程）。
class DbTranslationCache : public TranslationCache {
public:
    bool lookup(int64_t rowid, const std::string& lang, std::string& outText) override {
        if (rowid <= 0 || lang.empty()) { return false; }
        auto* db = Storage::instance().messageDb();
        if (!db) { return false; }
        auto row = db->get_translation(rowid, lang.c_str());
        if (!row || row->translated_text.empty()) { return false; }
        outText = row->translated_text;
        return true;
    }

    void save(int64_t rowid, const std::string& lang, const std::string& text) override {
        if (rowid <= 0 || lang.empty() || text.empty()) { return; }
        // 会话内去重：同一 (rowid, lang) 只写一次，避免重复触发 WriteQueue 写。
        // DB 层 INSERT OR REPLACE 兜底跨会话重复。
        if (!m_written.insert(std::make_pair(rowid, lang)).second) { return; }

        auto* db = Storage::instance().messageDbAsync();
        if (!db) { return; }
        TranslationRow row;
        row.message_rowid = rowid;
        row.target_lang = lang;
        row.translated_text = text;
        row.translated_entities.clear();
        row.source_lang.clear();
        row.provider = "cached";
        db->set_translation(row, [rowid, lang](bool ok) {
            if (!ok) {
                qWarning("translation_cache: save rowid=%lld lang=%s failed",
                         (long long)rowid, lang.c_str());
            }
        });
    }

private:
    std::set<std::pair<int64_t, std::string> > m_written;
};

NullTranslationCache g_nullCache;
DbTranslationCache g_dbCache;
bool g_enabled = true;

} // namespace

bool translationCacheEnabled() {
    return g_enabled;
}

void setTranslationCacheEnabled(bool on) {
    g_enabled = on;
}

TranslationCache* translationCache() {
    return g_enabled ? static_cast<TranslationCache*>(&g_dbCache)
                     : static_cast<TranslationCache*>(&g_nullCache);
}