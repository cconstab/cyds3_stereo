// Freenove FNK0104S (4.0" ST7796) pin map.
// Sources: Freenove TFT_eSPI_Setups v1.2 + Tutorial_With_Touch sketches (FNK0104S branch)
// and the 4.0inch_ESP32-S3_Display schematic.
#pragma once

// --- Display (ST7796, SPI) — configured via TFT_eSPI build flags in platformio.ini ---
#define PIN_LCD_BL 45 // backlight, LEDC PWM (TFT_eSPI does not drive it)

// --- Touch (FT6336U, I2C bus shared with ES8311 codec) ---
#define PIN_I2C_SDA 16
#define PIN_I2C_SCL 15
#define PIN_TOUCH_RST 18
#define PIN_TOUCH_INT 17

// --- Internal audio: ES8311 mono codec + SC8002B speaker amp ---
#define PIN_I2S_INT_MCK 4
#define PIN_I2S_INT_BCK 5
#define PIN_I2S_INT_DIN 6 // codec ADC -> ESP
#define PIN_I2S_INT_WS 7
#define PIN_I2S_INT_DOUT 8 // ESP -> codec DAC
#define PIN_AMP_ENABLE 1   // SC8002B SHUTDOWN, active high: LOW = amp on, HIGH = off

// --- External stereo I2S bus (Extended IO connector) ---
// Fans out to 2x MAX98357A (speakers) + PCM5102A (RCA line out) in parallel.
#define PIN_I2S_EXT_BCK 2
#define PIN_I2S_EXT_WS 3
#define PIN_I2S_EXT_DOUT 21
#define PIN_EXT_AMP_SD 14 // MAX98357A SD_MODE: LOW = amps muted (line-out stays live)
// Optional fixed-level line-out (config.lineOutFixed): PCM5102A DIN moves here,
// fed full-scale by I2S1 slave. TXD0 pin on the UART connector (console is USB-CDC).
#define PIN_I2S_LINE_DOUT 43

// --- SD card (4-bit SDMMC) ---
#define PIN_SD_CLK 38
#define PIN_SD_CMD 40
#define PIN_SD_D0 39
#define PIN_SD_D1 41
#define PIN_SD_D2 48
#define PIN_SD_D3 47

// --- Misc ---
#define PIN_RGB_LED 42 // WS2812B status LED
#define PIN_KEY_BOOT 0
