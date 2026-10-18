#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "types.h"
#include "storage.h"

// ============================================================
//  СОСТОЯНИЕ МОДУЛЯ (глобально)
// ============================================================
extern bool    timeMgr_dawnActive;   // сейчас идёт рассвет (для отрисовки)
extern uint8_t timeMgr_hours;
extern uint8_t timeMgr_minutes;
extern uint8_t timeMgr_seconds;
extern uint8_t timeMgr_weekday;      // 0=Пн..6=Вс
extern bool    timeMgr_timeSynced;   // было ли успешное NTP-обновление

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ / ТИКЕР
// ============================================================
// Запуск NTP-клиента. Вызывать после подключения к Wi-Fi.
void timeMgr_init();

// Вызывать из loop() как можно чаще.
void timeMgr_tick();

// Отрисовка рассвета. Вызывать из loop() когда timeMgr_dawnActive == true,
// ВМЕСТО effects_run().
void timeMgr_drawDawn();

// ============================================================
//  УПРАВЛЕНИЕ БУДИЛЬНИКОМ (для веба / MQTT)
// ============================================================
// Глобальный вкл/выкл будильника.
bool timeMgr_alarmEnabled();
void timeMgr_setAlarmEnabled(bool en);

// Получить/установить настройки одного дня (0..6).
AlarmDay timeMgr_getAlarmDay(uint8_t day);
void     timeMgr_setAlarmDay(uint8_t day, const AlarmDay& d);

// Сохранить настройки будильника в LittleFS.
void timeMgr_saveAlarms();

// ============================================================
//  ИНФО (для веба / MQTT)
// ============================================================
// "HH:MM:SS"
String timeMgr_getTimeString();



// "Пн 23:29" — день недели + время будильника
String timeMgr_getNextAlarmString();

// "1 ч 12 мин" — сколько осталось до следующего будильника
String timeMgr_getTimeToNextAlarmString();

// Сколько минут до следующего будильника (для сенсора HA).
// -1 если будильник отключён или не настроен.
int timeMgr_minutesToNextAlarm();

// Ручной перезапуск NTP-синхронизации.
void timeMgr_forceNtpSync();
