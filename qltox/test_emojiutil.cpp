#include "doctest.h"
#include "../qlcomp/emojiutil.cpp"

TEST_CASE("isEmojiChar - 基础分类") {
    SUBCASE("emoji characters") {
        CHECK(isEmojiChar(0x1F600) == true);   // 😀
        CHECK(isEmojiChar(0x1F4A9) == true);   // 💩
        CHECK(isEmojiChar(0x1F44D) == true);   // 👍
    }

    SUBCASE("non-emoji characters") {
        CHECK(isEmojiChar(0x0041) == false);   // A
        CHECK(isEmojiChar(0x4E00) == false);   // 中
        CHECK(isEmojiChar(0x3041) == false);   // あ
    }
}

TEST_CASE("isEmojiChar - 边界值") {
    SUBCASE("1F100 range start") {
        CHECK(isEmojiChar(0x1F0FF) == false);
        CHECK(isEmojiChar(0x1F100) == true);
    }

    SUBCASE("1FAFF range end") {
        CHECK(isEmojiChar(0x1FAFF) == true);
        CHECK(isEmojiChar(0x1FB00) == false);
    }
}

TEST_CASE("isEmojiChar - 特殊符号") {
    CHECK(isEmojiChar(0x00A9) == true);    // ©
    CHECK(isEmojiChar(0x00AE) == true);    // ®
    CHECK(isEmojiChar(0x203C) == true);    // ‼
    CHECK(isEmojiChar(0x2122) == true);    // ™
    CHECK(isEmojiChar(0x231A) == true);    // ⌚
    CHECK(isEmojiChar(0x2B50) == true);    // ⭐
    CHECK(isEmojiChar(0x3297) == true);    // ㊗
}

TEST_CASE("toCodepoints - 基本多文种面 surrogate pair") {
    // 😀 = U+1F600 = surrogate pair D83D DE00
    QString s;
    s += QChar((ushort)0xD83D);
    s += QChar((ushort)0xDE00);
    std::vector<uint32_t> cps = toCodepoints(s);
    CHECK(cps.size() == 1u);
    CHECK(cps[0] == 0x1F600);
}

TEST_CASE("toCodepoints - BMP字符") {
    QString s = "ABC";
    std::vector<uint32_t> cps = toCodepoints(s);
    CHECK(cps.size() == 3u);
    CHECK(cps[0] == 0x41);
    CHECK(cps[1] == 0x42);
    CHECK(cps[2] == 0x43);
}

TEST_CASE("toCodepoints - 混合文本") {
    // "A😀B" = A(41) + 😀(1F600) + B(42)
    QString s = "A";
    s += QChar((ushort)0xD83D);
    s += QChar((ushort)0xDE00);
    s += "B";
    std::vector<uint32_t> cps = toCodepoints(s);
    CHECK(cps.size() == 3u);
    CHECK(cps[0] == 0x41);
    CHECK(cps[1] == 0x1F600);
    CHECK(cps[2] == 0x42);
}

TEST_CASE("textHasEmoji - 混合文本") {
    CHECK(textHasEmoji("hello") == false);
    CHECK(textHasEmoji(QString::fromUtf8("hello 😀")) == true);
    QString s = "hi";
    s += QChar((ushort)0xD83D);
    s += QChar((ushort)0xDE00);
    CHECK(textHasEmoji(s) == true);
}

TEST_CASE("textHasEmoji - 纯文本无emoji") {
    CHECK(textHasEmoji("你好世界") == false);
    CHECK(textHasEmoji("12345") == false);
    CHECK(textHasEmoji("") == false);
}
