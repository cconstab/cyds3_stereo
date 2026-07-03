# CYD-S3 Stereo — Resilient Internet Radio Player

A standalone, touch-configurable stereo player for a radio station's HTTP music streams,
built on the Freenove FNK0104S ESP32-S3 4.0" touch display ("CYD"-class board).

The station publishes several stream URLs; the player treats them as a failover list —
if the active stream stalls or errors, it automatically rotates to the next URL and keeps
playing, forever, with no human intervention.

## Features

- **Resilient playback** — ordered list of stream URLs with automatic failover, retry with
  exponential backoff, multi-second PSRAM stream buffer, independent WiFi reconnect,
  hardware watchdog, and OTA rollback: the unit always fights its way back to playing.
- **True stereo** — external I2S audio: 2x MAX98357A amps driving speakers, plus a PCM5102A
  DAC providing RCA line-out. Both run simultaneously from one I2S bus.
  (The board's onboard ES8311 codec is mono — kept as a fallback/test output.)
- **View mode** — station name and now-playing metadata, stereo VU meters (peak + decay,
  fed from decoded PCM), WiFi signal strength, active-URL indicator, buffer health bar.
- **Standalone config** — touch UI for volume, station URLs, WiFi, brightness; first-boot
  captive-portal provisioning from a phone; small web config UI served by the device.
- **Easy firmware updates** — devices pull signed/hashed firmware from a self-hosted update
  server (single Docker container) and auto-roll-back on a bad flash. The same container
  serves an ESP Web Tools page to flash factory-fresh boards from a browser over USB.

## Hardware

Freenove FNK0104S (ESP32-S3R8, 16 MB flash / 8 MB PSRAM, 4.0" 320x480 ST7796 touch display)
plus ~$40 of audio add-ons. Full rationale, parts list with prices, wiring tables, and
first-power-up checklist: **[HARDWARE.md](HARDWARE.md)**.

## Software stack

- **Firmware:** PlatformIO + Arduino core for ESP32-S3
  - [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) — HTTP/ICY stream fetch,
    MP3/AAC decode, metadata, stereo I2S out, PCM hook for the VU meters
  - LVGL 9 + LovyanGFX — touch UI on the ST7796
  - Task layout: audio pinned to core 0 (dropout-proof), UI/WiFi/OTA on core 1
  - Config as JSON in LittleFS; dual OTA app partitions with rollback
- **Update server:** small web app in a single Docker container (`docker compose up`) —
  firmware upload page, version manifest (`{version, url, sha256}`) the devices poll,
  version history with one-click rollback, ESP Web Tools browser flasher for new boards.
  Optionally fed by CI on tagged releases.

## Build phases

1. **Hardware validation** — stock demo smoke test, pin continuity check, I2S test tone
2. **Firmware skeleton** — display + touch + LVGL, captive-portal WiFi provisioning, LittleFS config
3. **Audio core** — single stream playing in stereo, then the failover state machine
4. **UI** — view mode with VU meters, config screens, on-device web config
5. **OTA + Docker update server** — partitions, pull-OTA client with rollback, server container, web flasher
6. **Soak test** — days of runtime; kill WiFi, kill URLs mid-song, power-cycle; must always resume playing

## Repository layout

```
firmware/        PlatformIO project (ESP32-S3, Arduino core 3.x via pioarduino)
  src/           application code (player, UI, web config, OTA, WiFi)
  lib/           vendored libs: ESP32-audioI2S v3.0.13, FT6336U (Freenove-tested)
  scripts/       merged-image post-build + release.sh (build & publish one-liner)
update-server/   Docker container: OTA manifest + upload dashboard + browser flasher
web-installer/   Docker container: standalone "flash from your browser" page with
                 the firmware baked in (no server/toolchain needed to deploy a unit)
HARDWARE.md      Board selection, parts list, wiring
FLASHING.md      First flash, provisioning, OTA release workflow
```

## Quick start

```bash
# Update server
cd update-server && UPLOAD_TOKEN=secret docker compose up -d   # dashboard on :8080

# Firmware
cd firmware && pio run                # build
pio run -t upload                     # first flash over USB (or use /flash.html in Chrome)
UPLOAD_TOKEN=secret ./scripts/release.sh 0.2.0 http://<server>:8080  # publish OTA release
```

See [FLASHING.md](FLASHING.md) for the full walkthrough.
