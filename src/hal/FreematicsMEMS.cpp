#ifndef NATIVE_BUILD

#include "FreematicsMEMS.h"
#include "../config.h"

bool FreematicsMEMSImpl::begin() {
    if (_mems.begin() == 0) {
        LOG("MEMS: init failed");
        return false;
    }
    LOG("MEMS: initialized");
    return true;
}

bool FreematicsMEMSImpl::getAccel(float& x, float& y, float& z) {
    float acc[3] = {0};
    if (_mems.read(acc)) {
        _ax = acc[0];
        _ay = acc[1];
        _az = acc[2];
        x = _ax;
        y = _ay;
        z = _az;
        return true;
    }
    return false;
}

float FreematicsMEMSImpl::getMagnitude() {
    return sqrtf(_ax * _ax + _ay * _ay + _az * _az);
}

#endif // !NATIVE_BUILD
