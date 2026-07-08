#include "translate_util.h"
#include <stdint.h>

bool isUnicodeLetter(uint32_t cp) {
    return (cp >= 0x41 && cp <= 0x5A) ||
           (cp >= 0x61 && cp <= 0x7A) ||
           (cp >= 0xC0 && cp <= 0x24FF) ||
           (cp >= 0x2C00 && cp <= 0x2FEF) ||
           (cp >= 0x3040 && cp <= 0x309F) ||  // Hiragana
           (cp >= 0x30A0 && cp <= 0x30FF) ||  // Katakana
           (cp >= 0x3400 && cp <= 0x9FFF) ||  // CJK + Ext A
           (cp >= 0xAC00 && cp <= 0xD7AF) ||  // Hangul
           (cp >= 0xF900 && cp <= 0xFAFF) ||  // CJK Compat
           (cp >= 0x20000 && cp <= 0x2FA1F);   // CJK Ext B-I
}

bool isMostlyChinese(const QString& text) {
    int chineseCount = 0;
    int totalCount = 0;
    for (int i = 0; i < text.length(); ++i) {
        uint32_t cp = text[i].unicode();
        // Decode surrogate pair for CJK Extension B+ (U+20000+)
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < text.length()) {
            uint32_t low = text[i + 1].unicode();
            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            ++i;
        }
        if (isUnicodeLetter(cp)) {
            totalCount++;
            if ((cp >= 0x4E00 && cp <= 0x9FFF) ||
                (cp >= 0x3400 && cp <= 0x4DBF) ||
                (cp >= 0x20000 && cp <= 0x2A6DF) ||
                (cp >= 0xF900 && cp <= 0xFAFF)) {
                chineseCount++;
            }
            // 含平假名/片假名 → 日文 → 不是中文
            // 含谚文 → 韩文 → 不是中文
            if ((cp >= 0x3040 && cp <= 0x30FF) ||
                (cp >= 0xAC00 && cp <= 0xD7AF)) {
                return false;
            }
        }
    }
    return totalCount > 0 && (float)chineseCount / totalCount > 0.3f;
}

bool needsAutoTranslate(const QString& text, const QString& targetLang) {
    if (targetLang.startsWith("zh")) { return !isMostlyChinese(text); }
    return true;
}
