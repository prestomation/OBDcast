#include "SDBuffer.h"
#include "../config.h"
#include <string.h>

#ifndef NATIVE_BUILD
  #include <SD.h>
  #include <Arduino.h>
#endif

// ---------------------------------------------------------------------------
// begin
// ---------------------------------------------------------------------------
bool SDBuffer::begin() {
#ifndef NATIVE_BUILD
    if (!SD_BUFFER_ENABLED) return false;

    if (!SD.begin()) {
        LOG("SDBuffer: SD init failed");
        _available = false;
        return false;
    }

    LOG("SDBuffer: SD ready");
    _available = true;
    updateCount();
    return true;
#else
    _available = true;
    return true;
#endif
}

// ---------------------------------------------------------------------------
// write
// ---------------------------------------------------------------------------
bool SDBuffer::write(const Payload& payload) {
    if (!_available) return false;

    char jsonBuf[1024];
    size_t len = payload.toJson(jsonBuf, sizeof(jsonBuf));
    if (len == 0) return false;

#ifndef NATIVE_BUILD
    // Enforce max buffer size to prevent SD exhaustion
    if (sizeBytes() >= (size_t)SD_BUFFER_MAX_SIZE_MB * 1024 * 1024) {
        LOG("SDBuffer: max size reached, dropping oldest record");
        // Simple policy: drop write (user should increase SD_BUFFER_MAX_SIZE_MB
        // or reduce DATA_INTERVAL_MS if offline for extended periods)
        return false;
    }

    File f = SD.open(SD_BUFFER_FILE_PATH, FILE_APPEND);
    if (!f) {
        LOG("SDBuffer: open for write failed");
        return false;
    }
    size_t written = f.println(jsonBuf);
    f.close();
    if (written == 0) {
        LOG("SDBuffer: write failed");
        return false;
    }
    _pendingCount++;
    return true;
#else
    // Native stub — just increment counter
    _pendingCount++;
    return true;
#endif
}

// ---------------------------------------------------------------------------
// hasPending
// ---------------------------------------------------------------------------
bool SDBuffer::hasPending() const {
    return _available && _pendingCount > 0;
}

// ---------------------------------------------------------------------------
// readNext
// ---------------------------------------------------------------------------
bool SDBuffer::readNext(char* buf, size_t bufLen) {
    if (!_available || _pendingCount == 0) return false;

#ifndef NATIVE_BUILD
    File f = SD.open(SD_BUFFER_FILE_PATH, FILE_READ);
    if (!f) return false;

    // Seek to current replay offset
    if (_replayOffset > 0) {
        f.seek(_replayOffset);
    }

    // Read one line, stripping \r\n so JSON parsing works correctly.
    // If a single line exceeds bufLen-1 bytes it is truncated to fit the
    // caller's buffer — the truncated JSON will fail to parse and the
    // caller should skip it. This is preferable to a buffer overflow.
    int  idx      = 0;
    bool overflow = false;
    while (f.available()) {
        char c = (char)f.read();
        if (c == '\n') break;
        if (c == '\r') continue; // strip carriage returns
        if (idx < (int)bufLen - 1) {
            buf[idx++] = c;
        } else {
            overflow = true; // consume but don't store
        }
    }
    buf[idx] = '\0';
    if (overflow) {
        LOG("SDBuffer: readNext truncated oversized record");
    }
    _replayOffset = f.position();
    f.close();

    return (idx > 0);
#else
    // Native stub
    snprintf(buf, bufLen, "{\"device_id\":\"stub\",\"ts\":0}");
    _pendingCount = (_pendingCount > 0) ? _pendingCount - 1 : 0;
    return true;
#endif
}

// ---------------------------------------------------------------------------
// clearReplayed
// ---------------------------------------------------------------------------
void SDBuffer::clearReplayed() {
#ifndef NATIVE_BUILD
    if (!_available) return;
    SD.remove(SD_BUFFER_FILE_PATH);
    _pendingCount  = 0;
    _replayOffset  = 0;
    LOG("SDBuffer: buffer cleared");
#else
    _pendingCount = 0;
    _replayOffset = 0;
#endif
}

// ---------------------------------------------------------------------------
// sizeBytes
// ---------------------------------------------------------------------------
size_t SDBuffer::sizeBytes() const {
#ifndef NATIVE_BUILD
    if (!_available) return 0;
    File f = SD.open(SD_BUFFER_FILE_PATH, FILE_READ);
    if (!f) return 0;
    size_t s = f.size();
    f.close();
    return s;
#else
    return _pendingCount * 256; // rough estimate
#endif
}

// ---------------------------------------------------------------------------
// updateCount (hardware builds only)
// ---------------------------------------------------------------------------
#ifndef NATIVE_BUILD
void SDBuffer::updateCount() {
    File f = SD.open(SD_BUFFER_FILE_PATH, FILE_READ);
    if (!f) {
        _pendingCount = 0;
        return;
    }
    uint32_t count = 0;
    while (f.available()) {
        char c = (char)f.read();
        if (c == '\n') count++;
    }
    _pendingCount = count;
    f.close();
}
#endif
