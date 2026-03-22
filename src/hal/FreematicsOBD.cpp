#ifndef NATIVE_BUILD

#include "FreematicsOBD.h"
#include "../config.h"

bool FreematicsOBDImpl::begin() {
    // Initialize the OBD-II interface via FreematicsPlus
    // The library handles protocol detection (CAN, ISO, KWP)
    byte retries = 3;
    while (retries-- > 0) {
        if (_hal.init()) {
            LOG("OBD: initialized");
            _connected = true;
            return true;
        }
        delay(1000);
    }
    LOG("OBD: init failed");
    _connected = false;
    return false;
}

bool FreematicsOBDImpl::readPID(uint16_t pid, int& value) {
    if (!_connected) return false;
    // FreematicsPlus uses int pid; cast is safe for standard PIDs
    int v = 0;
    if (_hal.readPID(static_cast<byte>(pid), v)) {
        value = v;
        return true;
    }
    return false;
}

float FreematicsOBDImpl::readVoltage() {
    // FreematicsPlus provides battery voltage via analogRead + divider
    return _hal.getVoltage();
}

bool FreematicsOBDImpl::isConnected() {
    return _connected;
}

#endif // !NATIVE_BUILD
