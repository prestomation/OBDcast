#ifndef NATIVE_BUILD

#include "FreematicsOBD.h"
#include "../config.h"

bool FreematicsOBDImpl::begin() {
    // Initialize the OBD-II interface via FreematicsPlus COBD
    _obd.begin(_hal.link);
    byte retries = 3;
    while (retries-- > 0) {
        if (_obd.init()) {
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
    int v = 0;
    if (_obd.readPID(static_cast<byte>(pid), v)) {
        value = v;
        return true;
    }
    return false;
}

float FreematicsOBDImpl::readVoltage() {
    return _obd.getVoltage();
}

bool FreematicsOBDImpl::isConnected() {
    return _connected;
}

#endif // !NATIVE_BUILD
