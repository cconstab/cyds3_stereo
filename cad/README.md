# 3D-printed cases

Two designs share this directory:

- **[case.scad](case.scad)** — the original flat, screwless two-part case (below).
- **[case-stand.scad](case-stand.scad)** — an angled **desk-stand** case (v3): the
  display leans back 30° (parametric `tilt`), with room inside for the
  GY-PCM5102/PCM5102A DAC on **four M2 screw standoffs** (3.5mm jack out the back
  wall) and a **103665 LiPo pack** (65×36×10 mm, 3.7 V 3000 mAh, JST 1.25 pigtail)
  in a rib pocket on the floor.

## Desk-stand case (case-stand.scad)

- The **shell** holds the CYD and snaps into a recess in the stand's sloped
  face (sitting ~0.6 mm proud) — four cantilever tabs on the stand latch into
  the shell's side slots. To open, pry the barbs inward through the slots with
  a small screwdriver. Unlike the flat case's poke-through window, the 2.8 mm
  bezel **overlaps the display module's inactive border** by `win_lap` (2 mm,
  with a beveled window rim): on this board the module is nearly edge-to-edge
  on the PCB, so a poke-through window would leave knife-edge tolerances top
  and bottom. The module seats face-first against the bezel underside inside a
  shallow locating fence, and the stand's corner pads press PCB → module →
  bezel, so nothing bears on the PCB's top face. Measure `glass_h` (module
  face above PCB top) and **round it down** — a hair tall clamps snug, a hair
  short lets the screen sit back and rattle.
- A **connector moat** (`con_l`/`con_t`/`con_b`, default 8 mm) leaves room
  between the PCB edges and the case walls for plugs seated in the CYD's edge
  connectors — the case is fully closed around them (no external holes for
  them; the only openings are USB-C, the DAC's 3.5 mm jack, and the vents).
  Cables route through the rim opening straight into the stand cavity. The
  PCB is located by the display module in the bezel window and pressed by
  corner pads on shelf arms that bridge the moat. `con_r` (the USB edge)
  stays 0 — widening it would recess the USB-C port deeper than a plug
  reaches. Plugs near the PCB corners have ~6 mm of depth over the pad
  shelves; elsewhere they clear straight through into the cavity.
- The **stand** is a hollow wedge with the battery pocket on its floor (battery
  lying flat, JST wire gap in the low front rib). The CYD's USB-C stays
  reachable through the **right** side; vent slots in the back.
- Two DAC options, selected with `dac_type` (the shell is identical for both):
  - `"gy"` (default) — purple GY-PCM5102 on four M2 floor standoffs, 3.5mm
    jack out the **back** wall through a support-free teardrop hole.
  - `"rca"` — the black "PCM5102MK"-style board (2× RCA + 3.5mm, sold as a
    Raspberry-Pi add-on, ~50×49mm, Pi-style 2.7mm corner holes). It stands
    flat against the **back** wall on two 12mm pilasters fused to the wall
    and floor, two M2.5 self-tappers each into seating bosses (relieved
    between so through-hole solder joints don't rock the board). A floor
    step under the jack-side pilaster sits exactly at the board's bottom
    edge — rest the board on it, then drive the screws; it carries the
    static load. The jack column exits the **right** wall: the RCA barrels
    pass through close-fitting holes (plug pull-out force goes into the
    wall, not the screws) and the 3.5mm hole has an outside counterbore so
    the plug shoulder reaches the jack. The I2S/VCC wires leave the board's
    left-edge header straight to the CYD.
- Assembly order: screw the DAC down (2× M2 self-tappers minimum), drop the
  battery in (a foam pad or adhesive strip stops rattle), wire the DAC pigtail
  and battery lead to the CYD (~8 cm so the shell can lift off for service),
  then snap the shell in.

Calibrate the same `[MEASURE]` numbers as the flat case, **plus** `dac_hole_dx`
/ `dac_hole_dy` — purple PCM5102 boards vary in mounting-hole spacing — and the
battery dimensions if yours isn't a 103665. For `dac_type="rca"` every `rca_*`
parameter was **estimated from a scaled photo** ([source](https://www.audiosciencereview.com/forum/index.php?attachments/board-jpg.358905/)) —
measure the board size, hole insets, jack positions along the edge, and the
barrel heights above the PCB before printing. The FNK0104 has a TP4054 LiPo
charger and JST battery connector on board (see HARDWARE.md), so the battery
charges through the CYD's USB-C — no extra charger board needed.

Render:

```bash
openscad -o case-stand-shell.stl   -D 'part="shell"' case-stand.scad
openscad -o case-stand-body.stl    -D 'part="stand"' case-stand.scad
openscad -o case-stand-body-rca.stl -D 'part="stand"' -D 'dac_type="rca"' case-stand.scad
```

`part="fit"` shows the shell ghosted into the recess; `part="both"` puts the two
parts side by side.

Print: shell bezel-down; stand as it sits (bottom down) — the 30° face, snap
tabs and teardrop jack hole all print without supports. PETG or PLA, 0.2 mm
layers, 3 perimeters.

# Flat case (screwless)

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
