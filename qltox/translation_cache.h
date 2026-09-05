#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <utility>

// 翻译缓存：按键 (message_rowid, target_lang) 读写缓存结果。
// 开关通过切换空实现（NullTranslationCache）实现，默认开启（DbTranslationCache）。
//
// 并发说明：lookup/save 仅在主线程被调用（翻译触发与结果回调都在主线程），
// 因此内部去重集合无需加锁。
class TranslationCache {
public:
    virtual ~TranslationCache() {}

    // 同步查询：命中返回 true 并填 outText。rowid<=0 / lang 为空一律 miss。
    virtual bool lookup(int64_t rowid, const std::string& lang, std::string& outText) = 0;
    // 异步写入（fire-and-forget）：重复键自动去重（会话内集合 + DB 层 INSERT OR REPLACE）。
    virtual void save(int64_t rowid, const std::string& lang, const std::string& text) = 0;
};

// 空实现：关闭时的落点，lookup 恒 miss、save 空操作。
class NullTranslationCache : public TranslationCache {
public:
    bool lookup(int64_t, const std::string&, std::string&) override { return false; }
    void save(int64_t, const std::string&, const std::string&) override {}
};

// 缓存开关。默认开启（true）。
bool translationCacheEnabled();
void setTranslationCacheEnabled(bool on);
// 返回当前生效的实现：开启 → DbTranslationCache（懒创建单例），关闭 → NullTranslationCache。
TranslationCache* translationCache();