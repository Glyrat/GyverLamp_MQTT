// ============================================================
//  Мотыльки в банке
//  Перенос из butterflys.ino
// ============================================================

#include "effects_butterflys.h"
#include "config.h"

#define BUTTERFLY_MAX_COUNT   70U
#define BUTTERFLY_FIX_COUNT   20U

static float   butterflysPosX[BUTTERFLY_MAX_COUNT];
static float   butterflysPosY[BUTTERFLY_MAX_COUNT];
static float   butterflysSpeedX[BUTTERFLY_MAX_COUNT];
static float   butterflysSpeedY[BUTTERFLY_MAX_COUNT];
static float   butterflysTurn[BUTTERFLY_MAX_COUNT];
static uint8_t butterflysColor[BUTTERFLY_MAX_COUNT];
static uint8_t butterflysBrightness[BUTTERFLY_MAX_COUNT];

// ============================================================
//  ОСНОВНАЯ ФУНКЦИЯ
// ============================================================
void butterflysRoutine(bool isColored) {
    EffectSettings& s = effects_currentSettings();

    bool isWings = s.speed & 0x01;

    if (loadingFlag) {
        loadingFlag = false;
        randomSeed(millis());

        uint8_t count = isColored
            ? (s.scale > BUTTERFLY_MAX_COUNT ? BUTTERFLY_MAX_COUNT : s.scale)
            : BUTTERFLY_FIX_COUNT;

        for (uint8_t i = 0; i < BUTTERFLY_MAX_COUNT; i++) {
            butterflysPosX[i] = random8(WIDTH);
            butterflysPosY[i] = random8(HEIGHT);
            butterflysSpeedX[i] = 0;
            butterflysSpeedY[i] = 0;
            butterflysTurn[i] = 0;
            butterflysColor[i] = isColored ? random8() : 255U;
            butterflysBrightness[i] = 255U;
        }

        if (s.scale != 1U) {
            hue = (float)(s.scale - 1U) * 2.6;
        }
        hue2 = (s.scale == 100U) ? 0U : 255U;

        deltaValue = count;
    }

    if (isWings && isColored) matrix_dimAll(35U);
    else                      matrix_clear();

    float speedfactor = (float)s.speed / 2048.0f + 0.001f;

    if (++step >= deltaValue) step = 0U;

    for (uint8_t i = 0U; i < deltaValue; i++) {
        butterflysPosX[i] += butterflysSpeedX[i] * speedfactor;
        butterflysPosY[i] += butterflysSpeedY[i] * speedfactor;

        if (butterflysPosX[i] < 0)
            butterflysPosX[i] = (float)(WIDTH - 1) + butterflysPosX[i];
        if (butterflysPosX[i] > WIDTH - 1)
            butterflysPosX[i] = butterflysPosX[i] + 1 - WIDTH;

        if (butterflysPosY[i] < 0) {
            butterflysPosY[i] = -butterflysPosY[i];
            butterflysSpeedY[i] = -butterflysSpeedY[i];
        }
        if (butterflysPosY[i] > HEIGHT - 1U) {
            butterflysPosY[i] = (HEIGHT << 1U) - 2U - butterflysPosY[i];
            butterflysSpeedY[i] = -butterflysSpeedY[i];
        }

        // проворот траектории
        float maxspeed = fabs(butterflysSpeedX[i]) + fabs(butterflysSpeedY[i]);

        if (maxspeed == fabs(butterflysSpeedX[i] + butterflysSpeedY[i])) {
            if (butterflysSpeedX[i] > 0) {
                butterflysSpeedX[i] += butterflysTurn[i];
                if (butterflysSpeedX[i] > maxspeed) {
                    butterflysSpeedX[i] = maxspeed + maxspeed - butterflysSpeedX[i];
                    butterflysSpeedY[i] = butterflysSpeedX[i] - maxspeed;
                } else {
                    butterflysSpeedY[i] = maxspeed - fabs(butterflysSpeedX[i]);
                }
            } else {
                butterflysSpeedX[i] -= butterflysTurn[i];
                if (butterflysSpeedX[i] + maxspeed < 0) {
                    butterflysSpeedX[i] = 0 - butterflysSpeedX[i] - maxspeed - maxspeed;
                    butterflysSpeedY[i] = maxspeed - fabs(butterflysSpeedX[i]);
                } else {
                    butterflysSpeedY[i] = fabs(butterflysSpeedX[i]) - maxspeed;
                }
            }
        } else {
            if (butterflysSpeedX[i] > 0) {
                butterflysSpeedX[i] -= butterflysTurn[i];
                if (butterflysSpeedX[i] > maxspeed) {
                    butterflysSpeedX[i] = maxspeed + maxspeed - butterflysSpeedX[i];
                    butterflysSpeedY[i] = maxspeed - butterflysSpeedX[i];
                } else {
                    butterflysSpeedY[i] = fabs(butterflysSpeedX[i]) - maxspeed;
                }
            } else {
                butterflysSpeedX[i] += butterflysTurn[i];
                if (butterflysSpeedX[i] + maxspeed < 0) {
                    butterflysSpeedX[i] = 0 - butterflysSpeedX[i] - maxspeed - maxspeed;
                    butterflysSpeedY[i] = 0 - butterflysSpeedX[i] - maxspeed;
                } else {
                    butterflysSpeedY[i] = maxspeed - fabs(butterflysSpeedX[i]);
                }
            }
        }

        if (butterflysBrightness[i] == 255U) {
            if (step == i && random8(2U) == 0U) {
                butterflysBrightness[i] = random8(220U, 244U);
                butterflysSpeedX[i] = (float)random8(101U) / 20.0f + 1.0f;
                if (random8(2U) == 0U) butterflysSpeedX[i] = -butterflysSpeedX[i];
                butterflysSpeedY[i] = (float)random8(101U) / 20.0f + 1.0f;
                if (random8(2U) == 0U) butterflysSpeedY[i] = -butterflysSpeedY[i];

                butterflysTurn[i] =
                    (float)random8((fabs(butterflysSpeedX[i]) +
                                    fabs(butterflysSpeedY[i])) * 20.0f + 2.0f) / 200.0f;
                if (random8(2U) == 0U) butterflysTurn[i] = -butterflysTurn[i];
            }
        } else {
            if (step == i) butterflysBrightness[i]++;

            uint8_t tmp = 255U - butterflysBrightness[i];
            if (tmp == 0U ||
                ((uint16_t)(butterflysPosX[i] * tmp) % tmp == 0U &&
                 (uint16_t)(butterflysPosY[i] * tmp) % tmp == 0U)) {
                butterflysPosX[i] = round(butterflysPosX[i]);
                butterflysPosY[i] = round(butterflysPosY[i]);
                butterflysSpeedX[i] = 0;
                butterflysSpeedY[i] = 0;
                butterflysTurn[i] = 0;
                butterflysBrightness[i] = 255U;
            }
        }

        if (isWings)
            matrix_drawPixelXYF(butterflysPosX[i], butterflysPosY[i],
                CHSV(butterflysColor[i], 255U,
                     (butterflysBrightness[i] == 255U) ? 255U
                                                       : 128U + random8(2U) * 111U));
        else
            matrix_drawPixelXYF(butterflysPosX[i], butterflysPosY[i],
                CHSV(butterflysColor[i], 255U, butterflysBrightness[i]));
    }

    // постобработка кадра
    if (isColored) {
        for (uint8_t i = 0U; i < deltaValue; i++)
            if (butterflysBrightness[i] == 255U)
                matrix_drawPixel(butterflysPosX[i], butterflysPosY[i],
                    CHSV(butterflysColor[i], 255U, butterflysBrightness[i]));
    } else {
        if (s.scale == 1U)
            if (++deltaHue == 0U) hue++;

        for (uint16_t i = 0U; i < NUM_LEDS; i++)
            leds[i] = CHSV(hue, hue2, 255U - leds[i].r);
    }
}