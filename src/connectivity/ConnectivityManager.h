#pragma once

#include "../transport/ITransport.h"

// ---------------------------------------------------------------------------
// ConnectivityManager – WiFi-first, cellular fallback transport selection.
//
// Policy:
//   1. If WIFI_SSID is configured, attempt WiFi connection.
//   2. On WiFi success, instantiate and return WiFi-backed transport.
//   3. On WiFi failure (or no SSID), use cellular transport.
//   4. Caller owns the returned ITransport* (delete when done).
// ---------------------------------------------------------------------------

enum class ConnPath {
    NONE,
    WIFI,
    CELLULAR,
};

class ConnectivityManager {
public:
    ConnectivityManager() = default;

    // Attempt to connect. Returns the active path.
    ConnPath connect();

    // Disconnect cleanly.
    void disconnect();

    // Returns the currently active connectivity path.
    ConnPath activePath() const { return _path; }

    // Returns true if any path is connected.
    bool isConnected() const { return _path != ConnPath::NONE; }

    // Signal strength in dBm (cellular only; -1 for WiFi path).
    int getSignalDbm() const;

    // Human-readable path name.
    static const char* pathName(ConnPath p);

private:
    ConnPath _path = ConnPath::NONE;

#ifndef NATIVE_BUILD
    bool tryWiFi();
    bool tryCellular();
#endif
};
