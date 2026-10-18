// ============================================================
//  Эффекты Дождь и Гроза
//  Перенос из aquarium2.ino (функция rain) — светлая часть
// ============================================================

#include "effects_rain.h"
#include "config.h"

// ============================================================
//  ГЛОБАЛЬНЫЕ ДЛЯ ДОЖДЯ
// ============================================================
static CRGB solidRainColor = CRGB(60, 80, 90);

// Двухслойная маска для капель (использует matrixValue? нет — свою)
static uint8_t rainMask[WIDTH][HEIGHT];

// Служебные счётчики
static uint16_t ff_x = 0, ff_y = 0, ff_z = 0;

// ============================================================
//  ХЕЛПЕРЫ
// ============================================================
static inline uint8_t wrapX(int8_t x) {
    return (x + WIDTH) % WIDTH;
}
static inline uint8_t wrapY(int8_t y) {
    return (y + HEIGHT) % HEIGHT;
}

// ============================================================
//  ОСНОВНАЯ ФУНКЦИЯ
// ============================================================
static void rain(uint8_t backgroundDepth,
                 uint8_t maxBrightness,
                 uint8_t spawnFreq,
                 uint8_t tailLength,
                 CRGB rainColor,
                 bool splashes,
                 bool clouds,
                 bool storm)
{
    ff_x = random16();
    ff_y = random16();
    ff_z = random16();

    CRGB lightningColor = CRGB(72, 72, 80);
    CRGBPalette16 rain_p(CRGB::Black, rainColor);
    CRGBPalette16 rainClouds_p(CRGB::Black,
                               CRGB(15, 24, 24),
                               CRGB(9, 15, 15),
                               CRGB::Black);

    matrix_dimAll(tailLength);

    // ---------- шаги по каждой колонке ----------
    for (uint8_t x = 0; x < WIDTH; x++) {

        // шаг 1: сдвигаем капли вниз
        for (uint8_t i = 0; i < HEIGHT; i++) {
            if (rainMask[x][i] >= backgroundDepth) {
                if (i > 0) rainMask[x][wrapY(i - 1)] = rainMask[x][i];
                rainMask[x][i] = 0;
            }
        }

        // шаг 2: спавним новые капли сверху
        if (random8() < spawnFreq) {
            rainMask[x][HEIGHT - 1] = random(backgroundDepth, maxBrightness);
        }

        // шаг 3: рисуем капли
        for (uint8_t y = 0; y < HEIGHT; y++) {
            if (rainMask[x][y] >= backgroundDepth) {
                leds[matrix_pixelNumber(x, y)] = ColorFromPalette(rain_p, rainMask[x][y]);
            }
        }

        // шаг 4: брызги (не трогаем — в оригинале "сломано")
        // шаг 5: молнии
        if (storm) {
            uint8_t* lightning = (uint8_t*)malloc(WIDTH * HEIGHT);
            if (lightning == nullptr) return;
            memset(lightning, 0, WIDTH * HEIGHT);

            if (random16() < 72) {
                lightning[scale8(random8(), WIDTH - 1) + (HEIGHT - 1) * WIDTH] = 255;

                for (uint8_t ly = HEIGHT - 1; ly > 1; ly--) {
                    for (uint8_t lx = 1; lx < WIDTH - 1; lx++) {
                        if (lightning[lx + ly * WIDTH] == 255) {
                            lightning[lx + ly * WIDTH] = 0;
                            uint8_t dir = random8(4);
                            switch (dir) {
                                case 0:
                                    leds[matrix_pixelNumber(lx + 1, ly - 1)] = lightningColor;
                                    lightning[(lx + 1) + (ly - 1) * WIDTH] = 255;
                                    break;
                                case 1:
                                    leds[matrix_pixelNumber(lx, ly - 1)] = CRGB(128, 128, 128);
                                    lightning[lx + (ly - 1) * WIDTH] = 255;
                                    break;
                                case 2:
                                    leds[matrix_pixelNumber(lx - 1, ly - 1)] = CRGB(128, 128, 128);
                                    lightning[(lx - 1) + (ly - 1) * WIDTH] = 255;
                                    break;
                                case 3:
                                    leds[matrix_pixelNumber(lx - 1, ly - 1)] = CRGB(128, 128, 128);
                                    lightning[(lx - 1) + (ly - 1) * WIDTH] = 255;
                                    leds[matrix_pixelNumber(lx + 1, ly - 1)] = CRGB(128, 128, 128);
                                    lightning[(lx + 1) + (ly - 1) * WIDTH] = 255;
                                    break;
                            }
                        }
                    }
                }
            }
            free(lightning);
        }

        // шаг 6: облака
        if (clouds) {
            uint16_t noiseScale = 250;
            const uint8_t cloudHeight = HEIGHT * 0.4 + 1;

            static uint8_t* noise = nullptr;
            if (noise == nullptr) {
                noise = (uint8_t*)malloc(WIDTH * cloudHeight);
                if (noise == nullptr) return;
                memset(noise, 0, WIDTH * cloudHeight);
            }

            int xoffset = noiseScale * x + hue;

            for (uint8_t z = 0; z < cloudHeight; z++) {
                int yoffset = noiseScale * z - hue;
                uint8_t dataSmoothing = 192;
                uint8_t noiseData = qsub8(inoise8(ff_x + xoffset,
                                                 ff_y + yoffset,
                                                 ff_z), 16);
                noiseData = qadd8(noiseData, scale8(noiseData, 39));
                noise[x * cloudHeight + z] =
                    scale8(noise[x * cloudHeight + z], dataSmoothing) +
                    scale8(noiseData, 256 - dataSmoothing);

                nblend(leds[matrix_pixelNumber(x, HEIGHT - z - 1)],
                       ColorFromPalette(rainClouds_p, noise[x * cloudHeight + z]),
                       (cloudHeight - z) * (250 / cloudHeight));
            }
            ff_z++;
        }
    }
}

// ============================================================
//  ПУБЛИЧНЫЕ ФУНКЦИИ
// ============================================================
void simpleRain() {
    EffectSettings& s = effects_currentSettings();
    if (s.scale > 100U) s.scale = 100U;

    if (loadingFlag) {
        loadingFlag = false;
        matrix_clear();
        memset(rainMask, 0, sizeof(rainMask));
    }

    rain(60, 180,
         (s.scale - 1) * 2.58,
         30,
         solidRainColor,
         true, true, false);
}

void stormyRain() {
    EffectSettings& s = effects_currentSettings();
    if (s.scale > 100U) s.scale = 100U;

    if (loadingFlag) {
        loadingFlag = false;
        matrix_clear();
        memset(rainMask, 0, sizeof(rainMask));
    }

    rain(60, 160,
         (s.scale - 1) * 2.58,
         30,
         solidRainColor,
         true, true, true);
}