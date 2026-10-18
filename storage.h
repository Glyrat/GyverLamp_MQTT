#pragma once

#include <Arduino.h>
#include "types.h"

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ
// ============================================================
// Монтирует LittleFS. Если не удалось — форматирует.
// Возвращает true, если ФС готова к работе.
bool storage_init();

// ============================================================
//  WI-FI
// ============================================================
bool storage_loadWifi(WifiConfig& out);
bool storage_saveWifi(const WifiConfig& in);
void storage_clearWifi();

// ============================================================
//  MQTT
// ============================================================
bool storage_loadMqtt(MqttConfig& out);
bool storage_saveMqtt(const MqttConfig& in);

// ============================================================
//  СИСТЕМНЫЙ КОНФИГ
// ============================================================
bool storage_loadConfig(SystemConfig& out);
bool storage_saveConfig(const SystemConfig& in);

// ============================================================
//  НАСТРОЙКИ ЭФФЕКТОВ (массив на MODE_AMOUNT)
// ============================================================
// effectCount — сколько элементов писать/читать (обычно MODE_AMOUNT)
bool storage_loadEffectSettings(EffectSettings* arr, uint16_t count);
bool storage_saveEffectSettings(const EffectSettings* arr, uint16_t count);

// ============================================================
//  БУДИЛЬНИК
// ============================================================
struct AlarmConfig {
    bool      enabled;        // глобальный вкл/выкл будильника
    AlarmDay  days[7];        // пн..вс
};

bool storage_loadAlarms(AlarmConfig& out);
bool storage_saveAlarms(const AlarmConfig& in);

// ============================================================
//  BOOT COUNT (быстрые перезагрузки)
// ============================================================
bool storage_loadBoot(BootConfig& out);
bool storage_saveBoot(const BootConfig& in);
void storage_resetBoot();