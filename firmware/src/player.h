#pragma once
#include <Arduino.h>

// Player status snapshot, safe to read from the UI/web core.
struct PlayerStatus {
    bool playing = false;
    bool wantPlaying = false;
    int urlIndex = 0;
    int urlCount = 0;
    uint32_t bitrate = 0;
    uint8_t vuLeft = 0;   // 0..127
    uint8_t vuRight = 0;  // 0..127
    uint8_t bufferPct = 0;
    uint32_t reconnects = 0;
    char station[64] = "";
    char title[128] = "";
    char currentUrl[160] = "";
    char lastError[96] = "";
};

void playerBegin();                 // spawns the audio task on core 0
void playerGetStatus(PlayerStatus &out);
void playerGetVu(uint8_t &left, uint8_t &right); // lightweight, for high-rate meter updates

// Commands (thread-safe, marshalled to the audio task)
void playerPlay();
void playerStop();
void playerNextUrl();
void playerPrevUrl();
void playerSetVolume(uint8_t vol);      // 0..21
void playerReloadUrls();                // re-read config.streamUrls
void playerSetSpeakers(bool enabled);   // MAX98357A SD_MODE pin

// Onboard mono speaker (ES8311 codec + amp). NOT thread-safe: call from the
// UI/web core only — its I2C bus is shared with the touch controller.
void playerSetOnboardSpeaker(bool enabled);
