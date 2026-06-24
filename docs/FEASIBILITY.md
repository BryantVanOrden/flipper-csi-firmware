# Feasibility: what this hardware can and cannot do

## Your hardware (from the photos)

- **Flipper Zero FZ.1** + **3-in-1 Multi Board** top: ESP32 (dual 2.4 GHz
  antennas) + GPS + CC1101 (433 MHz), color TFT, running **ESP32 Marauder**.
- The "top ESP" you want to modify = the **ESP32 on the Multi Board**.

## What the ESP32 can actually sense with CSI

CSI = per-packet complex gain of each OFDM subcarrier. On the ESP32:

- 2.4 GHz, 20 MHz, **~64 subcarriers, 1 RX antenna**, int8 I/Q per subcarrier.
- Achievable, well-documented results:
  - **Presence** ("someone is in the room / behind the wall")
  - **Motion / walking detection**
  - **Coarse breathing rate** when the subject is still and close (experimental)
- The output is amplitude-change "energy," visualized as a **waterfall heatmap**
  + **motion gauge** — not a skeleton or image.

## What it cannot do (and why)

| MM-Fi requires | ESP32 reality |
|---|---|
| TP-Link N750, 5 GHz, 40 MHz, **114 subcarriers, 3×3 antennas** | 1 antenna, ~64 subcarriers, 20 MHz |
| Training data **synced with camera/LiDAR/mmWave** | CSI only |
| **GPU + PyTorch** to train & run 3D-pose nets | ~520 KB RAM MCU |
| 17-keypoint 3D skeletons | not possible on-device |

MM-Fi's pretrained models are trained on TP-Link CSI in a totally different
shape and will not accept ESP32 CSI. "See-through-wall" body imaging in viral
demos uses mmWave radar or router arrays + a GPU. We deliberately do not fake it.

## Two practical notes about CSI on this board

1. **You need steady packets.** CSI is only computed on *received* packets.
   - **Active mode (recommended):** ESP32 joins your Wi-Fi as a station and
     pings the gateway at a fixed rate; the ACKs/replies give a steady CSI stream
     — best for motion/breathing.
   - **Passive mode:** promiscuous capture of ambient traffic on a locked channel
     — no association needed, but irregular sample rate.
2. **CSI vs Marauder.** Marauder owns the Wi-Fi radio in its scan modes. The CSI
   feature is its own mode; while it runs, normal Marauder Wi-Fi attacks/scans
   are paused. The integration adds it as a separate menu item, not something
   that runs concurrently with sniffing.
