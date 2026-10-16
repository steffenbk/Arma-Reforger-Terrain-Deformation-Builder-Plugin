# Offline fix checks: craters, ruts, trenches, tunnels (2026-10-16)

Tool: `python Tools/fix_checks.py crater | trench | tunnel | ruts` (same field, mesher and metrics as
Tools/gap_checks.py and Tools/crater_slope_voxel.py). Each "fix" is a proposed Enforce change written in Python.
MEASURED offline with the game's formulas; NOTHING here is built in Scripts/ - the user builds on their local
version (ahead of git). Problems these answer: results/2026-10-16-gap-checks.md,
research/2026-10-16-fact-check-ruts-craters-trenches-tunnels.md.

Crater fix (CraterV2): heightfield cosine bowl as a vertical-distance field (max slope 0.9 = 42 deg), road-edge fade
smoothstep over max(1 m, 1.7 x depth), bottom never under undug - min(2.5, 1.67 x depth), and a sphere of r = depth
when solid ground lies within 3 m above the hit (under a roof). Ruts fix: RUT_R = max(0.5, cap / 0.58).
Trench fix: floor rectangle padded 0.25 m each side. Tunnel fix: FLOOR 6 m for tunnel ops only.

## Walk-out (same metric as results/2026-10-16-voxel-crater-slope.md)
| Charge | Depth m | Outer r m | Steepest tri deg | Area > 50 deg % | Steep climb, easiest way m |
|---|---|---|---|---|---|
| 81 mm | 1.43 | 3.49 | 44 | 0.0 | 0.00 |
| Hydra | 2.04 | 4.98 | 44 | 0.0 | 0.00 |
| TM-62M | 2.81 | 6.86 | 43 | 0.0 | 0.00 |


## Road edge (centre 1 / 2 m from the edge; drop over the first 0.5 m and steepest 0.5 m step within 2 m)
| Crater | Centre from edge m | Today: drop in first 0.5 m | Fix: drop in first 0.5 m | Fix: steepest 0.5 m step within 2 m deg |
|---|---|---|---|---|
| 81 mm 1.43 m | 1.0 | 1.40 | 0.14 | 38 |
| 81 mm 1.43 m | 2.0 | 1.23 | 0.04 | 46 |
| Hydra 2.04 m | 1.0 | 2.02 | 0.10 | 37 |
| Hydra 2.04 m | 2.0 | 1.90 | 0.05 | 44 |

## Repeated hits (scattered +-1 m)
| Charge | Hit | Today deepest m | Fix deepest m |
|---|---|---|---|
| 81 mm 1.43 m | 1 | 1.44 | 1.43 |
| 81 mm 1.43 m | 2 | 2.88 | 2.39 |
| 81 mm 1.43 m | 3 | 3.00 | 2.39 |
| 81 mm 1.43 m | 4 | 3.00 | 2.39 |
| 81 mm 1.43 m | 5 | 3.00 | 2.39 |
| 81 mm 1.43 m | 6 | 3.00 | 2.39 |
| O-832DU 0.86 m | 1 | 0.86 | 0.86 |
| O-832DU 0.86 m | 2 | 1.72 | 1.44 |
| O-832DU 0.86 m | 3 | 2.30 | 1.44 |
| O-832DU 0.86 m | 4 | 2.31 | 1.44 |
| O-832DU 0.86 m | 5 | 3.00 | 1.44 |
| O-832DU 0.86 m | 6 | 3.00 | 1.44 |

## Blast inside a tunnel (new sky-open columns)
| Charge | Depth m | Today: new open columns / radius m | Fix (sphere under a roof): new open columns / radius m |
|---|---|---|---|
| RPG HEAT | 0.6 | 52 / 2.09 | 0 / 0.00 |
| 0.76 kg charge | 1.27 | 150 / 3.55 | 0 / 0.00 |


## Trenches
| Floor width planned m | Pad m | Angle | Offset | Drawn floor m (range) | Wall p50 / max deg |
|---|---|---|---|---|---|
| 0.5 | 0.0 | 0 | 0.0 | 0.34 (0.26-0.44) | 49 / 60 |
| 0.5 | 0.0 | 0 | 0.25 | 0.47 (0.34-0.52) | 56 / 60 |
| 0.5 | 0.0 | 30 | 0.0 | 0.50 (0.42-0.54) | 56 / 60 |
| 0.5 | 0.25 | 0 | 0.0 | 0.72 (0.64-0.78) | 56 / 60 |
| 0.5 | 0.25 | 0 | 0.25 | 0.46 (0.34-0.54) | 49 / 60 |
| 0.5 | 0.25 | 30 | 0.0 | 0.62 (0.48-0.74) | 57 / 61 |
| 1.0 | 0.0 | 0 | 0.0 | 0.72 (0.64-0.78) | 56 / 60 |
| 1.0 | 0.0 | 0 | 0.25 | 0.46 (0.34-0.54) | 49 / 60 |
| 1.0 | 0.0 | 30 | 0.0 | 0.62 (0.48-0.74) | 57 / 61 |
| 1.0 | 0.25 | 0 | 0.0 | 0.92 (0.80-1.02) | 49 / 60 |
| 1.0 | 0.25 | 0 | 0.25 | 1.22 (1.16-1.28) | 56 / 60 |
| 1.0 | 0.25 | 30 | 0.0 | 0.97 (0.84-1.08) | 56 / 60 |
| 1.5 | 0.0 | 0 | 0.0 | 0.92 (0.80-1.02) | 49 / 60 |
| 1.5 | 0.0 | 0 | 0.25 | 1.22 (1.16-1.28) | 56 / 60 |
| 1.5 | 0.0 | 30 | 0.0 | 0.97 (0.84-1.08) | 56 / 60 |
| 1.5 | 0.25 | 0 | 0.0 | 1.71 (1.64-1.78) | 56 / 60 |
| 1.5 | 0.25 | 0 | 0.25 | 1.44 (1.32-1.54) | 49 / 60 |
| 1.5 | 0.25 | 30 | 0.0 | 1.55 (1.46-1.70) | 56 / 60 |


## Tunnels
**SUPERSEDED (2026-10-16 later): this table is wrong.** After levelling off, the sim dug at pitch 0, where the
capsule floor is 0.3 m under the feet, so the tunnel kept sinking to FLOOR (feet reached 6 m, hence the thick roofs).
Correct numbers (level = pitch +0.3): results/2026-10-16-improve-checks.md U1/U2 - FLOOR 6 m, dug to 4 m: roof
2.11-2.39 m; to 5 m: 2.82-3.12 m; FLOOR 3 m: 0.45-0.87 m.

| Tunnel FLOOR m | Player levels off at feet under undug m | Roof (sample field) min-max m | Clearance m |
|---|---|---|---|
| 3.0 | 3.0 | 0.84-0.87 | 2.15 |
| 6.0 | 3.0 | 2.26-3.87 | 2.40 |
| 6.0 | 4.0 | 2.87-3.87 | 2.30 |
| 6.0 | 5.0 | 3.33-3.87 | 2.22 |


## Ruts
| Vehicle | Cap m | RUT_R today -> fix m | Wall p90 deg today -> fix | Steepest today -> fix |
|---|---|---|---|---|
| UAZ ~2.5 t | 0.143 | 0.50 -> 0.50 | 23.1 -> 23.1 | 24.7 -> 24.7 |
| ~6 t | 0.230 | 0.50 -> 0.50 | 34.6 -> 34.6 | 36.6 -> 36.6 |
| M923 / Ural ~11 t | 0.350 | 0.50 -> 0.60 | 46.4 -> 40.3 | 48.5 -> 44.1 |

