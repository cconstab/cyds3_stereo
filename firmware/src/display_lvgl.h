#pragma once
#include <lvgl.h>

void displayBegin();                  // TFT + touch + LVGL init (call before uiBegin)
void displaySetBrightness(uint8_t pct);
void displayScreenOff();              // backlight off; any touch wakes it
void displayScreenWake();
bool displayScreenIsOff();
void displayLoop();                   // lv_timer_handler pump
