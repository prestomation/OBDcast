#ifndef NATIVE_BUILD

#include "WebhookTransport.h"
#include <string.h>
#include <stdio.h>

#if WEBHOOK_HMAC_ENABLED
  #include "mbedtls/md.h"
#endif

// ---------------------------------------------------------------------------
// computeHmacSig – compute HMAC-SHA256 of payload, write lowercase hex into
// sigHex (must be at least 65 bytes). No-op if WEBHOOK_HMAC_ENABLED is false.
// ---------------------------------------------------------------------------
static void computeHmacSig(const char* payload, size_t payloadLen, char sigHex[65]) {
    sigHex[0] = '\0';
#if WEBHOOK_HMAC_ENABLED
    uint8_t hmacResult[32];
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_init(&ctx);
    if (mbedtls_md_setup(&ctx, info, 1) == 0) {
        mbedtls_md_hmac_starts(&ctx,
            (const uint8_t*)WEBHOOK_HMAC_SECRET, strlen(WEBHOOK_HMAC_SECRET));
        mbedtls_md_hmac_update(&ctx, (const uint8_t*)payload, payloadLen);
        mbedtls_md_hmac_finish(&ctx, hmacResult);
    }
    mbedtls_md_free(&ctx);
    for (int i = 0; i < 32; i++) {
        snprintf(sigHex + i * 2, 3, "%02x", hmacResult[i]);
    }
#else
    (void)payload;
    (void)payloadLen;
#endif
}

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

    char sigHex[65];
    computeHmacSig(jsonBuf, jsonLen, sigHex);
    const char* sig = (sigHex[0] != '\0') ? sigHex : nullptr;

    const char* authToken = (strlen(WEBHOOK_AUTH_TOKEN) > 0) ? WEBHOOK_AUTH_TOKEN : nullptr;

    if (_wifiMode) {
        return sendViaWiFi(jsonBuf, jsonLen, authToken, sig);
    } else {
        int code = _modem->httpPost(WEBHOOK_URL, jsonBuf, jsonLen, authToken, sig);
        bool ok = (code >= 200 && code < 300);
        if (!ok) {
            LOGF("Webhook(Cellular): HTTP error %d", code);
        }
        return ok;
    }
}

// ---------------------------------------------------------------------------
// sendRaw – send pre-serialized JSON (used for SD buffer replay)
// ---------------------------------------------------------------------------
bool WebhookTransport::sendRaw(const char* json, size_t len) {
    char sigHex[65];
    computeHmacSig(json, len, sigHex);
    const char* sig = (sigHex[0] != '\0') ? sigHex : nullptr;

    const char* authToken = (strlen(WEBHOOK_AUTH_TOKEN) > 0) ? WEBHOOK_AUTH_TOKEN : nullptr;
    if (_wifiMode) {
        return sendViaWiFi(json, len, authToken, sig);
    } else {
        int code = _modem->httpPost(WEBHOOK_URL, json, len, authToken, sig);
        return (code >= 200 && code < 300);
    }
}

// ---------------------------------------------------------------------------
// sendViaWiFi – parse URL, build HTTP/1.1 request, check response code
// ---------------------------------------------------------------------------
bool WebhookTransport::sendViaWiFi(const char* jsonBuf, size_t jsonLen,
                                    const char* authToken, const char* hmacSig) {
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
    char header[640];
    int hlen;
    // Build optional header lines into a small staging buffer
    char optHeaders[256] = {};
    int optLen = 0;
    if (authToken) {
        optLen += snprintf(optHeaders + optLen, sizeof(optHeaders) - (size_t)optLen,
            "Authorization: Bearer %s\r\n", authToken);
    }
    if (hmacSig && hmacSig[0] != '\0') {
        optLen += snprintf(optHeaders + optLen, sizeof(optHeaders) - (size_t)optLen,
            "X-OBDcast-Signature: %s\r\n", hmacSig);
    }
    hlen = snprintf(header, sizeof(header),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "%s"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, optHeaders, (unsigned)jsonLen);

    if (hlen < 0 || (size_t)hlen >= sizeof(header)) {
        LOG("Webhook(WiFi): header buffer overflow");
        _wifiClient->stop();
        return false;
    }

    // Send header and body separately to avoid large combined buffer
    // Verify each write succeeds to detect silent failures
    size_t wh = _wifiClient->write((const uint8_t*)header,  (size_t)hlen);
    size_t wb = _wifiClient->write((const uint8_t*)jsonBuf, jsonLen);
    if (wh != (size_t)hlen || wb != jsonLen) {
        LOG("Webhook(WiFi): write incomplete — connection dropped?");
        _wifiClient->stop();
        return false;
    }

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
