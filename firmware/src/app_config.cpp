#include "app_config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

AppConfig config;
static const char *CONFIG_PATH = "/config.json";

bool configLoad() {
    if (!LittleFS.exists(CONFIG_PATH)) return false;
    File f = LittleFS.open(CONFIG_PATH, "r");
    if (!f) return false;
    String json = f.readString();
    f.close();
    return configFromJson(json);
}

bool configSave() {
    File f = LittleFS.open(CONFIG_PATH, "w");
    if (!f) return false;
    f.print(configToJson(true));
    f.close();
    return true;
}

String configToJson(bool includeSecrets) {
    JsonDocument doc;
    doc["wifiSsid"] = config.wifiSsid;
    if (includeSecrets) doc["wifiPass"] = config.wifiPass;
    doc["hostname"] = config.hostname;
    doc["stationName"] = config.stationName;
    JsonArray urls = doc["streamUrls"].to<JsonArray>();
    for (auto &u : config.streamUrls) urls.add(u);
    doc["volume"] = config.volume;
    doc["speakersEnabled"] = config.speakersEnabled;
    doc["onboardSpeaker"] = config.onboardSpeaker;
    doc["autoPlay"] = config.autoPlay;
    doc["brightness"] = config.brightness;
    doc["otaBaseUrl"] = config.otaBaseUrl;
    doc["autoUpdate"] = config.autoUpdate;
    doc["otaCheckMinutes"] = config.otaCheckMinutes;
    String out;
    serializeJson(doc, out);
    return out;
}

bool configFromJson(const String &json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;

    if (doc["wifiSsid"].is<const char *>()) config.wifiSsid = doc["wifiSsid"].as<String>();
    if (doc["wifiPass"].is<const char *>()) config.wifiPass = doc["wifiPass"].as<String>();
    if (doc["hostname"].is<const char *>()) config.hostname = doc["hostname"].as<String>();
    if (doc["stationName"].is<const char *>()) config.stationName = doc["stationName"].as<String>();
    if (doc["streamUrls"].is<JsonArray>()) {
        config.streamUrls.clear();
        for (JsonVariant v : doc["streamUrls"].as<JsonArray>()) {
            String u = v.as<String>();
            u.trim();
            if (u.length() && config.streamUrls.size() < MAX_STREAM_URLS) config.streamUrls.push_back(u);
        }
    }
    if (doc["volume"].is<int>()) config.volume = constrain(doc["volume"].as<int>(), 0, 21);
    if (doc["speakersEnabled"].is<bool>()) config.speakersEnabled = doc["speakersEnabled"];
    if (doc["onboardSpeaker"].is<bool>()) config.onboardSpeaker = doc["onboardSpeaker"];
    if (doc["autoPlay"].is<bool>()) config.autoPlay = doc["autoPlay"];
    if (doc["brightness"].is<int>()) config.brightness = constrain(doc["brightness"].as<int>(), 5, 100);
    if (doc["otaBaseUrl"].is<const char *>()) {
        config.otaBaseUrl = doc["otaBaseUrl"].as<String>();
        if (config.otaBaseUrl.endsWith("/")) config.otaBaseUrl.remove(config.otaBaseUrl.length() - 1);
    }
    if (doc["autoUpdate"].is<bool>()) config.autoUpdate = doc["autoUpdate"];
    if (doc["otaCheckMinutes"].is<int>()) config.otaCheckMinutes = max(5, doc["otaCheckMinutes"].as<int>());
    return true;
}
