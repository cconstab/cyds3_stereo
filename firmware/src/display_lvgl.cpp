// TFT_eSPI + FT6336U -> LVGL glue for the FNK0104S (landscape 480x320).
#include "display_lvgl.h"
#include "pins.h"
#include <TFT_eSPI.h>
#include <FT6336U.h>

static TFT_eSPI tft;
static FT6336U ctp(PIN_I2C_SDA, PIN_I2C_SCL, PIN_TOUCH_RST, PIN_TOUCH_INT);

static const uint16_t HOR_RES = 480;
static const uint16_t VER_RES = 320;
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

static void touchCb(lv_indev_drv_t *, lv_indev_data_t *data) {
    FT6336U_TouchPointType tp = ctp.scan();
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

void displayBegin() {
    // Backlight PWM (LEDC), full brightness during boot
    ledcAttach(PIN_LCD_BL, 20000, 8);
    ledcWrite(PIN_LCD_BL, 255);

    tft.init();
    tft.setRotation(1); // landscape, USB on the right
    tft.fillScreen(TFT_BLACK);

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
    pct = constrain(pct, 5, 100);
    ledcWrite(PIN_LCD_BL, (uint32_t)pct * 255 / 100);
}

void displayLoop() {
    lv_timer_handler();
}
