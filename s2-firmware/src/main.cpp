// main.cpp - Flipper CSI Sense, ESP32-S2 / UART build.
// Capture Wi-Fi CSI and stream it over UART to the Flipper, which bridges it to
// the phone over the Flipper's Bluetooth. No on-board BLE (the S2 has none).
#include <Arduino.h>
#include "config.h"
#include "csi_sense.h"
#include "uart_link.h"

static uint32_t s_lastStatus = 0;

// Control commands from the phone (relayed by the Flipper). See PROTOCOL.
static void onCtrl(uint8_t cmd, const uint8_t* payload, uint8_t len) {
  switch (cmd) {
    case CMD_THRESHOLD:   if (len >= 1) CsiSense::setMotionThreshold(payload[0]); break;
    case CMD_MODE:        if (len >= 1) CsiSense::setMode(payload[0]);            break;
    case CMD_RECALIBRATE: CsiSense::resetBaseline();                             break;
    case CMD_CHANNEL:     if (len >= 1) CsiSense::setChannel(payload[0]);        break;
    case CMD_SET_SSID: {
      char s[33]; uint8_t n = (len < 32) ? len : 32; memcpy(s, payload, n); s[n] = 0;
      CsiSense::setCredentials(s, nullptr);
      break;
    }
    case CMD_SET_PASS: {
      char p[64]; uint8_t n = (len < 63) ? len : 63; memcpy(p, payload, n); p[n] = 0;
      CsiSense::setCredentials(nullptr, p);
      break;
    }
    case CMD_CONNECT:     CsiSense::applyCredentials();                         break;
    default: break;
  }
}

static void pushStatus() {
  char json[160];
  snprintf(json, sizeof(json),
           "{\"mode\":\"%s\",\"ch\":%u,\"rate\":%.1f,\"rssi\":%d,\"sub\":%u,\"cal\":%s}",
           CsiSense::mode() == 1 ? "active" : "passive",
           CsiSense::channel(), CsiSense::sampleRate(), CsiSense::lastRssi(),
           CsiSense::subcarriers(), CsiSense::calibrated() ? "true" : "false");
  UartLink::sendStatus(json);
}

void setup() {
  UartLink::begin(UART_BAUD);
  CsiSense::begin(DEFAULT_MODE, DEFAULT_CHANNEL);
}

void loop() {
  UartLink::poll(onCtrl);

  CsiResult r;
  if (CsiSense::poll(r)) UartLink::sendCsi(r);

  uint32_t now = millis();
  if (now - s_lastStatus >= STATUS_PERIOD_MS) {
    s_lastStatus = now;
    pushStatus();
  }
}
