#pragma once

#ifndef NATIVE_BUILD

#include "IModem.h"
#include <FreematicsPlus.h>

// ---------------------------------------------------------------------------
// SIM7600Modem – IModem implementation wrapping CellularSIM7600 AT commands
//
// TLS offload strategy:
//   - HTTPS: AT+HTTPINIT / AT+HTTPPARA / AT+HTTPSSL=1 / AT+HTTPACTION
//   - MQTT:  AT+CMQTTSTART / AT+CMQTTCONNECT / AT+CMQTTPUB
// ---------------------------------------------------------------------------

class SIM7600Modem : public IModem {
public:
    explicit SIM7600Modem(FreematicsESP32& hal) : _hal(hal) {}

    bool begin() override;
    bool connect() override;
    void disconnect() override;
    void powerOff() override;

    int httpPost(const char* url, const char* body, size_t bodyLen,
                 const char* authHeader) override;

    int  tcpSend(const uint8_t* data, size_t len) override;
    int  tcpRecv(uint8_t* buf, size_t bufLen, uint32_t timeoutMs) override;
    bool tcpConnect(const char* host, uint16_t port, bool useTls) override;
    void tcpClose() override;

    int  getSignalDbm() override;
    bool isConnected() override;

    // Expose the underlying modem for GNSS / MQTT direct use
    CellularSIM7600& modem() { return _modem; }

private:
    FreematicsESP32&  _hal;
    CellularSIM7600   _modem;
    bool              _connected = false;

    bool sendAT(const char* cmd, const char* expected,
                uint32_t timeoutMs = 5000);
};

#endif // !NATIVE_BUILD
