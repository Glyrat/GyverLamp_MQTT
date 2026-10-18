#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include "effects.h"
#include "matrix.h"

// ============================================================
//  NOISE ЭФФЕКТЫ
//  Общий движок: fillNoiseLED / fillnoise8
// ============================================================
void madnessNoise();
void cloudNoise();
void lavaNoise();
void plasmaNoise();
void rainbowNoise();
void rainbowStripeNoise();
void zebraNoise();
void forestNoise();
void oceanNoise();

// ============================================================
//  ВНУТРЕННИЕ (доступны другим эффектам, использующим шум)
// ============================================================
// Заполнить массив noise[][] по текущим координатам и скорости.
void noise_fillnoise8();

// Полный проход noise-эффекта: заполнить шум + отрисовать с палитрой.
void noise_fillNoiseLED();
