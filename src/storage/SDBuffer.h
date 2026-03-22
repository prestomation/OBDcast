#pragma once

#include "../Payload.h"
#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// SDBuffer – Stores telemetry payloads to SD card when offline.
//            Replays buffered payloads when connectivity is restored.
//
// Storage format: newline-delimited JSON (JSONL), one payload per line.
// ---------------------------------------------------------------------------

class SDBuffer {
public:
    SDBuffer() = default;

    // Initialize SD card. Returns false if SD is not available.
    bool begin();

    // Write a payload to the buffer file.
    // Returns true on success.
    bool write(const Payload& payload);

    // Returns true if there are buffered payloads to replay.
    bool hasPending() const;

    // Read the next buffered payload JSON into buf (null-terminated).
    // Advances the read pointer.
    // Returns true if a line was read.
    bool readNext(char* buf, size_t bufLen);

    // Remove all replayed / acknowledged entries from the buffer.
    void clearReplayed();

    // Returns approximate size of the buffer file in bytes.
    size_t sizeBytes() const;

    // Returns the number of buffered records.
    uint32_t pendingCount() const { return _pendingCount; }

private:
    bool     _available    = false;
    uint32_t _pendingCount = 0;
    uint32_t _replayOffset = 0; // byte offset for read pointer

#ifndef NATIVE_BUILD
    void updateCount();
#endif
};
