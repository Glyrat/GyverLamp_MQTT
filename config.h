#pragma once

// ============================================================
//  КОНФИГУРАЦИЯ ЖЕЛЕЗА
// ============================================================
#define WIDTH                 16
#define HEIGHT                16
#define NUM_LEDS              (WIDTH * HEIGHT)
#define SEGMENTS              1
#define LED_PIN               4
#define COLOR_ORDER           GRB

#define MATRIX_TYPE           0     // 0 = зигзаг, 1 = параллельная
#define CONNECTION_ANGLE      1     // 0-3
#define STRIP_DIRECTION       3     // 0-3

#define BRIGHTNESS            40    // яркость по умолчанию
#define CURRENT_LIMIT         2000  // мА, 0 = выключить

// ============================================================
//  КНОПКА (не используется, но код сохранён)
// ============================================================
#define BTN_PIN               5
#define BTN_USE               0     // 0 = выключена, 1 = включена

// ============================================================
//  ВРЕМЯ / БУДИЛЬНИК
// ============================================================
#define GMT_OFFSET_HOURS      3
#define NTP_SERVER            "europe.pool.ntp.org"
#define NTP_INTERVAL_MS       (600UL * 1000UL)   // 10 минут
#define DAWN_MIN_MINUTES      1
#define DAWN_MAX_MINUTES      60
#define AFTER_DELAY_MIN       1
#define AFTER_DELAY_MAX       60

// ============================================================
//  WI-FI / BOOT COUNT
// ============================================================
#define AP_SSID               "GyverLamp"
#define AP_PASS               "12345678"
#define AP_PORT               80
#define BOOT_RESET_THRESHOLD  5     // быстрых перезагрузок до сброса Wi-Fi
#define BOOT_WINDOW_MS        8000  // окно "быстрой" перезагрузки (мс)

// ============================================================
//  OTA / MQTT
// ============================================================
#define OTA_PATH              "/update"
#define MQTT_BASE_TOPIC       "gyverlamp"

// ============================================================
//  ФАЙЛОВАЯ СИСТЕМА
// ============================================================
#define FS_WIFI_FILE          "/wifi.json"
#define FS_MQTT_FILE          "/mqtt.json"
#define FS_CONFIG_FILE        "/config.json"
#define FS_ALARMS_FILE        "/alarms.json"
#define FS_BOOT_FILE          "/boot.json"

// ============================================================
//  ИНДИКАЦИЯ СТАРТА
// ============================================================
#define STARTUP_ANIM_MS       1500  // длительность анимации запуска
#define AP_ANIM_MS            3000  // длительность анимации "мы в AP"

// ============================================================
//  ОТКЛЮЧЕНИЕ ТЯЖЁЛЫХ ЭФФЕКТОВ (по необходимости)
// ============================================================
//#define ENABLE_AQUARIUM       1
//#define ENABLE_SPIRO          1
//#define ENABLE_POLICE         1
#define ENABLE_CUBE2D         1
#define ENABLE_NEXUS          1

// ============================================================
//  ПРОЧЕЕ
// ============================================================
#define SERIAL_BAUD           115200
#define DEBUG_SERIAL          1     // 1 = включить отладку в Serial