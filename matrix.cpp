#include "matrix.h"

// ============================================================
//  КОНФИГУРАЦИЯ МАТРИЦЫ (аналогично utility.ino из оригинала)
// ============================================================
#if (CONNECTION_ANGLE == 0 && STRIP_DIRECTION == 0)
  #define _WIDTH WIDTH
  #define THIS_X x
  #define THIS_Y y
#elif (CONNECTION_ANGLE == 0 && STRIP_DIRECTION == 1)
  #define _WIDTH HEIGHT
  #define THIS_X y
  #define THIS_Y x
#elif (CONNECTION_ANGLE == 1 && STRIP_DIRECTION == 0)
  #define _WIDTH WIDTH
  #define THIS_X x
  #define THIS_Y (HEIGHT - y - 1)
#elif (CONNECTION_ANGLE == 1 && STRIP_DIRECTION == 3)
  #define _WIDTH HEIGHT
  #define THIS_X (HEIGHT - y - 1)
  #define THIS_Y x
#elif (CONNECTION_ANGLE == 2 && STRIP_DIRECTION == 2)
  #define _WIDTH WIDTH
  #define THIS_X (WIDTH - x - 1)
  #define THIS_Y (HEIGHT - y - 1)
#elif (CONNECTION_ANGLE == 2 && STRIP_DIRECTION == 3)
  #define _WIDTH HEIGHT
  #define THIS_X (HEIGHT - y - 1)
  #define THIS_Y (WIDTH - x - 1)
#elif (CONNECTION_ANGLE == 3 && STRIP_DIRECTION == 2)
  #define _WIDTH WIDTH
  #define THIS_X (WIDTH - x - 1)
  #define THIS_Y y
#elif (CONNECTION_ANGLE == 3 && STRIP_DIRECTION == 1)
  #define _WIDTH HEIGHT
  #define THIS_X y
  #define THIS_Y (WIDTH - x - 1)
#else
  #define _WIDTH WIDTH
  #define THIS_X x
  #define THIS_Y y
  #pragma message "Wrong matrix parameters! Set to default"
#endif




// ============================================================
//  БАЗОВЫЕ ОПЕРАЦИИ
// ============================================================
uint16_t matrix_pixelNumber(int8_t x, int8_t y) {
    if ((THIS_Y % 2 == 0) || MATRIX_TYPE) {
        return (THIS_Y * _WIDTH + THIS_X);
    } else {
        return (THIS_Y * _WIDTH + _WIDTH - THIS_X - 1);
    }
}

void matrix_drawPixel(int8_t x, int8_t y, const CRGB& color) {
    if (x < 0 || x > WIDTH - 1 || y < 0 || y > HEIGHT - 1) return;
    int thisPixel = matrix_pixelNumber(x, y) * SEGMENTS;
    for (uint8_t i = 0; i < SEGMENTS; i++) {
        leds[thisPixel + i] = color;
    }
}

void matrix_drawPixelSeg(int8_t x, int8_t y, const CRGB& color) {
    matrix_drawPixel(x, y, color);
}

uint32_t matrix_getPixel(uint16_t idx) {
    int thisPixel = idx * SEGMENTS;
    if (thisPixel < 0 || thisPixel > NUM_LEDS - 1) return 0;
    return (((uint32_t)leds[thisPixel].r << 16) |
            ((uint32_t)leds[thisPixel].g << 8)  |
            (uint32_t)leds[thisPixel].b);
}

uint32_t matrix_getPixelXY(int8_t x, int8_t y) {
    return matrix_getPixel(matrix_pixelNumber(x, y));
}

// ============================================================
//  ЗАЛИВКА / ОЧИСТКА
// ============================================================
void matrix_fillAll(const CRGB& color) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = color;
    }
}

void matrix_clear() {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(0, 0, 0);
    }
}

void matrix_dimAll(uint8_t value) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        leds[i].nscale8(value);
    }
}

void matrix_fadePixel(uint8_t i, uint8_t j, uint8_t step) {
    int pixelNum = matrix_pixelNumber(i, j);
    if (matrix_getPixel(pixelNum) == 0) return;

    if (leds[pixelNum].r >= 30 ||
        leds[pixelNum].g >= 30 ||
        leds[pixelNum].b >= 30) {
        leds[pixelNum].fadeToBlackBy(step);
    } else {
        leds[pixelNum] = 0;
    }
}

void matrix_fader(uint8_t step) {
    for (uint8_t i = 0; i < WIDTH; i++) {
        for (uint8_t j = 0; j < HEIGHT; j++) {
            matrix_fadePixel(i, j, step);
        }
    }
}

// ============================================================
//  СУБПИКСЕЛЬНАЯ ОТРИСОВКА
// ============================================================
void matrix_drawPixelXYF(float x, float y, const CRGB& color) {
    float xt = x + 1.0f, yt = y + 1.0f;
    if (xt < 0 || yt < 0) return;

    uint8_t xx = (xt - (int)xt) * 255;
    uint8_t yy = (yt - (int)yt) * 255;
    uint8_t ix = 255 - xx;
    uint8_t iy = 255 - yy;

    #define WU_WEIGHT(a,b) ((uint8_t)(((a)*(b)+(a)+(b)) >> 8))
    uint8_t wu[4] = {
        WU_WEIGHT(ix, iy),
        WU_WEIGHT(xx, iy),
        WU_WEIGHT(ix, yy),
        WU_WEIGHT(xx, yy)
    };
    #undef WU_WEIGHT

    for (uint8_t i = 0; i < 4; i++) {
        int16_t xn = x + (i & 1);
        int16_t yn = y + ((i >> 1) & 1);
        CRGB clr = CRGB(matrix_getPixelXY(xn, yn));
        clr.r = qadd8(clr.r, (color.r * wu[i]) >> 8);
        clr.g = qadd8(clr.g, (color.g * wu[i]) >> 8);
        clr.b = qadd8(clr.b, (color.b * wu[i]) >> 8);
        matrix_drawPixel(xn, yn, clr);
    }
}

// ============================================================
//  УТИЛИТЫ
// ============================================================
float matrix_sqrt3(const float x) {
    union {
        int i;
        float x;
    } u;
    u.x = x;
    u.i = (1 << 29) + (u.i >> 1) - (1 << 22);
    return u.x;
}

// void matrix_blur2d(uint8_t amount) {
//     static CRGB tmp[NUM_LEDS];
//     memcpy(tmp, leds, sizeof(CRGB) * NUM_LEDS);

//     for (uint8_t x = 0; x < WIDTH; x++) {
//         for (uint8_t y = 0; y < HEIGHT; y++) {
//             uint16_t self  = matrix_pixelNumber(x, y);
//             uint16_t left  = matrix_pixelNumber((x + WIDTH - 1) % WIDTH, y);
//             uint16_t right = matrix_pixelNumber((x + 1) % WIDTH, y);
//             uint16_t up    = matrix_pixelNumber(x, (y + HEIGHT - 1) % HEIGHT);
//             uint16_t down  = matrix_pixelNumber(x, (y + 1) % HEIGHT);

//             leds[self].r = (tmp[self].r * (255 - amount) +
//                             tmp[left].r + tmp[right].r + tmp[up].r + tmp[down].r) * amount / 255 / 4;
//             leds[self].g = (tmp[self].g * (255 - amount) +
//                             tmp[left].g + tmp[right].g + tmp[up].g + tmp[down].g) * amount / 255 / 4;
//             leds[self].b = (tmp[self].b * (255 - amount) +
//                             tmp[left].b + tmp[right].b + tmp[up].b + tmp[down].b) * amount / 255 / 4;
//         }
//     }
// }
// void matrix_blur2d(uint8_t amount) {
//     blur2d(leds, WIDTH, HEIGHT, amount);
// }

void matrix_blur2d(uint8_t amount) {
    static CRGB tmp[NUM_LEDS];
    for (uint16_t i = 0; i < NUM_LEDS; i++) tmp[i] = leds[i];

    for (uint8_t x = 0; x < WIDTH; x++) {
        for (uint8_t y = 0; y < HEIGHT; y++) {
            uint16_t self  = matrix_pixelNumber(x, y);
            uint16_t left  = matrix_pixelNumber((x + WIDTH - 1) % WIDTH, y);
            uint16_t right = matrix_pixelNumber((x + 1) % WIDTH, y);
            uint16_t up    = matrix_pixelNumber(x, (y + HEIGHT - 1) % HEIGHT);
            uint16_t down  = matrix_pixelNumber(x, (y + 1) % HEIGHT);

            // правильное усреднение с весами
            uint16_t wself = 255 - amount;
            leds[self].r = (tmp[self].r * wself + tmp[left].r * amount/4 +
                            tmp[right].r * amount/4 + tmp[up].r * amount/4 +
                            tmp[down].r * amount/4) >> 8;
            leds[self].g = (tmp[self].g * wself + tmp[left].g * amount/4 +
                            tmp[right].g * amount/4 + tmp[up].g * amount/4 +
                            tmp[down].g * amount/4) >> 8;
            leds[self].b = (tmp[self].b * wself + tmp[left].b * amount/4 +
                            tmp[right].b * amount/4 + tmp[up].b * amount/4 +
                            tmp[down].b * amount/4) >> 8;
        }
    }
}

// XY — для эффектов, которые используют свою адресацию
// (например, дождь). Не путать с matrix_pixelNumber.
uint16_t XY(uint8_t x, uint8_t y) {
    uint16_t i;
    if (y & 0x01) {
        uint8_t reverseX = (WIDTH - 1) - x;
        i = (y * WIDTH) + reverseX;
    } else {
        i = (y * WIDTH) + x;
    }
    return i;
}