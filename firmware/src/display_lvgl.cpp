// TFT_eSPI + FT6336U -> LVGL glue for the FNK0104S (landscape 480x320).
#include "display_lvgl.h"
#include "pins.h"
#include "app_config.h"
#include <TFT_eSPI.h>
#include <FT6336U.h>
#include <Wire.h>

static TFT_eSPI tft;
static FT6336U ctp(PIN_I2C_SDA, PIN_I2C_SCL, PIN_TOUCH_RST, PIN_TOUCH_INT);

// Landscape: swap the panel's portrait dimensions from the TFT_eSPI build flags
static const uint16_t HOR_RES = TFT_HEIGHT;
static const uint16_t VER_RES = TFT_WIDTH;
static const size_t BUF_PIXELS = HOR_RES * 40;

static lv_disp_draw_buf_t drawBuf;
static lv_color_t *buf1;
static lv_color_t *buf2;

static void flushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *pixels) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&pixels->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

static bool screenOff = false;
static uint32_t wakeGraceUntil = 0;

static void touchCb(lv_indev_drv_t *, lv_indev_data_t *data) {
    FT6336U_TouchPointType tp = ctp.scan();
    // Screen off: the first touch only wakes the display — swallow it (and a short
    // grace window) so the wake tap can't press whatever widget is underneath.
    if (screenOff || millis() < wakeGraceUntil) {
        if (tp.touch_count > 0 && screenOff) {
            displayScreenWake();
            wakeGraceUntil = millis() + 400;
        }
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    if (tp.touch_count > 0) {
        // Panel native orientation is portrait 320x480; map to landscape rotation 1.
        uint16_t rawX = tp.tp[0].x; // 0..319
        uint16_t rawY = tp.tp[0].y; // 0..479
        data->point.x = rawY;
        data->point.y = (VER_RES - 1) - rawX;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// Boot-time hardware probe + raw-TFT self-test, before LVGL is involved.
// Serial output tells us which panel/controllers we're actually talking to.
static void displaySelfTest() {
    Serial.println("[disp] i2c scan (SDA=16 SCL=15):");
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            const char *name = addr == 0x38 ? "FT6336U touch" : addr == 0x18 ? "ES8311 codec" : "?";
            Serial.printf("[disp]   found 0x%02X (%s)\n", addr, name);
        }
    }
    Serial.println("[disp] i2c scan done");

    struct { const char *name; uint16_t color; } steps[] = {
        {"RED", TFT_RED}, {"GREEN", TFT_GREEN}, {"BLUE", TFT_BLUE}, {"WHITE", TFT_WHITE},
    };
    for (auto &s : steps) {
        Serial.printf("[disp] selftest fill %s\n", s.name);
        tft.fillScreen(s.color);
        delay(500);
    }
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    char msg[40];
    snprintf(msg, sizeof(msg), "DISPLAY OK %dx%d", HOR_RES, VER_RES);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(msg, HOR_RES / 2, VER_RES / 2, 4);
    tft.setTextDatum(TL_DATUM);
    Serial.println("[disp] selftest text drawn");
    delay(1200);
}

void displayBegin() {
    // Backlight PWM (LEDC), full brightness during boot
    ledcAttach(PIN_LCD_BL, 20000, 8);
    ledcWrite(PIN_LCD_BL, 255);

    Serial.printf("[disp] tft.init() %dx%d, MOSI=11 SCLK=12 CS=10 DC=46 BL=45\n", HOR_RES, VER_RES);
    tft.init();
    tft.setRotation(1); // landscape, USB on the right
    tft.fillScreen(TFT_BLACK);

    if (config.bootSelfTest) displaySelfTest();

    ctp.begin();

    lv_init();
    buf1 = (lv_color_t *)heap_caps_malloc(BUF_PIXELS * sizeof(lv_color_t), MALLOC_CAP_DMA);
    buf2 = (lv_color_t *)heap_caps_malloc(BUF_PIXELS * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_disp_draw_buf_init(&drawBuf, buf1, buf2, BUF_PIXELS);

    static lv_disp_drv_t dispDrv;
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res = HOR_RES;
    dispDrv.ver_res = VER_RES;
    dispDrv.flush_cb = flushCb;
    dispDrv.draw_buf = &drawBuf;
    lv_disp_drv_register(&dispDrv);

    static lv_indev_drv_t indevDrv;
    lv_indev_drv_init(&indevDrv);
    indevDrv.type = LV_INDEV_TYPE_POINTER;
    indevDrv.read_cb = touchCb;
    lv_indev_drv_register(&indevDrv);
}

void displaySetBrightness(uint8_t pct) {
    if (screenOff) return; // applied on wake instead
    pct = constrain(pct, 5, 100);
    ledcWrite(PIN_LCD_BL, (uint32_t)pct * 255 / 100);
}

void displayScreenOff() {
    screenOff = true;
    ledcWrite(PIN_LCD_BL, 0);
}

void displayScreenWake() {
    screenOff = false;
    displaySetBrightness(config.brightness);
}

bool displayScreenIsOff() { return screenOff; }

void displayLoop() {
    lv_timer_handler();
}
