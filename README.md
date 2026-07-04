# CYD-S3 Stereo — Resilient Internet Radio Player

A standalone, touch-configurable stereo player for a radio station's HTTP music streams,
built on the Freenove FNK0104 ESP32-S3 touch displays ("CYD"-class boards). Supports the
2.8" **FNK0104B** (default build, tested hardware) and the 4.0" **FNK0104S**.

The station publishes several stream URLs; the player treats them as a failover list —
if the active stream stalls or errors, it automatically rotates to the next URL and keeps
playing, forever, with no human intervention.

## Features

- **Resilient playback** — ordered list of stream URLs with automatic failover, retry with
  exponential backoff, connect/stall watchdogs, multi-second PSRAM stream buffer, WiFi
  modem-sleep disabled for streaming throughput, independent WiFi reconnect, and OTA
  rollback: the unit always fights its way back to playing. A reconnect counter on the
  Settings screen tracks failovers as a health metric.
- **Preferred-stream recovery** — the URL list is a quality ranking (e.g. 256k first,
  128k fallback). While playing a fallback, a background task probes the better URLs
  (byte-flow check, no decoding) and migrates back automatically once one is stable —
  two consecutive healthy probes, with settle and cool-down windows to prevent flapping.
  Toggleable in the web UI (`preferredResume`, default on).
- **Audio out, three ways** — onboard mono speaker (ES8311 codec, on/off toggle), plus the
  external stereo I2S bus: 2x MAX98357A amps driving speakers and a PCM5102A DAC providing
  RCA line-out. All share one I2S bus and play simultaneously; the speaker amps and the
  onboard speaker can each be muted independently (line-out always stays live).
- **View mode** — station name and now-playing metadata, LED-ladder VU meters
  (green/amber/red segments, true RMS-in-dB metering with attack/decay ballistics and a
  1s peak-hold dot), active stream URL + bitrate, buffer gauge, WiFi signal.
- **Standalone config** — everything on the touchscreen: WiFi setup with network scan and
  on-screen keyboard, station URL editor, volume/brightness, output toggles. First-boot
  captive-portal provisioning from a phone as an alternative.
- **Web interface** — live status/config page served by the device; can be switched off
  from the touchscreen (never from the web — no remote lock-out), optional password.
- **Easy firmware updates** — devices pull SHA-256-verified firmware from a self-hosted
  update server (single Docker container) and auto-roll-back after 3 failed boots. A
  standalone web-installer container flashes factory-fresh boards from a browser over USB.

## Hardware

Freenove FNK0104B (ESP32-S3R8, 16 MB flash / 8 MB PSRAM, 2.8" 240x320 ILI9341 touch) or
FNK0104S (4.0" 320x480 ST7796), plus ~$40 of audio add-ons for stereo. Full rationale,
parts list with prices, wiring tables, and first-power-up checklist:
**[HARDWARE.md](HARDWARE.md)**.

## Software stack

- **Firmware:** PlatformIO + Arduino core 3.x (pioarduino) for ESP32-S3
  - [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) (vendored v3.0.13) —
    HTTP/ICY stream fetch, MP3/AAC decode, metadata; PCM hook feeds the RMS VU meters
  - LVGL 8.4 + TFT_eSPI — touch UI; per-variant display config, resolution-adaptive layout
  - Vendored ES8311 codec driver (Wire-based I2C) for the onboard speaker path
  - Task layout: audio pinned to core 0 (dropout-proof), UI/WiFi/web/OTA on core 1
  - Config as JSON in LittleFS; dual OTA app partitions with boot-health rollback
- **Update server:** small Node app in a single Docker container (`docker compose up`) —
  firmware upload dashboard, version manifest (`{version, url, sha256}`) the devices poll,
  per-variant device channels, version history with one-click rollback, device check-in
  fleet view, ESP Web Tools browser flasher. 8 end-to-end tests.
- **Web installer:** nginx container serving a self-contained "flash from your browser"
  page with the firmware baked in.

## Repository layout

```
firmware/        PlatformIO project (ESP32-S3, Arduino core 3.x via pioarduino)
  src/           application code (player, UI, web config, OTA, WiFi, ES8311 codec)
  lib/           vendored libs: ESP32-audioI2S v3.0.13, FT6336U (Freenove-tested)
  scripts/       merged-image post-build + release.sh (build & publish one-liner)
update-server/   Docker container: OTA manifest + upload dashboard + browser flasher
web-installer/   Docker container: standalone "flash from your browser" page with
                 the firmware baked in (no server/toolchain needed to deploy a unit)
HARDWARE.md      Board selection, parts list, wiring
FLASHING.md      First flash, provisioning, troubleshooting/recovery, OTA workflow
```

## Quick start

```bash
# Update server
cd update-server && UPLOAD_TOKEN=secret docker compose up -d   # dashboard on :8080

# Firmware (default env = fnk0104b, the 2.8" board)
cd firmware && pio run               # build
pio run -t upload                    # first flash over USB (or use the web installer)
UPLOAD_TOKEN=secret ./scripts/release.sh 0.2.0 http://<server>:8080 "notes" fnk0104b
```

See [FLASHING.md](FLASHING.md) for the full walkthrough, including **recovery when the
board won't connect** (BOOT/RESET bootloader entry) and **factory reset** (full flash erase).
