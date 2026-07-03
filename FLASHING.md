# Flashing & First-Run Guide

## What you need

- Freenove FNK0104S connected via a **data-capable** USB-C cable
- Either: any Chrome/Edge browser (easiest), or PlatformIO CLI (for development)

## Option A — Standalone web installer (no toolchain, no server state)

A self-contained Docker image that serves a "flash your device" page with the firmware
baked in — ideal to hand to anyone who just needs to bring up a new unit.

```bash
cd web-installer
./build.sh                    # stages the current firmware build into the image
docker run -d --rm -p 8090:80 cyds3-web-installer
```

Open `http://localhost:8090` in Chrome/Edge **on the machine the board is plugged into**,
click **Connect**, pick the serial port, install.

> Web Serial requires a secure context: `http://localhost` works as-is; if you host this
> for others, put it behind HTTPS (any reverse proxy with a real cert).

Rebuild the image (`./build.sh`) whenever you cut a new firmware release you want it to carry.

## Option B — Browser flashing from the update server

The fleet update server also serves a flasher at `http://<server>:8080/flash.html`, using
whatever version is currently published on its dashboard (upload must include the merged
image). Same browser rules as above. If the port doesn't appear: hold **BOOT**, tap
**RESET**, release **BOOT**, retry.

## Option C — PlatformIO (development)

```bash
cd firmware
pio run -t upload        # builds and flashes the default variant (fnk0104b, 2.8")
pio device monitor       # serial console (115200)

pio run -e fnk0104s -t upload   # 4.0" variant instead
```

Two board variants are supported — pick the env that matches the panel:

| Env | Board | Panel | OTA device channel |
|---|---|---|---|
| `fnk0104b` (default) | FNK0104B 2.8" | ILI9341 240x320 | `cyds3-stereo` |
| `fnk0104s` | FNK0104S 4.0" | ST7796 320x480 | `cyds3-stereo-s` |

The two builds are kept on separate update-server channels so a mixed fleet can never
cross-flash the wrong panel driver. `release.sh` takes the env as its 4th argument, and
`web-installer/build.sh` takes it as its 1st.

Build artifacts land in `firmware/.pio/build/<env>/`:
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
