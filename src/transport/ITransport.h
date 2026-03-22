#pragma once

#include "../Payload.h"

// ---------------------------------------------------------------------------
// ITransport – Abstract interface for telemetry transport
// ---------------------------------------------------------------------------

class ITransport {
public:
    virtual ~ITransport() = default;

    // Initialize the transport (connect to broker, validate URL, etc.)
    // Returns true on success.
    virtual bool begin() = 0;

    // Send a telemetry payload (serializes to JSON internally).
    // Returns true on success.
    virtual bool send(const Payload& payload) = 0;

    // Send a pre-serialized JSON payload (used for SD buffer replay).
    // Returns true on success.
    virtual bool sendRaw(const char* json, size_t len) = 0;

    // Returns true if the transport is connected/ready to send.
    virtual bool isConnected() = 0;

    // Graceful shutdown / disconnect.
    virtual void end() = 0;

    // Human-readable transport name for logging.
    virtual const char* getName() = 0;
};
