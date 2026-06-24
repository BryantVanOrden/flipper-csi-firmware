// main.cpp - Flipper CSI Sense standalone firmware.
//
// Captures Wi-Fi CSI, derives presence/motion/breathing, streams it over BLE
// to the Android companion app, and (optionally) shows a minimal status line on
// the on-board TFT. The phone is the real display.
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "csi_sense.h"
#include "ble_link.h"
#include "ota.h"

#if USE_TFT
#include <TFT_eSPI.h>
static TFT_eSPI tft = TFT_eSPI();
#endif

static uint32_t s_lastStatus = 0;

// BLE control commands -> sensing engine (see PROTOCOL.md).
static void onCtrl(uint8_t cmd, uint8_t arg, bool hasArg) {
  switch (cmd) {
    case 0x01: if (hasArg) CsiSense::setMotionThreshold(arg); break;  // motion threshold
    case 0x02: if (hasArg) CsiSense::setMode(arg);            break;  // 0 passive / 1 active
    case 0x03: CsiSense::resetBaseline();                     break;  // recalibrate
    case 0x04: if (hasArg) CsiSense::setChannel(arg);         break;  // passive channel
    default: break;
  }
}

static void pushStatus() {
  char json[192];
  snprintf(json, sizeof(json),
           "{\"mode\":\"%s\",\"ch\":%u,\"rate\":%.1f,\"rssi\":%d,\"sub\":%u,\"cal\":%s,"
           "\"ip\":\"%s\",\"ota\":%s}",
           CsiSense::mode() == 1 ? "active" : "passive",
           CsiSense::channel(),
           CsiSense::sampleRate(),
           CsiSense::lastRssi(),
           CsiSense::subcarriers(),
           CsiSense::calibrated() ? "true" : "false",
           Ota::ip().c_str(),
           Ota::running() ? "true" : "false");
  BleLink::updateStatus(json);
}

#if USE_TFT
static void drawTft(const CsiResult &r) {
  tft.fillRect(0, 0, tft.width(), 40, r.presence ? TFT_RED : TFT_DARKGREEN);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(r.presence ? "PRESENCE" : "CLEAR", tft.width() / 2, 20, 4);

  int barW = map(r.motion, 0, 255, 0, tft.width());
  tft.fillRect(0, 50, tft.width(), 16, TFT_NAVY);
  tft.fillRect(0, 50, barW, 16, TFT_CYAN);

  char line[48];
  snprintf(line, sizeof(line), "motion %3u  bpm %2u  %s",
           r.motion, r.breathing_bpm, BleLink::isConnected() ? "BT" : "--");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(0, 72, tft.width(), 20, TFT_BLACK);
  tft.drawString(line, 4, 74, 2);
}
#endif

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Flipper CSI Sense ===");

#if USE_TFT
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("CSI Sense booting...", 4, 4, 2);
#endif

  BleLink::begin(onCtrl);
  CsiSense::begin(DEFAULT_MODE, DEFAULT_CHANNEL);
}

void loop() {
  // Bring up the OTA web server once WiFi is up (active mode).
  if (!Ota::running() && WiFi.status() == WL_CONNECTED) Ota::begin();
  Ota::loop();

  CsiResult r;
  if (CsiSense::poll(r)) {
    BleLink::notifyCsi(r);
#if USE_TFT
    drawTft(r);
#endif
  }

  uint32_t now = millis();
  if (now - s_lastStatus >= STATUS_PERIOD_MS) {
    s_lastStatus = now;
    pushStatus();
  }
}
