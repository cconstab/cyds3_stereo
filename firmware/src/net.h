#pragma once
#include <Arduino.h>

enum class NetMode { PORTAL, CONNECTING, ONLINE, OFFLINE };

void netBegin();          // STA if credentials exist, else captive-portal AP
void netLoop();           // reconnect handling + DNS server in portal mode
void netStartPortal();    // force provisioning AP
NetMode netMode();
String netIp();
int netRssi();
const char *netApName();
