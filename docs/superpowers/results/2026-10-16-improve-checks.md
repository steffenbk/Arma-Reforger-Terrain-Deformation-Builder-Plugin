# Offline checks of the proposed fixes (2026-10-16)

Tool: `python Tools/improve_checks.py road | trench | tunnel | blast | stack | jip` (same copied game formulas as
Tools/gap_checks.py; each fix applied as plans/2026-10-16-ground-improvement-plan.md describes). MEASURED = this run,
offline; not seen in game. ESTIMATE / ASSUMED marked where an input is not measured.

## C3 - crater fades to 0 at kept / hard ground (smoothstep over 1 m, as the heightfield's EdgeLock)
| Crater | Centre from road m | Today: drop at 0.5 m / steepest 0.5 m step deg | Fade 1 m: drop at 0.5 m / steepest deg | Depth at the centre with fade m |
|---|---|---|---|---|
| O-832DU 0.86 m | 1.0 | 0.82 / 59 | 0.40 / 43 | 0.86 |
| O-832DU 0.86 m | 2.0 | 0.48 / 44 | 0.13 / 49 | 0.86 |
| O-832DU 0.86 m | 3.0 | -0.10 / 51 | -0.04 / 51 | - (centre off the profile) |
| 81 mm 1.43 m | 1.0 | 1.40 / 70 | 0.69 / 56 | 1.43 |
| 81 mm 1.43 m | 2.0 | 1.23 / 68 | 0.53 / 58 | 1.43 |
| 81 mm 1.43 m | 3.0 | 0.80 / 58 | 0.24 / 58 | - |
| Hydra 2.04 m | 1.0 | 2.02 / 76 | 1.00 / 64 | 2.04 |
| Hydra 2.04 m | 2.0 | 1.90 / 75 | 0.89 / 65 | 2.04 |
| Hydra 2.04 m | 3.0 | 1.64 / 73 | 0.67 / 66 | - |
Reading: the 1 m fade halves the drop at the road edge and keeps the crater depth, but big craters still reach 56-66
deg inside the first 2 m (their own bowl wall). Together with the cosine bowl (C1) the bowl wall itself stays <= ~44
deg (voxel-crater-slope.md), so C1 + C3 together are the fix; a 1.5-2 m fade for big charges is the tuning knob.

## T1 - pad the trench cut by half a sample so the DRAWN floor matches the plan
| Pad each side m | Planned floor m | Drawn floor mean m (min-max, 2 offsets x 3 angles) | Mean error m |
|---|---|---|---|
| 0 (today) | 0.5 | 0.40 (0.23-0.50) | -0.10 |
| 0 (today) | 1.0 | 0.60 (0.46-0.72) | -0.40 |
| 0 (today) | 1.5 | 1.07 (0.92-1.22) | -0.43 |
| 0 (today) | 2.0 | 1.53 (1.42-1.71) | -0.47 |
| 0.125 | 0.5 | 0.57 (0.41-0.71) | +0.07 |
| 0.125 | 1.0 | 0.78 (0.65-0.87) | -0.22 |
| 0.125 | 1.5 | 1.25 (1.11-1.39) | -0.25 |
| 0.125 | 2.0 | 1.77 (1.61-1.86) | -0.23 |
| **0.25** | 0.5 | 0.60 (0.46-0.72) | +0.10 |
| **0.25** | 1.0 | 1.07 (0.92-1.22) | +0.07 |
| **0.25** | 1.5 | 1.53 (1.42-1.71) | +0.03 |
| **0.25** | 2.0 | 2.00 (1.90-2.22) | +0.00 |
Reading: a 0.25 m pad per side (half the 0.5 m sample) puts the drawn floor on the plan (error <= +0.10 m).

## U1 / U2 - tunnels: FLOOR per op kind, and what "level" digging does
| Tunnel FLOOR m | Player digs down to m, then holds level (pitch +0.3) | Roof (sample field) m | Roof drawn on the axis m | Clearance m |
|---|---|---|---|---|
| 3.0 (today) | 3.0 | 0.45-0.57 | 0.53-0.66 | 2.46 |
| 6.0 | 3.0 | 1.42-1.71 | 1.54-1.76 | 2.47 |
| 6.0 | 4.0 | 2.11-2.39 | 2.22-2.45 | 2.47 |
| 6.0 | 5.0 | 2.82-3.12 | 2.86-3.14 | 2.47 |
(The first row is thinner than gap-checks' 0.84-0.87 because there the player kept pitch 0 and the floor sat on
FLOOR; holding level at +0.3 gives a 2.46 m tall tunnel, so under FLOOR 3 the roof is 0.45-0.87 m depending on how
it was dug.)

Level look (pitch 0) from the surface, depth under undug after each 0.5 m step: 0.05, 0.34, 0.43, 0.69, 0.79, 1.03,
1.12, 1.36, 1.46, 1.67, 1.77, 1.98, 2.08, 2.29, 2.40, 2.61, 2.72, 2.94, 3.07, 3.28 m - **a tunnel dug looking
straight ahead sinks ~0.17 m per 0.5 m walked (~18 deg) until it hits FLOOR.** Cause (CODE MUD_VoxelProbe.c:313-316):
the capsule bottom is TUNNEL_MID 0.95 - TUNNEL_HALF 0.45 - TUNNEL_R 0.8 = 0.3 m under the feet at pitch 0.

## C4 - a blast with solid ground over it carves a sphere (r = crater depth) instead of bowl + funnel
| Charge | Depth m | Roof over the hit found (solid sample within 3 m above) | New columns open to the sky | Today (gap-checks) |
|---|---|---|---|---|
| RPG HEAT | 0.60 | yes | 0 | 52 (hole ~4.2 m across) |
| 0.76 kg charge | 1.27 | yes | 0 | 150 |
| 81 mm-size | 1.43 | yes | 0 | 185 (~8 m across) |
Reading: the tunnel survives (chamber r = depth); the roof test reads the field, so it is the same on every machine.

## C2 - the heightfield's stack cap applied on the voxel path (server picks the op depth)
| Charge | Hit | Deepest m today | Deepest m with the cap | Cap m |
|---|---|---|---|---|
| 81 mm 1.43 m | 1 | 1.44 | 1.44 | 2.39 |
| 81 mm 1.43 m | 2 | 2.88 | 2.15 | 2.39 |
| 81 mm 1.43 m | 3 | 3.00 | 2.39 | 2.39 |
| 81 mm 1.43 m | 4-6 | 3.00 | 2.37-2.39 | 2.39 |
| O-832DU 0.86 m | 1 | 0.85 | 0.85 | 1.44 |
| O-832DU 0.86 m | 2 | 1.56 | 1.12 | 1.44 |
| O-832DU 0.86 m | 3 | 2.21 | 1.19 | 1.44 |
| O-832DU 0.86 m | 4-6 | 3.00 | 1.38-1.42 | 1.44 |

## J2 - JIP as a rut-floor snapshot instead of the rut op log
Tracks: random smooth drives, two wheel tracks 1.9 m apart (ASSUMED track width), RUT_R 0.5, profile 1 - k^2, cap
0.35 m. Snapshot = per touched 4 m rut tile (16 x 16 samples, as MUD_RutTile), NONE runs RLE'd as MakeSeed does.
Op log = 2.9 rut ops / s per truck (MEASURED freeze-test) x 339 chars per op (MEASURED jip-size) at an ASSUMED mean
6 m/s.
| Track km | Touched 4 m tiles | Snapshot chars (mm deltas) | Snapshot chars (cm, 1 char per sample) | Op log chars, one pass | Op log / snapshot (mm / cm) | Op log chars, same tracks driven 3 times |
|---|---|---|---|---|---|---|
| 1 | 490 | 135440 | 50579 | 163850 | 1.2x / 3.2x | 491550 |
| 5 | 2392 | 795374 | 294464 | 819250 | 1.0x / 2.8x | 2457750 |
| 10 | 4747 | 1424922 | 537568 | 1638500 | 1.1x / 3.0x | 4915500 |
Reading (corrects deep dive 3.2 "~1 MB, once"): in mm the snapshot is NOT smaller for one pass; in cm it is ~3x
smaller, and it does not grow when the same ground is driven again (the op log does: 9x at 3 passes). cm is lossless
only if every machine stores the floor in cm when it writes it (then server and joiner hold the same numbers).

## C5 - crater size vs measured data (calculation, no sim)
Cube-root calibration from ONE measured point: CRREL 2007 120 mm, 0.72 m apparent depth (READ), with ~2.5 kg Comp B
fill (a KNDS 120 mm round, READ - not the US M933 whose fill was not found) x 1.456, the TntEquivalent the pak gives
the 81 mm M821 warhead (Tools/crater_profile.py, pak read 2026-10-10; used here as a Comp B factor - ASSUMED) -> K = 0.72 / 3.64^(1/3) = 0.468 m per kg^(1/3). Crest diameter = 4.4 x depth (Carve).
| Charge | TNT kg (pak) | Today: depth / crest diameter m | Cube root K 0.468: depth / crest diameter m |
|---|---|---|---|
| VOG-25 40 mm | 0.077 | no crater (<= 0.3 kg) | 0.20 / 0.9 |
| O-832DU 82 mm | 0.40 | 0.87 / 3.8 | 0.34 / 1.5 |
| PG-7VM HEAT (x 0.6) | 0.512 | 0.60 / 2.7 | 0.22 / 1.0 |
| M821 81 mm | 0.99 | 1.43 / 6.3 | 0.47 / 2.1 |
| Hydra M229 | 2.902 | 2.04 / 9.0 | 0.67 / 2.9 |
| TM-62M | 7.5 | 2.80 / 12.3 | 0.92 / 4.0 |
Cross-check: CRREL SR 95-2 measured 81 mm point-detonating craters ~2.3 m across (frozen ground) - matches the
cube-root diameter (2.1 m), not today's 6.3 m. One data point and wet-marsh soil: an order of magnitude, not a fit.
