#ifndef QCRC64_H
#define QCRC64_H

// ── CRC-64/XZ (ECMA-182 reflected variant, 同 xz 压缩器) ──────────
// 核心为可增量计算的纯 C 函数，qt 包装见下。
//
// 网上核对（net-search-verify）：
//   width=64 poly=0x42f0e1eba9ea3693 init=0xffffffffffffffff
//   refin=true refout=true xorout=0xffffffffffffffff
//   check("123456789")=0x995dc9bbdf1939fa  (与 xz 官方 lzma_crc64 一致)

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
#include <qstring.h>
extern "C" {
#endif

typedef uint64_t qcrc64_t;

// 增量计算：crc 传 (qcrc64_t)0 的补满(全 1)起算，或上一次的返回值续传。
qcrc64_t qcrc64(const uint8_t* buf, size_t size, qcrc64_t crc);

#ifdef __cplusplus
} // extern "C"

// 对字符串(UTF-8 字节)计算整个 CRC-64/XZ。
uint64_t qCrc64Str(const QString& s);

// 对文件内容计算整个 CRC-64/XZ；打开失败返回 0。
uint64_t qCrc64File(const QString& path);
#endif

#endif  // QCRC64_H
