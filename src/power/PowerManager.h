#pragma once

#include <stdint.h>
#include <limits.h>

// ---------------------------------------------------------------------------
// PowerManager – Tiered power state machine
//
// State transitions:
//   ACTIVE    → STANDBY     : voltage drops below VOLTAGE_ACTIVE_THRESHOLD
//   STANDBY   → ACTIVE      : voltage rises above VOLTAGE_ACTIVE_THRESHOLD
//   STANDBY   → DEEP_SLEEP  : voltage drops below VOLTAGE_STANDBY_THRESHOLD
//                             OR standby idle timeout exceeded
//   DEEP_SLEEP → ACTIVE     : wake via motion or timer (re-evaluate voltage)
// ---------------------------------------------------------------------------

enum class PowerState {
    ACTIVE,
    STANDBY,
    DEEP_SLEEP,
};

class PowerManager {
public:
    // Construct with configurable thresholds for testability
    PowerManager(float activeThreshold    = 13.2f,
                 float standbyThreshold   = 12.2f,
                 uint32_t standbyPingMs   = 900000UL,
                 uint32_t standbyIdleMs   = 3600000UL);

    // Call periodically with current battery voltage (and optionally motion flag).
    // Returns the new state (may be the same as before).
    PowerState update(float voltage, bool motionDetected = false);

    // Current state accessor.
    PowerState getState() const { return _state; }

    // Returns true if it's time to send a standby ping.
    bool shouldSendStandbyPing();

    // Returns true if it's time to collect and transmit in ACTIVE mode.
    bool shouldCollect(uint32_t intervalMs);

    // Force entry into deep sleep (called by external logic).
    void enterDeepSleep();

    // Called when waking from deep sleep.
    void onWake();

    // Human-readable state name.
    static const char* stateName(PowerState s);

private:
    PowerState _state;
    float      _activeThreshold;
    float      _standbyThreshold;
    uint32_t   _standbyPingMs;
    uint32_t   _standbyIdleMs;

    uint32_t   _lastPingMs      = 0;
    uint32_t   _lastCollectMs   = UINT32_MAX; // UINT32_MAX = collect immediately on first call
    uint32_t   _standbyEnteredMs= 0;

    // Millis abstraction (overridable for tests via subclass / function pointer)
    virtual uint32_t now() const;
};
