#include "net.h"
#include "app_config.h"
#include <WiFi.h>
#include <DNSServer.h>

static NetMode mode = NetMode::OFFLINE;
static DNSServer dns;
static char apName[32];
static uint32_t lastReconnectMs = 0;

void netBegin() {
    snprintf(apName, sizeof(apName), "CYD-Radio-%04X", (uint16_t)(ESP.getEfuseMac() & 0xFFFF));
    WiFi.setHostname(config.hostname.c_str());

    if (config.wifiSsid.isEmpty()) {
        netStartPortal();
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPass.c_str());
    mode = NetMode::CONNECTING;
    Serial.printf("[net] connecting to %s\n", config.wifiSsid.c_str());
}

void netStartPortal() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName);
    dns.start(53, "*", WiFi.softAPIP()); // captive portal: answer every DNS query with our IP
    mode = NetMode::PORTAL;
    Serial.printf("[net] provisioning AP '%s' at %s\n", apName, WiFi.softAPIP().toString().c_str());
}

void netLoop() {
    if (mode == NetMode::PORTAL) {
        dns.processNextRequest();
        return;
    }
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
        if (mode != NetMode::ONLINE) {
            mode = NetMode::ONLINE;
            Serial.printf("[net] online, IP %s\n", WiFi.localIP().toString().c_str());
        }
        return;
    }
    if (mode == NetMode::ONLINE) {
        mode = NetMode::OFFLINE;
        Serial.println("[net] connection lost");
    }
    // WiFi.setAutoReconnect covers most cases; kick it if it stays down
    if (millis() - lastReconnectMs > 15000) {
        lastReconnectMs = millis();
        if (mode != NetMode::CONNECTING) {
            WiFi.disconnect();
            WiFi.begin(config.wifiSsid.c_str(), config.wifiPass.c_str());
            mode = NetMode::CONNECTING;
        } else {
            mode = NetMode::OFFLINE; // give the next kick a fresh begin()
        }
    }
}

NetMode netMode() { return mode; }

String netIp() {
    return (mode == NetMode::PORTAL) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

int netRssi() { return (mode == NetMode::ONLINE) ? WiFi.RSSI() : 0; }

const char *netApName() { return apName; }
