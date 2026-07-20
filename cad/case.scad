// CYD-S3 Stereo — screwless case for the Freenove FNK0104B (2.8") + GY-PCM5102 DAC
// ==============================================================================
// Two parts, no screws:
//   * FRONT shell: display bezel + walls. The board drops in display-first and
//     rests on internal ledges; the USB-C cutout is in the right wall.
//   * BACK lid: slides in and latches with four snap tabs. Carries corner posts
//     that press the PCB onto its ledges, and a rail cradle the DAC board slides
//     into, its 3.5mm jack poking through the left wall.
//
// !!! CALIBRATE BEFORE PRINTING !!!
// Every dimension marked [MEASURE] is my best estimate — verify with calipers
// against your actual boards and adjust. Then render both parts:
//   openscad -o case-front.stl -D 'part="front"' case.scad
//   openscad -o case-back.stl  -D 'part="back"'  case.scad
//
// Suggested print: PETG or PLA, 0.2mm layers, 3 walls, no supports.
// Front prints bezel-down, back prints flat side down.

part = "both"; // "front" | "back" | "both" (both = side-by-side preview)

// ---------------- CYD board [MEASURE] ----------------
cyd_w = 86.5;        // PCB width  (long edge, mm)
cyd_h = 50.5;        // PCB height (short edge, mm)
pcb_t = 1.6;         // PCB thickness
glass_h = 3.6;       // display module height above PCB top (glass face to PCB)
under_h = 5.0;       // tallest component below PCB (connectors etc.)
usb_edge_off = 25.0; // USB-C center, measured along the RIGHT short edge from the front-top corner
usb_w = 10.5;        // USB-C cutout width
usb_h = 5.0;         // USB-C cutout height (below PCB plane; connector is on the underside)

// ---------------- DAC board (GY-PCM5102, 3.5mm jack version) [MEASURE] ----------------
dac_w = 42.0;        // PCB length (jack axis)
dac_h = 21.6;        // PCB width (slides into the rails)
dac_t = 1.7;         // PCB thickness (groove size gets +0.25 clearance)
jack_d = 6.4;        // 3.5mm jack barrel hole diameter
jack_z = 3.4;        // jack barrel center height above DAC PCB top face

// ---------------- case parameters ----------------
wall = 2.4;          // wall thickness
lid_t = 2.4;         // back lid plate thickness
bezel_t = 2.0;       // bezel plate thickness
bezel_lip = 3.0;     // how far the bezel overhangs the board opening (grips the glass edge)
clr = 0.35;          // printer fit clearance (board pockets, lid skirt)
dac_zone = 10.0;     // interior depth reserved under the CYD for the DAC cradle
vent = true;         // vent slots in the back lid

// derived
iw = cyd_w + 2*clr;                  // interior width
ih = cyd_h + 2*clr;                  // interior height
idepth = glass_h + pcb_t + under_h + dac_zone; // interior depth below the bezel lip
ow = iw + 2*wall;
oh = ih + 2*wall;
odepth = bezel_t + idepth + lid_t;
ledge_z = bezel_t + glass_h;         // top of the PCB support ledge (from outer front face)

$fn = 48;

// =============================================================
module front_shell() {
    difference() {
        // outer block
        rcube([ow, oh, bezel_t + idepth], r=2.5);
        // interior cavity (below bezel plate)
        translate([wall, wall, bezel_t]) cube([iw, ih, idepth + 1]);
        // bezel window (display glass shows through)
        translate([wall + bezel_lip, wall + bezel_lip, -1])
            cube([iw - 2*bezel_lip, ih - 2*bezel_lip, bezel_t + 2]);
        // USB-C cutout, right wall, below PCB plane
        translate([ow - wall - 1, wall + usb_edge_off - usb_w/2, ledge_z + pcb_t - 0.01])
            cube([wall + 2, usb_w, usb_h]);
        // 3.5mm jack hole, left wall, aligned with the DAC sitting in the lid cradle
        // (board rides component-side-up in the rail grooves; see back_lid/dac_rails)
        translate([-1, oh/2, bezel_t + idepth - jack_center_above_lid()])
            rotate([0, 90, 0]) cylinder(d=jack_d, h=wall + 2);
        // lid snap slots: two per long side
        for (x = [ow*0.25, ow*0.75], y = [0, oh - wall])
            translate([x - 4, y - 0.01, bezel_t + idepth - 2.6])
                cube([8, wall + 0.02, 1.6]);
    }
    // PCB support ledges (interrupted at the USB cutout)
    lw = 2.0; // ledge width
    for (y = [wall, wall + ih - lw])
        translate([wall, y, bezel_t]) cube([iw, lw, glass_h]);
    translate([wall, wall, bezel_t]) cube([lw, ih, glass_h]);
    // right ledge: split around the USB opening
    translate([wall + iw - lw, wall, bezel_t])
        cube([lw, usb_edge_off - usb_w/2 - 1, glass_h]);
    translate([wall + iw - lw, wall + usb_edge_off + usb_w/2 + 1, bezel_t])
        cube([lw, ih - usb_edge_off - usb_w/2 - 1, glass_h]);
}

// =============================================================
module back_lid() {
    skirt = 6.0;
    difference() {
        union() {
            rcube([ow, oh, lid_t], r=2.5);
            // skirt that slides inside the shell walls
            translate([wall + clr, wall + clr, lid_t - 0.01])
                frame([iw - 2*clr, ih - 2*clr], t=1.8, h=skirt);
            // snap tabs matching the shell slots (barbs face outward on both sides)
            for (x = [ow*0.25, ow*0.75]) {
                translate([x - 3.5, wall + clr + 1.2, lid_t]) mirror([0, 1, 0]) snap_tab();
                translate([x - 3.5, oh - wall - clr - 1.2, lid_t]) snap_tab();
            }
            // corner posts: press the CYD PCB onto its ledges
            post_h = idepth - glass_h - pcb_t - clr;
            for (x = [wall + 3, wall + iw - 3 - 4], y = [wall + 3, wall + ih - 3 - 4])
                translate([x, y, lid_t - 0.01]) cube([4, 4, post_h]);
            // DAC cradle: two grooved rails + end stop, jack toward the LEFT wall
            dac_rails();
        }
        if (vent)
            for (i = [0:5])
                translate([ow/2 - 24 + i*8, oh/2 - 12, -1]) rcube([3.2, 24, lid_t + 2], r=1.5);
    }
}

// PCB rides component-side-up; its edges sit in grooves near the rail tops.
function rail_h() = jack_z + dac_t + 5.0;
function groove_bottom() = rail_h() - 3 - (dac_t + 0.25); // above lid inner face
function jack_center_above_lid() = groove_bottom() + dac_t + jack_z;

module dac_rails() {
    // rails run along X (jack axis); DAC slides in from the +X end, jack pokes left (-X wall)
    y0 = oh/2 - dac_h/2 - clr - 2.2;          // lower rail base (board face = its +Y side)
    yU = oh/2 + dac_h/2 + clr;                // upper rail base (board face = its -Y side)
    for (y = [y0, yU])
        translate([wall + 1, y, lid_t - 0.01])
            difference() {
                cube([dac_w - 4, 2.2, rail_h()]);
                // groove opens on the board-facing side of each rail
                translate([-1, (y == y0 ? 2.2 - (dac_t + 0.25) : 0) - 0.01, groove_bottom()])
                    cube([dac_w - 2, dac_t + 0.26, dac_t + 0.25]);
            }
    // end stop so the jack can't push the board back off its hole
    translate([wall + dac_w - 3, oh/2 - dac_h/2, lid_t - 0.01]) cube([2.2, dac_h, rail_h()]);
}

module snap_tab() {
    // simple cantilever with a 1.2mm barb, engages the shell slot
    tab_h = idepth - 2.6 - clr;
    cube([7, 1.2, tab_h]);
    translate([0, 0, tab_h - 1.4]) cube([7, 1.2 + 1.0, 1.4]);
}

// =============================================================
// helpers
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

// =============================================================
if (part == "front") front_shell();
if (part == "back") back_lid();
if (part == "both") {
    front_shell();
    translate([ow + 12, 0, 0]) back_lid();
}
