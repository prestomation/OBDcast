#pragma once

#include "ITransport.h"
#include "../config.h"

#ifndef NATIVE_BUILD
  #include "../hal/IModem.h"
  #include <WiFiClientSecure.h>
#endif

// ---------------------------------------------------------------------------
// WebhookTransport – POSTs JSON telemetry via HTTPS.
//
// Connectivity path:
//   - WiFi available  → WiFiClientSecure (ESP32 native TLS)
//   - Cellular only   → SIM7600 AT+HTTP* commands (modem TLS offload)
// ---------------------------------------------------------------------------

class WebhookTransport : public ITransport {
public:
#ifndef NATIVE_BUILD
    // WiFi-backed constructor
    struct WiFiTag {};
    WebhookTransport(WiFiClientSecure& client, WiFiTag);
    // Cellular-backed constructor
    explicit WebhookTransport(IModem& modem);
#else
    struct MockDeps {};
    explicit WebhookTransport(MockDeps&) {}
    bool _wifiMode  = true;
    bool _connected = false;
#endif

    bool begin() override;
    bool send(const Payload& payload) override;
    bool isConnected() override;
    void end() override;
    const char* getName() override { return "Webhook"; }

private:
#ifndef NATIVE_BUILD
    bool              _wifiMode   = false;
    WiFiClientSecure* _wifiClient = nullptr;
    IModem*           _modem      = nullptr;
    bool              _connected  = false;
#endif
};
