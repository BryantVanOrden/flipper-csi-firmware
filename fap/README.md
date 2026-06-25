# CSI BLE Bridge — Flipper Zero app (FAP)

A Flipper Zero app that bridges the **ESP32‑S2's UART** (Wi‑Fi CSI frames) to the
**phone over the Flipper's own Bluetooth**. This is how we get a *connectable*
BLE link off an ESP32‑S2 board (the S2 has no Bluetooth — the Flipper does).

```
ESP32-S2 ──UART──► Flipper (this app) ──BLE serial──► phone app
 phone app ──BLE──► Flipper (this app) ──UART──► ESP32-S2   (control bytes)
```

The bridge is **byte‑transparent** — it doesn't parse CSI. The phone reassembles
frames from the stream using the `0xC5` magic + length (see `../PROTOCOL.md`).

## How the phone connects
The phone connects to the **Flipper's own BLE serial service** (not the ESP32):
- Service `8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000`
- Notify (Flipper→phone) `19ed82ae-ed21-4c9d-4145-228e62fe0000`
- Write  (phone→Flipper) `19ed82ae-ed21-4c9d-4145-228e61fe0000`

So the Android app scans for **"Flipper …"** and talks this service — different
from the direct‑to‑ESP32 BLE design.

## Build & install (ufbt)
```
pip install ufbt
ufbt          # builds csi_bridge.fap against your firmware's SDK
ufbt launch   # build + install + run on a connected Flipper
```
`ufbt` pulls the SDK for the firmware on your Flipper (Momentum API 86). Or build
in CI with `flipperdevices/flipperzero-ufbt-action` and copy the `.fap` to
`/ext/apps/GPIO/` on the SD.

## Pins
Set `CSI_UART_ID` in `csi_bridge.h` to the UART your board's ESP32 uses:
- `FuriHalSerialIdUsart` = pins **13(TX)/14(RX)**
- `FuriHalSerialIdLpuart` = pins **15(TX)/16(RX)** ← current default here

Use the pin set that answered when you bridged the ESP32 over USB‑UART. Baud is
`115200` to match Marauder/our firmware.

## Using it
1. Flipper: **Settings → Bluetooth → ON** (the app uses the Flipper's BLE).
2. Run **Apps → GPIO → CSI BLE Bridge**. It hijacks the serial‑over‑BLE and
   advertises; the screen shows connection + byte counters.
3. Open the phone app, connect to the **Flipper**, watch CSI stream in.

## ⚠️ Needs on‑device validation (no SDK/hardware here to compile)
- **API drift:** `furi_hal_bt_serial_*`, `furi_hal_serial_*`, and
  `bt_set_status_changed_callback` are firmware‑version sensitive. If `ufbt`
  errors, the symbol names/signatures may differ slightly in your SDK — adjust.
  The BLE pattern is taken from the working `maybe-hello-world/fbs` example.
- **UART pins:** confirm `CSI_UART_ID` matches your board (see above).
- **Frame reassembly:** because BLE serial is a byte stream (not framed like a
  GATT notify), the phone app must buffer and resync on the `0xC5` magic — that's
  a change in the Android BLE layer for this bridge mode.
- The Flipper must have **Bluetooth enabled** and **not be mid‑RPC** with another
  app (this app takes over the serial profile while running).
