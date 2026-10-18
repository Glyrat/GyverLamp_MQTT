// ============================================================
//  Остальные эффекты:
//  pride / twinkles / drift / pendulum / cube2d / nexus
// ============================================================

#include "effects_misc.h"
#include "config.h"

static uint8_t beatcos8(accum88 beats_per_minute, uint8_t lowest = 0,
                        uint8_t highest = 255, uint32_t timebase = 0,
                        uint8_t phase_offset = 0)
{
    uint8_t beat = beat8(beats_per_minute, timebase);
    uint8_t beatcos = cos8(beat + phase_offset);
    uint8_t rangewidth = highest - lowest;
    uint8_t scaledbeat = scale8(beatcos, rangewidth);
    return lowest + scaledbeat;
}

// ============================================================
//  СТАЯ (pride)
// ============================================================
void prideRoutine() {
    EffectSettings& s = effects_currentSettings();

    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    uint8_t  sat8 = beatsin88(87, 220, 250);
    uint8_t  brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, 25 * 256, 40 * 256);

    uint8_t lo   = map(s.speed, 0, 255, 10, 60);
    uint8_t hi   = map(s.speed, 0, 255, 30, 150);
    uint8_t freq = map(s.speed, 0, 255, 80, 300);
    uint8_t msmultiplier = beatsin88(freq, lo, hi);

    uint16_t hue16 = sHue16;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for (int i = 0; i < NUM_LEDS; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t  bri8  = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV(hue8, sat8, bri8);

        uint8_t pixelnumber = i;
        pixelnumber = (NUM_LEDS - 1) - pixelnumber;

        nblend(leds[pixelnumber], newcolor, 64);
    }
}
// ============================================================
//  МЕРЦАНИЕ (twinkles)
// ============================================================
#define TWINKLES_SPEEDS      4
//uint8_t multiplier = map(s.speed, 0, 255, 1, 20);
//#define TWINKLES_MULTIPLIER  6

static CRGB twinkleBuf[NUM_LEDS];

void twinklesRoutine() {
    EffectSettings& s = effects_currentSettings();

    uint8_t multiplier = map(s.speed, 0, 255, 1, 20);

    if (loadingFlag) {
        loadingFlag = false;
        effects_setCurrentPalette();
        hue = 0U;
        for (uint16_t idx = 0; idx < NUM_LEDS; idx++) {
            if (random8(s.scale % 11U) == 0) {
                twinkleBuf[idx].r = random8();
                twinkleBuf[idx].g = random8(1, TWINKLES_SPEEDS * 2 + 1);
                twinkleBuf[idx].b = random8();
            } else {
                twinkleBuf[idx] = CRGB(0, 0, 0);
            }
        }
    }

    for (uint16_t idx = 0; idx < NUM_LEDS; idx++) {
        if (twinkleBuf[idx].b == 0) {
            if (random8(s.scale % 11U) == 0 && hue > 0) {
                twinkleBuf[idx].r = random8();
                twinkleBuf[idx].g = random8(1, multiplier + 1);
                twinkleBuf[idx].b = twinkleBuf[idx].g;
                hue--;
            }
        } else if (twinkleBuf[idx].g <= TWINKLES_SPEEDS) {
            if (twinkleBuf[idx].b > 255U - twinkleBuf[idx].g - multiplier) {
                twinkleBuf[idx].b = 255U;
                twinkleBuf[idx].g = twinkleBuf[idx].g + TWINKLES_SPEEDS;
            } else {
                twinkleBuf[idx].b = twinkleBuf[idx].b + twinkleBuf[idx].g + multiplier;
            }
        } else {
            if (twinkleBuf[idx].b <= twinkleBuf[idx].g - TWINKLES_SPEEDS + multiplier) {
                twinkleBuf[idx].b = 0;
                hue++;
            } else {
                twinkleBuf[idx].b = twinkleBuf[idx].b - twinkleBuf[idx].g + TWINKLES_SPEEDS - multiplier;
            }
        }
        if (twinkleBuf[idx].b == 0) {
            leds[idx] = CRGB(0, 0, 0);
        } else {
            leds[idx] = ColorFromPalette(*curPalette, twinkleBuf[idx].r, twinkleBuf[idx].b);
        }
    }
}

// ============================================================
//  ДРЕЙФ (PatternIncrementalDrift2)
// ============================================================
void PatternIncrementalDrift2() {
    EffectSettings& s = effects_currentSettings();

    //const int CENTER_X = WIDTH / 2;

    uint8_t freqMul = map(s.speed, 0, 255, 1, 6);   // было 2
    const int CENTER_X = WIDTH / 2;

    if (s.scale < 20)       cPalette = RainbowColors_p;
    else if (s.scale < 40)  cPalette = PartyColors_p;
    else if (s.scale < 60)  cPalette = CloudColors_p;
    else if (s.scale < 80)  cPalette = LavaColors_p;
    else                    cPalette = ForestColors_p;

    uint8_t dim = beatsin8(2, 170, 250);
    matrix_dimAll(dim);

    for (uint8_t i = 0; i < WIDTH; i++) {
        CRGB color;
        uint8_t x = 0;
        uint8_t y = 0;

        if (i < CENTER_X) {
            x = beatcos8((i + 1) * freqMul, i, WIDTH - i);
            y = beatsin8((i + 1) * freqMul, i, HEIGHT - i);
            color = ColorFromPalette(cPalette, i * 14);
        } else {
            x = beatsin8((WIDTH - i) * freqMul, WIDTH - i, i + 1);
            y = beatcos8((HEIGHT - i) * freqMul, HEIGHT - i, i + 1);
            color = ColorFromPalette(cPalette, (31 - i) * 14);
        }
        matrix_drawPixel(x, y, color);
    }
}

// ============================================================
//  МАЯТНИК (prismata)
// ============================================================
void prismataRoutine() {
    EffectSettings& s = effects_currentSettings();
    if (s.scale > 100) s.scale = 100;

    EVERY_N_MILLIS(33) {
        hue++;
    }

    matrix_clear();

    if (s.scale < 20)       cPalette = RainbowColors_p;
    else if (s.scale < 40)  cPalette = PartyColors_p;
    else if (s.scale < 60)  cPalette = CloudColors_p;
    else if (s.scale < 80)  cPalette = LavaColors_p;
    else                    cPalette = ForestColors_p;

    for (int x = 0; x < WIDTH; x++) {
        uint8_t beat = (millis() * (accum88(x + 1)) * 28 * s.speed) >> 17;
        uint8_t y = scale8(sin8(beat), HEIGHT - 1);
        matrix_drawPixel(x, y, ColorFromPalette(cPalette, x * 7 + hue));
    }
}

// ============================================================
//  КУБИК РУБИКА (cube2d)
// ============================================================
#define CUBE_PAUSE_MAX 7

static uint8_t cube_razmerX, cube_razmerY;
static uint8_t cube_shtukX, cube_shtukY;
static uint8_t cube_poleX, cube_poleY;
static int8_t  cube_globalShiftX, cube_globalShiftY;
static bool    cube_seamlessX;
static bool    cube_krutimVertikalno;

void cube2dRoutine() {
    uint8_t x, y;
    uint8_t anim0;
    int8_t shift, kudaVse;
    CRGB color, color2;

    EffectSettings& s = effects_currentSettings();

    if (loadingFlag) {
        loadingFlag = false;
        effects_setCurrentPalette();
        matrix_clear();

        cube_razmerX = (s.scale - 1U) % 11U + 1U;
        cube_razmerY = cube_razmerX;
        if (s.speed & 0x01) cube_razmerY = (cube_razmerY << 1U) + 1U;

        cube_shtukY = HEIGHT / (cube_razmerY + 1U);
        if (cube_shtukY < 2U) cube_shtukY = 2U;
        y = HEIGHT / cube_shtukY - 1U;
        if (cube_razmerY > y) cube_razmerY = y;
        cube_poleY = (cube_razmerY + 1U) * cube_shtukY;

        cube_shtukX = WIDTH / (cube_razmerX + 1U);
        if (cube_shtukX < 2U) cube_shtukX = 2U;
        x = WIDTH / cube_shtukX - 1U;
        if (cube_razmerX > x) cube_razmerX = x;
        cube_poleX = (cube_razmerX + 1U) * cube_shtukX;

        cube_seamlessX = (cube_poleX == WIDTH);
        deltaHue  = 0U;
        deltaHue2 = 0U;
        cube_globalShiftX = 0;
        cube_globalShiftY = 0;

        for (uint8_t j = 0U; j < cube_shtukY; j++) {
            y = j * (cube_razmerY + 1U);
            for (uint8_t i = 0U; i < cube_shtukX; i++) {
                x = i * (cube_razmerX + 1U);
                if (s.scale == 100U)
                    color = CHSV(45U, 0U, 128U + random8(128U));
                else
                    color = ColorFromPalette(*curPalette, random8());

                for (uint8_t k = 0U; k < cube_razmerY; k++)
                    for (uint8_t m = 0U; m < cube_razmerX; m++)
                        leds[matrix_pixelNumber(x + m, y + k)] = color;
            }
        }
        step = 4U;
        deltaValue = 4U;
        hue2 = 0U;
    }

    if (hue2 == 0 && step < deltaValue) {
        step++;
        if (cube_krutimVertikalno) {
            for (uint8_t i = 0U; i < cube_shtukX; i++) {
                x = (deltaHue + i * (cube_razmerX + 1U)) % WIDTH;
                if (noise3d[0][i][0] > 0) {
                    noise3d[0][i][0]--;
                    shift = noise3d[0][i][1] - 1;

                    if (cube_globalShiftY == 0)
                        anim0 = (deltaHue2 == 0U) ? 0U : deltaHue2 - 1U;
                    else if (cube_globalShiftY > 0)
                        anim0 = deltaHue2;
                    else
                        anim0 = deltaHue2 - 1U;

                    if (shift < 0) {
                        color = leds[matrix_pixelNumber(x, anim0)];
                        for (uint8_t k = anim0; k < anim0 + cube_poleY - 1; k++) {
                            color2 = leds[matrix_pixelNumber(x, k + 1)];
                            for (uint8_t m = x; m < x + cube_razmerX; m++)
                                leds[matrix_pixelNumber(m % WIDTH, k)] = color2;
                        }
                        for (uint8_t m = x; m < x + cube_razmerX; m++)
                            leds[matrix_pixelNumber(m % WIDTH, anim0 + cube_poleY - 1)] = color;
                    } else if (shift > 0) {
                        color = leds[matrix_pixelNumber(x, anim0 + cube_poleY - 1)];
                        for (uint8_t k = anim0 + cube_poleY - 1; k > anim0; k--) {
                            color2 = leds[matrix_pixelNumber(x, k - 1)];
                            for (uint8_t m = x; m < x + cube_razmerX; m++)
                                leds[matrix_pixelNumber(m % WIDTH, k)] = color2;
                        }
                        for (uint8_t m = x; m < x + cube_razmerX; m++)
                            leds[matrix_pixelNumber(m % WIDTH, anim0)] = color;
                    }
                }
            }
        } else {
            for (uint8_t j = 0U; j < cube_shtukY; j++) {
                y = deltaHue2 + j * (cube_razmerY + 1U);
                if (noise3d[0][0][j] > 0) {
                    noise3d[0][0][j]--;
                    shift = noise3d[0][1][j] - 1;

                    if (cube_seamlessX)
                        anim0 = 0U;
                    else if (cube_globalShiftX == 0)
                        anim0 = (deltaHue == 0U) ? 0U : deltaHue - 1U;
                    else if (cube_globalShiftX > 0)
                        anim0 = deltaHue;
                    else
                        anim0 = deltaHue - 1U;

                    if (shift < 0) {
                        color = leds[matrix_pixelNumber(anim0, y)];
                        for (uint8_t k = anim0; k < anim0 + cube_poleX - 1; k++) {
                            color2 = leds[matrix_pixelNumber(k + 1, y)];
                            for (uint8_t m = y; m < y + cube_razmerY; m++)
                                leds[matrix_pixelNumber(k, m)] = color2;
                        }
                        for (uint8_t m = y; m < y + cube_razmerY; m++)
                            leds[matrix_pixelNumber(anim0 + cube_poleX - 1, m)] = color;
                    } else if (shift > 0) {
                        color = leds[matrix_pixelNumber(anim0 + cube_poleX - 1, y)];
                        for (uint8_t k = anim0 + cube_poleX - 1; k > anim0; k--) {
                            color2 = leds[matrix_pixelNumber(k - 1, y)];
                            for (uint8_t m = y; m < y + cube_razmerY; m++)
                                leds[matrix_pixelNumber(k, m)] = color2;
                        }
                        for (uint8_t m = y; m < y + cube_razmerY; m++)
                            leds[matrix_pixelNumber(anim0, m)] = color;
                    }
                }
            }
        }
    }
    else if (hue2 != 0U) {
        hue2--;
    }

    if (step >= deltaValue) {
        step = 0U;
        hue2 = CUBE_PAUSE_MAX;

        deltaHue2 = deltaHue2 + cube_globalShiftY;
        cube_globalShiftY = 0;
        deltaHue = (WIDTH + deltaHue + cube_globalShiftX) % WIDTH;
        cube_globalShiftX = 0;

        kudaVse = 0;
        cube_krutimVertikalno = random8(2U);

        if (cube_krutimVertikalno) {
            for (uint8_t i = 0U; i < cube_shtukX; i++) {
                noise3d[0][i][1] = random8(3);
                shift = noise3d[0][i][1] - 1;
                if (kudaVse == 0) kudaVse = shift;
                else if (shift != 0 && kudaVse != shift) kudaVse = 50;
            }
            deltaValue = cube_razmerY +
                ((deltaHue2 - kudaVse >= 0 && deltaHue2 - kudaVse + cube_poleY < (int)HEIGHT)
                    ? random8(2U) : 1U);

            if (deltaValue == cube_razmerY) {
                cube_globalShiftY = 1 - kudaVse;
                for (uint8_t i = 0U; i < cube_shtukX; i++)
                    if (noise3d[0][i][1] == 1U) {
                        noise3d[0][i][1] = cube_globalShiftY;
                        noise3d[0][i][0] = 1U;
                    } else {
                        noise3d[0][i][0] = deltaValue;
                    }
                cube_globalShiftY--;
            } else {
                x = 0;
                for (uint8_t i = 0U; i < cube_shtukX; i++)
                    if (noise3d[0][i][1] != 1U) {
                        y = random8(cube_shtukY);
                        if (y > x) x = y;
                        noise3d[0][i][0] = deltaValue * (x + 1U);
                    }
                deltaValue = deltaValue * (x + 1U);
            }
        } else {
            for (uint8_t j = 0U; j < cube_shtukY; j++) {
                noise3d[0][1][j] = random8(3);
                shift = noise3d[0][1][j] - 1;
                if (kudaVse == 0) kudaVse = shift;
                else if (shift != 0 && kudaVse != shift) kudaVse = 50;
            }
            if (cube_seamlessX)
                deltaValue = cube_razmerX + ((kudaVse < 50) ? random8(2U) : 1U);
            else
                deltaValue = cube_razmerX +
                    ((deltaHue - kudaVse >= 0 && deltaHue - kudaVse + cube_poleX < (int)WIDTH)
                        ? random8(2U) : 1U);

            if (deltaValue == cube_razmerX) {
                cube_globalShiftX = 1 - kudaVse;
                for (uint8_t j = 0U; j < cube_shtukY; j++)
                    if (noise3d[0][1][j] == 1U) {
                        noise3d[0][1][j] = cube_globalShiftX;
                        noise3d[0][0][j] = 1U;
                    } else {
                        noise3d[0][0][j] = deltaValue;
                    }
                cube_globalShiftX--;
            } else {
                y = 0;
                for (uint8_t j = 0U; j < cube_shtukY; j++)
                    if (noise3d[0][1][j] != 1U) {
                        x = random8(cube_shtukX);
                        if (x > y) y = x;
                        noise3d[0][0][j] = deltaValue * (y + 1U);
                    }
                deltaValue = deltaValue * (y + 1U);
            }
        }
    }
}

// ============================================================
//  NEXUS
// ============================================================
enum NexusDir : uint8_t {
    NEX_DOWN  = 1,
    NEX_UP    = 2,
    NEX_LEFT  = 3,
    NEX_RIGHT = 4,
};

#define NEXUS_TRACKING_MAX 100U
#define NEXUS_ENLARGED_MAX (WIDTH * 2)

static float   nexusPosX[NEXUS_TRACKING_MAX];
static float   nexusPosY[NEXUS_TRACKING_MAX];
static float   nexusSpeedX[NEXUS_TRACKING_MAX];
static uint8_t nexusHue[NEXUS_TRACKING_MAX];
static uint8_t nexusState[NEXUS_TRACKING_MAX];
static uint8_t nexusNum;
static float   nexus_speedfactor;

static inline float fmap(float x, float in_min, float in_max, float out_min, float out_max) {
    return (out_max - out_min) * (x - in_min) / (in_max - in_min) + out_min;
}

static void nexusReset(uint8_t i) {
    nexusHue[i] = random8();
    nexusState[i] = random8(4);
    nexusSpeedX[i] = (float)random8(5, 11) / 70 + nexus_speedfactor;

    switch (nexusState[i]) {
        case NEX_DOWN: nexusPosY[i] = HEIGHT;  nexusPosX[i] = random8(WIDTH);  break;
        case NEX_UP: nexusPosY[i] = -1;      nexusPosX[i] = random8(WIDTH);  break;
        case NEX_LEFT: nexusPosX[i] = WIDTH;   nexusPosY[i] = random8(HEIGHT); break;
        case NEX_RIGHT: nexusPosX[i] = -1;      nexusPosY[i] = random8(HEIGHT); break;
    }
}

void nexusRoutine() {
    EffectSettings& s = effects_currentSettings();

    if (loadingFlag) {
        loadingFlag = false;
        nexus_speedfactor = fmap(s.speed, 1, 255, 0.1f, 0.33f);

        nexusNum = (s.scale - 1U) / 99.0f * (NEXUS_ENLARGED_MAX - 1U) + 1U;
        if (nexusNum > NEXUS_ENLARGED_MAX) nexusNum = NEXUS_ENLARGED_MAX;

        for (uint8_t i = 0; i < nexusNum; i++) {
            nexusPosX[i] = random8(WIDTH);
            nexusPosY[i] = random8(HEIGHT);
            nexusSpeedX[i] = (float)random8(5, 11) / 70 + nexus_speedfactor;
            nexusHue[i] = random8();
            nexusState[i] = random8(4);
        }
        deltaValue = 255U - map(s.speed, 1, 255, 11, 33);
    }

    matrix_dimAll(deltaValue);

    for (uint8_t i = 0; i < nexusNum; i++) {
        switch (nexusState[i]) {
            case NEX_DOWN:
                nexusPosY[i] -= nexusSpeedX[i];
                if (nexusPosY[i] <= -1) nexusReset(i);
                break;
            case NEX_UP:
                nexusPosY[i] += nexusSpeedX[i];
                if (nexusPosY[i] >= HEIGHT) nexusReset(i);
                break;
            case NEX_LEFT:
                nexusPosX[i] -= nexusSpeedX[i];
                if (nexusPosX[i] <= -1) nexusReset(i);
                break;
            case NEX_RIGHT:
                nexusPosX[i] += nexusSpeedX[i];
                if (nexusPosX[i] >= WIDTH) nexusReset(i);
                break;
        }
        matrix_drawPixelXYF(nexusPosX[i], nexusPosY[i],
                            CHSV(nexusHue[i], 255U, 255));
    }
}
