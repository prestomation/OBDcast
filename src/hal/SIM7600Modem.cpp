#ifndef NATIVE_BUILD

#include "SIM7600Modem.h"
#include "../config.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// sanitizeForAT – reject any input containing control chars or double-quote.
// Returns true if sanitization succeeded (input is safe and fits in outLen).
// Returns false if the input contained forbidden characters or was truncated.
// ---------------------------------------------------------------------------
static bool sanitizeForAT(const char* in, char* out, size_t outLen) {
    size_t i = 0;
    for (; *in && i < outLen - 1; in++) {
        if ((unsigned char)*in < 0x20 || *in == '"') return false;
        out[i++] = *in;
    }
    out[i] = '\0';
    return (*in == '\0'); // false if input was truncated
}

// ---------------------------------------------------------------------------
// begin
// ---------------------------------------------------------------------------
bool SIM7600Modem::begin() {
    LOG("Modem: powering on");
    if (!_cell.begin(&_hal)) {
        LOG("Modem: begin() failed");
        return false;
    }
    LOG("Modem: ready");
    return true;
}

// ---------------------------------------------------------------------------
// connect – register on network and activate PDP context
// ---------------------------------------------------------------------------
bool SIM7600Modem::connect() {
    if (!_cell.setup(CELLULAR_APN)) {
        LOG("Modem: network setup failed");
        return false;
    }
    LOG("Modem: connected");
    _connected = true;
    return true;
}

// ---------------------------------------------------------------------------
// disconnect / powerOff
// ---------------------------------------------------------------------------
void SIM7600Modem::disconnect() {
    sendAT("AT+CGACT=0,1", "OK", 5000);
    _connected = false;
}

void SIM7600Modem::powerOff() {
    _cell.end();
    _connected = false;
}

// ---------------------------------------------------------------------------
// httpPost – HTTPS POST via SIM7600 AT+HTTP* commands with TLS offload
// ---------------------------------------------------------------------------
int SIM7600Modem::httpPost(const char* url, const char* body, size_t bodyLen,
                            const char* authHeader, const char* hmacSig) {
    // Sanitize URL — reject control chars and quotes to prevent AT injection
    char safeUrl[256];
    if (!sanitizeForAT(url, safeUrl, sizeof(safeUrl))) {
        LOG("Modem: URL contains invalid characters or is too long");
        return -1;
    }

    // Terminate any previous session
    sendAT("AT+HTTPTERM", "OK", 3000);

    if (!sendAT("AT+HTTPINIT", "OK", 5000)) {
        LOG("Modem: HTTPINIT failed");
        return -1;
    }

    // Set URL
    char urlCmd[320];
    int urlCmdLen = snprintf(urlCmd, sizeof(urlCmd), "AT+HTTPPARA=\"URL\",\"%s\"", safeUrl);
    if (urlCmdLen < 0 || (size_t)urlCmdLen >= sizeof(urlCmd)) {
        LOG("Modem: URL command truncated");
        sendAT("AT+HTTPTERM", "OK", 3000);
        return -1;
    }
    if (!sendAT(urlCmd, "OK", 3000)) {
        sendAT("AT+HTTPTERM", "OK", 3000);
        return -1;
    }

    // Enable SSL — required for HTTPS; abort if this fails
    if (!sendAT("AT+HTTPSSL=1", "OK", 3000)) {
        LOG("Modem: HTTPSSL enable failed — aborting to prevent plaintext transmission");
        sendAT("AT+HTTPTERM", "OK", 3000);
        return -1;
    }

    // Content-Type
    sendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 3000);

    // Build USERDATA header(s): Authorization and/or X-OBDcast-Signature
    bool hasAuth = (authHeader && authHeader[0] != '\0');
    bool hasSig  = (hmacSig   && hmacSig[0]   != '\0');
    if (hasAuth || hasSig) {
        char safeAuth[256] = {};
        if (hasAuth && !sanitizeForAT(authHeader, safeAuth, sizeof(safeAuth))) {
            LOG("Modem: auth token contains invalid characters");
            sendAT("AT+HTTPTERM", "OK", 3000);
            return -1;
        }
        // hmacSig is hex-only (0-9 a-f) — safe by construction; sanitize anyway
        char safeSig[72] = {};
        if (hasSig && !sanitizeForAT(hmacSig, safeSig, sizeof(safeSig))) {
            LOG("Modem: HMAC sig contains invalid characters");
            sendAT("AT+HTTPTERM", "OK", 3000);
            return -1;
        }

        char hdrCmd[512];
        int hdrLen;
        if (hasAuth && hasSig) {
            hdrLen = snprintf(hdrCmd, sizeof(hdrCmd),
                "AT+HTTPPARA=\"USERDATA\",\"Authorization: Bearer %s\\r\\nX-OBDcast-Signature: %s\\r\\n\"",
                safeAuth, safeSig);
        } else if (hasAuth) {
            hdrLen = snprintf(hdrCmd, sizeof(hdrCmd),
                "AT+HTTPPARA=\"USERDATA\",\"Authorization: Bearer %s\\r\\n\"",
                safeAuth);
        } else {
            hdrLen = snprintf(hdrCmd, sizeof(hdrCmd),
                "AT+HTTPPARA=\"USERDATA\",\"X-OBDcast-Signature: %s\\r\\n\"",
                safeSig);
        }
        if (hdrLen < 0 || (size_t)hdrLen >= sizeof(hdrCmd)) {
            LOG("Modem: USERDATA header command truncated");
            sendAT("AT+HTTPTERM", "OK", 3000);
            return -1;
        }
        sendAT(hdrCmd, "OK", 3000);
    }

    // Send body
    char dataCmd[32];
    snprintf(dataCmd, sizeof(dataCmd), "AT+HTTPDATA=%u,10000", (unsigned)bodyLen);
    if (!sendAT(dataCmd, "DOWNLOAD", 5000)) {
        sendAT("AT+HTTPTERM", "OK", 3000);
        return -1;
    }

    // Write body data via xBee UART
    _hal.xbWrite((const char*)body, (int)bodyLen);
    delay(500);

    // Execute POST (method 1) — wait for +HTTPACTION response
    // Format: +HTTPACTION: 1,<status>,<data_len>
    char actionResp[64] = {};
    _hal.xbWrite("AT+HTTPACTION=1\r");
    int recvLen = _hal.xbReceive(actionResp, sizeof(actionResp) - 1, 30000);
    if (recvLen <= 0 || strstr(actionResp, "+HTTPACTION:") == nullptr) {
        LOG("Modem: HTTPACTION timeout or error");
        sendAT("AT+HTTPTERM", "OK", 3000);
        return -1;
    }

    // Parse HTTP status code from "+HTTPACTION: 1,<code>,<len>"
    int httpCode = -1;
    const char* comma1 = strchr(actionResp, ',');
    if (comma1) {
        httpCode = atoi(comma1 + 1);
    }

    sendAT("AT+HTTPTERM", "OK", 3000);

    if (httpCode <= 0) {
        LOG("Modem: HTTPACTION response parse failed");
        return -1;
    }

    LOGF("Modem: HTTP %d", httpCode);
    return httpCode;
}

// ---------------------------------------------------------------------------
// TCP helpers (used by MQTT transport)
// ---------------------------------------------------------------------------
bool SIM7600Modem::tcpConnect(const char* host, uint16_t port, bool useTls) {
    // Use CSSLCFG to enable TLS if required
    if (useTls) {
        // TLS 1.2 only (sslversion 3 = TLS 1.2)
        if (!sendAT("AT+CSSLCFG=\"sslversion\",0,3", "OK", 3000)) {
            LOG("Modem: TLS version config failed — aborting secure connect");
            return false;
        }
        // NOTE: ignorertctime=1 accepts certificates regardless of device clock.
        // This is needed when the ESP32 clock is not yet NTP-synced on first boot.
        // For maximum security, sync NTP before connecting and set ignorertctime=0.
        if (!sendAT("AT+CSSLCFG=\"ignorertctime\",1,1", "OK", 3000)) {
            LOG("Modem: TLS cert time config failed — aborting secure connect");
            return false;
        }
        // Enable SSL for this connection context
        if (!sendAT("AT+CCHSSLCFG=0,1", "OK", 3000)) {
            // Non-fatal: older firmware may not support this command
            LOG("Modem: CCHSSLCFG not supported, continuing");
        }
    }
    char safeHost[128];
    if (!sanitizeForAT(host, safeHost, sizeof(safeHost))) {
        LOG("Modem: TCP host contains invalid characters");
        return false;
    }
    char cmd[160];
    int cmdLen = snprintf(cmd, sizeof(cmd), "AT+CIPOPEN=0,\"TCP\",\"%s\",%u", safeHost, port);
    if (cmdLen < 0 || (size_t)cmdLen >= sizeof(cmd)) {
        LOG("Modem: CIPOPEN command truncated");
        return false;
    }
    return sendAT(cmd, "OK", 15000);
}

int SIM7600Modem::tcpSend(const uint8_t* data, size_t len) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%u", (unsigned)len);
    if (!sendAT(cmd, ">", 5000)) return -1;
    _hal.xbWrite((const char*)data, (int)len);
    return (int)len;
}

int SIM7600Modem::tcpRecv(uint8_t* buf, size_t bufLen, uint32_t timeoutMs) {
    return _hal.xbRead((char*)buf, (int)bufLen, (unsigned int)timeoutMs);
}

void SIM7600Modem::tcpClose() {
    sendAT("AT+CIPCLOSE=0", "OK", 5000);
}

// ---------------------------------------------------------------------------
// Signal / status
// ---------------------------------------------------------------------------
int SIM7600Modem::getSignalDbm() {
    return _cell.RSSI();
}

bool SIM7600Modem::isConnected() {
    return _connected;
}

// ---------------------------------------------------------------------------
// sendAT helper — writes raw AT command via xBee UART, waits for expected
// ---------------------------------------------------------------------------
bool SIM7600Modem::sendAT(const char* cmd, const char* expected,
                            uint32_t timeoutMs) {
    char fullCmd[132];
    snprintf(fullCmd, sizeof(fullCmd), "%s\r", cmd);
    _hal.xbWrite(fullCmd);
    char resp[128] = {};
    int len = _hal.xbReceive(resp, sizeof(resp) - 1, (unsigned int)timeoutMs);
    return (len > 0) && (strstr(resp, expected) != nullptr);
}

#endif // !NATIVE_BUILD
