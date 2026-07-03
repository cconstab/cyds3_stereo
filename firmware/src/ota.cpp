// Pull-OTA client. Polls <otaBaseUrl>/api/manifest, compares versions, streams the
// .bin into the inactive app partition with SHA-256 verification, and reboots.
// App-level rollback: a boot counter in NVS is cleared by otaMarkHealthy(); three
// consecutive unhealthy boots trigger Update.rollBack() to the previous slot.
#include "ota.h"
#include "app_config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>

static OtaState state;
static SemaphoreHandle_t stateMutex;
static Preferences prefs;
static uint32_t lastCheckMs = 0;
static bool healthyMarked = false;

static void setState(const char *msg, int pct = -1) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    strlcpy(state.message, msg, sizeof(state.message));
    if (pct >= 0) state.progressPct = pct;
    xSemaphoreGive(stateMutex);
    Serial.printf("[ota] %s\n", msg);
}

void otaBegin() {
    stateMutex = xSemaphoreCreateMutex();
    prefs.begin("ota", false);
    uint32_t failedBoots = prefs.getUInt("boots", 0) + 1;
    prefs.putUInt("boots", failedBoots);
    Serial.printf("[ota] boot %lu since last healthy mark\n", (unsigned long)failedBoots);
    if (failedBoots >= 3 && Update.canRollBack()) {
        Serial.println("[ota] 3 unhealthy boots -> rolling back to previous firmware");
        prefs.putUInt("boots", 0);
        Update.rollBack();
        ESP.restart();
    }
}

void otaMarkHealthy() {
    if (healthyMarked) return;
    healthyMarked = true;
    prefs.putUInt("boots", 0);
    Serial.println("[ota] boot marked healthy");
}

// numeric dotted-version compare: returns true if a > b
static bool versionNewer(const char *a, const char *b) {
    int am[4] = {0}, bm[4] = {0};
    sscanf(a, "%d.%d.%d.%d", &am[0], &am[1], &am[2], &am[3]);
    sscanf(b, "%d.%d.%d.%d", &bm[0], &bm[1], &bm[2], &bm[3]);
    for (int i = 0; i < 4; i++) {
        if (am[i] != bm[i]) return am[i] > bm[i];
    }
    return false;
}

static bool hexEqual(const uint8_t *digest, size_t len, const String &hex) {
    if (hex.length() != len * 2) return false;
    char buf[3] = {0};
    for (size_t i = 0; i < len; i++) {
        buf[0] = hex[i * 2];
        buf[1] = hex[i * 2 + 1];
        if ((uint8_t)strtoul(buf, nullptr, 16) != digest[i]) return false;
    }
    return true;
}

static bool downloadAndInstall(const String &url, const String &sha256hex) {
    HTTPClient http;
    http.setTimeout(30000);
    if (!http.begin(url)) {
        setState("bad firmware URL");
        return false;
    }
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        setState("firmware download failed");
        http.end();
        return false;
    }
    int total = http.getSize();
    if (total <= 0) {
        setState("firmware size unknown");
        http.end();
        return false;
    }
    if (!Update.begin(total)) {
        setState("not enough OTA space");
        http.end();
        return false;
    }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buf[4096];
    int written = 0;
    uint32_t lastData = millis();
    while (written < total) {
        size_t avail = stream->available();
        if (avail) {
            int n = stream->readBytes(buf, min(avail, sizeof(buf)));
            if (n <= 0) break;
            mbedtls_sha256_update(&sha, buf, n);
            if (Update.write(buf, n) != (size_t)n) break;
            written += n;
            lastData = millis();
            setState("downloading", (int)((uint64_t)written * 100 / total));
        } else {
            if (!http.connected() || millis() - lastData > 20000) break;
            delay(10);
        }
    }
    http.end();

    uint8_t digest[32];
    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);

    if (written != total) {
        Update.abort();
        setState("download incomplete");
        return false;
    }
    if (sha256hex.length() && !hexEqual(digest, 32, sha256hex)) {
        Update.abort();
        setState("sha256 mismatch — rejected");
        return false;
    }
    if (!Update.end(true)) {
        setState(Update.errorString());
        return false;
    }
    setState("installed, rebooting", 100);
    delay(750);
    ESP.restart();
    return true; // unreachable
}

bool otaCheckNow(bool install) {
    if (config.otaBaseUrl.isEmpty() || WiFi.status() != WL_CONNECTED) return false;
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    if (state.inProgress) {
        xSemaphoreGive(stateMutex);
        return false;
    }
    state.inProgress = true;
    xSemaphoreGive(stateMutex);

    bool result = false;
    HTTPClient http;
    http.setTimeout(10000);
    String url = config.otaBaseUrl + "/api/manifest?device=" FW_DEVICE "&fw=" FW_VERSION "&id=" +
                 String((uint32_t)ESP.getEfuseMac(), HEX);
    if (http.begin(url) && http.GET() == HTTP_CODE_OK) {
        JsonDocument doc;
        if (deserializeJson(doc, http.getString()) == DeserializationError::Ok && doc["version"].is<const char *>()) {
            const char *version = doc["version"];
            xSemaphoreTake(stateMutex, portMAX_DELAY);
            strlcpy(state.availableVersion, version, sizeof(state.availableVersion));
            xSemaphoreGive(stateMutex);
            if (versionNewer(version, FW_VERSION)) {
                String binUrl = doc["url"].as<String>();
                if (binUrl.startsWith("/")) binUrl = config.otaBaseUrl + binUrl;
                if (install) {
                    setState("update found, installing");
                    http.end();
                    result = downloadAndInstall(binUrl, doc["sha256"].as<String>());
                } else {
                    setState("update available");
                    result = true;
                }
            } else {
                setState("up to date");
            }
        } else {
            setState("bad manifest");
        }
    } else {
        setState("update server unreachable");
    }
    http.end();

    xSemaphoreTake(stateMutex, portMAX_DELAY);
    state.inProgress = false;
    xSemaphoreGive(stateMutex);
    return result;
}

void otaLoop() {
    if (!config.autoUpdate || config.otaBaseUrl.isEmpty()) return;
    uint32_t interval = (uint32_t)config.otaCheckMinutes * 60000UL;
    if (millis() - lastCheckMs < interval && lastCheckMs != 0) return;
    if (millis() < 60000) return; // let the system settle after boot
    lastCheckMs = millis();
    otaCheckNow(true);
}

void otaGetState(OtaState &out) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    out = state;
    xSemaphoreGive(stateMutex);
}
