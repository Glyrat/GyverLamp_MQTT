#include "ota.h"
#include "config.h"
#include "matrix.h"
#include "web.h"
#include <Updater.h> 

#include <ESP8266WebServer.h>
#include <FastLED.h>

extern CRGB leds[NUM_LEDS];

static ESP8266WebServer* s_server = nullptr;

// ============================================================
//  HTML 页面 /update
// ============================================================
static const char UPDATE_PAGE[] PROGMEM = R"HTML(
<div class="card">
  <h1>Обновление прошивки</h1>
  <form method="POST" action="/update" enctype="multipart/form-data">
    <label>Выберите файл .bin</label>
    <input type="file" name="update" accept=".bin">
    <button type="submit">Загрузить</button>
  </form>
  <div id="msg" class="msg"></div>
  <p style="color:#888;font-size:13px;margin-top:16px">
    После загрузки лампа перезагрузится.
  </p>
</div>
)HTML";

static void _otaGet() {
    String html;
    html.reserve(4096);
    html += FPSTR(PAGE_HEAD);
    html += FPSTR(UPDATE_PAGE);
    html += FPSTR(PAGE_FOOT);
    s_server->sendHeader("Cache-Control", "no-store");
    s_server->send(200, "text/html; charset=utf-8", html);
}

static void _otaPost() {
    if (Update.hasError()) {
        s_server->send(200, "text/plain",
                       "Ошибка: " + String(Update.getError()));
    } else {
        s_server->send(200, "text/plain", "OK. Перезагрузка...");
        delay(500);
        ESP.restart();
    }
}

static void _otaUpload() {
    HTTPUpload& up = s_server->upload();

    if (up.status == UPLOAD_FILE_START) {
        Serial.print(F("[ota] start: "));
        Serial.println(up.filename);

        FastLED.setBrightness(255);
        matrix_fillAll(CRGB(32, 0, 0));
        FastLED.show();

        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) {
            Update.printError(Serial);
        }
    }
    else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
            Update.printError(Serial);
        }
    }
    else if (up.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.print(F("[ota] done, size: "));
            Serial.println(up.totalSize);
            matrix_fillAll(CRGB(0, 64, 0));
            FastLED.show();
        } else {
            Update.printError(Serial);
            matrix_fillAll(CRGB(64, 0, 0));
            FastLED.show();
        }
    }
    else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        Serial.println(F("[ota] aborted"));
    }
}

void ota_init(ESP8266WebServer* server) {
    s_server = server;
    s_server->on("/update", HTTP_GET,  _otaGet);
    s_server->on("/update", HTTP_POST, _otaPost, _otaUpload);

    #if DEBUG_SERIAL
    Serial.println(F("[ota] /update ready"));
    #endif
}
