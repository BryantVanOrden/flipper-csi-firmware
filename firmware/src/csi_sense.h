// csi_sense.h - Wi-Fi CSI capture + motion/presence/breathing detection.
//
// Captures Channel State Information from the ESP32 radio, tracks a slow
// empty-room baseline, and derives a motion metric, a presence decision, and
// an (experimental) breathing-rate estimate. Designed to be called from a
// normal Arduino loop(); the heavy WiFi-task work is just a memcpy into a queue.
#pragma once

#include <Arduino.h>
#include "config.h"

struct CsiResult {
  uint8_t n_sub;                    // number of valid subcarrier amplitudes
  uint8_t amp[MAX_SUBCARRIERS];     // per-subcarrier amplitude, normalized 0..255
  uint8_t motion;                   // motion metric 0..255 (drives the gauge)
  bool    motion_flag;              // motion > threshold
  bool    presence;                 // someone is detected in range
  uint8_t breathing_bpm;            // experimental; 0 = unknown
  int8_t  rssi;                     // last packet RSSI (dBm)
  bool    calibrated;               // baseline established
  uint8_t seq;                      // rolling counter
};

namespace CsiSense {
  // mode: 0 = passive (promiscuous sniff), 1 = active (STA + ping gateway)
  void    begin(uint8_t mode, uint8_t channel);

  // Drain captured CSI, update detectors, and (if new data arrived) fill `out`.
  // Returns true when `out` holds a fresh result. Call this every loop().
  bool    poll(CsiResult &out);

  // Runtime control (also reachable over BLE).
  void    setMode(uint8_t mode);            // 0 passive / 1 active
  void    setChannel(uint8_t ch);           // passive only
  void    setMotionThreshold(uint8_t t);    // 0..255
  void    resetBaseline();                  // recalibrate empty room

  // Introspection (used for the Status JSON).
  uint8_t mode();
  uint8_t channel();
  float   sampleRate();                     // CSI samples/sec
  int8_t  lastRssi();
  uint8_t subcarriers();
  bool    calibrated();
}
