#include <Arduino.h>
#include <ETH.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_system.h>

#include "Config.h"
#include "WebSerial.h"
#include "AtmegaProtocol.h"
#include "Mqtt.h"

extern uint32_t compatGetOutputMode();
extern uint8_t compatGetOutputFilter();
extern bool compatGetHaSetup();

extern bool eth_connected;

// MQTT configuration from Arnold compatibility layer

extern const char* P1P2Compat_mqttUser();
extern const char* P1P2Compat_mqttPassword();
extern const char* P1P2Compat_mqttClientName();

extern void P1P2Compat_setMqttServer(const char* value);
extern void P1P2Compat_setMqttPort(uint16_t value);
extern void P1P2Compat_setMqttUser(const char* value);
extern void P1P2Compat_setMqttPassword(const char* value);
extern void P1P2Compat_setMqttClientName(const char* value);
extern void P1P2Compat_setMqttEnabled(bool value);
extern void P1P2Compat_saveSettings();

// Shared ESP32 restart, same effect as HA's "Restart_P1P2MQTT_ESP" button
// (MQTT "D0" command): publishes "offline" on the availability topic
// first, then ESP.restart(). Implemented in P1P2_Compat.cpp so both the
// web UI and the MQTT command path trigger identical behaviour.
extern void P1P2Compat_restartEsp();


WebServer server(80);

static uint32_t bootMillis = 0;
static bool webStarted = false;

// ---- System diagnostics ----
static uint32_t bootCount = 0;
static esp_reset_reason_t resetReason = ESP_RST_UNKNOWN;

static const char* resetReasonText(esp_reset_reason_t reason)
{
    switch (reason)
    {
        case ESP_RST_UNKNOWN:   return "UNKNOWN";
        case ESP_RST_POWERON:   return "POWERON_RESET";
        case ESP_RST_EXT:       return "EXTERNAL_RESET";
        case ESP_RST_SW:        return "SOFTWARE_RESET";
        case ESP_RST_PANIC:     return "PANIC_RESET";
        case ESP_RST_INT_WDT:   return "INT_WDT_RESET";
        case ESP_RST_TASK_WDT:  return "TASK_WDT_RESET";
        case ESP_RST_WDT:       return "OTHER_WDT_RESET";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP_RESET";
        case ESP_RST_BROWNOUT:  return "BROWNOUT_RESET";
        case ESP_RST_SDIO:      return "SDIO_RESET";
        default:                return "OTHER_RESET";
    }
}

static void initSystemDiagnostics()
{
    resetReason = esp_reset_reason();

    Preferences prefs;

    if (prefs.begin("sysdiag", false))
    {
        bootCount = prefs.getUInt("bootCount", 0);
        bootCount++;
        prefs.putUInt("bootCount", bootCount);
        prefs.end();
    }
    else
    {
        // If NVS is temporarily unavailable, keep a useful session value.
        bootCount = 1;
    }
}

// ---- OTA-przez-WWW: stan aktualizacji ----
static bool otaWebInProgress = false;
static bool otaWebError = false;
static String otaWebErrorMsg;

void handleSerialFormatUART0()
{
    if(!server.hasArg("plain"))
    {
        server.send(400,"text/plain","missing");
        return;
    }

    String s = server.arg("plain");

    if(s=="ascii")
        webSerialSetFormatUART0(SERIAL_ASCII);

    else if(s=="hex")
        webSerialSetFormatUART0(SERIAL_HEX);

    else
        webSerialSetFormatUART0(SERIAL_BOTH);

    server.send(200,"text/plain","OK");
}



void handleSerialFormatUART2()
{
    if(!server.hasArg("plain"))
    {
        server.send(400,"text/plain","missing");
        return;
    }

    String s = server.arg("plain");

    if(s=="ascii")
        webSerialSetFormatUART2(SERIAL_ASCII);

    else if(s=="hex")
        webSerialSetFormatUART2(SERIAL_HEX);

    else
        webSerialSetFormatUART2(SERIAL_BOTH);

    server.send(200,"text/plain","OK");
}




String uptimeString()
{
    uint32_t sec = (millis() - bootMillis) / 1000;

    uint32_t days = sec / 86400;
    sec %= 86400;

    uint32_t hours = sec / 3600;
    sec %= 3600;

    uint32_t minutes = sec / 60;
    sec %= 60;

    char buf[32];

    sprintf(buf, "%ud %02u:%02u:%02u",
            days,
            hours,
            minutes,
            sec);

    return String(buf);
}

String jsonEscape(const String &s)
{
    String out;

    out.reserve(s.length() + 8);

    for (size_t i = 0; i < s.length(); i++)
    {
        char c = s[i];

        switch (c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\r': out += "\\r";  break;
            case '\n': out += "\\n";  break;
            case '\t': out += "\\t";  break;

            default:
                if ((uint8_t) c < 0x20)
                {
                    char buf[8];
                    sprintf(buf, "\\u%04x", c);
                    out += buf;
                }
                else
                {
                    out += c;
                }
                break;
        }
    }

    return out;
}

String mqttPage()
{
    String s;

    s.reserve(5000);

    s += "<!DOCTYPE html>";
    s += "<html>";
    s += "<head>";
    s += "<meta charset='utf-8'>";
    s += "<title>P1P2MQTT - MQTT Configuration</title>";

    s += "<style>";
    s += "body{font-family:Arial;margin:30px;background:#f4f4f4;}";
    s += "h2{margin-bottom:20px;}";
    s += ".box{background:#fff;padding:20px;border-radius:6px;";
    s += "max-width:600px;}";
    s += "label{display:block;margin-top:14px;font-weight:bold;}";
    s += "input[type=text],input[type=number],input[type=password]";
    s += "{width:100%;padding:8px;box-sizing:border-box;margin-top:5px;}";
    s += "button{margin-top:20px;padding:9px 18px;}";
    s += "#status{margin-top:15px;padding:10px;border-radius:5px;}";
    s += ".ok{background:#dff0d8;color:#3c763d;}";
    s += ".err{background:#f2dede;color:#a94442;}";
    s += ".muted{color:#777;font-size:13px;}";
    s += "</style>";

    s += "</head>";
    s += "<body>";

    s += "<h2>MQTT Configuration</h2>";

    s += "<div class='box'>";

    s += "<label>";
    s += "<input type='checkbox' id='enabled'>";
    s += " Enable MQTT publishing";
    s += "</label>";

    s += "<label>MQTT Server</label>";
    s += "<input type='text' id='server' placeholder='10.192.160.17'>";

    s += "<label>Port</label>";
    s += "<input type='number' id='port' min='1' max='65535' placeholder='1883'>";

    s += "<label>Username</label>";
    s += "<input type='text' id='user'>";

    s += "<label>Password</label>";
    s += "<input type='password' id='password'>";
    s += "<div class='muted'>Leave empty to keep the current password.</div>";

    s += "<label>Client name</label>";
    s += "<input type='text' id='client'>";

    s += "<button onclick='saveMqtt()'>Save & Reconnect</button>";

    s += "<div id='status'></div>";

    s += "<p><a href='/'>&larr; Back</a></p>";

    s += "</div>";

    s += "<script>";

    s += "function loadMqtt(){";
    s += " fetch('/api/mqtt')";
    s += " .then(function(r){return r.json();})";
    s += " .then(function(j){";

    s += " document.getElementById('enabled').checked=j.enabled;";
    s += " document.getElementById('server').value=j.server;";
    s += " document.getElementById('port').value=j.port;";
    s += " document.getElementById('user').value=j.user;";
    s += " document.getElementById('client').value=j.client;";

    s += " });";
    s += "}";

    s += "function saveMqtt(){";

    s += " const data=new URLSearchParams();";

    s += " data.append('enabled',";
    s += "document.getElementById('enabled').checked?'1':'0');";

    s += " data.append('server',document.getElementById('server').value);";
    s += " data.append('port',document.getElementById('port').value);";
    s += " data.append('user',document.getElementById('user').value);";
    s += " data.append('password',document.getElementById('password').value);";
    s += " data.append('client',document.getElementById('client').value);";

    s += " fetch('/api/mqtt',{";
    s += " method:'POST',";
    s += " headers:{'Content-Type':'application/x-www-form-urlencoded'},";
    s += " body:data.toString()";
    s += " })";

    s += " .then(function(r){";
    s += " return r.text().then(function(t){";
    s += " if(!r.ok) throw new Error(t);";
    s += " return t;";
    s += " });";
    s += " })";

    s += " .then(function(t){";
    s += " const e=document.getElementById('status');";
    s += " e.className='ok';";
    s += " e.textContent='MQTT configuration saved.';";
    s += " document.getElementById('password').value='';";
    s += " })";

    s += " .catch(function(e){";
    s += " const x=document.getElementById('status');";
    s += " x.className='err';";
    s += " x.textContent='Error: '+e.message;";
    s += " });";

    s += "}";

    s += "loadMqtt();";

    s += "</script>";

    s += "</body>";
    s += "</html>";

    return s;
}


String htmlPage()
{
    String s;

    s.reserve(11000);

    s += "<!DOCTYPE html>";
    s += "<html>";
    s += "<head>";
    s += "<meta charset='utf-8'>";
    s += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    s += "<title>P1P2MQTT Bridge</title>";

    s += "<style>";
    s += "*{box-sizing:border-box;}";
    s += "body{font-family:Arial,sans-serif;margin:0;padding:24px;";
    s += "background:#f2f4f7;color:#222;}";
    s += ".wrap{max-width:1100px;margin:0 auto;}";
    s += "h1{margin:0 0 6px;font-size:25px;}";
    s += ".subtitle{color:#777;margin-bottom:22px;font-size:13px;}";
    s += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:16px;}";
    s += ".card{background:#fff;border-radius:10px;padding:18px;";
    s += "box-shadow:0 1px 4px rgba(0,0,0,.12);}";
    s += ".card h2{font-size:17px;margin:0 0 14px;";
    s += "padding-bottom:9px;border-bottom:1px solid #e5e7eb;}";
    s += "table{width:100%;border-collapse:collapse;}";
    s += "td{padding:7px 3px;border-bottom:1px solid #eee;";
    s += "vertical-align:top;font-size:13px;}";
    s += "td:first-child{width:46%;color:#666;}";
    s += "td:last-child{font-family:Consolas,monospace;word-break:break-word;}";
    s += ".status{font-family:Arial,sans-serif!important;font-weight:bold;}";
    s += ".ok{color:#16803c;}";
    s += ".bad{color:#c62828;}";
    s += ".warn{color:#b26a00;}";
    s += ".neutral{color:#666;}";
    s += ".pill{display:inline-block;padding:3px 9px;border-radius:12px;";
    s += "font-size:12px;font-weight:bold;font-family:Arial,sans-serif;}";
    s += ".pill.ok{background:#e4f5ea;color:#16803c;}";
    s += ".pill.bad{background:#fde8e8;color:#c62828;}";
    s += ".pill.warn{background:#fff3d6;color:#9a6200;}";
    s += ".pill.neutral{background:#eee;color:#666;}";
    s += ".last{font-size:11px;max-height:42px;overflow:hidden;}";
    s += ".nav{margin-top:18px;background:#fff;padding:14px 18px;";
    s += "border-radius:10px;box-shadow:0 1px 4px rgba(0,0,0,.12);}";
    s += ".nav a{margin-right:18px;}";
    s += ".restart{margin-top:14px;}";
    s += ".restart input{padding:8px 16px;}";
    s += ".footer{margin-top:12px;color:#888;font-size:11px;text-align:right;}";
    s += "@media(max-width:600px){body{padding:12px;}.grid{grid-template-columns:1fr;}}";
    s += "</style>";

    s += "</head>";
    s += "<body>";

    s += "<div class='wrap'>";

    s += "<h1>P1P2MQTT Bridge</h1>";
    s += "<div class='subtitle'>ESP32 / Arnold compatibility diagnostics";
    s += " &nbsp;|&nbsp; Auto-refresh: <span id='refreshLabel'>ON</span>";
    s += " <label style='display:inline-flex;align-items:center;gap:7px;";
    s += "margin-left:10px;cursor:pointer;font-weight:normal;color:#555;'>";
    s += "<input type='checkbox' id='refreshToggle' checked ";
    s += "onchange='toggleRefresh()'> ON/OFF</label></div>";

    s += "<div class='grid'>";

    // ESP32 card
    s += "<div class='card'>";
    s += "<h2>ESP32</h2>";
    s += "<table>";
    s += "<tr><td>Firmware</td><td id='firmware'>-</td></tr>";
    s += "<tr><td>Author</td><td id='author'>-</td></tr>";
    s += "<tr><td>IP</td><td id='ip'>-</td></tr>";
    s += "<tr><td>Gateway</td><td id='gateway'>-</td></tr>";
    s += "<tr><td>Subnet</td><td id='subnet'>-</td></tr>";
    s += "<tr><td>MAC</td><td id='mac'>-</td></tr>";
    s += "<tr><td>Ethernet</td><td id='ethernet' class='status'>-</td></tr>";
    s += "<tr><td>Uptime</td><td id='uptime'>-</td></tr>";
    s += "</table>";
    s += "</div>";

    // System Health / ESP32 diagnostics
    s += "<div class='card'>";
    s += "<h2>System Health</h2>";
    s += "<table>";
    s += "<tr><td>Ethernet</td><td id='healthEthernet'>-</td></tr>";
    s += "<tr><td>MQTT</td><td id='healthMqtt'>-</td></tr>";
    s += "<tr><td>P1P2</td><td id='healthP1p2'>-</td></tr>";
    s += "<tr><td>MQTT publish</td><td id='healthPublish'>-</td></tr>";
    s += "<tr><td>MQTT command</td><td id='healthCommand'>-</td></tr>";
    s += "<tr><td>Free RAM</td><td id='healthRam'>-</td></tr>";
    s += "<tr><td>Uptime</td><td id='healthUptime'>-</td></tr>";
    s += "</table>";
    s += "</div>";

    // ESP32 diagnostics
    s += "<div class='card'>";
    s += "<h2>ESP32 Diagnostics</h2>";
    s += "<table>";
    s += "<tr><td>Free heap</td><td id='freeHeap'>-</td></tr>";
    s += "<tr><td>Minimum free heap</td><td id='minFreeHeap'>-</td></tr>";
    s += "<tr><td>Boot count</td><td id='bootCount'>-</td></tr>";
    s += "<tr><td>Reset reason</td><td id='resetReason'>-</td></tr>";
    s += "</table>";
    s += "</div>";

    // P1P2 / Arnold card
    s += "<div class='card'>";
    s += "<h2>P1P2 / Arnold Compatibility</h2>";
    s += "<table>";
    s += "<tr><td>Output mode</td><td id='outputMode'>-</td></tr>";
    s += "<tr><td>Mode</td><td id='outputModeText'>-</td></tr>";
    s += "<tr><td>Raw MQTT</td><td id='outputModeRaw'>-</td></tr>";
    s += "<tr><td>Parameter MQTT</td><td id='outputModeParam'>-</td></tr>";
    s += "<tr><td>Pseudo</td><td id='outputModePseudo'>-</td></tr>";
    s += "<tr><td>Unknown bit</td><td id='outputModeUnknown'>-</td></tr>";
    s += "<tr><td>Output filter</td><td id='outputFilter'>-</td></tr>";
    s += "<tr><td>HA Setup</td><td id='haSetup'>-</td></tr>";
    s += "</table>";
    s += "</div>";

    // MQTT card
    s += "<div class='card'>";
    s += "<h2>MQTT</h2>";
    s += "<table>";
    s += "<tr><td>Enabled</td><td id='mqttEnabled'>-</td></tr>";
    s += "<tr><td>Status</td><td id='mqttStatus'>-</td></tr>";
    s += "<tr><td>Server</td><td id='mqttServer'>-</td></tr>";
    s += "<tr><td>Client</td><td id='mqttClient'>-</td></tr>";
    s += "<tr><td>Connect attempts</td><td id='connectAttempts'>-</td></tr>";
    s += "<tr><td>Last connect attempt</td><td id='lastConnectAttempt'>-</td></tr>";
    s += "<tr><td>Publish calls</td><td id='publishCalls'>-</td></tr>";
    s += "<tr><td>Publish success</td><td id='publishSuccess'>-</td></tr>";
    s += "<tr><td>Publish failed</td><td id='publishFailed'>-</td></tr>";
    s += "<tr><td>Publish acknowledged</td><td id='publishAck'>-</td></tr>";
    s += "<tr><td>Publish rejected</td><td id='publishRejected'>-</td></tr>";
    s += "<tr><td>Budget exhausted</td><td id='budget'>-</td></tr>";
    s += "</table>";
    s += "</div>";

    // Last MQTT activity
    s += "<div class='card'>";
    s += "<h2>Last MQTT Activity</h2>";
    s += "<table>";
    s += "<tr><td>Last publish topic</td><td id='lastTopic' class='last'>-</td></tr>";
    s += "<tr><td>Last publish submission</td><td id='lastResult'>-</td></tr>";
    s += "<tr><td>Last publish</td><td id='lastPublishAge'>-</td></tr>";
    s += "<tr><td>Publish calls</td><td id='pubCalls2'>-</td></tr>";
    s += "<tr><td>Success / Failed</td><td id='pubSummary'>-</td></tr>";
    s += "</table>";
    s += "</div>";

    s += "</div>";

    s += "<div class='nav'>";
    s += "<a href='/mqtt'>MQTT configuration</a>";
    s += "<a href='/update'>Firmware update</a>";
    s += "<a href='/serial'>Serial monitor</a>";
    s += "<a href='/api/status'>JSON status</a>";
    s += "<form class='restart' method='POST' action='/api/restart' ";
    s += "onsubmit=\"return confirm('Restart ESP32 now?');\">";
    s += "<input type='submit' value='Restart ESP32'>";
    s += "</form>";
    s += "</div>";

    s += "<div class='footer'>P1P2MQTT Bridge</div>";

    s += "</div>";

    s += "<script>";

    // Small helpers
    s += "function el(id){return document.getElementById(id);}";
    s += "function esc(v){";
    s += " if(v===null||v===undefined)return '-';";
    s += " return String(v);";
    s += "}";

    s += "function boolPill(v){";
    s += " return v ? \"<span class='pill ok'>YES</span>\" : ";
    s += "\"<span class='pill neutral'>NO</span>\";";
    s += "}";

    s += "function connPill(v){";
    s += " var ok=(v==='CONNECTED');";
    s += " return ok ? \"<span class='pill ok'>CONNECTED</span>\" : ";
    s += "\"<span class='pill bad'>\"+esc(v)+\"</span>\";";
    s += "}";

    s += "function healthPill(ok){";
    s += " return ok ? \"<span class='pill ok'>OK</span>\" : ";
    s += "\"<span class='pill bad'>ERROR</span>\";";
    s += "}";

    s += "function formatAge(ms){";
    s += " if(ms===null||ms===undefined||ms<0)return 'NEVER';";
    s += " var sec=Math.floor(ms/1000);";
    s += " var min=Math.floor(sec/60);";
    s += " sec=sec%60;";
    s += " var hr=Math.floor(min/60);";
    s += " min=min%60;";
    s += " var day=Math.floor(hr/24);";
    s += " hr=hr%24;";
    s += " return (day>0?day+'d ':'')+";
    s += " (hr>0?hr+'h ':'')+(min>0?min+'m ':'')+sec+'s ago';";
    s += "}";

    s += "function resultPill(v){";
    s += " return v ? \"<span class='pill ok'>ACCEPTED</span>\" : ";
    s += "\"<span class='pill bad'>FAILED</span>\";";
    s += "}";

    s += "var refreshEnabled=true;";
    s += "var refreshTimer=null;";
    s += "var refreshBusy=false;";

    s += "function updateRefreshLabel(){";
    s += " var label=el('refreshLabel');";
    s += " var toggle=el('refreshToggle');";
    s += " if(label) label.textContent=refreshEnabled?'ON':'OFF';";
    s += " if(toggle) toggle.checked=refreshEnabled;";
    s += "}";

    s += "function toggleRefresh(){";
    s += " refreshEnabled=el('refreshToggle').checked;";
    s += " updateRefreshLabel();";
    s += " if(refreshTimer!==null){";
    s += "   clearTimeout(refreshTimer);";
    s += "   refreshTimer=null;";
    s += " }";
    s += " if(refreshEnabled) loadStatus();";
    s += "}";

    s += "function scheduleRefresh(){";
    s += " if(refreshEnabled){";
    s += "   refreshTimer=setTimeout(function(){";
    s += "     refreshTimer=null;";
    s += "     loadStatus();";
    s += "   },2000);";
    s += " }";
    s += "}";

    s += "function loadStatus(){";
    s += " if(refreshBusy) return;";
    s += " refreshBusy=true;";
    s += " fetch('/api/status',{cache:'no-store'})";
    s += " .then(function(r){";
    s += "   if(!r.ok) throw new Error('HTTP '+r.status);";
    s += "   return r.json();";
    s += " })";
    s += " .then(function(j){";
    s += "   var m=j.mqtt||{};";
    s += "   var sys=j.system||{};";

    // ESP32
    s += "   el('firmware').textContent=esc(j.firmware);";
    s += "   el('author').textContent=esc(j.author);";
    s += "   el('ip').textContent=esc(j.ip);";
    s += "   el('gateway').textContent=esc(j.gateway);";
    s += "   el('subnet').textContent=esc(j.subnet);";
    s += "   el('mac').textContent=esc(j.mac);";
    s += "   el('ethernet').innerHTML=connPill(j.ethernet);";
    s += "   el('uptime').textContent=esc(j.uptime);";

    // System Health
    s += "   el('healthEthernet').innerHTML=healthPill(j.ethernet==='CONNECTED');";
    s += "   el('healthMqtt').innerHTML=healthPill(!!m.connected);";
    s += "   el('healthP1p2').innerHTML=healthPill(j.p1p2&&j.p1p2.outputMode!==undefined);";
    s += "   el('healthPublish').innerHTML=healthPill(!!m.last_publish_result && !m.publish_budget_exhausted);";
    s += "   el('healthCommand').innerHTML=healthPill(!!m.command_path_ok);";
    s += "   el('healthRam').innerHTML=healthPill((sys.free_heap||0)>50000);";
    s += "   el('healthUptime').textContent=esc(j.uptime);";

    // ESP32 Diagnostics
    s += "   el('freeHeap').textContent=esc(sys.free_heap)+' bytes ('+((sys.free_heap||0)/1024).toFixed(0)+' kB)';";
    s += "   el('minFreeHeap').textContent=esc(sys.min_free_heap)+' bytes ('+((sys.min_free_heap||0)/1024).toFixed(0)+' kB)';";
    s += "   el('bootCount').textContent=esc(sys.boot_count);";
    s += "   el('resetReason').textContent=esc(sys.reset_reason);";

    // P1P2
    s += "   var p=j.p1p2||{};";
    s += "   el('outputMode').textContent=esc(p.outputMode)+' / '+esc(p.outputMode_hex);";
    s += "   el('outputModeText').textContent=esc(p.outputMode_text);";
    s += "   el('outputModeRaw').innerHTML=boolPill(p.outputMode_raw_mqtt);";
    s += "   el('outputModeParam').innerHTML=boolPill(p.outputMode_parameter_mqtt);";
    s += "   el('outputModePseudo').innerHTML=boolPill(p.outputMode_pseudo);";
    s += "   el('outputModeUnknown').innerHTML=boolPill(p.outputMode_unknown);";
    s += "   el('outputFilter').textContent=esc(p.outputFilter);";
    s += "   el('haSetup').innerHTML=boolPill(p.haSetup);";

    // MQTT
    s += "   el('mqttEnabled').innerHTML=boolPill(m.enabled);";
    s += "   el('mqttStatus').innerHTML=connPill(m.state_text);";
    s += "   el('mqttServer').textContent=esc(m.server)+':'+esc(m.port);";
    s += "   el('mqttClient').textContent=esc(m.client);";
    s += "   el('connectAttempts').textContent=esc(m.connect_attempts);";
    s += "   el('lastConnectAttempt').textContent=esc(m.last_connect_attempt_ms)+' ms';";
    s += "   el('publishCalls').textContent=esc(m.publish_calls);";
    s += "   el('publishSuccess').textContent=esc(m.publish_success);";
    s += "   el('publishFailed').textContent=esc(m.publish_failed);";
    s += "   el('publishAck').textContent=esc(m.publish_acknowledged);";
    s += "   el('publishRejected').textContent=esc(m.publish_rejected);";
    s += "   el('budget').innerHTML=m.publish_budget_exhausted";
    s += "      ? \"<span class='pill bad'>EXHAUSTED</span>\"";
    s += "      : \"<span class='pill ok'>OK</span>\";";

    // Last activity
    s += "   el('lastTopic').textContent=esc(m.last_publish_topic);";
    s += "   el('lastResult').innerHTML=resultPill(!!m.last_publish_result);";
    s += "   el('lastPublishAge').textContent=formatAge(m.last_publish_age_ms);";
    s += "   el('pubCalls2').textContent=esc(m.publish_calls);";
    s += "   el('pubSummary').textContent=esc(m.publish_success)+' / '+esc(m.publish_failed);";

    s += " })";
    s += " .catch(function(e){";
    s += "   el('ethernet').innerHTML=\"<span class='pill bad'>STATUS ERROR</span>\";";
    s += "   el('mqttStatus').innerHTML=\"<span class='pill bad'>STATUS ERROR</span>\";";
    s += " })";
    s += " .finally(function(){";
    s += "   refreshBusy=false;";
    s += "   scheduleRefresh();";
    s += " });";
    s += "}";

    s += "updateRefreshLabel();";
    s += "loadStatus();";

    s += "</script>";

    s += "</body>";
    s += "</html>";

    return s;
}

String updatePage()
{
    String s;

    s.reserve(1200);

    s += "<!DOCTYPE html>";
    s += "<html>";
    s += "<head>";
    s += "<meta charset='utf-8'>";
    s += "<title>P1P2MQTT - OTA Update</title>";

    s += "<style>";
    s += "body{font-family:Arial;margin:30px;background:#f4f4f4;}";
    s += "h2{margin-bottom:20px;}";
    s += ".box{background:#fff;padding:20px;border-radius:6px;max-width:500px;}";
    s += "input[type=submit]{margin-top:10px;padding:8px 16px;}";
    s += "</style>";

    s += "</head>";
    s += "<body>";

    s += "<h2>P1P2MQTT Firmware Update</h2>";

    s += "<div class='box'>";
    s += "<p>Current firmware: ";
    s += FW_VERSION;
    s += " (";
    s += FW_AUTHOR;
    s += ")";
    s += "</p>";

    s += "<form method='POST' action='/update' enctype='multipart/form-data'>";
    s += "<input type='file' name='firmware' accept='.bin'>";
    s += "<br>";
    s += "<input type='submit' value='Upload & Flash'>";
    s += "</form>";

    s += "<p><a href='/'>&larr; Back</a></p>";

    s += "</div>";

    s += "</body>";
    s += "</html>";

    return s;
}

String serialPage()
{
    String s;

    s.reserve(2600);

    s += "<!DOCTYPE html>";
    s += "<html>";
    s += "<head>";
    s += "<meta charset='utf-8'>";
    s += "<title>P1P2MQTT - Serial Monitor</title>";

    s += "<style>";
    s += "body{font-family:Arial;margin:20px;background:#f4f4f4;}";
    s += "h2{margin-bottom:10px;}";
    s += ".term{background:#111;color:#0f0;font-family:Consolas,monospace;";
    s += "font-size:13px;padding:10px;height:35vh;overflow-y:scroll;";
    s += "white-space:pre-wrap;word-break:break-all;border-radius:6px;}";
    s += "button{padding:6px 14px;margin:8px 8px 8px 0;}";
    s += "#stat{color:#555;font-size:12px;margin-bottom:6px;}";
    s += "</style>";

    s += "</head>";
    s += "<body>";

    s += "<h2>P1P2 Serial Monitor</h2>";

    s += "<div id='stat'>polaczony...</div>";

    s += "<button id='btnPause'>Pause</button>";
    s += "<button id='btnClear'>Clear</button>";
    s += "<a href='/'>&larr; Main page</a>";



    s += "<h3>UART2 (TX only - commands sent to ATmega)</h3>";

    s += "<div style='margin-bottom:8px'>";
    s += "<label><input type='radio' name='fmt0' value='ascii'>ASCII</label>";
    s += "<label><input type='radio' name='fmt0' value='hex'>HEX</label>";
    s += "<label><input type='radio' name='fmt0' value='both' checked>BOTH</label>";
    s += "</div>";

    s += "<div id='term0' class='term'></div>";

    s += "<br>";

    s += "<h3>UART2 (ATmega TX/RX, full duplex)</h3>";

    s += "<div style='margin-bottom:8px'>";
    s += "<label><input type='radio' name='fmt2' value='ascii'>ASCII</label>";
    s += "<label><input type='radio' name='fmt2' value='hex'>HEX</label>";
    s += "<label><input type='radio' name='fmt2' value='both' checked>BOTH</label>";
    s += "</div>";

    s += "<div id='term2' class='term'></div>";


    s += "<br>";

    s += "<h3>System Log</h3>";

    s += "<div id='termLog' class='term'></div>";


    s += "<br>";

    s += "<input id='cmd' ";
    s += "style='width:500px;font-family:Consolas' ";
    s += "placeholder='UART command'>";

    s += "<button onclick='sendCmd()'>Send</button>";

    s += "<script>";

    s += "let since0 = 0;";
    s += "let since2 = 0;";
    s += "let sinceLog = 0;";
    s += "let paused = false;";
    s += "const term0 = document.getElementById('term0');";
    s += "const term2 = document.getElementById('term2');";
    s += "const termLog = document.getElementById('termLog');";
    s += "const stat = document.getElementById('stat');";

    s += "document.getElementById('btnPause').onclick = function(){";
    s += "  paused = !paused;";
    s += "  this.textContent = paused ? 'Wznow' : 'Pauza';";
    s += "};";

    s += "document.getElementById('btnClear').onclick = function(){";
    s += "  fetch('/api/serial/clear', {method:'POST'}).then(function(){";
    s += "    term0.textContent='';";
    s += "    term2.textContent='';";
    s += "    termLog.textContent='';";
    s += "  });";
    s += "};";


    s += "document.querySelectorAll('input[name=fmt0]').forEach(function(r){";

    s += "    r.onchange=function(){";

    s += "        fetch('/api/serial/format0',{";

    s += "            method:'POST',";

    s += "            body:this.value";

    s += "        });";

    s += "    };";

    s += "});";

    s += "document.querySelectorAll('input[name=fmt2]').forEach(function(r){";

    s += "    r.onchange=function(){";

    s += "        fetch('/api/serial/format2',{";

    s += "            method:'POST',";

    s += "            body:this.value";

    s += "        });";

    s += "    };";

    s += "});";


    s += "function poll(){";
    s += "  if (paused) { setTimeout(poll, 250); return; }";
    s += "  fetch('/api/serial/data?since0=' + since0 + '&since2=' + since2 + '&sinceLog=' + sinceLog)";
    s += "    .then(function(r){ return r.json(); })";
    s += "    .then(function(j){";

    s += "since0=j.total0;";
    s += "since2=j.total2;";

    s += "if(j.overflow0)";
    s += "    term0.textContent+='\\n[overflow]\\n';";

    s += "if(j.overflow2)";
    s += "    term2.textContent+='\\n[overflow]\\n';";

    s += "if(j.data0.length)";
    s += "{";
    s += "    const atBottom0 =";
    s += "        term0.scrollHeight - term0.scrollTop - term0.clientHeight < 40;";

    s += "    term0.textContent += j.data0;";

    s += "    if(atBottom0)";
    s += "        term0.scrollTop = term0.scrollHeight;";
    s += "}";

    s += "if(j.data2.length)";
    s += "{";
    s += "    const atBottom2 =";
    s += "        term2.scrollHeight - term2.scrollTop - term2.clientHeight < 40;";

    s += "    term2.textContent += j.data2;";

    s += "    if(atBottom2)";
    s += "        term2.scrollTop = term2.scrollHeight;";
    s += "}";

    s += "sinceLog=j.totalLog;";

    s += "if(j.overflowLog)";
    s += "    termLog.textContent+='\\n[overflow]\\n';";

    s += "if(j.dataLog.length)";
    s += "{";
    s += "    const atBottomLog =";
    s += "        termLog.scrollHeight - termLog.scrollTop - termLog.clientHeight < 40;";

    s += "    termLog.textContent += j.dataLog;";

    s += "    if(atBottomLog)";
    s += "        termLog.scrollTop = termLog.scrollHeight;";
    s += "}";

    s += "stat.textContent='UART0: '+j.total0+' B    UART2: '+j.total2+' B';";

    s += "    })";
    s += "    .catch(function(){ stat.textContent = 'blad polaczenia...'; })";
    s += "    .finally(function(){ setTimeout(poll, 100); });";
    s += "}";

    s += "function sendCmd(){";

    s += " const c=document.getElementById('cmd').value.trim();";

    s += " if(c.length==0)";
    s += "     return;";

    s += " fetch('/api/serial/send',{";

    s += " method:'POST',";

    s += " body:c";

    s += " });";

    s += " document.getElementById('cmd').value='';";

    s += " document.getElementById('cmd').focus();";

    s += "}";

    s += "document.getElementById('cmd').addEventListener('keydown',function(e){";

    s += " if(e.key==='Enter') sendCmd();";

    s += "});";

    s += "poll();";

    s += "</script>";

    s += "</body>";
    s += "</html>";

    return s;
}

void handleRoot()
{
    server.send(200, "text/html", htmlPage());
}

void handleMqttPage()
{
    server.send(200, "text/html", mqttPage());
}


void handleMqttGet()
{
    String json;

    json.reserve(500);

    json += "{";

    json += "\"enabled\":";
    json += Esp32Mqtt::enabled() ? "true" : "false";
    json += ",";

    json += "\"connected\":";
    json += Esp32Mqtt::connected() ? "true" : "false";
    json += ",";

    json += "\"server\":\"";
    json += jsonEscape(String(Esp32Mqtt::server()));
    json += "\",";

    json += "\"port\":";
    json += Esp32Mqtt::port();
    json += ",";

    json += "\"user\":\"";
    json += jsonEscape(String(P1P2Compat_mqttUser()));
    json += "\",";

    json += "\"client\":\"";
    json += jsonEscape(String(P1P2Compat_mqttClientName()));
    json += "\"";

    json += "}";

    server.send(200, "application/json", json);
}

void handleMqttSave()
{
    if (!server.hasArg("server") ||
        !server.hasArg("port") ||
        !server.hasArg("user") ||
        !server.hasArg("client") ||
        !server.hasArg("enabled"))
    {
        server.send(400, "text/plain", "Missing MQTT parameters");
        return;
    }

    String serverAddress = server.arg("server");
    String portString = server.arg("port");
    String user = server.arg("user");
    String password = server.arg("password");
    String client = server.arg("client");
    String enabled = server.arg("enabled");

    serverAddress.trim();
    portString.trim();
    user.trim();
    client.trim();

    uint32_t port = portString.toInt();

    if (serverAddress.length() == 0)
    {
        server.send(400, "text/plain", "MQTT server is empty");
        return;
    }

    if (port < 1 || port > 65535)
    {
        server.send(400, "text/plain", "Invalid MQTT port");
        return;
    }

    if (client.length() == 0)
    {
        server.send(400, "text/plain", "MQTT client name is empty");
        return;
    }

    bool mqttEnable = (enabled == "1");

    P1P2Compat_setMqttServer(serverAddress.c_str());
    P1P2Compat_setMqttPort((uint16_t)port);
    P1P2Compat_setMqttUser(user.c_str());
    P1P2Compat_setMqttClientName(client.c_str());
    P1P2Compat_setMqttEnabled(mqttEnable);

    // Empty password means: keep existing password.
    if (password.length() > 0)
    {
        P1P2Compat_setMqttPassword(password.c_str());
    }

    // Persist the complete MQTT configuration in NVS.
    P1P2Compat_saveSettings();

    // Apply configuration immediately.
    Esp32Mqtt::setEnabled(mqttEnable);

    if (mqttEnable)
    {
        Esp32Mqtt::reconnect();
    }
    else
    {
        Esp32Mqtt::disconnect();
    }

    server.send(200, "text/plain", "OK");
}


void handleStatus()
{
    const uint32_t outputMode = compatGetOutputMode();
    const uint8_t outputFilter = compatGetOutputFilter();
    const bool haSetup = compatGetHaSetup();

char outputModeHex[11];
snprintf(outputModeHex, sizeof(outputModeHex), "0x%04lX",
         (unsigned long)outputMode);

String outputModeText;

    // Output mode is a bit mask, not a single enumerated mode.
    if (outputMode == 0x0000)
    {
        outputModeText = "NONE";
    }
    else
    {
        if (outputMode & 0x0001)
            outputModeText += "Raw MQTT";

        if (outputMode & 0x0002)
        {
            if (outputModeText.length() > 0) outputModeText += ", ";
            outputModeText += "Parameter MQTT";
        }

        if (outputMode & 0x0004)
        {
            if (outputModeText.length() > 0) outputModeText += ", ";
            outputModeText += "Pseudo";
        }

        if (outputMode & 0x0008)
        {
            if (outputModeText.length() > 0) outputModeText += ", ";
            outputModeText += "Unknown parameters";
        }

        // Keep future/unsupported bits visible.
        if (outputMode & ~0x000Fu)
        {
            if (outputModeText.length() > 0) outputModeText += ", ";
            outputModeText += "Other bits";
        }
    }

   String json;

    json.reserve(1000);

    json += "{";

    json += "\"firmware\":\"";
    json += FW_VERSION;
    json += "\",";

    json += "\"author\":\"";
    json += FW_AUTHOR;
    json += "\",";

    json += "\"ip\":\"";
    json += ETH.localIP().toString();
    json += "\",";

    json += "\"gateway\":\"";
    json += ETH.gatewayIP().toString();
    json += "\",";

    json += "\"subnet\":\"";
    json += ETH.subnetMask().toString();
    json += "\",";

    json += "\"mac\":\"";
    json += ETH.macAddress();
    json += "\",";

    json += "\"ethernet\":\"";
    json += eth_connected ? "CONNECTED" : "DOWN";
    json += "\",";

    json += "\"uptime\":\"";
    json += uptimeString();
    json += "\",";

    json += "\"system\":{";

    json += "\"free_heap\":";
    json += ESP.getFreeHeap();
    json += ",";

    json += "\"min_free_heap\":";
    json += ESP.getMinFreeHeap();
    json += ",";

    json += "\"boot_count\":";
    json += bootCount;
    json += ",";

    json += "\"reset_reason\":\"";
    json += resetReasonText(resetReason);
    json += "\"";

    json += "},";

    // P1P2 / Arnold compatibility diagnostics
json += "\"p1p2\":{";

json += "\"outputMode\":";
json += outputMode;
json += ",";

json += "\"outputMode_hex\":\"";
json += outputModeHex;
json += "\",";

json += "\"outputMode_raw_mqtt\":";
json += (outputMode & 0x0001) ? "true" : "false";
json += ",";

json += "\"outputMode_parameter_mqtt\":";
json += (outputMode & 0x0002) ? "true" : "false";
json += ",";

json += "\"outputMode_pseudo\":";
json += (outputMode & 0x0004) ? "true" : "false";
json += ",";

json += "\"outputMode_unknown\":";
json += (outputMode & 0x0008) ? "true" : "false";
json += ",";

json += "\"outputMode_text\":\"";
json += jsonEscape(outputModeText);
json += "\",";

json += "\"outputFilter\":";
json += outputFilter;
json += ",";

json += "\"haSetup\":";
json += haSetup ? "true" : "false";


json += "},";

    // MQTT status
    json += "\"mqtt\":{";

    json += "\"enabled\":";
    json += Esp32Mqtt::enabled() ? "true" : "false";
    json += ",";

    json += "\"connected\":";
    json += Esp32Mqtt::connected() ? "true" : "false";
    json += ",";

    json += "\"server\":\"";
    json += Esp32Mqtt::server();
    json += "\",";

    json += "\"port\":";
    json += Esp32Mqtt::port();
    json += ",";

    json += "\"client\":\"";
    json += Esp32Mqtt::clientName();
    json += "\",";

    json += "\"state\":";
    json += Esp32Mqtt::state();
    json += ",";

    json += "\"state_text\":\"";
    json += Esp32Mqtt::stateText();
    json += "\",";

    json += "\"connect_attempts\":";
    json += Esp32Mqtt::connectAttempts();
    json += ",";

    json += "\"last_connect_attempt_ms\":";
    json += Esp32Mqtt::lastConnectAttempt();
    json += ",";

    json += "\"publish_calls\":";
    json += Esp32Mqtt::publishCalls();
    json += ",";

    json += "\"publish_success\":";
    json += Esp32Mqtt::publishSuccess();
    json += ",";

    json += "\"publish_failed\":";
    json += Esp32Mqtt::publishFailed();
    json += ",";

    json += "\"publish_queued\":";
    json += Esp32Mqtt::publishQueued();
    json += ",";

    json += "\"publish_acknowledged\":";
    json += Esp32Mqtt::publishAcknowledged();
    json += ",";

    json += "\"publish_rejected\":";
    json += Esp32Mqtt::publishRejected();
    json += ",";

    const uint32_t nowMs = millis();
    const uint32_t lastPublishMs = Esp32Mqtt::lastPublishMs();

    json += "\"last_publish_ms\":";
    json += lastPublishMs;
    json += ",";

    json += "\"last_publish_age_ms\":";
    if (lastPublishMs == 0)
        json += "-1";
    else
        json += (uint32_t)(nowMs - lastPublishMs);
    json += ",";

    json += "\"last_publish_topic\":\"";
    json += Esp32Mqtt::lastPublishTopic();
    json += "\",";

    json += "\"last_publish_result\":";
    json += Esp32Mqtt::lastPublishResult() ? "true" : "false";
    json += ",";
    json += "\"publish_budget_exhausted\":";
    json += Esp32Mqtt::publishBudgetExhausted() ? "true" : "false";

    // Current command-path health indicator:
    // MQTT command path is considered available when MQTT is connected.
    // Exact RX command counters/results will be added later in Mqtt.cpp.
    json += ",";
    json += "\"command_path_ok\":";
    json += Esp32Mqtt::connected() ? "true" : "false";

    json += "}";

    json += "}";

    server.send(200, "application/json", json);
}


void handleSerialSend()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, "text/plain", "Missing body");
        return;
    }

    String txt = server.arg("plain");
    txt.trim();

    if (txt.length() == 0)
    {
        server.send(200, "text/plain", "EMPTY");
        return;
    }

    AtmegaProtocol::sendCommand(txt.c_str());

    server.send(200, "text/plain", "OK");
}


void handleRestart()
{
    String s;
    s.reserve(300);

    s += "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    s += "<title>Restarting...</title></head><body style='font-family:Arial;margin:30px;'>";
    s += "<h2>Restarting ESP32...</h2>";
    s += "<p>Device will be back shortly. <a href='/'>Return</a></p>";
    s += "</body></html>";

    server.sendHeader("Connection", "close");
    server.send(200, "text/html", s);

    // Let the HTTP response actually go out before the reboot cuts
    // the connection.
    delay(200);

    P1P2Compat_restartEsp();
}

void handle404()
{
    server.send(404, "text/plain", "404 Not Found");
}

// GET /update - formularz uploadu
void handleUpdatePage()
{
    server.send(200, "text/html", updatePage());
}

// POST /update - odpowiedź po zakończeniu uploadu (sukces/błąd)
void handleUpdateResult()
{
    bool ok = !Update.hasError();

    String s;
    s.reserve(600);

    s += "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    s += "<title>OTA Update</title></head><body style='font-family:Arial;margin:30px;'>";

    if (ok)
    {
        s += "<h2>Update OK</h2>";
        s += "<p>Device is rebooting...</p>";
    }
    else
    {
        s += "<h2>Update FAILED</h2>";
        s += "<p>";
        s += otaWebErrorMsg;
        s += "</p>";
        s += "<p><a href='/update'>Try again</a></p>";
    }

    s += "</body></html>";

    server.sendHeader("Connection", "close");
    server.send(ok ? 200 : 500, "text/html", s);

    otaWebInProgress = false;

    if (ok)
    {
        delay(500);
        ESP.restart();
    }
}

// Handler wywoływany podczas przesyłania kolejnych chunków pliku
void handleUpdateUpload()
{
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        otaWebInProgress = true;
        otaWebError = false;
        otaWebErrorMsg = "";

        Serial.println();
        Serial.println("================================");
        Serial.print("Web OTA: receiving ");
        Serial.println(upload.filename);

        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
        {
            otaWebError = true;
            otaWebErrorMsg = "Update.begin() failed - not enough space?";
            Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (!otaWebError)
        {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            {
                otaWebError = true;
                otaWebErrorMsg = "Write failed";
                Update.printError(Serial);
            }
            else
            {
                static uint32_t lastPrint = 0;

                if (millis() - lastPrint > 500)
                {
                    lastPrint = millis();
                    Serial.printf("Web OTA: %u bytes\n", upload.totalSize);
                }
            }
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (!otaWebError)
        {
            if (Update.end(true))
            {
                Serial.printf("Web OTA: OK, %u bytes\n", upload.totalSize);
            }
            else
            {
                otaWebError = true;
                otaWebErrorMsg = "Update.end() failed";
                Update.printError(Serial);
            }
        }

        Serial.println("================================");
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        otaWebError = true;
        otaWebErrorMsg = "Upload aborted";
        Update.end();

        Serial.println("Web OTA: aborted");
    }
}

// GET /serial - strona z terminalem
void handleSerialPage()
{
    server.send(200, "text/html", serialPage());
}

// GET /api/serial/data?since=N - zwraca nowe bajty od sekwencji N

void handleSerialData()
{
    uint32_t since0 = 0;
    uint32_t since2 = 0;
    uint32_t sinceLog = 0;

    if(server.hasArg("since0"))
        since0 = server.arg("since0").toInt();

    if(server.hasArg("since2"))
        since2 = server.arg("since2").toInt();

    if(server.hasArg("sinceLog"))
        sinceLog = server.arg("sinceLog").toInt();

    bool overflow0 = false;
    bool overflow2 = false;
    bool overflowLog = false;

    String data0 = webSerialGetSinceUART0(since0, overflow0);
    String data2 = webSerialGetSinceUART2(since2, overflow2);
    String dataLog = webSerialGetSinceLog(sinceLog, overflowLog);

    String json;

    json.reserve(data0.length()+data2.length()+dataLog.length()+400);

    json += "{";

    json += "\"total0\":";
    json += since0;

    json += ",\"overflow0\":";
    json += overflow0 ? "true":"false";

    json += ",\"data0\":\"";
    json += jsonEscape(data0);
    json += "\"";

    json += ",\"total2\":";
    json += since2;

    json += ",\"overflow2\":";
    json += overflow2 ? "true":"false";

    json += ",\"data2\":\"";
    json += jsonEscape(data2);
    json += "\"";

    json += ",\"totalLog\":";
    json += sinceLog;

    json += ",\"overflowLog\":";
    json += overflowLog ? "true":"false";

    json += ",\"dataLog\":\"";
    json += jsonEscape(dataLog);
    json += "\"";

    json += "}";

    server.send(200,"application/json",json);
}

// POST /api/serial/clear - czyści bufor terminala
void handleSerialClear()
{
    webSerialClear();

    server.send(200, "text/plain", "OK");
}

void webSetup()
{
    if (webStarted)
        return;

    bootMillis = millis();
    initSystemDiagnostics();

    server.on("/", handleRoot);

    server.on("/mqtt", HTTP_GET, handleMqttPage);

    server.on("/api/mqtt", HTTP_GET, handleMqttGet);

    server.on("/api/mqtt",
              HTTP_POST,
              handleMqttSave);

    server.on("/api/status", handleStatus);
    server.on("/api/restart", HTTP_POST, handleRestart);

    server.on("/update", HTTP_GET, handleUpdatePage);
    server.on("/update", HTTP_POST, handleUpdateResult, handleUpdateUpload);

    server.on("/serial", HTTP_GET, handleSerialPage);
    server.on("/api/serial/data", HTTP_GET, handleSerialData);
    server.on("/api/serial/clear", HTTP_POST, handleSerialClear);
    server.on("/api/serial/send", HTTP_POST, handleSerialSend);

    server.on("/api/serial/format0",
              HTTP_POST,
              handleSerialFormatUART0);

    server.on("/api/serial/format2",
              HTTP_POST,
              handleSerialFormatUART2);

    server.onNotFound(handle404);

        server.begin();

    webStarted = true;

    Serial.println();
    Serial.println("================================");
    Serial.println("HTTP server started");
    Serial.print("URL: http://");
    Serial.println(ETH.localIP());
    Serial.println("================================");
}

void webLoop()
{
    if (webStarted)
        server.handleClient();
}