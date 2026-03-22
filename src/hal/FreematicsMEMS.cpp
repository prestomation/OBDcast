#ifndef NATIVE_BUILD

#include "FreematicsMEMS.h"
#include "../config.h"

bool FreematicsMEMSImpl::begin() {
    if (!_mems.begin()) {
        LOG("MEMS: init failed");
        return false;
    }
    LOG("MEMS: initialized");
    return true;
}

bool FreematicsMEMSImpl::getAccel(float& x, float& y, float& z) {
    float gyrox, gyroy, gyroz;
    float temp;
    if (_mems.read(_ax, _ay, _az, gyrox, gyroy, gyroz, &temp)) {
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
