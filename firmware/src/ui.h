#pragma once

void uiBegin();    // build screens (after displayBegin)
void uiUpdate();   // labels/bars from player/net/ota state; call ~5-10x/s from loop
void uiUpdateVu(); // VU meters only, with ballistics; call ~30x/s from loop
