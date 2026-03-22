#pragma once

#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// IModem – Hardware Abstraction Interface for Cellular Modem
// ---------------------------------------------------------------------------

class IModem {
public:
    virtual ~IModem() = default;

    // Initialize and power on the modem.
    virtual bool begin() = 0;

    // Connect to the cellular network using the configured APN.
    // Returns true when the modem is registered and data is available.
    virtual bool connect() = 0;

    // Disconnect from cellular network (does NOT power off modem).
    virtual void disconnect() = 0;

    // Power off the modem (for deep sleep).
    virtual void powerOff() = 0;

    // Send an HTTP/HTTPS POST request (TLS offloaded to modem).
    // url:        full URL including scheme
    // body:       request body (JSON)
    // bodyLen:    length of body in bytes
    // authHeader: optional Bearer token (pass nullptr to omit)
    // hmacSig:    optional HMAC-SHA256 hex signature (pass nullptr to omit);
    //             sent as X-OBDcast-Signature header when non-null/non-empty
    // Returns HTTP response code, or -1 on error.
    virtual int httpPost(const char* url, const char* body, size_t bodyLen,
                         const char* authHeader,
                         const char* hmacSig = nullptr) = 0;

    // Send raw bytes over TCP (used for MQTT).
    // Returns bytes sent, or -1 on error.
    virtual int tcpSend(const uint8_t* data, size_t len) = 0;

    // Receive raw bytes from TCP.
    // Returns bytes received, or -1 on error.
    virtual int tcpRecv(uint8_t* buf, size_t bufLen, uint32_t timeoutMs) = 0;

    // Open a TCP connection. Returns true on success.
    virtual bool tcpConnect(const char* host, uint16_t port, bool useTls) = 0;

    // Close the TCP connection.
    virtual void tcpClose() = 0;

    // Signal strength in dBm. Returns 0 if unknown.
    virtual int getSignalDbm() = 0;

    // Returns true if registered on the network with data available.
    virtual bool isConnected() = 0;
};
