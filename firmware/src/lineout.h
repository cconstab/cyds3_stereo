#pragma once
#include <Arduino.h>

// Fixed-level line-out on I2S1 (TX slave, clocked by the I2S0 bus on GPIO 2/3,
// data out on PIN_I2S_LINE_DOUT). Carries full-scale samples so the PCM5102A
// level is independent of the UI volume. All calls from the audio task only.
bool lineoutStart();
void lineoutStop();
bool lineoutActive();
void lineoutWrite(const int16_t *samples, size_t frames); // 16-bit stereo frames
