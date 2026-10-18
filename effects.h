#pragma once

#include <Arduino.h>
#include "types.h"
#include "config.h"


extern uint8_t gR, gG, gB;

// ============================================================
//  РЕЕСТР ЭФФЕКТОВ
// ============================================================
// Количество эффектов (заполняется в effects.cpp)
extern const uint8_t EFFECT_COUNT;

// Массив эффектов (name + routine + флаги)
extern const Effect EFFECTS[];

// Хранилище настроек каждого эффекта (brightness/speed/scale)
extern EffectSettings effectSettings[];

// Флаги, которые эффекты используют в работе
extern volatile bool loadingFlag;    // перерисовать с нуля
extern bool demo;                    // демо-режим
extern bool epilepsy;                // разрешить "смену цвета" в демо

// ============================================================
//  API ДИСПЕТЧЕРА
// ============================================================
// Инициализация (только для загрузки настроек; сам эффект не запускает).
void effects_init();

// Запустить текущий эффект (вызывается из loop с частотой speed).
void effects_run();

// Сменить текущий эффект.
void effects_setMode(int8_t idx, bool save = true);

// Текущий индекс режима.
int8_t effects_currentMode();

// Настройки текущего эффекта (яркость/скорость/масштаб).
EffectSettings& effects_currentSettings();

// ============================================================
//  ВСПОМОГАТЕЛЬНОЕ ДЛЯ ЭФФЕКТОВ
// ============================================================
// Общие переменные, используемые несколькими эффектами.
// Здесь только объявления; определения — в effects.cpp.
extern uint8_t hue;
extern uint8_t hue2;
extern uint8_t deltaValue;
extern uint8_t step;
extern uint8_t deltaHue;
extern uint8_t deltaHue2;

// Палитры (для эффектов, где выбор палитры зависит от scale)
extern const TProgmemRGBPalette16* palette_arr[];
extern const uint8_t PALETTE_ARR_SIZE;
extern const TProgmemRGBPalette16* curPalette;

// Установить текущую палитру по масштабу (как в оригинальном setCurrentPalette).
void effects_setCurrentPalette();

// Пересчитать палитру для эффектов, у которых она выбирается из массива по scale.
void effects_setCurrentPallete(); // синоним с опечаткой из оригинала

// ============================================================
//  ГЛОБАЛЬНЫЕ, ИСПОЛЬЗУЕМЫЕ ЭФФЕКТАМИ
// ============================================================
extern CRGBPalette16 cPalette;
extern uint8_t       gR, gG, gB;
extern uint8_t       noise3d[2][WIDTH][HEIGHT];

// Сеттер цвета из веба/MQTT (используется эффектом «Цвет»)
void effects_setRgbColor(uint8_t r, uint8_t g, uint8_t b);