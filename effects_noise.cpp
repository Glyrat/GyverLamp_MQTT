// ============================================================
//  NOISE ЭФФЕКТЫ
//  Перенос из noiseEffects.ino
// ============================================================

#include "effects_noise.h"

// ============================================================
//  СОСТОЯНИЕ ДВИЖКА
// ============================================================
static uint16_t x = 0;
static uint16_t y = 0;
static uint16_t z = 0;

static uint16_t noise_speed = 20;
static uint16_t noise_scale = 30;

// Буфер шума (максимум из WIDTH/HEIGHT по обеим осям)
#if (WIDTH > HEIGHT)
  static uint8_t noiseBuf[WIDTH][WIDTH];
  #define NOISE_DIM WIDTH
#else
  static uint8_t noiseBuf[HEIGHT][HEIGHT];
  #define NOISE_DIM HEIGHT
#endif

static CRGBPalette16 noisePalette(PartyColors_p);
static uint8_t colorLoop = 1;
static uint8_t ihue = 0;

// ============================================================
//  ДВИЖОК
// ============================================================
void noise_fillNoiseLED() {
    uint8_t dataSmoothing = 0;
    if (noise_speed < 50) {
        dataSmoothing = 200 - (noise_speed * 4);
    }

    for (int i = 0; i < NOISE_DIM; i++) {
        int ioffset = noise_scale * i;
        for (int j = 0; j < NOISE_DIM; j++) {
            int joffset = noise_scale * j;

            uint8_t data = inoise8(x + ioffset, y + joffset, z);
            data = qsub8(data, 16);
            data = qadd8(data, scale8(data, 39));

            if (dataSmoothing) {
                uint8_t olddata = noiseBuf[i][j];
                uint8_t newdata = scale8(olddata, dataSmoothing) +
                                  scale8(data, 256 - dataSmoothing);
                data = newdata;
            }
            noiseBuf[i][j] = data;
        }
    }
    z += noise_speed;

    // медленный дрейф по X/Y для визуального разнообразия
    x += noise_speed / 8;
    y -= noise_speed / 16;

    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < HEIGHT; j++) {
            uint8_t index = noiseBuf[j][i];
            uint8_t bri   = noiseBuf[i][j];

            if (colorLoop) {
                index += ihue;
            }

            if (bri > 127) {
                bri = 255;
            } else {
                bri = dim8_raw(bri * 2);
            }
            CRGB color = ColorFromPalette(noisePalette, index, bri);
            matrix_drawPixel(i, j, color);
        }
    }
    ihue += 1;
}

void noise_fillnoise8() {
    for (int i = 0; i < NOISE_DIM; i++) {
        int ioffset = noise_scale * i;
        for (int j = 0; j < NOISE_DIM; j++) {
            int joffset = noise_scale * j;
            noiseBuf[i][j] = inoise8(x + ioffset, y + joffset, z);
        }
    }
    z += noise_speed;
}

// ============================================================
//  МАКРОСЫ ЗАГРУЗКИ ДЛЯ КАЖДОГО ЭФФЕКТА
// ============================================================
#define NOISE_LOAD_INIT(PALETTE, LOOP)                     \
    do {                                                   \
        if (loadingFlag) {                                 \
            loadingFlag = false;                           \
            noisePalette = PALETTE;                        \
            noise_scale  = s.scale;                        \
            noise_speed  = s.speed;                        \
            colorLoop    = LOOP;                           \
        }                                                  \
    } while (0)

// ============================================================
//  ЭФФЕКТЫ
// ============================================================
void madnessNoise() {
    EffectSettings& s = effects_currentSettings();
    if (loadingFlag) {
        loadingFlag = false;
        noise_scale = s.scale;
        noise_speed = s.speed;
    }
    noise_fillnoise8();
    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < HEIGHT; j++) {
            CRGB thisColor = CHSV(noiseBuf[j][i], 255, noiseBuf[i][j]);
            matrix_drawPixel(i, j, thisColor);
        }
    }
    ihue += 1;
}

void rainbowNoise() {
    EffectSettings& s = effects_currentSettings();
    NOISE_LOAD_INIT(RainbowColors_p, 1);
    noise_fillNoiseLED();
}

void rainbowStripeNoise() {
    EffectSettings& s = effects_currentSettings();
    NOISE_LOAD_INIT(RainbowStripeColors_p, 1);
    noise_fillNoiseLED();
}

void zebraNoise() {
    EffectSettings& s = effects_currentSettings();
    if (loadingFlag) {
        loadingFlag = false;
        // "обнуляем" все 16 слотов палитры и зажигаем каждый четвёртый белым
        fill_solid(noisePalette, 16, CRGB::Black);
        noisePalette[0]  = CRGB::White;
        noisePalette[4]  = CRGB::White;
        noisePalette[8]  = CRGB::White;
        noisePalette[12] = CRGB::White;
        noise_scale = s.scale;
        noise_speed = s.speed;
        colorLoop   = 1;
    }
    noise_fillNoiseLED();
}

void forestNoise() {
    EffectSettings& s = effects_currentSettings();
    NOISE_LOAD_INIT(ForestColors_p, 0);
    noise_fillNoiseLED();
}

void oceanNoise() {
    EffectSettings& s = effects_currentSettings();
    NOISE_LOAD_INIT(OceanColors_p, 0);
    noise_fillNoiseLED();
}

void plasmaNoise() {
    EffectSettings& s = effects_currentSettings();
    NOISE_LOAD_INIT(PartyColors_p, 1);
    noise_fillNoiseLED();
}

void cloudNoise() {
    EffectSettings& s = effects_currentSettings();
    NOISE_LOAD_INIT(CloudColors_p, 0);
    noise_fillNoiseLED();
}

void lavaNoise() {
    EffectSettings& s = effects_currentSettings();
    NOISE_LOAD_INIT(LavaColors_p, 0);
    noise_fillNoiseLED();
}