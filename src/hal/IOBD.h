#pragma once

// ---------------------------------------------------------------------------
// IOBD – Hardware Abstraction Interface for OBD-II
// ---------------------------------------------------------------------------
// Implementations must be thread-safe if called from multiple tasks.
// ---------------------------------------------------------------------------

class IOBD {
public:
    virtual ~IOBD() = default;

    // Initialize the OBD interface. Returns true on success.
    virtual bool begin() = 0;

    // Read a single OBD-II PID.
    // pid:   2-byte PID (e.g., 0x0C for RPM)
    // value: output, scaled integer (RPM in RPM, speed in km/h, etc.)
    // Returns true if the PID was successfully read.
    virtual bool readPID(uint16_t pid, int& value) = 0;

    // Read battery/OBD rail voltage in volts.
    // Returns 0.0 on failure.
    virtual float readVoltage() = 0;

    // Returns true if the OBD adapter is connected to the vehicle ECU.
    virtual bool isConnected() = 0;
};
