// Device web UI: status/config page + JSON API on port 80.
// In portal (AP) mode the same server acts as the captive-portal target.
#include "webui.h"
#include "app_config.h"
#include "player.h"
#include "net.h"
#include "ota.h"
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>

static WebServer server(80);

static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD-S3 Stereo</title><style>
body{font-family:system-ui,sans-serif;background:#111;color:#eee;max-width:640px;margin:0 auto;padding:16px}
h1{font-size:1.3em}h2{font-size:1.05em;margin-top:1.4em;border-bottom:1px solid #333;padding-bottom:4px}
input,textarea,button,select{width:100%;box-sizing:border-box;background:#222;color:#eee;border:1px solid #444;border-radius:6px;padding:8px;margin:4px 0;font-size:1em}
button{background:#2563eb;border:0;cursor:pointer;font-weight:600}button.alt{background:#374151}
.row{display:flex;gap:8px}.row>*{flex:1}
#np{background:#1a1a1a;border-radius:8px;padding:12px;margin:8px 0}
.vu{height:10px;background:#222;border-radius:5px;overflow:hidden;margin:4px 0}
.vu>div{height:100%;width:0;background:linear-gradient(90deg,#22c55e,#eab308,#ef4444);transition:width .1s}
small{color:#888}label{display:block;margin-top:8px}
.ok{color:#22c55e}.bad{color:#ef4444}
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
</div>
<label>Volume <span id=volv></span><input type=range min=0 max=21 id=vol onchange="act('volume&value='+this.value)"></label>

<h2>Station</h2>
<label>Name<input id=stationName></label>
<label>Stream URLs (one per line, priority order)<textarea id=streamUrls rows=5 placeholder="http://stream1.example.com/live.mp3"></textarea></label>

<h2>WiFi</h2>
<label>SSID<input id=wifiSsid></label>
<label>Password<input id=wifiPass type=password placeholder="(unchanged)"></label>

<h2>Options</h2>
<label><input type=checkbox id=onboardSpeaker style="width:auto"> Onboard speaker</label>
<label><input type=checkbox id=speakersEnabled style="width:auto"> External speakers (line-out always on)</label>
<label><input type=checkbox id=autoPlay style="width:auto"> Auto-play on boot</label>
<label>Brightness %<input type=number id=brightness min=5 max=100></label>

<h2>Firmware</h2>
<div><small>Installed: <b id=fw></b> &middot; <span id=otamsg></span></small></div>
<label>Update server URL<input id=otaBaseUrl placeholder="http://192.168.1.50:8080"></label>
<label><input type=checkbox id=autoUpdate style="width:auto"> Auto-update</label>
<div class=row>
 <button class=alt onclick="act('checkupdate')">Check for update now</button>
 <button class=alt onclick="if(confirm('Reboot?'))act('reboot')">Reboot</button>
</div>

<br><button onclick="save()">Save configuration</button>
<div id=msg></div>
<script>
const $=id=>document.getElementById(id);
function act(a){fetch('/api/action?do='+a).then(()=>setTimeout(poll,300))}
function poll(){fetch('/api/status').then(r=>r.json()).then(s=>{
 $('station').textContent=s.station||s.stationName||'—';
 $('title').textContent=s.title||'—';
 $('urlinfo').textContent=s.urlCount?('URL '+(s.urlIndex+1)+'/'+s.urlCount):'';
 $('vul').style.width=(s.vuLeft/127*100)+'%';$('vur').style.width=(s.vuRight/127*100)+'%';
 $('stat').innerHTML=(s.playing?'<span class=ok>playing</span>':'<span class=bad>stopped</span>')
  +' · '+(s.bitrate?Math.round(s.bitrate/1000)+' kbps':'')+' · buffer '+s.bufferPct+'% · wifi '+s.rssi+' dBm · reconnects '+s.reconnects;
 $('fw').textContent=s.fw;$('otamsg').textContent=s.otaMessage||'';
 if(document.activeElement.id!=='vol'){$('vol').value=s.volume;$('volv').textContent=s.volume}
})}
function load(){fetch('/api/config').then(r=>r.json()).then(c=>{
 for(const k of ['stationName','wifiSsid','otaBaseUrl','brightness'])$(k).value=c[k]??'';
 $('streamUrls').value=(c.streamUrls||[]).join('\n');
 for(const k of ['speakersEnabled','onboardSpeaker','autoPlay','autoUpdate'])$(k).checked=!!c[k];
})}
function save(){
 const body={stationName:$('stationName').value,wifiSsid:$('wifiSsid').value,
  streamUrls:$('streamUrls').value.split('\n').map(s=>s.trim()).filter(Boolean),
  speakersEnabled:$('speakersEnabled').checked,onboardSpeaker:$('onboardSpeaker').checked,autoPlay:$('autoPlay').checked,
  autoUpdate:$('autoUpdate').checked,brightness:+$('brightness').value,
  otaBaseUrl:$('otaBaseUrl').value};
 if($('wifiPass').value)body.wifiPass=$('wifiPass').value;
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
    doc["bitrate"] = ps.bitrate;
    doc["vuLeft"] = ps.vuLeft;
    doc["vuRight"] = ps.vuRight;
    doc["bufferPct"] = ps.bufferPct;
    doc["reconnects"] = ps.reconnects;
    doc["lastError"] = ps.lastError;
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
    bool prevSpeakers = config.speakersEnabled;
    bool prevOnboard = config.onboardSpeaker;

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
    } else if (a == "checkupdate") otaCheckNow(true);
    else if (a == "reboot") {
        server.send(200, "application/json", "{\"ok\":true}");
        delay(300);
        ESP.restart();
        return;
    } else ok = false;
    server.send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void webuiBegin() {
    server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGE); });
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/config", HTTP_GET, handleConfigGet);
    server.on("/api/config", HTTP_POST, handleConfigPost);
    server.on("/api/action", HTTP_GET, handleAction);
    // Captive-portal probes -> redirect to our page
    server.onNotFound([]() {
        if (netMode() == NetMode::PORTAL) {
            server.sendHeader("Location", "http://" + netIp() + "/", true);
            server.send(302, "text/plain", "");
        } else {
            server.send(404, "text/plain", "not found");
        }
    });
    server.begin();
}

void webuiLoop() {
    server.handleClient();
}
