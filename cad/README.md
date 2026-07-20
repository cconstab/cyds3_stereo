# 3D-printed case (screwless)

Parametric OpenSCAD case holding the FNK0104B (2.8") and the GY-PCM5102 DAC
(3.5mm-jack version) with **no screws**:

- The CYD drops display-first into the **front shell**: the bezel window frames the
  **raised display module** (which sits flush-ish or slightly proud), and the bezel
  plate hides the rest of the PCB, whose top face presses against the bezel underside.
- The **back lid** snaps in with four cantilever tabs. Its corner posts press the CYD
  PCB onto the ledges, and the DAC board slides into a grooved rail cradle on the lid,
  jack aligned with the left wall hole.
- Openings: USB-C (right wall), 3.5mm jack (left wall), vent slots (back).

## Before printing — calibrate!

Freenove publishes no mechanical drawings, so every `[MEASURE]` parameter at the top of
[case.scad](case.scad) is an estimate. Verify with calipers:

1. `cyd_w`, `cyd_h`, `pcb_t` — the CYD PCB itself
2. `disp_x`, `disp_y`, `disp_w`, `disp_h` — the raised display module's footprint on the
   PCB (the bezel window is cut to this), and `glass_h` — module top above PCB top
3. `under_h` — tallest part under the PCB
4. `usb_edge_off` — USB-C center along the right edge (and confirm which edge yours is on)
5. `dac_w`, `dac_h`, `dac_t` — the DAC board
6. `jack_z` — jack barrel center above the DAC PCB

## Fit-check against a board mesh

If you have an STL of the board, overlay it as a ghost inside the front shell:

```bash
openscad -D 'part="front"' -D 'board_stl="/path/board.stl"' -D board_scale=1000 case.scad
```

A Sunton ESP32-2432S024C (2.4" CYD) preset, measured from such a mesh, is noted in the
header of case.scad — NB: that board is smaller than the FNK0104B and has its USB on the
bottom long edge, so it needs the USB cutout moved as well as the size parameters.

## Render STLs

```bash
brew install --cask openscad     # once
openscad -o case-front.stl -D 'part="front"' case.scad
openscad -o case-back.stl  -D 'part="back"'  case.scad
```

(Or open `case.scad` in the OpenSCAD GUI, set `part`, F6, export STL.)

## Print settings

PETG or PLA · 0.2 mm layers · 3 perimeters · no supports.
Front shell prints bezel-down; back lid prints flat side down.
If the snap tabs are too tight/loose, tune `clr` (±0.1) and reprint just the lid.

Wiring note: keep the DAC's BCK/LCK/DIN/5V/GND pigtail ~8 cm so the lid can hinge
open for service without unplugging.
