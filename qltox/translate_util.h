#ifndef TRANSLATE_UTIL_H
#define TRANSLATE_UTIL_H

#include <qstring.h>
#include <stdint.h>

// 检查 UCS-4 codepoint 是否为 Unicode Letter 类别
// 替代 QChar::isLetter()，因为后者对 surrogate 返回 false
bool isUnicodeLetter(uint32_t cp);

// 判断文本是否主要为中文（用于 zh-CN 目标语言检测）
bool isMostlyChinese(const QString& text);

// 判断文本是否需要自动翻译（相对目标语言）
bool needsAutoTranslate(const QString& text, const QString& targetLang);

#endif
