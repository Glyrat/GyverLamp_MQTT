extern "C" {
#include "user_interface.h"
}
ADC_MODE(ADC_VCC);

#include <FastLED.h>
#include "config.h"
#include "types.h"
#include "matrix.h"
#include "effects.h"
#include "storage.h"
#include "time_mgr.h"
#include "wifi_mgr.h"
#include "mqtt_mgr.h"
#include "web.h"
#include "ota.h"


CRGB leds[NUM_LEDS];
bool ONflag = true;

// ============================================================
//  ОТЛОЖЕННОЕ СОХРАНЕНИЕ
// ============================================================
static uint32_t s_lastChangeMs = 0;
static bool     s_settingsChanged = false;

void markSettingsChanged() {
    s_settingsChanged = true;
    s_lastChangeMs = millis();
}

void saveSettingsNow() {
    SystemConfig cfg;
    cfg.currentMode = effects_currentMode();
    cfg.ONflag      = ONflag;
    cfg.version     = 1;
    storage_saveConfig(cfg);
    storage_saveEffectSettings(effectSettings, EFFECT_COUNT);
    s_settingsChanged = false;
    
    #if DEBUG_SERIAL
        Serial.println(F("[save] settings saved"));
    #endif
}


// ============================================================
//  ПРИМЕНЕНИЕ СОСТОЯНИЯ (вкл/выкл)
// ============================================================
void applyPower() {
    if (!ONflag) {
        FastLED.clear();
        FastLED.show();
    }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(500);
    Serial.println(F("Boot..."));

    FastLED.addLeds<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
    FastLED.clear();
    FastLED.show();

    storage_init();
    effects_init();

    // Загружаем сохранённое состояние
    {
        SystemConfig cfg;
        if (storage_loadConfig(cfg)) {
            effects_setMode(cfg.currentMode, false);
            ONflag = cfg.ONflag;
        }
        // настройки эффектов
        storage_loadEffectSettings(effectSettings, EFFECT_COUNT);
        FastLED.setBrightness(effects_currentSettings().brightness);
    }

    // Wi-Fi + анимация старта
    wifiMgr_showStartupAnim();
    wifiMgr_init();

    // Время и MQTT — только если подключены
    if (wifiMgr_connected) {
        timeMgr_init();
        mqttMgr_init();
        web_init(); 
        web_initOta();        // ← ЭТУ СТРОКУ
    }

    Serial.println(F("Ready"));
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    // --- сервисные модули
    wifiMgr_tick();
    if (wifiMgr_connected) {
        timeMgr_tick();
        mqttMgr_tick();
        web_tick();  
    }

    // --- отрисовка
    static uint32_t lastTick = 0;

    
    // Отложенное сохранение (через 5 сек после последнего изменения)
    if (s_settingsChanged && (millis() - s_lastChangeMs > 5000)) {
    saveSettingsNow();
    }

    if (millis() - lastTick > 30) {
        lastTick = millis();

        if (wifiMgr_inAP) {
            // портал Wi-Fi
            if (wifiMgr_apHasClient) wifiMgr_drawClientFrame();
            else                     wifiMgr_drawAPFrame();
            FastLED.show();
        }
        else if (timeMgr_dawnActive) {
            // рассвет
            timeMgr_drawDawn();
            FastLED.show();
        }
        else if (ONflag) {
            // обычный режим
            effects_run();
            FastLED.show();
        }
        else {
            // выключено — держим матрицу тёмной
            FastLED.clear();
            FastLED.show();
        }
    }

}