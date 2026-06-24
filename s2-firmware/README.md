# Flipper CSI Sense — ESP32-S2 firmware (UART → Flipper → phone)

For the **official Flipper Zero WiFi Dev Board (ESP32-S2)**. The S2 has **no
Bluetooth**, so this build does **not** use BLE. Instead it streams Wi-Fi CSI over
the **UART** to the Flipper, and the Flipper's **`CSI BLE Bridge` FAP** relays it
to the phone over the **Flipper's own Bluetooth**.

```
ESP32-S2 (this fw) ──UART0 115200──► Flipper (CSI BLE Bridge FAP) ──BLE──► phone app
```

## Files
| File | Role |
|------|------|
| `src/csi_sense.*` | CSI capture + motion/presence/breathing (same engine as the BLE build; works on S2) |
| `src/uart_link.*` | Frames CSI/status to UART; parses control frames from the phone |
| `src/main.cpp` | Glue |
| `src/config.h` | WiFi creds, CSI params, UART baud, wire magics |

## Wire framing (the FAP forwards raw bytes; the phone demuxes by magic)
- **ESP32 → phone**
  - CSI data:  `0xC5, ver, n_sub, flags, rssi(int8), motion, bpm, seq, amp[n_sub]`
  - Status:    `0xC6, len, <json bytes>`  e.g. `{"mode":"active","ch":6,...}`
- **phone → ESP32**
  - Control:   `0xC7, cmd, arg`  (cmd 0x01 threshold, 0x02 mode, 0x03 recalibrate, 0x04 channel)

The phone keeps a small reassembly buffer and resyncs on the magic bytes (the
BLE serial stream isn't framed like a GATT notify).

## Build & flash
1. Set `WIFI_SSID` / `WIFI_PASSWORD` in `src/config.h` (used by active CSI mode).
2. **Bootloader mode:** hold the dev board's **BOOT** button, plug in **USB-C**,
   then:
   ```
   pio run -t upload
   ```
   (The S2's USB only appears in bootloader mode — that's normal.)

## Using it
1. Flash this firmware to the dev board; plug the board into the Flipper.
2. On the Flipper: enable **Bluetooth**, then run **Apps → GPIO → CSI BLE Bridge**
   (`../flipper-csi-bridge`). Set its UART to **USART (pins 13/14)** for this board.
3. Open the phone app, connect to the **Flipper**, watch CSI stream in.

> Same UART output is used by the Marauder+CSI build for the Predator board — so
> the **one FAP + one phone app work with either board**.
