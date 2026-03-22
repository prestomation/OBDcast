#pragma once

// ---------------------------------------------------------------------------
// IMEMS – Hardware Abstraction Interface for MEMS IMU
// ---------------------------------------------------------------------------

class IMEMS {
public:
    virtual ~IMEMS() = default;

    // Initialize the MEMS sensor. Returns true on success.
    virtual bool begin() = 0;

    // Read accelerometer data in g-force units.
    // Returns true on success.
    virtual bool getAccel(float& x, float& y, float& z) = 0;

    // Returns magnitude of last acceleration vector (useful for motion detection).
    virtual float getMagnitude() = 0;
};
