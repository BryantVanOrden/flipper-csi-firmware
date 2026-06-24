// ota.cpp - see ota.h
#include "ota.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

static WebServer s_server(80);
static bool s_running = false;

static const char* INFO_PAGE =
    "<html><body style='font-family:sans-serif;background:#0b1320;color:#e6edf3'>"
    "<h2>Flipper CSI Sense</h2>"
    "<p>OTA endpoint is up. POST a firmware .bin to <code>/update</code>.</p>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<input type='file' name='firmware'><input type='submit' value='Flash'></form>"
    "</body></html>";

static void handleUpdateUpload() {
  HTTPUpload& up = s_server.upload();
  if (up.status == UPLOAD_FILE_START) {
    Serial.printf("[ota] start: %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) Serial.printf("[ota] success: %u bytes\n", up.totalSize);
    else Update.printError(Serial);
  }
}

void Ota::begin() {
  if (s_running) return;

  s_server.on("/", HTTP_GET, []() {
    s_server.send(200, "text/html", INFO_PAGE);
  });

  // The upload handler runs first (streams the file), then the response lambda.
  s_server.on("/update", HTTP_POST,
    []() {
      bool ok = !Update.hasError();
      s_server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "FAIL");
      delay(500);
      if (ok) ESP.restart();
    },
    handleUpdateUpload
  );

  s_server.begin();
  s_running = true;
  Serial.printf("[ota] http server on http://%s/update\n", WiFi.localIP().toString().c_str());
}

void Ota::loop() { if (s_running) s_server.handleClient(); }
bool Ota::running() { return s_running; }
String Ota::ip() { return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String(""); }
