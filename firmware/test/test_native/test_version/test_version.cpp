#include <unity.h>
#include "util/version.h"

using namespace mclite;

void test_equal() {
    TEST_ASSERT_EQUAL_INT(0, compareVersions("0.2.0", "0.2.0"));
}

void test_newer_patch() {
    TEST_ASSERT_EQUAL_INT(1, compareVersions("0.2.1", "0.2.0"));
    TEST_ASSERT_EQUAL_INT(-1, compareVersions("0.2.0", "0.2.1"));
}

void test_newer_minor() {
    TEST_ASSERT_EQUAL_INT(1, compareVersions("0.3.0", "0.2.9"));
    TEST_ASSERT_EQUAL_INT(-1, compareVersions("0.2.9", "0.3.0"));
}

void test_newer_major() {
    TEST_ASSERT_EQUAL_INT(1, compareVersions("1.0.0", "0.99.99"));
    TEST_ASSERT_EQUAL_INT(-1, compareVersions("0.99.99", "1.0.0"));
}

void test_leading_v() {
    TEST_ASSERT_EQUAL_INT(0, compareVersions("v0.2.0", "0.2.0"));
    TEST_ASSERT_EQUAL_INT(1, compareVersions("v0.2.1", "v0.2.0"));
}

void test_missing_components() {
    // "1" == "1.0.0", "1.2" == "1.2.0"
    TEST_ASSERT_EQUAL_INT(0, compareVersions("1", "1.0.0"));
    TEST_ASSERT_EQUAL_INT(0, compareVersions("1.2", "1.2.0"));
    TEST_ASSERT_EQUAL_INT(1, compareVersions("1.2.1", "1.2"));
}

void test_multi_digit() {
    TEST_ASSERT_EQUAL_INT(1, compareVersions("0.2.10", "0.2.9"));
    TEST_ASSERT_EQUAL_INT(1, compareVersions("0.10.0", "0.9.0"));
}

void test_null_and_empty() {
    TEST_ASSERT_EQUAL_INT(0, compareVersions("", ""));
    TEST_ASSERT_EQUAL_INT(0, compareVersions(nullptr, "0.0.0"));
    TEST_ASSERT_EQUAL_INT(-1, compareVersions("", "0.0.1"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_equal);
    RUN_TEST(test_newer_patch);
    RUN_TEST(test_newer_minor);
    RUN_TEST(test_newer_major);
    RUN_TEST(test_leading_v);
    RUN_TEST(test_missing_components);
    RUN_TEST(test_multi_digit);
    RUN_TEST(test_null_and_empty);
    return UNITY_END();
}
