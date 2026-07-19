// Device web UI: status/config page + JSON API on port 80.
// In portal (AP) mode the same server acts as the captive-portal target.
#include "webui.h"
#include "app_config.h"
#include "player.h"
#include "net.h"
#include "ota.h"
#include "display_lvgl.h"
#include "lineout.h"
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>

static WebServer server(80);

static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD-S3 Stereo</title><style>
body{font-family:system-ui,sans-serif;background:#111;color:#eee;max-width:640px;margin:0 auto;padding:16px}
h1{font-size:1.3em}h2{font-size:1.05em;margin-top:1.6em;border-bottom:1px solid #333;padding-bottom:4px}
input,textarea,button,select{width:100%;box-sizing:border-box;background:#222;color:#eee;border:1px solid #444;border-radius:6px;padding:8px;margin:4px 0;font-size:1em}
button{background:#2563eb;border:0;cursor:pointer;font-weight:600}button.alt{background:#374151}
.row{display:flex;gap:8px}.row>*{flex:1}
#np{background:#1a1a1a;border-radius:8px;padding:12px;margin:8px 0}
.vu{height:10px;background:#222;border-radius:5px;overflow:hidden;margin:4px 0}
.vu>div{height:100%;width:0;background:linear-gradient(90deg,#22c55e,#eab308,#ef4444);transition:width .1s}
small{color:#888}label{display:block;margin-top:8px}
.ok{color:#22c55e}.bad{color:#ef4444}.err{color:#f87171}
</style></head><body>
<h1>CYD-S3 Stereo</h1>

<div id=np>
 <div><b id=station>—</b> <small id=urlinfo></small></div>
 <div id=title style="min-height:1.3em">—</div>
 <div class=vu><div id=vul></div></div>
 <div class=vu><div id=vur></div></div>
 <small id=stat>—</small>
</div>
<div class=row>
 <button onclick="act('play')">Play</button>
 <button class=alt onclick="act('stop')">Stop</button>
 <button class=alt onclick="act('next')">Next URL</button>
 <button class=alt onclick="act('screenoff')">Screen off</button>
 <button class=alt onclick="act('screenon')">Screen on</button>
</div>
<label>Volume <span id=volv></span><input type=range min=0 max=21 id=vol onchange="act('volume&value='+this.value)"></label>

<h2>Station &amp; playback</h2>
<label>Station display name (leave blank to show the stream's own name)<input id=stationName></label>
<label>Stream URLs — one per line, best quality first (the failover order)<textarea id=streamUrls rows=5 placeholder="http://stream.example.com/256k.aac"></textarea></label>
<label><input type=checkbox id=autoPlay style="width:auto"> Auto-play on boot</label>
<label><input type=checkbox id=preferredResume style="width:auto"> Migrate back to a higher-priority stream once it recovers</label>

<h2>Audio outputs</h2>
<label><input type=checkbox id=onboardSpeaker style="width:auto"> Onboard speaker (mono)</label>
<label><input type=checkbox id=speakersEnabled style="width:auto"> External speaker amps (mute leaves line-out playing)</label>
<label><input type=checkbox id=lineOutFixed style="width:auto"> Fixed line-out level — RCA level independent of volume (needs the one-wire DIN mod, see HARDWARE.md)</label>
<label>Line-out level % <input type=number id=lineOutLevel min=0 max=100></label>
<small>Line-out DIN is GPIO 43 (the TXD0 pin). Alternative pin for custom builds: set lineOutPin via the config API.</small>

<h2>Display</h2>
<label>Brightness % <input type=number id=brightness min=5 max=100></label>
<label><input type=checkbox id=bootSelfTest style="width:auto"> Color-cycle self-test at power-on (display diagnostic)</label>

<h2>WiFi</h2>
<label>SSID<input id=wifiSsid></label>
<label>Password<input id=wifiPass type=password placeholder="(unchanged)" autocomplete=off></label>
<small>Saving new WiFi credentials reboots the player.</small>

<h2>Web access</h2>
<label>Web password (optional; user "admin")<input id=webUiPassword type=password placeholder="(none)" autocomplete=off></label>
<small>The web interface on/off switch is on the device (Settings &gt; Network) so it can't be disabled remotely.</small>

<h2>Firmware</h2>
<div><small>Installed: <b id=fw></b> &middot; <span id=otamsg></span></small></div>
<label>Update server URL<input id=otaBaseUrl placeholder="http://192.168.1.50:8080"></label>
<label><input type=checkbox id=autoUpdate style="width:auto"> Install updates automatically (hourly check)</label>
<div class=row>
 <button class=alt onclick="act('checkupdate')">Check for update now</button>
 <button class=alt onclick="if(confirm('Reboot the player?'))act('reboot')">Reboot</button>
</div>

<br><button onclick="save()">Save configuration</button>
<div id=msg></div>
<script>
const $=id=>document.getElementById(id);
function act(a){fetch('/api/action?do='+a).then(()=>setTimeout(poll,300))}
function poll(){fetch('/api/status').then(r=>r.json()).then(s=>{
 $('station').textContent=s.stationName||s.station||'—';
 $('title').textContent=s.title||'—';
 $('urlinfo').textContent=s.urlCount?('URL '+(s.urlIndex+1)+'/'+s.urlCount+(s.currentUrl?' — '+s.currentUrl.replace(/^https?:\/\//,''):'')):'';
 $('vul').style.width=(s.vuLeft/127*100)+'%';$('vur').style.width=(s.vuRight/127*100)+'%';
 const bits=[s.playing?'<span class=ok>playing</span>':'<span class=bad>stopped</span>'];
 if(s.bitrate)bits.push(Math.round(s.bitrate/1000)+' kbps');
 bits.push('buffer '+s.bufferPct+'%','wifi '+s.rssi+' dBm','reconnects '+s.reconnects);
 if(s.lineout&&s.lineout!=='off')bits.push('lineout '+s.lineout);
 if(s.probe)bits.push(s.probe);
 if(!s.playing&&s.lastError)bits.push('<span class=err>'+s.lastError+'</span>');
 $('stat').innerHTML=bits.join(' &middot; ');
 $('fw').textContent=s.fw;$('otamsg').textContent=s.otaMessage||'';
 if(document.activeElement.id!=='vol'){$('vol').value=s.volume;$('volv').textContent=s.volume}
})}
function load(){fetch('/api/config').then(r=>r.json()).then(c=>{
 for(const k of ['stationName','wifiSsid','otaBaseUrl','brightness','lineOutLevel'])$(k).value=c[k]??'';
 $('streamUrls').value=(c.streamUrls||[]).join('\n');
 for(const k of ['speakersEnabled','onboardSpeaker','autoPlay','autoUpdate','bootSelfTest','preferredResume','lineOutFixed'])$(k).checked=!!c[k];
 $('webUiPassword').placeholder=c.webUiPasswordSet?"(set — type new, or 'off' to remove)":"(none — type to set)";
})}
function save(){
 const body={stationName:$('stationName').value,wifiSsid:$('wifiSsid').value,
  streamUrls:$('streamUrls').value.split('\n').map(s=>s.trim()).filter(Boolean),
  speakersEnabled:$('speakersEnabled').checked,onboardSpeaker:$('onboardSpeaker').checked,autoPlay:$('autoPlay').checked,preferredResume:$('preferredResume').checked,
  autoUpdate:$('autoUpdate').checked,bootSelfTest:$('bootSelfTest').checked,lineOutFixed:$('lineOutFixed').checked,lineOutLevel:+$('lineOutLevel').value,brightness:+$('brightness').value,
  otaBaseUrl:$('otaBaseUrl').value};
 if($('wifiPass').value)body.wifiPass=$('wifiPass').value;
 const wp=$('webUiPassword').value;
 if(wp)body.webUiPassword=(wp==='off'?'':wp);
 fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
  .then(r=>r.json()).then(r=>{$('msg').textContent=r.ok?'Saved.':'Error saving';setTimeout(()=>$('msg').textContent='',3000)});
}
load();poll();setInterval(poll,1000);
</script></body></html>)HTML";

static void handleStatus() {
    PlayerStatus ps;
    playerGetStatus(ps);
    OtaState os;
    otaGetState(os);
    JsonDocument doc;
    doc["playing"] = ps.playing;
    doc["station"] = ps.station;
    doc["stationName"] = config.stationName;
    doc["title"] = ps.title;
    doc["urlIndex"] = ps.urlIndex;
    doc["urlCount"] = ps.urlCount;
    doc["currentUrl"] = ps.currentUrl;
    doc["bitrate"] = ps.bitrate;
    doc["vuLeft"] = ps.vuLeft;
    doc["vuRight"] = ps.vuRight;
    doc["bufferPct"] = ps.bufferPct;
    doc["reconnects"] = ps.reconnects;
    doc["lastError"] = ps.lastError;
    doc["probe"] = ps.probeMsg;
    {
        char lo[24];
        if (lineoutActive()) snprintf(lo, sizeof(lo), "on GPIO%d", config.lineOutPin);
        else strlcpy(lo, config.lineOutFixed ? "FAILED to start" : "off", sizeof(lo));
        doc["lineout"] = lo;
    }
    doc["volume"] = config.volume;
    doc["rssi"] = netRssi();
    doc["ip"] = netIp();
    doc["fw"] = FW_VERSION;
    doc["otaMessage"] = os.message;
    doc["uptime"] = millis() / 1000;
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static void handleConfigGet() {
    server.send(200, "application/json", configToJson(false));
}

static void handleConfigPost() {
    String prevSsid = config.wifiSsid, prevPass = config.wifiPass;
    uint8_t prevVol = config.volume;
    uint8_t prevBright = config.brightness;
    bool prevSpeakers = config.speakersEnabled;
    bool prevOnboard = config.onboardSpeaker;
    bool prevLineOut = config.lineOutFixed;
    uint8_t prevLineLevel = config.lineOutLevel;

    if (!configFromJson(server.arg("plain"))) {
        server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    configSave();
    server.send(200, "application/json", "{\"ok\":true}");

    playerReloadUrls();
    if (config.volume != prevVol) playerSetVolume(config.volume);
    if (config.speakersEnabled != prevSpeakers) playerSetSpeakers(config.speakersEnabled);
    if (config.onboardSpeaker != prevOnboard) playerSetOnboardSpeaker(config.onboardSpeaker);
    if (config.brightness != prevBright) displaySetBrightness(config.brightness);
    if (config.lineOutFixed != prevLineOut) playerSetLineOutFixed(config.lineOutFixed);
    if (config.lineOutLevel != prevLineLevel) playerSetLineOutLevel(config.lineOutLevel);
    if (config.wifiSsid != prevSsid || config.wifiPass != prevPass) {
        // Apply new WiFi credentials after the response goes out
        delay(500);
        ESP.restart();
    }
}

static void handleAction() {
    String a = server.arg("do");
    bool ok = true;
    if (a == "play") playerPlay();
    else if (a == "stop") playerStop();
    else if (a == "next") playerNextUrl();
    else if (a == "prev") playerPrevUrl();
    else if (a == "volume") {
        config.volume = constrain(server.arg("value").toInt(), 0, 21);
        playerSetVolume(config.volume);
        configSave();
    } else if (a == "screenoff") displayScreenOff();
    else if (a == "screenon") displayScreenWake();
    else if (a == "checkupdate") otaCheckNow(true);
    else if (a == "reboot") {
        server.send(200, "application/json", "{\"ok\":true}");
        delay(300);
        ESP.restart();
        return;
    } else ok = false;
    server.send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

static bool serverRunning = false;

// Optional basic auth. Skipped in portal mode: reaching the AP implies
// physical proximity, and captive-portal browsers handle auth prompts badly.
static bool checkAuth() {
    if (config.webUiPassword.isEmpty() || netMode() == NetMode::PORTAL) return true;
    if (server.authenticate("admin", config.webUiPassword.c_str())) return true;
    server.requestAuthentication(BASIC_AUTH, "CYD-S3 Stereo");
    return false;
}

void webuiBegin() {
    server.on("/", HTTP_GET, []() {
        if (!checkAuth()) return;
        server.send_P(200, "text/html", PAGE);
    });
    server.on("/api/status", HTTP_GET, []() { if (checkAuth()) handleStatus(); });
    server.on("/api/config", HTTP_GET, []() { if (checkAuth()) handleConfigGet(); });
    server.on("/api/config", HTTP_POST, []() { if (checkAuth()) handleConfigPost(); });
    server.on("/api/action", HTTP_GET, []() { if (checkAuth()) handleAction(); });
    // Captive-portal probes -> redirect to our page
    server.onNotFound([]() {
        if (netMode() == NetMode::PORTAL) {
            server.sendHeader("Location", "http://" + netIp() + "/", true);
            server.send(302, "text/plain", "");
        } else {
            server.send(404, "text/plain", "not found");
        }
    });
    // socket is opened/closed in webuiLoop based on config + net mode
}

void webuiLoop() {
    // Forced on in portal mode — the web page IS the provisioning mechanism.
    bool shouldRun = config.webUiEnabled || netMode() == NetMode::PORTAL;
    if (shouldRun && !serverRunning) {
        server.begin();
        serverRunning = true;
        Serial.println("[web] server started");
    } else if (!shouldRun && serverRunning) {
        server.stop();
        serverRunning = false;
        Serial.println("[web] server stopped");
    }
    if (serverRunning) server.handleClient();
}
