#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef NATIVE_BUILD
  #include <ArduinoJson.h>
#else
  // Native build stubs for unit tests
  #include "../test/stubs/ArduinoJsonStub.h"
#endif

// ---------------------------------------------------------------------------
// Telemetry payload structure
// ---------------------------------------------------------------------------

struct OBDData {
    int     speed        = -1;   // km/h       (PID 0x0D)
    int     rpm          = -1;   // RPM        (PID 0x0C)
    int     fuel_pct     = -1;   // %          (PID 0x2F)
    int     coolant_c    = -1;   // °C         (PID 0x05)
    int     engine_load  = -1;   // %          (PID 0x04)
    int     throttle_pct = -1;   // %          (PID 0x11)
    int     intake_temp  = -1;   // °C         (PID 0x0F)
    float   fuel_rate    = -1.f; // L/h        (PID 0x5E)
    float   maf          = -1.f; // g/s        (PID 0x10)
    int     runtime      = -1;   // seconds    (PID 0x1F)
    float   voltage      = 0.f;  // V (battery/OBD rail)
};

struct GPSData {
    float   lat      = 0.f;
    float   lon      = 0.f;
    float   alt_m    = 0.f;
    float   heading  = 0.f;
    float   speed_kph= 0.f;
    float   hdop     = 99.f;
    int     satellites = 0;
    bool    fix      = false;
};

struct MotionData {
    float ax = 0.f;
    float ay = 0.f;
    float az = 0.f;
};

struct Payload {
    // Header
    char        device_id[32];
    uint32_t    ts          = 0;    // Unix timestamp
    bool        ignition    = false;
    int         signal_dbm  = 0;

    OBDData     obd;
    GPSData     gps;
    MotionData  motion;

    // ---------------------------------------------------------------------------
    // Serialize to JSON. Returns number of bytes written (0 on failure).
    // ---------------------------------------------------------------------------
    size_t toJson(char* buf, size_t bufLen) const {
        StaticJsonDocument<1024> doc;

        doc["device_id"] = device_id;
        doc["ts"]        = ts;
        doc["ignition"]  = ignition;
        doc["signal_dbm"]= signal_dbm;

        JsonObject jobdobj = doc.createNestedObject("obd");
        if (obd.speed       >= 0) jobdobj["speed"]            = obd.speed;
        if (obd.rpm         >= 0) jobdobj["rpm"]              = obd.rpm;
        if (obd.fuel_pct    >= 0) jobdobj["fuel_pct"]         = obd.fuel_pct;
        if (obd.coolant_c   >= 0) jobdobj["coolant_c"]        = obd.coolant_c;
        if (obd.engine_load >= 0) jobdobj["engine_load_pct"]  = obd.engine_load;
        if (obd.throttle_pct>= 0) jobdobj["throttle_pct"]    = obd.throttle_pct;
        if (obd.intake_temp >= 0) jobdobj["intake_temp_c"]   = obd.intake_temp;
        if (obd.fuel_rate   >= 0) jobdobj["fuel_rate_lph"]   = obd.fuel_rate;
        if (obd.maf         >= 0) jobdobj["maf_gs"]          = obd.maf;
        if (obd.runtime     >= 0) jobdobj["runtime_s"]        = obd.runtime;
        jobdobj["voltage"]                                     = obd.voltage;

        JsonObject jgps = doc.createNestedObject("gps");
        jgps["lat"]      = gps.lat;
        jgps["lon"]      = gps.lon;
        jgps["alt_m"]    = gps.alt_m;
        jgps["heading"]  = gps.heading;
        jgps["speed_kph"]= gps.speed_kph;
        jgps["hdop"]     = gps.hdop;
        jgps["satellites"]= gps.satellites;
        jgps["fix"]      = gps.fix;

        JsonObject jmotion = doc.createNestedObject("motion");
        jmotion["ax"] = motion.ax;
        jmotion["ay"] = motion.ay;
        jmotion["az"] = motion.az;

        return serializeJson(doc, buf, bufLen);
    }
};
