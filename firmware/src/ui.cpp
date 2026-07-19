// Touch UI (LVGL 8): Now Playing view with VU meters + a settings screen.
// Layout adapts to the panel: 480x320 (FNK0104S) or 320x240 (FNK0104B).
#include "ui.h"
#include "app_config.h"
#include "player.h"
#include "net.h"
#include "ota.h"
#include "display_lvgl.h"
#include <lvgl.h>
#include <WiFi.h>

// Resolution-dependent metrics, set in uiBegin()
static lv_coord_t W, H;
static bool big;
static const lv_font_t *fontBig;
static const lv_font_t *fontMid;
static const int PAD = 10;

// ---- Now Playing screen ----
static lv_obj_t *scrMain;
static lv_obj_t *lblStation;
static lv_obj_t *lblTitle;
static lv_obj_t *lblWifi;
static lv_obj_t *lblStatus;
static lv_obj_t *lblCfgUrl;
static const int VU_SEGS = 24;
static lv_obj_t *vuSegL[VU_SEGS], *vuSegR[VU_SEGS];
static lv_obj_t *barBuffer;
static lv_obj_t *btnPlayLabel;
static lv_obj_t *sliderVol;

// ---- Settings screen ----
static lv_obj_t *scrSettings;
static lv_obj_t *lblInfo;
static lv_obj_t *swSpeakers;
static lv_obj_t *sliderBright;
static lv_obj_t *lblOta;

// ---- WiFi setup screen ----
static lv_obj_t *scrWifi;
static lv_obj_t *ddSsid;
static lv_obj_t *lblWifiInfo;
static char pendingPass[65] = "";
static bool scanning = false;

// ---- Text editor screen (shared: WiFi password / station URLs) ----
enum EditTarget { ET_NONE, ET_PASS, ET_URLS };
static lv_obj_t *scrEdit;
static lv_obj_t *editTa;
static lv_obj_t *lblEditTitle;
static EditTarget editTarget = ET_NONE;

static bool lastPlaying = false;

// Classic LED VU: green -> amber -> red segments, unlit ones glow dimly.
static lv_color_t vuSegColor(int i) {
    if (i >= VU_SEGS * 85 / 100) return lv_color_hex(0xef4444); // top 15%: red
    if (i >= VU_SEGS * 60 / 100) return lv_color_hex(0xf59e0b); // 60-85%: amber
    return lv_color_hex(0x22c55e);                              // rest: green
}

static void mkVuRow(lv_obj_t *parent, lv_obj_t **segs, int x, int y, int totalW, int h) {
    const int gap = 2;
    const int segW = (totalW - gap * (VU_SEGS - 1)) / VU_SEGS;
    for (int i = 0; i < VU_SEGS; i++) {
        lv_obj_t *s = lv_obj_create(parent);
        lv_obj_remove_style_all(s);
        lv_obj_set_size(s, segW, h);
        lv_obj_set_pos(s, x + i * (segW + gap), y);
        lv_obj_set_style_radius(s, 2, 0);
        lv_obj_set_style_bg_color(s, vuSegColor(i), 0);
        lv_obj_set_style_bg_opa(s, LV_OPA_20, 0);
        segs[i] = s;
    }
}

static void setVuLevel(lv_obj_t **segs, uint8_t level, uint8_t peak) {
    int lit = (level * VU_SEGS + 63) / 127;
    int peakIdx = (peak * VU_SEGS + 63) / 127 - 1; // lingering peak-hold segment
    for (int i = 0; i < VU_SEGS; i++) {
        bool on = i < lit || (i == peakIdx && peakIdx >= lit);
        lv_obj_set_style_bg_opa(segs[i], on ? LV_OPA_COVER : LV_OPA_20, 0);
    }
}

// Muted styling — the theme's default blue is too loud for a dark dashboard.
static void styleBtn(lv_obj_t *btn) {
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x263241), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x33455c), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 0, 0);
}

static void styleSlider(lv_obj_t *slider) {
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x1a2029), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x475569), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x8494a8), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 3, LV_PART_KNOB);
}

static void buildMain() {
    const int titleY = big ? 52 : 40;
    const int vuY = big ? 94 : 72;
    const int vuH = big ? 22 : 16;
    const int vuGap = big ? 32 : 22;
    const int barW = W - PAD * 2 - 24;
    const int btnW = big ? 72 : 60;
    const int btnH = big ? 52 : 42;
    const int volBottom = big ? -20 : -12;
    const int btnBottom = volBottom - (big ? 28 : 20); // sits lower: breathing room for the config-URL line

    scrMain = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scrMain, lv_color_hex(0x0b0f14), 0);

    lblStation = lv_label_create(scrMain);
    lv_obj_set_style_text_font(lblStation, fontBig, 0);
    lv_obj_set_width(lblStation, W - 110);
    lv_label_set_long_mode(lblStation, LV_LABEL_LONG_DOT);
    lv_obj_align(lblStation, LV_ALIGN_TOP_LEFT, PAD, 8);
    lv_label_set_text(lblStation, "CYD-S3 Stereo");

    lblWifi = lv_label_create(scrMain);
    lv_obj_set_style_text_font(lblWifi, &lv_font_montserrat_12, 0);
    lv_obj_align(lblWifi, LV_ALIGN_TOP_RIGHT, -PAD, 12);
    lv_label_set_text(lblWifi, LV_SYMBOL_WIFI " --");

    lblTitle = lv_label_create(scrMain);
    lv_obj_set_style_text_font(lblTitle, fontMid, 0);
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(0x93c5fd), 0);
    lv_obj_set_width(lblTitle, W - PAD * 2);
    lv_label_set_long_mode(lblTitle, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(lblTitle, LV_ALIGN_TOP_LEFT, PAD, titleY);
    lv_label_set_text(lblTitle, "-");

    // VU meters
    lv_obj_t *lblL = lv_label_create(scrMain);
    lv_label_set_text(lblL, "L");
    lv_obj_align(lblL, LV_ALIGN_TOP_LEFT, PAD, vuY + 2);
    mkVuRow(scrMain, vuSegL, PAD + 24, vuY, barW, vuH);

    lv_obj_t *lblR = lv_label_create(scrMain);
    lv_label_set_text(lblR, "R");
    lv_obj_align(lblR, LV_ALIGN_TOP_LEFT, PAD, vuY + vuGap + 2);
    mkVuRow(scrMain, vuSegR, PAD + 24, vuY + vuGap, barW, vuH);

    // Buffer + status line
    barBuffer = lv_bar_create(scrMain);
    lv_obj_set_size(barBuffer, barW, 5);
    lv_obj_align(barBuffer, LV_ALIGN_TOP_LEFT, PAD + 24, vuY + vuGap * 2);
    lv_bar_set_range(barBuffer, 0, 100);
    lv_obj_set_style_bg_color(barBuffer, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_bg_color(barBuffer, lv_color_hex(0x3b82f6), LV_PART_INDICATOR);

    lblStatus = lv_label_create(scrMain);
    lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblStatus, lv_color_hex(0x9ca3af), 0);
    lv_obj_set_width(lblStatus, W - PAD * 2);
    lv_label_set_long_mode(lblStatus, LV_LABEL_LONG_DOT);
    lv_obj_align(lblStatus, LV_ALIGN_TOP_LEFT, PAD, vuY + vuGap * 2 + 12);
    lv_label_set_text(lblStatus, "starting...");

    // Web-config URL (shown while the web interface is enabled and online)
    lblCfgUrl = lv_label_create(scrMain);
    lv_obj_set_style_text_font(lblCfgUrl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblCfgUrl, lv_color_hex(0x6b7f95), 0);
    lv_obj_set_width(lblCfgUrl, W - PAD * 2);
    lv_label_set_long_mode(lblCfgUrl, LV_LABEL_LONG_DOT);
    lv_obj_align(lblCfgUrl, LV_ALIGN_TOP_LEFT, PAD, vuY + vuGap * 2 + (big ? 30 : 28));
    lv_label_set_text(lblCfgUrl, "");

    // Controls row
    auto mkBtn = [&](const char *txt, int x, lv_event_cb_t cb) {
        lv_obj_t *btn = lv_btn_create(scrMain);
        styleBtn(btn);
        lv_obj_set_size(btn, btnW, btnH);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, x, btnBottom);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, txt);
        lv_obj_center(l);
        return l;
    };
    mkBtn(LV_SYMBOL_PREV, PAD, [](lv_event_t *) { playerPrevUrl(); });
    btnPlayLabel = mkBtn(LV_SYMBOL_PLAY, PAD + btnW + 8, [](lv_event_t *) {
        PlayerStatus ps;
        playerGetStatus(ps);
        if (ps.wantPlaying) playerStop(); else playerPlay();
    });
    mkBtn(LV_SYMBOL_NEXT, PAD + (btnW + 8) * 2, [](lv_event_t *) { playerNextUrl(); });
    mkBtn(LV_SYMBOL_SETTINGS, W - PAD - btnW, [](lv_event_t *) { lv_scr_load(scrSettings); });

    // Volume slider
    sliderVol = lv_slider_create(scrMain);
    styleSlider(sliderVol);
    lv_slider_set_range(sliderVol, 0, 21);
    lv_slider_set_value(sliderVol, config.volume, LV_ANIM_OFF);
    lv_obj_set_size(sliderVol, W - PAD * 2 - 8, big ? 14 : 10);
    lv_obj_align(sliderVol, LV_ALIGN_BOTTOM_LEFT, PAD + 4, volBottom);
    lv_obj_add_event_cb(sliderVol, [](lv_event_t *e) {
        int v = lv_slider_get_value(lv_event_get_target(e));
        config.volume = v;
        playerSetVolume(v);
        if (lv_event_get_code(e) == LV_EVENT_RELEASED) configSave();
    }, LV_EVENT_ALL, nullptr);
}

// ---- Shared text editor with touch keyboard ----

static void openEditor(EditTarget target, const char *title, const char *initial, bool oneLine) {
    editTarget = target;
    lv_label_set_text(lblEditTitle, title);
    lv_textarea_set_one_line(editTa, oneLine);
    lv_textarea_set_text(editTa, initial);
    lv_scr_load(scrEdit);
}

static void editorDone(bool ok) {
    if (ok && editTarget == ET_PASS) {
        strlcpy(pendingPass, lv_textarea_get_text(editTa), sizeof(pendingPass));
    }
    if (ok && editTarget == ET_URLS) {
        config.streamUrls.clear();
        String all = lv_textarea_get_text(editTa);
        int start = 0;
        while (start < (int)all.length() && config.streamUrls.size() < MAX_STREAM_URLS) {
            int nl = all.indexOf('\n', start);
            if (nl < 0) nl = all.length();
            String line = all.substring(start, nl);
            line.trim();
            if (line.length()) config.streamUrls.push_back(line);
            start = nl + 1;
        }
        configSave();
        playerReloadUrls();
    }
    lv_scr_load(editTarget == ET_PASS ? scrWifi : scrSettings);
    editTarget = ET_NONE;
}

static void buildEditor() {
    scrEdit = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scrEdit, lv_color_hex(0x0b0f14), 0);

    lblEditTitle = lv_label_create(scrEdit);
    lv_obj_set_style_text_font(lblEditTitle, &lv_font_montserrat_14, 0);
    lv_obj_align(lblEditTitle, LV_ALIGN_TOP_LEFT, PAD, 10);

    lv_obj_t *btnOk = lv_btn_create(scrEdit);
    styleBtn(btnOk);
    lv_obj_set_style_bg_color(btnOk, lv_color_hex(0x14532d), 0); // subdued green: confirm
    lv_obj_set_size(btnOk, 64, 30);
    lv_obj_align(btnOk, LV_ALIGN_TOP_RIGHT, -PAD, 4);
    lv_obj_add_event_cb(btnOk, [](lv_event_t *) { editorDone(true); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *okl = lv_label_create(btnOk);
    lv_label_set_text(okl, LV_SYMBOL_OK);
    lv_obj_center(okl);

    lv_obj_t *btnCancel = lv_btn_create(scrEdit);
    styleBtn(btnCancel);
    lv_obj_set_size(btnCancel, 64, 30);
    lv_obj_align(btnCancel, LV_ALIGN_TOP_RIGHT, -PAD - 72, 4);
    lv_obj_add_event_cb(btnCancel, [](lv_event_t *) { editorDone(false); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *cl = lv_label_create(btnCancel);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE);
    lv_obj_center(cl);

    editTa = lv_textarea_create(scrEdit);
    lv_obj_set_size(editTa, W - PAD * 2, big ? 96 : 56);
    lv_obj_align(editTa, LV_ALIGN_TOP_LEFT, PAD, 40);
    lv_obj_set_style_bg_color(editTa, lv_color_hex(0x1a2029), 0);

    lv_obj_t *kb = lv_keyboard_create(scrEdit);
    lv_keyboard_set_textarea(kb, editTa);
    lv_obj_set_size(kb, W, H / 2);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
}

// ---- WiFi setup screen ----

static void wifiStartScan() {
    scanning = true;
    lv_dropdown_set_options(ddSsid, "Scanning...");
    WiFi.scanNetworks(true); // async; polled in uiUpdate
}

static void buildWifi() {
    scrWifi = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scrWifi, lv_color_hex(0x0b0f14), 0);

    lv_obj_t *title = lv_label_create(scrWifi);
    lv_obj_set_style_text_font(title, fontMid, 0);
    lv_label_set_text(title, "WiFi setup");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, PAD, 10);

    lv_obj_t *btnBack = lv_btn_create(scrWifi);
    styleBtn(btnBack);
    lv_obj_set_size(btnBack, big ? 84 : 74, big ? 40 : 32);
    lv_obj_align(btnBack, LV_ALIGN_TOP_RIGHT, -PAD, 6);
    lv_obj_add_event_cb(btnBack, [](lv_event_t *) { lv_scr_load(scrSettings); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl = lv_label_create(btnBack);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(bl);

    ddSsid = lv_dropdown_create(scrWifi);
    lv_obj_set_size(ddSsid, W - PAD * 2, big ? 44 : 36);
    lv_obj_align(ddSsid, LV_ALIGN_TOP_LEFT, PAD, big ? 56 : 46);
    lv_obj_set_style_bg_color(ddSsid, lv_color_hex(0x1a2029), 0);
    lv_dropdown_set_options(ddSsid, "Scanning...");

    const int rowH = big ? 44 : 36;
    const int btnY = (big ? 56 : 46) + rowH + 10;
    const int halfW = (W - PAD * 3) / 2;
    auto mkWifiBtn = [&](const char *txt, int col, int row, lv_event_cb_t cb) {
        lv_obj_t *btn = lv_btn_create(scrWifi);
        styleBtn(btn);
        lv_obj_set_size(btn, halfW, rowH);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, PAD + col * (halfW + PAD), btnY + row * (rowH + 8));
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, txt);
        lv_obj_center(l);
        return btn;
    };
    mkWifiBtn(LV_SYMBOL_REFRESH " Rescan", 0, 0, [](lv_event_t *) { wifiStartScan(); });
    mkWifiBtn(LV_SYMBOL_EDIT " Password", 1, 0, [](lv_event_t *) {
        openEditor(ET_PASS, "WiFi password", pendingPass, true);
    });
    lv_obj_t *btnConnect = mkWifiBtn(LV_SYMBOL_WIFI " Connect", 0, 1, [](lv_event_t *e) {
        char ssid[64];
        lv_dropdown_get_selected_str(ddSsid, ssid, sizeof(ssid));
        if (!ssid[0] || !strcmp(ssid, "Scanning...") || !strcmp(ssid, "No networks found")) return;
        config.wifiSsid = ssid;
        config.wifiPass = pendingPass;
        configSave();
        lv_obj_t *l = lv_obj_get_child(lv_event_get_target(e), 0);
        lv_label_set_text(l, "Rebooting...");
        lv_refr_now(nullptr);
        delay(400);
        ESP.restart();
    });
    lv_obj_set_style_bg_color(btnConnect, lv_color_hex(0x14532d), 0);
    mkWifiBtn("Hotspot mode", 1, 1, [](lv_event_t *) {
        playerStop();
        netStartPortal();
        lv_scr_load(scrMain);
    });

    lblWifiInfo = lv_label_create(scrWifi);
    lv_obj_set_style_text_font(lblWifiInfo, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblWifiInfo, lv_color_hex(0x9ca3af), 0);
    lv_obj_set_width(lblWifiInfo, W - PAD * 2);
    lv_label_set_long_mode(lblWifiInfo, LV_LABEL_LONG_DOT);
    lv_obj_align(lblWifiInfo, LV_ALIGN_TOP_LEFT, PAD, btnY + 2 * (rowH + 8) + 6);
}

static void buildSettings() {
    const int rowH = big ? 48 : 40;
    const int headerH = big ? 52 : 44;
    const int halfW = (W - PAD * 3) / 2;

    scrSettings = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scrSettings, lv_color_hex(0x0b0f14), 0);

    lv_obj_t *title = lv_label_create(scrSettings);
    lv_obj_set_style_text_font(title, fontMid, 0);
    lv_label_set_text(title, "Settings");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, PAD, 10);

    lv_obj_t *btnBack = lv_btn_create(scrSettings);
    styleBtn(btnBack);
    lv_obj_set_size(btnBack, big ? 84 : 74, headerH - 12);
    lv_obj_align(btnBack, LV_ALIGN_TOP_RIGHT, -PAD, 6);
    lv_obj_add_event_cb(btnBack, [](lv_event_t *) { lv_scr_load(scrMain); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *bl = lv_label_create(btnBack);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Back");
    lv_obj_center(bl);

    // Tabbed body: Audio / Network / System
    lv_obj_t *tv = lv_tabview_create(scrSettings, LV_DIR_TOP, big ? 40 : 32);
    lv_obj_set_size(tv, W, H - headerH);
    lv_obj_set_pos(tv, 0, headerH);
    lv_obj_set_style_bg_color(tv, lv_color_hex(0x0b0f14), 0);
    lv_obj_t *tabBtns = lv_tabview_get_tab_btns(tv);
    lv_obj_set_style_bg_color(tabBtns, lv_color_hex(0x11161d), 0);
    lv_obj_set_style_text_color(tabBtns, lv_color_hex(0x9ca3af), 0);
    lv_obj_set_style_text_color(tabBtns, lv_color_hex(0xffffff), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_t *tabAudio = lv_tabview_add_tab(tv, "Audio");
    lv_obj_t *tabNet = lv_tabview_add_tab(tv, "Network");
    lv_obj_t *tabSys = lv_tabview_add_tab(tv, "System");
    for (lv_obj_t *t : {tabAudio, tabNet, tabSys}) {
        lv_obj_set_style_pad_hor(t, PAD, 0);
        lv_obj_set_style_pad_top(t, 6, 0);
    }

    // -- row helpers (content-area coordinates) --
    auto addSwitch = [&](lv_obj_t *parent, int row, const char *txt, bool checked, lv_event_cb_t cb) {
        lv_obj_t *l = lv_label_create(parent);
        lv_label_set_text(l, txt);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, row * rowH + 8);
        lv_obj_t *sw = lv_switch_create(parent);
        lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, 0, row * rowH + 2);
        if (checked) lv_obj_add_state(sw, LV_STATE_CHECKED);
        lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, nullptr);
        return sw;
    };
    auto addSlider = [&](lv_obj_t *parent, int row, const char *txt, int mn, int mx, int val, lv_event_cb_t cb) {
        lv_obj_t *l = lv_label_create(parent);
        lv_label_set_text(l, txt);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, row * rowH + 8);
        lv_obj_t *s = lv_slider_create(parent);
        styleSlider(s);
        lv_slider_set_range(s, mn, mx);
        lv_slider_set_value(s, val, LV_ANIM_OFF);
        lv_obj_set_size(s, W - 130 - PAD * 2, 12);
        lv_obj_align(s, LV_ALIGN_TOP_RIGHT, -6, row * rowH + 10);
        lv_obj_add_event_cb(s, cb, LV_EVENT_ALL, nullptr);
        return s;
    };
    auto addBtn = [&](lv_obj_t *parent, int col, int row, const char *txt, lv_event_cb_t cb) {
        lv_obj_t *btn = lv_btn_create(parent);
        styleBtn(btn);
        lv_obj_set_size(btn, halfW, rowH - 8);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, col * (halfW + PAD), row * rowH + 2);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, txt);
        lv_obj_center(l);
        return btn;
    };

    // ---- Audio tab ----
    addSwitch(tabAudio, 0, "Onboard speaker", config.onboardSpeaker, [](lv_event_t *e) {
        bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        config.onboardSpeaker = on;
        playerSetOnboardSpeaker(on);
        configSave();
    });
    swSpeakers = addSwitch(tabAudio, 1, "External speakers", config.speakersEnabled, [](lv_event_t *e) {
        bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        config.speakersEnabled = on;
        playerSetSpeakers(on);
        configSave();
    });
    addSwitch(tabAudio, 2, "Fixed line-out", config.lineOutFixed, [](lv_event_t *e) {
        bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        config.lineOutFixed = on;
        playerSetLineOutFixed(on);
        configSave();
    });
    addSlider(tabAudio, 3, "Line out", 0, 100, config.lineOutLevel, [](lv_event_t *e) {
        int v = lv_slider_get_value(lv_event_get_target(e));
        config.lineOutLevel = v;
        playerSetLineOutLevel(v);
        if (lv_event_get_code(e) == LV_EVENT_RELEASED) configSave();
    });

    // ---- Network tab ----
    addSwitch(tabNet, 0, "Web interface", config.webUiEnabled, [](lv_event_t *e) {
        config.webUiEnabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        configSave(); // webuiLoop applies it on the next tick
    });
    addBtn(tabNet, 0, 1, LV_SYMBOL_WIFI " WiFi setup", [](lv_event_t *) {
        wifiStartScan();
        lv_scr_load(scrWifi);
    });
    addBtn(tabNet, 1, 1, LV_SYMBOL_LIST " Stations", [](lv_event_t *) {
        String all;
        for (auto &u : config.streamUrls) {
            all += u;
            all += "\n";
        }
        openEditor(ET_URLS, "Stream URLs (one per line)", all.c_str(), false);
    });

    // ---- System tab ----
    sliderBright = addSlider(tabSys, 0, "Brightness", 5, 100, config.brightness, [](lv_event_t *e) {
        int v = lv_slider_get_value(lv_event_get_target(e));
        config.brightness = v;
        displaySetBrightness(v);
        if (lv_event_get_code(e) == LV_EVENT_RELEASED) configSave();
    });
    addBtn(tabSys, 0, 1, LV_SYMBOL_DOWNLOAD " Update", [](lv_event_t *) { otaCheckNow(true); });
    addBtn(tabSys, 1, 1, LV_SYMBOL_EYE_CLOSE " Screen off", [](lv_event_t *) { displayScreenOff(); });

    lblOta = lv_label_create(tabSys);
    lv_obj_set_style_text_font(lblOta, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblOta, lv_color_hex(0x9ca3af), 0);
    lv_obj_set_width(lblOta, W - PAD * 2);
    lv_label_set_long_mode(lblOta, LV_LABEL_LONG_DOT);
    lv_obj_align(lblOta, LV_ALIGN_TOP_LEFT, 0, 2 * rowH + 8);
    lv_label_set_text(lblOta, "");

    lblInfo = lv_label_create(tabSys);
    lv_obj_set_style_text_font(lblInfo, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblInfo, lv_color_hex(0x9ca3af), 0);
    lv_obj_set_width(lblInfo, W - PAD * 2);
    lv_label_set_long_mode(lblInfo, LV_LABEL_LONG_DOT);
    lv_obj_align(lblInfo, LV_ALIGN_TOP_LEFT, 0, 2 * rowH + 28);
    lv_label_set_text(lblInfo, "");
}

void uiBegin() {
    W = lv_disp_get_hor_res(nullptr);
    H = lv_disp_get_ver_res(nullptr);
    big = H >= 320;
    fontBig = big ? &lv_font_montserrat_28 : &lv_font_montserrat_20;
    fontMid = big ? &lv_font_montserrat_20 : &lv_font_montserrat_16;

    buildMain();
    buildEditor();
    buildWifi();
    buildSettings();
    lv_scr_load(scrMain);
    displaySetBrightness(config.brightness);
}

// VU with meter ballistics: instant attack, ~0.5s full-scale decay, and a
// peak-hold segment that lingers ~1s before sliding down.
struct VuChannel {
    float disp = 0;
    float peak = 0;
    uint32_t holdUntil = 0;
};

static void vuStep(VuChannel &ch, uint8_t raw, uint32_t now) {
    const float DECAY = 8.0f;      // bar fall per 30ms tick
    const float PEAK_DECAY = 3.0f; // peak-dot fall after the hold expires
    ch.disp = (raw >= ch.disp) ? raw : fmaxf((float)raw, ch.disp - DECAY);
    if (ch.disp >= ch.peak) {
        ch.peak = ch.disp;
        ch.holdUntil = now + 1000;
    } else if (now > ch.holdUntil) {
        ch.peak = fmaxf(ch.disp, ch.peak - PEAK_DECAY);
    }
}

void uiUpdateVu() {
    uint8_t l, r;
    playerGetVu(l, r);
    static VuChannel chL, chR;
    uint32_t now = millis();
    vuStep(chL, l, now);
    vuStep(chR, r, now);
    setVuLevel(vuSegL, (uint8_t)chL.disp, (uint8_t)chL.peak);
    setVuLevel(vuSegR, (uint8_t)chR.disp, (uint8_t)chR.peak);
}

void uiUpdate() {
    PlayerStatus ps;
    playerGetStatus(ps);

    lv_bar_set_value(barBuffer, ps.bufferPct, LV_ANIM_OFF);

    lv_label_set_text(lblStation, ps.station[0] ? ps.station : config.stationName.c_str());
    lv_label_set_text(lblTitle, ps.title[0] ? ps.title : "-");

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

    char statusTxt[224];
    if (netMode() == NetMode::PORTAL) {
        snprintf(statusTxt, sizeof(statusTxt), "Setup: join WiFi '%s', open http://%s", netApName(), netIp().c_str());
    } else if (!ps.wantPlaying) {
        snprintf(statusTxt, sizeof(statusTxt), "Stopped%s", config.webUiEnabled ? "" : "  •  web interface off");
    } else {
        // "2/3 stream.example.com/live · 128 kbps", scheme stripped to save width
        const char *u = ps.currentUrl;
        if (!strncmp(u, "https://", 8)) u += 8;
        else if (!strncmp(u, "http://", 7)) u += 7;
        char rate[24] = "";
        if (ps.bitrate) snprintf(rate, sizeof(rate), "  •  %lu kbps", (unsigned long)(ps.bitrate / 1000));
        snprintf(statusTxt, sizeof(statusTxt), "%d/%d %s%s%s",
                 ps.urlCount ? ps.urlIndex + 1 : 0, ps.urlCount, u, rate,
                 ps.playing ? "" : "  •  reconnecting...");
    }
    lv_label_set_text(lblStatus, statusTxt);

    if (netMode() == NetMode::ONLINE && config.webUiEnabled) {
        char cfgTxt[64];
        snprintf(cfgTxt, sizeof(cfgTxt), "config: http://%s", netIp().c_str());
        lv_label_set_text(lblCfgUrl, cfgTxt);
    } else {
        lv_label_set_text(lblCfgUrl, "");
    }

    // Settings screen extras
    OtaState os;
    otaGetState(os);
    char otaTxt[128];
    snprintf(otaTxt, sizeof(otaTxt), "Firmware update: %s%s", os.message, os.inProgress ? "..." : "");
    lv_label_set_text(lblOta, otaTxt);

    char infoTxt[128];
    snprintf(infoTxt, sizeof(infoTxt), "fw %s  •  http://%s  •  reconnects %lu",
             FW_VERSION, netIp().c_str(), (unsigned long)ps.reconnects);
    lv_label_set_text(lblInfo, infoTxt);

    // WiFi setup screen: deliver async scan results, refresh the info line
    if (scanning) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            scanning = false;
            String opts;
            for (int i = 0; i < n && i < 20; i++) {
                if (WiFi.SSID(i).isEmpty()) continue;
                if (opts.length()) opts += "\n";
                opts += WiFi.SSID(i);
            }
            lv_dropdown_set_options(ddSsid, opts.length() ? opts.c_str() : "No networks found");
            WiFi.scanDelete();
        } else if (n == WIFI_SCAN_FAILED) {
            scanning = false;
            lv_dropdown_set_options(ddSsid, "No networks found");
        }
    }
    if (lv_scr_act() == scrWifi) {
        char t[96];
        snprintf(t, sizeof(t), "Current: %s  •  Password: %s",
                 config.wifiSsid.length() ? config.wifiSsid.c_str() : "(none)",
                 pendingPass[0] ? "entered" : "(empty)");
        lv_label_set_text(lblWifiInfo, t);
    }
}
