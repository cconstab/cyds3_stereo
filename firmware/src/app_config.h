#pragma once
#include <Arduino.h>
#include <vector>

#define MAX_STREAM_URLS 8

struct AppConfig {
    // WiFi
    String wifiSsid;
    String wifiPass;
    String hostname = "cyds3-stereo";

    // Station
    String stationName = "My Radio";
    std::vector<String> streamUrls;

    // Audio
    uint8_t volume = 12;         // 0..21
    bool speakersEnabled = true;  // external MAX98357A pair (line-out is always live)
    bool onboardSpeaker = true;   // ES8311 codec + onboard mono speaker
    bool autoPlay = true;

    // Display
    uint8_t brightness = 80;    // percent
    bool bootSelfTest = false;  // color-cycle + I2C probe at power-on (bring-up diagnostic)

    // Web interface (always forced on while in provisioning/hotspot mode)
    bool webUiEnabled = true;

    // OTA
    String otaBaseUrl; // e.g. http://192.168.1.50:8080
    bool autoUpdate = true;
    uint16_t otaCheckMinutes = 60;
};

extern AppConfig config;

bool configLoad();
bool configSave();
String configToJson(bool includeSecrets);
bool configFromJson(const String &json); // partial updates allowed
