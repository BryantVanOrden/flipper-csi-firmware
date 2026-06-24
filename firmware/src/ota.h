// ota.h - WiFi HTTP OTA update endpoint.
//
// When the board is on WiFi (active mode), this serves a tiny web server with a
// POST /update endpoint that accepts a new firmware .bin and flashes it via the
// ESP32 Update library. The Android app drives this; you can also flash from a
// browser/curl on the same network:
//   curl -F "firmware=@flipper-csi-sense.bin" http://<board-ip>/update
#pragma once

#include <Arduino.h>

namespace Ota {
  void   begin();          // start the web server (call once WiFi is connected)
  void   loop();           // service HTTP clients (call every loop())
  bool   running();        // true once begin() has started the server
  String ip();             // board IP, or "" if not connected
}
