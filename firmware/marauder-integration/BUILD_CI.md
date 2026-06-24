# Where the Marauder + CSI build runs

Marauder is an **Arduino-IDE** project (not PlatformIO), so its build does **not**
run in this repo. The working CI lives in the **Marauder fork**:

- Fork: **https://github.com/BryantVanOrden/ESP32Marauder**
- Branch: **`csi-sense`** — has the CSI modules + the menu/scan integration
  (`WiFi → Scanners → WiFi CSI Sense`).
- Workflow: **`build_s3.yml`** (`workflow_dispatch`) — builds the **Flipper Multi
  Board S3** target with the proven upstream recipe (ESP32 core `2.0.11`,
  NimBLE `1.3.8`) and uploads the `.bin` artifact. ✅ builds green.

Run it:

```
gh workflow run build_s3.yml -R BryantVanOrden/ESP32Marauder --ref csi-sense
```

To make this OTA-installable from the app, publish the fork's built `.bin` as a
`marauder-csi*.bin` **release asset in this public repo** (the app pulls release
assets here, not fork artifacts). That cross-repo publish step is the remaining
piece if you want OTA for the Marauder build (the standalone firmware already
has full OTA).
