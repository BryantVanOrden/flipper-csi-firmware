// ble_link.h - BLE GATT server that streams CSI results to the phone and
// receives control commands. Protocol defined in PROTOCOL.md.
#pragma once

#include <Arduino.h>
#include "csi_sense.h"

namespace BleLink {
  // Control command callback: cmd byte, optional arg byte, whether arg present.
  typedef void (*CtrlCb)(uint8_t cmd, uint8_t arg, bool hasArg);

  void begin(CtrlCb cb);
  bool isConnected();
  void notifyCsi(const CsiResult &r);   // sends one CSI Data frame
  void updateStatus(const char *json);  // sets + notifies the Status characteristic
}
