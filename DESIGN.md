# OBDcast Architecture Design

## Overview

OBDcast is ESP32 firmware for the **Freematics ONE+ Model B** hardware platform. It collects vehicle telemetry data (OBD-II PIDs, GPS, accelerometer, battery voltage) and transmits it to external services via MQTT or HTTPS webhooks over cellular connectivity.

The firmware replaces the stock Freematics "telelogger" reference implementation with a clean, modular architecture designed for reliability, extensibility, and Home Assistant integration.

### Goals

- **Reliable telemetry collection**: OBD-II vehicle data, 10Hz GPS, accelerometer, battery voltage
- **Flexible transport**: User-configurable MQTT or Webhook (HTTPS POST) output
- **Cellular-first**: 4G LTE via SIM7600 modem with native TLS offload
- **Smart power management**: Tiered sleep policy to avoid draining car battery
- **Offline resilience**: SD card buffering when connectivity is unavailable
- **Home Assistant integration**: Designed to feed data to the `ha-obdcast` custom component

---

## Hardware Overview

### Freematics ONE+ Model B

| Component | Details |
|-----------|---------|
| MCU | ESP32-WROVER (dual-core 240MHz, 4MB flash, 8MB PSRAM) |
| Cellular | SIM7600A-H (4G LTE Cat-4, US/Canada bands) |
| GNSS | 10Hz GPS/GLONASS via SIM7600 integrated receiver |
| OBD-II | CAN bus interface via Freematics OBD adapter |
| IMU | 6-axis MEMS accelerometer/gyroscope |
| Storage | MicroSD card slot |
| Power | 12V input from OBD-II port, onboard voltage regulation |
| Connector | Standard OBD-II plug |

### Key Hardware Capabilities

- **SIM7600 TLS offload**: The modem handles SSL/TLS natively via AT commands, avoiding ESP32 WiFiClientSecure memory constraints
- **10Hz GNSS**: High-frequency position updates for accurate trip tracking
- **Always-on power**: OBD-II port provides constant 12V, requiring intelligent sleep management
- **CAN bus access**: Direct vehicle ECU communication for PID queries

---

## Software Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application Layer                         │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ │
│  │   Config    │ │   Main      │ │   State     │ │   SD Card   │ │
│  │   Manager   │ │   Loop      │ │   Machine   │ │   Buffer    │ │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                        Service Layer                             │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                    Transport Interface                       │ │
│  │  ┌───────────────────┐    ┌───────────────────┐             │ │
│  │  │  MQTT Transport   │    │ Webhook Transport │             │ │
│  │  └───────────────────┘    └───────────────────┘             │ │
│  └─────────────────────────────────────────────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                   Data Collector                             │ │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐            │ │
│  │  │  OBD-II │ │  GNSS   │ │  IMU    │ │ Battery │            │ │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘            │ │
│  └─────────────────────────────────────────────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                   Power Manager                              │ │
│  └─────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                    Hardware Abstraction Layer                    │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              FreematicsPlus Arduino Library                  │ │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐            │ │
│  │  │  OBD    │ │ SIM7600 │ │  MEMS   │ │   GPS   │            │ │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘            │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### Layer Responsibilities

#### Hardware Abstraction Layer (FreematicsPlus)

The **FreematicsPlus** Arduino library provides the HAL for all hardware interaction:

- `FreematicsOBD`: CAN bus communication, PID queries, protocol detection
- `FreematicsSIM7600`: Modem control, HTTP/HTTPS, MQTT, TCP/IP, GPS via modem
- `FreematicsMEMS`: Accelerometer/gyroscope readings
- Hardware initialization and low-level I/O

We depend on this library rather than reimplementing hardware drivers.

#### Service Layer

**Data Collector**: Aggregates readings from all sensors into a unified data structure. Handles polling rates, data validation, and timestamp assignment.

**Transport Interface**: Abstract interface for data transmission. Implementations:
- `MqttTransport`: Publishes JSON payloads to configured broker/topic
- `WebhookTransport`: POSTs JSON to configured HTTPS endpoint

**Power Manager**: Monitors battery voltage and ignition state, transitions between power modes, manages wake sources.

#### Application Layer

**Config Manager**: Loads/saves configuration from NVS (ESP32 non-volatile storage). Provides compile-time defaults with runtime overrides.

**State Machine**: Manages firmware state (initializing → active → standby → sleep) and transitions.

**SD Card Buffer**: Stores telemetry data during connectivity outages, replays when connection restored.

**Main Loop**: Orchestrates all components, handles timing, and manages the collection/transmission cycle.

---

## Transport Abstraction

### Interface Design

```cpp
class ITransport {
public:
    virtual ~ITransport() = default;
    
    // Initialize the transport (connect to broker, validate URL, etc.)
    virtual bool begin() = 0;
    
    // Send a telemetry payload, returns true on success
    virtual bool send(const TelemetryData& data) = 0;
    
    // Check if transport is connected/ready
    virtual bool isConnected() = 0;
    
    // Graceful shutdown
    virtual void end() = 0;
    
    // Get transport type name for logging
    virtual const char* getName() = 0;
};
```

### MQTT Transport

Uses SIM7600 AT commands for MQTT:

```cpp
class MqttTransport : public ITransport {
    // Config
    const char* broker;      // e.g., "mqtt.example.com"
    uint16_t port;           // 8883 for TLS
    const char* topic;       // e.g., "obdcast/vehicle1/telemetry"
    const char* clientId;
    const char* username;
    const char* password;
    bool useTls;
    
    // AT commands: AT+CMQTTSTART, AT+CMQTTCONNECT, AT+CMQTTPUB
};
```

**Features**:
- TLS via modem's native SSL stack
- QoS 0 or 1 (configurable)
- Auto-reconnect on connection loss
- LWT (Last Will and Testament) for offline detection

### Webhook Transport

HTTPS POST via SIM7600:

```cpp
class WebhookTransport : public ITransport {
    // Config
    const char* url;         // e.g., "https://ha.example.com/api/webhook/obdcast"
    const char* authHeader;  // Optional: "Bearer <token>"
    
    // AT commands: AT+HTTPINIT, AT+HTTPPARA, AT+HTTPACTION
};
```

**Features**:
- TLS via modem's native SSL stack
- Custom headers for authentication
- Configurable timeout
- Response code validation

### Transport Selection

Configured at compile time or via NVS:

```cpp
TransportType transportType = TRANSPORT_MQTT;  // or TRANSPORT_WEBHOOK

ITransport* createTransport() {
    switch (transportType) {
        case TRANSPORT_MQTT:
            return new MqttTransport(config);
        case TRANSPORT_WEBHOOK:
            return new WebhookTransport(config);
    }
}
```

---

## Power Management Strategy

### The Problem

The OBD-II port provides constant 12V power, even when the car is off. Without power management, the device would drain the car battery over days/weeks of parking.

### Power States

| State | Condition | Behavior | Power Draw |
|-------|-----------|----------|------------|
| **Active** | Engine running | Full telemetry collection, continuous transmission | ~150mA |
| **Standby** | Engine off, battery healthy | Reduced polling, periodic pings, motion wake armed | ~30mA |
| **Deep Sleep** | Battery low OR extended parking | ESP32 deep sleep, modem off, motion wake only | ~1mA |

### State Transitions

```
                    ┌─────────────┐
                    │ INITIALIZING│
                    └──────┬──────┘
                           │
                           ▼
    ┌──────────────────────────────────────────┐
    │                 ACTIVE                    │
    │  • Full data collection                  │
    │  • Continuous transmission               │
    │  • 1-second main loop                    │
    └──────┬───────────────────────────┬───────┘
           │ Engine off               │ Battery critical
           ▼                          │ (<11.5V)
    ┌──────────────┐                  │
    │   STANDBY    │                  │
    │  • Reduced polling              │
    │  • Ping every N minutes         │
    │  • Motion detection armed       │
    └──────┬───────────────┬──────────┘
           │               │
           │ Battery low   │ Motion detected
           │ (<12.0V) OR   │ OR engine start
           │ Idle timeout  │
           ▼               ▼
    ┌──────────────┐    ┌──────────────┐
    │  DEEP SLEEP  │    │    ACTIVE    │
    │  • ESP32 sleeps    │              │
    │  • Modem off       │              │
    │  • Motion wake     │              │
    └──────────────┘    └──────────────┘
```

### Voltage Thresholds

| Threshold | Voltage | Action |
|-----------|---------|--------|
| Battery OK | ≥12.4V | Normal operation |
| Battery Low | 12.0V - 12.4V | Enter standby if engine off |
| Battery Critical | <12.0V | Enter deep sleep immediately |
| Battery Recovery | >12.6V | Safe to exit deep sleep |

### Motion Wake

The MEMS accelerometer can wake the ESP32 from deep sleep:

- Configured threshold: ~0.1g acceleration
- Debounce: 500ms sustained motion required
- On wake: Check battery voltage before full activation

### Standby Ping

In standby mode, the device periodically transmits a "heartbeat" ping:

```json
{
  "type": "ping",
  "timestamp": 1679529600,
  "battery_voltage": 12.45,
  "state": "standby",
  "location": { "lat": 47.6062, "lon": -122.3321 }
}
```

Configurable interval: 5-60 minutes (default: 15 minutes).

---

## Data Model

### Telemetry Payload

All telemetry is transmitted as JSON:

```json
{
  "device_id": "obdcast_A1B2C3",
  "timestamp": 1679529600,
  "type": "telemetry",
  
  "location": {
    "lat": 47.6062,
    "lon": -122.3321,
    "alt": 56.2,
    "speed": 45.5,
    "heading": 270,
    "satellites": 12,
    "hdop": 0.9
  },
  
  "vehicle": {
    "speed": 45,
    "rpm": 2100,
    "throttle": 22,
    "engine_load": 35,
    "coolant_temp": 92,
    "intake_temp": 28,
    "fuel_level": 65,
    "fuel_rate": 3.2,
    "maf": 12.5,
    "runtime": 1234
  },
  
  "imu": {
    "accel_x": 0.02,
    "accel_y": -0.15,
    "accel_z": 0.98
  },
  
  "power": {
    "battery_voltage": 14.2,
    "state": "active"
  }
}
```

### Field Reference

#### Location Fields

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `lat` | float | degrees | Latitude (-90 to 90) |
| `lon` | float | degrees | Longitude (-180 to 180) |
| `alt` | float | meters | Altitude above sea level |
| `speed` | float | km/h | GPS-derived speed |
| `heading` | int | degrees | Course over ground (0-359) |
| `satellites` | int | count | Number of satellites in fix |
| `hdop` | float | - | Horizontal dilution of precision |

#### Vehicle Fields (OBD-II PIDs)

| Field | PID | Type | Unit | Description |
|-------|-----|------|------|-------------|
| `speed` | 0x0D | int | km/h | Vehicle speed |
| `rpm` | 0x0C | int | RPM | Engine RPM |
| `throttle` | 0x11 | int | % | Throttle position |
| `engine_load` | 0x04 | int | % | Calculated engine load |
| `coolant_temp` | 0x05 | int | °C | Engine coolant temperature |
| `intake_temp` | 0x0F | int | °C | Intake air temperature |
| `fuel_level` | 0x2F | int | % | Fuel tank level |
| `fuel_rate` | 0x5E | float | L/h | Engine fuel rate |
| `maf` | 0x10 | float | g/s | Mass airflow rate |
| `runtime` | 0x1F | int | seconds | Engine runtime since start |

#### IMU Fields

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `accel_x` | float | g | Lateral acceleration |
| `accel_y` | float | g | Longitudinal acceleration |
| `accel_z` | float | g | Vertical acceleration |

#### Power Fields

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `battery_voltage` | float | V | Vehicle battery voltage |
| `state` | string | - | Current power state (active/standby/sleep) |

---

## Configuration Reference

### Config Header (`config.h`)

Compile-time configuration with runtime NVS overrides where noted.

```cpp
// =============================================================================
// DEVICE IDENTITY
// =============================================================================
#define DEVICE_ID "obdcast_CHANGEME"  // Unique device identifier

// =============================================================================
// TRANSPORT CONFIGURATION
// =============================================================================
#define TRANSPORT_TYPE TRANSPORT_MQTT  // TRANSPORT_MQTT or TRANSPORT_WEBHOOK

// MQTT Settings (if TRANSPORT_TYPE == TRANSPORT_MQTT)
#define MQTT_BROKER "mqtt.example.com"
#define MQTT_PORT 8883
#define MQTT_TOPIC "obdcast/vehicle1/telemetry"
#define MQTT_CLIENT_ID DEVICE_ID
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
#define MQTT_USE_TLS true
#define MQTT_QOS 1

// Webhook Settings (if TRANSPORT_TYPE == TRANSPORT_WEBHOOK)
#define WEBHOOK_URL "https://ha.example.com/api/webhook/obdcast_abc123"
#define WEBHOOK_AUTH_HEADER ""  // e.g., "Bearer your-token"
#define WEBHOOK_TIMEOUT_MS 10000

// =============================================================================
// DATA COLLECTION
// =============================================================================
// Polling intervals (milliseconds)
#define OBD_POLL_INTERVAL 1000      // OBD-II PID queries
#define GPS_POLL_INTERVAL 100       // GPS updates (10Hz)
#define IMU_POLL_INTERVAL 100       // Accelerometer updates
#define TRANSMIT_INTERVAL 5000      // Telemetry transmission

// PIDs to collect (comment out to disable)
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
#define BATTERY_OK_VOLTAGE 12.4       // Full operation threshold
#define BATTERY_LOW_VOLTAGE 12.0      // Enter standby threshold
#define BATTERY_CRITICAL_VOLTAGE 11.5 // Enter deep sleep threshold
#define BATTERY_RECOVERY_VOLTAGE 12.6 // Safe to wake threshold

#define STANDBY_PING_INTERVAL_MIN 15  // Minutes between pings in standby
#define STANDBY_IDLE_TIMEOUT_MIN 60   // Minutes before standby → deep sleep

#define MOTION_WAKE_THRESHOLD_G 0.1   // Accelerometer wake threshold
#define MOTION_DEBOUNCE_MS 500        // Motion detection debounce

// =============================================================================
// CELLULAR / MODEM
// =============================================================================
#define APN "hologram"                // Cellular APN
#define SIM_PIN ""                    // SIM PIN if required

// =============================================================================
// SD CARD BUFFER
// =============================================================================
#define SD_BUFFER_ENABLED true
#define SD_BUFFER_MAX_SIZE_MB 100     // Max buffer size before rotation
#define SD_BUFFER_REPLAY_ON_CONNECT true

// =============================================================================
// DEBUG
// =============================================================================
#define DEBUG_SERIAL true
#define DEBUG_BAUD 115200
```

### NVS Runtime Overrides

The following can be changed at runtime via a config command or web interface:

| NVS Key | Type | Description |
|---------|------|-------------|
| `device_id` | string | Device identifier |
| `mqtt_broker` | string | MQTT broker hostname |
| `mqtt_topic` | string | MQTT publish topic |
| `webhook_url` | string | Webhook endpoint URL |
| `transmit_int` | int | Transmission interval (ms) |
| `standby_ping` | int | Standby ping interval (min) |

---

## Build System

### PlatformIO

The project uses PlatformIO for builds, targeting the ESP32 platform.

**`platformio.ini`**:

```ini
[env:freematics_one_plus]
platform = espressif32
board = esp32dev
framework = arduino
board_build.partitions = default_16MB.csv

monitor_speed = 115200

lib_deps =
    freematics/FreematicsPlus
    bblanchon/ArduinoJson@^6.21.0

build_flags =
    -DBOARD_HAS_PSRAM
    -DCONFIG_SPIRAM_CACHE_WORKAROUND
```

### Build Commands

```bash
# Build
pio run

# Upload
pio run --target upload

# Monitor serial output
pio device monitor

# Clean build
pio run --target clean
```

---

## Integration with ha-obdcast

OBDcast is designed to work with **ha-obdcast**, a Home Assistant custom integration (separate repository).

### Architecture

```
┌─────────────┐         ┌─────────────┐         ┌─────────────────┐
│  OBDcast    │ ──────► │   MQTT or   │ ──────► │   ha-obdcast    │
│  Firmware   │         │   Webhook   │         │   Integration   │
└─────────────┘         └─────────────┘         └─────────────────┘
                                                        │
                                                        ▼
                                                ┌─────────────────┐
                                                │  Home Assistant │
                                                │  Entities       │
                                                └─────────────────┘
```

### MQTT Integration

If using MQTT transport:
- ha-obdcast subscribes to the configured topic
- Telemetry payloads are parsed and exposed as HA sensors
- MQTT discovery can auto-configure entities

### Webhook Integration

If using webhooks:
- ha-obdcast exposes a webhook endpoint
- OBDcast POSTs telemetry to this endpoint
- The integration parses and creates entities

### Entity Examples

ha-obdcast creates entities like:

- `sensor.vehicle_speed`
- `sensor.vehicle_rpm`
- `sensor.vehicle_fuel_level`
- `device_tracker.vehicle_location`
- `sensor.vehicle_battery_voltage`

---

## FreematicsPlus Dependency

OBDcast depends on the [FreematicsPlus](https://github.com/stanleyhuangyc/Freematics) Arduino library for hardware abstraction.

### Why FreematicsPlus?

- **Official HAL**: Written by Freematics for their hardware
- **Proven code**: Used by thousands of Freematics users
- **Hardware support**: OBD protocols, SIM7600 AT commands, MEMS, GPS
- **Maintained**: Active development, bug fixes

### What We Use

| Class | Purpose |
|-------|---------|
| `FreematicsOBD` | OBD-II CAN bus communication |
| `CellularSIM7600` | Modem control, HTTP/MQTT, GPS |
| `MEMS_ICC42688` | Accelerometer/gyroscope readings |

### What We Don't Use

- Freematics cloud integration (we use our own transport)
- WiFi features (cellular only in v1)
- Web dashboard (headless device)

---

## Known Limitations

### v1 Scope

1. **Cellular only**: No WiFi support planned for v1. The SIM7600 provides reliable WAN connectivity; WiFi adds complexity without benefit for a mobile device.

2. **US/Canada only**: The SIM7600A-H modem supports LTE bands for North America. Other regions need different modem variants.

3. **No OTA updates**: Firmware updates require physical USB access. OTA may be added in v2.

4. **Single vehicle**: Each device is configured for one vehicle. Multi-vehicle requires multiple devices.

5. **PID support varies**: Not all vehicles support all PIDs. The firmware handles missing PIDs gracefully but can't collect unavailable data.

### Hardware Constraints

- **ESP32 RAM**: ~320KB available. JSON payloads and buffers must be sized carefully.
- **SD card speed**: Large buffer replays may cause transmission delays.
- **Modem warmup**: SIM7600 takes 5-15 seconds to register on network after wake.

### Protocol Limitations

- **OBD-II polling rate**: Most vehicles limit PID query rate to prevent bus congestion. 1-second interval is safe for most.
- **GPS cold start**: First fix after deep sleep may take 30-60 seconds.

---

## Future Considerations

Potential v2 features (not in scope for initial release):

- OTA firmware updates via modem
- WiFi configuration portal for initial setup
- Geofencing alerts
- Trip detection and summary
- DTC (diagnostic trouble code) reading and clearing
- CAN bus sniffing for non-standard data
- Integration with additional platforms beyond Home Assistant
