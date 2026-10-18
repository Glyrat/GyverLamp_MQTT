#include "effects.h"
#include "matrix.h"

uint8_t gR = 255, gG = 255, gB = 255;

CRGBPalette16 cPalette(PartyColors_p);
uint8_t       noise3d[2][WIDTH][HEIGHT];

void effects_setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
    gR = r; gG = g; gB = b;
}


// ============================================================
//  ВНЕШНИЕ ФУНКЦИИ ЭФФЕКТОВ
// ============================================================
// Fire
void fireRoutine();
void warmLightRoutine();

// Rainbow / Color
void rainbowVertical();
void rainbowHorizontal();
void colorsRoutine();
void colorRoutine();

// Sparkles / Snow / Matrix / Lighters
void sparklesRoutine();
void snowRoutine();
void matrixRoutine();
void lightersRoutine();
void snowStormRoutine();
void starfallRoutine();

// Noise (общий файл noiseEffects.cpp)
void madnessNoise();
void cloudNoise();
void lavaNoise();
void plasmaNoise();
void rainbowNoise();
void rainbowStripeNoise();
void zebraNoise();
void forestNoise();
void oceanNoise();

void sparklesRoutine();
// Rain / Storm
void simpleRain();
void stormyRain();

// Butterflys / Cube / Nexus
void butterflysRoutine(bool isColored);
void cube2dRoutine();
void nexusRoutine();

// Pride / Twinkles / Drift / Pendulum
void prideRoutine();
void twinklesRoutine();
void PatternIncrementalDrift2();
void prismataRoutine();

void lightBallsRoutine();

// ============================================================
//  РЕЕСТР ЭФФЕКТОВ
// ============================================================
// ВАЖНО: порядок совпадает с оригинальным MODE_AMOUNT,
//        чтобы не сломать уже сохранённые настройки.
// Удалены: Аквариум (18), Спираль (21), Полицейская сирена (25)
//
// Формат: { "Имя", routine, usesSpeed, usesScale }
//
//   usesSpeed — показывать ли слайдер "Скорость" в вебе
//   usesScale — показывать ли слайдер "Масштаб" в вебе
//
// ВНИМАНИЕ: butterflysRoutine(bool) не подходит под сигнатуру void().
// Решение: обернём в лямбду-обёртку ниже.
// ============================================================

static void butterflysRoutine_wrap() { butterflysRoutine(false); }

const Effect EFFECTS[] = {
    // idx   name                      routine                     speed  scale
    { "Конфетти",                    sparklesRoutine,              true,  true  }, // 0
    { "Огонь",                       fireRoutine,                  true,  true  }, // 1
    { "Радуга верт.",                rainbowVertical,              true, true  }, // 2
    { "Радуга гориз.",               rainbowHorizontal,            true, true  }, // 3
    { "Смена цвета",                 colorsRoutine,                true, false }, // 4
    { "Безумие 3D",                  madnessNoise,                 true,  true  }, // 5
    { "Облака 3D",                   cloudNoise,                   true,  true  }, // 6
    { "Лава 3D",                     lavaNoise,                    true,  true  }, // 7
    { "Плазма 3D",                   plasmaNoise,                  true,  true  }, // 8
    { "Радуга 3D",                   rainbowNoise,                 true,  true  }, // 9
    { "Павлин 3D",                   rainbowStripeNoise,           true,  true  }, // 10
    { "Зебра 3D",                    zebraNoise,                   true,  true  }, // 11
    { "Лес 3D",                      forestNoise,                  true,  true  }, // 12
    { "Океан 3D",                    oceanNoise,                   true,  true  }, // 13
    { "Цвет",                        colorRoutine,                 false, false }, // 14
    { "Снегопад",                    snowRoutine,                  true, true  }, // 15
    { "Матрица",                     matrixRoutine,                true, true  }, // 16
    { "Светлячки",                   lightersRoutine,              true, true  }, // 17
    { "Звездопад",                   starfallRoutine,              true, true  }, // 18
    { "Пейнтбол",                    lightBallsRoutine,            true,  true  }, // 19
    { "Теплый свет",                 warmLightRoutine,             true,  true  }, // 20
    { "Маятник",                     prismataRoutine,              true,  true  }, // 21
    { "Мерцание",                    twinklesRoutine,              true, true  }, // 22
    { "Дрейф",                       PatternIncrementalDrift2,     true, true  }, // 23
    { "Стая",                        prideRoutine,                 true, false }, // 24
    { "Мотыльки в банке",            butterflysRoutine_wrap,       true,  true  }, // 25
    { "Дождь",                       simpleRain,                   true,  true  }, // 26
    { "Гроза",                       stormyRain,                   true,  true  }, // 27
    { "Кубик Рубика",                cube2dRoutine,                true,  true  }, // 28
    { "Nexus",                       nexusRoutine,                 true,  true  }, // 29
};

const uint8_t EFFECT_COUNT = sizeof(EFFECTS) / sizeof(EFFECTS[0]);

// ============================================================
//  НАСТРОЙКИ ЭФФЕКТОВ
// ============================================================
EffectSettings effectSettings[EFFECT_COUNT];

// ============================================================
//  ОБЩИЕ ПЕРЕМЕННЫЕ
// ============================================================
volatile bool loadingFlag = true;
bool demo      = false;
bool epilepsy  = false;   // по умолчанию выключаем "Смену цвета" в демо

uint8_t hue          = 0;
uint8_t hue2         = 0;
uint8_t deltaValue   = 0;
uint8_t step         = 0;
uint8_t deltaHue     = 0;
uint8_t deltaHue2    = 0;

// ============================================================
//  ПАЛИТРЫ
// ============================================================
const TProgmemRGBPalette16* palette_arr[] = {
    &PartyColors_p,
    &OceanColors_p,
    &LavaColors_p,
    &HeatColors_p,
    &CloudColors_p,
    &ForestColors_p,
    &RainbowColors_p
};
const uint8_t PALETTE_ARR_SIZE = sizeof(palette_arr) / sizeof(palette_arr[0]);

const TProgmemRGBPalette16* curPalette = palette_arr[0];

// ============================================================
//  ТЕКУЩИЙ РЕЖИМ
// ============================================================
static int8_t _currentMode = 0;

// ============================================================
//  API
// ============================================================
void effects_init() {
    // Дефолты на случай первого запуска
    for (uint8_t i = 0; i < EFFECT_COUNT; i++) {
        effectSettings[i].brightness = BRIGHTNESS;
        effectSettings[i].speed      = 30;
        effectSettings[i].scale      = 40;
    }

    effectSettings[1].scale = 15;   // Огонь
}

void effects_run() {
    if (_currentMode < 0 || _currentMode >= EFFECT_COUNT) return;

    const Effect& e = EFFECTS[_currentMode];
    if (e.routine == nullptr) return;

    // Пробрасываем настройки текущего эффекта в общие переменные,
    // которые используют эффекты.
    // (В оригинале они читают modes[currentMode].speed напрямую.)
    // Здесь эффекты будут читать effectSettings[_currentMode].

    e.routine();

    if (loadingFlag) {
        loadingFlag = false;
    }
}

void effects_setMode(int8_t idx, bool save) {
    if (idx < 0) idx = 0;
    if (idx >= EFFECT_COUNT) idx = EFFECT_COUNT - 1;

    if (_currentMode == idx) return;

    _currentMode = idx;
    loadingFlag = true;
    matrix_clear();

    if (save) {
        // сохранение настроек сделает вызывающий код (web/mqtt)
    }
}

int8_t effects_currentMode() {
    return _currentMode;
}

EffectSettings& effects_currentSettings() {
    return effectSettings[_currentMode];
}

// ============================================================
//  ПАЛИТРЫ
// ============================================================
void effects_setCurrentPalette() {
    EffectSettings& s = effects_currentSettings();
    if (s.scale > 100U) s.scale = 100U;

    uint8_t idx = (uint8_t)((float)s.scale / 100.0f *
                            ((float)PALETTE_ARR_SIZE - 0.01f));
    if (idx >= PALETTE_ARR_SIZE) idx = PALETTE_ARR_SIZE - 1;
    curPalette = palette_arr[idx];
}

// Синоним с опечаткой из оригинала (совместимость)
void effects_setCurrentPallete() {
    effects_setCurrentPalette();
}