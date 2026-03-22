#ifndef NATIVE_BUILD

#include "MQTTTransport.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

MQTTTransport::MQTTTransport(WiFiClient& wifiClient)
    : _wifiMode(true), _wifiClient(&wifiClient), _mqtt(wifiClient) {}

MQTTTransport::MQTTTransport(IModem& modem)
    : _wifiMode(false), _modem(&modem) {}

// ---------------------------------------------------------------------------
// begin
// ---------------------------------------------------------------------------
bool MQTTTransport::begin() {
    if (_wifiMode) {
        return connectViaWiFi();
    } else {
        return connectViaCellular();
    }
}

// ---------------------------------------------------------------------------
// WiFi path – PubSubClient
// ---------------------------------------------------------------------------
bool MQTTTransport::connectViaWiFi() {
    _mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    _mqtt.setKeepAlive(60);

    char lwt[64];
    makeTopic(lwt, sizeof(lwt), "status");

    const char* user = (strlen(MQTT_USER) > 0) ? MQTT_USER : nullptr;
    const char* pass = (strlen(MQTT_PASS) > 0) ? MQTT_PASS : nullptr;

    if (!_mqtt.connect(DEVICE_ID, user, pass,
                        lwt, MQTT_QOS, true, "{\"status\":\"offline\"}")) {
        LOGF("MQTT(WiFi): connect failed, rc=%d", _mqtt.state());
        return false;
    }

    // Publish online status
    _mqtt.publish(lwt, "{\"status\":\"online\"}", true);
    LOG("MQTT(WiFi): connected");
    _connected = true;
    return true;
}

// ---------------------------------------------------------------------------
// Cellular path – AT+CMQTT* commands
// ---------------------------------------------------------------------------
bool MQTTTransport::connectViaCellular() {
    if (!_modem || !_modem->isConnected()) {
        LOG("MQTT(Cellular): modem not connected");
        return false;
    }

    // Open a TCP connection to the broker
    if (!_modem->tcpConnect(MQTT_BROKER, MQTT_PORT, MQTT_USE_TLS)) {
        LOG("MQTT(Cellular): TCP connect failed");
        return false;
    }

    // Build and send MQTT CONNECT packet manually
    // (Minimal MQTT 3.1.1 CONNECT)
    uint8_t connectPkt[128];
    size_t  idx = 0;
    size_t  clientIdLen = strlen(DEVICE_ID);
    uint16_t remainLen  = 10 + 2 + clientIdLen; // fixed header + client ID

    connectPkt[idx++] = 0x10; // CONNECT
    connectPkt[idx++] = (uint8_t)remainLen;
    // Protocol name
    connectPkt[idx++] = 0x00; connectPkt[idx++] = 0x04;
    connectPkt[idx++] = 'M'; connectPkt[idx++] = 'Q';
    connectPkt[idx++] = 'T'; connectPkt[idx++] = 'T';
    // Protocol version
    connectPkt[idx++] = 0x04;
    // Connect flags: clean session
    connectPkt[idx++] = 0x02;
    // Keep alive (60 seconds)
    connectPkt[idx++] = 0x00; connectPkt[idx++] = 60;
    // Client ID
    connectPkt[idx++] = (uint8_t)(clientIdLen >> 8);
    connectPkt[idx++] = (uint8_t)(clientIdLen & 0xFF);
    memcpy(&connectPkt[idx], DEVICE_ID, clientIdLen);
    idx += clientIdLen;

    if (_modem->tcpSend(connectPkt, idx) < 0) {
        LOG("MQTT(Cellular): CONNECT send failed");
        return false;
    }

    // Read CONNACK
    uint8_t ack[4];
    if (_modem->tcpRecv(ack, sizeof(ack), 5000) < 4 ||
        ack[0] != 0x20 || ack[3] != 0x00) {
        LOG("MQTT(Cellular): CONNACK bad");
        return false;
    }

    LOG("MQTT(Cellular): connected");
    _connected = true;
    return true;
}

// ---------------------------------------------------------------------------
// send
// ---------------------------------------------------------------------------
bool MQTTTransport::send(const Payload& payload) {
    char jsonBuf[1024];
    size_t len = payload.toJson(jsonBuf, sizeof(jsonBuf));
    if (len == 0) {
        LOG("MQTT: JSON serialization failed");
        return false;
    }

    char topic[128];
    makeTopic(topic, sizeof(topic), "telemetry");

    if (_wifiMode) {
        if (!_mqtt.connected()) {
            connectViaWiFi();
        }
        _mqtt.loop();
        bool ok = _mqtt.publish(topic, (const uint8_t*)jsonBuf, len, false);
        if (!ok) {
            LOG("MQTT(WiFi): publish failed");
        }
        return ok;
    } else {
        return publishViaCellular(topic, jsonBuf);
    }
}

bool MQTTTransport::publishViaCellular(const char* topic, const char* payload) {
    size_t topicLen   = strlen(topic);
    size_t payloadLen = strlen(payload);

    // Build MQTT PUBLISH packet (QoS 0, no packet ID)
    size_t pktLen = 2 + topicLen + payloadLen + 2; // header + topic len + topic + payload
    uint8_t* pkt  = new uint8_t[pktLen + 4];
    size_t   idx  = 0;

    pkt[idx++] = 0x30; // PUBLISH, QoS 0, no retain
    // Variable-length remaining length (simplified: assume < 128 bytes total)
    size_t remaining = 2 + topicLen + payloadLen;
    pkt[idx++] = (uint8_t)(remaining & 0x7F);
    pkt[idx++] = (uint8_t)(topicLen >> 8);
    pkt[idx++] = (uint8_t)(topicLen & 0xFF);
    memcpy(&pkt[idx], topic, topicLen); idx += topicLen;
    memcpy(&pkt[idx], payload, payloadLen); idx += payloadLen;

    bool ok = (_modem->tcpSend(pkt, idx) > 0);
    delete[] pkt;

    if (!ok) {
        LOG("MQTT(Cellular): publish failed");
    }
    return ok;
}

// ---------------------------------------------------------------------------
// isConnected / end
// ---------------------------------------------------------------------------
bool MQTTTransport::isConnected() {
    if (_wifiMode) return _mqtt.connected();
    return _connected;
}

void MQTTTransport::end() {
    if (_wifiMode) {
        char lwt[64];
        makeTopic(lwt, sizeof(lwt), "status");
        _mqtt.publish(lwt, "{\"status\":\"offline\"}", true);
        _mqtt.disconnect();
    } else if (_modem) {
        _modem->tcpClose();
    }
    _connected = false;
}

// ---------------------------------------------------------------------------
// makeTopic
// ---------------------------------------------------------------------------
void MQTTTransport::makeTopic(char* buf, size_t bufLen, const char* suffix) const {
    snprintf(buf, bufLen, "%s/%s/%s", MQTT_TOPIC_PREFIX, DEVICE_ID, suffix);
}

#endif // !NATIVE_BUILD
