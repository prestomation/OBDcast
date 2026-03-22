#ifndef NATIVE_BUILD

#include "WebhookTransport.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------
WebhookTransport::WebhookTransport(WiFiClientSecure& client, WiFiTag)
    : _wifiMode(true), _wifiClient(&client) {}

WebhookTransport::WebhookTransport(IModem& modem)
    : _wifiMode(false), _modem(&modem) {}

// ---------------------------------------------------------------------------
// begin
// ---------------------------------------------------------------------------
bool WebhookTransport::begin() {
    if (_wifiMode) {
        // NOTE: Certificate validation is disabled for initial deployment
        // flexibility. Users who can provide a CA cert should call
        // setCACert() / setCertificate() / setPrivateKey() on the
        // WiFiClientSecure instance before constructing WebhookTransport,
        // and remove setInsecure(). This is a known security trade-off —
        // see config.h comment on WEBHOOK_URL.
        _wifiClient->setInsecure();
        _connected = true;
        LOG("Webhook(WiFi): ready (TLS cert validation disabled — set CA cert to enable)");
    } else {
        _connected = (_modem && _modem->isConnected());
        LOG("Webhook(Cellular): ready");
    }
    return _connected;
}

// ---------------------------------------------------------------------------
// send
// ---------------------------------------------------------------------------
bool WebhookTransport::send(const Payload& payload) {
    char jsonBuf[1024];
    size_t jsonLen = payload.toJson(jsonBuf, sizeof(jsonBuf));
    if (jsonLen == 0) {
        LOG("Webhook: JSON serialization failed");
        return false;
    }

    const char* authToken = (strlen(WEBHOOK_AUTH_TOKEN) > 0) ? WEBHOOK_AUTH_TOKEN : nullptr;

    if (_wifiMode) {
        return sendViaWiFi(jsonBuf, jsonLen, authToken);
    } else {
        int code = _modem->httpPost(WEBHOOK_URL, jsonBuf, jsonLen, authToken);
        bool ok = (code >= 200 && code < 300);
        if (!ok) {
            LOGF("Webhook(Cellular): HTTP error %d", code);
        }
        return ok;
    }
}

// ---------------------------------------------------------------------------
// sendViaWiFi – parse URL, build HTTP/1.1 request, check response code
// ---------------------------------------------------------------------------
bool WebhookTransport::sendViaWiFi(const char* jsonBuf, size_t jsonLen,
                                    const char* authToken) {
    // Parse host and path from WEBHOOK_URL (expects https://host/path)
    const char* url = WEBHOOK_URL;
    const char* schemeEnd = strstr(url, "://");
    if (!schemeEnd) {
        LOG("Webhook(WiFi): invalid URL (no scheme)");
        return false;
    }
    const char* hostStart = schemeEnd + 3;
    const char* pathStart = strchr(hostStart, '/');

    char host[128];
    char path[256];

    if (pathStart) {
        size_t hostLen = (size_t)(pathStart - hostStart);
        if (hostLen >= sizeof(host)) {
            LOG("Webhook(WiFi): hostname too long");
            return false;
        }
        memcpy(host, hostStart, hostLen);
        host[hostLen] = '\0';
        strncpy(path, pathStart, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    } else {
        strncpy(host, hostStart, sizeof(host) - 1);
        host[sizeof(host) - 1] = '\0';
        strncpy(path, "/", sizeof(path) - 1);
    }

    if (!_wifiClient->connect(host, 443)) {
        LOG("Webhook(WiFi): TLS connect failed");
        return false;
    }

    // Build headers first, then body — avoids fixed-size request buffer overflow
    // Send header
    char header[512];
    int hlen;
    if (authToken) {
        hlen = snprintf(header, sizeof(header),
            "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: application/json\r\n"
            "Authorization: Bearer %s\r\n"
            "Content-Length: %u\r\n"
            "Connection: close\r\n"
            "\r\n",
            path, host, authToken, (unsigned)jsonLen);
    } else {
        hlen = snprintf(header, sizeof(header),
            "POST %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %u\r\n"
            "Connection: close\r\n"
            "\r\n",
            path, host, (unsigned)jsonLen);
    }

    if (hlen < 0 || (size_t)hlen >= sizeof(header)) {
        LOG("Webhook(WiFi): header buffer overflow");
        _wifiClient->stop();
        return false;
    }

    // Send header and body separately to avoid large combined buffer
    _wifiClient->write((const uint8_t*)header,  (size_t)hlen);
    _wifiClient->write((const uint8_t*)jsonBuf, jsonLen);

    // Read response status line and extract HTTP status code
    uint32_t start = millis();
    while (!_wifiClient->available() &&
           millis() - start < (uint32_t)WEBHOOK_TIMEOUT_MS) {
        delay(10);
    }

    int httpCode = -1;
    if (_wifiClient->available()) {
        char statusLine[64];
        int n = _wifiClient->readBytesUntil('\n', statusLine, sizeof(statusLine) - 1);
        statusLine[n] = '\0';
        // Status line format: "HTTP/1.1 200 OK\r"
        const char* codeStart = strchr(statusLine, ' ');
        if (codeStart) {
            httpCode = atoi(codeStart + 1);
        }
    }

    _wifiClient->stop();

    bool ok = (httpCode >= 200 && httpCode < 300);
    if (!ok) {
        LOGF("Webhook(WiFi): HTTP error %d", httpCode);
    } else {
        LOGF("Webhook(WiFi): HTTP %d OK", httpCode);
    }
    return ok;
}

// ---------------------------------------------------------------------------
// isConnected / end
// ---------------------------------------------------------------------------
bool WebhookTransport::isConnected() {
    return _connected;
}

void WebhookTransport::end() {
    if (_wifiMode && _wifiClient) {
        _wifiClient->stop();
    }
    _connected = false;
}

#endif // !NATIVE_BUILD
