// Audio task: owns the Audio (ESP32-audioI2S) instance on core 0 and implements
// the stream-failover state machine. All Audio calls happen on this task; other
// cores talk to it through a command queue and read a mutex-guarded snapshot.
#include "player.h"
#include "app_config.h"
#include "pins.h"
#include "es8311.h"
#include "lineout.h"
#include <Audio.h>
#include <WiFi.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>

static Audio audio; // I2S port 0 -> external stereo bus (MAX98357A x2 + PCM5102A)

enum CmdType : uint8_t { CMD_PLAY, CMD_STOP, CMD_NEXT, CMD_PREV, CMD_VOLUME, CMD_RELOAD, CMD_SPEAKERS, CMD_MIGRATE, CMD_LINEOUT, CMD_LINELEVEL };
struct Cmd {
    CmdType type;
    int32_t arg;
};

static QueueHandle_t cmdQueue;
static SemaphoreHandle_t statusMutex;
static SemaphoreHandle_t urlsMutex; // guards the urls vector for the probe task's reads
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
static uint32_t titleSetMs = 0; // audio task only (metadata callback + publishStatus share the task)

// RMS VU metering in dB. The library's own getVUlevel() is peak-biased, and
// internet radio is compressed with peaks pinned near full scale, so a peak
// meter just slams the top. RMS over each 30ms status window, mapped
// logarithmically (-42dB..0dB -> 0..127), moves the way studio meters do.
// Accumulated in audio_process_i2s and consumed in publishStatus — both run
// on the audio task, so plain statics are safe.
static uint64_t vuAccL = 0, vuAccR = 0;
static uint32_t vuFrames = 0;

// Our own volume, applied in the PCM hook (library gain is pinned at unity so the
// hook sees full-scale samples — line-out copies them before this scaling).
// Same square-law curve the library used: gain = (vol/steps)^2, as Q15.
static volatile int32_t volGainQ15 = 32768;
static volatile int32_t lineGainQ15 = 32768; // line-out level, independent of UI volume

static void setVolGain(uint8_t vol) {
    float g = vol / 21.0f;
    volGainQ15 = (int32_t)(g * g * 32768.0f);
}

static void setLineGain(uint8_t pct) {
    float g = pct / 100.0f;
    lineGainQ15 = (int32_t)(g * g * 32768.0f);
}

static uint8_t rmsTakeVu(uint64_t acc) {
    if (!vuFrames) return 0;
    // Samples are metered pre-volume (full scale) — no gain compensation needed.
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
static void send(CmdType t, int32_t arg = 0);

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
    // Retire a title that hasn't been refreshed in 20 minutes — longer than any
    // plausible song, so it only fires when the stream stopped sending metadata.
    if (status.title[0] && millis() - titleSetMs > 20UL * 60UL * 1000UL) status.title[0] = 0;
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
    status.title[0] = 0;   // don't carry stale metadata across connections
    status.station[0] = 0;
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

// The amp-mute pin is unavailable when GPIO 14 doubles as the line-out DIN.
static bool muteAvailable() {
    return !(config.lineOutFixed && config.lineOutPin == PIN_EXT_AMP_SD);
}

// Firmware-level amp mute: zeroes the I2S0 stream in the PCM hook. With fixed
// line-out on I2S1 this silences the amps (and onboard codec) without touching
// the RCA feed — no dedicated mute GPIO required.
static volatile bool extMute = false;

static void applySpeakers(bool enabled) {
    extMute = !enabled;
    if (muteAvailable()) digitalWrite(PIN_EXT_AMP_SD, enabled ? HIGH : LOW);
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
            setVolGain(constrain(cmd.arg, 0, 21)); // applied in the PCM hook, not the library
            break;
        case CMD_RELOAD:
            xSemaphoreTake(urlsMutex, portMAX_DELAY);
            urls = config.streamUrls;
            xSemaphoreGive(urlsMutex);
            urlIndex = 0;
            backoffMs = 1000;
            if (wantPlaying) startCurrentUrl();
            break;
        case CMD_SPEAKERS:
            applySpeakers(cmd.arg != 0);
            break;
        case CMD_LINEOUT:
            if (cmd.arg) lineoutStart(); else lineoutStop();
            break;
        case CMD_LINELEVEL:
            setLineGain(constrain(cmd.arg, 0, 100));
            break;
        case CMD_MIGRATE: // preferred stream recovered: deliberate switch (not a failure)
            if (wantPlaying && cmd.arg >= 0 && cmd.arg < (int)urls.size() && cmd.arg < urlIndex) {
                Serial.printf("[player] migrating back to URL %d\n", (int)cmd.arg + 1);
                urlIndex = cmd.arg;
                backoffMs = 1000;
                startCurrentUrl();
            }
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
    audio.setVolume(21); // pinned at unity — volume is applied in the PCM hook (see audio_process_i2s)
    setVolGain(config.volume);
    setLineGain(config.lineOutLevel);
    audio.setConnectionTimeout(5000, 7000);
    audio.setPrebuffer(65536); // ~4s @128kbps before playback starts — no start-of-stream stutter
    if (config.lineOutFixed) lineoutStart();

    xSemaphoreTake(urlsMutex, portMAX_DELAY);
    urls = config.streamUrls;
    xSemaphoreGive(urlsMutex);
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

// ---- Preferred-stream recovery (core 1, low priority) ----
// While playing a lower-priority URL, periodically open the better URLs and
// verify bytes actually flow (no decoding — pure I/O). Two consecutive healthy
// probes of the same URL trigger a migration back.

#include <HTTPClient.h>

static void setProbeMsg(const char *msg) {
    xSemaphoreTake(statusMutex, portMAX_DELAY);
    strlcpy(status.probeMsg, msg, sizeof(status.probeMsg));
    xSemaphoreGive(statusMutex);
}

static bool probeUrl(const String &url) {
    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(url)) return false;
    bool ok = false;
    if (http.GET() == HTTP_CODE_OK) {
        WiFiClient *s = http.getStreamPtr();
        uint8_t buf[1024];
        uint32_t got = 0;
        uint32_t start = millis(), lastData = start;
        while (millis() - start < 6000) {
            size_t avail = s->available();
            if (avail) {
                int n = s->readBytes(buf, min(avail, sizeof(buf)));
                if (n > 0) {
                    got += n;
                    lastData = millis();
                }
            } else {
                if (!http.connected() || millis() - lastData > 2500) break;
                delay(20);
            }
        }
        ok = got >= 30000; // sustained >= ~40kbps for 6s = actually serving audio
    }
    http.end();
    return ok;
}

static void probeTask(void *) {
    uint32_t nextProbeMs = 0;
    int lastCandidate = -1;
    int streak = 0;
    int prevIdx = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));

        PlayerStatus st;
        playerGetStatus(st);
        if (!config.preferredResume || !st.playing || st.urlIndex == 0 || WiFi.status() != WL_CONNECTED) {
            if (st.urlIndex == 0 && status.probeMsg[0]) setProbeMsg("");
            streak = 0;
            lastCandidate = -1;
            if (prevIdx == 0 && st.urlIndex > 0) nextProbeMs = millis() + 90000; // settle first
            prevIdx = st.urlIndex;
            continue;
        }
        if (prevIdx == 0) nextProbeMs = millis() + 90000; // just failed over: let it settle
        prevIdx = st.urlIndex;
        if (millis() < nextProbeMs) continue;

        // Snapshot the better-priority URLs
        std::vector<String> candidates;
        xSemaphoreTake(urlsMutex, portMAX_DELAY);
        for (int i = 0; i < st.urlIndex && i < (int)urls.size(); i++) candidates.push_back(urls[i]);
        xSemaphoreGive(urlsMutex);

        int passed = -1;
        for (int i = 0; i < (int)candidates.size(); i++) {
            char msg[64];
            snprintf(msg, sizeof(msg), "checking URL %d…", i + 1);
            setProbeMsg(msg);
            if (probeUrl(candidates[i])) {
                passed = i;
                break;
            }
        }

        if (passed >= 0 && passed == lastCandidate) streak++;
        else streak = (passed >= 0) ? 1 : 0;
        lastCandidate = passed;

        if (streak >= 2) {
            char msg[64];
            snprintf(msg, sizeof(msg), "URL %d recovered, switching back", passed + 1);
            setProbeMsg(msg);
            send(CMD_MIGRATE, passed);
            streak = 0;
            lastCandidate = -1;
            nextProbeMs = millis() + 300000; // if the migration bounces, don't flap
        } else {
            setProbeMsg(passed >= 0 ? "preferred URL improving…" : "preferred URL still down");
            nextProbeMs = millis() + 60000;
        }
    }
}

void playerBegin() {
    if (muteAvailable()) {
        pinMode(PIN_EXT_AMP_SD, OUTPUT);
    } else {
        Serial.println("[player] GPIO14 carries line-out data — amp mute is data-level only");
    }
    applySpeakers(config.speakersEnabled);

    pinMode(PIN_AMP_ENABLE, OUTPUT);
    digitalWrite(PIN_AMP_ENABLE, HIGH); // amp in shutdown until the codec is ready

    cmdQueue = xQueueCreate(8, sizeof(Cmd));
    statusMutex = xSemaphoreCreateMutex();
    urlsMutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 5, &audioTaskHandle, 0);
    xTaskCreatePinnedToCore(probeTask, "probe", 8192, nullptr, 1, nullptr, 1); // low prio, UI core

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

static void send(CmdType t, int32_t arg) {
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
void playerSetLineOutFixed(bool fixed) { send(CMD_LINEOUT, fixed ? 1 : 0); }
void playerSetLineOutLevel(uint8_t pct) { send(CMD_LINELEVEL, pct); }

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
    titleSetMs = millis();
}

// Called by the library with each decoded PCM block before it goes to I2S.
// Order matters: (1) full-scale copy to the fixed line-out, (2) meter the
// full-scale signal, (3) apply the volume in place for the amps/onboard codec.
void audio_process_i2s(int16_t *buff, uint16_t validSamples, uint8_t bitsPerSample, uint8_t channels, bool *continueI2S) {
    *continueI2S = true;
    if (bitsPerSample != 16 || channels != 2) return;

    if (lineoutActive()) {
        int32_t lg = lineGainQ15;
        if (lg >= 32768) {
            lineoutWrite(buff, validSamples); // full level: no copy needed
        } else {
            static int16_t tmp[512]; // audio task only; chunked to keep it off the heap
            uint16_t done = 0;
            while (done < validSamples) {
                uint16_t n = min<uint16_t>(256, validSamples - done);
                for (uint16_t i = 0; i < n * 2; i++) {
                    tmp[i] = (int16_t)(((int32_t)buff[done * 2 + i] * lg) >> 15);
                }
                lineoutWrite(tmp, n);
                done += n;
            }
        }
    }

    int32_t g = extMute ? 0 : volGainQ15;
    for (uint16_t i = 0; i < validSamples; i++) {
        int32_t l = buff[2 * i], r = buff[2 * i + 1];
        vuAccL += (uint64_t)((int64_t)l * l);
        vuAccR += (uint64_t)((int64_t)r * r);
        buff[2 * i] = (int16_t)((l * g) >> 15);
        buff[2 * i + 1] = (int16_t)((r * g) >> 15);
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
