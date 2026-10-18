#include "mqtt_mgr.h"
#include "config.h"
#include "storage.h"
#include "effects.h"
#include "matrix.h"
#include "time_mgr.h"
#include "wifi_mgr.h"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ============================================================
//  СОСТОЯНИЕ
// ============================================================
bool mqttMgr_connected = false;
bool mqttMgr_enabled   = false;

// Глобал из lamp_10.ino — управление питанием
extern bool ONflag;

// ============================================================
//  ВНУТРЕННИЕ
// ============================================================
static WiFiClient        s_wifiClient;
static PubSubClient      s_mqtt(s_wifiClient);
static MqttConfig        s_cfg;
static String            s_baseTopic;
static uint32_t          s_lastReconnect = 0;
static uint32_t          s_reconnectDelay = 5000;
static uint8_t           s_reconnectCount = 0;
static uint32_t          s_lastSensorPub = 0;

// ============================================================
//  ТОПИКИ
// ============================================================
static String _tState()       { return s_baseTopic + "/status"; }
static String _tBright()      { return s_baseTopic + "/brightness/status"; }
static String _tEffect()      { return s_baseTopic + "/effect/status"; }
static String _tSpeed()       { return s_baseTopic + "/effect/speed/status"; }
static String _tScale()       { return s_baseTopic + "/effect/scale/status"; }
static String _tRgb()         { return s_baseTopic + "/rgb/status"; }
static String _tAlarmSw()     { return s_baseTopic + "/alarm/status"; }
static String _tRssi()        { return s_baseTopic + "/sensor/rssi"; }
static String _tUptime()      { return s_baseTopic + "/sensor/uptime"; }
static String _tVcc()         { return s_baseTopic + "/sensor/vcc"; }
static String _tNextAlarm()   { return s_baseTopic + "/sensor/next_alarm"; }
static String _tCurTime()     { return s_baseTopic + "/sensor/time"; }
static String _tStatusBin()   { return s_baseTopic + "/status_bin"; }

static String _tAlarmIn() { return s_baseTopic + "/sensor/alarm_in"; }

static String _tSpeedN() { return s_baseTopic + "/speed_n/status"; }
static String _tScaleN() { return s_baseTopic + "/scale_n/status"; }

// ============================================================
//  ПОДПИСКИ
// ============================================================
static String _cSwitch()      { return s_baseTopic + "/switch"; }
static String _cBright()      { return s_baseTopic + "/brightness/set"; }
static String _cEffect()      { return s_baseTopic + "/effect/set"; }
static String _cSpeed()       { return s_baseTopic + "/effect/speed/set"; }
static String _cScale()       { return s_baseTopic + "/effect/scale/set"; }
static String _cRgb()         { return s_baseTopic + "/rgb/set"; }
static String _cAlarmSw()     { return s_baseTopic + "/alarm/set"; }

static String _cSpeedN() { return s_baseTopic + "/speed_n/set"; }
static String _cScaleN() { return s_baseTopic + "/scale_n/set"; }

static void _publishDiscovery();


// ============================================================
//  КОЛБЭК
// ============================================================
static void _onMqttMessage(char* topic, byte* payload, unsigned int length) {
    String t = String(topic);
    String p;
    for (unsigned int i = 0; i < length; i++) p += (char)payload[i];

    #if DEBUG_SERIAL
    Serial.print(F("[mqtt] ")); Serial.print(t); Serial.print(F(" = ")); Serial.println(p);
    #endif

    if (t == _cSwitch()) {
        ONflag = (p == "ON" || p == "on" || p == "1");
        mqttMgr_publishPower(ONflag);
        markSettingsChanged(); 
    }
    else if (t == _cBright()) {
        uint8_t bri = (uint8_t)p.toInt();
        effects_currentSettings().brightness = bri;
        FastLED.setBrightness(bri);
        mqttMgr_publishBrightness(bri);
        markSettingsChanged(); 
    }
    else if (t == _cEffect()) {
        // p — имя эффекта или индекс
        int8_t idx = -1;
        for (uint8_t i = 0; i < EFFECT_COUNT; i++) {
            if (p == EFFECTS[i].name) { idx = i; break; }
        }
        if (idx < 0) idx = (int8_t)p.toInt();
        if (idx >= 0 && idx < EFFECT_COUNT) {
            effects_setMode(idx);
            mqttMgr_publishEffect(idx);
            markSettingsChanged(); 
        }
    }
    else if (t == _cSpeed()) {
        uint8_t sp = (uint8_t)p.toInt();
        effects_currentSettings().speed = sp;
        loadingFlag = true;
        mqttMgr_publishSpeed(sp);
        markSettingsChanged(); 
    }
    else if (t == _cScale()) {
        uint8_t sc = (uint8_t)p.toInt();
        if (sc > 100) sc = 100;
        effects_currentSettings().scale = sc;
        loadingFlag = true;
        mqttMgr_publishScale(sc);
        markSettingsChanged(); 
    }
    else if (t == _cRgb()) {
        // ожидаем "R,G,B"
        int c1 = p.indexOf(',');
        int c2 = p.indexOf(',', c1 + 1);
        if (c1 > 0 && c2 > c1) {
            uint8_t r = (uint8_t)p.substring(0, c1).toInt();
            uint8_t g = (uint8_t)p.substring(c1 + 1, c2).toInt();
            uint8_t b = (uint8_t)p.substring(c2 + 1).toInt();
            effects_setRgbColor(r, g, b);
            // переключаемся на эффект «Цвет»
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
    else if (t == _cAlarmSw()) {
        bool en = (p == "ON" || p == "on" || p == "1");
        timeMgr_setAlarmEnabled(en);
        mqttMgr_publishAlarmEnabled(en);
    }
    


    else if (t == _cSpeedN()) {
    uint8_t sp = (uint8_t)p.toInt();
    effects_currentSettings().speed = sp;
    loadingFlag = true;                                   // ← пересчитать эффект
    s_mqtt.publish(_tSpeedN().c_str(), String(sp).c_str(), true);   // ← обновить speed_n/status
    mqttMgr_publishSpeed(sp);                             // обновить и старый топик
    markSettingsChanged(); 
}

else if (t == _cScaleN()) {
    uint8_t sc = (uint8_t)p.toInt();
    if (sc > 100) sc = 100;
    effects_currentSettings().scale = sc;
    loadingFlag = true;
    s_mqtt.publish(_tScaleN().c_str(), String(sc).c_str(), true);   // ← обновить scale_n/status
    mqttMgr_publishScale(sc);
    markSettingsChanged(); 
}

}

// ============================================================
//  ПОДКЛЮЧЕНИЕ
// ============================================================
static void _connect() {
    if (!mqttMgr_enabled) return;
    if (strlen(s_cfg.host) == 0) return;
    if (WiFi.status() != WL_CONNECTED) return;

    String clientId = "gyverlamp-" + String(ESP.getChipId(), HEX);

    String willTopic   = _tState();
    String willPayload = "offline";

    bool ok;
    if (strlen(s_cfg.user) > 0) {
        ok = s_mqtt.connect(clientId.c_str(), s_cfg.user, s_cfg.pass,
                            willTopic.c_str(), 0, true, willPayload.c_str());
    } else {
        ok = s_mqtt.connect(clientId.c_str(), nullptr, nullptr,
                            willTopic.c_str(), 0, true, willPayload.c_str());
    }

    if (ok) {
    #if DEBUG_SERIAL
    Serial.println(F("[mqtt] connected"));
    #endif

    mqttMgr_connected = true;
    s_reconnectDelay = 5000;
    s_reconnectCount = 0;

    // подписки
    s_mqtt.subscribe(_cSwitch().c_str());
    s_mqtt.subscribe(_cBright().c_str());
    s_mqtt.subscribe(_cEffect().c_str());
    s_mqtt.subscribe(_cSpeed().c_str());
    s_mqtt.subscribe(_cScale().c_str());
    s_mqtt.subscribe(_cRgb().c_str());
    s_mqtt.subscribe(_cAlarmSw().c_str());
    s_mqtt.subscribe(_cSpeedN().c_str());
    s_mqtt.subscribe(_cScaleN().c_str());

    // публикуем online
    s_mqtt.publish(_tState().c_str(), "online", true);

    // === HA Discovery (ДОБАВИТЬ ЭТУ СТРОКУ) ===
    _publishDiscovery();

    // текущее состояние
    mqttMgr_publishState();
    mqttMgr_publishSensors();
    } else {
        #if DEBUG_SERIAL
        Serial.print(F("[mqtt] connect failed, rc="));
        Serial.println(s_mqtt.state());
        #endif

        s_reconnectCount++;
        if (s_reconnectCount > 10) s_reconnectCount = 10;
        s_reconnectDelay = 5000UL * (1UL << (s_reconnectCount - 1));
        if (s_reconnectDelay > 300000UL) s_reconnectDelay = 300000UL;
    }
}

// ============================================================
//  HA DISCOVERY
// ============================================================
static void _publishDiscovery() {
    String devId   = "gyverlamp_" + String(ESP.getChipId(), HEX);
    String devName = "GyverLamp "  + String(ESP.getChipId(), HEX);

    // --- Основная лампа (light) ---
    {
        DynamicJsonDocument doc(2048);
        doc["name"]         = devName;
        doc["uniq_id"]      = devId + "_light";
        doc["~"]            = s_baseTopic;
        doc["stat_t"]       = "~/status";
        doc["cmd_t"]        = "~/switch";
        doc["bri_stat_t"]   = "~/brightness/status";
        doc["bri_cmd_t"]    = "~/brightness/set";
        doc["bri_scl"]      = 255;
        doc["rgb_stat_t"]   = "~/rgb/status";
        doc["rgb_cmd_t"]    = "~/rgb/set";
        doc["fx_stat_t"]    = "~/effect/status";
        doc["fx_cmd_t"]     = "~/effect/set";
        doc["avty_t"]       = "~/status";
        doc["pl_avail"]     = "online";
        doc["pl_not_avail"] = "offline";

        // device
        JsonObject dev = doc.createNestedObject("dev");
        JsonArray  ids = dev.createNestedArray("identifiers");
        ids.add(devId);
        dev["name"]         = devName;
        dev["manufacturer"] = "Glyrat";
        dev["model"]        = "GyverLamp";
        dev["sw_version"]   = "MQTT 1.8";

        // fx_list
        JsonArray fx = doc.createNestedArray("fx_list");
        for (uint8_t i = 0; i < EFFECT_COUNT; i++) {
            fx.add(EFFECTS[i].name);
        }

        String topic = "homeassistant/light/" + devId + "/light/config";
        String out;
        serializeJson(doc, out);
        s_mqtt.publish(topic.c_str(), out.c_str(), true);
    }

    // --- Switch будильника ---
    {
        DynamicJsonDocument doc(1024);
        doc["name"]         = devName + " Alarm";
        doc["uniq_id"]      = devId + "_alarm";
        doc["~"]            = s_baseTopic;
        doc["stat_t"]       = "~/alarm/status";
        doc["cmd_t"]        = "~/alarm/set";
        doc["avty_t"]       = "~/status";
        doc["pl_avail"]     = "online";
        doc["pl_not_avail"] = "offline";

        JsonObject dev = doc.createNestedObject("dev");
        JsonArray  ids = dev.createNestedArray("identifiers");
        ids.add(devId);
        dev["name"]         = devName;
        dev["manufacturer"] = "GyverLamp";
        dev["model"]        = "RGB Lamp";

        String topic = "homeassistant/switch/" + devId + "/alarm/config";
        String out;
        serializeJson(doc, out);
        s_mqtt.publish(topic.c_str(), out.c_str(), true);
    }


// --- Number: Speed ---
{
    DynamicJsonDocument doc(1024);
    doc["name"]         = devName + " Speed";
    doc["uniq_id"]      = devId + "_speed";
    doc["~"]            = s_baseTopic;
    doc["stat_t"]       = "~/speed_n/status";
    doc["cmd_t"]        = "~/speed_n/set";
    doc["min"]          = 0;
    doc["max"]          = 255;
    doc["step"]         = 1;
    doc["mode"]         = "slider";
    doc["avty_t"]       = "~/status";
    doc["pl_avail"]     = "online";
    doc["pl_not_avail"] = "offline";

    JsonObject dev = doc.createNestedObject("dev");
    JsonArray  ids = dev.createNestedArray("identifiers");
    ids.add(devId);
    dev["name"]         = devName;
    dev["manufacturer"] = "GyverLamp";
    dev["model"]        = "RGB Lamp";

    String topic = "homeassistant/number/" + devId + "/speed/config";
    String out;
    serializeJson(doc, out);
    s_mqtt.publish(topic.c_str(), out.c_str(), true);
}

// --- Number: Scale ---
{
    DynamicJsonDocument doc(1024);
    doc["name"]         = devName + " Scale";
    doc["uniq_id"]      = devId + "_scale";
    doc["~"]            = s_baseTopic;
    doc["stat_t"]       = "~/scale_n/status";
    doc["cmd_t"]        = "~/scale_n/set";
    doc["min"]          = 0;
    doc["max"]          = 100;
    doc["step"]         = 1;
    doc["mode"]         = "slider";
    doc["avty_t"]       = "~/status";
    doc["pl_avail"]     = "online";
    doc["pl_not_avail"] = "offline";

    JsonObject dev = doc.createNestedObject("dev");
    JsonArray  ids = dev.createNestedArray("identifiers");
    ids.add(devId);
    dev["name"]         = devName;
    dev["manufacturer"] = "GyverLamp";
    dev["model"]        = "RGB Lamp";

    String topic = "homeassistant/number/" + devId + "/scale/config";
    String out;
    serializeJson(doc, out);
    s_mqtt.publish(topic.c_str(), out.c_str(), true);
}


    // --- Сенсоры ---
    struct SensorDef {
        const char* id;
        const char* name;
        const char* topic;
        const char* unit;
        const char* devClass;
    };

    static const SensorDef sensors[] = {
        {"rssi",       "RSSI",       "~/sensor/rssi",       "dBm", "signal_strength"},
        {"uptime",     "Uptime",     "~/sensor/uptime",     "s",   "duration"},
        {"vcc",        "VCC",        "~/sensor/vcc",        "V",   "voltage"},
        {"next_alarm", "Next Alarm", "~/sensor/next_alarm", nullptr, nullptr},
        {"alarm_in",   "Alarm In",   "~/sensor/alarm_in",   nullptr, nullptr},
        {"time",       "Time",       "~/sensor/time",       nullptr, nullptr},
    };

    for (uint8_t i = 0; i < sizeof(sensors) / sizeof(sensors[0]); i++) {
        const SensorDef& sd = sensors[i];

        DynamicJsonDocument doc(1024);
        doc["name"]         = devName + " " + sd.name;
        doc["uniq_id"]      = devId + "_" + sd.id;
        doc["~"]            = s_baseTopic;
        doc["stat_t"]       = sd.topic;
        doc["avty_t"]       = "~/status";
        doc["pl_avail"]     = "online";
        doc["pl_not_avail"] = "offline";
        if (sd.unit)     doc["unit_of_meas"] = sd.unit;
        if (sd.devClass) doc["dev_cla"]      = sd.devClass;

        JsonObject dev = doc.createNestedObject("dev");
        JsonArray  ids = dev.createNestedArray("identifiers");
        ids.add(devId);
        dev["name"]         = devName;
        dev["manufacturer"] = "GyverLamp";
        dev["model"]        = "RGB Lamp";

        String topic = String("homeassistant/sensor/") + devId + "/" + sd.id + "/config";
        String out;
        serializeJson(doc, out);
        s_mqtt.publish(topic.c_str(), out.c_str(), true);
    }
}

// ============================================================
//  ПУБЛИКАЦИИ
// ============================================================
void mqttMgr_publishPower(bool on) {
    if (!mqttMgr_connected) return;
    s_mqtt.publish(_tState().c_str(), on ? "ON" : "OFF", true);
}

void mqttMgr_publishBrightness(uint8_t bri) {
    if (!mqttMgr_connected) return;
    s_mqtt.publish(_tBright().c_str(), String(bri).c_str(), true);
}

void mqttMgr_publishRgb(uint8_t r, uint8_t g, uint8_t b) {
    if (!mqttMgr_connected) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%u,%u,%u", r, g, b);
    s_mqtt.publish(_tRgb().c_str(), buf, true);
}

void mqttMgr_publishEffect(int8_t idx) {
    if (!mqttMgr_connected) return;
    if (idx < 0 || idx >= EFFECT_COUNT) return;
    s_mqtt.publish(_tEffect().c_str(), EFFECTS[idx].name, true);
}

void mqttMgr_publishSpeed(uint8_t sp) {
    if (!mqttMgr_connected) return;
    s_mqtt.publish(_tSpeed().c_str(), String(sp).c_str(), true);
}

void mqttMgr_publishScale(uint8_t sc) {
    if (!mqttMgr_connected) return;
    s_mqtt.publish(_tScale().c_str(), String(sc).c_str(), true);
}

void mqttMgr_publishAlarmEnabled(bool en) {
    if (!mqttMgr_connected) return;
    s_mqtt.publish(_tAlarmSw().c_str(), en ? "ON" : "OFF", true);
}

void mqttMgr_publishState() {
    if (!mqttMgr_connected) return;

    mqttMgr_publishPower(ONflag);
    mqttMgr_publishBrightness(effects_currentSettings().brightness);
    mqttMgr_publishEffect(effects_currentMode());
    mqttMgr_publishSpeed(effects_currentSettings().speed);
    mqttMgr_publishScale(effects_currentSettings().scale);
    mqttMgr_publishAlarmEnabled(timeMgr_alarmEnabled());
    // RGB публикуем отдельно, когда эффект «Цвет» — а пока просто gR/gG/gB
    mqttMgr_publishRgb(gR, gG, gB);

        s_mqtt.publish(_tSpeedN().c_str(), String(effects_currentSettings().speed).c_str(), true);
    s_mqtt.publish(_tScaleN().c_str(), String(effects_currentSettings().scale).c_str(), true);

}

void mqttMgr_publishSensors() {
    if (!mqttMgr_connected) return;

     

    s_mqtt.publish(_tRssi().c_str(),      String(wifiMgr_getRssi()).c_str(), true);
    s_mqtt.publish(_tUptime().c_str(),    String(millis() / 1000).c_str(), true);
    s_mqtt.publish(_tVcc().c_str(),       String((float)ESP.getVcc() / 1000.0f).c_str(), true);
    s_mqtt.publish(_tNextAlarm().c_str(), timeMgr_getNextAlarmString().c_str(), true);
    s_mqtt.publish(_tAlarmIn().c_str(),   timeMgr_getTimeToNextAlarmString().c_str(), true);  // ← добавить
    s_mqtt.publish(_tCurTime().c_str(),   timeMgr_getTimeString().c_str(), true);
    s_mqtt.publish(_tStatusBin().c_str(), "online", true);

}

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ / ТИКЕР
// ============================================================
void mqttMgr_init() {
    // Загрузка MQTT-настроек из /mqtt.json
    // (файл создаётся через веб-страницу /mqtt)
    bool loaded = storage_loadMqtt(s_cfg);

    #if DEBUG_SERIAL
    Serial.print(F("[mqtt] load: "));
    Serial.print(loaded ? "ok" : "fail");
    Serial.print(F("  host='"));
    Serial.print(s_cfg.host);
    Serial.print(F("' port="));
    Serial.print(s_cfg.port);
    Serial.print(F(" user='"));
    Serial.print(s_cfg.user);
    Serial.print(F("' enabled="));
    Serial.println(s_cfg.enabled ? "1" : "0");
    #endif

    mqttMgr_enabled = s_cfg.enabled && (strlen(s_cfg.host) > 0);

    if (!mqttMgr_enabled) {
        #if DEBUG_SERIAL
        Serial.println(F("[mqtt] disabled"));
        #endif
        return;
    }

    s_baseTopic = String(MQTT_BASE_TOPIC) + "/" + String(ESP.getChipId(), HEX);

    s_mqtt.setServer(s_cfg.host, s_cfg.port);
    s_mqtt.setCallback(_onMqttMessage);
    s_mqtt.setBufferSize(2048);
    s_mqtt.setKeepAlive(30);

    #if DEBUG_SERIAL
    Serial.print(F("[mqtt] cfg: ")); Serial.print(s_cfg.host);
    Serial.print(':'); Serial.println(s_cfg.port);
    #endif
}

void mqttMgr_tick() {
    if (!mqttMgr_enabled) return;
    if (WiFi.status() != WL_CONNECTED) return;

    if (!s_mqtt.connected()) {
        mqttMgr_connected = false;
        uint32_t now = millis();
        if (now - s_lastReconnect >= s_reconnectDelay) {
            s_lastReconnect = now;
            _connect();
        }
    } else {
        s_mqtt.loop();

        // Периодические сенсоры раз в 60 сек
        uint32_t now = millis();
        if (now - s_lastSensorPub >= 60000UL) {
            s_lastSensorPub = now;
            mqttMgr_publishSensors();
        }
    }
}

void mqttMgr_reloadConfig() {
    storage_loadMqtt(s_cfg);
    mqttMgr_enabled = s_cfg.enabled && (strlen(s_cfg.host) > 0);

    if (mqttMgr_connected) {
        s_mqtt.disconnect();
        mqttMgr_connected = false;
    }

    if (mqttMgr_enabled) {
        s_mqtt.setServer(s_cfg.host, s_cfg.port);
        s_lastReconnect = 0;
        s_reconnectDelay = 5000;
        s_reconnectCount = 0;
    }
}

void mqttMgr_disconnect() {
    if (s_mqtt.connected()) {
        s_mqtt.publish(_tState().c_str(), "offline", true);
        s_mqtt.disconnect();
    }
    mqttMgr_connected = false;
    mqttMgr_enabled = false;
}
