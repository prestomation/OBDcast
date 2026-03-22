#pragma once
// ---------------------------------------------------------------------------
// Minimal ArduinoJson stub for native unit test builds.
// Only the subset used by Payload.h is implemented.
// ---------------------------------------------------------------------------

#include <cstring>
#include <cstdio>
#include <cstddef>

// ---------------------------------------------------------------------------
// JsonObject stub
// ---------------------------------------------------------------------------
struct JsonObjectStub {
    struct Entry { char key[64]; char value[128]; };
    Entry entries[32];
    int   count = 0;

    template<typename T>
    void operator[](const char* k) {
        // no-op for stub; values written but not stored
    }

    // Operator[] returns a proxy that accepts assignment
    struct Proxy {
        char* dst;
        explicit Proxy(char* d) : dst(d) {}
        template<typename T> Proxy& operator=(T v) {
            if (dst) snprintf(dst, 64, "%f", (double)v);
            return *this;
        }
        Proxy& operator=(bool v) {
            if (dst) snprintf(dst, 64, "%s", v ? "true" : "false");
            return *this;
        }
        Proxy& operator=(const char* v) {
            if (dst && v) snprintf(dst, 64, "%s", v);
            return *this;
        }
        Proxy& operator=(int v) {
            if (dst) snprintf(dst, 64, "%d", v);
            return *this;
        }
    };

    Proxy operator[](const char* k) {
        if (count < 32) {
            strncpy(entries[count].key, k, 63);
            count++;
            return Proxy(entries[count-1].value);
        }
        return Proxy(nullptr);
    }
};

using JsonObject = JsonObjectStub;

// ---------------------------------------------------------------------------
// StaticJsonDocument stub
// ---------------------------------------------------------------------------
template<size_t N>
struct StaticJsonDocument {
    char _deviceId[64]  = {};
    char _ts[32]        = {};
    char _ignition[16]  = {};
    char _signal[32]    = {};
    JsonObjectStub _obd;
    JsonObjectStub _gps;
    JsonObjectStub _motion;
    bool _hasObd    = false;
    bool _hasGps    = false;
    bool _hasMotion = false;

    struct TopProxy {
        StaticJsonDocument* doc;
        const char* key;

        template<typename T>
        TopProxy& operator=(T v) {
            if (strcmp(key, "device_id") == 0)
                snprintf(doc->_deviceId, sizeof(doc->_deviceId), "%s", (const char*)v);
            else if (strcmp(key, "ts") == 0)
                snprintf(doc->_ts, sizeof(doc->_ts), "%lu", (unsigned long)(unsigned)v);
            else if (strcmp(key, "ignition") == 0)
                snprintf(doc->_ignition, sizeof(doc->_ignition), "%s", v ? "true" : "false");
            else if (strcmp(key, "signal_dbm") == 0)
                snprintf(doc->_signal, sizeof(doc->_signal), "%d", (int)v);
            return *this;
        }
        TopProxy& operator=(bool v) {
            snprintf(doc->_ignition, sizeof(doc->_ignition), "%s", v ? "true" : "false");
            return *this;
        }
        TopProxy& operator=(const char* v) {
            if (strcmp(key, "device_id") == 0)
                snprintf(doc->_deviceId, sizeof(doc->_deviceId), "%s", v ? v : "");
            return *this;
        }
    };

    TopProxy operator[](const char* k) { return TopProxy{this, k}; }

    JsonObjectStub& createNestedObject(const char* k) {
        if (strcmp(k, "obd") == 0)    { _hasObd = true;    return _obd; }
        if (strcmp(k, "gps") == 0)    { _hasGps = true;    return _gps; }
        if (strcmp(k, "motion") == 0) { _hasMotion = true; return _motion; }
        static JsonObjectStub dummy;
        return dummy;
    }

    const char* getDeviceId() const { return _deviceId; }
    bool hasObd()    const { return _hasObd; }
    bool hasGps()    const { return _hasGps; }
    bool hasMotion() const { return _hasMotion; }
};

// ---------------------------------------------------------------------------
// serializeJson stub – writes a simple JSON string
// ---------------------------------------------------------------------------
template<size_t N>
size_t serializeJson(const StaticJsonDocument<N>& doc, char* buf, size_t bufLen) {
    // Produce valid (minimal) JSON so JSON parsing tests work
    int written = snprintf(buf, bufLen,
        "{\"device_id\":\"%s\",\"ts\":%s,\"ignition\":%s,\"signal_dbm\":%s,"
        "\"obd\":{},\"gps\":{},\"motion\":{}}",
        doc._deviceId,
        doc._ts[0]       ? doc._ts       : "0",
        doc._ignition[0] ? doc._ignition : "false",
        doc._signal[0]   ? doc._signal   : "0");
    return (written > 0 && (size_t)written < bufLen) ? (size_t)written : 0;
}
