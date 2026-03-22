#pragma once

#ifndef NATIVE_BUILD

#include "IGNSS.h"
#include <FreematicsPlus.h>

// ---------------------------------------------------------------------------
// FreematicsGNSS – IGNSS implementation backed by FreematicsESP32 GPS engine
// ---------------------------------------------------------------------------

class FreematicsGNSS : public IGNSS {
public:
    explicit FreematicsGNSS(FreematicsESP32& hal) : _hal(hal) {}

    bool begin() override;
    bool update() override;
    bool getLocation(float& lat, float& lon, float& alt,
                     float& hdop, float& speedKph,
                     float& heading, int& satellites) override;
    bool hasFix() override;

private:
    FreematicsESP32& _hal;
    GPS_DATA         _gpsData{};
    bool             _hasFix = false;
};

#endif // !NATIVE_BUILD
