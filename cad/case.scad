// CYD-S3 Stereo — screwless case for a CYD-class display board + GY-PCM5102 DAC
// ==============================================================================
// Two parts, no screws:
//   * FRONT shell: bezel plate whose window frames the RAISED DISPLAY MODULE —
//     the module pokes through, the bezel hides the rest of the PCB. The PCB's
//     top face presses against the bezel underside.
//   * BACK lid: slides in and latches with four snap tabs. Corner posts press
//     the PCB against the bezel; a grooved rail cradle holds the DAC board,
//     its 3.5mm jack through the left wall.
//
// !!! CALIBRATE BEFORE PRINTING !!!
// [MEASURE]-tagged numbers below are estimates for the Freenove FNK0104B —
// no official drawings exist; verify with calipers. Render:
//   openscad -o case-front.stl -D 'part="front"' case.scad
//   openscad -o case-back.stl  -D 'part="back"'  case.scad
//
// Fit-check with a board mesh (ghost overlay, preview only):
//   openscad -D 'part="front"' -D 'board_stl="board.stl"' -D board_scale=1000 case.scad
//
// Preset: Sunton ESP32-2432S024C (2.4" CYD), measured from its STL mesh —
//   cyd_w=74.4; cyd_h=42.9; pcb_t=1.6; disp_x=6.9; disp_y=0; disp_w=61.0;
//   disp_h=43.0; glass_h=5.9; USB on the BOTTOM LONG edge (this file's cutout
//   is on the right short edge — move it before using that preset).

part = "both"; // "front" | "back" | "both" (both = side-by-side preview)

// Fit-check ghost (not printed)
board_stl = "";      // path to a board STL ("" = off)
board_scale = 1;     // 1000 for meshes modeled in meters
board_rot = [0,0,0];
board_off = [0,0,0]; // nudge after placement at bezel underside, board center

// ---------------- CYD board [MEASURE] — defaults: Freenove FNK0104B ----------------
cyd_w = 86.5;        // PCB width  (long edge, mm)
cyd_h = 50.5;        // PCB height (short edge, mm)
pcb_t = 1.6;         // PCB thickness
disp_x = 9.0;        // raised display module: offset of its left edge from the PCB left edge
disp_y = 0.25;       // offset of its bottom edge from the PCB bottom edge
disp_w = 69.2;       // module width  (the raised block, incl. touch glass)
disp_h = 50.0;       // module height
glass_h = 4.0;       // module top face above PCB top (>= bezel_t pokes out / flush)
under_h = 5.0;       // tallest component below PCB
usb_edge_off = 25.0; // USB-C center along the RIGHT short edge, from the bottom corner
usb_w = 10.5;        // USB-C cutout width
usb_h = 5.0;         // USB-C cutout height (connector is on the underside)

// ---------------- DAC board (GY-PCM5102, 3.5mm jack version) [MEASURE] ----------------
dac_w = 42.0;        // PCB length (jack axis)
dac_h = 21.6;        // PCB width (slides into the rails)
dac_t = 1.7;         // PCB thickness (grooves get +0.25 clearance)
jack_d = 6.4;        // 3.5mm jack barrel hole diameter
jack_z = 3.4;        // jack barrel center above the DAC PCB top face

// ---------------- case parameters ----------------
wall = 2.4;          // wall thickness
lid_t = 2.4;         // back lid plate thickness
bezel_t = 2.0;       // bezel plate thickness
win_clr = 0.6;       // clearance around the display module in the bezel window
clr = 0.35;          // printer fit clearance (pockets, lid skirt)
dac_zone = 10.0;     // interior depth reserved behind the CYD for the DAC cradle
vent = true;         // vent slots in the back lid

// derived
iw = cyd_w + 2*clr;
ih = cyd_h + 2*clr;
idepth = pcb_t + under_h + dac_zone;   // interior depth below the bezel underside
ow = iw + 2*wall;
oh = ih + 2*wall;

$fn = 48;

// =============================================================
module front_shell() {
    difference() {
        rcube([ow, oh, bezel_t + idepth], r=2.5);
        // interior cavity behind the bezel plate
        translate([wall, wall, bezel_t]) cube([iw, ih, idepth + 1]);
        // bezel window: frames the raised display module, hides the PCB margins
        translate([wall + clr + disp_x - win_clr, wall + clr + disp_y - win_clr, -1])
            cube([disp_w + 2*win_clr, disp_h + 2*win_clr, bezel_t + 2]);
        // USB-C cutout, right wall, below the PCB plane
        translate([ow - wall - 1, wall + usb_edge_off - usb_w/2, bezel_t + pcb_t - 0.01])
            cube([wall + 2, usb_w, usb_h]);
        // 3.5mm jack hole, left wall, aligned with the DAC in the lid cradle
        translate([-1, oh/2, bezel_t + idepth - jack_center_above_lid()])
            rotate([0, 90, 0]) cylinder(d=jack_d, h=wall + 2);
        // lid snap slots: two per long side
        for (x = [ow*0.25, ow*0.75], y = [0, oh - wall])
            translate([x - 4, y - 0.01, bezel_t + idepth - 2.6])
                cube([8, wall + 0.02, 1.6]);
    }
}

// =============================================================
module back_lid() {
    skirt = 6.0;
    difference() {
        union() {
            rcube([ow, oh, lid_t], r=2.5);
            translate([wall + clr, wall + clr, lid_t - 0.01])
                frame([iw - 2*clr, ih - 2*clr], t=1.8, h=skirt);
            // snap tabs (barbs face outward on both sides)
            for (x = [ow*0.25, ow*0.75]) {
                translate([x - 3.5, wall + clr + 1.2, lid_t]) mirror([0, 1, 0]) snap_tab();
                translate([x - 3.5, oh - wall - clr - 1.2, lid_t]) snap_tab();
            }
            // corner posts: press the CYD PCB against the bezel underside
            post_h = idepth - pcb_t - clr;
            for (x = [wall + 3, wall + iw - 3 - 4], y = [wall + 3, wall + ih - 3 - 4])
                translate([x, y, lid_t - 0.01]) cube([4, 4, post_h]);
            dac_rails();
        }
        if (vent)
            for (i = [0:5])
                translate([ow/2 - 24 + i*8, oh/2 - 12, -1]) rcube([3.2, 24, lid_t + 2], r=1.5);
    }
}

// DAC rides component-side-up; its edges sit in grooves near the rail tops.
function rail_h() = jack_z + dac_t + 5.0;
function groove_bottom() = rail_h() - 3 - (dac_t + 0.25);
function jack_center_above_lid() = groove_bottom() + dac_t + jack_z;

module dac_rails() {
    y0 = oh/2 - dac_h/2 - clr - 2.2;  // lower rail base (board face = its +Y side)
    yU = oh/2 + dac_h/2 + clr;        // upper rail base (board face = its -Y side)
    for (y = [y0, yU])
        translate([wall + 1, y, lid_t - 0.01])
            difference() {
                cube([dac_w - 4, 2.2, rail_h()]);
                translate([-1, (y == y0 ? 2.2 - (dac_t + 0.25) : 0) - 0.01, groove_bottom()])
                    cube([dac_w - 2, dac_t + 0.26, dac_t + 0.25]);
            }
    // end stop so the jack stays pressed to its hole
    translate([wall + dac_w - 3, oh/2 - dac_h/2, lid_t - 0.01]) cube([2.2, dac_h, rail_h()]);
}

module snap_tab() {
    tab_h = idepth - 2.6 - clr;
    cube([7, 1.2, tab_h]);
    translate([0, 0, tab_h - 1.4]) cube([7, 1.2 + 1.0, 1.4]);
}

// =============================================================
module rcube(s, r=2) {
    hull() for (x = [r, s[0]-r], y = [r, s[1]-r])
        translate([x, y, 0]) cylinder(r=r, h=s[2]);
}
module frame(s, t, h) {
    difference() {
        cube([s[0], s[1], h]);
        translate([t, t, -1]) cube([s[0]-2*t, s[1]-2*t, h+2]);
    }
}
module board_ghost() {
    if (board_stl != "")
        %translate([ow/2 + board_off[0], oh/2 + board_off[1], bezel_t + board_off[2]])
            rotate(board_rot) scale(board_scale) import(board_stl);
}

// =============================================================
if (part == "front") { front_shell(); board_ghost(); }
if (part == "back") back_lid();
if (part == "both") {
    front_shell();
    translate([ow + 12, 0, 0]) back_lid();
}
