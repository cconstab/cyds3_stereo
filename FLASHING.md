# Flashing & First-Run Guide

## What you need

- Freenove FNK0104S connected via a **data-capable** USB-C cable
- Either: any Chrome/Edge browser (easiest), or PlatformIO CLI (for development)

## Option A — Browser flashing (no toolchain needed)

1. Start the update server:
   ```bash
   cd update-server
   UPLOAD_TOKEN=yoursecret docker compose up -d
   ```
2. Open `http://<server>:8080`, upload both build artifacts
   (`firmware.bin` and `firmware-merged.bin`) with a version number.
3. Open `http://<server>:8080/flash.html` in Chrome/Edge, plug in the board, click
   **Connect**, pick the serial port, and install.
   - If the port doesn't appear: hold **BOOT**, tap **RESET**, release **BOOT**, retry.

## Option B — PlatformIO (development)

```bash
cd firmware
pio run -t upload        # builds and flashes over USB
pio device monitor       # serial console (115200)
```

Build artifacts land in `firmware/.pio/build/fnk0104s/`:
- `firmware.bin` — app image, used for OTA updates
- `firmware-merged.bin` — full image (bootloader+partitions+app), used for USB/browser flashing

## First boot & provisioning

1. On first boot the device has no WiFi credentials, so it starts an access point
   named **CYD-Radio-xxxx** and the screen shows setup instructions.
2. Join that AP from a phone/laptop; a captive portal opens (or browse to `192.168.4.1`).
3. Enter WiFi SSID/password, station name, and the stream URLs (one per line, priority
   order). Set the update server URL (e.g. `http://192.168.1.50:8080`). Save.
4. The device reboots, connects, and starts playing the first reachable stream.

After that, the same config page is available anytime at `http://<device-ip>/`
(the IP is shown in the status line on the display).

## OTA updates from then on

- Devices poll the update server (default: hourly) and install newer versions
  automatically, verified by SHA-256, with automatic rollback if a bad build
  fails to boot 3 times.
- Publish a new version with one command:
  ```bash
  cd firmware
  UPLOAD_TOKEN=yoursecret ./scripts/release.sh 0.2.0 http://<server>:8080 "release notes"
  ```
- Or upload the two `.bin` files manually on the server dashboard. The dashboard also
  shows every device's last check-in and firmware version, and can roll the published
  version back with one click.

## On-device smoke test checklist (first hardware bring-up)

1. Screen lights up, UI renders, touch responds (tap the gear icon).
2. Serial monitor shows `[net] online`, then `[player] connecting to URL 1/…`.
3. Audio: VU meters move; sound from speakers/RCA (external I2S boards wired per
   [HARDWARE.md](HARDWARE.md)).
4. Kill the active stream URL (or WiFi) — player should fail over / recover on its own.
5. `http://<device-ip>/` loads the web config; volume slider works live.
6. Settings → "Check for update" reports "up to date" (or installs a newer published build).
