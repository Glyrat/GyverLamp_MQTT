// ============================================================
//  Эффекты на тему "огонь"
//  - Fire (Огонь)
//  - Warm Light (Теплый свет)
// ============================================================

#include <Arduino.h>
#include <FastLED.h>
#include "effects.h"
#include "matrix.h"
#include "config.h"

// ============================================================
//  ОБЩЕЕ ДЛЯ ЭФФЕКТА "ОГОНЬ"
// ============================================================
#define SPARKLES 1

static unsigned char line[WIDTH];
static uint8_t fire_pcnt = 0;

// Форма пламени (маска значений, вычитается из сгенерированного шума)
static const unsigned char valueMask[8][16] PROGMEM = {
    {32 , 0  , 0  , 0  , 0  , 0  , 0  , 32 , 32 , 0  , 0  , 0  , 0  , 0  , 0  , 32 },
    {64 , 0  , 0  , 0  , 0  , 0  , 0  , 64 , 64 , 0  , 0  , 0  , 0  , 0  , 0  , 64 },
    {96 , 32 , 0  , 0  , 0  , 0  , 32 , 96 , 96 , 32 , 0  , 0  , 0  , 0  , 32 , 96 },
    {128, 64 , 32 , 0  , 0  , 32 , 64 , 128, 128, 64 , 32 , 0  , 0  , 32 , 64 , 128},
    {160, 96 , 64 , 32 , 32 , 64 , 96 , 160, 160, 96 , 64 , 32 , 32 , 64 , 96 , 160},
    {192, 128, 96 , 64 , 64 , 96 , 128, 192, 192, 128, 96 , 64 , 64 , 96 , 128, 192},
    {255, 160, 128, 96 , 96 , 128, 160, 255, 255, 160, 128, 96 , 96 , 128, 160, 255},
    {255, 192, 160, 128, 128, 160, 192, 255, 255, 192, 160, 128, 128, 160, 192, 255}
};

static const unsigned char hueMask[8][16] PROGMEM = {
    {1 , 11, 19, 25, 25, 22, 11, 1 , 1 , 11, 19, 25, 25, 22, 11, 1 },
    {1 , 8 , 13, 19, 25, 19, 8 , 1 , 1 , 8 , 13, 19, 25, 19, 8 , 1 },
    {1 , 8 , 13, 16, 19, 16, 8 , 1 , 1 , 8 , 13, 16, 19, 16, 8 , 1 },
    {1 , 5 , 11, 13, 13, 13, 5 , 1 , 1 , 5 , 11, 13, 13, 13, 5 , 1 },
    {1 , 5 , 11, 11, 11, 11, 5 , 1 , 1 , 5 , 11, 11, 11, 11, 5 , 1 },
    {0 , 1 , 5 , 8 , 8 , 5 , 1 , 0 , 0 , 1 , 5 , 8 , 8 , 5 , 1 , 0 },
    {0 , 0 , 1 , 5 , 5 , 1 , 0 , 0 , 0 , 0 , 1 , 5 , 5 , 1 , 0 , 0 },
    {0 , 0 , 0 , 1 , 1 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 1 , 0 , 0 , 0 }
};


// Внешний буфер (matrixValue в оригинале — глобальный, используется ещё эффектом "огонь")
unsigned char matrixValue[WIDTH][HEIGHT];

// ============================================================
//  ВНУТРЕННИЕ ФУНКЦИИ ОГНЯ
// ============================================================
static void fire_generateLine() {
    for (uint8_t x = 0; x < WIDTH; x++) {
        line[x] = random(64, 255);
    }
}

static void fire_shiftUp() {
    for (uint8_t y = HEIGHT - 1; y > 0; y--) {
        for (uint8_t x = 0; x < WIDTH; x++) {
            uint8_t newX = x;
            if (x > 15) newX = x - 15;
            if (y > 7) continue;
            matrixValue[y][newX] = matrixValue[y - 1][newX];
        }
    }

    for (uint8_t x = 0; x < WIDTH; x++) {
        uint8_t newX = x;
        if (x > 15) newX = x - 15;
        matrixValue[0][newX] = line[newX];
    }
}

static void fire_drawFrame(int pcnt, uint8_t scale) {
    int nextv;

    for (unsigned char y = HEIGHT - 1; y > 0; y--) {
        for (unsigned char x = 0; x < WIDTH; x++) {
            uint8_t newX = x;
            if (x > 15) newX = x - 15;

            if (y < 8) {
                nextv =
                    (((100.0 - pcnt) * matrixValue[y][newX]
                      + pcnt * matrixValue[y - 1][newX]) / 100.0)
                    - pgm_read_byte(&(valueMask[y][newX]));

                CRGB color = CHSV(
                    scale * 2.5 + pgm_read_byte(&(hueMask[y][newX])),
                    255,
                    (uint8_t)max(0, nextv)
                );

                matrix_drawPixel(x, y, color);
            }
            else if (y == 8 && SPARKLES) {
                if (random(0, 20) == 0 && matrix_getPixelXY(x, y - 1) != 0)
                    matrix_drawPixel(x, y, CRGB(matrix_getPixelXY(x, y - 1)));
                else
                    matrix_drawPixel(x, y, CRGB(0,0,0));
            }
            else if (SPARKLES) {
                if (matrix_getPixelXY(x, y - 1) > 0)
                    matrix_drawPixel(x, y, CRGB(matrix_getPixelXY(x, y - 1)));
                else
                    matrix_drawPixel(x, y, CRGB(0,0,0));
            }
        }
    }

    for (unsigned char x = 0; x < WIDTH; x++) {
        uint8_t newX = x;
        if (x > 15) newX = x - 15;
        CRGB color = CHSV(
            scale * 2.5 + pgm_read_byte(&(hueMask[0][newX])),
            255,
            (uint8_t)(((100.0 - pcnt) * matrixValue[0][newX] + pcnt * line[newX]) / 100.0)
        );
        matrix_drawPixel(newX, 0, color);
    }
}

// ============================================================
//  ЭФФЕКТ "Огонь"
// ============================================================
void fireRoutine() {
    EffectSettings& s = effects_currentSettings();

    if (loadingFlag) {
        loadingFlag = false;
        fire_generateLine();
    }

    if (fire_pcnt >= 100) {
        fire_shiftUp();
        fire_generateLine();
        fire_pcnt = 0;
    }
    fire_drawFrame(fire_pcnt, s.scale);
    // было fire_pcnt += 30; → делаем зависящим от speed
    fire_pcnt += map(s.speed, 0, 255, 10, 60);
}

// ============================================================
//  Эффект "Теплый свет"
// ============================================================
void warmLightRoutine() {
    EffectSettings& s = effects_currentSettings();

    uint8_t centerY = max((uint8_t)round(HEIGHT / 2.0F) - 1, 0);
    uint8_t bottomOffset = (uint8_t)(!(HEIGHT & 1) && (HEIGHT > 1));
    if (s.scale < 17) s.scale = 17;

    for (int16_t y = centerY; y >= 0; y--) {
        CRGB color = CHSV(
            45U,
            map(s.speed, 0U, 255U, 0U, 170U),
            y == centerY
                ? 255U
                : (s.scale / 100.0F) > ((centerY + 1.0F) - (y + 1.0F)) / (centerY + 1.0F)
                    ? 255U : 0U);
        for (uint8_t x = 0U; x < WIDTH; x++) {
            matrix_drawPixel(x, y, color);
            matrix_drawPixel(
                x,
                max((uint8_t)(HEIGHT - 1U) - (y + 1U) + bottomOffset, 0U),
                color);
        }
    }
}

// ============================================================
//  КОНФЕТТИ
// ============================================================
void sparklesRoutine() {
    EffectSettings& s = effects_currentSettings();

    //for (byte i = 0; i < s.scale; i++) {
    uint8_t spawn = (s.scale / 4) + 1;   // базовое количество
    
    for (byte i = 0; i < spawn; i++) {
        byte x = random(0, WIDTH);
        byte y = random(0, HEIGHT);
        if (matrix_getPixelXY(x, y) == 0)
            leds[matrix_pixelNumber(x, y)] = CHSV(random(0, 255), 255, 255);
    }
    uint8_t fade = map(s.speed, 0, 255, 30, 100);
    matrix_fader(fade);
}
