#pragma once

#ifndef NATIVE_BUILD

#include "IGNSS.h"
#include <FreematicsPlus.h>

// ---------------------------------------------------------------------------
// FreematicsGNSS – IGNSS implementation backed by SIM7600 GNSS engine
// via FreematicsPlus (CellularSIM7600 / GPS methods)
// ---------------------------------------------------------------------------

class FreematicsGNSS : public IGNSS {
public:
    explicit FreematicsGNSS(CellularSIM7600& modem) : _modem(modem) {}

    bool begin() override;
    bool update() override;
    bool getLocation(float& lat, float& lon, float& alt,
                     float& hdop, float& speedKph,
                     float& heading, int& satellites) override;
    bool hasFix() override;

private:
    CellularSIM7600& _modem;
    GPS_DATA         _gpsData{};
    bool             _hasFix = false;
};

#endif // !NATIVE_BUILD
