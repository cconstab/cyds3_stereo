/* LVGL 8.4 configuration — CYD-S3 Stereo (FNK0104S 480x320)
 * Only overrides; everything else falls back to lv_conf_internal.h defaults. */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Static LVGL heap — 4 screens incl. keyboard + 48 VU segments need headroom;
 * OOM in LVGL hangs in an assert loop (blank screen), so keep this generous. */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (96U * 1024U)

/* Use Arduino millis() for ticks */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 30

/* Fonts */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#define LV_USE_PERF_MONITOR 0
#define LV_USE_LOG 0

#endif /* LV_CONF_H */
