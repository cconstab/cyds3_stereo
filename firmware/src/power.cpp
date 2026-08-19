// Deep-sleep "off" mode. Standby draw is set by what stays on the 3V3 rail
// (FT6336 in monitor mode, WS2812, external DACs, LDO quiescent) — expect a
// few mA, i.e. weeks on a 3000mAh pack, vs ~15h idling awake.
//
// While the ESP sleeps its GPIOs float, so everything that must stay put is
// latched with gpio_hold: amps in shutdown, backlight off, and — critically —
// the FT6336's reset line HIGH: if RST floated, the touch controller could
// drop into reset and never raise the wake interrupt.
#include "power.h"
#include <Arduino.h>
#include "driver/gpio.h"
#include "pins.h"
#include "app_config.h"
#include "player.h"
#include "display_lvgl.h"

bool powerWokeByTouch() {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
}

// Pins latched through deep sleep, and the level each is parked at.
static const gpio_num_t holdPin[] = {
    (gpio_num_t)PIN_AMP_ENABLE,  // SC8002B SHUTDOWN high = onboard amp off
    (gpio_num_t)PIN_EXT_AMP_SD,  // MAX98357A SD_MODE low = external amps off
    (gpio_num_t)PIN_LCD_BL,      // backlight off
    (gpio_num_t)PIN_TOUCH_RST,   // keep the FT6336 out of reset so it can wake us
};
static const uint8_t holdLevel[] = {HIGH, LOW, LOW, HIGH};
static const int N_HOLD = sizeof(holdPin) / sizeof(holdPin[0]);

// Re-drive each held pin to the same level BEFORE releasing its hold, so a
// boot after power-off never glitches an amp on. On a cold boot the holds
// aren't set and this just parks the audio pins safely early — which also
// happens to suppress the power-up pop.
void powerBootInit() {
    for (int i = 0; i < N_HOLD; i++) {
        pinMode(holdPin[i], OUTPUT);
        digitalWrite(holdPin[i], holdLevel[i]);
        gpio_hold_dis(holdPin[i]);
    }
    gpio_deep_sleep_hold_dis();
    if (powerWokeByTouch()) Serial.println("[power] woke from power-off (touch/BOOT)");
}

void powerOff() {
    Serial.println("[power] powering off — touch the screen (or BOOT) to wake");
    configSave();
    playerStop();
    playerSetOnboardSpeaker(false); // ES8311 mute + amp shutdown (I2C — UI core, i.e. here)
    delay(250);                     // let the audio task drain its last buffers
    rgbLedWrite(PIN_RGB_LED, 0, 0, 0);
    displayPanelSleep();            // backlight off + ST7796 sleep-in
    delay(50);

    for (int i = 0; i < N_HOLD; i++) {
        pinMode(holdPin[i], OUTPUT);
        digitalWrite(holdPin[i], holdLevel[i]);
        gpio_hold_en(holdPin[i]);
    }
    gpio_deep_sleep_hold_en();

    // FT6336 pulses INT low on any touch (also from its auto monitor mode).
    // Deliberately NOT waking on the BOOT key: GPIO0 is a strapping pin and a
    // deep-sleep wake is a reset, so a held BOOT would land in the serial
    // bootloader instead of the app. (BOOT+RST still works for flashing.)
    esp_sleep_enable_ext1_wakeup(1ULL << PIN_TOUCH_INT, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
}
