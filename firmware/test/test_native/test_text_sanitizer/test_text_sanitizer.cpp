#include <unity.h>
#include "util/TextSanitizer.h"

using namespace mclite;

void setUp() {}
void tearDown() {}

// Plain ASCII passes through untouched.
void test_plain_ascii_unchanged() {
    TEST_ASSERT_EQUAL_STRING("hello world", sanitizeForDisplay(String("hello world")).c_str());
}

// U+FE0F (variation selector, EF B8 8F) is stripped.
void test_strips_variation_selector() {
    // "A" + VS16 + "B"
    TEST_ASSERT_EQUAL_STRING("AB", sanitizeForDisplay(String("A\xEF\xB8\x8F" "B")).c_str());
}

// Emoji + trailing VS16 keeps the emoji, drops the selector.
void test_emoji_with_vs16() {
    // ❤ (E2 9D A4) + VS16 (EF B8 8F)  ->  ❤
    TEST_ASSERT_EQUAL_STRING("\xE2\x9D\xA4", sanitizeForDisplay(String("\xE2\x9D\xA4\xEF\xB8\x8F")).c_str());
}

// Typographic quotes normalize to ASCII.
void test_normalizes_quotes() {
    // “hi” -> "hi"   (U+201C E2 80 9C / U+201D E2 80 9D)
    TEST_ASSERT_EQUAL_STRING("\"hi\"", sanitizeForDisplay(String("\xE2\x80\x9C" "hi" "\xE2\x80\x9D")).c_str());
    // ’ -> '   (U+2019 E2 80 99)
    TEST_ASSERT_EQUAL_STRING("it's", sanitizeForDisplay(String("it\xE2\x80\x99s")).c_str());
}

// A 4-byte emoji (not a quote/VS) is preserved verbatim.
void test_preserves_4byte_emoji() {
    // 🙂 = F0 9F 99 82
    TEST_ASSERT_EQUAL_STRING("hi \xF0\x9F\x99\x82", sanitizeForDisplay(String("hi \xF0\x9F\x99\x82")).c_str());
}

// Empty string is fine.
void test_empty() {
    TEST_ASSERT_EQUAL_STRING("", sanitizeForDisplay(String("")).c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_plain_ascii_unchanged);
    RUN_TEST(test_strips_variation_selector);
    RUN_TEST(test_emoji_with_vs16);
    RUN_TEST(test_normalizes_quotes);
    RUN_TEST(test_preserves_4byte_emoji);
    RUN_TEST(test_empty);
    return UNITY_END();
}
