// CYD-S3 Stereo — angled DESK-STAND case (v3)
// ===========================================================================
// Holds the CYD (Freenove FNK0104B), a PCM5102-family I2S DAC on screw
// standoffs (two board options — see dac_type), and a 103665 LiPo pack
// (65 x 36 x 10 mm, 3.7V 3000mAh, JST 1.25 pigtail).
// Render the RCA-DAC base:
//   openscad -o case-stand-body-rca.stl -D 'part="stand"' -D 'dac_type="rca"' case-stand.scad
//
// Two printed parts:
//   * SHELL — bezel plate + skirt that holds the CYD exactly as in case.scad
//     (display module pokes through the window, PCB presses on the bezel
//     underside). A "connector moat" (con_l/con_t/con_b) leaves room between
//     the PCB edges and the walls for plugs seated in the CYD's edge
//     connectors — enclosed, with no external holes; the cables route through
//     the rim opening into the stand. The shell drops into a recess in the
//     stand's sloped face and latches on four cantilever snap tabs (pry
//     through the shell's side slots with a small screwdriver to release).
//   * STAND — hollow wedge leaning `tilt` degrees back from vertical.
//     Inside, on the floor: a rib pocket for the battery (lying flat) and
//     four M2 self-tap standoffs for the DAC, its 3.5mm jack exiting the
//     BACK wall. The CYD's USB-C stays reachable through the RIGHT side.
//
// !!! CALIBRATE BEFORE PRINTING !!!
// [MEASURE]-tagged numbers are estimates — verify with calipers, especially
// the DAC mounting-hole spacing (dac_hole_dx/dy): purple PCM5102 boards vary.
//
// Render:
//   openscad -o case-stand-shell.stl -D 'part="shell"' case-stand.scad
//   openscad -o case-stand-body.stl  -D 'part="stand"' case-stand.scad

part = "both"; // "shell" | "stand" | "both" (side-by-side) | "fit" (assembled preview)

// Which DAC the stand cradles (the shell is identical for both):
//   "gy"  — purple GY-PCM5102, flat on floor standoffs, 3.5mm jack out the BACK
//   "rca" — black "PCM5102MK" board (2x RCA + 3.5mm, M3 corner holes), mounted
//           flat against the BACK wall on two vertical standoff ribs; the jack
//           column exits through the RIGHT wall, RCA barrels passing through
dac_type = "gy";

// ---------------- CYD board [MEASURE] — defaults: Freenove FNK0104B ----------------
cyd_w = 86.5;        // PCB width  (long edge, mm)
cyd_h = 50.5;        // PCB height (short edge, mm)
pcb_t = 1.6;         // PCB thickness
disp_x = 9.0;        // raised display module: offset of its left edge from the PCB left edge
disp_y = 0.25;       // offset of its bottom edge from the PCB bottom edge
disp_w = 69.2;       // module width  (the raised block, incl. touch glass)
disp_h = 50.0;       // module height
glass_h = 4.0;       // module face above the PCB top — round DOWN when measuring
under_h = 5.0;       // tallest component below the PCB (must stay < skirt depth d_s)
usb_edge_off = 25.0; // USB-C center along the RIGHT short edge, from the bottom corner
usb_w = 14.0;        // USB wall-slot width — oversize: the plug's overmold must
                     // pass THROUGH the slot to reach the recessed connector

// Connector moat: clearance between each PCB edge and the case wall so plugs
// seated in the CYD's edge connectors (and their wires) fit INSIDE the case —
// no external holes; cables route through the rim opening into the stand.
// con_l/con_r are paired so the display window sits CENTERED in the case face
// (the module is offset on the PCB: 9.0 left margin vs 8.3 right). If you
// raise con_l for a side-entry plug, raise con_r by the same amount to keep
// the window centered. The USB-C sits recessed behind the right wall; its
// wall slot is oversized so the whole plug head passes through to reach it.
con_l = 3.6;         // left short edge
con_r = 4.3;         // right short edge (USB)
con_t = 8.0;         // top long edge
con_b = 8.0;         // bottom long edge

// ---------------- DAC board (GY-PCM5102 / PCM5102A, 3.5mm jack) [MEASURE] ----------
dac_w = 42.0;        // PCB length (jack axis)
dac_h = 21.6;        // PCB width
dac_t = 1.7;         // PCB thickness
jack_d = 6.4;        // 3.5mm jack barrel hole diameter
jack_z = 3.4;        // jack barrel center above the DAC PCB top face
jack_proud = 1.5;    // how far the barrel tucks into the back-wall hole
dac_hole_dx = 37.0;  // mounting-hole spacing along the jack axis  [MEASURE!]
dac_hole_dy = 16.6;  // mounting-hole spacing across the board     [MEASURE!]
standoff_od = 5.0;   // standoff outer diameter
standoff_h = 4.0;    // standoff height (clears solder joints under the DAC)
pilot_d = 1.8;       // pilot hole for M2 self-tapping screws

// ---------------- RCA DAC board ("PCM5102MK": 2x RCA + 3.5mm) ----------------------
// Gauge-measured on the actual board (2026-09). The two [DERIVED] values are
// inferred from the measured connector-top heights — verify before trusting.
rca_bw = 48.17;      // board width  (header edge -> jack edge; horizontal)
rca_bh = 48.7;       // board height (jack edge length; vertical when mounted)
rca_bt = 1.65;       // PCB thickness
rca_hole_in = 3.0;   // corner mounting holes: edge to center
rca_pilot = 2.5;     // pilot for M3 self-tappers (board holes measure 3.26 dia)
rca_so = 4.0;        // standoff rib depth off the back wall
rca_z0 = 14.0;       // board bottom edge height above the desk
rca_j = [15.0, 36.7]; // RCA centers (white/L, red/R) along the jack edge, from
                     // the board BOTTOM. JIG-CALIBRATED: with the board seated
                     // on the base step, the barrels sat ~1.5 above the
                     // edge-referenced gauge values (13.52 from each edge), so
                     // these carry that offset. The holes are also slotted
                     // +2/-1 vertically to absorb board-to-board variation.
rca_j35 = 25.9;      // 3.5mm jack center, same reference + same calibration
rca_axis = 8.27;     // RCA barrel axis above the PCB component face (measured).
                     // NB the barrels tilt slightly UPWARD on real boards (rear
                     // solder pin sits lower) — the teardrop holes' extra
                     // headroom above center absorbs that.
j35_axis = 2.6;      // [DERIVED] 3.5mm axis: body top 6.79 - 1.65 = 5.14 above
                     // the face, barrel centered -> ~2.6. VERIFY.
rca_house = 12.79;   // RCA housing height above the PCB face (square bodies)
rca_house_l = 13.0;  // RCA housing depth inboard from the jack edge
rca_proud = 7.6;     // RCA barrel protrusion past the board edge (through the
                     // 0.5 gap + 2.4 wall -> ~4.7 exposed for the plug)
j35_proud = 3.0;     // 3.5mm nose past the board edge (~flush with the wall
                     // outside; the counterbore gives it a little room)
rca_hole_d = 11.0;   // RCA barrel pass-through hole (barrel ~8.3)
j35_hole_d = 7.2;    // 3.5mm plug hole (gets an outside counterbore too)

// ---------------- battery: 103665 LiPo (3.7V 3000mAh) ------------------------------
bat_l = 65.0;        // battery length  (lies along X)
bat_w = 36.0;        // battery width   (along Y)
bat_clr = 0.75;      // pocket clearance per side
rib_t = 2.0;         // pocket rib thickness
rib_h = 12.0;        // pocket rib height (front rib is lower, see below)

// ---------------- stand / case ------------------------------------------------------
tilt = 30;           // lean-back angle from vertical (0 = bolt upright), deg
toe_h = 8.0;         // vertical toe below the sloped face
rim = 4.0;           // sloped-face margin around the shell recess
stand_d = 66.0;      // footprint depth, front toe -> back wall
wall = 2.4;          // wall / floor thickness
bezel_t = 2.8;       // shell bezel plate thickness
win_lap = 2.0;       // bezel overlap onto the display module's inactive border
                     // (2.8" panels have ~3.4mm border top/bottom, ~5mm sides)
win_bevel = 1.5;     // 45-ish bevel around the window, opening toward the viewer
mod_clr = 0.5;       // pocket clearance around the display module
glass_gap = 0.3;     // seating allowance above the module face (round glass_h
                     // DOWN when measuring — too-tall seats snug, too-short rattles)
shell_proud = 0.6;   // shell face stands this far above the sloped face
clr = 0.35;          // printer fit clearance
vent = true;         // vent slots in the back wall

// ---------------- derived — shell ----------------
iw = cyd_w + 2*clr + con_l + con_r;
ih = cyd_h + 2*clr + con_b + con_t;
usb_y = wall + con_b + clr + usb_edge_off;   // USB center, shell-local
face_pcb = bezel_t + glass_h + glass_gap;    // bezel face -> PCB top face
d_s = pcb_t + 6.0;             // skirt depth below the PCB top face
s_ow = iw + 2*wall;
s_oh = ih + 2*wall;
shell_h = face_pcb + d_s;      // total shell height (bezel face -> skirt edge)

// ---------------- derived — stand ----------------
OW  = s_ow + 2*rim;                    // stand width
SL  = s_oh + 2*rim;                    // sloped-face length
run = SL*sin(tilt);
topz = toe_h + SL*cos(tilt);           // stand height
r_w = s_ow + 2*clr;  r_h = s_oh + 2*clr;   // shell recess in the sloped face
rx = (OW - r_w)/2;   ry = (SL - r_h)/2;
R  = shell_h - shell_proud;            // recess depth (shell sits slightly proud)
o_w = iw;  o_h = ih;                   // rim opening = PCB footprint + clr
ox = (OW - o_w)/2;   oy = ry + (r_h - o_h)/2;
ring_t = 4.0;                          // rim frame thickness behind the recess floor
s_usb = ry + clr + s_oh - usb_y;       // USB center along the slope (shell mounts flipped)

// PCB rectangle in the slope frame (shell mounts flipped about X)
pcb_fx0 = rx + clr + wall + con_l + clr;
pcb_fx1 = pcb_fx0 + cyd_w;
pcb_fy1 = ry + clr + s_oh - (wall + con_b + clr);
pcb_fy0 = pcb_fy1 - cyd_h;

$fn = 48;

// =============================================================
// SHELL — bezel + skirt holding the CYD (snaps into the stand)
// =============================================================
// The bezel plate overlaps the display module's inactive border by win_lap
// (the module is nearly edge-to-edge on the PCB, so a poke-through window
// would leave knife-edge tolerances top and bottom). The module face seats
// against the plate underside inside a shallow fence; the corner pads in the
// stand press PCB -> module -> plate, so nothing bears on the PCB top face.
module shell() {
    x0m = wall + con_l + clr + disp_x;   // display module rectangle
    y0m = wall + con_b + clr + disp_y;
    difference() {
        union() {
            difference() {
                rcube([s_ow, s_oh, shell_h], r=2.5);
                // interior cavity behind the bezel plate
                translate([wall, wall, bezel_t]) cube([iw, ih, shell_h - bezel_t + 1]);
            }
            // fence around the module: locates it, stops 0.1 above the PCB top
            translate([x0m - mod_clr - 1.8, y0m - mod_clr - 1.8, bezel_t - 0.01])
                frame([disp_w + 2*(mod_clr + 1.8), disp_h + 2*(mod_clr + 1.8)],
                      t=1.8, h=glass_h + 0.2);
        }
        // window, overlapping the module border by win_lap all around
        translate([x0m + win_lap, y0m + win_lap, -1])
            cube([disp_w - 2*win_lap, disp_h - 2*win_lap, bezel_t + 2]);
        // bevel opening toward the viewer
        hull() {
            translate([x0m + win_lap - win_bevel, y0m + win_lap - win_bevel, -0.01])
                cube([disp_w - 2*(win_lap - win_bevel), disp_h - 2*(win_lap - win_bevel), 0.01]);
            translate([x0m + win_lap, y0m + win_lap, bezel_t - 1.0])
                cube([disp_w - 2*win_lap, disp_h - 2*win_lap, 0.01]);
        }
        // USB-C notch, right wall, open to the skirt edge (continues into the
        // stand lip). Starts 1 above the PCB plane so the plug head — wider
        // and taller than the connector — passes through the wall.
        translate([s_ow - wall - 1, usb_y - usb_w/2, face_pcb - 1])
            cube([wall + 2, usb_w, shell_h + 2]);
        // snap slots: two per long side; also the pry points for disassembly
        for (x = [s_ow*0.25, s_ow*0.75], y = [0, s_oh - wall])
            translate([x - 4, y - 0.01, shell_proud + 4.05]) cube([8, wall + 0.02, 1.9]);
    }
}
module frame(s, t, h) {
    difference() {
        cube([s[0], s[1], h]);
        translate([t, t, -1]) cube([s[0] - 2*t, s[1] - 2*t, h + 2]);
    }
}

// =============================================================
// STAND — hollow wedge
// =============================================================
// Side profile in (y,z): toe -> slope -> flat top -> vertical back.
profile = [[0,0], [stand_d,0], [stand_d,topz], [run,topz], [0,toe_h]];

module wedge_solid() {
    intersection() {
        rotate([90,0,90]) linear_extrude(OW) polygon(profile);
        rcube([OW, stand_d, topz + 1], r=3);   // round the vertical edges
    }
}
module wedge_cavity() {
    rotate([90,0,90]) translate([0,0,wall])
        linear_extrude(OW - 2*wall) offset(delta=-wall) polygon(profile);
}

// Local frame on the sloped face: x = width, y = distance up the slope,
// z = OUTWARD normal (so negative z goes into the body).
module slope_frame() {
    translate([0, 0, toe_h]) rotate([90 - tilt, 0, 0]) children();
}

module stand() {
    difference() {
        union() {
            difference() {
                wedge_solid();
                wedge_cavity();
                // recess for the shell
                slope_frame() translate([rx, ry, -R]) rcube([r_w, r_h, R + 30], r=2.7);
            }
            intersection() {   // clip interior additions to the outer solid
                union() {
                    if (part != "jig") {   // jig: DAC mount + jack wall only
                        rim_frame();
                        pcb_pads();
                        snap_tabs();
                        battery_pocket();
                    }
                    if (dac_type == "gy") dac_standoffs();
                    if (dac_type == "rca") rca_ribs();
                }
                wedge_solid();
            }
        }
        // USB-C pass-through in the right lip (meets the shell's wall notch)
        slope_frame() translate([rx + r_w - 0.8, s_usb - usb_w/2 - 2, -(R + 0.1)])
            cube([rim + 4, usb_w + 4, R + 0.1 - (face_pcb - 1) + shell_proud]);
        if (dac_type == "gy") {
            // 3.5mm jack, back wall (teardrop so it prints without support)
            translate([dac_cx(), stand_d + 1, jack_zc()])
                rotate([90, 0, 0]) teardrop(d=jack_d, h=wall + 3);
        }
        if (dac_type == "rca") {
            // jack column through the right wall: RCA barrels pass through,
            // 3.5mm gets a snout hole plus an outside counterbore. All are
            // vertical slots (+2/-1) — cheap jacks vary in height and tilt
            // upward, and the plug bodies cover the slots from outside.
            for (jz = rca_j) hull() for (dz = [-1, 2])
                translate([OW + 1, rca_jy(rca_axis), rca_z0 + jz + dz])
                    rotate([0, -90, 0]) rotate([0, 0, -90]) teardrop(d=rca_hole_d, h=wall + 3);
            hull() for (dz = [-1, 2])
                translate([OW + 1, rca_jy(j35_axis), rca_z0 + rca_j35 + dz])
                    rotate([0, -90, 0]) rotate([0, 0, -90]) teardrop(d=j35_hole_d, h=wall + 3);
            hull() for (dz = [-1, 2])
                translate([OW - 1.0, rca_jy(j35_axis), rca_z0 + rca_j35 + dz])
                    rotate([0, 90, 0]) cylinder(d=11.5, h=3);
        }
        // vents, back wall (over the battery bay; shorter row in rca mode so
        // they stay clear of the wall-mounted board)
        if (vent)
            for (i = [0 : (dac_type == "rca" ? 4 : 5)])
                translate([12 + i*9, stand_d - 3.5, 30]) cube([3.5, 5, 22]);
        // recesses for stick-on rubber feet
        for (x = [12, OW - 12], y = [10, stand_d - 10])
            translate([x, y, -1]) cylinder(d=10, h=2);
    }
}

// Rim frame: backs the recess floor and borders the opening the CYD's
// underside components / wiring pass through.
module rim_frame() {
    // top band stops just past the recess edge so it can't reach the
    // wall-mounted RCA DAC behind it
    slope_frame() difference() {
        translate([rx - 4, ry - 4, -R - ring_t]) cube([r_w + 8, r_h + 5, ring_t]);
        translate([ox, oy, -R - ring_t - 1]) cube([o_w, o_h, ring_t + 2]);
    }
}

// Corner pads press the CYD PCB against the shell's bezel underside. The PCB
// corners sit inboard of the rim opening (connector moat), so each pad rides
// on a shelf arm rooted in the rim frame and bridging the moat.
// In "rca" mode the top-right pad would collide with the red RCA jack's body
// on the wall-mounted DAC, so it slides left along the PCB's top edge into
// the free window between the snap tab and the RCA housing.
module pcb_pads() {
    pad = 6;
    trx = rx + clr + s_ow*0.75 + 4.2;   // relocated top-right pad, clear of the tab
    slope_frame()
        for (cx = [-1, 1], cy = [-1, 1]) {
            moved = dac_type == "rca" && cx > 0 && cy > 0;
            px = cx < 0 ? pcb_fx0 : (moved ? trx : pcb_fx1 - pad);
            py = cy < 0 ? pcb_fy0 : pcb_fy1 - pad;
            sx0 = cx < 0 ? ox - 3 : px;
            sy0 = cy < 0 ? oy - 3 : py;
            sx1 = cx < 0 ? px + pad : (moved ? px + pad + 1.5 : ox + o_w + 3);
            sy1 = cy < 0 ? py + pad : oy + o_h + 3;
            translate([sx0, sy0, -R - ring_t]) cube([sx1 - sx0, sy1 - sy0, ring_t]);
            translate([px, py, -R - 1])
                cube([pad, pad, shell_h + 1 - (face_pcb + pcb_t) - 0.05]);
        }
}

// Four cantilever tabs rise from the rim frame; barbs latch into the shell's
// wall slots (slot z 3.9..5.7 below the bezel face).
module snap_tabs() {
    slope_frame()
        for (tx = [rx + clr + s_ow*0.25, rx + clr + s_ow*0.75]) {
            translate([tx, oy, 0])           tab();                    // lower edge
            translate([tx, oy + o_h, 0]) mirror([0, 1, 0]) tab();      // upper edge
        }
}
module tab() {  // local: barb faces -y (toward the shell wall), y=0 = opening edge
    translate([-3.5, -0.6, -R - ring_t]) cube([7, 1.9, 1.6]);      // root foot on the frame
    translate([-3.5, 0.15, -R - ring_t]) cube([7, 1.0, R + ring_t - 4.2]);  // flexing shaft
    hull() {                                                        // barb, ramped lead-in
        translate([-3.5, -0.55, -5.55]) cube([7, 1.1, 0.4]);
        translate([-3.5, 0.10, -4.35]) cube([7, 0.5, 0.1]);
    }
}

// ---------------- DAC cradle ----------------
function dac_cx() = OW - wall - 0.5 - dac_h/2;                    // bay against the right wall
function dac_cy() = stand_d - wall - (dac_w/2 - jack_proud + 2.0); // jack end at the back wall
function jack_zc() = wall + standoff_h + dac_t + jack_z;

module dac_standoffs() {
    for (sx = [-1, 1], sy = [-1, 1])
        translate([dac_cx() + sx*dac_hole_dy/2, dac_cy() + sy*dac_hole_dx/2, wall - 0.01])
            difference() {
                cylinder(d=standoff_od, h=standoff_h);
                translate([0, 0, -1.4]) cylinder(d=pilot_d, h=standoff_h + 1.4 + 0.01);
            }
}

// ---------------- RCA DAC mount (dac_type = "rca") ----------------
// The board stands flat against the back wall on two vertical ribs (one per
// hole column, two M2.5 screws each), right edge 0.5mm off the right wall so
// the RCA barrels reach through their holes. Wires exit its left-edge header.
function rca_x1()  = OW - wall - 0.5;              // board right edge
function rca_face() = stand_d - wall - rca_so;     // PCB rests on this plane
function rca_jy(axis) = rca_face() - rca_bt - axis; // jack barrel axis, world Y

module rca_ribs() {
    for (hx = [rca_x1() - rca_bw + rca_hole_in, rca_x1() - rca_hole_in])
        translate([hx, 0, 0]) difference() {
            union() {
                // full-height pilaster, fused to the back wall and the floor
                translate([-6, rca_face() + 1.5, wall - 0.1])
                    cube([12, rca_so - 1.4, rca_z0 + rca_bh - rca_hole_in + 3.5 - wall]);
                // seating bosses around each screw hole (1.5 relief between
                // them so the board's through-hole joints don't rock it)
                for (hz = [rca_z0 + rca_hole_in, rca_z0 + rca_bh - rca_hole_in])
                    translate([0, rca_face(), hz]) rotate([-90, 0, 0])
                        cylinder(d=9, h=1.6);
            }
            for (hz = [rca_z0 + rca_hole_in, rca_z0 + rca_bh - rca_hole_in])
                translate([0, rca_face() - 1, hz]) rotate([-90, 0, 0])
                    cylinder(d=rca_pilot, h=rca_so + 2);
        }
    // base step under the jack-side pilaster: its top face is at the board's
    // bottom edge (rca_z0), so the board rests on it during assembly and the
    // step carries the static load instead of the screws. (Left side has the
    // battery below, so it gets no step.)
    translate([rca_x1() - rca_hole_in - 6, rca_face() - 1.2, wall - 0.1])
        cube([12, stand_d - wall - rca_face() + 1.3, rca_z0 - wall + 0.1]);
}

// ---------------- battery pocket ----------------
module battery_pocket() {
    px = wall; py = 21;                       // pocket outer corner
    pw = bat_l + 2*bat_clr + 2*rib_t;         // outer size
    pd = bat_w + 2*bat_clr + 2*rib_t;
    difference() {
        union() {
            translate([px, py, wall - 0.1]) cube([pw, rib_t, 8]);              // front rib (low)
            translate([px, py + pd - rib_t, wall - 0.1]) cube([pw, rib_t, rib_h]); // back rib
            for (x = [px, px + pw - rib_t])                                    // side ribs
                translate([x, py, wall - 0.1]) cube([rib_t, pd, rib_h]);
        }
        // JST pigtail gap, front rib, DAC side
        translate([px + pw - 26, py - 1, wall + 1]) cube([16, rib_t + 2, rib_h + 2]);
    }
}

// =============================================================
module rcube(s, r=2) {
    hull() for (x = [r, s[0]-r], y = [r, s[1]-r])
        translate([x, y, 0]) cylinder(r=r, h=s[2]);
}
module teardrop(d, h) {   // round hole + 45-degree roof, axis = local Z
    linear_extrude(h) union() {
        circle(d=d);
        polygon([[-d*0.354, d*0.354], [d*0.354, d*0.354], [0, d*0.71]]);
    }
}

// =============================================================
if (part == "shell") shell();
if (part == "stand") stand();
if (part == "both") {
    stand();
    translate([OW + 15, 0, 0]) shell();
}
if (part == "fit") {   // assembled preview: shell ghosted into the recess
    stand();
    %slope_frame() translate([rx + clr, ry + clr + s_oh, 0]) rotate([180, 0, 0]) shell();
    // rca mode: ghost the DAC board + jack housing envelopes for clearance checks
    if (dac_type == "rca") %union() {
        translate([rca_x1() - rca_bw, rca_face() - rca_bt, rca_z0])
            cube([rca_bw, rca_bt, rca_bh]);
        for (jz = rca_j)   // RCA housings + barrels through the wall
            translate([rca_x1() - rca_house_l, rca_face() - rca_bt - rca_house,
                       rca_z0 + jz - rca_house/2])
                cube([rca_house_l + rca_proud, rca_house, rca_house]);
        translate([rca_x1() - 13, rca_face() - rca_bt - 5.2, rca_z0 + rca_j35 - 3])
            cube([13 + j35_proud, 5.2, 6]);
    }
}
if (part == "jig") {
    // Fast-printing fit-check: just the stand's DAC corner — both standoff
    // ribs with their screw bosses, the base step, and the right-wall jack
    // holes. Prints as it sits. Render with dac_type="rca":
    //   openscad -o rca-jig.stl -D 'part="jig"' -D 'dac_type="rca"' case-stand.scad
    intersection() {
        stand();
        translate([OW - 63, 38, -1]) cube([64, stand_d - 37, rca_z0 + rca_bh + 7]);
    }
}
