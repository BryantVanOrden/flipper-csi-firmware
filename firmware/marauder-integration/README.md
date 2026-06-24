# Adding "WiFi CSI Sense" as an ESP32 Marauder menu item

This folds the CSI sensing feature into justcallmekoko's **ESP32 Marauder** so it
appears as its own menu entry (e.g. under **WiFi**), launching CSI capture +
BLE streaming. While CSI runs, normal Marauder WiFi scans/attacks are paused —
it's a separate mode that owns the radio, exactly like Marauder's other modes.

> **Reality check / recommendation.** Marauder's menu and scan code changes
> between releases, so a blind patch can't be guaranteed to match your exact
> version. The robust path is: flash the **standalone** firmware in `../` first
> to confirm sensing + BLE work, then apply this integration to your Marauder
> checkout and adjust the snippets to match your version's code. Pin your
> Marauder source to a known commit before editing.

## Prerequisites

- A local clone of the Marauder source you build from (PlatformIO/Arduino),
  folder `esp32_marauder/`.
- The **NimBLE-Arduino** library available to that build (Marauder already uses
  NimBLE in recent versions; if not, add it to `platformio.ini` `lib_deps`).

## Step 1 — Drop in the CSI module

Copy these from the standalone firmware into the Marauder `esp32_marauder/`
source folder (rename to avoid clashes if needed):

```
../src/csi_sense.h     -> esp32_marauder/csi_sense.h
../src/csi_sense.cpp   -> esp32_marauder/csi_sense.cpp
../src/ble_link.h      -> esp32_marauder/csi_ble_link.h
../src/ble_link.cpp    -> esp32_marauder/csi_ble_link.cpp
../src/ota.h           -> esp32_marauder/csi_ota.h
../src/ota.cpp         -> esp32_marauder/csi_ota.cpp
../src/config.h        -> esp32_marauder/csi_config.h
```

Update the `#include "config.h"` lines in the copied files to
`#include "csi_config.h"`, the ble_link include to `csi_ble_link.h`, and the ota
include to `csi_ota.h`, so they don't collide with Marauder's own headers. Set
your Wi-Fi creds in `csi_config.h`.

> The `build-marauder-csi.yml` CI workflow does this copy + include-rename for you
> on every build — see [`docs/UPDATING.md`](../../docs/UPDATING.md). You only need
> to commit the menu/scan/loop edits (steps 2–5) to your Marauder fork once.

## Step 2 — Add a scan-mode constant

In **`WiFiScan.h`**, alongside the other `#define WIFI_SCAN_*` constants, add a
new unique value (pick a number not already used):

```cpp
#define WIFI_SCAN_CSI_SENSE 200
```

## Step 3 — Start/stop CSI from WiFiScan

In **`WiFiScan.cpp`**:

```cpp
#include "csi_sense.h"
#include "csi_ble_link.h"

// --- in StartScan(uint8_t scan_mode, uint16_t color) ---
else if (scan_mode == WIFI_SCAN_CSI_SENSE) {
    static bool csi_ble_started = false;
    if (!csi_ble_started) {                 // BLE server only needs starting once
        BleLink::begin([](uint8_t cmd, uint8_t arg, bool hasArg){
            switch (cmd) {
                case 0x01: if (hasArg) CsiSense::setMotionThreshold(arg); break;
                case 0x02: if (hasArg) CsiSense::setMode(arg);            break;
                case 0x03: CsiSense::resetBaseline();                     break;
                case 0x04: if (hasArg) CsiSense::setChannel(arg);         break;
            }
        });
        csi_ble_started = true;
    }
    CsiSense::begin(DEFAULT_MODE, DEFAULT_CHANNEL);
    this->wifi_initialized = true;          // match how other modes set their flags
}

// --- in StopScan(uint8_t scan_mode) ---
else if (scan_mode == WIFI_SCAN_CSI_SENSE) {
    // optional: park the radio; leaving CSI running is also fine
}
```

> Note: Marauder initializes WiFi for its own modes. CSI's `begin()` reconfigures
> WiFi (STA+ping or promiscuous) and registers the CSI callback, so call it after
> Marauder hands the radio to this mode. If your version centralizes
> `esp_wifi_init`, you can drop CSI's own WiFi bring-up and just call
> `enableCsi()` — see comments in `csi_sense.cpp`.

## Step 4 — Service it in the WiFiScan main loop

In **`WiFiScan::main(uint32_t currentTime)`** add a branch so CSI is polled and
streamed each loop:

```cpp
#include "csi_ota.h"   // at the top of WiFiScan.cpp

else if (currentScanMode == WIFI_SCAN_CSI_SENSE) {
    // Bring up the OTA web server once WiFi associates (active mode), so the
    // Android app can flash future updates wirelessly.
    if (!Ota::running() && WiFi.status() == WL_CONNECTED) Ota::begin();
    Ota::loop();

    CsiResult r;
    if (CsiSense::poll(r)) {
        BleLink::notifyCsi(r);
        // Optional: draw a tiny status on Marauder's display here using its
        // existing `display_obj` helpers (presence + motion bar).
    }
    static uint32_t lastStatus = 0;
    if (currentTime - lastStatus > 1000) {
        lastStatus = currentTime;
        char json[128];
        snprintf(json, sizeof(json),
          "{\"mode\":\"%s\",\"ch\":%u,\"rate\":%.1f,\"rssi\":%d,\"sub\":%u,\"cal\":%s}",
          CsiSense::mode()==1?"active":"passive", CsiSense::channel(),
          CsiSense::sampleRate(), CsiSense::lastRssi(), CsiSense::subcarriers(),
          CsiSense::calibrated()?"true":"false");
        BleLink::updateStatus(json);
    }
}
```

## Step 5 — Add the menu entry

In **`MenuFunctions.cpp`**, where the WiFi submenu nodes are built (look for the
`addNodes(&wifiMenu, ...)` calls), add:

```cpp
addNodes(&wifiMenu, "WiFi CSI Sense", TFT_CYAN, NULL, SNIFF,
    [this]() {
        this->wifi_scan_obj.StartScan(WIFI_SCAN_CSI_SENSE, TFT_CYAN);
        // mirror how neighboring items switch the display to a scan view
    });
```

Match the exact `addNodes` signature your version uses (icon arg, lambda capture
style). Some versions use `[](){ ... }` with a global `wifi_scan_obj`.

## Step 6 — Build & flash

Build Marauder as usual (`pio run -t upload`). On the device: **WiFi → WiFi CSI
Sense**. The board starts advertising as **FlipperCSI**; connect the Android app.

## Gotchas

- **WiFi + BLE coexistence:** the classic ESP32 time-shares one 2.4 GHz radio.
  CSI at ~10–20 Hz + BLE notify is fine, but expect lower throughput than either
  alone — don't run Marauder attacks simultaneously.
- **Flash/RAM:** Marauder is already large; adding NimBLE + CSI may push flash
  usage. Use a partition scheme with enough app space (`huge_app` /
  `min_spiffs`) if the build overflows.
- **Display:** wiring CSI into Marauder's `display_obj` is optional and
  version-specific; the phone app is the intended display.
