#pragma once

#ifndef NATIVE_BUILD

#include "IMEMS.h"
#include <FreematicsPlus.h>
#include <math.h>

// ---------------------------------------------------------------------------
// FreematicsMEMS – IMEMS implementation using FreematicsPlus ICM_42627
// ---------------------------------------------------------------------------

class FreematicsMEMSImpl : public IMEMS {
public:
    FreematicsMEMSImpl() = default;

    bool begin() override;
    bool getAccel(float& x, float& y, float& z) override;
    float getMagnitude() override;

private:
    ICM_42627 _mems;
    float     _ax = 0.f, _ay = 0.f, _az = 0.f;
};

#endif // !NATIVE_BUILD
