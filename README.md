# Flipper CSI Sense — Firmware (public)

ESP32 firmware for the Flipper Zero 3-in-1 Multi Board: Wi-Fi **CSI human
sensing** (presence / motion / experimental breathing) streamed over BLE, plus
**WiFi OTA** so the companion app can flash updates wirelessly.

> This repo is **public** on purpose: GitHub Actions builds the firmware and
> publishes the `.bin` files as **Release assets**, which the Android companion
> app downloads over the air. (Release assets are downloadable without auth only
> from public repos.) The companion **app** lives in a separate private repo.

## Contents

```
firmware/                ESP32 firmware (PlatformIO/Arduino)
  src/                   CSI capture + motion detection + BLE + WiFi OTA
  marauder-integration/  How to add this as a menu item in ESP32 Marauder
.github/workflows/       CI: build standalone .bin + build Marauder+CSI .bin
PROTOCOL.md              BLE protocol shared with the app
docs/FEASIBILITY.md      What this hardware can / cannot do (honest)
docs/UPDATING.md         OTA update pipeline (CI -> release -> app -> board)
```

## Build / flash

See [`firmware/README.md`](firmware/README.md). TL;DR: set WiFi creds in
`firmware/src/config.h`, then `pio run -d firmware -t upload`.

## Releases

Pushing a tag `v*` (e.g. `v0.1.0`) runs `.github/workflows/build-standalone.yml`,
which compiles the firmware and attaches **`flipper-csi-sense.bin`** to a GitHub
Release. The app pulls the latest release asset for OTA. The Marauder+CSI build
is a manual (`workflow_dispatch`) job — see `docs/UPDATING.md`.
