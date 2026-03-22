#include "ConnectivityManager.h"
#include "../config.h"

#ifndef NATIVE_BUILD
  #include <WiFi.h>
  #include "hal/SIM7600Modem.h"
  // SIM7600Modem is accessed as a global singleton from main.cpp
  extern SIM7600Modem* gModem;
#endif

// ---------------------------------------------------------------------------
// connect
// ---------------------------------------------------------------------------
ConnPath ConnectivityManager::connect() {
#ifndef NATIVE_BUILD
    // Try WiFi first if an SSID is configured
    if (strlen(WIFI_SSID) > 0) {
        if (tryWiFi()) {
            _path = ConnPath::WIFI;
            LOGF("Connectivity: WiFi connected (%s)", WiFi.localIP().toString().c_str());
            return _path;
        }
        LOG("Connectivity: WiFi failed, trying cellular");
    }

    // Cellular fallback (or cellular-only mode)
    if (tryCellular()) {
        _path = ConnPath::CELLULAR;
        LOG("Connectivity: cellular connected");
        return _path;
    }

    LOG("Connectivity: all paths failed");
#endif
    _path = ConnPath::NONE;
    return _path;
}

// ---------------------------------------------------------------------------
// disconnect
// ---------------------------------------------------------------------------
void ConnectivityManager::disconnect() {
#ifndef NATIVE_BUILD
    if (_path == ConnPath::WIFI) {
        WiFi.disconnect(true);
    } else if (_path == ConnPath::CELLULAR && gModem) {
        gModem->disconnect();
    }
#endif
    _path = ConnPath::NONE;
}

// ---------------------------------------------------------------------------
// getSignalDbm
// ---------------------------------------------------------------------------
int ConnectivityManager::getSignalDbm() const {
#ifndef NATIVE_BUILD
    if (_path == ConnPath::WIFI) {
        return WiFi.RSSI();
    }
    if (_path == ConnPath::CELLULAR && gModem) {
        return gModem->getSignalDbm();
    }
#endif
    return 0;
}

// ---------------------------------------------------------------------------
// pathName
// ---------------------------------------------------------------------------
const char* ConnectivityManager::pathName(ConnPath p) {
    switch (p) {
    case ConnPath::WIFI:     return "WiFi";
    case ConnPath::CELLULAR: return "Cellular";
    default:                 return "None";
    }
}

// ---------------------------------------------------------------------------
// Private helpers (hardware builds only)
// ---------------------------------------------------------------------------
#ifndef NATIVE_BUILD

bool ConnectivityManager::tryWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t deadline = millis() + 15000; // 15-second timeout
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        delay(250);
    }
    return (WiFi.status() == WL_CONNECTED);
}

bool ConnectivityManager::tryCellular() {
    if (!gModem) return false;
    return gModem->connect();
}

#endif // !NATIVE_BUILD
