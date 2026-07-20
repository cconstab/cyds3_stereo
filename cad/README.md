# 3D-printed case (screwless)

Parametric OpenSCAD case holding the FNK0104B (2.8") and the GY-PCM5102 DAC
(3.5mm-jack version) with **no screws**:

- The CYD drops display-first into the **front shell** and rests on internal ledges;
  the bezel lip holds the glass edge.
- The **back lid** snaps in with four cantilever tabs. Its corner posts press the CYD
  PCB onto the ledges, and the DAC board slides into a grooved rail cradle on the lid,
  jack aligned with the left wall hole.
- Openings: USB-C (right wall), 3.5mm jack (left wall), vent slots (back).

## Before printing — calibrate!

Freenove publishes no mechanical drawings, so every `[MEASURE]` parameter at the top of
[case.scad](case.scad) is an estimate. Verify with calipers:

1. `cyd_w`, `cyd_h`, `pcb_t` — the CYD PCB itself
2. `glass_h` — glass face to PCB top
3. `under_h` — tallest part under the PCB
4. `usb_edge_off` — USB-C center along the right edge (and confirm which edge yours is on)
5. `dac_w`, `dac_h`, `dac_t` — the DAC board
6. `jack_z` — jack barrel center above the DAC PCB

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
