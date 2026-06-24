# Flipper CSI Sense — BLE Protocol (v1)

This is the contract shared by the ESP32 firmware (`firmware/`) and the Android
companion app (`android/`). **Do not change one side without the other.**

The ESP32 advertises as `FlipperCSI` and exposes one GATT service.

## UUIDs

| Item | UUID |
|------|------|
| Service | `c5510000-b0b0-4a5a-9e10-000000000001` |
| CSI Data (Notify) | `c5510001-b0b0-4a5a-9e10-000000000001` |
| Control (Write) | `c5510002-b0b0-4a5a-9e10-000000000001` |
| Status (Read / Notify) | `c5510003-b0b0-4a5a-9e10-000000000001` |

The central (phone) should request an MTU of **at least 128 bytes** after
connecting so a full CSI frame fits in one notification.

## CSI Data frame (Notify characteristic, binary, little-endian)

Sent on every processed CSI sample (target ~10–20 Hz).

```
offset  type     name            meaning
0       uint8    magic           always 0xC5
1       uint8    version         protocol version, currently 1
2       uint8    n_sub           number of subcarrier amplitudes that follow
3       uint8    flags           bit0 presence, bit1 motion, bit2 active_mode
4       int8     rssi            packet RSSI in dBm
5       uint8    motion          normalized motion metric 0..255
6       uint8    breathing_bpm   estimated breaths/min, 0 = unknown (experimental)
7       uint8    seq             rolling 0..255 counter (gap = dropped notify)
8..8+n_sub-1  uint8[]  amplitude per subcarrier, normalized 0..255
```

Total length = `8 + n_sub` bytes. Typical `n_sub` = 64 → 72 bytes.

Parsing rules:
- Drop any frame whose `magic != 0xC5` or `version != 1`.
- `presence` = a person is detected in range. `motion` flag = active movement.
- `motion` byte is the continuous metric used to drive the gauge; the `motion`
  flag is just `motion byte > threshold`.

## Control frame (Write characteristic, binary)

```
byte0   command
byte1+  argument(s)

0x01  set_motion_threshold   byte1 = threshold 0..255
0x02  set_mode               byte1 = 0 passive, 1 active(STA+ping)
0x03  reset_baseline         (no args) — recalibrate the empty-room baseline
0x04  set_channel            byte1 = wifi channel 1..13 (passive mode only)
```

## Status frame (Read / Notify characteristic, ASCII JSON)

A short JSON string the app can read or subscribe to for diagnostics:

```json
{"mode":"active","ch":6,"rate":18.2,"rssi":-47,"sub":64,"cal":true,"ip":"192.168.1.42","ota":true}
```

- `mode`: `"active"` or `"passive"`
- `ch`: current wifi channel
- `rate`: CSI samples/sec actually being produced
- `rssi`: last packet RSSI
- `sub`: number of subcarriers
- `cal`: true once the empty-room baseline is established
- `ip`: board's IP address on the WiFi (empty `""` if not connected) — the app
  POSTs OTA firmware to `http://<ip>/update`
- `ota`: true once the OTA web server is running (only in active mode)
