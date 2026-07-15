#include "doctest.h"
#include <string>
#include <cstdio>
#include <cstdint>

// 避免与 OpenSSL MD5 符号冲突：重命名项目 md5.c 的函数
#define MD5_Init    ql_md5_Init
#define MD5_Update  ql_md5_Update
#define MD5_Final   ql_md5_Final
#define MD5_CTX     ql_MD5_CTX
extern "C" {
#include "../qlcomp/md5.c"
}

static std::string digestToHex(const uint8_t d[16]) {
    char buf[33];
    for (int i = 0; i < 16; i++) {
        sprintf(buf + i * 2, "%02x", d[i]);
    }
    buf[32] = '\0';
    return std::string(buf);
}

TEST_CASE("MD5 - RFC 1321 test vectors") {
    uint8_t d[16];
    MD5_CTX ctx;

    SUBCASE("empty string") {
        ql_md5_Init(&ctx);
        ql_md5_Update(&ctx, (const uint8_t*)"", 0);
        ql_md5_Final(d, &ctx);
        CHECK(digestToHex(d) == "d41d8cd98f00b204e9800998ecf8427e");
    }

    SUBCASE("a") {
        ql_md5_Init(&ctx);
        ql_md5_Update(&ctx, (const uint8_t*)"a", 1);
        ql_md5_Final(d, &ctx);
        CHECK(digestToHex(d) == "0cc175b9c0f1b6a831c399e269772661");
    }

    SUBCASE("abc") {
        ql_md5_Init(&ctx);
        ql_md5_Update(&ctx, (const uint8_t*)"abc", 3);
        ql_md5_Final(d, &ctx);
        CHECK(digestToHex(d) == "900150983cd24fb0d6963f7d28e17f72");
    }

    SUBCASE("message digest") {
        ql_md5_Init(&ctx);
        ql_md5_Update(&ctx, (const uint8_t*)"message digest", 14);
        ql_md5_Final(d, &ctx);
        CHECK(digestToHex(d) == "f96b697d7cb7938d525a2f31aaf161d0");
    }

    SUBCASE("abcdefghijklmnopqrstuvwxyz") {
        ql_md5_Init(&ctx);
        ql_md5_Update(&ctx, (const uint8_t*)"abcdefghijklmnopqrstuvwxyz", 26);
        ql_md5_Final(d, &ctx);
        CHECK(digestToHex(d) == "c3fcd3d76192e4007dfb496cca67e13b");
    }
}

TEST_CASE("MD5 - incremental update") {
    uint8_t d[16];
    MD5_CTX ctx;

    ql_md5_Init(&ctx);
    ql_md5_Update(&ctx, (const uint8_t*)"a", 1);
    ql_md5_Update(&ctx, (const uint8_t*)"b", 1);
    ql_md5_Final(d, &ctx);
    CHECK(digestToHex(d) == "187ef4436122d1cc2f40dc2b92f0eba0");
}
