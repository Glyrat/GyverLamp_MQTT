#pragma once

#include <Arduino.h>
#include "types.h"

// ============================================================
//  СОСТОЯНИЕ
// ============================================================
extern bool mqttMgr_connected;
extern bool mqttMgr_enabled;

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ
// ============================================================
// Загружает /mqtt.json. Не подключается — это делает tick().
// Вызывать в setup() после wifiMgr_init(), если wifiMgr_connected.
void mqttMgr_init();

// ============================================================
//  ТИКЕР
// ============================================================
// Вызывать из loop() часто. Обрабатывает реконнект и loop().
void mqttMgr_tick();

// ============================================================
//  ПУБЛИКАЦИЯ СОСТОЯНИЯ
// ============================================================
// Публикует всё состояние (on/off, brightness, RGB, effect).
// Вызывать изменить после изменения настроек.
void mqttMgr_publishState();

// Публикация одного параметра.
void mqttMgr_publishPower(bool on);
void mqttMgr_publishBrightness(uint8_t bri);
void mqttMgr_publishRgb(uint8_t r, uint8_t g, uint8_t b);
void mqttMgr_publishEffect(int8_t idx);
void mqttMgr_publishSpeed(uint8_t sp);
void mqttMgr_publishScale(uint8_t sc);
void mqttMgr_publishAlarmEnabled(bool en);

// Периодические сенсоры (RSSI, uptime, VCC, nextAlarm, time).
// Вызывать раз в 30-60 сек.
void mqttMgr_publishSensors();

// ============================================================
//  УПРАВЛЕНИЕ НАСТРОЙКАМИ MQTT
// ============================================================
// Перечитать конфиг из /mqtt.json и переподключиться.
void mqttMgr_reloadConfig();

// Отключиться (например, при отключении MQTT через веб).
void mqttMgr_disconnect();