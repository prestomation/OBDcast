#include <unity.h>
#include <string.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Transport selection logic tests
//
// These tests verify the ConnectivityManager / transport selection rules
// without any actual hardware. We test the decision logic directly.
// ---------------------------------------------------------------------------

#ifndef NATIVE_BUILD
  #define NATIVE_BUILD
#endif

// ---------------------------------------------------------------------------
// Config defaults for tests
// ---------------------------------------------------------------------------
#ifndef DEVICE_ID
  #define DEVICE_ID "test-device"
#endif
#ifndef VOLTAGE_ACTIVE_THRESHOLD
  #define VOLTAGE_ACTIVE_THRESHOLD 13.2f
#endif
#ifndef TRANSPORT_MQTT
  #define TRANSPORT_MQTT    1
#endif
#ifndef TRANSPORT_WEBHOOK
  #define TRANSPORT_WEBHOOK 2
#endif
#ifndef TRANSPORT_MODE
  #define TRANSPORT_MODE TRANSPORT_MQTT
#endif
#ifndef MQTT_BROKER
  #define MQTT_BROKER "mqtt.test.local"
#endif
#ifndef MQTT_PORT
  #define MQTT_PORT 1883
#endif
#ifndef MQTT_TOPIC_PREFIX
  #define MQTT_TOPIC_PREFIX "obdcast"
#endif
#ifndef MQTT_USER
  #define MQTT_USER ""
#endif
#ifndef MQTT_PASS
  #define MQTT_PASS ""
#endif
#ifndef MQTT_USE_TLS
  #define MQTT_USE_TLS false
#endif
#ifndef MQTT_QOS
  #define MQTT_QOS 0
#endif
#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD ""
#endif
#ifndef WEBHOOK_URL
  #define WEBHOOK_URL "https://example.com/webhook"
#endif
#ifndef WEBHOOK_AUTH_TOKEN
  #define WEBHOOK_AUTH_TOKEN ""
#endif
#ifndef WEBHOOK_TIMEOUT_MS
  #define WEBHOOK_TIMEOUT_MS 10000
#endif

// ---------------------------------------------------------------------------
// ConnPath enum (mirrors ConnectivityManager.h without pulling in hardware)
// ---------------------------------------------------------------------------
enum class ConnPath {
    NONE,
    WIFI,
    CELLULAR,
};

// ---------------------------------------------------------------------------
// Transport type enum
// ---------------------------------------------------------------------------
enum class TransportType {
    MQTT,
    WEBHOOK,
};

// ---------------------------------------------------------------------------
// Simulated connection state (stands in for ConnectivityManager internals)
// ---------------------------------------------------------------------------
struct MockConnState {
    bool wifiAvailable;
    bool cellularAvailable;
    const char* configuredSsid; // WIFI_SSID equivalent
};

// ---------------------------------------------------------------------------
// Pure selection logic function (extracted from main.cpp createTransport)
// This is the function we're actually testing.
// ---------------------------------------------------------------------------
ConnPath selectPath(const MockConnState& s) {
    // WiFi first if SSID configured and WiFi available
    if (s.configuredSsid && s.configuredSsid[0] != '\0' && s.wifiAvailable) {
        return ConnPath::WIFI;
    }
    // Cellular fallback
    if (s.cellularAvailable) {
        return ConnPath::CELLULAR;
    }
    return ConnPath::NONE;
}

TransportType selectTransportType(int configuredMode) {
    if (configuredMode == TRANSPORT_WEBHOOK) return TransportType::WEBHOOK;
    return TransportType::MQTT;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void setUp(void) {}
void tearDown(void) {}

// --- WiFi preferred when available and SSID configured ---
void test_wifi_selected_when_available_and_ssid_configured(void) {
    MockConnState s{ true, true, "MyHomeWiFi" };
    TEST_ASSERT_EQUAL(ConnPath::WIFI, selectPath(s));
}

// --- Cellular fallback when WiFi fails ---
void test_cellular_fallback_when_wifi_fails(void) {
    MockConnState s{ false, true, "MyHomeWiFi" };
    TEST_ASSERT_EQUAL(ConnPath::CELLULAR, selectPath(s));
}

// --- Cellular-only when no SSID configured ---
void test_cellular_only_when_no_ssid_configured(void) {
    MockConnState s{ true, true, "" };
    TEST_ASSERT_EQUAL(ConnPath::CELLULAR, selectPath(s));
}

// --- NONE when both unavailable ---
void test_none_when_both_unavailable(void) {
    MockConnState s{ false, false, "MyHomeWiFi" };
    TEST_ASSERT_EQUAL(ConnPath::NONE, selectPath(s));
}

// --- NONE when no SSID and no cellular ---
void test_none_when_no_ssid_and_no_cellular(void) {
    MockConnState s{ false, false, "" };
    TEST_ASSERT_EQUAL(ConnPath::NONE, selectPath(s));
}

// --- WiFi not used when SSID is blank even if WiFi adapter is up ---
void test_wifi_not_used_when_ssid_blank(void) {
    MockConnState s{ true, false, "" };
    // No cellular available either → NONE
    TEST_ASSERT_EQUAL(ConnPath::NONE, selectPath(s));
}

// --- Transport type selection ---
void test_mqtt_transport_selected_by_default(void) {
    TransportType t = selectTransportType(TRANSPORT_MQTT);
    TEST_ASSERT_EQUAL(TransportType::MQTT, t);
}

void test_webhook_transport_selected_when_configured(void) {
    TransportType t = selectTransportType(TRANSPORT_WEBHOOK);
    TEST_ASSERT_EQUAL(TransportType::WEBHOOK, t);
}

// --- WiFi preferred over cellular even with both available ---
void test_wifi_preferred_over_cellular(void) {
    MockConnState s{ true, true, "OfficeWiFi" };
    ConnPath path = selectPath(s);
    TEST_ASSERT_EQUAL(ConnPath::WIFI, path);
    TEST_ASSERT_NOT_EQUAL(ConnPath::CELLULAR, path);
}

// --- Null SSID treated as empty (no WiFi) ---
void test_null_ssid_treated_as_no_wifi(void) {
    MockConnState s{ true, true, nullptr };
    TEST_ASSERT_EQUAL(ConnPath::CELLULAR, selectPath(s));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_wifi_selected_when_available_and_ssid_configured);
    RUN_TEST(test_cellular_fallback_when_wifi_fails);
    RUN_TEST(test_cellular_only_when_no_ssid_configured);
    RUN_TEST(test_none_when_both_unavailable);
    RUN_TEST(test_none_when_no_ssid_and_no_cellular);
    RUN_TEST(test_wifi_not_used_when_ssid_blank);
    RUN_TEST(test_mqtt_transport_selected_by_default);
    RUN_TEST(test_webhook_transport_selected_when_configured);
    RUN_TEST(test_wifi_preferred_over_cellular);
    RUN_TEST(test_null_ssid_treated_as_no_wifi);

    return UNITY_END();
}
