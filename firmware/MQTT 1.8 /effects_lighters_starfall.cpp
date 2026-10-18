// ============================================================
//  Эффекты Светлячки и Звездопад
// ============================================================

#include <Arduino.h>
#include <FastLED.h>
#include "effects.h"
#include "matrix.h"
#include "config.h"

// ============================================================
//  СВЕТЛЯЧКИ
// ============================================================
#define LIGHTERS_AM 100

static int      lightersPos[2][LIGHTERS_AM];
static int8_t   lightersSpeed[2][LIGHTERS_AM];
static CHSV     lightersColor[LIGHTERS_AM];
static uint8_t  lightersLoopCounter = 0;

void lightersRoutine() {
    EffectSettings& s = effects_currentSettings();

    if (loadingFlag) {
        loadingFlag = false;
        randomSeed(millis());
        for (uint8_t i = 0; i < LIGHTERS_AM; i++) {
            lightersPos[0][i] = random(0, WIDTH * 10);
            lightersPos[1][i] = random(0, HEIGHT * 10);
            lightersSpeed[0][i] = random(-10, 10);
            lightersSpeed[1][i] = random(-10, 10);
            lightersColor[i] = CHSV(random(0, 255), 255, 255);
        }
    }
    matrix_clear();

    if (++lightersLoopCounter > 20) lightersLoopCounter = 0;

    uint8_t speedMul = map(s.speed, 0, 255, 1, 15);

    for (uint8_t i = 0; i < s.scale; i++) {
        if (lightersLoopCounter == 0) {
            lightersSpeed[0][i] += random(-3, 4);
            lightersSpeed[1][i] += random(-3, 4);
            lightersSpeed[0][i] = constrain(lightersSpeed[0][i], -20, 20);
            lightersSpeed[1][i] = constrain(lightersSpeed[1][i], -20, 20);
        }

        lightersPos[0][i] += lightersSpeed[0][i] * speedMul / 10;
        lightersPos[1][i] += lightersSpeed[1][i] * speedMul / 10;

        if (lightersPos[0][i] < 0) lightersPos[0][i] = (WIDTH - 1) * 10;
        if (lightersPos[0][i] >= WIDTH * 10) lightersPos[0][i] = 0;

        if (lightersPos[1][i] < 0) {
            lightersPos[1][i] = 0;
            lightersSpeed[1][i] = -lightersSpeed[1][i];
        }
        if (lightersPos[1][i] >= (HEIGHT - 1) * 10) {
            lightersPos[1][i] = (HEIGHT - 1) * 10;
            lightersSpeed[1][i] = -lightersSpeed[1][i];
        }

        matrix_drawPixel(lightersPos[0][i] / 10,
                         lightersPos[1][i] / 10,
                         lightersColor[i]);
    }
}

// ============================================================
//  ЗВЕЗДОПАД
// ============================================================
#define STAR_DENSE      60U    // плотность комет
#define STAR_TAIL_STEP 100U    // длина хвоста кометы
#define STAR_SATURATION 200U   // насыщенность

void starfallRoutine() {
    EffectSettings& s = effects_currentSettings();

    if (loadingFlag) {
        loadingFlag = false;
        matrix_clear();
    }

    static uint8_t skip = 0;
    uint8_t period = map(s.speed, 0, 255, 8, 1);
    if (++skip < period) return;
    skip = 0;

    for (uint8_t i = 0; i < WIDTH; i++) {
        if (matrix_getPixelXY(i, HEIGHT - 1) == 0U &&
            (random(0, map(s.scale, 0U, 255U, 10U, 120U)) == 0U) &&
            matrix_getPixelXY(i + 1, HEIGHT - 1) == 0U &&
            matrix_getPixelXY(i - 1, HEIGHT - 1) == 0U)
        {
            matrix_drawPixel(i, HEIGHT - 1,
                             CHSV(random(0, 200), STAR_SATURATION, 255U));
        }
    }

    for (uint8_t y = 0U; y < HEIGHT - 1U; y++) {
        for (uint8_t x = WIDTH - 1U; x > 0U; x--) {
            matrix_drawPixel(x, y, CRGB(matrix_getPixelXY(x - 1U, y + 1U)));
        }
        matrix_drawPixel(0, y, CRGB(matrix_getPixelXY(WIDTH - 1U, y + 1U)));
    }

    for (uint8_t i = 0U; i < WIDTH; i++) {
        matrix_fadePixel(i, HEIGHT - 1U, STAR_TAIL_STEP);
    }
}
