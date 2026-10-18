// ============================================================
//  Радуга и цвет
//  - Rainbow Vertical (Радуга верт.)
//  - Rainbow Horizontal (Радуга гориз.)
//  - Colors (Смена цвета)
//  - Color (заливка RGB, используется из HA и веба)
// ============================================================

#include <Arduino.h>
#include <FastLED.h>
#include "effects.h"
#include "matrix.h"
#include "config.h"

// ============================================================
//  ГЛОБАЛЬНЫЙ ЦВЕТ ДЛЯ ЭФФЕКТА "ЦВЕТ"
// ============================================================
// (gR/gG/gB определены в effects.cpp)

// ============================================================
//  РАДУГА ВЕРТИКАЛЬНАЯ
// ============================================================
void rainbowVertical() {
    EffectSettings& s = effects_currentSettings();
    uint8_t step = map(s.speed, 0, 255, 1, 8);
    hue += step;
    for (uint8_t j = 0; j < HEIGHT; j++) {
        CHSV thisColor = CHSV((uint8_t)(hue + j * s.scale), 255, 255);
        for (uint8_t i = 0; i < WIDTH; i++) {
            matrix_drawPixel(i, j, thisColor);
        }
    }
}

// ============================================================
//  РАДУГА ГОРИЗОНТАЛЬНАЯ
// ============================================================
void rainbowHorizontal() {
    EffectSettings& s = effects_currentSettings();
    uint8_t step = map(s.speed, 0, 255, 1, 8);
    hue += step;
    for (uint8_t i = 0; i < WIDTH; i++) {
        CHSV thisColor = CHSV((uint8_t)(hue + i * s.scale), 255, 255);
        for (uint8_t j = 0; j < HEIGHT; j++) {
            matrix_drawPixel(i, j, thisColor);
        }
    }
}

// ============================================================
//  СМЕНА ЦВЕТА
// ============================================================
void colorsRoutine() {
    EffectSettings& s = effects_currentSettings();

    curPalette = &RainbowColors_p;
    EVERY_N_MILLIS(s.speed) {
        hue++;
    }

    for (uint8_t x = 0; x < WIDTH; x++) {
        for (uint8_t y = 0; y < HEIGHT; y++) {
            matrix_drawPixel(x, y, ColorFromPalette(*curPalette, hue));
        }
    }
}

// ============================================================
//  ЗАЛИВКА ЦВЕТОМ
// ============================================================
void colorRoutine() {
    CRGB c(gR, gG, gB);
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = c;
    }
}