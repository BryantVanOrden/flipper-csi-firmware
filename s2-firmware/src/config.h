// config.h - Flipper CSI Sense (ESP32-S2 / UART transport build)
//
// Target: official Flipper Zero WiFi Dev Board (ESP32-S2, no Bluetooth).
// CSI frames are streamed over the hardware UART (UART0 -> Flipper header), and
// the Flipper's "CSI BLE Bridge" FAP relays them to the phone over the Flipper's
// own Bluetooth. The S2 has no BT, so there is NO on-board BLE here.
#pragma once

// ---- WiFi (active CSI mode joins this network + pings the gateway) ----
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
#define DEFAULT_MODE        1      // 0 passive sniff, 1 active (STA + ping)
#define DEFAULT_CHANNEL     6      // passive-mode channel
#define PING_INTERVAL_MS    50

// ---- CSI / detection (see csi_sense.cpp) ----
#define MAX_SUBCARRIERS     64
#define BASELINE_ALPHA      0.01f
#define MOTION_GAIN         6.0f
#define MOTION_EMA_ALPHA    0.25f
#define PRESENCE_EMA_ALPHA  0.05f
#define DEFAULT_MOTION_THRESHOLD 40
#define PRESENCE_THRESHOLD  18
#define CALIBRATION_SAMPLES 60
#define BREATH_WINDOW       256
#define BREATH_MIN_BPM      6
#define BREATH_MAX_BPM      30

// ---- UART transport to the Flipper ----
// Marauder/the Flipper dev board talk over UART0 at 115200. The FAP reads the
// matching Flipper pins (official Dev Board -> USART 13/14).
#define UART_BAUD           115200
#define STATUS_PERIOD_MS    1000

// ---- Wire framing (the FAP forwards raw bytes; the phone demuxes by magic) ----
#define PROTO_VERSION   1
#define CSI_MAGIC       0xC5   // ESP32 -> phone: CSI data frame
#define STATUS_MAGIC    0xC6   // ESP32 -> phone: status JSON frame
#define CTRL_MAGIC      0xC7   // phone -> ESP32: control frame [0xC7, cmd, len, payload[len]]

// Control commands
#define CMD_THRESHOLD   0x01   // payload[0] = motion threshold 0..255
#define CMD_MODE        0x02   // payload[0] = 0 passive / 1 active
#define CMD_RECALIBRATE 0x03   // no payload
#define CMD_CHANNEL     0x04   // payload[0] = channel 1..13
#define CMD_SET_SSID    0x10   // payload = WiFi SSID string
#define CMD_SET_PASS    0x11   // payload = WiFi password string
#define CMD_CONNECT     0x12   // no payload: apply creds + (re)connect in active mode
