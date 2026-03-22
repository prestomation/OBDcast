#ifndef NATIVE_BUILD

#include "SIM7600Modem.h"
#include "../config.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// begin
// ---------------------------------------------------------------------------
bool SIM7600Modem::begin() {
    LOG("Modem: powering on");
    if (!_modem.begin()) {
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
    char apnCmd[64];
    snprintf(apnCmd, sizeof(apnCmd), "AT+CGDCONT=1,\"IP\",\"%s\"", CELLULAR_APN);

    if (!sendAT(apnCmd, "OK", 5000)) {
        LOG("Modem: APN config failed");
        return false;
    }

    // Activate PDP context
    if (!sendAT("AT+CGACT=1,1", "OK", 30000)) {
        LOG("Modem: PDP activation failed");
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
    _modem.end();
    _connected = false;
}

// ---------------------------------------------------------------------------
// httpPost – HTTPS POST via SIM7600 AT+HTTP* commands with TLS offload
// ---------------------------------------------------------------------------
int SIM7600Modem::httpPost(const char* url, const char* body, size_t bodyLen,
                            const char* authHeader) {
    // Terminate any previous session
    sendAT("AT+HTTPTERM", "OK", 3000);

    if (!sendAT("AT+HTTPINIT", "OK", 5000)) {
        LOG("Modem: HTTPINIT failed");
        return -1;
    }

    // Set URL
    char urlCmd[256];
    snprintf(urlCmd, sizeof(urlCmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    if (!sendAT(urlCmd, "OK", 3000)) {
        sendAT("AT+HTTPTERM", "OK", 3000);
        return -1;
    }

    // Enable SSL
    if (!sendAT("AT+HTTPSSL=1", "OK", 3000)) {
        LOG("Modem: HTTPSSL enable failed");
    }

    // Content-Type
    sendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 3000);

    // Authorization header (optional)
    if (authHeader && authHeader[0] != '\0') {
        char hdrCmd[256];
        snprintf(hdrCmd, sizeof(hdrCmd),
                 "AT+HTTPPARA=\"USERDATA\",\"Authorization: Bearer %s\\r\\n\"",
                 authHeader);
        sendAT(hdrCmd, "OK", 3000);
    }

    // Send body
    char dataCmd[32];
    snprintf(dataCmd, sizeof(dataCmd), "AT+HTTPDATA=%u,10000", (unsigned)bodyLen);
    if (!sendAT(dataCmd, "DOWNLOAD", 5000)) {
        sendAT("AT+HTTPTERM", "OK", 3000);
        return -1;
    }

    // Write body data directly to modem serial
    _modem.write((const uint8_t*)body, bodyLen);
    delay(500);

    // Execute POST (method 1)
    if (!sendAT("AT+HTTPACTION=1", "+HTTPACTION:", 30000)) {
        sendAT("AT+HTTPTERM", "OK", 3000);
        return -1;
    }

    // Response code is embedded in the +HTTPACTION line – return 200 as
    // success approximation; a full impl would parse the response.
    sendAT("AT+HTTPTERM", "OK", 3000);
    return 200;
}

// ---------------------------------------------------------------------------
// TCP helpers (used by MQTT transport)
// ---------------------------------------------------------------------------
bool SIM7600Modem::tcpConnect(const char* host, uint16_t port, bool useTls) {
    // Use CSSLCFG to enable TLS if required
    if (useTls) {
        sendAT("AT+CSSLCFG=\"sslversion\",0,3", "OK", 3000);
        sendAT("AT+CSSLCFG=\"ignorertctime\",1,1", "OK", 3000);
    }
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CIPOPEN=0,\"TCP\",\"%s\",%u", host, port);
    return sendAT(cmd, "OK", 15000);
}

int SIM7600Modem::tcpSend(const uint8_t* data, size_t len) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%u", (unsigned)len);
    if (!sendAT(cmd, ">", 5000)) return -1;
    return (int)_modem.write(data, len);
}

int SIM7600Modem::tcpRecv(uint8_t* buf, size_t bufLen, uint32_t timeoutMs) {
    return _modem.receive(buf, (int)bufLen, timeoutMs);
}

void SIM7600Modem::tcpClose() {
    sendAT("AT+CIPCLOSE=0", "OK", 5000);
}

// ---------------------------------------------------------------------------
// Signal / status
// ---------------------------------------------------------------------------
int SIM7600Modem::getSignalDbm() {
    // AT+CSQ returns signal quality; convert to approximate dBm
    // CSQ 0-31: dBm = (CSQ * 2) - 113
    // Not fully implemented here; return last known value
    return -99; // placeholder
}

bool SIM7600Modem::isConnected() {
    return _connected;
}

// ---------------------------------------------------------------------------
// sendAT helper
// ---------------------------------------------------------------------------
bool SIM7600Modem::sendAT(const char* cmd, const char* expected,
                            uint32_t timeoutMs) {
    _modem.sendCommand(cmd);
    char resp[128] = {};
    return _modem.receiveResponse(resp, sizeof(resp), timeoutMs) &&
           strstr(resp, expected) != nullptr;
}

#endif // !NATIVE_BUILD
