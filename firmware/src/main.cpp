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
    uint32_t rgb;
    switch (netMode()) {
        case NetMode::PORTAL: rgb = 0x000028; break;
        case NetMode::ONLINE: rgb = ps.playing ? 0x002800 : 0x1e1900; break;
        default: rgb = 0x280000; break;
    }
    static uint32_t lastRgb = 0xFFFFFFFF;
    if (rgb == lastRgb) return; // avoid hammering the RMT driver every tick
    lastRgb = rgb;
    rgbLedWrite(PIN_RGB_LED, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
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

    static uint32_t lastVuMs = 0;
    if (millis() - lastVuMs >= 30) {
        lastVuMs = millis();
        uiUpdateVu();
    }
    if (millis() - lastUiMs >= 150) {
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
