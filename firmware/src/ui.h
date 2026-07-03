#pragma once

void uiBegin();   // build screens (after displayBegin)
void uiUpdate();  // refresh from player/net/ota state; call ~10x/s from loop
