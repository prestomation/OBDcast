#ifndef NATIVE_BUILD

#include "WebhookTransport.h"
#include <string.h>

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
        // Skip certificate verification for simplicity (user can pin later)
        _wifiClient->setInsecure();
        _connected = true;
        LOG("Webhook(WiFi): ready");
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
    size_t len = payload.toJson(jsonBuf, sizeof(jsonBuf));
    if (len == 0) {
        LOG("Webhook: JSON serialization failed");
        return false;
    }

    const char* authToken = (strlen(WEBHOOK_AUTH_TOKEN) > 0) ? WEBHOOK_AUTH_TOKEN : nullptr;

    if (_wifiMode) {
        // Parse host and path from WEBHOOK_URL (simplified parser)
        // Assumes format: https://hostname/path
        const char* url = WEBHOOK_URL;
        const char* hostStart = strstr(url, "://");
        if (!hostStart) return false;
        hostStart += 3;

        const char* pathStart = strchr(hostStart, '/');
        char host[128];
        char path[256];
        if (pathStart) {
            size_t hostLen = (size_t)(pathStart - hostStart);
            snprintf(host, sizeof(host), "%.*s", (int)hostLen, hostStart);
            snprintf(path, sizeof(path), "%s", pathStart);
        } else {
            snprintf(host, sizeof(host), "%s", hostStart);
            snprintf(path, sizeof(path), "/");
        }

        if (!_wifiClient->connect(host, 443)) {
            LOG("Webhook(WiFi): connect failed");
            return false;
        }

        // Build HTTP request
        char request[2048];
        int  reqLen;
        if (authToken) {
            reqLen = snprintf(request, sizeof(request),
                "POST %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Content-Type: application/json\r\n"
                "Authorization: Bearer %s\r\n"
                "Content-Length: %u\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                path, host, authToken, (unsigned)len, jsonBuf);
        } else {
            reqLen = snprintf(request, sizeof(request),
                "POST %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %u\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                path, host, (unsigned)len, jsonBuf);
        }

        _wifiClient->write((const uint8_t*)request, (size_t)reqLen);

        // Read response (just check for 200)
        uint32_t start = millis();
        while (_wifiClient->connected() &&
               millis() - start < (uint32_t)WEBHOOK_TIMEOUT_MS) {
            if (_wifiClient->available()) break;
            delay(10);
        }

        bool ok = false;
        if (_wifiClient->available()) {
            char statusLine[64];
            _wifiClient->readBytesUntil('\n', statusLine, sizeof(statusLine));
            ok = (strstr(statusLine, "200") != nullptr);
        }

        _wifiClient->stop();
        return ok;

    } else {
        // Cellular path via modem TLS offload
        int code = _modem->httpPost(WEBHOOK_URL, jsonBuf, len, authToken);
        return (code >= 200 && code < 300);
    }
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
