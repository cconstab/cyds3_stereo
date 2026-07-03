#pragma once
#include <Arduino.h>

struct OtaState {
    bool inProgress = false;
    uint8_t progressPct = 0;
    char message[96] = "idle";
    char availableVersion[24] = "";
};

void otaBegin();                  // boot-health counter / rollback check — call early in setup
void otaMarkHealthy();            // call once the app has proven itself (clears rollback counter)
void otaLoop();                   // periodic update checks (respects config.autoUpdate)
bool otaCheckNow(bool install);   // poll manifest; optionally download+install (reboots on success)
void otaGetState(OtaState &out);
