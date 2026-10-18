// ============================================================
//  Эффекты Снегопад и Матрица
// ============================================================

#include <Arduino.h>
#include <FastLED.h>
#include "effects.h"
#include "matrix.h"
#include "config.h"

// ============================================================
//  СНЕГОПАД
// ============================================================
void snowRoutine() {
    EffectSettings& s = effects_currentSettings();
    static uint8_t skip = 0;
    uint8_t period = map(s.speed, 0, 255, 6, 1);   // 6=медленно, 1=быстро
    if (++skip < period) return;
    skip = 0;

    // сдвигаем всё вниз
    for (uint8_t x = 0; x < WIDTH; x++) {
        for (uint8_t y = 0; y < HEIGHT - 1; y++) {
            matrix_drawPixel(x, y, CRGB(matrix_getPixelXY(x, y + 1)));
        }
    }

    for (uint8_t x = 0; x < WIDTH; x++) {
        // верхняя строка — случайная снежинка
        if (matrix_getPixelXY(x, HEIGHT - 2) == 0 &&
            (random(0, s.scale) == 0))
            matrix_drawPixel(x, HEIGHT - 1, 0xE0FFFF - 0x101010 * random(0, 4));
        else
            matrix_drawPixel(x, HEIGHT - 1, CRGB(0, 0, 0));
    }
}

// ============================================================
//  МАТРИЦА
// ============================================================
void matrixRoutine() {
    EffectSettings& s = effects_currentSettings();
    static uint8_t skip = 0;
    uint8_t period = map(s.speed, 0, 255, 6, 1);
    if (++skip < period) return;
    skip = 0;
    
    for (uint8_t x = 0; x < WIDTH; x++) {
        uint32_t thisColor = matrix_getPixelXY(x, HEIGHT - 1);
        if (thisColor == 0)
            matrix_drawPixel(x, HEIGHT - 1,
                             CRGB(0x00FF00) * (random(0, s.scale) == 0));
        else if (thisColor < 0x002000)
            matrix_drawPixel(x, HEIGHT - 1, CRGB(0, 0, 0));
        else
            matrix_drawPixel(x, HEIGHT - 1,
                             CRGB(thisColor - 0x002000));
    }

    // сдвигаем всё вниз
    for (uint8_t x = 0; x < WIDTH; x++) {
        for (uint8_t y = 0; y < HEIGHT - 1; y++) {
            matrix_drawPixel(x, y, CRGB(matrix_getPixelXY(x, y + 1)));
        }
    }
}
