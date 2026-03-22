#include <unity.h>
#include <string.h>
#include <stdlib.h>

#ifndef NATIVE_BUILD
  #define NATIVE_BUILD
#endif
#include "../../src/Payload.h"

// ---------------------------------------------------------------------------
// config.h constants needed by Payload.h serializer
// ---------------------------------------------------------------------------
#ifndef DEVICE_ID
  #define DEVICE_ID "test-device"
#endif
#ifndef VOLTAGE_ACTIVE_THRESHOLD
  #define VOLTAGE_ACTIVE_THRESHOLD 13.2f
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool jsonContains(const char* json, const char* key, const char* value) {
    // Simple substring search for "key":value or "key":"value"
    char needle1[128], needle2[128];
    snprintf(needle1, sizeof(needle1), "\"%s\":\"%s\"", key, value);
    snprintf(needle2, sizeof(needle2), "\"%s\":%s",    key, value);
    return (strstr(json, needle1) != nullptr) ||
           (strstr(json, needle2) != nullptr);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void setUp(void) {}
void tearDown(void) {}

void test_payload_serializes_device_id(void) {
    Payload p;
    strncpy(p.device_id, "obdcast-test-001", sizeof(p.device_id) - 1);
    p.ts = 1742600000;

    char buf[1024];
    size_t len = p.toJson(buf, sizeof(buf));

    TEST_ASSERT_GREATER_THAN(0u, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "obdcast-test-001"));
}

void test_payload_serializes_timestamp(void) {
    Payload p;
    strncpy(p.device_id, "dev", sizeof(p.device_id) - 1);
    p.ts = 1742600000;

    char buf[1024];
    p.toJson(buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "1742600000"));
}

void test_payload_serializes_ignition_true(void) {
    Payload p;
    strncpy(p.device_id, "dev", sizeof(p.device_id) - 1);
    p.ignition = true;

    char buf[1024];
    p.toJson(buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "\"ignition\":true"));
}

void test_payload_serializes_ignition_false(void) {
    Payload p;
    strncpy(p.device_id, "dev", sizeof(p.device_id) - 1);
    p.ignition = false;

    char buf[1024];
    p.toJson(buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "\"ignition\":false"));
}

void test_payload_has_obd_section(void) {
    Payload p;
    strncpy(p.device_id, "dev", sizeof(p.device_id) - 1);
    p.obd.voltage = 13.8f;

    char buf[1024];
    size_t len = p.toJson(buf, sizeof(buf));

    TEST_ASSERT_GREATER_THAN(0u, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"obd\":{"));
}

void test_payload_has_gps_section(void) {
    Payload p;
    strncpy(p.device_id, "dev", sizeof(p.device_id) - 1);
    p.gps.fix = true;

    char buf[1024];
    p.toJson(buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "\"gps\":{"));
}

void test_payload_has_motion_section(void) {
    Payload p;
    strncpy(p.device_id, "dev", sizeof(p.device_id) - 1);
    p.motion.ax = 0.01f;
    p.motion.ay = -0.02f;
    p.motion.az = 9.81f;

    char buf[1024];
    p.toJson(buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "\"motion\":{"));
}

void test_payload_is_valid_json_structure(void) {
    Payload p;
    strncpy(p.device_id, "obdcast-001", sizeof(p.device_id) - 1);
    p.ts         = 1742600000;
    p.ignition   = true;
    p.signal_dbm = -87;
    p.obd.speed        = 65;
    p.obd.rpm          = 2100;
    p.obd.fuel_pct     = 72;
    p.obd.coolant_c    = 88;
    p.obd.engine_load  = 34;
    p.obd.throttle_pct = 18;
    p.obd.voltage      = 13.8f;
    p.gps.lat      = 47.6062f;
    p.gps.lon      = -122.3321f;
    p.gps.alt_m    = 52.f;
    p.gps.heading  = 180.f;
    p.gps.speed_kph= 65.f;
    p.gps.hdop     = 1.2f;
    p.gps.fix      = true;
    p.motion.ax    = 0.01f;
    p.motion.ay    = -0.02f;
    p.motion.az    = 9.81f;

    char buf[1024];
    size_t len = p.toJson(buf, sizeof(buf));

    TEST_ASSERT_GREATER_THAN(0u, len);

    // Basic JSON structure checks
    TEST_ASSERT_NOT_NULL(strstr(buf, "{"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "}"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "device_id"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "obdcast-001"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "obd"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "gps"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "motion"));
}

void test_payload_buffer_too_small_returns_zero(void) {
    Payload p;
    strncpy(p.device_id, "dev", sizeof(p.device_id) - 1);

    char tinyBuf[4];
    size_t len = p.toJson(tinyBuf, sizeof(tinyBuf));
    // Either 0 (failure) or truncated — ArduinoJson returns 0 when buf is too small
    TEST_ASSERT_EQUAL(0u, len);
}

void test_payload_default_obd_voltage_is_zero(void) {
    Payload p;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, p.obd.voltage);
}

void test_payload_default_gps_fix_is_false(void) {
    Payload p;
    TEST_ASSERT_FALSE(p.gps.fix);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_payload_serializes_device_id);
    RUN_TEST(test_payload_serializes_timestamp);
    RUN_TEST(test_payload_serializes_ignition_true);
    RUN_TEST(test_payload_serializes_ignition_false);
    RUN_TEST(test_payload_has_obd_section);
    RUN_TEST(test_payload_has_gps_section);
    RUN_TEST(test_payload_has_motion_section);
    RUN_TEST(test_payload_is_valid_json_structure);
    RUN_TEST(test_payload_buffer_too_small_returns_zero);
    RUN_TEST(test_payload_default_obd_voltage_is_zero);
    RUN_TEST(test_payload_default_gps_fix_is_false);

    return UNITY_END();
}
