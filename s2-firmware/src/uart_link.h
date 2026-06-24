// uart_link.h - CSI transport over the hardware UART (to the Flipper bridge).
// Replaces the BLE link used on Bluetooth-capable chips. See config.h for the
// wire framing magics.
#pragma once

#include <Arduino.h>
#include "csi_sense.h"

namespace UartLink {
  void begin(uint32_t baud);
  void sendCsi(const CsiResult &r);     // 0xC5 frame: ESP32 -> phone
  void sendStatus(const char* json);    // 0xC6 frame: ESP32 -> phone

  // Drain incoming control frames [0xC7, cmd, len, payload[len]] from the phone.
  typedef void (*CtrlCb)(uint8_t cmd, const uint8_t* payload, uint8_t len);
  void poll(CtrlCb cb);
}
