// Audio task: owns the Audio (ESP32-audioI2S) instance on core 0 and implements
// the stream-failover state machine. All Audio calls happen on this task; other
// cores talk to it through a command queue and read a mutex-guarded snapshot.
#include "player.h"
#include "app_config.h"
#include "pins.h"
#include <Audio.h>
#include <WiFi.h>

static Audio audio; // I2S port 0 -> external stereo bus (MAX98357A x2 + PCM5102A)

enum CmdType : uint8_t { CMD_PLAY, CMD_STOP, CMD_NEXT, CMD_PREV, CMD_VOLUME, CMD_RELOAD, CMD_SPEAKERS };
struct Cmd {
    CmdType type;
    int32_t arg;
};

static QueueHandle_t cmdQueue;
static SemaphoreHandle_t statusMutex;
static PlayerStatus status;
static TaskHandle_t audioTaskHandle;

// Failover state (audio task only)
static std::vector<String> urls;
static int urlIndex = 0;
static bool wantPlaying = false;
static uint32_t nextAttemptMs = 0;
static uint32_t backoffMs = 1000;
static uint32_t lastProgressMs = 0;
static uint32_t lastAudioTime = 0;
static uint32_t connectStartedMs = 0;
static bool connecting = false;

static const uint32_t STALL_TIMEOUT_MS = 12000;   // no decode progress -> reconnect
static const uint32_t CONNECT_TIMEOUT_MS = 15000; // no playback after connect -> next URL
static const uint32_t BACKOFF_MAX_MS = 30000;

static void scheduleFailover();

static void setError(const char *msg) {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    strlcpy(status.lastError, msg, sizeof(status.lastError));
    xSemaphoreGive(statusMutex);
    Serial.printf("[player] %s\n", msg);
}

static void publishStatus() {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    status.playing = audio.isRunning();
    status.wantPlaying = wantPlaying;
    status.urlIndex = urlIndex;
    status.urlCount = urls.size();
    status.bitrate = audio.getBitRate(true);
    uint16_t vu = audio.getVUlevel();
    status.vuLeft = vu >> 8;
    status.vuRight = vu & 0xFF;
    uint32_t bufSize = audio.inBufferSize();
    status.bufferPct = bufSize ? (uint8_t)((uint64_t)audio.inBufferFilled() * 100 / bufSize) : 0;
    xSemaphoreGive(statusMutex);
}

static void startCurrentUrl() {
    if (urls.empty()) {
        setError("no stream URLs configured");
        wantPlaying = false;
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        // No point cycling URLs without a network; retry quietly.
        nextAttemptMs = millis() + 2000;
        return;
    }
    urlIndex %= urls.size();
    Serial.printf("[player] connecting to URL %d/%d: %s\n", urlIndex + 1, urls.size(), urls[urlIndex].c_str());
    audio.stopSong();
    connecting = true;
    connectStartedMs = millis();
    lastProgressMs = millis();
    lastAudioTime = 0;
    if (!audio.connecttohost(urls[urlIndex].c_str())) {
        connecting = false;
        setError("connect failed");
        scheduleFailover();
    }
}

static void scheduleFailover() {
    audio.stopSong();
    connecting = false;
    urlIndex++;
    if (urlIndex >= (int)urls.size()) {
        urlIndex = 0;
        backoffMs = min(backoffMs * 2, BACKOFF_MAX_MS); // full cycle failed -> back off
    }
    nextAttemptMs = millis() + backoffMs;
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    status.reconnects++;
    xSemaphoreGive(statusMutex);
    Serial.printf("[player] failover -> URL %d, retry in %lums\n", urlIndex + 1, (unsigned long)backoffMs);
}

static void applySpeakers(bool enabled) {
    digitalWrite(PIN_EXT_AMP_SD, enabled ? HIGH : LOW);
}

static void handleCmd(const Cmd &cmd) {
    switch (cmd.type) {
        case CMD_PLAY:
            wantPlaying = true;
            backoffMs = 1000;
            nextAttemptMs = 0;
            startCurrentUrl();
            break;
        case CMD_STOP:
            wantPlaying = false;
            connecting = false;
            audio.stopSong();
            break;
        case CMD_NEXT:
        case CMD_PREV:
            if (!urls.empty()) {
                urlIndex = (urlIndex + (cmd.type == CMD_NEXT ? 1 : (int)urls.size() - 1)) % urls.size();
                backoffMs = 1000;
                if (wantPlaying) startCurrentUrl();
            }
            break;
        case CMD_VOLUME:
            audio.setVolume(constrain(cmd.arg, 0, 21));
            break;
        case CMD_RELOAD:
            urls = config.streamUrls;
            urlIndex = 0;
            backoffMs = 1000;
            if (wantPlaying) startCurrentUrl();
            break;
        case CMD_SPEAKERS:
            applySpeakers(cmd.arg != 0);
            break;
    }
}

static void watchdog() {
    if (!wantPlaying) return;
    uint32_t now = millis();

    if (audio.isRunning()) {
        connecting = false;
        backoffMs = 1000; // healthy -> reset backoff
        uint32_t t = audio.getAudioCurrentTime();
        uint32_t buffered = audio.inBufferFilled();
        if (t != lastAudioTime || buffered > 4096) {
            lastAudioTime = t;
            lastProgressMs = now;
        } else if (now - lastProgressMs > STALL_TIMEOUT_MS) {
            setError("stream stalled");
            scheduleFailover();
        }
        return;
    }

    if (connecting) {
        if (now - connectStartedMs > CONNECT_TIMEOUT_MS) {
            setError("connect timeout");
            scheduleFailover();
        }
        return;
    }

    // Not running, not connecting: stream ended/errored -> retry when due
    if (nextAttemptMs == 0 || now >= nextAttemptMs) {
        nextAttemptMs = now + backoffMs;
        startCurrentUrl();
    }
}

static void audioTask(void *) {
    audio.setPinout(PIN_I2S_EXT_BCK, PIN_I2S_EXT_WS, PIN_I2S_EXT_DOUT);
    audio.setVolumeSteps(21);
    audio.setVolume(config.volume);
    audio.setConnectionTimeout(5000, 7000);

    urls = config.streamUrls;
    if (config.autoPlay && !urls.empty()) {
        wantPlaying = true;
        nextAttemptMs = 0;
    }

    uint32_t lastStatusMs = 0;
    Cmd cmd;
    for (;;) {
        while (xQueueReceive(cmdQueue, &cmd, 0) == pdTRUE) handleCmd(cmd);
        audio.loop();
        uint32_t now = millis();
        if (now - lastStatusMs >= 100) {
            lastStatusMs = now;
            publishStatus();
            watchdog();
        }
        vTaskDelay(1);
    }
}

void playerBegin() {
    pinMode(PIN_EXT_AMP_SD, OUTPUT);
    applySpeakers(config.speakersEnabled);
    // Keep the onboard mono amp off; the ES8311 codec is left unconfigured (muted).
    pinMode(PIN_AMP_ENABLE, OUTPUT);
    digitalWrite(PIN_AMP_ENABLE, LOW);

    cmdQueue = xQueueCreate(8, sizeof(Cmd));
    statusMutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 5, &audioTaskHandle, 0);
}

void playerGetStatus(PlayerStatus &out) {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    out = status;
    xSemaphoreGive(statusMutex);
}

static void send(CmdType t, int32_t arg = 0) {
    Cmd c{t, arg};
    if (cmdQueue) xQueueSend(cmdQueue, &c, pdMS_TO_TICKS(100));
}

void playerPlay() { send(CMD_PLAY); }
void playerStop() { send(CMD_STOP); }
void playerNextUrl() { send(CMD_NEXT); }
void playerPrevUrl() { send(CMD_PREV); }
void playerSetVolume(uint8_t vol) { send(CMD_VOLUME, vol); }
void playerReloadUrls() { send(CMD_RELOAD); }
void playerSetSpeakers(bool enabled) { send(CMD_SPEAKERS, enabled ? 1 : 0); }

// ---- ESP32-audioI2S weak callbacks (called from the audio task) ----
void audio_showstation(const char *info) {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    strlcpy(status.station, info, sizeof(status.station));
    xSemaphoreGive(statusMutex);
}

void audio_showstreamtitle(const char *info) {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    strlcpy(status.title, info, sizeof(status.title));
    xSemaphoreGive(statusMutex);
}

void audio_eof_stream(const char *info) {
    // Stream dropped at the server end; the watchdog will reconnect/fail over.
    Serial.printf("[player] stream ended: %s\n", info ? info : "");
}

void audio_info(const char *info) {
    Serial.printf("[audio] %s\n", info);
}
