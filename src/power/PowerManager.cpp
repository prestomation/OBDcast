#include "PowerManager.h"

#ifndef NATIVE_BUILD
  #include <Arduino.h>
#else
  #include <cstdint>
  #include <chrono>
  static uint32_t millis_stub() {
      using namespace std::chrono;
      static auto start = steady_clock::now();
      return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - start).count();
  }
#endif

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
PowerManager::PowerManager(float activeThreshold, float standbyThreshold,
                            uint32_t standbyPingMs, uint32_t standbyIdleMs)
    : _state(PowerState::ACTIVE),
      _activeThreshold(activeThreshold),
      _standbyThreshold(standbyThreshold),
      _standbyPingMs(standbyPingMs),
      _standbyIdleMs(standbyIdleMs)
{}

// ---------------------------------------------------------------------------
// now() – thin wrapper so tests can control time via subclass
// ---------------------------------------------------------------------------
uint32_t PowerManager::now() const {
#ifndef NATIVE_BUILD
    return millis();
#else
    return millis_stub();
#endif
}

// ---------------------------------------------------------------------------
// update
// ---------------------------------------------------------------------------
PowerState PowerManager::update(float voltage, bool motionDetected) {
    PowerState prev = _state;

    switch (_state) {
    case PowerState::ACTIVE:
        if (voltage < _standbyThreshold) {
            // Battery critically low — skip standby, go straight to sleep
            _state = PowerState::DEEP_SLEEP;
        } else if (voltage < _activeThreshold) {
            _state = PowerState::STANDBY;
            _standbyEnteredMs = now();
        }
        break;

    case PowerState::STANDBY:
        if (voltage >= _activeThreshold || motionDetected) {
            // Engine restarted or motion suggests someone got in
            _state = PowerState::ACTIVE;
        } else if (voltage < _standbyThreshold) {
            // Battery draining — go to deep sleep
            _state = PowerState::DEEP_SLEEP;
        } else {
            // Check idle timeout
            uint32_t inStandby = now() - _standbyEnteredMs;
            if (inStandby >= _standbyIdleMs) {
                _state = PowerState::DEEP_SLEEP;
            }
        }
        break;

    case PowerState::DEEP_SLEEP:
        // Wake is handled externally (timer / motion ISR).
        // After waking, caller should call onWake() then update() again.
        // In deep sleep we just stay here until onWake() is called.
        break;
    }

    return _state;
}

// ---------------------------------------------------------------------------
// shouldSendStandbyPing
// ---------------------------------------------------------------------------
bool PowerManager::shouldSendStandbyPing() {
    if (_state != PowerState::STANDBY) return false;
    uint32_t elapsed = now() - _lastPingMs;
    if (elapsed >= _standbyPingMs) {
        _lastPingMs = now();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// shouldCollect
// ---------------------------------------------------------------------------
bool PowerManager::shouldCollect(uint32_t intervalMs) {
    if (_state != PowerState::ACTIVE) return false;
    // Fire immediately on first call (_lastCollectMs == UINT32_MAX sentinel)
    uint32_t t = now();
    uint32_t elapsed = (_lastCollectMs == UINT32_MAX) ? intervalMs : (t - _lastCollectMs);
    if (elapsed >= intervalMs) {
        _lastCollectMs = now();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// enterDeepSleep / onWake
// ---------------------------------------------------------------------------
void PowerManager::enterDeepSleep() {
    _state = PowerState::DEEP_SLEEP;
}

void PowerManager::onWake() {
    // Re-enter ACTIVE; update() will correct to STANDBY/SLEEP if needed
    _state = PowerState::ACTIVE;
    _standbyEnteredMs = 0;
}

// ---------------------------------------------------------------------------
// stateName
// ---------------------------------------------------------------------------
const char* PowerManager::stateName(PowerState s) {
    switch (s) {
    case PowerState::ACTIVE:     return "ACTIVE";
    case PowerState::STANDBY:    return "STANDBY";
    case PowerState::DEEP_SLEEP: return "DEEP_SLEEP";
    default:                     return "UNKNOWN";
    }
}
