#pragma once

#include "../Payload.h"
#include "../hal/IOBD.h"
#include "../hal/IGNSS.h"
#include "../hal/IMEMS.h"

// ---------------------------------------------------------------------------
// DataCollector – Aggregates readings from all sensors into a Payload.
// ---------------------------------------------------------------------------

class DataCollector {
public:
    DataCollector(IOBD& obd, IGNSS& gnss, IMEMS& mems)
        : _obd(obd), _gnss(gnss), _mems(mems) {}

    // Collect all available data and populate payload.
    // signalDbm: current modem signal strength (from ConnectivityManager).
    // Returns true if at least OBD voltage was read successfully.
    bool collect(Payload& out, int signalDbm = 0);

    // Collect only a minimal ping payload (for STANDBY mode).
    void collectPing(Payload& out, int signalDbm = 0);

private:
    IOBD&  _obd;
    IGNSS& _gnss;
    IMEMS& _mems;

    void collectOBD(Payload& out);
    void collectGPS(Payload& out);
    void collectMEMS(Payload& out);
    uint32_t getTimestamp();
};
