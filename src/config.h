#pragma once

// =============================================================================
// CONFIGURATION SECURITY NOTE
// =============================================================================
// This file contains compile-time defaults. For production deployments:
//   - Do NOT commit real credentials (WiFi passwords, MQTT passwords, tokens)
//   - Copy config.h to config_local.h, add config_local.h to .gitignore,
//     and include it instead; or supply credentials via NVS at runtime.
//   - The NVS runtime override mechanism (planned) will allow changing
//     credentials without reflashing.
// =============================================================================

// =============================================================================
// DEVICE IDENTITY
// =============================================================================
#define DEVICE_ID "obdcast-001"  // Unique device identifier

// =============================================================================
// TRANSPORT CONFIGURATION
// =============================================================================
#define TRANSPORT_MQTT    1
#define TRANSPORT_WEBHOOK 2
#define TRANSPORT_MODE    TRANSPORT_MQTT  // TRANSPORT_MQTT or TRANSPORT_WEBHOOK

// MQTT Settings (used when TRANSPORT_MODE == TRANSPORT_MQTT)
#define MQTT_BROKER       "mqtt.example.com"
#define MQTT_PORT         8883
#define MQTT_TOPIC_PREFIX "obdcast"
#define MQTT_USER         ""
#define MQTT_PASS         ""
#define MQTT_USE_TLS      true
#define MQTT_QOS          1

// Webhook Settings (used when TRANSPORT_MODE == TRANSPORT_WEBHOOK)
#define WEBHOOK_URL        "https://ha.example.com/api/webhook/obdcast_abc123"
#define WEBHOOK_AUTH_TOKEN ""  // e.g., "mytoken" -> sent as "Bearer mytoken"
#define WEBHOOK_TIMEOUT_MS 10000

// Webhook HMAC signing — set to the same secret configured in ha-obdcast.
// Leave blank ("") to disable signing (useful for local dev/testing).
// When set, an X-OBDcast-Signature header is added to every webhook request
// containing the HMAC-SHA256 hex digest of the JSON body.
#define WEBHOOK_HMAC_SECRET ""

// Convenience: HMAC is automatically enabled when a secret is provided.
#define WEBHOOK_HMAC_ENABLED (sizeof(WEBHOOK_HMAC_SECRET) > 1)

// =============================================================================
// WIFI (optional — cellular fallback used when WiFi unavailable)
// =============================================================================
#define WIFI_SSID     ""  // Leave empty to disable WiFi (cellular-only mode)
#define WIFI_PASSWORD ""

// =============================================================================
// CELLULAR / MODEM
// =============================================================================
#define CELLULAR_APN "hologram"   // Cellular APN
#define SIM_PIN      ""           // SIM PIN if required

// =============================================================================
// DATA COLLECTION INTERVALS (milliseconds)
// =============================================================================
#define DATA_INTERVAL_MS  10000   // How often to collect and send while ACTIVE
#define STANDBY_PING_MS   900000  // Ping interval when engine off (15 min default)

#define GPS_POLL_INTERVAL_MS  100   // GPS updates (10 Hz)
#define IMU_POLL_INTERVAL_MS  100   // Accelerometer updates (10 Hz)

// =============================================================================
// OBD PIDs TO COLLECT (comment out to disable individual PIDs)
// =============================================================================
#define COLLECT_PID_SPEED
#define COLLECT_PID_RPM
#define COLLECT_PID_THROTTLE
#define COLLECT_PID_ENGINE_LOAD
#define COLLECT_PID_COOLANT_TEMP
#define COLLECT_PID_INTAKE_TEMP
#define COLLECT_PID_FUEL_LEVEL
#define COLLECT_PID_FUEL_RATE
#define COLLECT_PID_MAF
#define COLLECT_PID_RUNTIME

// =============================================================================
// POWER MANAGEMENT
// =============================================================================
// Two thresholds govern the power state machine:
//   voltage > VOLTAGE_ACTIVE_THRESHOLD              => ACTIVE (engine running)
//   VOLTAGE_STANDBY_THRESHOLD < v <= ACTIVE_THRESH  => STANDBY
//   voltage <= VOLTAGE_STANDBY_THRESHOLD             => DEEP_SLEEP
#define VOLTAGE_ACTIVE_THRESHOLD    13.2f  // Engine running voltage
#define VOLTAGE_STANDBY_THRESHOLD   12.2f  // Minimum battery voltage before deep sleep

#define STANDBY_IDLE_TIMEOUT_MS     3600000UL  // 1 hour before standby → deep sleep
#define DEEP_SLEEP_WAKE_INTERVAL_S  3600       // Wake timer (1 hour)

#define MOTION_WAKE_THRESHOLD_G     0.1f   // Accelerometer wake threshold (g)
#define MOTION_DEBOUNCE_MS          500    // Motion detection debounce

// =============================================================================
// SD CARD BUFFER
// =============================================================================
#define SD_BUFFER_ENABLED           true
#define SD_BUFFER_MAX_SIZE_MB       100
#define SD_BUFFER_REPLAY_ON_CONNECT true
#define SD_BUFFER_FILE_PATH         "/obdcast_buffer.jsonl"

// =============================================================================
// DEBUG
// =============================================================================
#define DEBUG_SERIAL true
#define DEBUG_BAUD   115200

#ifdef DEBUG_SERIAL
  #define LOG(msg)        Serial.println(msg)
  #define LOGF(fmt, ...)  Serial.printf(fmt "\n", __VA_ARGS__)
#else
  #define LOG(msg)
  #define LOGF(fmt, ...)
#endif
