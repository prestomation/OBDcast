#pragma once

#include "ITransport.h"
#include "../config.h"

#ifndef NATIVE_BUILD
  #include <PubSubClient.h>
  #include <WiFiClient.h>
  #include "../hal/IModem.h"
#endif

// ---------------------------------------------------------------------------
// MQTTTransport – Publishes telemetry as JSON to an MQTT broker.
//
// Connectivity path:
//   - WiFi available  → uses WiFiClient + PubSubClient
//   - Cellular only   → uses SIM7600 AT+CMQTT* commands via IModem
// ---------------------------------------------------------------------------

class MQTTTransport : public ITransport {
public:
#ifndef NATIVE_BUILD
    // WiFi-backed constructor
    explicit MQTTTransport(WiFiClient& wifiClient);
    // Cellular-backed constructor
    explicit MQTTTransport(IModem& modem);
#else
    // Stub constructor for unit tests
    struct MockDeps {};
    explicit MQTTTransport(MockDeps&) {}
    bool _wifiMode   = true;
    bool _connected  = false;
#endif

    bool begin() override;
    bool send(const Payload& payload) override;
    bool isConnected() override;
    void end() override;
    const char* getName() override { return "MQTT"; }

private:
#ifndef NATIVE_BUILD
    bool         _wifiMode = false;
    WiFiClient*  _wifiClient = nullptr;
    IModem*      _modem      = nullptr;
    PubSubClient _mqtt;
    bool         _connected  = false;

    bool connectViaCellular();
    bool connectViaWiFi();
    bool publishViaCellular(const char* topic, const char* payload, size_t payloadLen);
#endif

    // Build topic strings
    void makeTopic(char* buf, size_t bufLen, const char* suffix) const;
};
