#include <unity.h>
#include "util/color.h"

using namespace mclite;

void test_hash_prefixed() {
    uint32_t v = 0;
    TEST_ASSERT_TRUE(parseHexRGB("#1A2B3C", v));
    TEST_ASSERT_EQUAL_HEX32(0x1A2B3C, v);
}

void test_no_hash() {
    uint32_t v = 0;
    TEST_ASSERT_TRUE(parseHexRGB("FF8C00", v));
    TEST_ASSERT_EQUAL_HEX32(0xFF8C00, v);
}

void test_lowercase() {
    uint32_t v = 0;
    TEST_ASSERT_TRUE(parseHexRGB("#00cc66", v));
    TEST_ASSERT_EQUAL_HEX32(0x00CC66, v);
}

void test_black_white() {
    uint32_t v = 1;
    TEST_ASSERT_TRUE(parseHexRGB("#000000", v));
    TEST_ASSERT_EQUAL_HEX32(0x000000, v);
    TEST_ASSERT_TRUE(parseHexRGB("FFFFFF", v));
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, v);
}

void test_too_short() {
    uint32_t v = 0xABCDEF;
    TEST_ASSERT_FALSE(parseHexRGB("#123", v));
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF, v);   // out untouched on failure
}

void test_too_long() {
    uint32_t v = 0;
    TEST_ASSERT_FALSE(parseHexRGB("1234567", v));
    TEST_ASSERT_FALSE(parseHexRGB("#1234567", v));
}

void test_non_hex() {
    uint32_t v = 0;
    TEST_ASSERT_FALSE(parseHexRGB("#12G45A", v));
    TEST_ASSERT_FALSE(parseHexRGB("zzzzzz", v));
}

void test_empty_and_null() {
    uint32_t v = 0;
    TEST_ASSERT_FALSE(parseHexRGB("", v));
    TEST_ASSERT_FALSE(parseHexRGB("#", v));
    TEST_ASSERT_FALSE(parseHexRGB(nullptr, v));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_hash_prefixed);
    RUN_TEST(test_no_hash);
    RUN_TEST(test_lowercase);
    RUN_TEST(test_black_white);
    RUN_TEST(test_too_short);
    RUN_TEST(test_too_long);
    RUN_TEST(test_non_hex);
    RUN_TEST(test_empty_and_null);
    return UNITY_END();
}
