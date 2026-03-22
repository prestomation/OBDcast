#ifndef NATIVE_BUILD

#include "FreematicsGNSS.h"
#include "../config.h"

bool FreematicsGNSS::begin() {
    // Enable GPS on the SIM7600 modem
    if (!_modem.gpsOn()) {
        LOG("GNSS: failed to enable GPS");
        return false;
    }
    LOG("GNSS: GPS enabled");
    return true;
}

bool FreematicsGNSS::update() {
    GPS_DATA g{};
    if (_modem.getGPSInfo(g)) {
        _gpsData = g;
        _hasFix  = (g.satellites >= 1);
        return _hasFix;
    }
    _hasFix = false;
    return false;
}

bool FreematicsGNSS::getLocation(float& lat, float& lon, float& alt,
                                  float& hdop, float& speedKph,
                                  float& heading, int& satellites) {
    if (!_hasFix) return false;
    lat        = _gpsData.lat;
    lon        = _gpsData.lng;
    alt        = _gpsData.alt;
    hdop       = _gpsData.hdop;
    speedKph   = _gpsData.speed;
    heading    = _gpsData.heading;
    satellites = _gpsData.satellites;
    return true;
}

bool FreematicsGNSS::hasFix() {
    return _hasFix;
}

#endif // !NATIVE_BUILD
