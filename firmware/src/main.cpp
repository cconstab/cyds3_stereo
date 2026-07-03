// CYD-S3 Stereo — resilient internet radio player (Freenove FNK0104S)
// Core 0: audio task (player.cpp). Core 1 (this file): LVGL UI, WiFi, web UI, OTA.
#include <Arduino.h>
#include <LittleFS.h>
#include "pins.h"
#include "app_config.h"
#include "player.h"
#include "net.h"
#include "webui.h"
#include "ota.h"
#include "display_lvgl.h"
#include "ui.h"

static uint32_t lastUiMs = 0;
static uint32_t lastLedMs = 0;
static bool healthReported = false;

static void statusLed() {
    // WS2812: red = no network, blue = portal, yellow = idle, green = playing
    if (millis() - lastLedMs < 500) return;
    lastLedMs = millis();
    PlayerStatus ps;
    playerGetStatus(ps);
    switch (netMode()) {
        case NetMode::PORTAL: neopixelWrite(PIN_RGB_LED, 0, 0, 40); break;
        case NetMode::ONLINE:
            if (ps.playing) neopixelWrite(PIN_RGB_LED, 0, 40, 0);
            else neopixelWrite(PIN_RGB_LED, 30, 25, 0);
            break;
        default: neopixelWrite(PIN_RGB_LED, 40, 0, 0); break;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.printf("\nCYD-S3 Stereo fw %s\n", FW_VERSION);

    otaBegin(); // rollback check first — must run even if everything below crashes

    if (!LittleFS.begin(true)) Serial.println("[fs] LittleFS mount failed");
    if (!configLoad()) Serial.println("[cfg] no config, using defaults");

    displayBegin();
    uiBegin();

    netBegin();
    playerBegin();
    webuiBegin();
}

void loop() {
    displayLoop();
    netLoop();
    webuiLoop();
    otaLoop();
    statusLed();

    if (millis() - lastUiMs >= 100) {
        lastUiMs = millis();
        uiUpdate();
    }

    // Health: after 90s up with WiFi (or portal serving), this firmware is good.
    if (!healthReported && millis() > 90000) {
        if (netMode() == NetMode::ONLINE || netMode() == NetMode::PORTAL) {
            healthReported = true;
            otaMarkHealthy();
        }
    }

    delay(2);
}
