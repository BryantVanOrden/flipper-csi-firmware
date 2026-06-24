# In-app firmware updating (top board)

This is the end-to-end pipeline for updating the **ESP32 top board** from the
Android app — built around **WiFi OTA**. (Updating the *main Flipper Zero* with
Momentum is a separate, much larger effort over Flipper's BLE RPC and is **not**
included here.)

## The flow

```
GitHub Actions builds .bin  ──►  GitHub Release asset
                                        │
                          app downloads latest .bin (Internet)
                                        │
        app POSTs .bin ──► http://<board-ip>/update  (same WiFi)
                                        │
                            ESP32 Update lib flashes + reboots
```

Two firmware products are published as release assets and selectable in the app:

| Asset name contains | What it is |
|---|---|
| `flipper-csi-sense` | Our standalone firmware (CSI + BLE + OTA). Reliable, smallest. |
| `marauder-csi` | Full ESP32 Marauder with the CSI Sense menu + OTA baked in. |

## One-time setup

### 1. First flash (USB) — the chicken-and-egg step
OTA can only push *new* firmware once an **OTA-capable** firmware is already on
the board. So the very first flash must be done over USB (or via the Flipper's
**ESP Flasher** app over UART):

```
pio run -d firmware -t upload
```

After that, every future update can be wireless from the app.

### 2. Point the app and CI at your GitHub repo
- Push this project to a GitHub repo.
- In `android/app/.../net/UpdateConfig.kt`, set `GITHUB_OWNER` and `GITHUB_REPO`
  to that repo. The app reads the repo's **latest release** and downloads the
  asset matching each target.

### 3. CI: building the .bin files
Two workflows live in `.github/workflows/`:

- **`build-standalone.yml`** — fully automatic. On a tag push (`git tag v1.0 &&
  git push --tags`) it builds `firmware/` and attaches `flipper-csi-sense.bin`
  to the release. Nothing else needed.
- **`build-marauder-csi.yml`** — builds Marauder + our changes. Because Marauder
  is a separate project, you:
  1. **Fork** `justcallmekoko/ESP32Marauder`.
  2. Apply the menu/scan/loop edits from
     [`../firmware/marauder-integration/README.md`](../firmware/marauder-integration/README.md)
     (steps 2–5) to your fork and commit them on a branch (e.g. `csi-sense`).
     The CSI *module files* themselves are copied in automatically by CI, so you
     only commit the small wiring edits.
  3. Run the workflow (Actions tab → "Build Marauder + CSI" → Run), setting
     `marauder_repo` to your fork, `marauder_ref` to your branch, and `pio_env`
     to the Marauder PlatformIO env you build. It publishes `marauder-csi.bin`.

> Tag both builds onto the **same release** so the app sees both assets at once.

## Updating from the app

1. Put the board in **Active mode** (it joins your WiFi). The status shows an
   `ip` and `ota:true`. Make sure your **phone is on the same WiFi**.
2. App → **Update board**. It shows the board IP and the two firmware choices.
3. Tap **Flash**. The app downloads the latest `.bin` and streams it to the
   board; the board flashes and reboots. Progress is shown for both phases.

## Safety notes

- The OTA endpoint is **unauthenticated on your LAN**. Anyone on the same WiFi
  could POST firmware. Fine for a home/lab network; add a token check in
  `ota.cpp` if you care.
- A failed OTA does **not** brick the ESP32 — the Update library only switches to
  the new image after a fully verified write, and you can always re-flash over
  USB.
- Don't update while the board is mid-task you care about; it reboots on success.

## What's intentionally NOT here

Updating the **main Flipper Zero** (Momentum/OFW) from this app. That requires
reimplementing Flipper's BLE protobuf RPC + update orchestration (transfer the
update bundle to the SD card, trigger update mode). It's a large, separate, and
bricking-sensitive effort. Use **qFlipper** or the **official Flipper app** for
the Flipper itself for now; ask if you want to take that on as its own project.
