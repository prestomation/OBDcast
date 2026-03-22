# OBDcast

ESP32 firmware for the **Freematics ONE+ Model B** that collects vehicle telemetry and transmits it via MQTT or webhooks.

## What It Does

OBDcast turns your Freematics ONE+ into a connected vehicle telemetry device:

- **Collects**: OBD-II data (speed, RPM, fuel level, temps), GPS location, accelerometer, battery voltage
- **Transmits**: Via MQTT or HTTPS webhooks over 4G LTE cellular
- **Survives offline**: Buffers data to SD card when connectivity is lost
- **Protects your battery**: Smart power management prevents car battery drain

Designed to feed data to [ha-obdcast](https://github.com/prestomation/ha-obdcast), a Home Assistant custom integration.

## Hardware Requirements

### Freematics ONE+ Model B

This firmware is designed specifically for the **Freematics ONE+ Model B**:

| Component | Spec |
|-----------|------|
| MCU | ESP32-WROVER |
| Cellular | SIM7600A-H (4G LTE, US/Canada) |
| GPS | 10Hz via SIM7600 |
| OBD-II | CAN bus adapter |
| Accelerometer | 6-axis MEMS |
| Storage | MicroSD slot |

**Not compatible** with other Freematics models or generic ESP32 boards without modification.

### Additional Requirements

- **SIM card**: Any data-capable SIM (Hologram, Twilio, carrier SIM)
- **MicroSD card**: Optional but recommended for offline buffering (any size, FAT32 formatted)

## Quick Start

### 1. Clone and Configure

```bash
git clone https://github.com/prestomation/OBDcast.git
cd OBDcast
```

Edit `src/config.h`:

```cpp
// Your device ID
#define DEVICE_ID "obdcast_mycar"

// Choose transport
#define TRANSPORT_TYPE TRANSPORT_MQTT  // or TRANSPORT_WEBHOOK

// MQTT settings
#define MQTT_BROKER "mqtt.example.com"
#define MQTT_PORT 8883
#define MQTT_TOPIC "obdcast/mycar/telemetry"

// OR Webhook settings
#define WEBHOOK_URL "https://ha.example.com/api/webhook/obdcast_secret"

// Cellular APN
#define APN "hologram"
```

### 2. Build and Flash

Requires [PlatformIO](https://platformio.org/).

```bash
# Install PlatformIO CLI (if needed)
pip install platformio

# Build
pio run

# Flash (connect device via USB)
pio run --target upload

# Monitor output
pio device monitor
```

### 3. Install in Vehicle

1. Insert SIM card into Freematics ONE+
2. Optionally insert MicroSD card
3. Plug into your vehicle's OBD-II port (usually under the dashboard)
4. Device will power on and begin transmitting

## Configuration

All configuration is in `src/config.h`. Key settings:

### Transport

Choose **one** transport type:

**MQTT** — For Home Assistant MQTT integration or any MQTT broker:
```cpp
#define TRANSPORT_TYPE TRANSPORT_MQTT
#define MQTT_BROKER "mqtt.example.com"
#define MQTT_PORT 8883
#define MQTT_TOPIC "obdcast/mycar/telemetry"
#define MQTT_USE_TLS true
```

**Webhook** — For direct HTTPS POST to an endpoint:
```cpp
#define TRANSPORT_TYPE TRANSPORT_WEBHOOK
#define WEBHOOK_URL "https://ha.example.com/api/webhook/your_secret"
```

### Data Collection

Enable/disable specific OBD-II PIDs:
```cpp
#define COLLECT_PID_SPEED        // Vehicle speed
#define COLLECT_PID_RPM          // Engine RPM
#define COLLECT_PID_FUEL_LEVEL   // Fuel tank level
// ... see config.h for full list
```

### Power Management

Voltage thresholds to prevent battery drain:
```cpp
#define BATTERY_OK_VOLTAGE 12.4       // Normal operation
#define BATTERY_LOW_VOLTAGE 12.0      // Enter standby
#define BATTERY_CRITICAL_VOLTAGE 11.5 // Enter deep sleep
```

See [DESIGN.md](DESIGN.md) for complete configuration reference.

## Transport Options

### MQTT

- Publishes JSON telemetry to configured topic
- TLS encryption via SIM7600 modem
- QoS 0 or 1 support
- Works with Home Assistant, Mosquitto, AWS IoT, etc.

### Webhook (HTTPS POST)

- POSTs JSON to configured URL
- TLS encryption via SIM7600 modem
- Optional Bearer token authentication
- Works with Home Assistant webhooks, custom servers, etc.

### Why SIM7600 for TLS?

The SIM7600 modem handles TLS natively via AT commands. This is more reliable than ESP32 WiFiClientSecure, which has RAM constraints and stability issues with large certificates.

## Data Format

Telemetry is transmitted as JSON:

```json
{
  "device_id": "obdcast_mycar",
  "timestamp": 1679529600,
  "type": "telemetry",
  "location": {
    "lat": 47.6062,
    "lon": -122.3321,
    "speed": 45.5
  },
  "vehicle": {
    "speed": 45,
    "rpm": 2100,
    "fuel_level": 65,
    "coolant_temp": 92
  },
  "power": {
    "battery_voltage": 14.2,
    "state": "active"
  }
}
```

See [DESIGN.md](DESIGN.md) for complete field reference.

## Power Management

OBDcast uses smart power management to avoid draining your car battery:

| State | When | Behavior |
|-------|------|----------|
| **Active** | Engine running | Full data collection, continuous transmission |
| **Standby** | Engine off, battery OK | Periodic pings, motion detection armed |
| **Deep Sleep** | Battery low or extended parking | Minimal power draw, wakes on motion |

The device monitors battery voltage and automatically transitions between states.

## Home Assistant Integration

OBDcast is designed to work with [ha-obdcast](https://github.com/prestomation/ha-obdcast), a Home Assistant custom component.

### Setup

1. Configure OBDcast with MQTT or webhook transport pointing to your HA instance
2. Install ha-obdcast via HACS or manual installation
3. Configure the integration with matching topic/webhook
4. Entities appear automatically (speed, location, fuel level, etc.)

## Troubleshooting

### Device won't connect to cellular

- Check SIM card is inserted correctly
- Verify APN setting matches your carrier
- Check for antenna connection (internal on ONE+)
- Monitor serial output for modem errors

### No OBD-II data

- Ensure vehicle ignition is ON
- Check OBD-II port connection
- Some vehicles require engine running for certain PIDs
- Not all vehicles support all PIDs

### High battery drain

- Check power management thresholds in config
- Verify standby/sleep states are working (check serial output)
- Some vehicles have always-on OBD-II ports; check fuse if needed

## Architecture

See [DESIGN.md](DESIGN.md) for detailed architecture documentation covering:

- Software architecture and layers
- Transport abstraction design
- Power management strategy
- Complete data model
- Configuration reference
- Integration details

## Dependencies

- [FreematicsPlus](https://github.com/stanleyhuangyc/Freematics) — Hardware abstraction library
- [ArduinoJson](https://arduinojson.org/) — JSON serialization
- [PlatformIO](https://platformio.org/) — Build system

## License

MIT License. See [LICENSE](LICENSE) for details.

## Contributing

Contributions welcome! Please open an issue to discuss before submitting PRs for major changes.
