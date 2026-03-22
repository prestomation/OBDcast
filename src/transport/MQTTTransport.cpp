#ifndef NATIVE_BUILD

#include "MQTTTransport.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

MQTTTransport::MQTTTransport(Client& wifiClient)
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

    char lwt[128];
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
// Cellular path – hand-rolled MQTT 3.1.1 CONNECT over AT TCP
// ---------------------------------------------------------------------------

// Write a MQTT variable-length integer (up to 4 bytes).
// Returns number of bytes written.
static size_t writeVarInt(uint8_t* buf, uint32_t value) {
    size_t n = 0;
    do {
        uint8_t byte = (uint8_t)(value & 0x7F);
        value >>= 7;
        if (value > 0) byte |= 0x80;
        buf[n++] = byte;
    } while (value > 0 && n < 4);
    return n;
}

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

    // Build MQTT 3.1.1 CONNECT packet
    // Fixed header:   1 byte (type) + VarInt remaining length (≤4B)
    // Variable header: 10 bytes
    // Payload:        2 + len(clientId) [+ 2+len(user) + 2+len(pass) if configured]
    size_t clientIdLen = strlen(DEVICE_ID);
    size_t userLen = strlen(MQTT_USER);
    size_t passLen = strlen(MQTT_PASS);
    bool hasUser = (userLen > 0);
    bool hasPass = (passLen > 0);

    uint32_t remainLen = 10 + 2 + (uint32_t)clientIdLen;
    if (hasUser) remainLen += 2 + (uint32_t)userLen;
    if (hasPass) remainLen += 2 + (uint32_t)passLen;

    // Build connect flags byte
    uint8_t connectFlags = 0x02; // clean session
    if (hasUser) connectFlags |= 0x80; // username flag
    if (hasPass) connectFlags |= 0x40; // password flag

    // Max packet size: 4 (fixed hdr) + 10 (var hdr) + 2 + 256 (clientId)
    //                  + 2 + 256 (user) + 2 + 256 (pass)
    static const size_t PKT_MAX = 4 + 10 + 2 + 256 + 2 + 256 + 2 + 256;
    uint8_t pkt[PKT_MAX];
    size_t  idx = 0;

    // Fixed header
    pkt[idx++] = 0x10; // CONNECT
    idx += writeVarInt(&pkt[idx], remainLen);

    // Protocol name "MQTT"
    pkt[idx++] = 0x00; pkt[idx++] = 0x04;
    pkt[idx++] = 'M'; pkt[idx++] = 'Q';
    pkt[idx++] = 'T'; pkt[idx++] = 'T';
    // Protocol version 4 (3.1.1)
    pkt[idx++] = 0x04;
    // Connect flags
    pkt[idx++] = connectFlags;
    // Keep alive: 60 seconds
    pkt[idx++] = 0x00; pkt[idx++] = 60;

    // Client ID (length-prefixed)
    if (idx + 2 + clientIdLen > PKT_MAX) {
        LOG("MQTT(Cellular): client ID too long");
        _modem->tcpClose();
        return false;
    }
    pkt[idx++] = (uint8_t)(clientIdLen >> 8);
    pkt[idx++] = (uint8_t)(clientIdLen & 0xFF);
    memcpy(&pkt[idx], DEVICE_ID, clientIdLen);
    idx += clientIdLen;

    // Username (if configured)
    if (hasUser) {
        if (idx + 2 + userLen > PKT_MAX) {
            LOG("MQTT(Cellular): username too long");
            _modem->tcpClose();
            return false;
        }
        pkt[idx++] = (uint8_t)(userLen >> 8);
        pkt[idx++] = (uint8_t)(userLen & 0xFF);
        memcpy(&pkt[idx], MQTT_USER, userLen);
        idx += userLen;
    }

    // Password (if configured)
    if (hasPass) {
        if (idx + 2 + passLen > PKT_MAX) {
            LOG("MQTT(Cellular): password too long");
            _modem->tcpClose();
            return false;
        }
        pkt[idx++] = (uint8_t)(passLen >> 8);
        pkt[idx++] = (uint8_t)(passLen & 0xFF);
        memcpy(&pkt[idx], MQTT_PASS, passLen);
        idx += passLen;
    }

    if (_modem->tcpSend(pkt, idx) < 0) {
        LOG("MQTT(Cellular): CONNECT send failed");
        _modem->tcpClose();
        return false;
    }

    // Read CONNACK (fixed: 4 bytes: 0x20 0x02 0x00 returnCode)
    // TCP is a stream — loop to handle partial delivery from modem
    uint8_t ack[4] = {};
    int total = 0;
    uint32_t deadline = millis() + 5000;
    while (total < 4 && millis() < deadline) {
        int n = _modem->tcpRecv(ack + total, 4 - total, 200);
        if (n > 0) total += n;
    }
    if (total < 4 || ack[0] != 0x20 || ack[1] != 0x02 || ack[3] != 0x00) {
        LOGF("MQTT(Cellular): CONNACK failed (got=%d rc=%d)", total,
             (total >= 4) ? ack[3] : -1);
        _modem->tcpClose();
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
        bool ok = _mqtt.publish(topic, (const uint8_t*)jsonBuf, (unsigned int)len, false);
        if (!ok) {
            LOG("MQTT(WiFi): publish failed");
        }
        return ok;
    } else {
        return publishViaCellular(topic, jsonBuf, len);
    }
}

// ---------------------------------------------------------------------------
// sendRaw – send pre-serialized JSON (used for SD buffer replay)
// ---------------------------------------------------------------------------
bool MQTTTransport::sendRaw(const char* json, size_t len) {
    char topic[128];
    makeTopic(topic, sizeof(topic), "telemetry");

    if (_wifiMode) {
        if (!_mqtt.connected()) connectViaWiFi();
        _mqtt.loop();
        return _mqtt.publish(topic, (const uint8_t*)json, (unsigned int)len, false);
    } else {
        return publishViaCellular(topic, json, len);
    }
}

bool MQTTTransport::publishViaCellular(const char* topic, const char* payload,
                                        size_t payloadLen) {
    size_t topicLen  = strlen(topic);
    // PUBLISH remaining length = 2 (topic len) + topicLen + payloadLen
    // (QoS 0: no packet ID)
    uint32_t remainLen = (uint32_t)(2 + topicLen + payloadLen);

    // Allocate packet on heap to support large payloads
    // Fixed header: 1 (type) + up to 4 (VarInt remaining length)
    size_t pktMax = 1 + 4 + 2 + topicLen + payloadLen;
    uint8_t* pkt  = new uint8_t[pktMax];
    if (!pkt) {
        LOG("MQTT(Cellular): publish alloc failed");
        return false;
    }

    size_t idx = 0;
    pkt[idx++] = 0x30; // PUBLISH, QoS 0, no retain, no dup
    idx += writeVarInt(&pkt[idx], remainLen);

    // Topic (length-prefixed)
    pkt[idx++] = (uint8_t)(topicLen >> 8);
    pkt[idx++] = (uint8_t)(topicLen & 0xFF);
    memcpy(&pkt[idx], topic, topicLen);
    idx += topicLen;

    // Payload
    memcpy(&pkt[idx], payload, payloadLen);
    idx += payloadLen;

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
        char lwt[128];
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
