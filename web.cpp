#include "web.h"
#include "config.h"
#include "storage.h"
#include "effects.h"
#include "matrix.h"
#include "time_mgr.h"
#include "wifi_mgr.h"
#include "mqtt_mgr.h"
#include "ota.h" 

#include <ESP8266WebServer.h>

// Глобал из lamp_10.ino
extern bool ONflag;

// ============================================================
//  СЕРВЕР
// ============================================================
static ESP8266WebServer* s_server = nullptr;

static void _handleWifi();
static void _handleMqtt();
static void _handleWifiReset();
static void _handleMqttSave();
static void _handleMqttGet();

static void _handleAlarm();
static void _handleInfo();
static void _handleAlarmGet();
static void _handleAlarmSave();
static void _handleInfoApi();

// ============================================================
//  HTML/CSS/JS КАРКАС (общий для всех страниц)
// ============================================================
// Используется в каждой странице: отдаёт header, footer, стили.
// Контент страницы вставляется между header и footer.
const char PAGE_HEAD[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="ru"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GyverLamp</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:-apple-system,Segoe UI,Roboto,sans-serif;
     background:#111;color:#eee;padding:16px;min-height:100vh}
h1{font-size:20px;margin:0 0 16px}
h2{font-size:15px;color:#aaa;margin:20px 0 8px}
.card{background:#1c1c1c;border-radius:12px;padding:20px;max-width:520px;margin:0 auto 16px}
label{display:block;font-size:13px;color:#aaa;margin:12px 0 6px}
input[type=text],input[type=password],input[type=number],select{
  width:100%;padding:10px 12px;border-radius:8px;border:1px solid #333;
  background:#0a0a0a;color:#eee;font-size:15px}
input[type=range]{width:100%;margin:6px 0}
input[type=color]{width:100%;height:44px;border:none;border-radius:8px;background:#0a0a0a}
button{margin-top:16px;width:100%;padding:12px;border:none;border-radius:8px;
       background:#3b82f6;color:#fff;font-size:15px;font-weight:600;cursor:pointer}
button:hover{background:#2563eb}
button.secondary{background:#374151}
button.secondary:hover{background:#4b5563}
button.danger{background:#b91c1c}
button.danger:hover{background:#991b1b}
.msg{margin-top:12px;padding:10px;border-radius:8px;font-size:14px;display:none}
.msg.ok{background:#064e3b;color:#6ee7b7;display:block}
.msg.err{background:#7f1d1d;color:#fecaca;display:block}
.row{display:flex;gap:8px;align-items:center}
.row label{flex:0 0 90px;margin:0}
.row input[type=range]{flex:1}
.row .val{flex:0 0 50px;text-align:right;color:#aaa;font-variant-numeric:tabular-nums}
.nav{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:16px;justify-content:center}
.nav a{color:#9ca3af;text-decoration:none;padding:8px 14px;border-radius:8px;
       background:#1c1c1c;font-size:14px}
.nav a:hover{background:#2a2a2a;color:#fff}
.nav a.active{background:#3b82f6;color:#fff}

.day{background:#1c1c1c;border-radius:12px;padding:16px;margin-bottom:12px;
     max-width:520px;margin-left:auto;margin-right:auto}
.day .head{display:flex;justify-content:space-between;align-items:center;
           margin-bottom:8px}
.day .head .name{font-size:16px;font-weight:600}
.day .row{display:flex;gap:8px;align-items:center;margin:10px 0}
.day .row label{flex:0 0 100px;margin:0;font-size:13px;color:#aaa}
.day .row input,.day .row select{flex:1}
.day .time{width:100px}
.day.collapsed .body{display:none}
@media(max-width:480px){
  .day .row label{flex:0 0 84px;font-size:12px}
  .day{padding:12px}
}

</style>
</head><body>
<div class="nav">
<a href="/">Лампа</a>
<a href="/wifi">Wi-Fi</a>
<a href="/mqtt">MQTT</a>
<a href="/alarm">Будильник</a>
<a href="/info">Инфо</a>
<a href="/update">Прошивка</a>
</div>
)HTML";

const char PAGE_FOOT[] PROGMEM = R"HTML(
<div style="text-align:center;color:#555;font-size:12px;padding:16px 0 8px;margin-top:24px">
  GyverLamp by Glyrat
</div>
</body></html>
)HTML";

// ============================================================
//  ОТПРАВКА СТРАНИЦЫ
// ============================================================
static void _sendPage(const String& content) {
    String html;
    html.reserve(4096);
    html += FPSTR(PAGE_HEAD);
    html += content;
    html += FPSTR(PAGE_FOOT);
    s_server->sendHeader("Cache-Control", "no-store");
    s_server->send(200, "text/html; charset=utf-8", html);
}

static void _sendJson(const String& json, int code = 200) {
    s_server->sendHeader("Cache-Control", "no-store");
    s_server->send(code, "application/json", json);
}

// ============================================================
//  ГЛАВНАЯ СТРАНИЦА
// ============================================================
static void _handleRoot() {
    static const char PAGE[] PROGMEM = R"HTML(
<div class="card">
  <h1>Лампа</h1>

  <label>Питание</label>
  <select id="power">
    <option value="1">Вкл</option>
    <option value="0">Выкл</option>
  </select>

  <label>Эффект</label>
  <select id="effect"></select>

  <div class="row">
    <label>Яркость</label>
    <input type="range" id="brightness" min="1" max="255" value="40">
    <span class="val" id="brightnessVal">40</span>
  </div>

  <div class="row">
    <label>Скорость</label>
    <input type="range" id="speed" min="0" max="255" value="30">
    <span class="val" id="speedVal">30</span>
  </div>

  <div class="row">
    <label>Масштаб</label>
    <input type="range" id="scale" min="0" max="100" value="40">
    <span class="val" id="scaleVal">40</span>
  </div>

  <label>Цвет</label>
  <input type="color" id="rgb" value="#ffffff">

  <div id="msg" class="msg"></div>
</div>

<script>
// ============ UI ============
const $ = id => document.getElementById(id);
let state = null;
let suppress = false;

// загрузка текущего состояния
async function loadState() {
  suppress = true;
  const r = await fetch('/api/state');
  state = await r.json();

  // заполняем селект эффектов
  const effSel = $('effect');
  if (effSel.options.length === 0) {
    state.effects.forEach((name, i) => {
      const o = document.createElement('option');
      o.value = i;
      o.textContent = name;
      effSel.appendChild(o);
    });
  }

  $('power').value      = state.on ? '1' : '0';
  effSel.value          = state.mode;
  $('brightness').value = state.brightness;
  $('brightnessVal').textContent = state.brightness;
  $('speed').value      = state.speed;
  $('speedVal').textContent = state.speed;
  $('scale').value      = state.scale;
  $('scaleVal').textContent = state.scale;

  // RGB от текущего эффекта
  const rr = state.r.toString(16).padStart(2, '0');
  const gg = state.g.toString(16).padStart(2, '0');
  const bb = state.b.toString(16).padStart(2, '0');
  $('rgb').value = '#' + rr + gg + bb;

  // спрятать "неактивные" ползунки, если эффект их не использует
  const eff = state.effectsMeta[state.mode];
  $('speed').disabled = !eff.usesSpeed;
  $('scale').disabled = !eff.usesScale;

  suppress = false;
}

// отправка изменений на сервер
async function send(changes) {
  const r = await fetch('/api/set', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(changes)
  });
  return r.json();
}

function debounce(fn, ms) {
  let t;
  return (...args) => { clearTimeout(t); t = setTimeout(() => fn(...args), ms); };
}

// ============ ОБРАБОТЧИКИ ============
$('power').onchange = async () => {
  if (suppress) return;
  await send({on: $('power').value === '1'});
  loadState();
};

$('effect').onchange = async () => {
  if (suppress) return;
  await send({mode: parseInt($('effect').value)});
  loadState();
};

const sendBright = debounce(async v => { await send({brightness: v}); }, 200);
$('brightness').oninput = () => {
  $('brightnessVal').textContent = $('brightness').value;
  if (!suppress) sendBright(parseInt($('brightness').value));
};

const sendSpeed = debounce(async v => { await send({speed: v}); }, 200);
$('speed').oninput = () => {
  $('speedVal').textContent = $('speed').value;
  if (!suppress) sendSpeed(parseInt($('speed').value));
};

const sendScale = debounce(async v => { await send({scale: v}); }, 200);
$('scale').oninput = () => {
  $('scaleVal').textContent = $('scale').value;
  if (!suppress) sendScale(parseInt($('scale').value));
};

const sendRgb = debounce(async v => {
  await send({rgb: v});
  // переключим селект на "Цвет" (индекс ищем по имени)
  const idx = state.effects.indexOf('Цвет');
  if (idx >= 0 && $('effect').value != idx) {
    suppress = true;
    $('effect').value = idx;
    suppress = false;
    // подгрузим метаданные, чтобы правильно включить/выключить ползунки
    loadState();
  }
}, 200);

$('rgb').oninput = () => {
  if (!suppress) sendRgb($('rgb').value.replace('#', ''));
};

// ============ СТАРТ ============
loadState();
// периодически обновляем состояние (раз в 5 сек)
setInterval(loadState, 5000);
</script>
)HTML";

    _sendPage(FPSTR(PAGE));
}

// ============================================================
//  API: СОСТОЯНИЕ
// ============================================================
static void _handleApiState() {
    EffectSettings& s = effects_currentSettings();

    String j;
    j.reserve(2048);
    j += "{";
    j += "\"on\":";           j += (ONflag ? "true" : "false");
    j += ",\"mode\":";        j += String(effects_currentMode());
    j += ",\"brightness\":";  j += String(s.brightness);
    j += ",\"speed\":";       j += String(s.speed);
    j += ",\"scale\":";       j += String(s.scale);
    j += ",\"r\":";           j += String(gR);
    j += ",\"g\":";           j += String(gG);
    j += ",\"b\":";           j += String(gB);
    j += ",\"time\":\"";      j += timeMgr_getTimeString();  j += "\"";
    j += ",\"nextAlarm\":\"";  j += timeMgr_getNextAlarmString();       j += "\"";
    j += ",\"alarmIn\":\"";    j += timeMgr_getTimeToNextAlarmString(); j += "\"";
    j += ",\"wifiSsid\":\"";  j += wifiMgr_getSsid();        j += "\"";
    j += ",\"wifiIp\":\"";    j += wifiMgr_getIp();          j += "\"";
    j += ",\"wifiRssi\":";    j += String(wifiMgr_getRssi());
    j += ",\"uptime\":";      j += String(millis() / 1000);
    j += ",\"mqttConnected\":"; j += (mqttMgr_connected ? "true" : "false");

    // список эффектов
    j += ",\"effects\":[";
    for (uint8_t i = 0; i < EFFECT_COUNT; i++) {
        if (i) j += ",";
        j += "\""; j += EFFECTS[i].name; j += "\"";
    }
    j += "]";

    // метаданные эффектов
    j += ",\"effectsMeta\":[";
    for (uint8_t i = 0; i < EFFECT_COUNT; i++) {
        if (i) j += ",";
        j += "{\"usesSpeed\":";
        j += (EFFECTS[i].usesSpeed ? "true" : "false");
        j += ",\"usesScale\":";
        j += (EFFECTS[i].usesScale ? "true" : "false");
        j += "}";
    }
    j += "]";

    j += "}";

    _sendJson(j);
}

// ============================================================
//  API: УСТАНОВКА
// ============================================================
static void _handleApiSet() {
    if (!s_server->hasArg("plain")) {
        _sendJson("{\"ok\":false,\"err\":\"no body\"}", 400);
        return;
    }

    String body = s_server->arg("plain");

    // Простейший парсер JSON вручную — без ArduinoJson в этом месте,
    // чтобы не тратить память. Поля: on, mode, brightness, speed, scale, rgb.

    // ON
    int pOn = body.indexOf("\"on\"");
    if (pOn >= 0) {
        int v = body.indexOf(':', pOn);
        bool on = (body.indexOf("true", v) >= 0);
        if (on != ONflag) {
            ONflag = on;
            mqttMgr_publishPower(on);
             markSettingsChanged(); 
        }
    }

    // MODE
    int pMode = body.indexOf("\"mode\"");
    if (pMode >= 0) {
        int v = body.indexOf(':', pMode) + 1;
        int mode = body.substring(v).toInt();
        if (mode >= 0 && mode < EFFECT_COUNT) {
            effects_setMode(mode);
            mqttMgr_publishEffect(mode);
             markSettingsChanged(); 
        }
    }

    // BRIGHTNESS
    int pBri = body.indexOf("\"brightness\"");
    if (pBri >= 0) {
        int v = body.indexOf(':', pBri) + 1;
        int bri = body.substring(v).toInt();
        if (bri >= 1 && bri <= 255) {
            effects_currentSettings().brightness = bri;
            FastLED.setBrightness(bri);
            mqttMgr_publishBrightness(bri);
             markSettingsChanged(); 
        }
    }

    // SPEED
    int pSp = body.indexOf("\"speed\"");
    if (pSp >= 0) {
        int v = body.indexOf(':', pSp) + 1;
        int sp = body.substring(v).toInt();
        if (sp >= 0 && sp <= 255) {
            effects_currentSettings().speed = sp;
            loadingFlag = true;
            mqttMgr_publishSpeed(sp);
            mqttMgr_publishState();   // обновим speed_n/status
             markSettingsChanged(); 
        }
    }

    // SCALE
    int pSc = body.indexOf("\"scale\"");
    if (pSc >= 0) {
        int v = body.indexOf(':', pSc) + 1;
        int sc = body.substring(v).toInt();
        if (sc >= 0 && sc <= 100) {
            effects_currentSettings().scale = sc;
            loadingFlag = true;
            mqttMgr_publishScale(sc);
            mqttMgr_publishState();
             markSettingsChanged(); 
        }
    }

    // RGB (строка "RRGGBB")
    int pRgb = body.indexOf("\"rgb\"");
    if (pRgb >= 0) {
        int q1 = body.indexOf('"', pRgb + 5);
        int q2 = body.indexOf('"', q1 + 1);
        if (q1 > 0 && q2 > q1) {
            String hex = body.substring(q1 + 1, q2);
            if (hex.length() >= 6) {
                long val = strtol(hex.c_str(), nullptr, 16);
                uint8_t r = (val >> 16) & 0xFF;
                uint8_t g = (val >> 8)  & 0xFF;
                uint8_t b = val & 0xFF;
                effects_setRgbColor(r, g, b);
                // переключимся на эффект «Цвет»
                for (uint8_t i = 0; i < EFFECT_COUNT; i++) {
                    if (String(EFFECTS[i].name) == "Цвет") {
                        effects_setMode(i);
                        break;
                    }
                }
                mqttMgr_publishRgb(r, g, b);
                mqttMgr_publishEffect(effects_currentMode());
                 markSettingsChanged(); 
            }
        }
    }

    _sendJson("{\"ok\":true}");
}

// ============================================================
//  НЕ НАЙДЕНО
// ============================================================
static void _handleNotFound() {
    String out = "<div class=\"card\"><h1>404</h1><p>Страница не найдена: ";
    out += s_server->uri();
    out += "</p><p><a href=\"/\" style=\"color:#3b82f6\">На главную</a></p></div>";
    s_server->sendHeader("Cache-Control", "no-store");
    s_server->send(404, "text/html; charset=utf-8", out);
}

// ============================================================
//  СТРАНИЦА WI-FI
// ============================================================
static void _handleWifi() {
    static const char PAGE[] PROGMEM = R"HTML(
    <div class="card">
  <h1>Wi-Fi</h1>
  <div class="row"><label>Сеть</label><span id="ssid">—</span></div>
  <div class="row"><label>IP</label><span id="ip">—</span></div>
  <div class="row"><label>RSSI</label><span id="rssi">—</span></div>
  <div class="row"><label>Uptime</label><span id="uptime">—</span></div>
  <div class="row"><label>MQTT</label><span id="mqtt">—</span></div>

  <button class="danger" onclick="resetWifi()">Сбросить Wi-Fi и перезагрузить</button>
  <div id="msg" class="msg"></div>
</div>

<script>
async function load() {
  const r = await fetch('/api/state');
  const s = await r.json();
  document.getElementById('ssid').textContent   = s.wifiSsid;
  document.getElementById('ip').textContent     = s.wifiIp;
  document.getElementById('rssi').textContent   = s.wifiRssi + ' dBm';
  document.getElementById('uptime').textContent = s.uptime + ' с';
  document.getElementById('mqtt').textContent   = s.mqttConnected ? 'подключён' : 'отключён';
}
load();
setInterval(load, 3000);

async function resetWifi() {
  if (!confirm('Сбросить Wi-Fi и перезагрузить лампу?')) return;
  await fetch('/api/wifi/reset', {method: 'POST'});
  const m = document.getElementById('msg');
  m.className = 'msg ok';
  m.textContent = 'Сбрасываем... Лампа поднимется в AP через 3 секунды.';
}
</script>
)HTML";

    _sendPage(FPSTR(PAGE));
}

static void _handleWifiReset() {
    _sendJson("{\"ok\":true}");
    delay(300);
    wifiMgr_resetAndReboot();
}

// ============================================================
//  СТРАНИЦА MQTT
// ============================================================
static void _handleMqtt() {
    static const char PAGE[] PROGMEM = R"HTML(
<div class="card">
  <h1>MQTT</h1>

  <label>Хост</label>
  <input type="text" id="host" placeholder="192.168.0.90">

  <label>Порт</label>
  <input type="number" id="port" value="1883">

  <label>Пользователь</label>
  <input type="text" id="user">

  <label>Пароль</label>
  <input type="password" id="pass">

  <label>Включить MQTT</label>
  <select id="enabled">
    <option value="1">Да</option>
    <option value="0">Нет</option>
  </select>

  <button onclick="save()">Сохранить и применить</button>
  <div id="msg" class="msg"></div>
</div>

<script>
async function load() {
  const r = await fetch('/api/mqtt/get');
  const s = await r.json();
  document.getElementById('host').value    = s.host;
  document.getElementById('port').value    = s.port;
  document.getElementById('user').value    = s.user;
  document.getElementById('pass').value    = s.pass;
  document.getElementById('enabled').value = s.enabled ? '1' : '0';
}
load();

async function save() {
  const data = {
    host:    document.getElementById('host').value.trim(),
    port:    parseInt(document.getElementById('port').value) || 1883,
    user:    document.getElementById('user').value,
    pass:    document.getElementById('pass').value,
    enabled: document.getElementById('enabled').value === '1'
  };
  const r = await fetch('/api/mqtt/save', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(data)
  });
  const res = await r.json();
  const m = document.getElementById('msg');
  if (res.ok) {
    m.className = 'msg ok';
    m.textContent = 'Сохранено, переподключаемся...';
  } else {
    m.className = 'msg err';
    m.textContent = 'Ошибка: ' + (res.err || 'unknown');
  }
}
</script>
)HTML";

    _sendPage(FPSTR(PAGE));
}

static void _handleMqttGet() {
    MqttConfig c;
    storage_loadMqtt(c);

    String j = "{";
    j += "\"host\":\"" + String(c.host) + "\",";
    j += "\"port\":" + String(c.port) + ",";
    j += "\"user\":\"" + String(c.user) + "\",";
    j += "\"pass\":\"" + String(c.pass) + "\",";
    j += "\"enabled\":" + String(c.enabled ? "true" : "false");
    j += "}";

    _sendJson(j);
}

static void _handleMqttSave() {
    if (!s_server->hasArg("plain")) {
        _sendJson("{\"ok\":false,\"err\":\"no body\"}", 400);
        return;
    }
    String body = s_server->arg("plain");

    // минимальный парсер
    MqttConfig c;
    memset(&c, 0, sizeof(c));

    auto getStr = [&](const char* key, char* out, size_t maxLen) {
        int p = body.indexOf("\"" + String(key) + "\"");
        if (p < 0) return;
        int q1 = body.indexOf('"', p + strlen(key) + 2);
        int q2 = body.indexOf('"', q1 + 1);
        if (q1 > 0 && q2 > q1) {
            String v = body.substring(q1 + 1, q2);
            strlcpy(out, v.c_str(), maxLen);
        }
    };

    getStr("host", c.host, sizeof(c.host));
    getStr("user", c.user, sizeof(c.user));
    getStr("pass", c.pass, sizeof(c.pass));

    // port
    int pPort = body.indexOf("\"port\"");
    if (pPort >= 0) {
        int v = body.indexOf(':', pPort) + 1;
        c.port = body.substring(v).toInt();
        if (c.port == 0) c.port = 1883;
    }

    // enabled
    int pEn = body.indexOf("\"enabled\"");
    if (pEn >= 0) {
        c.enabled = (body.indexOf("true", pEn) >= 0);
    }

    storage_saveMqtt(c);

    _sendJson("{\"ok\":true}");

    delay(200);
    mqttMgr_reloadConfig();
}

// ============================================================
//  СТРАНИЦА БУДИЛЬНИКА
// ============================================================
static void _handleAlarm() {
    static const char PAGE[] PROGMEM = R"HTML(
<div class="card">
  <h1>Будильник</h1>
  <div class="row">
    <label>Глобально</label>
    <select id="enabled">
      <option value="1">Вкл</option>
      <option value="0">Выкл</option>
    </select>
  </div>
  <div id="msg" class="msg"></div>
</div>

<div id="days"></div>

<div style="max-width:520px;margin:16px auto;">
  <button onclick="saveAll()">Сохранить всё</button>
</div>

<script>
const DAY_NAMES = ['Пн','Вт','Ср','Чт','Пт','Сб','Вс'];
const AFTER_MODES = [
  {v:0, t:'Оставить вкл'},
  {v:1, t:'Выключить'},
  {v:2, t:'Выкл через N мин'}
];
let alarm = null;

async function load() {
  const r = await fetch('/api/alarm/get');
  alarm = await r.json();

  document.getElementById('enabled').value = alarm.enabled ? '1' : '0';

  const container = document.getElementById('days');
  container.innerHTML = '';

  alarm.days.forEach((d, i) => {
    const div = document.createElement('div');
    div.className = 'day';
    div.innerHTML = `
      <div class="head">
        <span class="name">${DAY_NAMES[i]}</span>
        <select id="en_${i}" style="width:80px">
          <option value="1">Вкл</option>
          <option value="0">Выкл</option>
        </select>
      </div>
      <div class="body">
        <div class="row">
          <label>Время</label>
          <input class="time" type="time" id="time_${i}">
        </div>
        <div class="row">
          <label>Рассвет</label>
          <input type="number" min="1" max="60" id="dawn_${i}">
          <span style="color:#888;font-size:12px;flex:0 0 40px">мин</span>
        </div>
        <div class="row">
          <label>Макс. ярк.</label>
          <input type="range" min="1" max="255" id="bri_${i}">
          <span style="color:#aaa;font-size:12px;flex:0 0 34px" id="briV_${i}"></span>
        </div>
        <div class="row">
          <label>После</label>
          <select id="after_${i}">
            ${AFTER_MODES.map(m => `<option value="${m.v}">${m.t}</option>`).join('')}
          </select>
        </div>
        <div class="row" id="delayRow_${i}">
          <label>Задержка</label>
          <input type="number" min="1" max="60" id="delay_${i}">
          <span style="color:#888;font-size:12px;flex:0 0 40px">мин</span>
        </div>
      </div>
    `;
    container.appendChild(div);

    // заполняем значения
    document.getElementById(`en_${i}`).value      = d.enabled ? '1' : '0';
    document.getElementById(`time_${i}`).value    =
      String(d.hour).padStart(2,'0') + ':' + String(d.minute).padStart(2,'0');
    document.getElementById(`dawn_${i}`).value    = d.dawnMinutes;
    document.getElementById(`bri_${i}`).value     = d.maxBrightness;
    document.getElementById(`briV_${i}`).textContent = d.maxBrightness;
    document.getElementById(`after_${i}`).value   = d.afterMode;
    document.getElementById(`delay_${i}`).value   = d.afterDelay;

    // показ/скрытие задержки
    const upd = () => {
      const a = parseInt(document.getElementById(`after_${i}`).value);
      document.getElementById(`delayRow_${i}`).style.display = (a === 2) ? '' : 'none';
    };
    upd();
    document.getElementById(`after_${i}`).onchange = upd;

    // отображение яркости
    document.getElementById(`bri_${i}`).oninput = () => {
      document.getElementById(`briV_${i}`).textContent =
        document.getElementById(`bri_${i}`).value;
    };
  });
}

async function saveAll() {
  const out = {
    enabled: document.getElementById('enabled').value === '1',
    days: []
  };
  for (let i = 0; i < 7; i++) {
    const t = document.getElementById(`time_${i}`).value.split(':');
    out.days.push({
      enabled:       document.getElementById(`en_${i}`).value === '1',
      hour:          parseInt(t[0]),
      minute:        parseInt(t[1]),
      dawnMinutes:   parseInt(document.getElementById(`dawn_${i}`).value),
      maxBrightness: parseInt(document.getElementById(`bri_${i}`).value),
      afterMode:     parseInt(document.getElementById(`after_${i}`).value),
      afterDelay:    parseInt(document.getElementById(`delay_${i}`).value)
    });
  }
  const r = await fetch('/api/alarm/save', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(out)
  });
  const res = await r.json();
  const m = document.getElementById('msg');
  if (res.ok) {
    m.className = 'msg ok';
    m.textContent = 'Сохранено';
    setTimeout(() => { m.className = 'msg'; }, 2000);
  } else {
    m.className = 'msg err';
    m.textContent = 'Ошибка: ' + (res.err || 'unknown');
  }
}

load();
</script>
)HTML";

    _sendPage(FPSTR(PAGE));
}

// ---------- API: получить будильники ----------
static void _handleAlarmGet() {
    String j = "{\"enabled\":";
    j += (timeMgr_alarmEnabled() ? "true" : "false");
    j += ",\"days\":[";

    for (uint8_t i = 0; i < 7; i++) {
        AlarmDay d = timeMgr_getAlarmDay(i);
        if (i) j += ",";
        j += "{";
        j += "\"enabled\":";        j += (d.enabled ? "true" : "false"); j += ",";
        j += "\"hour\":";           j += String(d.hour);                 j += ",";
        j += "\"minute\":";         j += String(d.minute);               j += ",";
        j += "\"dawnMinutes\":";    j += String(d.dawnMinutes);          j += ",";
        j += "\"maxBrightness\":";  j += String(d.maxBrightness);        j += ",";
        j += "\"afterMode\":";      j += String((uint8_t)d.afterMode);   j += ",";
        j += "\"afterDelay\":";     j += String(d.afterDelay);
        j += "}";
    }

    j += "]}";
    _sendJson(j);
}

// ---------- API: сохранить будильники ----------
static void _handleAlarmSave() {
    if (!s_server->hasArg("plain")) {
        _sendJson("{\"ok\":false,\"err\":\"no body\"}", 400);
        return;
    }
    String body = s_server->arg("plain");

    // Простой парсер — вытаскиваем "enabled" и массив days.
    // Парсить полноценный JSON вручную муторно, но структура
    // фиксированная, ключи идут в известном порядке.

    // Верхний enabled
    int pEn = body.indexOf("\"enabled\"");
    if (pEn >= 0) {
        bool en = (body.indexOf("true", pEn) >= 0);
        timeMgr_setAlarmEnabled(en);
    }

    // Идём по всем { ... } внутри days
    int pDays = body.indexOf("\"days\"");
    if (pDays < 0) {
        _sendJson("{\"ok\":false,\"err\":\"no days\"}", 400);
        return;
    }

    int cur = pDays;
    for (uint8_t i = 0; i < 7; i++) {
        int open = body.indexOf('{', cur);
        int close = body.indexOf('}', open);
        if (open < 0 || close < 0) break;

        String obj = body.substring(open + 1, close);

        auto getInt = [&](const char* key, int def) -> int {
            int p = obj.indexOf("\"" + String(key) + "\"");
            if (p < 0) return def;
            int v = obj.indexOf(':', p) + 1;
            return obj.substring(v).toInt();
        };

        auto getBool = [&](const char* key, bool def) -> bool {
            int p = obj.indexOf("\"" + String(key) + "\"");
            if (p < 0) return def;
            return (obj.indexOf("true", p) >= 0);
        };

        AlarmDay d = timeMgr_getAlarmDay(i);
        d.enabled       = getBool("enabled", d.enabled);
        d.hour          = (uint8_t)getInt("hour", d.hour);
        d.minute        = (uint8_t)getInt("minute", d.minute);
        d.dawnMinutes   = (uint8_t)getInt("dawnMinutes", d.dawnMinutes);
        d.maxBrightness = (uint8_t)getInt("maxBrightness", d.maxBrightness);
        d.afterMode     = (AfterMode)getInt("afterMode", (int)d.afterMode);
        d.afterDelay    = (uint8_t)getInt("afterDelay", d.afterDelay);

        // ограничения
        if (d.hour > 23) d.hour = 0;
        if (d.minute > 59) d.minute = 0;
        if (d.dawnMinutes < 1) d.dawnMinutes = 1;
        if (d.dawnMinutes > 60) d.dawnMinutes = 60;
        if (d.maxBrightness < 1) d.maxBrightness = 1;
        if (d.afterDelay < 1) d.afterDelay = 1;
        if (d.afterDelay > 60) d.afterDelay = 60;

        #if DEBUG_SERIAL
        Serial.print(F("[alarm save] day=")); Serial.print(i);
        Serial.print(F(" h="));               Serial.print(d.hour);
        Serial.print(F(" m="));               Serial.print(d.minute);
        Serial.print(F(" en="));              Serial.println(d.enabled);
        #endif

        timeMgr_setAlarmDay(i, d);
        cur = close + 1;
    }

    _sendJson("{\"ok\":true}");
}

// ============================================================
//  СТРАНИЦА ИНФО
// ============================================================
static void _handleInfo() {
    static const char PAGE[] PROGMEM = R"HTML(
<div class="card">
  <h1>Инфо</h1>
  <div class="row"><label>Время</label><span id="time">—</span></div>
  <div class="row"><label>Будильник</label><span id="nextAlarm">—</span></div>
  <div class="row"><label>До него</label><span id="alarmIn">—</span></div>
  <div class="row"><label>Wi-Fi</label><span id="ssid">—</span></div>
  <div class="row"><label>IP</label><span id="ip">—</span></div>
  <div class="row"><label>RSSI</label><span id="rssi">—</span></div>
  <div class="row"><label>MQTT</label><span id="mqtt">—</span></div>
  <div class="row"><label>Uptime</label><span id="uptime">—</span></div>
  <div class="row"><label>VCC</label><span id="vcc">—</span></div>
  <div class="row"><label>Heap</label><span id="heap">—</span></div>
  <div class="row"><label>Chip ID</label><span id="chip">—</span></div>
  <div class="row"><label>Reset</label><span id="reset">—</span></div>
</div>

<script>
async function load() {
  const r = await fetch('/api/state');
  const s = await r.json();
  document.getElementById('time').textContent = s.time;
  document.getElementById('nextAlarm').textContent = s.nextAlarm;
  document.getElementById('alarmIn').textContent   = s.alarmIn;
  document.getElementById('ssid').textContent = s.wifiSsid;
  document.getElementById('ip').textContent   = s.wifiIp;
  document.getElementById('rssi').textContent = s.wifiRssi + ' dBm';
  document.getElementById('mqtt').textContent = s.mqttConnected ? 'подключён' : 'отключён';
  document.getElementById('uptime').textContent = s.uptime + ' с';

  const r2 = await fetch('/api/info');
  const i = await r2.json();
  document.getElementById('vcc').textContent  = i.vcc + ' V';
  document.getElementById('heap').textContent = i.heap + ' B';
  document.getElementById('chip').textContent = i.chip;
  document.getElementById('reset').textContent = i.reset;
}
load();
setInterval(load, 3000);
</script>
)HTML";

    _sendPage(FPSTR(PAGE));
}

// ---------- API: инфо ----------
static void _handleInfoApi() {
    String j = "{";
    j += "\"vcc\":";   j += String((float)ESP.getVcc() / 1000.0f, 2);
    j += ",\"heap\":"; j += String(ESP.getFreeHeap());
    j += ",\"chip\":\"" + String(ESP.getChipId(), HEX) + "\"";
    j += ",\"reset\":\"" + ESP.getResetReason() + "\"";
    j += "}";
    _sendJson(j);
}

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ
// ============================================================
void web_init() {
    if (s_server == nullptr) {
        s_server = new ESP8266WebServer(80);

        s_server->on("/",            HTTP_GET,  _handleRoot);
        s_server->on("/wifi",        HTTP_GET,  _handleWifi);
        s_server->on("/mqtt",        HTTP_GET,  _handleMqtt);
        s_server->on("/api/state",   HTTP_GET,  _handleApiState);
        s_server->on("/api/set",     HTTP_POST, _handleApiSet);
        s_server->on("/api/wifi/reset", HTTP_POST, _handleWifiReset);
        s_server->on("/api/mqtt/save",  HTTP_POST, _handleMqttSave);
        s_server->on("/api/mqtt/get",   HTTP_GET,  _handleMqttGet);
        s_server->on("/alarm",          HTTP_GET,  _handleAlarm);
        s_server->on("/info",           HTTP_GET,  _handleInfo);
        s_server->on("/api/alarm/get",  HTTP_GET,  _handleAlarmGet);
        s_server->on("/api/alarm/save", HTTP_POST, _handleAlarmSave);
        s_server->on("/api/info",     HTTP_GET,  _handleInfoApi);
        s_server->onNotFound(_handleNotFound);
    }
    s_server->begin();

    #if DEBUG_SERIAL
    Serial.print(F("[web] http://"));
    Serial.println(WiFi.localIP());
    #endif
}

void web_tick() {
    if (s_server) s_server->handleClient();
}

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ OTA (регистрирует /update на нашем сервере)
// ============================================================
void web_initOta() {
    if (s_server) ota_init(s_server);
}