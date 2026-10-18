#include "time_mgr.h"
#include "config.h"
#include "effects.h"
#include "matrix.h"

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <time.h>


// Снимок состояния для AFTER_DELAY (чтобы вернуть, если пользователь не трогал)
static bool   s_delaySavedValid    = false;
static CRGB   s_delaySavedRgb      = CRGB(0, 0, 0);
static int8_t s_delaySavedMode     = 0;
static uint8_t s_delaySavedBright  = 40;
static CRGB   s_delayAppliedRgb    = CRGB(255, 180, 60);   // что установили
static uint8_t s_delayAppliedMode  = 14;                   // "Цвет"
static uint8_t s_delayAppliedBright = 200;

// ============================================================
//  NTP
// ============================================================
static WiFiUDP  ntpUDP;
static NTPClient ntp(ntpUDP, NTP_SERVER, GMT_OFFSET_HOURS * 3600, NTP_INTERVAL_MS);

// ============================================================
//  СОСТОЯНИЕ
// ============================================================
bool    timeMgr_dawnActive  = false;
uint8_t timeMgr_hours       = 0;
uint8_t timeMgr_minutes     = 0;
uint8_t timeMgr_seconds     = 0;
uint8_t timeMgr_weekday     = 0;       // 0=Пн..6=Вс
bool    timeMgr_timeSynced  = false;

// Настройки будильников (в RAM, копия из /alarms.json)
static AlarmConfig s_alarms;

// Миллис последнего обновления секунд
static uint32_t s_lastSecTick = 0;

// Снимок состояния до начала рассвета
static bool    s_prevONflag  = false;
static int8_t  s_prevMode    = 0;
static uint8_t s_prevBright  = 40;

// Учёт текущего активного будильника
static int8_t  s_activeAlarmDay = -1;   // индекс дня, для которого идёт рассвет
static uint32_t s_afterDelayMs  = 0;    // когда выключать (для AFTER_DELAY)
static bool     s_afterDelayPending = false;

// NTP-таймер
static uint32_t s_lastNtpSync = 0;

// ============================================================
//  ВНУТРЕННИЕ
// ============================================================
static void _applyNtpTime() {
    timeMgr_hours   = ntp.getHours();
    timeMgr_minutes = ntp.getMinutes();
    timeMgr_seconds = ntp.getSeconds();

    // NTPClient::getDay() возвращает 0=Вс..6=Сб.
    // Переведём в 0=Пн..6=Вс.
    uint8_t wd = ntp.getDay();  // 0..6, 0=Вс
    timeMgr_weekday = (wd == 0) ? 6 : (wd - 1);

    timeMgr_timeSynced = true;
    s_lastNtpSync = millis();

    #if DEBUG_SERIAL
    Serial.print(F("[time] NTP synced: "));
    Serial.print(timeMgr_hours); Serial.print(':');
    Serial.print(timeMgr_minutes); Serial.print(':');
    Serial.println(timeMgr_seconds);
    #endif
}

static void _saveSnapshot() {
    s_prevONflag = true;    // во время рассвета лампа точно включена
    s_prevMode   = effects_currentMode();
    s_prevBright = effects_currentSettings().brightness;
}

static void _restoreAfterDawn() {
    if (s_activeAlarmDay < 0) return;

    AlarmDay d = s_alarms.days[s_activeAlarmDay];

    switch (d.afterMode) {
        case AFTER_ON:
            // оставить включённой, эффект — тот же, что был
            effects_setMode(s_prevMode, false);
            FastLED.setBrightness(s_prevBright);
            {
                extern bool ONflag;
                ONflag = true;
            }
            markSettingsChanged();
            #if DEBUG_SERIAL
            Serial.println(F("[alarm] AFTER_ON → prev effect"));
            #endif
            break;

        case AFTER_OFF:
            FastLED.setBrightness(s_prevBright);
            effects_setMode(s_prevMode, false);
            {
                extern bool ONflag;
                ONflag = false;
            }
            markSettingsChanged();
            #if DEBUG_SERIAL
            Serial.println(F("[alarm] AFTER_OFF → OFF"));
            #endif
            break;

        case AFTER_DELAY: {
            // Сохраняем прежние настройки
            s_delaySavedRgb   = CRGB(gR, gG, gB);
            s_delaySavedMode  = effects_currentMode();
            s_delaySavedBright = effects_currentSettings().brightness;
            s_delaySavedValid = true;

            // Цвет рассвета (жёлто-белый)
            CHSV finalColor(35, 170, 255);
            CRGB rgb = finalColor;

            s_delayAppliedRgb    = rgb;
            s_delayAppliedBright = d.maxBrightness;

            // Найти «Цвет»
            int8_t colorMode = -1;
            for (uint8_t i = 0; i < EFFECT_COUNT; i++) {
                if (String(EFFECTS[i].name) == "Цвет") { colorMode = i; break; }
            }
            s_delayAppliedMode = (colorMode >= 0) ? (uint8_t)colorMode : 14;

            // Применить
            effects_setRgbColor(rgb.r, rgb.g, rgb.b);
            if (colorMode >= 0) effects_setMode(colorMode, false);
            FastLED.setBrightness(d.maxBrightness);
            effects_currentSettings().brightness = d.maxBrightness;

            {
                extern bool ONflag;
                ONflag = true;
            }

            s_afterDelayPending = true;
            s_afterDelayMs = millis() + (uint32_t)d.afterDelay * 60000UL;
            markSettingsChanged();

            #if DEBUG_SERIAL
            Serial.print(F("[alarm] AFTER_DELAY → warm color "));
            Serial.print(d.afterDelay);
            Serial.println(F(" min"));
            #endif
            break;
        }
    }

    s_activeAlarmDay = -1;
}

// Начало рассвета для указанного дня.
static void _startDawn(uint8_t dayIdx) {
    _saveSnapshot();
    s_activeAlarmDay = dayIdx;
    timeMgr_dawnActive = true;

    #if DEBUG_SERIAL
    Serial.print(F("[dawn] start for day "));
    Serial.println(dayIdx);
    #endif
}

// Завершение рассвета.
static void _stopDawn() {
    timeMgr_dawnActive = false;

    // если после рассвета не начинался AFTER_DELAY — просто сбросить
    if (!s_afterDelayPending) {
        s_delaySavedValid = false;
    }

    #if DEBUG_SERIAL
    Serial.println(F("[dawn] stop"));
    #endif
    _restoreAfterDawn();
}

// Проверка: попали ли в окно рассвета.
// Возвращает индекс дня будильника или -1.
static int8_t _checkDawnWindow() {
    if (!s_alarms.enabled) return -1;

    uint16_t nowMin = timeMgr_hours * 60 + timeMgr_minutes;

    for (uint8_t i = 0; i < 7; i++) {
        const AlarmDay& d = s_alarms.days[i];
        if (!d.enabled) continue;
        if (i != timeMgr_weekday) continue;

        uint16_t almMin = d.hour * 60 + d.minute;
        uint16_t dawnStart = (almMin >= d.dawnMinutes) ? (almMin - d.dawnMinutes) : 0;

        // #if DEBUG_SERIAL
        // Serial.print(F("[dawn chk] day=")); Serial.print(i);
        // Serial.print(F(" wd="));             Serial.print(timeMgr_weekday);
        // Serial.print(F(" now="));            Serial.print(nowMin);
        // Serial.print(F(" alm="));            Serial.print(almMin);
        // Serial.print(F(" start="));          Serial.print(dawnStart);
        // Serial.print(F(" in="));
        // Serial.println((nowMin >= dawnStart && nowMin < almMin) ? "YES" : "no");
        // #endif

        if (nowMin >= dawnStart && nowMin < almMin) return (int8_t)i;
    }
    return -1;
}

// Расчёт текущего цвета/яркости для рассвета.
static void _computeDawnColor(uint8_t dayIdx, CHSV& outColor, uint8_t& outBright) {
    const AlarmDay& d = s_alarms.days[dayIdx];

    uint16_t nowSec  = timeMgr_hours * 3600 + timeMgr_minutes * 60 + timeMgr_seconds;
    uint16_t almSec  = d.hour * 3600 + d.minute * 60;
    uint16_t dawnSec = d.dawnMinutes * 60;

    // Прогресс 0..255
    uint8_t progress = 0;
    if (nowSec >= almSec) {
        progress = 255;
    } else if (dawnSec > 0) {
        uint16_t elapsed = nowSec - (almSec - dawnSec);
        if (elapsed > 65535U) elapsed = 0;  // защита от underflow (ночь)
        progress = (uint8_t)((uint32_t)elapsed * 255UL / dawnSec);
    }

    // HUE: 10 (красный) -> 35 (жёлто-белый)
    uint8_t hueVal  = map(progress, 0, 255, 10, 35);
    // Насыщенность: 255 -> 170
    uint8_t satVal  = map(progress, 0, 255, 255, 170);
    // Яркость: 10 -> d.maxBrightness
    uint8_t briVal  = map(progress, 0, 255, 10, d.maxBrightness);

    outColor  = CHSV(hueVal, satVal, briVal);
    outBright = briVal;
}

// ============================================================
//  ИНИЦИАЛИЗАЦИЯ
// ============================================================
void timeMgr_init() {
    ntp.begin();
    ntp.setTimeOffset(GMT_OFFSET_HOURS * 3600);

    // Загружаем будильники из LittleFS
    if (!storage_loadAlarms(s_alarms)) {
        // storage_loadAlarms уже заполняет дефолты
        #if DEBUG_SERIAL
        Serial.println(F("[time] alarms: using defaults"));
        #endif
    }

    // Пытаемся синхронизироваться (до 5 попыток)
    for (uint8_t i = 0; i < 5; i++) {
        if (ntp.update()) {
            _applyNtpTime();
            break;
        }
        delay(500);
    }
}

// ============================================================
//  ТИКЕР
// ============================================================
void timeMgr_tick() {
    uint32_t now = millis();

    // Обновление секунд (локально между NTP-синхронизациями)
    if (now - s_lastSecTick >= 1000) {
        s_lastSecTick = now;

        timeMgr_seconds++;
        if (timeMgr_seconds >= 60) {
            timeMgr_seconds = 0;
            timeMgr_minutes++;
            if (timeMgr_minutes >= 60) {
                timeMgr_minutes = 0;
                timeMgr_hours++;
                if (timeMgr_hours >= 24) {
                    timeMgr_hours = 0;
                    timeMgr_weekday = (timeMgr_weekday + 1) % 7;
                }
            }
        }
    }

    // NTP-обновление раз в NTP_INTERVAL_MS
    if (WiFi.status() == WL_CONNECTED) {
        if (now - s_lastNtpSync >= NTP_INTERVAL_MS) {
            if (ntp.update()) {
                _applyNtpTime();
            } else {
                s_lastNtpSync = now;   // не спамить
            }
        }
    }

    // Проверка рассвета раз в 3 секунды
    static uint32_t lastDawnCheck = 0;
    if (now - lastDawnCheck >= 3000) {
        lastDawnCheck = now;

        int8_t dayIdx = _checkDawnWindow();

        if (dayIdx >= 0 && !timeMgr_dawnActive) {
            // начали рассвет
            _startDawn(dayIdx);
            timeMgr_dawnActive = true;
        } else if (dayIdx < 0 && timeMgr_dawnActive) {
            // рассвет закончился
            _stopDawn();
        }
    }

if (s_afterDelayPending && now >= s_afterDelayMs) {
    s_afterDelayPending = false;

    // Проверяем: пользователь что-то трогал?
    bool userTouched = false;

    if (effects_currentMode() != (int8_t)s_delayAppliedMode) userTouched = true;
    if (gR != s_delayAppliedRgb.r ||
        gG != s_delayAppliedRgb.g ||
        gB != s_delayAppliedRgb.b) userTouched = true;
    if (effects_currentSettings().brightness != s_delayAppliedBright) userTouched = true;

    extern bool ONflag;
    if (!ONflag) userTouched = true;

    #if DEBUG_SERIAL
    Serial.print(F("[alarm] AFTER_DELAY timeout, userTouched="));
    Serial.println(userTouched ? "YES" : "no");
    #endif

    if (userTouched) {
        // Пользователь что-то трогал — ничего не делаем:
        // ни восстанавливаем, ни выключаем. Лампа остаётся как есть.
        #if DEBUG_SERIAL
        Serial.println(F("[alarm] AFTER_DELAY skipped (user intervention)"));
        #endif
    } else {
        // Пользователь не трогал — восстанавливаем прежнее и выключаем
        if (s_delaySavedValid) {
            effects_setRgbColor(s_delaySavedRgb.r, s_delaySavedRgb.g, s_delaySavedRgb.b);
            if (s_delaySavedMode >= 0 && s_delaySavedMode < EFFECT_COUNT) {
                effects_setMode(s_delaySavedMode, false);
            }
            FastLED.setBrightness(s_delaySavedBright);
            effects_currentSettings().brightness = s_delaySavedBright;
        }
        ONflag = false;
        markSettingsChanged();

        #if DEBUG_SERIAL
        Serial.println(F("[alarm] AFTER_DELAY timeout → restore + OFF"));
        #endif
    }

    s_delaySavedValid = false;
}
}

// ============================================================
//  ОТРИСОВКА РАССВЕТА
// ============================================================
void timeMgr_drawDawn() {
    if (s_activeAlarmDay < 0) return;

    CHSV color;
    uint8_t bri;
    _computeDawnColor(s_activeAlarmDay, color, bri);

    FastLED.setBrightness(255);
    matrix_fillAll(color);
}

// ============================================================
//  УПРАВЛЕНИЕ БУДИЛЬНИКОМ
// ============================================================
bool timeMgr_alarmEnabled() {
    return s_alarms.enabled;
}

void timeMgr_setAlarmEnabled(bool en) {
    s_alarms.enabled = en;
    timeMgr_saveAlarms();
}

AlarmDay timeMgr_getAlarmDay(uint8_t day) {
    if (day > 6) day = 0;
    return s_alarms.days[day];
}

void timeMgr_setAlarmDay(uint8_t day, const AlarmDay& d) {
    if (day > 6) return;
    s_alarms.days[day] = d;
    timeMgr_saveAlarms();

    #if DEBUG_SERIAL
    Serial.print(F("[time] setAlarmDay ")); Serial.print(day);
    Serial.print(F(" -> ")); Serial.print(d.hour);
    Serial.print(':');       Serial.println(d.minute);
    #endif
    
}

void timeMgr_saveAlarms() {
    storage_saveAlarms(s_alarms);
}

// ============================================================
//  ИНФО
// ============================================================
String timeMgr_getTimeString() {
    char buf[10];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
             timeMgr_hours, timeMgr_minutes, timeMgr_seconds);
    return String(buf);
}

String timeMgr_getNextAlarmString() {
    if (!s_alarms.enabled) return "—";

    uint16_t nowMin = timeMgr_hours * 60 + timeMgr_minutes;

    int16_t bestDelta = -1;
    uint8_t bestDay = 0;
    uint8_t bestH = 0, bestM = 0;

    for (uint8_t i = 0; i < 7; i++) {
        const AlarmDay& d = s_alarms.days[i];
        if (!d.enabled) continue;

        int16_t dayDiff = (int16_t)i - (int16_t)timeMgr_weekday;
        if (dayDiff < 0) dayDiff += 7;

        int16_t almMin = d.hour * 60 + d.minute;
        int16_t delta   = dayDiff * 1440 + (almMin - nowMin);
        if (delta <= 0) delta += 7 * 1440;

        if (bestDelta < 0 || delta < bestDelta) {
            bestDelta = delta;
            bestDay = i;
            bestH = d.hour;
            bestM = d.minute;
        }
    }

    if (bestDelta < 0) return "—";

    static const char* dayNames[] = {"Пн","Вт","Ср","Чт","Пт","Сб","Вс"};

    // Разбиваем delta в минутах на ч/мин
    uint16_t dh = bestDelta / 60;
    uint16_t dm = bestDelta % 60;

    char buf[40];
    snprintf(buf, sizeof(buf), "%s %02u:%02u",
             dayNames[bestDay], bestH, bestM);
    return String(buf);
}

// Новая функция: «осталось до будильника» как строка
// Пример: "1 ч 12 мин", "45 мин", "3 дн 5 ч"
String timeMgr_getTimeToNextAlarmString() {
    if (!s_alarms.enabled) return "—";

    int16_t best = -1;
    uint16_t nowMin = timeMgr_hours * 60 + timeMgr_minutes;

    for (uint8_t i = 0; i < 7; i++) {
        const AlarmDay& d = s_alarms.days[i];
        if (!d.enabled) continue;

        int16_t dayDiff = (int16_t)i - (int16_t)timeMgr_weekday;
        if (dayDiff < 0) dayDiff += 7;

        int16_t almMin = d.hour * 60 + d.minute;
        int16_t delta  = dayDiff * 1440 + (almMin - nowMin);
        if (delta <= 0) delta += 7 * 1440;

        if (best < 0 || delta < best) best = delta;
    }

    if (best < 0) return "—";

    char buf[32];
    if (best < 60) {
        snprintf(buf, sizeof(buf), "%u мин", best);
    } else if (best < 1440) {
        snprintf(buf, sizeof(buf), "%u ч %u мин", best / 60, best % 60);
    } else {
        snprintf(buf, sizeof(buf), "%u дн %u ч", best / 1440, (best % 1440) / 60);
    }
    return String(buf);
}

int timeMgr_minutesToNextAlarm() {
    if (!s_alarms.enabled) return -1;

    uint16_t nowMin = timeMgr_hours * 60 + timeMgr_minutes;
    int16_t best = -1;

    for (uint8_t i = 0; i < 7; i++) {
        const AlarmDay& d = s_alarms.days[i];
        if (!d.enabled) continue;

        int16_t dayDiff = (int16_t)i - (int16_t)timeMgr_weekday;
        if (dayDiff < 0) dayDiff += 7;

        int16_t almMin = d.hour * 60 + d.minute;
        int16_t delta   = dayDiff * 1440 + (almMin - nowMin);
        if (delta <= 0) delta += 7 * 1440;

        if (best < 0 || delta < best) best = delta;
    }
    return best;
}

void timeMgr_forceNtpSync() {
    if (ntp.update()) {
        _applyNtpTime();
    }
}
