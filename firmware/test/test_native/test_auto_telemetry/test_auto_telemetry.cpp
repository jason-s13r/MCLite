#include <unity.h>
#include "util/AutoTelemetry.h"

using namespace mclite;

// Refresh threshold used across the table tests (mirrors AUTO_TELEM_REFRESH_AGE_MS).
static constexpr uint32_t REFRESH = 1500000;  // 25 min

void setUp() {}
void tearDown() {}

// They broadcast location → never request (we already have it for free).
void test_advertiser_not_due() {
    TEST_ASSERT_FALSE(autoTelemetryDue(/*advertisesLoc*/true, /*gaveUp*/false,
                                       /*hasTelemGps*/false, /*ageMs*/0, REFRESH));
    // …even if we hold an old telemetry fix.
    TEST_ASSERT_FALSE(autoTelemetryDue(true, false, true, REFRESH + 1, REFRESH));
}

// Backed off this session → never request.
void test_gave_up_not_due() {
    TEST_ASSERT_FALSE(autoTelemetryDue(false, /*gaveUp*/true, false, 0, REFRESH));
    TEST_ASSERT_FALSE(autoTelemetryDue(false, true, true, REFRESH + 1, REFRESH));
}

// No location at all and not backed off → request (seed a first fix).
void test_no_location_due() {
    TEST_ASSERT_TRUE(autoTelemetryDue(false, false, /*hasTelemGps*/false, 0, REFRESH));
}

// Fresh telemetry fix → not yet due.
void test_fresh_telemetry_not_due() {
    TEST_ASSERT_FALSE(autoTelemetryDue(false, false, /*hasTelemGps*/true,
                                       /*ageMs*/REFRESH - 1, REFRESH));
}

// Telemetry fix at/over the refresh age → due (refresh before it goes stale).
void test_aging_telemetry_due() {
    TEST_ASSERT_TRUE(autoTelemetryDue(false, false, true, REFRESH, REFRESH));
    TEST_ASSERT_TRUE(autoTelemetryDue(false, false, true, REFRESH + 60000, REFRESH));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_advertiser_not_due);
    RUN_TEST(test_gave_up_not_due);
    RUN_TEST(test_no_location_due);
    RUN_TEST(test_fresh_telemetry_not_due);
    RUN_TEST(test_aging_telemetry_due);
    return UNITY_END();
}
