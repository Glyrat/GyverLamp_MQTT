#include "wifi_mgr.h"
#include "config.h"
#include "storage.h"
#include "matrix.h"

#include <ESP8266WebServer.h>
#include <FastLED.h>

// ============================================================
//  СОСТОЯНИЕ
// ============================================================
bool wifiMgr_inAP       = false;
bool wifiMgr_apHasClient = false;
bool wifiMgr_connected  = false;

// ============================================================
//  AP-СЕРВЕР
// ============================================================
static ESP8266WebServer* s_apServer = nullptr;
static uint32_t s_lastRetryMs = 0;
static const uint32_t RETRY_INTERVAL_MS = 30000UL;
static const uint32_t STA_CONNECT_TIMEOUT_MS = 30000UL;
static const uint32_t STA_RETRY_TIMEOUT_MS   = 10000UL;   // в AP-режиме короче

// ============================================================
//  ВНУТРЕННИЕ ПРОТОТИПЫ
// ============================================================
static void _apServerOnRoot();
static void _apServerOnSave();
static void _apServerOnNotFound();
static void _apServerStart();
static void _apServerStop();
static bool _connectTo(const char* ssid, const char* pass, uint32_t timeoutMs);

// ============================================================
//  АНИМАЦИЯ СТАРТА
// ============================================================
void wifiMgr_showStartupAnim() {
    FastLED.setBrightness(255);
    const uint16_t steps = 60;
    for (uint16_t frame = 0; frame < steps; frame++) {
        uint8_t phase = frame * 255 / steps;
        for (uint8_t x = 0; x < WIDTH; x++) {
            uint8_t hue = phase + x * (255 / WIDTH);
            for (uint8_t y = 0; y < HEIGHT; y++) {
                matrix_drawPixel(x, y, CHSV(hue, 255, 255));
            }
        }
        FastLED.show();
        delay(15);
    }
    for (int bri = 255; bri > 0; bri -= 5) {
        FastLED.setBrightness(bri);
        FastLED.show();
        delay(15);
    }
    FastLED.setBrightness(BRIGHTNESS);
    matrix_clear();
    FastLED.show();
}

// ============================================================
//  AP-ИНДИКАЦИЯ (один кадр)
// ============================================================
void wifiMgr_drawAPFrame() {
    uint8_t bri = beatsin8(30, 20, 255);
    matrix_fillAll(CHSV(160, 255, bri));
}

void wifiMgr_drawClientFrame() {
    uint8_t bri = beatsin8(120, 100, 255);
    matrix_fillAll(CHSV(96, 255, bri));
}

// ============================================================
//  ПОДКЛЮЧЕНИЕ
// ============================================================
static bool _connectTo(const char* ssid, const char* pass, uint32_t timeoutMs) {
    if (!ssid || strlen(ssid) == 0) return false;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(100);
        yield();
    }

    bool ok = (WiFi.status() == WL_CONNECTED);

    #if DEBUG_SERIAL
    if (ok) {
        Serial.print(F("[wifi] connected: "));
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(F("[wifi] connect timeout"));
    }
    #endif

    return ok;
}

bool wifiMgr_tryConnect(const char* ssid, const char* pass) {
    return _connectTo(ssid, pass, STA_CONNECT_TIMEOUT_MS);
}

bool wifiMgr_saveAndConnect(const char* ssid, const char* pass) {
    if (!_connectTo(ssid, pass, STA_CONNECT_TIMEOUT_MS)) return false;

    WifiConfig cfg;
    strlcpy(cfg.ssid, ssid, sizeof(cfg.ssid));
    strlcpy(cfg.pass, pass, sizeof(cfg.pass));
    storage_saveWifi(cfg);

    wifiMgr_connected = true;
    _apServerStop();    // <-- добавить: закрыть AP и сервер

    BootConfig boot;
    boot.count = 0;
    boot.lastBootMs = millis();
    storage_saveBoot(boot);

    return true;
}

// ============================================================
//  AP-СЕРВЕР (портал настройки)
// ============================================================
static const char AP_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GyverLamp — Wi-Fi</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:-apple-system,Segoe UI,Roboto,sans-serif;
     background:#111;color:#eee;padding:24px;min-height:100vh}
h1{font-size:20px;margin:0 0 16px}
.card{background:#1c1c1c;border-radius:12px;padding:20px;max-width:480px;margin:auto}
label{display:block;font-size:13px;color:#aaa;margin:12px 0 6px}
input,select{width:100%;padding:10px 12px;border-radius:8px;border:1px solid #333;
             background:#0a0a0a;color:#eee;font-size:15px}
button{margin-top:16px;width:100%;padding:12px;border:none;border-radius:8px;
       background:#3b82f6;color:#fff;font-size:15px;font-weight:600;cursor:pointer}
button:hover{background:#2563eb}
.msg{margin-top:12px;padding:10px;border-radius:8px;font-size:14px;display:none}
.msg.ok{background:#064e3b;color:#6ee7b7;display:block}
.msg.err{background:#7f1d1d;color:#fecaca;display:block}
.net{padding:8px;border-bottom:1px solid #333;cursor:pointer;font-size:14px}
.net:hover{background:#222}
</style>
</head>
<body>
<div class="card">
  <h1>Подключение к Wi-Fi</h1>
  <label>SSID</label>
  <input id="ssid" type="text" placeholder="Имя сети">
  <label>Пароль</label>
  <input id="pass" type="password" placeholder="Пароль">
  <button onclick="save()">Сохранить и подключиться</button>
  <div id="msg" class="msg"></div>
  <h2 style="font-size:15px;color:#aaa;margin:24px 0 8px">Доступные сети</h2>
  <div id="nets"></div>
</div>
<script>
fetch('/scan').then(r=>r.json()).then(list=>{
  let html='';
  list.forEach(n=>{
    html += '<div class="net" onclick="pick(\''+n.s+'\')">'+n.s+' ('+n.r+'%)</div>';
  });
  document.getElementById('nets').innerHTML = html || '<i>нет сетей</i>';
});
function pick(s){document.getElementById('ssid').value=s}
function save(){
  const s=document.getElementById('ssid').value;
  const p=document.getElementById('pass').value;
  const m=document.getElementById('msg');
  m.className='msg'; m.textContent='Подключение...';
  fetch('/save?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p))
    .then(r=>r.json()).then(j=>{
      if(j.ok){m.className='msg ok';m.textContent='Подключено! IP: '+j.ip}
      else   {m.className='msg err';m.textContent='Ошибка: '+j.err}
    });
}
</script>
</body>
</html>
)HTML";

static void _apServerOnRoot() {
    s_apServer->send_P(200, "text/html; charset=utf-8", AP_PAGE);
}

static void _apServerOnSave() {
    String ssid = s_apServer->arg("ssid");
    String pass = s_apServer->arg("pass");

    if (ssid.length() == 0) {
        s_apServer->send(200, "application/json", "{\"ok\":false,\"err\":\"empty ssid\"}");
        return;
    }

    #if DEBUG_SERIAL
    Serial.print(F("[portal] trying ")); Serial.println(ssid);
    #endif

    bool ok = wifiMgr_saveAndConnect(ssid.c_str(), pass.c_str());

    String out;
    if (ok) {
        out = "{\"ok\":true,\"ip\":\"" + WiFi.localIP().toString() + "\"}";
        s_apServer->send(200, "application/json", out);
        // _apServerStop уже вызван внутри wifiMgr_saveAndConnect
        s_lastRetryMs = millis();   // защита от повторного retry
    } else {
        out = "{\"ok\":false,\"err\":\"connection failed\"}";
        s_apServer->send(200, "application/json", out);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASS);
    }
}

static void _apServerOnScan() {
    int n = WiFi.scanNetworks();
    String out = "[";
    for (int i = 0; i < n; i++) {
        if (i) out += ",";
        int pct = 2 * (WiFi.RSSI(i) + 100);
        if (pct < 0) pct = 0; if (pct > 100) pct = 100;
        out += "{\"s\":\"" + WiFi.SSID(i) + "\",\"r\":" + String(pct) + "}";
    }
    out += "]";
    WiFi.scanDelete();
    s_apServer->send(200, "application/json", out);
}

static void _apServerOnNotFound() {
    s_apServer->send(404, "text/plain", "not found");
}

static void _apServerStart() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);

    if (s_apServer == nullptr) {
        s_apServer = new ESP8266WebServer(80);
        s_apServer->on("/",       _apServerOnRoot);
        s_apServer->on("/save",   _apServerOnSave);
        s_apServer->on("/scan",   _apServerOnScan);
        s_apServer->onNotFound(_apServerOnNotFound);
    }
    s_apServer->begin();

    #if DEBUG_SERIAL
    Serial.print(F("[portal] AP: "));
    Serial.print(AP_SSID);
    Serial.print(F(" / http://"));
    Serial.println(WiFi.softAPIP());
    #endif

    s_lastRetryMs = millis();   // чтобы retry не срабатывал сразу
    wifiMgr_inAP = true;
    wifiMgr_apHasClient = false;
}

static void _apServerStop() {
    if (s_apServer) s_apServer->stop();
    WiFi.softAPdisconnect(true);
    wifiMgr_inAP = false;
    wifiMgr_apHasClient = false;

    #if DEBUG_SERIAL
    Serial.println(F("[portal] AP stopped"));
    #endif
}

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ
// ============================================================
void wifiMgr_init() {
    wifiMgr_inAP = false;
    wifiMgr_apHasClient = false;
    wifiMgr_connected = false;

    // 1. Boot count
    BootConfig boot;
    storage_loadBoot(boot);

    uint32_t now = millis();
    if (boot.count > 0 && now < BOOT_WINDOW_MS) {
        boot.count++;
    } else {
        boot.count = 1;
    }
    boot.lastBootMs = now;
    storage_saveBoot(boot);

    #if DEBUG_SERIAL
    Serial.print(F("[wifi] boot count: "));
    Serial.println(boot.count);
    #endif

    // 2. Слишком много перезагрузок → сброс
    bool forcePortal = (boot.count >= BOOT_RESET_THRESHOLD);
    if (forcePortal) {
        Serial.println(F("[wifi] too many reboots → reset Wi-Fi"));
        storage_clearWifi();
        boot.count = 0;
        storage_saveBoot(boot);
    }

    // 3. Попытка подключиться к сохранённому SSID (30 сек)
    WifiConfig cfg;
    bool haveStored = storage_loadWifi(cfg);

    if (!forcePortal && haveStored && strlen(cfg.ssid) > 0) {
        Serial.print(F("[wifi] trying stored SSID: "));
        Serial.println(cfg.ssid);

        if (_connectTo(cfg.ssid, cfg.pass, STA_CONNECT_TIMEOUT_MS)) {
            wifiMgr_connected = true;
            // сбрасываем boot count
            boot.count = 0;
            boot.lastBootMs = millis();
            storage_saveBoot(boot);
            return;
        }
    }

    // 4. Не подключились → поднимаем портал
    Serial.println(F("[wifi] starting config portal"));
    _apServerStart();
}

// ============================================================
//  ТИКЕР
// ============================================================
void wifiMgr_tick() {
    uint32_t now = millis();

    if (wifiMgr_inAP && !wifiMgr_connected) {
        // обработка веб-запросов портала
        if (s_apServer) s_apServer->handleClient();

        // проверка клиента на AP
        bool hasClient = (WiFi.softAPgetStationNum() > 0);
        if (hasClient && !wifiMgr_apHasClient) {
            wifiMgr_apHasClient = true;
            #if DEBUG_SERIAL
            Serial.println(F("[portal] client connected"));
            #endif
        } else if (!hasClient && wifiMgr_apHasClient) {
            wifiMgr_apHasClient = false;
            #if DEBUG_SERIAL
            Serial.println(F("[portal] client disconnected"));
            #endif
        }

        // фоновые повторные попытки подключиться к сохранённому SSID
        if (!wifiMgr_apHasClient && (now - s_lastRetryMs >= RETRY_INTERVAL_MS)) {
            s_lastRetryMs = now;

            WifiConfig cfg;
            if (storage_loadWifi(cfg) && strlen(cfg.ssid) > 0) {
                #if DEBUG_SERIAL
                Serial.print(F("[portal] retry ")); Serial.println(cfg.ssid);
                #endif

                // временно выходим из AP, пытаемся STA
                _apServerStop();
                bool ok = _connectTo(cfg.ssid, cfg.pass, STA_RETRY_TIMEOUT_MS);

                if (ok) {
                    wifiMgr_connected = true;
                    BootConfig b; b.count = 0; b.lastBootMs = millis();
                    storage_saveBoot(b);
                    return;
                }

                // не получилось — обратно в AP
                _apServerStart();
            }
        }
    }
}

// ============================================================
//  ИНФО
// ============================================================
String wifiMgr_getSsid() {
    return wifiMgr_inAP ? String(AP_SSID) : WiFi.SSID();
}

String wifiMgr_getIp() {
    return wifiMgr_inAP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

int wifiMgr_getRssi() {
    return WiFi.RSSI();
}

uint8_t wifiMgr_getRssiPercent() {
    int rssi = WiFi.RSSI();
    if (rssi <= -100) return 0;
    if (rssi >= -50) return 100;
    return (uint8_t)(2 * (rssi + 100));
}

int wifiMgr_getChannel() {
    return WiFi.channel();
}

// ============================================================
//  УПРАВЛЕНИЕ
// ============================================================
void wifiMgr_resetAndReboot() {
    storage_clearWifi();
    WiFi.disconnect(true);
    delay(300);
    ESP.restart();
}
