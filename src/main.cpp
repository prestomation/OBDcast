#ifndef NATIVE_BUILD

// =============================================================================
// OBDcast – ESP32 firmware for Freematics ONE+ Model B
// =============================================================================

#include <Arduino.h>
#include <esp_sleep.h>
#include <time.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"
#include "Payload.h"

// HAL
#include "hal/FreematicsOBD.h"
#include "hal/FreematicsGNSS.h"
#include "hal/FreematicsMEMS.h"
#include "hal/SIM7600Modem.h"

// Services
#include "transport/ITransport.h"
#include "transport/MQTTTransport.h"
#include "transport/WebhookTransport.h"
#include "power/PowerManager.h"
#include "collector/DataCollector.h"
#include "connectivity/ConnectivityManager.h"
#include "storage/SDBuffer.h"

// ---------------------------------------------------------------------------
// Globals (accessed by ConnectivityManager.cpp extern)
// ---------------------------------------------------------------------------
static FreematicsESP32 hal;
SIM7600Modem* gModem = nullptr;
static bool   gModemOk = false;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static ITransport* createTransport(ConnPath path);
static void        replayBuffered(ITransport* transport, SDBuffer& buf);
static void        enterDeepSleepMode(uint32_t wakeIntervalSec);

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(DEBUG_BAUD);
    delay(200);
    LOG("OBDcast: starting");

    // --- Initialize HAL ---
    if (!hal.begin()) {
        LOG("HAL: init failed — halting");
        while (true) delay(1000);
    }

    // --- Modem ---
    gModem = new SIM7600Modem(hal);
    gModemOk = gModem->begin();
    if (!gModemOk) {
        LOG("Modem: init failed — continuing without cellular");
        delete gModem;
        gModem = nullptr;
    }

    // --- Connectivity ---
    ConnectivityManager conn;
    ConnPath path = conn.connect();
    if (path == ConnPath::NONE) {
        LOG("Connectivity: no path available");
    }

    // --- Create transport ---
    ITransport* transport = createTransport(path);
    if (transport) {
        if (!transport->begin()) {
            LOGF("%s: transport begin() failed", transport->getName());
            delete transport;
            transport = nullptr;
        }
    }

    // --- HAL subsystems ---
    FreematicsOBDImpl  obd(hal);
    // Guard: only use GNSS if modem initialized successfully.
    // gnssPtr lifetime: lives for the duration of the main loop.
    // Released explicitly before esp_deep_sleep_start() in the DEEP_SLEEP
    // case (which does not return — ESP32 resets on wake).
    FreematicsGNSS*     gnssPtr  = nullptr;
    if (gModemOk) {
        gnssPtr = new FreematicsGNSS(gModem->modem());
        gnssPtr->begin();
    }

    FreematicsMEMSImpl mems;
    obd.begin();
    mems.begin();

    // Fallback stub GNSS if modem init failed
    // (DataCollector requires a reference; we provide a no-op stub)
    struct NoopGNSS : public IGNSS {
        bool begin() override { return false; }
        bool update() override { return false; }
        bool getLocation(float&,float&,float&,float&,float&,float&,int&) override { return false; }
        bool hasFix() override { return false; }
    } noopGnss;

    IGNSS& gnssRef = gnssPtr ? static_cast<IGNSS&>(*gnssPtr) : static_cast<IGNSS&>(noopGnss);

    // --- Services ---
    DataCollector    collector(obd, gnssRef, mems);
    PowerManager     power(VOLTAGE_ACTIVE_THRESHOLD, VOLTAGE_STANDBY_THRESHOLD,
                           STANDBY_PING_MS, STANDBY_IDLE_TIMEOUT_MS);
    SDBuffer         sdBuffer;
    sdBuffer.begin();

    // ---------------------------------------------------------------------------
    // Main loop
    // ---------------------------------------------------------------------------
    while (true) {
        float voltage = obd.readVoltage();
        float ax = 0.f, ay = 0.f, az = 0.f;
        mems.getAccel(ax, ay, az);
        float magnitude = mems.getMagnitude();
        // Use absolute difference from 1g (gravity) to detect real motion
        float gravDiff = magnitude - 9.81f;
        if (gravDiff < 0.f) gravDiff = -gravDiff;
        bool  motion   = (gravDiff > MOTION_WAKE_THRESHOLD_G);

        PowerState state = power.update(voltage, motion);
        LOGF("Power: %s  V=%.2f", PowerManager::stateName(state), voltage);

        switch (state) {
        case PowerState::ACTIVE: {
            if (power.shouldCollect(DATA_INTERVAL_MS)) {
                Payload p;
                collector.collect(p, conn.getSignalDbm());

                bool sent = false;
                if (transport && transport->isConnected()) {
                    sent = transport->send(p);
                    if (!sent) {
                        LOG("Transport: send failed, buffering to SD");
                    }
                }
                if (!sent) {
                    sdBuffer.write(p);
                }

                // Replay buffered payloads if connected
                if (transport && transport->isConnected() && sdBuffer.hasPending()) {
                    replayBuffered(transport, sdBuffer);
                }
            }
            break;
        }

        case PowerState::STANDBY: {
            if (power.shouldSendStandbyPing()) {
                LOG("Standby ping");
                Payload ping;
                collector.collectPing(ping, conn.getSignalDbm());
                if (transport) transport->send(ping);
            }
            delay(10000); // check every 10 seconds in standby
            break;
        }

        case PowerState::DEEP_SLEEP: {
            LOG("Entering deep sleep");
            if (transport) {
                transport->end();
                delete transport;
                transport = nullptr;
            }
            conn.disconnect();
            if (gModem) gModem->powerOff();
            if (gnssPtr) delete gnssPtr;
            enterDeepSleepMode(DEEP_SLEEP_WAKE_INTERVAL_S);
            // Does not return — ESP32 resets on wake
            break;
        }
        }
    }
}

// ---------------------------------------------------------------------------
// loop – unused (main loop runs inside setup())
// ---------------------------------------------------------------------------
void loop() {}

// ---------------------------------------------------------------------------
// createTransport
// ---------------------------------------------------------------------------
static ITransport* createTransport(ConnPath path) {
    if (path == ConnPath::WIFI) {
        static WiFiClient      wifiClient;
        static WiFiClientSecure wifiSecureClient;

        if (TRANSPORT_MODE == TRANSPORT_MQTT) {
            return new MQTTTransport(wifiClient);
        } else {
            return new WebhookTransport(wifiSecureClient,
                                        WebhookTransport::WiFiTag{});
        }
    } else if (path == ConnPath::CELLULAR && gModem && gModemOk) {
        if (TRANSPORT_MODE == TRANSPORT_MQTT) {
            return new MQTTTransport(*gModem);
        } else {
            return new WebhookTransport(*gModem);
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// replayBuffered – drain SD buffer to transport
//
// Reads buffered JSON records one at a time and sends them via sendRaw().
// Only clears the buffer after ALL records are successfully sent.
// If any record fails to send, replay stops and the buffer is preserved
// for the next opportunity.
// ---------------------------------------------------------------------------
static void replayBuffered(ITransport* transport, SDBuffer& buf) {
    if (!buf.hasPending()) return;
    LOGF("SDBuffer: replaying %u records", (unsigned)buf.pendingCount());

    char line[1024];
    uint32_t replayed = 0;
    bool     anyFailed = false;

    while (buf.readNext(line, sizeof(line))) {
        size_t len = strlen(line);
        if (len == 0) continue; // skip blank lines

        bool ok = transport->sendRaw(line, len);
        if (ok) {
            replayed++;
        } else {
            LOG("SDBuffer: replay send failed — stopping replay");
            anyFailed = true;
            break;
        }
    }

    if (replayed > 0 && !anyFailed) {
        // All records sent successfully — safe to clear the buffer
        buf.clearReplayed();
        LOGF("SDBuffer: replayed %u records, buffer cleared", (unsigned)replayed);
    } else if (replayed > 0) {
        // Partial success — buffer NOT cleared; will retry on next connection
        LOGF("SDBuffer: replayed %u records, %s",
             (unsigned)replayed,
             anyFailed ? "partial replay — buffer retained" : "buffer retained");
    }
}

// ---------------------------------------------------------------------------
// enterDeepSleepMode
// ---------------------------------------------------------------------------
static void enterDeepSleepMode(uint32_t wakeIntervalSec) {
    // Enable wake on timer
    esp_sleep_enable_timer_wakeup((uint64_t)wakeIntervalSec * 1000000ULL);

    // Enable wake on external GPIO (motion from MEMS INT pin)
    // GPIO pin 39 is commonly INT on Freematics ONE+; adjust as needed
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 1);

    esp_deep_sleep_start();
    // Does not return
}

#endif // !NATIVE_BUILD
