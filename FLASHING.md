# Flashing & First-Run Guide

## What you need

- A Freenove FNK0104 board (2.8" **FNK0104B** or 4.0" **FNK0104S**) connected via a
  **data-capable** USB-C cable
- Either: any Chrome/Edge browser (easiest), or PlatformIO CLI (for development)

## Option A — Standalone web installer (no toolchain, no server state)

A self-contained Docker image that serves a "flash your device" page with the firmware
baked in — ideal to hand to anyone who just needs to bring up a new unit.

```bash
cd web-installer
./build.sh fnk0104b           # stages the current firmware build (2.8" variant) into the image
docker run -d --rm -p 8090:80 cyds3-web-installer
```

Open `http://localhost:8090` in Chrome/Edge **on the machine the board is plugged into**,
click **Connect**, pick the serial port, install.

> Web Serial requires a secure context: `http://localhost` works as-is; if you host this
> for others, put it behind HTTPS (any reverse proxy with a real cert).

Rebuild the image (`./build.sh <env>`) whenever you cut a new firmware release you want it to carry.

## Option B — Browser flashing from the update server

The fleet update server also serves a flasher at `http://<server>:8080/flash.html`, using
whatever version is currently published on its dashboard (upload must include the merged
image). Same browser rules as above.

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

## Troubleshooting: recovery & factory reset

### Board won't connect / "Failed to connect: No serial data received"

If the running firmware has crashed or hung, the auto-reset into the bootloader over
native USB doesn't work — the crashed firmware isn't servicing USB. Force the ROM
bootloader manually:

1. **Hold BOOT** (IO0), **tap RESET**, then **release BOOT**. Screen stays dark — the
   chip is now in download mode (the port may re-enumerate; esptool re-detects it).
2. Run the erase/upload — it will connect within a second or two.
3. **Press RESET once when flashing finishes** — after a manual-bootloader flash the
   chip stays in download mode until you reset it; it won't reboot into the app itself.

The same BOOT/RESET dance fixes the browser flasher when no port appears.

### Factory reset (full flash erase)

SRAM is volatile — a power cycle clears it. What persists between flashes lives in
flash: the config filesystem (LittleFS), NVS (OTA boot counter), and the OTA slot
selector. A normal upload deliberately leaves all of those alone. To wipe everything:

```bash
cd firmware
pio run -e fnk0104b -t erase    # erases the entire 16MB flash
pio run -t upload               # then flash fresh
```

After an erase the device has no config, so it boots into the provisioning hotspot
(below). If the erase can't connect, use the BOOT/RESET procedure above first.

### Blank screen after flashing

1. Reflash the latest build — don't assume the flash "took" if the upload had errors.
2. Full erase + flash (above) to rule out stale config/partition state.
3. Still blank: capture the boot log (`pio device monitor`, press RESET, copy
   everything) — the last line before output stops identifies the failing init step.
4. Display bring-up diagnostics (color-cycle self-test + I2C probe) are available by
   enabling **"Display self-test at power-on"** in the web config, or setting
   `"bootSelfTest": true` in the config JSON.

## First boot & provisioning

1. On first boot the device has no WiFi credentials, so it starts an access point
   named **CYD-Radio-xxxx** and the screen shows setup instructions.
2. Join that AP from a phone/laptop; a captive portal opens (or browse to `192.168.4.1`).
3. Enter WiFi SSID/password, station name, and the stream URLs (one per line, priority
   order). Set the update server URL (e.g. `http://192.168.1.50:8080`). Save.
4. The device reboots, connects, and starts playing the first reachable stream.

**Or do it all on the touchscreen** — no second device needed:
Settings (gear) → **WiFi setup** scans networks into a dropdown; enter the password on
the on-screen keyboard and Connect (saves + reboots). Settings → **Edit station URLs**
edits the failover list, one URL per line. **Hotspot mode** (on the WiFi screen) starts
the provisioning AP manually at any time.

After setup, the web config page is available anytime at `http://<device-ip>/`
(the IP is shown on the Settings screen, and in the status line when stopped).

## Web interface controls

- **On/off**: Settings → "Web interface" switch. The toggle lives only on the
  touchscreen, so remote config can never lock itself out; it is forced on while in
  provisioning/hotspot mode (the setup page *is* the web server).
- **Optional password**: set one in the web page's Options ("Web password") — the
  browser then prompts for user **admin** + your password. Type `off` in that field to
  remove it. Forgot it? Hotspot mode skips auth (physical access is the master key).

## OTA updates from then on

- Devices poll the update server (default: hourly) and install newer versions
  automatically, verified by SHA-256, with automatic rollback if a bad build
  fails to boot 3 times.
- Publish a new version with one command:
  ```bash
  cd firmware
  UPLOAD_TOKEN=yoursecret ./scripts/release.sh 0.2.0 http://<server>:8080 "release notes" fnk0104b
  ```
- Or upload the two `.bin` files manually on the server dashboard. The dashboard also
  shows every device's last check-in and firmware version, and can roll the published
  version back with one click.

## On-device smoke test checklist (first hardware bring-up)

1. Screen lights up, UI renders, touch responds (tap the gear icon).
2. Serial monitor shows `[net] online`, then `[player] connecting to URL 1/…` and
   `[player] ES8311 codec initialized`.
3. Audio from the onboard speaker (Settings → "Onboard speaker" toggles it); VU meters
   dance with the music, peak-hold dot lingers.
4. External stereo (when wired per [HARDWARE.md](HARDWARE.md)): sound from
   speakers/RCA; "External speakers" switch mutes the amps, line-out stays live.
5. Kill the active stream URL (or WiFi) — player fails over / recovers on its own
   (watch the `2/3 <url>` status line and the reconnect counter in Settings).
6. `http://<device-ip>/` loads the web config; volume slider works live.
7. Settings → "Update" reports "up to date" (or installs a newer published build).
