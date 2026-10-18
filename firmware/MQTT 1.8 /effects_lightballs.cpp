// ============================================================
//  Эффект "Пейнтбол" (lightBalls)
//  Перенос из effects.ino
// ============================================================

#include <Arduino.h>
#include <FastLED.h>
#include "effects.h"
#include "matrix.h"
#include "config.h"

// ============================================================
//  ГРАНИЦЫ ДЛЯ РАЗМЫТИЯ
// ============================================================
#define BORDERTHICKNESS 1U

// ============================================================
//  ЭФФЕКТ
// ============================================================
void lightBallsRoutine() {
    EffectSettings& s = effects_currentSettings();

    // размываем то, что уже на матрице (без очистки)
    uint8_t blurAmount = dim8_raw(beatsin8(3, 64, 100));
    matrix_blur2d(blurAmount);

    const uint8_t paintWidth  = WIDTH  - BORDERTHICKNESS * 2;
    const uint8_t paintHeight = HEIGHT - BORDERTHICKNESS * 2;

    // 4 синусоидальных точки (разные фазы)
    uint16_t i = beatsin16(79, 0, 255);
    uint16_t j = beatsin16(67, 0, 255);
    uint16_t k = beatsin16(53, 0, 255);
    uint16_t m = beatsin16(97, 0, 255);

    uint32_t ms = millis() / (s.scale / 4 + 1);

    leds[matrix_pixelNumber(highByte(i * paintWidth) + BORDERTHICKNESS,
                            highByte(j * paintHeight) + BORDERTHICKNESS)]
        += CHSV(ms / 29, 200U, 255U);

    leds[matrix_pixelNumber(highByte(j * paintWidth) + BORDERTHICKNESS,
                            highByte(k * paintHeight) + BORDERTHICKNESS)]
        += CHSV(ms / 41, 200U, 255U);

    leds[matrix_pixelNumber(highByte(k * paintWidth) + BORDERTHICKNESS,
                            highByte(m * paintHeight) + BORDERTHICKNESS)]
        += CHSV(ms / 37, 200U, 255U);

    leds[matrix_pixelNumber(highByte(m * paintWidth) + BORDERTHICKNESS,
                            highByte(i * paintHeight) + BORDERTHICKNESS)]
        += CHSV(ms / 53, 200U, 255U);
}
// void lightBallsRoutine() {
//     EffectSettings& s = effects_currentSettings();
    
//     // ТЕСТ: залить всё красным
//     matrix_fillAll(CRGB(64, 0, 0));
    
//     // И поставить 4 ярких белых точки по углам
//     matrix_drawPixel(1, 1, CRGB::White);
//     matrix_drawPixel(WIDTH-2, 1, CRGB::White);
//     matrix_drawPixel(1, HEIGHT-2, CRGB::White);
//     matrix_drawPixel(WIDTH-2, HEIGHT-2, CRGB::White);
// }
