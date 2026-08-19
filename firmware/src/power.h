#pragma once

// Deep-sleep "power off" with wake-on-touch (FT6336 INT on GPIO 17, which is
// RTC-capable on the ESP32-S3). Wake is a full reboot.

void powerBootInit();    // re-drive + release deep-sleep pin holds; call FIRST in setup()
bool powerWokeByTouch(); // this boot is a wake from power-off (skip self-test etc.)
void powerOff();         // save config, quiesce audio + panel, enter deep sleep
