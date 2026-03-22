#pragma once

// ---------------------------------------------------------------------------
// IGNSS – Hardware Abstraction Interface for GNSS / GPS
// ---------------------------------------------------------------------------

class IGNSS {
public:
    virtual ~IGNSS() = default;

    // Initialize the GNSS receiver. Returns true on success.
    virtual bool begin() = 0;

    // Poll for a new fix. Should be called at GPS_POLL_INTERVAL_MS.
    // Returns true if the data was refreshed with a valid fix.
    virtual bool update() = 0;

    // Populate the caller-supplied variables with the latest fix data.
    // Returns true if a valid fix is available.
    virtual bool getLocation(float& lat, float& lon, float& alt,
                             float& hdop, float& speedKph,
                             float& heading, int& satellites) = 0;

    // Returns true if a valid fix is currently available.
    virtual bool hasFix() = 0;
};
