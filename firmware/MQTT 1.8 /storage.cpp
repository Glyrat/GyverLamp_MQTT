#include "storage.h"
#include "config.h"

#include <LittleFS.h>
#include <ArduinoJson.h>

// ============================================================
//  ВНУТРЕННИЕ ХЕЛПЕРЫ
// ============================================================
static bool _readFile(const char* path, String& out) {
    if (!LittleFS.exists(path)) return false;
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    out = f.readString();
    f.close();
    return out.length() > 0;
}

static bool _writeFile(const char* path, const String& data) {
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    size_t written = f.print(data);
    f.close();
    return written == data.length();
}

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ
// ============================================================
bool storage_init() {
    if (LittleFS.begin()) return true;

    #if DEBUG_SERIAL
    Serial.println(F("[storage] LittleFS mount failed, formatting..."));
    #endif

    if (!LittleFS.format()) return false;
    return LittleFS.begin();
}

// ============================================================
//  WI-FI
// ============================================================
bool storage_loadWifi(WifiConfig& out) {
    String json;
    if (!_readFile(FS_WIFI_FILE, json)) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    strlcpy(out.ssid, doc["ssid"] | "", sizeof(out.ssid));
    strlcpy(out.pass, doc["pass"] | "", sizeof(out.pass));
    return strlen(out.ssid) > 0;
}

bool storage_saveWifi(const WifiConfig& in) {
    JsonDocument doc;
    doc["ssid"] = in.ssid;
    doc["pass"] = in.pass;

    String json;
    serializeJson(doc, json);
    return _writeFile(FS_WIFI_FILE, json);
}

void storage_clearWifi() {
    if (LittleFS.exists(FS_WIFI_FILE)) LittleFS.remove(FS_WIFI_FILE);
}

// ============================================================
//  MQTT
// ============================================================
bool storage_loadMqtt(MqttConfig& out) {
    out.host[0] = 0;
    out.port    = 1883;
    out.user[0] = 0;
    out.pass[0] = 0;
    out.enabled = false;

    String json;
    if (!_readFile(FS_MQTT_FILE, json)) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    strlcpy(out.host, doc["host"] | "", sizeof(out.host));
    out.port    = doc["port"]    | 1883;
    strlcpy(out.user, doc["user"] | "", sizeof(out.user));
    strlcpy(out.pass, doc["pass"] | "", sizeof(out.pass));
    out.enabled = doc["enabled"] | false;
    return true;
}

bool storage_saveMqtt(const MqttConfig& in) {
    JsonDocument doc;
    doc["host"]    = in.host;
    doc["port"]    = in.port;
    doc["user"]    = in.user;
    doc["pass"]    = in.pass;
    doc["enabled"] = in.enabled;

    String json;
    serializeJson(doc, json);
    return _writeFile(FS_MQTT_FILE, json);
}

// ============================================================
//  СИСТЕМНЫЙ КОНФИГ
// ============================================================
bool storage_loadConfig(SystemConfig& out) {
    out.currentMode = 0;
    out.ONflag      = true;
    out.version     = 1;

    String json;
    if (!_readFile(FS_CONFIG_FILE, json)) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    out.currentMode = doc["mode"]    | 0;
    out.ONflag      = doc["on"]      | true;
    out.version     = doc["version"] | 1;
    return true;
}

bool storage_saveConfig(const SystemConfig& in) {
    JsonDocument doc;
    doc["mode"]    = in.currentMode;
    doc["on"]      = in.ONflag;
    doc["version"] = in.version;

    String json;
    serializeJson(doc, json);
    return _writeFile(FS_CONFIG_FILE, json);
}

// ============================================================
//  НАСТРОЙКИ ЭФФЕКТОВ
// ============================================================
bool storage_loadEffectSettings(EffectSettings* arr, uint16_t count) {
    String json;
    if (!_readFile(FS_CONFIG_FILE, json)) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    JsonArray eff = doc["effects"].as<JsonArray>();
    if (eff.isNull()) return false;

    uint16_t i = 0;
    for (JsonObject o : eff) {
        if (i >= count) break;
        arr[i].brightness = o["b"] | 40;
        arr[i].speed      = o["s"] | 30;
        arr[i].scale      = o["k"] | 40;
        i++;
    }
    return true;
}

bool storage_saveEffectSettings(const EffectSettings* arr, uint16_t count) {
    SystemConfig sys;
    storage_loadConfig(sys);

    JsonDocument doc;
    doc["mode"]    = sys.currentMode;
    doc["on"]      = sys.ONflag;
    doc["version"] = sys.version;

    JsonArray eff = doc["effects"].to<JsonArray>();
    for (uint16_t i = 0; i < count; i++) {
        JsonObject o = eff.add<JsonObject>();
        o["b"] = arr[i].brightness;
        o["s"] = arr[i].speed;
        o["k"] = arr[i].scale;
    }

    String json;
    serializeJson(doc, json);
    return _writeFile(FS_CONFIG_FILE, json);
}

// ============================================================
//  БУДИЛЬНИК
// ============================================================
bool storage_loadAlarms(AlarmConfig& out) {
    out.enabled = false;
    for (uint8_t i = 0; i < 7; i++) {
        out.days[i].enabled       = false;
        out.days[i].hour          = 7;
        out.days[i].minute        = 0;
        out.days[i].dawnMinutes   = 10;
        out.days[i].maxBrightness = 200;
        out.days[i].afterMode     = AFTER_OFF;
        out.days[i].afterDelay    = 10;
    }

    String json;
    if (!_readFile(FS_ALARMS_FILE, json)) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    out.enabled = doc["enabled"] | false;

    JsonArray days = doc["days"].as<JsonArray>();
    if (days.isNull()) return false;

    uint8_t i = 0;
    for (JsonObject d : days) {
        if (i >= 7) break;
        out.days[i].enabled       = d["on"]    | false;
        out.days[i].hour          = d["h"]     | 7;
        out.days[i].minute        = d["m"]     | 0;
        out.days[i].dawnMinutes   = d["dawn"]  | 10;
        out.days[i].maxBrightness = d["bri"]   | 200;
        out.days[i].afterMode     = (AfterMode)(d["after"] | 1);
        out.days[i].afterDelay    = d["delay"] | 10;
        i++;
    }
    return true;
}

bool storage_saveAlarms(const AlarmConfig& in) {
    JsonDocument doc;
    doc["enabled"] = in.enabled;

    JsonArray days = doc["days"].to<JsonArray>();
    for (uint8_t i = 0; i < 7; i++) {
        JsonObject d = days.add<JsonObject>();
        d["on"]    = in.days[i].enabled;
        d["h"]     = in.days[i].hour;
        d["m"]     = in.days[i].minute;
        d["dawn"]  = in.days[i].dawnMinutes;
        d["bri"]   = in.days[i].maxBrightness;
        d["after"] = (uint8_t)in.days[i].afterMode;
        d["delay"] = in.days[i].afterDelay;
    }

    String json;
    serializeJson(doc, json);
    return _writeFile(FS_ALARMS_FILE, json);
}

// ============================================================
//  BOOT COUNT
// ============================================================
bool storage_loadBoot(BootConfig& out) {
    out.count      = 0;
    out.lastBootMs = 0;

    String json;
    if (!_readFile(FS_BOOT_FILE, json)) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json)) return false;

    out.count      = doc["count"] | 0;
    out.lastBootMs = doc["last"]  | 0;
    return true;
}

bool storage_saveBoot(const BootConfig& in) {
    JsonDocument doc;
    doc["count"] = in.count;
    doc["last"]  = in.lastBootMs;

    String json;
    serializeJson(doc, json);
    return _writeFile(FS_BOOT_FILE, json);
}

void storage_resetBoot() {
    BootConfig b;
    b.count = 0;
    b.lastBootMs = 0;
    storage_saveBoot(b);
}
