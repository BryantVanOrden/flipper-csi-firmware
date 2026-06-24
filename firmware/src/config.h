// config.h - build-time configuration for Flipper CSI Sense
#pragma once

// ---------------------------------------------------------------------------
// Wi-Fi (used by ACTIVE mode: ESP32 joins your network and pings the gateway
// to generate a steady stream of received packets -> steady CSI).
// In PASSIVE mode these are ignored and we sniff a fixed channel instead.
// ---------------------------------------------------------------------------
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"

// Start-up sensing mode: 0 = passive (promiscuous sniff), 1 = active (STA + ping).
// Active is recommended for motion/breathing. Can be changed at runtime over BLE.
#define DEFAULT_MODE        1

// Passive-mode channel to lock onto (1..13). Active mode follows the AP's channel.
#define DEFAULT_CHANNEL     6

// Ping interval in ACTIVE mode (ms). Lower = higher CSI rate, more airtime.
#define PING_INTERVAL_MS    50

// ---------------------------------------------------------------------------
// Sensing parameters
// ---------------------------------------------------------------------------
#define MAX_SUBCARRIERS     64     // we report at most this many subcarriers
#define BASELINE_ALPHA      0.01f  // EWMA rate for the empty-room baseline (slow)
#define MOTION_GAIN         6.0f   // scales raw deviation -> 0..255 motion metric
#define MOTION_EMA_ALPHA    0.25f  // smoothing for the reported motion value
#define PRESENCE_EMA_ALPHA  0.05f  // slower EMA used for the presence decision
#define DEFAULT_MOTION_THRESHOLD 40 // motion-flag threshold (0..255)
#define PRESENCE_THRESHOLD  18     // presence decision threshold on the slow EMA
#define CALIBRATION_SAMPLES 60     // samples to establish baseline before reporting

// ---------------------------------------------------------------------------
// Breathing estimator (EXPERIMENTAL). Looks for a 0.1-0.5 Hz (6-30 bpm)
// periodicity in the mean CSI amplitude while the subject is still.
// ---------------------------------------------------------------------------
#define BREATH_WINDOW       256    // samples held for breathing analysis
#define BREATH_MIN_BPM      6
#define BREATH_MAX_BPM      30

// ---------------------------------------------------------------------------
// BLE
// ---------------------------------------------------------------------------
#define BLE_DEVICE_NAME     "FlipperCSI"
#define STATUS_PERIOD_MS    1000   // how often the Status JSON is refreshed/notified

// ---------------------------------------------------------------------------
// Protocol (must match PROTOCOL.md and the Android app)
// ---------------------------------------------------------------------------
#define PROTO_MAGIC     0xC5
#define PROTO_VERSION   1

#define SVC_UUID        "c5510000-b0b0-4a5a-9e10-000000000001"
#define CHR_CSI_UUID    "c5510001-b0b0-4a5a-9e10-000000000001"
#define CHR_CTRL_UUID   "c5510002-b0b0-4a5a-9e10-000000000001"
#define CHR_STATUS_UUID "c5510003-b0b0-4a5a-9e10-000000000001"
