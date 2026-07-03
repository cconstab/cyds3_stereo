// Audio task: owns the Audio (ESP32-audioI2S) instance on core 0 and implements
// the stream-failover state machine. All Audio calls happen on this task; other
// cores talk to it through a command queue and read a mutex-guarded snapshot.
#include "player.h"
#include "app_config.h"
#include "pins.h"
#include "es8311.h"
#include <Audio.h>
#include <WiFi.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>

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

static uint32_t icyBitrate = 0; // from the audio_bitrate callback (icy-br header)

// RMS VU metering in dB. The library's own getVUlevel() is peak-biased, and
// internet radio is compressed with peaks pinned near full scale, so a peak
// meter just slams the top. RMS over each 30ms status window, mapped
// logarithmically (-42dB..0dB -> 0..127), moves the way studio meters do.
// Accumulated in audio_process_i2s and consumed in publishStatus — both run
// on the audio task, so plain statics are safe.
static uint64_t vuAccL = 0, vuAccR = 0;
static uint32_t vuFrames = 0;

static uint8_t rmsTakeVu(uint64_t acc) {
    if (!vuFrames) return 0;
    float rms = sqrtf((float)(acc / vuFrames)) / 32768.0f;
    if (rms < 1e-4f) return 0;
    float db = 20.0f * log10f(rms);
    float pct = (db + 42.0f) / 42.0f;
    return (uint8_t)(constrain(pct, 0.0f, 1.0f) * 127.0f);
}

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
    if (!status.bitrate) status.bitrate = icyBitrate; // AAC/ICY streams often only report via header
    status.vuLeft = rmsTakeVu(vuAccL);
    status.vuRight = rmsTakeVu(vuAccR);
    vuAccL = vuAccR = 0;
    vuFrames = 0;
    // Scale the buffer gauge to a 5-second playback window, not the full 655KB
    // PSRAM ring (live servers only ever send a few seconds ahead, so a
    // total-capacity gauge would sit near zero forever).
    uint32_t bitrate = status.bitrate ? status.bitrate : 128000;
    uint32_t fiveSecBytes = bitrate / 8 * 5;
    status.bufferPct = (uint8_t)min<uint64_t>(100, (uint64_t)audio.inBufferFilled() * 100 / fiveSecBytes);
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
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    strlcpy(status.currentUrl, urls[urlIndex].c_str(), sizeof(status.currentUrl));
    xSemaphoreGive(statusMutex);
    icyBitrate = 0; // don't carry a stale rate across URLs
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

// Mirror one I2S0 output signal onto an extra GPIO via the routing matrix, so the
// same bus drives both the external DACs and the onboard ES8311 codec.
static void mirrorI2sPin(uint8_t gpio, uint32_t signalIdx) {
    pinMode(gpio, OUTPUT);
    esp_rom_gpio_connect_out_signal(gpio, signalIdx, false, false);
}

static void audioTask(void *) {
    // Primary pins: external stereo bus. MCLK goes straight to the ES8311.
    audio.setPinout(PIN_I2S_EXT_BCK, PIN_I2S_EXT_WS, PIN_I2S_EXT_DOUT, PIN_I2S_INT_MCK);
    // Fan the bus out to the onboard codec's I2S pins as well.
    mirrorI2sPin(PIN_I2S_INT_BCK, I2S0O_BCK_OUT_IDX);
    mirrorI2sPin(PIN_I2S_INT_WS, I2S0O_WS_OUT_IDX);
    mirrorI2sPin(PIN_I2S_INT_DOUT, I2S0O_SD_OUT_IDX);
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
        if (now - lastStatusMs >= 30) { // 30ms so the VU meters track the audio closely
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

    pinMode(PIN_AMP_ENABLE, OUTPUT);
    digitalWrite(PIN_AMP_ENABLE, HIGH); // amp in shutdown until the codec is ready

    cmdQueue = xQueueCreate(8, sizeof(Cmd));
    statusMutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 5, &audioTaskHandle, 0);

    // Onboard mono speaker: the ES8311 needs MCLK running to complete its
    // power-up, and the audio task starts the I2S clocks in setPinout() —
    // so give it a moment, then program the codec. Still core 1 (shared I2C
    // bus with the touch controller) and still before the UI loop runs.
    delay(400);
    if (es8311_codec_init() == ESP_OK) {
        Serial.println("[player] ES8311 codec initialized");
        playerSetOnboardSpeaker(config.onboardSpeaker);
    } else {
        Serial.println("[player] ES8311 init failed — onboard speaker unavailable");
        digitalWrite(PIN_AMP_ENABLE, HIGH);
    }
}

// Core 1 only (I2C shared with touch): toggles codec mute + speaker amp.
// The SC8002B pin is SHUTDOWN (active high): LOW = amp on, HIGH = amp off.
void playerSetOnboardSpeaker(bool enabled) {
    es8311_output_enable(enabled);
    digitalWrite(PIN_AMP_ENABLE, enabled ? LOW : HIGH);
}

void playerGetStatus(PlayerStatus &out) {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    out = status;
    xSemaphoreGive(statusMutex);
}

void playerGetVu(uint8_t &left, uint8_t &right) {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    left = status.playing ? status.vuLeft : 0;
    right = status.playing ? status.vuRight : 0;
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

// Called by the library with each decoded PCM block before it goes to I2S.
void audio_process_i2s(int16_t *buff, uint16_t validSamples, uint8_t bitsPerSample, uint8_t channels, bool *continueI2S) {
    *continueI2S = true;
    if (bitsPerSample != 16 || channels != 2) return;
    for (uint16_t i = 0; i < validSamples; i++) {
        int32_t l = buff[2 * i], r = buff[2 * i + 1];
        vuAccL += (uint64_t)((int64_t)l * l);
        vuAccR += (uint64_t)((int64_t)r * r);
    }
    vuFrames += validSamples;
}

void audio_bitrate(const char *info) {
    icyBitrate = strtoul(info, nullptr, 10);
    if (icyBitrate && icyBitrate < 10000) icyBitrate *= 1000; // icy-br is in kbps
}

void audio_eof_stream(const char *info) {
    // Stream dropped at the server end; the watchdog will reconnect/fail over.
    Serial.printf("[player] stream ended: %s\n", info ? info : "");
}

void audio_info(const char *info) {
    Serial.printf("[audio] %s\n", info);
}
