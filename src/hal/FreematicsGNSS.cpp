#ifndef NATIVE_BUILD

#include "FreematicsGNSS.h"
#include "../config.h"

bool FreematicsGNSS::begin() {
    // Enable GPS on the hardware
    if (!_hal.gpsBegin()) {
        LOG("GNSS: failed to enable GPS");
        return false;
    }
    LOG("GNSS: GPS enabled");
    return true;
}

bool FreematicsGNSS::update() {
    GPS_DATA* pgd = nullptr;
    if (_hal.gpsGetData(&pgd) && pgd) {
        _gpsData = *pgd;
        _hasFix  = (_gpsData.sat >= 1);
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
    hdop       = (float)_gpsData.hdop;
    speedKph   = _gpsData.speed * 1.852f; // knots to km/h
    heading    = (float)_gpsData.heading;
    satellites = (int)_gpsData.sat;
    return true;
}

bool FreematicsGNSS::hasFix() {
    return _hasFix;
}

#endif // !NATIVE_BUILD
