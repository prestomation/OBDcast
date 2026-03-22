#include <unity.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// We need to control "time" in tests. We subclass PowerManager and override
// now(). Include the implementation directly to get full access.
// ---------------------------------------------------------------------------

// NATIVE_BUILD is already defined by the native env build_flags
#ifndef NATIVE_BUILD
  #define NATIVE_BUILD
#endif
#include "../../src/power/PowerManager.h"
#include "../../src/power/PowerManager.cpp"

// ---------------------------------------------------------------------------
// Controllable-time PowerManager for tests
// ---------------------------------------------------------------------------
static uint32_t g_fakeMs = 0;

class TestPowerManager : public PowerManager {
public:
    using PowerManager::PowerManager;
    uint32_t now() const override { return g_fakeMs; }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void setUp(void) {
    g_fakeMs = 0;
}

void tearDown(void) {}

// --- ACTIVE → STANDBY ---
void test_active_to_standby_on_low_voltage(void) {
    TestPowerManager pm(13.2f, 12.2f, 900000, 3600000);

    // Initially ACTIVE
    TEST_ASSERT_EQUAL(PowerState::ACTIVE, pm.getState());

    // Voltage drops below active threshold but above standby threshold
    PowerState s = pm.update(12.5f, false);
    TEST_ASSERT_EQUAL(PowerState::STANDBY, s);
}

// --- STANDBY → ACTIVE on voltage recovery ---
void test_standby_to_active_on_voltage_recovery(void) {
    TestPowerManager pm(13.2f, 12.2f, 900000, 3600000);

    pm.update(12.5f, false); // → STANDBY
    TEST_ASSERT_EQUAL(PowerState::STANDBY, pm.getState());

    PowerState s = pm.update(13.5f, false); // → ACTIVE
    TEST_ASSERT_EQUAL(PowerState::ACTIVE, s);
}

// --- STANDBY → ACTIVE on motion ---
void test_standby_to_active_on_motion(void) {
    TestPowerManager pm(13.2f, 12.2f, 900000, 3600000);

    pm.update(12.5f, false); // → STANDBY
    TEST_ASSERT_EQUAL(PowerState::STANDBY, pm.getState());

    PowerState s = pm.update(12.5f, true); // motion detected → ACTIVE
    TEST_ASSERT_EQUAL(PowerState::ACTIVE, s);
}

// --- STANDBY → DEEP_SLEEP on critical voltage ---
void test_standby_to_deep_sleep_on_critical_voltage(void) {
    TestPowerManager pm(13.2f, 12.2f, 900000, 3600000);

    pm.update(12.5f, false); // → STANDBY
    PowerState s = pm.update(12.0f, false); // → DEEP_SLEEP (below standby threshold)
    TEST_ASSERT_EQUAL(PowerState::DEEP_SLEEP, s);
}

// --- STANDBY → DEEP_SLEEP on idle timeout ---
void test_standby_to_deep_sleep_on_idle_timeout(void) {
    TestPowerManager pm(13.2f, 12.2f, 900000, 3600000UL);

    pm.update(12.5f, false); // → STANDBY at t=0

    // Advance time past idle timeout (1 hour)
    g_fakeMs = 3600001UL;
    PowerState s = pm.update(12.5f, false); // should → DEEP_SLEEP
    TEST_ASSERT_EQUAL(PowerState::DEEP_SLEEP, s);
}

// --- ACTIVE → DEEP_SLEEP directly on critical voltage ---
void test_active_to_deep_sleep_on_critical_voltage(void) {
    TestPowerManager pm(13.2f, 12.2f, 900000, 3600000);

    TEST_ASSERT_EQUAL(PowerState::ACTIVE, pm.getState());

    // Voltage drops below STANDBY threshold directly (skip standby)
    PowerState s = pm.update(12.0f, false);
    TEST_ASSERT_EQUAL(PowerState::DEEP_SLEEP, s);
}

// --- DEEP_SLEEP → ACTIVE via onWake ---
void test_wake_from_deep_sleep(void) {
    TestPowerManager pm(13.2f, 12.2f, 900000, 3600000);

    pm.enterDeepSleep();
    TEST_ASSERT_EQUAL(PowerState::DEEP_SLEEP, pm.getState());

    pm.onWake();
    // After wake with good voltage, stays ACTIVE
    PowerState s = pm.update(13.5f, false);
    TEST_ASSERT_EQUAL(PowerState::ACTIVE, s);
}

// --- shouldSendStandbyPing ---
void test_standby_ping_timing(void) {
    TestPowerManager pm(13.2f, 12.2f, 5000UL, 3600000UL); // 5s ping interval for test

    pm.update(12.5f, false); // → STANDBY

    // Immediately after entering standby, no ping yet
    TEST_ASSERT_FALSE(pm.shouldSendStandbyPing());

    // Advance time past ping interval
    g_fakeMs = 5001UL;
    TEST_ASSERT_TRUE(pm.shouldSendStandbyPing());

    // Should not fire again immediately
    TEST_ASSERT_FALSE(pm.shouldSendStandbyPing());
}

// --- shouldCollect ---
void test_should_collect_timing(void) {
    TestPowerManager pm(13.2f, 12.2f, 900000, 3600000);

    TEST_ASSERT_EQUAL(PowerState::ACTIVE, pm.getState());

    // First call should collect immediately (sentinel value)
    g_fakeMs = 0;
    TEST_ASSERT_TRUE(pm.shouldCollect(10000UL));
    // After first collect, _lastCollectMs is set to now() (0)
    TEST_ASSERT_FALSE(pm.shouldCollect(10000UL)); // too soon (elapsed=0)

    // Advance time past interval
    g_fakeMs = 10001UL;
    TEST_ASSERT_TRUE(pm.shouldCollect(10000UL));

    // Should not fire again immediately
    TEST_ASSERT_FALSE(pm.shouldCollect(10000UL));
}

// --- shouldCollect returns false in STANDBY ---
void test_should_collect_false_in_standby(void) {
    TestPowerManager pm(13.2f, 12.2f, 900000, 3600000);

    pm.update(12.5f, false); // → STANDBY
    TEST_ASSERT_FALSE(pm.shouldCollect(10000UL));
}

// --- stateName ---
void test_state_names(void) {
    TEST_ASSERT_EQUAL_STRING("ACTIVE",     PowerManager::stateName(PowerState::ACTIVE));
    TEST_ASSERT_EQUAL_STRING("STANDBY",    PowerManager::stateName(PowerState::STANDBY));
    TEST_ASSERT_EQUAL_STRING("DEEP_SLEEP", PowerManager::stateName(PowerState::DEEP_SLEEP));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_active_to_standby_on_low_voltage);
    RUN_TEST(test_standby_to_active_on_voltage_recovery);
    RUN_TEST(test_standby_to_active_on_motion);
    RUN_TEST(test_standby_to_deep_sleep_on_critical_voltage);
    RUN_TEST(test_standby_to_deep_sleep_on_idle_timeout);
    RUN_TEST(test_active_to_deep_sleep_on_critical_voltage);
    RUN_TEST(test_wake_from_deep_sleep);
    RUN_TEST(test_standby_ping_timing);
    RUN_TEST(test_should_collect_timing);
    RUN_TEST(test_should_collect_false_in_standby);
    RUN_TEST(test_state_names);

    return UNITY_END();
}
