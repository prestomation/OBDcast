#pragma once

#ifndef NATIVE_BUILD

#include "IOBD.h"
#include <FreematicsPlus.h>

// ---------------------------------------------------------------------------
// FreematicsOBD – IOBD implementation using FreematicsPlus library
// ---------------------------------------------------------------------------

class FreematicsOBDImpl : public IOBD {
public:
    explicit FreematicsOBDImpl(FreematicsESP32& hal) : _hal(hal) {}

    bool begin() override;
    bool readPID(uint16_t pid, int& value) override;
    float readVoltage() override;
    bool isConnected() override;

private:
    FreematicsESP32& _hal;
    bool _connected = false;
};

#endif // !NATIVE_BUILD
