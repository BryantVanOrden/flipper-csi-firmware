# Flipper CSI Sense — ESP32 firmware

Standalone firmware for the ESP32 on the Flipper Multi Board. Captures Wi-Fi
CSI, detects presence/motion/breathing, and streams it over BLE to the Android
app. Validate this standalone first; then optionally fold it into ESP32 Marauder
via [`marauder-integration/`](marauder-integration/).

## Files

| File | Role |
|------|------|
| `src/config.h` | Wi-Fi creds, mode, thresholds, BLE UUIDs, optional TFT pins |
| `src/csi_sense.*` | CSI capture + baseline + motion/presence/breathing |
| `src/ble_link.*` | NimBLE GATT server (CSI notify, control write, status) |
| `src/main.cpp` | Glue: poll CSI → BLE notify → (optional) TFT |
| `platformio.ini` | Build config |

## Build & flash (PlatformIO)

1. Install [PlatformIO](https://platformio.org/) (VS Code extension or `pip install platformio`).
2. Edit `src/config.h`: set `WIFI_SSID` / `WIFI_PASSWORD` (used by **active** mode).
3. Plug the board in, put the ESP32 in flash mode if needed, then:
   ```
   pio run -t upload
   pio device monitor -b 115200
   ```
4. The serial log should show CSI starting and `advertising as FlipperCSI`.

> Arduino IDE alternative: copy `src/*` into a sketch folder, install the
> **NimBLE-Arduino** library, select an ESP32 Dev Module board, and upload.

## Active vs passive mode

- **Active (default, recommended):** the ESP32 joins your Wi-Fi and pings the
  gateway every `PING_INTERVAL_MS`. The replies give a steady CSI stream — best
  for motion and breathing. Requires correct Wi-Fi credentials.
- **Passive:** promiscuous sniff on a fixed channel (`DEFAULT_CHANNEL`), no
  association. Works anywhere but the CSI rate depends on ambient traffic.

Switch at runtime from the app (mode toggle) or change `DEFAULT_MODE`.

## Using it

1. Power the board. Stay out of the area for a few seconds — it calibrates an
   empty-room baseline (`cal` flips to `true` in the status).
2. Open the Android app, connect to **FlipperCSI**, watch the waterfall + motion
   gauge. Walk near/behind the wall; you should see motion rise and presence
   trip.
3. Tune `MOTION_GAIN` / `DEFAULT_MOTION_THRESHOLD` in `config.h` (or the app
   slider) for your environment. Use **Recalibrate** after moving the board.

## Tuning notes

- **Too sensitive / always "presence":** raise the threshold, lower `MOTION_GAIN`,
  recalibrate with the room empty.
- **Not sensitive enough:** raise `MOTION_GAIN`, lower the threshold, prefer
  active mode, place the board so the wall/person is between it and the AP.
- **Breathing reads 0:** expected unless the subject is still and close; it needs
  a steady ≥4 Hz CSI rate and is explicitly experimental.

## Optional on-board TFT

The phone is the real display, but you can show a minimal status line on the
board's TFT: uncomment `-DUSE_TFT=1` in `platformio.ini`, add the `TFT_eSPI`
lib, and set the correct pins for your board (defaults given are for a common
ILI9341 Marauder Multi Board wiring — verify against your unit).
