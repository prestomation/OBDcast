#include "DataCollector.h"
#include "../config.h"
#include <string.h>

#ifndef NATIVE_BUILD
  #include <Arduino.h>
#else
  #include <ctime>
  static uint32_t millis_stub() { return 0; }
  #define millis() millis_stub()
#endif

// OBD-II PID constants
static constexpr uint16_t PID_SPEED        = 0x0D;
static constexpr uint16_t PID_RPM          = 0x0C;
static constexpr uint16_t PID_THROTTLE     = 0x11;
static constexpr uint16_t PID_ENGINE_LOAD  = 0x04;
static constexpr uint16_t PID_COOLANT_TEMP = 0x05;
static constexpr uint16_t PID_INTAKE_TEMP  = 0x0F;
static constexpr uint16_t PID_FUEL_LEVEL   = 0x2F;
static constexpr uint16_t PID_RUNTIME      = 0x1F;

// ---------------------------------------------------------------------------
// collect
// ---------------------------------------------------------------------------
bool DataCollector::collect(Payload& out, int signalDbm) {
    memset(&out, 0, sizeof(out));
    strncpy(out.device_id, DEVICE_ID, sizeof(out.device_id) - 1);
    out.ts         = getTimestamp();
    out.signal_dbm = signalDbm;

    collectOBD(out);
    collectGPS(out);
    collectMEMS(out);

    // Ignition inferred from voltage
    out.ignition = (out.obd.voltage >= VOLTAGE_ACTIVE_THRESHOLD);

    return true;
}

// ---------------------------------------------------------------------------
// collectPing – minimal standby heartbeat
// ---------------------------------------------------------------------------
void DataCollector::collectPing(Payload& out, int signalDbm) {
    memset(&out, 0, sizeof(out));
    strncpy(out.device_id, DEVICE_ID, sizeof(out.device_id) - 1);
    out.ts         = getTimestamp();
    out.signal_dbm = signalDbm;
    out.ignition   = false;

    // Only read voltage and GPS for a ping
    out.obd.voltage = _obd.readVoltage();
    collectGPS(out);
}

// ---------------------------------------------------------------------------
// collectOBD
// ---------------------------------------------------------------------------
void DataCollector::collectOBD(Payload& out) {
    out.obd.voltage = _obd.readVoltage();

    if (!_obd.isConnected()) return;

    int v = 0;

#ifdef COLLECT_PID_SPEED
    if (_obd.readPID(PID_SPEED, v))        out.obd.speed        = v;
#endif
#ifdef COLLECT_PID_RPM
    if (_obd.readPID(PID_RPM, v))          out.obd.rpm          = v;
#endif
#ifdef COLLECT_PID_THROTTLE
    if (_obd.readPID(PID_THROTTLE, v))     out.obd.throttle_pct = v;
#endif
#ifdef COLLECT_PID_ENGINE_LOAD
    if (_obd.readPID(PID_ENGINE_LOAD, v))  out.obd.engine_load  = v;
#endif
#ifdef COLLECT_PID_COOLANT_TEMP
    if (_obd.readPID(PID_COOLANT_TEMP, v)) out.obd.coolant_c    = v;
#endif
#ifdef COLLECT_PID_INTAKE_TEMP
    if (_obd.readPID(PID_INTAKE_TEMP, v))  out.obd.intake_temp  = v;
#endif
#ifdef COLLECT_PID_FUEL_LEVEL
    if (_obd.readPID(PID_FUEL_LEVEL, v))   out.obd.fuel_pct     = v;
#endif
#ifdef COLLECT_PID_RUNTIME
    if (_obd.readPID(PID_RUNTIME, v))      out.obd.runtime      = v;
#endif
}

// ---------------------------------------------------------------------------
// collectGPS
// ---------------------------------------------------------------------------
void DataCollector::collectGPS(Payload& out) {
    if (!_gnss.hasFix()) return;
    _gnss.getLocation(out.gps.lat, out.gps.lon, out.gps.alt_m,
                      out.gps.hdop, out.gps.speed_kph,
                      out.gps.heading, out.gps.satellites);
    out.gps.fix = true;
}

// ---------------------------------------------------------------------------
// collectMEMS
// ---------------------------------------------------------------------------
void DataCollector::collectMEMS(Payload& out) {
    _mems.getAccel(out.motion.ax, out.motion.ay, out.motion.az);
}

// ---------------------------------------------------------------------------
// getTimestamp – returns Unix time (NTP synced), or millis as fallback
// ---------------------------------------------------------------------------
uint32_t DataCollector::getTimestamp() {
#ifndef NATIVE_BUILD
    // Use ESP32 time (set via NTP or GNSS epoch)
    return (uint32_t)time(nullptr);
#else
    return (uint32_t)time(nullptr);
#endif
}
