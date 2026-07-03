// Touch UI (LVGL 8): Now Playing view with VU meters + a settings screen.
#include "ui.h"
#include "app_config.h"
#include "player.h"
#include "net.h"
#include "ota.h"
#include "display_lvgl.h"
#include <lvgl.h>

// ---- Now Playing screen ----
static lv_obj_t *scrMain;
static lv_obj_t *lblStation;
static lv_obj_t *lblTitle;
static lv_obj_t *lblWifi;
static lv_obj_t *lblStatus;
static lv_obj_t *barVuL, *barVuR;
static lv_obj_t *barBuffer;
static lv_obj_t *btnPlayLabel;
static lv_obj_t *sliderVol;

// ---- Settings screen ----
static lv_obj_t *scrSettings;
static lv_obj_t *lblInfo;
static lv_obj_t *swSpeakers;
static lv_obj_t *sliderBright;
static lv_obj_t *lblOta;

static bool lastPlaying = false;

static void styleVu(lv_obj_t *bar) {
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x22c55e), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, 127);
}

static void buildMain() {
    scrMain = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scrMain, lv_color_hex(0x0b0f14), 0);

    lblStation = lv_label_create(scrMain);
    lv_obj_set_style_text_font(lblStation, &lv_font_montserrat_28, 0);
    lv_obj_set_width(lblStation, 380);
    lv_label_set_long_mode(lblStation, LV_LABEL_LONG_DOT);
    lv_obj_align(lblStation, LV_ALIGN_TOP_LEFT, 12, 10);
    lv_label_set_text(lblStation, "CYD-S3 Stereo");

    lblWifi = lv_label_create(scrMain);
    lv_obj_set_style_text_font(lblWifi, &lv_font_montserrat_14, 0);
    lv_obj_align(lblWifi, LV_ALIGN_TOP_RIGHT, -12, 16);
    lv_label_set_text(lblWifi, LV_SYMBOL_WIFI " --");

    lblTitle = lv_label_create(scrMain);
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(0x93c5fd), 0);
    lv_obj_set_width(lblTitle, 456);
    lv_label_set_long_mode(lblTitle, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(lblTitle, LV_ALIGN_TOP_LEFT, 12, 52);
    lv_label_set_text(lblTitle, "—");

    // VU meters
    lv_obj_t *lblL = lv_label_create(scrMain);
    lv_label_set_text(lblL, "L");
    lv_obj_align(lblL, LV_ALIGN_TOP_LEFT, 12, 96);
    barVuL = lv_bar_create(scrMain);
    lv_obj_set_size(barVuL, 430, 22);
    lv_obj_align(barVuL, LV_ALIGN_TOP_LEFT, 34, 94);
    styleVu(barVuL);

    lv_obj_t *lblR = lv_label_create(scrMain);
    lv_label_set_text(lblR, "R");
    lv_obj_align(lblR, LV_ALIGN_TOP_LEFT, 12, 128);
    barVuR = lv_bar_create(scrMain);
    lv_obj_set_size(barVuR, 430, 22);
    lv_obj_align(barVuR, LV_ALIGN_TOP_LEFT, 34, 126);
    styleVu(barVuR);

    // Buffer + status line
    barBuffer = lv_bar_create(scrMain);
    lv_obj_set_size(barBuffer, 430, 6);
    lv_obj_align(barBuffer, LV_ALIGN_TOP_LEFT, 34, 158);
    lv_bar_set_range(barBuffer, 0, 100);
    lv_obj_set_style_bg_color(barBuffer, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_bg_color(barBuffer, lv_color_hex(0x3b82f6), LV_PART_INDICATOR);

    lblStatus = lv_label_create(scrMain);
    lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblStatus, lv_color_hex(0x9ca3af), 0);
    lv_obj_align(lblStatus, LV_ALIGN_TOP_LEFT, 12, 174);
    lv_label_set_text(lblStatus, "starting…");

    // Controls row
    auto mkBtn = [](lv_obj_t *parent, const char *txt, lv_coord_t x, lv_event_cb_t cb) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, 72, 52);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, x, -58);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, txt);
        lv_obj_center(l);
        return l;
    };
    mkBtn(scrMain, LV_SYMBOL_PREV, 12, [](lv_event_t *) { playerPrevUrl(); });
    btnPlayLabel = mkBtn(scrMain, LV_SYMBOL_PLAY, 92, [](lv_event_t *) {
        PlayerStatus ps;
        playerGetStatus(ps);
        if (ps.wantPlaying) playerStop(); else playerPlay();
    });
    mkBtn(scrMain, LV_SYMBOL_NEXT, 172, [](lv_event_t *) { playerNextUrl(); });
    lv_obj_t *gearLbl = mkBtn(scrMain, LV_SYMBOL_SETTINGS, 396, [](lv_event_t *) {
        lv_scr_load(scrSettings);
    });
    (void)gearLbl;

    // Volume slider
    sliderVol = lv_slider_create(scrMain);
    lv_slider_set_range(sliderVol, 0, 21);
    lv_slider_set_value(sliderVol, config.volume, LV_ANIM_OFF);
    lv_obj_set_size(sliderVol, 456, 14);
    lv_obj_align(sliderVol, LV_ALIGN_BOTTOM_LEFT, 12, -22);
    lv_obj_add_event_cb(sliderVol, [](lv_event_t *e) {
        int v = lv_slider_get_value(lv_event_get_target(e));
        config.volume = v;
        playerSetVolume(v);
        if (lv_event_get_code(e) == LV_EVENT_RELEASED) configSave();
    }, LV_EVENT_ALL, nullptr);
}

static void buildSettings() {
    scrSettings = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scrSettings, lv_color_hex(0x0b0f14), 0);

    lv_obj_t *title = lv_label_create(scrSettings);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "Settings");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 10);

    lv_obj_t *btnBack = lv_btn_create(scrSettings);
    lv_obj_set_size(btnBack, 72, 40);
    lv_obj_align(btnBack, LV_ALIGN_TOP_RIGHT, -12, 6);
    lv_obj_add_event_cb(btnBack, [](lv_event_t *) { lv_scr_load(scrMain); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl = lv_label_create(btnBack);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(bl);

    // Brightness
    lv_obj_t *lb = lv_label_create(scrSettings);
    lv_label_set_text(lb, "Brightness");
    lv_obj_align(lb, LV_ALIGN_TOP_LEFT, 12, 60);
    sliderBright = lv_slider_create(scrSettings);
    lv_slider_set_range(sliderBright, 5, 100);
    lv_slider_set_value(sliderBright, config.brightness, LV_ANIM_OFF);
    lv_obj_set_size(sliderBright, 300, 14);
    lv_obj_align(sliderBright, LV_ALIGN_TOP_LEFT, 150, 62);
    lv_obj_add_event_cb(sliderBright, [](lv_event_t *e) {
        int v = lv_slider_get_value(lv_event_get_target(e));
        config.brightness = v;
        displaySetBrightness(v);
        if (lv_event_get_code(e) == LV_EVENT_RELEASED) configSave();
    }, LV_EVENT_ALL, nullptr);

    // Speakers switch
    lv_obj_t *ls = lv_label_create(scrSettings);
    lv_label_set_text(ls, "Speakers (line-out always on)");
    lv_obj_align(ls, LV_ALIGN_TOP_LEFT, 12, 104);
    swSpeakers = lv_switch_create(scrSettings);
    lv_obj_align(swSpeakers, LV_ALIGN_TOP_LEFT, 380, 98);
    if (config.speakersEnabled) lv_obj_add_state(swSpeakers, LV_STATE_CHECKED);
    lv_obj_add_event_cb(swSpeakers, [](lv_event_t *e) {
        bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        config.speakersEnabled = on;
        playerSetSpeakers(on);
        configSave();
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    // Check update button
    lv_obj_t *btnUpd = lv_btn_create(scrSettings);
    lv_obj_set_size(btnUpd, 220, 44);
    lv_obj_align(btnUpd, LV_ALIGN_TOP_LEFT, 12, 148);
    lv_obj_add_event_cb(btnUpd, [](lv_event_t *) { otaCheckNow(true); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *ul = lv_label_create(btnUpd);
    lv_label_set_text(ul, LV_SYMBOL_DOWNLOAD " Check for update");
    lv_obj_center(ul);

    // WiFi portal button
    lv_obj_t *btnWifi = lv_btn_create(scrSettings);
    lv_obj_set_size(btnWifi, 220, 44);
    lv_obj_align(btnWifi, LV_ALIGN_TOP_LEFT, 248, 148);
    lv_obj_set_style_bg_color(btnWifi, lv_color_hex(0x374151), 0);
    lv_obj_add_event_cb(btnWifi, [](lv_event_t *) {
        playerStop();
        netStartPortal();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *wl = lv_label_create(btnWifi);
    lv_label_set_text(wl, LV_SYMBOL_WIFI " WiFi setup");
    lv_obj_center(wl);

    lblOta = lv_label_create(scrSettings);
    lv_obj_set_style_text_color(lblOta, lv_color_hex(0x9ca3af), 0);
    lv_obj_align(lblOta, LV_ALIGN_TOP_LEFT, 12, 206);
    lv_label_set_text(lblOta, "");

    lblInfo = lv_label_create(scrSettings);
    lv_obj_set_style_text_font(lblInfo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblInfo, lv_color_hex(0x9ca3af), 0);
    lv_obj_align(lblInfo, LV_ALIGN_BOTTOM_LEFT, 12, -12);
    lv_label_set_text(lblInfo, "");
}

void uiBegin() {
    buildMain();
    buildSettings();
    lv_scr_load(scrMain);
    displaySetBrightness(config.brightness);
}

void uiUpdate() {
    PlayerStatus ps;
    playerGetStatus(ps);

    lv_bar_set_value(barVuL, ps.playing ? ps.vuLeft : 0, LV_ANIM_OFF);
    lv_bar_set_value(barVuR, ps.playing ? ps.vuRight : 0, LV_ANIM_OFF);
    lv_bar_set_value(barBuffer, ps.bufferPct, LV_ANIM_OFF);

    lv_label_set_text(lblStation, ps.station[0] ? ps.station : config.stationName.c_str());
    lv_label_set_text(lblTitle, ps.title[0] ? ps.title : "—");

    if (ps.wantPlaying != lastPlaying) {
        lastPlaying = ps.wantPlaying;
        lv_label_set_text(btnPlayLabel, ps.wantPlaying ? LV_SYMBOL_STOP : LV_SYMBOL_PLAY);
    }

    char wifiTxt[48];
    if (netMode() == NetMode::PORTAL) {
        snprintf(wifiTxt, sizeof(wifiTxt), "AP: %s", netApName());
    } else {
        snprintf(wifiTxt, sizeof(wifiTxt), LV_SYMBOL_WIFI " %d dBm", netRssi());
    }
    lv_label_set_text(lblWifi, wifiTxt);

    char statusTxt[160];
    if (netMode() == NetMode::PORTAL) {
        snprintf(statusTxt, sizeof(statusTxt), "Setup: join WiFi '%s', open http://%s", netApName(), netIp().c_str());
    } else {
        snprintf(statusTxt, sizeof(statusTxt), "%s  ·  URL %d/%d  ·  %lu kbps  ·  http://%s",
                 ps.playing ? "PLAYING" : (ps.wantPlaying ? "RECONNECTING…" : "STOPPED"),
                 ps.urlCount ? ps.urlIndex + 1 : 0, ps.urlCount,
                 (unsigned long)(ps.bitrate / 1000), netIp().c_str());
    }
    lv_label_set_text(lblStatus, statusTxt);

    // Settings screen extras
    OtaState os;
    otaGetState(os);
    char otaTxt[128];
    snprintf(otaTxt, sizeof(otaTxt), "Firmware update: %s%s%s", os.message,
             os.inProgress ? " " : "", os.inProgress ? "…" : "");
    lv_label_set_text(lblOta, otaTxt);

    char infoTxt[128];
    snprintf(infoTxt, sizeof(infoTxt), "fw %s  ·  %s  ·  http://%s  ·  reconnects %lu",
             FW_VERSION, config.hostname.c_str(), netIp().c_str(), (unsigned long)ps.reconnects);
    lv_label_set_text(lblInfo, infoTxt);
}
