# U1 deep tunnels: offline checks (2026-10-17)

Tool: `python Tools/u1_deep_tunnel_checks.py all` (MEASURED, offline model; CELL 0.5, ports of MUD_VoxelProbe.c
CarveTunnel / Carve C1 bowl / CarveSphere as of 2026-10-17 after C1-C4 + U2; ground = dc_vertex_proto.top_of, which
rises 0.04 m per m along x). Full stdout below. The game's character movement is NOT modelled: feet follow the dug
floor one 0.5 m step per click with a 0.6 m step-up (as u2_level_check.py).

## What it decides
| Question | Answer (from the tables) | Build choice |
|---|---|---|
| Does the new clamp stop floor refills? | Yes. Sphere (C4) in a 5 m tunnel with FLOOR 3: old clamp refills 26 (RPG) / 56 (81 mm) samples, new clamp 0 / 0 | `d = max(d, min(old, floorY - y))` in Carve, CarveSphere, CarveTunnel |
| Which floor for the C4 sphere? | With FLOOR 3 + new clamp the sphere digs NOTHING in a 5 m tunnel (new air 0: no chamber). With TUNNEL_FLOOR 5: 2 (RPG) / 42 (81 mm) new air, 0 refilled | CarveSphere uses TUNNEL_FLOOR (it only runs under a roof) |
| Crater on the surface over a 5 m tunnel | New clamp alone still refills 44 tunnel samples (81 mm; > 1 m under the undug ground, up to 3.05 m): the RIM bump (`d = old + bump`) lowers the tunnel ceiling by up to rimH. Not the floor clamp | NEW, not in the plan: rim bump not added to air under the column's top solid sample ("rim top only"): 0 tunnel refills; the 44 left are the rim itself above ground (depth -0.20 .. -0.01) |
| Carve's gj1 reach (FLOOR vs TUNNEL_FLOOR) at a 5 m shaft bottom | 0 lid columns either way (the 1.5 m margin covers 5 m; at 10 m it would not) | gj1 uses TUNNEL_FLOOR (costs a few more Value reads; safe for the 10 m step) |
| Slope cap (P2) | Drop per click 0.7 (today): ramp mean 38.7 deg, max step 40.0. 0.6: 32.5 / 34.2. 0.5: 26.8 / 30.4, 8.5 m of ramp to 4.6 m deep. Straight down (flat look 0): a 90 deg shaft in 8 clicks | TUNNEL_DOWN_MAX 0.5 (5 deg margin under the 35 deg target, the model is not the game's walker); a straight-down look digs ahead along the body facing (no in-place shaft) |

## Full output
### Clamp: blasts in / over a tunnel dug to feet ~4.5 m (TUNNEL_FLOOR 5)
Air = samples with value < 0 deeper than undug - 3 m (where FLOOR 3 clamps). Refilled = air before -> solid after.
New air = solid before -> air after (the chamber). Hit 'in tunnel' = tunnel floor + 0.3 m at x 12; 'surface' =
the undug ground over the tunnel at x 12. 'roof' = RoofOver at the hit (C4 sphere when true).
'rim top only' (bowl): the rim bump is not added to air under the column's top solid sample (a buried hole).

| Blast | Depth m | Hit | Roof | Op (floor) | Clamp | Air below undug-3 before | after | Refilled (all, incl. the rim over air above ground) | Refilled > 1 m under undug (tunnel) | Refilled below undug-3 | Refilled depth range m | New air |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| RPG HEAT | 0.6 | in tunnel | True | sphere (FLOOR 3) | old | 228 | 202 | 26 | 26 | 26 | 3.86 .. 4.47 | 0 |
| RPG HEAT | 0.6 | in tunnel | True | sphere (FLOOR 3) | new | 228 | 228 | 0 | 0 | 0 | - | 0 |
| RPG HEAT | 0.6 | in tunnel | True | sphere (TUNNEL_FLOOR 5) | old | 228 | 230 | 0 | 0 | 0 | - | 2 |
| RPG HEAT | 0.6 | in tunnel | True | sphere (TUNNEL_FLOOR 5) | new | 228 | 230 | 0 | 0 | 0 | - | 2 |
| RPG HEAT | 0.6 | in tunnel | True | sphere (FLOOR 3, rim top only) | old | 228 | 202 | 26 | 26 | 26 | 3.86 .. 4.47 | 0 |
| RPG HEAT | 0.6 | in tunnel | True | sphere (FLOOR 3, rim top only) | new | 228 | 228 | 0 | 0 | 0 | - | 0 |
| RPG HEAT | 0.6 | surface | False | bowl (FLOOR 3) | old | 228 | 228 | 5 | 1 | 0 | -0.07 .. 1.86 | 2 |
| RPG HEAT | 0.6 | surface | False | bowl (FLOOR 3) | new | 228 | 228 | 5 | 1 | 0 | -0.07 .. 1.86 | 2 |
| RPG HEAT | 0.6 | surface | False | bowl (TUNNEL_FLOOR 5) | old | 228 | 228 | 5 | 1 | 0 | -0.07 .. 1.86 | 2 |
| RPG HEAT | 0.6 | surface | False | bowl (TUNNEL_FLOOR 5) | new | 228 | 228 | 5 | 1 | 0 | -0.07 .. 1.86 | 2 |
| RPG HEAT | 0.6 | surface | False | bowl (FLOOR 3, rim top only) | old | 228 | 228 | 4 | 0 | 0 | -0.07 .. -0.01 | 2 |
| RPG HEAT | 0.6 | surface | False | bowl (FLOOR 3, rim top only) | new | 228 | 228 | 4 | 0 | 0 | -0.07 .. -0.01 | 2 |
| 81 mm | 1.43 | in tunnel | True | sphere (FLOOR 3) | old | 228 | 172 | 56 | 56 | 56 | 3.00 .. 4.50 | 0 |
| 81 mm | 1.43 | in tunnel | True | sphere (FLOOR 3) | new | 228 | 228 | 0 | 0 | 0 | - | 0 |
| 81 mm | 1.43 | in tunnel | True | sphere (TUNNEL_FLOOR 5) | old | 228 | 270 | 0 | 0 | 0 | - | 42 |
| 81 mm | 1.43 | in tunnel | True | sphere (TUNNEL_FLOOR 5) | new | 228 | 270 | 0 | 0 | 0 | - | 42 |
| 81 mm | 1.43 | in tunnel | True | sphere (FLOOR 3, rim top only) | old | 228 | 172 | 56 | 56 | 56 | 3.00 .. 4.50 | 0 |
| 81 mm | 1.43 | in tunnel | True | sphere (FLOOR 3, rim top only) | new | 228 | 228 | 0 | 0 | 0 | - | 0 |
| 81 mm | 1.43 | surface | False | bowl (FLOOR 3) | old | 228 | 218 | 97 | 51 | 10 | -0.20 .. 3.09 | 60 |
| 81 mm | 1.43 | surface | False | bowl (FLOOR 3) | new | 228 | 225 | 90 | 44 | 3 | -0.20 .. 3.05 | 60 |
| 81 mm | 1.43 | surface | False | bowl (TUNNEL_FLOOR 5) | old | 228 | 225 | 90 | 44 | 3 | -0.20 .. 3.05 | 60 |
| 81 mm | 1.43 | surface | False | bowl (TUNNEL_FLOOR 5) | new | 228 | 225 | 90 | 44 | 3 | -0.20 .. 3.05 | 60 |
| 81 mm | 1.43 | surface | False | bowl (FLOOR 3, rim top only) | old | 228 | 218 | 54 | 10 | 10 | -0.20 .. 3.09 | 60 |
| 81 mm | 1.43 | surface | False | bowl (FLOOR 3, rim top only) | new | 228 | 228 | 44 | 0 | 0 | -0.20 .. -0.01 | 60 |

### Lid: a crater at the bottom of an open shaft dug straight down to the floor (in place)
Lid columns = columns with air that has solid above it (a 'ceil' crossing) that had none before the blast.

| Blast | Depth m | Shaft bottom under undug m | Carve gj1 reach | Clamp floor | Lid columns | New air samples |
|---|---|---|---|---|---|---|
| RPG HEAT | 0.6 | 5.00 (roof False) | FLOOR 3 (today) | FLOOR 3, new clamp | 0 | 66 |
| RPG HEAT | 0.6 | 5.00 (roof False) | TUNNEL_FLOOR 5 | FLOOR 3, new clamp | 0 | 66 |
| 81 mm | 1.43 | 5.00 (roof False) | FLOOR 3 (today) | FLOOR 3, new clamp | 0 | 588 |
| 81 mm | 1.43 | 5.00 (roof False) | TUNNEL_FLOOR 5 | FLOOR 3, new clamp | 0 | 588 |

### Slope: dig down at the max drop per click, to feet 4.5 m (TUNNEL_FLOOR 5), then 8 m level
Per 0.5 m step: feet drop. Angle = atan(drop / 0.5). Walk-out = the same steps climbed back. Target <= 35 deg.

| Max drop per click m (= pitch clamp x TUNNEL_SLOPE) | Ramp steps | Ramp length m | Mean deg | Max step deg | Max step drop m | Reached depth m |
|---|---|---|---|---|---|---|
| 0.7 | 11 | 5.5 | 38.7 | 40.0 | 0.42 | 4.66 |
| 0.6 | 14 | 7.0 | 32.5 | 34.2 | 0.34 | 4.74 |
| 0.5 | 17 | 8.5 | 26.8 | 30.4 | 0.29 | 4.63 |
| 0.45 | 19 | 9.5 | 23.9 | 29.7 | 0.29 | 4.59 |
| 0.4 | 21 | 10.5 | 21.2 | 29.1 | 0.28 | 4.52 |
| 0.35 | 24 | 12.0 | 18.6 | 28.3 | 0.27 | 4.57 |
| 0.3 | 28 | 14.0 | 16.0 | 27.4 | 0.26 | 4.61 |
| straight down (flat look 0, today) | 8 | 0.0 | 90 | 90 | - | 5.00 |

Per step (x m: drop m / depth m):
- drop 0.7: 3.5:0.38/0.41, 4.0:0.38/0.82, 4.5:0.40/1.25, 5.0:0.41/1.69, 5.5:0.42/2.14, 6.0:0.40/2.56, 6.5:0.38/2.97, 7.0:0.40/3.38, 7.5:0.41/3.81, 8.0:0.42/4.25, 8.5:0.40/4.66
- drop 0.6: 3.5:0.28/0.31, 4.0:0.32/0.65, 4.5:0.34/1.02, 5.0:0.31/1.36, 5.5:0.32/1.70, 6.0:0.34/2.06, 6.5:0.29/2.38, 7.0:0.33/2.73, 7.5:0.33/3.08, 8.0:0.30/3.39, 8.5:0.34/3.74, 9.0:0.32/4.07, 9.5:0.31/4.39, 10.0:0.34/4.74
- drop 0.5: 3.5:0.19/0.22, 4.0:0.29/0.54, 4.5:0.23/0.80, 5.0:0.26/1.09, 5.5:0.26/1.37, 6.0:0.25/1.64, 6.5:0.27/1.93, 7.0:0.24/2.20, 7.5:0.27/2.48, 8.0:0.24/2.73, 8.5:0.27/3.02, 9.0:0.24/3.26, 9.5:0.27/3.55, 10.0:0.24/3.79, 10.5:0.27/4.08, 11.0:0.23/4.33, 11.5:0.27/4.63
- drop 0.45: 3.5:0.15/0.18, 4.0:0.29/0.49, 4.5:0.16/0.68, 5.0:0.27/0.99, 5.5:0.17/1.18, 6.0:0.26/1.48, 6.5:0.19/1.69, 7.0:0.25/1.96, 7.5:0.20/2.18, 8.0:0.24/2.44, 8.5:0.21/2.66, 9.0:0.23/2.90, 9.5:0.22/3.14, 10.0:0.22/3.37, 10.5:0.23/3.61, 11.0:0.21/3.84, 11.5:0.24/4.10, 12.0:0.20/4.32, 12.5:0.24/4.59
- drop 0.4: 3.5:0.11/0.13, 4.0:0.28/0.44, 4.5:0.13/0.60, 5.0:0.25/0.89, 5.5:0.16/1.07, 6.0:0.23/1.33, 6.5:0.17/1.52, 7.0:0.21/1.75, 7.5:0.19/1.96, 8.0:0.20/2.17, 8.5:0.20/2.38, 9.0:0.19/2.58, 9.5:0.19/2.79, 10.0:0.20/3.00, 10.5:0.19/3.20, 11.0:0.20/3.42, 11.5:0.19/3.63, 12.0:0.20/3.85, 12.5:0.20/4.07, 13.0:0.19/4.29, 13.5:0.20/4.52
- drop 0.35: 3.5:0.07/0.09, 4.0:0.27/0.39, 4.5:0.10/0.52, 5.0:0.24/0.79, 5.5:0.12/0.94, 6.0:0.21/1.18, 6.5:0.13/1.33, 7.0:0.20/1.56, 7.5:0.13/1.71, 8.0:0.20/1.92, 8.5:0.14/2.07, 9.0:0.20/2.28, 9.5:0.15/2.44, 10.0:0.19/2.64, 10.5:0.15/2.80, 11.0:0.19/3.01, 11.5:0.15/3.17, 12.0:0.19/3.39, 12.5:0.15/3.57, 13.0:0.18/3.78, 13.5:0.15/3.96, 14.0:0.18/4.17, 14.5:0.15/4.36, 15.0:0.18/4.57
- drop 0.3: 3.5:0.03/0.05, 4.0:0.26/0.34, 4.5:0.06/0.43, 5.0:0.23/0.69, 5.5:0.07/0.79, 6.0:0.21/1.03, 6.5:0.07/1.12, 7.0:0.21/1.36, 7.5:0.09/1.46, 8.0:0.20/1.67, 8.5:0.09/1.77, 9.0:0.20/1.98, 9.5:0.09/2.08, 10.0:0.19/2.29, 10.5:0.10/2.40, 11.0:0.19/2.61, 11.5:0.10/2.72, 12.0:0.19/2.94, 12.5:0.10/3.07, 13.0:0.18/3.28, 13.5:0.10/3.41, 14.0:0.18/3.62, 14.5:0.10/3.76, 15.0:0.18/3.96, 15.5:0.11/4.09, 16.0:0.18/4.29, 16.5:0.11/4.42, 17.0:0.18/4.61


## 10 m + slope choices (2026-10-17, user: TUNNEL_FLOOR 10, let the player choose the slope)
`python Tools/u1_deep_tunnel_checks.py deep10` (MEASURED offline, same model; grid down to -14 m, 40 m long).
Decides: Carve gj1 with TUNNEL_FLOOR (built) still opens a crater at a 10 m shaft bottom (FLOOR 3 reach: no new air)
and makes no lid. Slope choices built (X in shovel TUNNEL mode): 0.35 / 0.5 / 0.7 / 0.85 / 1.0 m per click =
19 / 27 / 39 / 47 / 54 deg mean. The character climbs ~50 deg (IDA 2026-10-15): 0.85 is borderline, 1.0 a trap
(the player's choice, said in the hint).

### Lid: a crater at the bottom of an open shaft dug straight down to the floor (in place, TUNNEL_FLOOR 10)
Lid columns = columns with air that has solid above it (a 'ceil' crossing) that had none before the blast.

| Blast | Depth m | Shaft bottom under undug m | Carve gj1 reach | Clamp floor | Lid columns | New air samples |
|---|---|---|---|---|---|---|
| RPG HEAT | 0.6 | 10.00 (roof False) | FLOOR 3 (today) | FLOOR 3, new clamp | 0 | 0 |
| RPG HEAT | 0.6 | 10.00 (roof False) | TUNNEL_FLOOR 10 | FLOOR 3, new clamp | 0 | 66 |
| 81 mm | 1.43 | 10.00 (roof False) | FLOOR 3 (today) | FLOOR 3, new clamp | 0 | 0 |
| 81 mm | 1.43 | 10.00 (roof False) | TUNNEL_FLOOR 10 | FLOOR 3, new clamp | 0 | 588 |

### Slope: dig down at the max drop per click, to feet 9.5 m (TUNNEL_FLOOR 10), then 8 m level
Per 0.5 m step: feet drop. Angle = atan(drop / 0.5). Walk-out = the same steps climbed back. Target <= 35 deg.

| Max drop per click m (= pitch clamp x TUNNEL_SLOPE) | Ramp steps | Ramp length m | Mean deg | Max step deg | Max step drop m | Reached depth m |
|---|---|---|---|---|---|---|
| 1.0 | 14 | 7.0 | 54.1 | 55.2 | 0.72 | 9.95 |
| 0.85 | 17 | 8.5 | 47.3 | 48.5 | 0.57 | 9.56 |
| 0.7 | 23 | 11.5 | 38.7 | 40.1 | 0.42 | 9.71 |
| 0.6 | 28 | 14.0 | 32.5 | 34.2 | 0.34 | 9.53 |
| 0.5 | 35 | 17.5 | 26.8 | 30.4 | 0.29 | 9.52 |
| 0.35 | 51 | 25.5 | 18.6 | 28.3 | 0.27 | 9.60 |
| straight down (flat look 0, today) | 15 | 0.0 | 90 | 90 | - | 10.00 |

Per step (x m: drop m / depth m):
- drop 1.0: 3.5:0.69/0.72, 4.0:0.68/1.43, 4.5:0.71/2.17, 5.0:0.68/2.88, 5.5:0.69/3.60, 6.0:0.70/4.32, 6.5:0.68/5.03, 7.0:0.72/5.77, 7.5:0.69/6.47, 8.0:0.68/7.17, 8.5:0.70/7.88, 9.0:0.68/8.57, 9.5:0.71/9.29, 10.0:0.65/9.95
- drop 0.85: 3.5:0.57/0.59, 4.0:0.56/1.18, 4.5:0.55/1.75, 5.0:0.54/2.32, 5.5:0.54/2.89, 6.0:0.53/3.44, 6.5:0.53/4.00, 7.0:0.53/4.54, 7.5:0.53/5.09, 8.0:0.53/5.63, 8.5:0.53/6.18, 9.0:0.54/6.73, 9.5:0.56/7.31, 10.0:0.56/7.88, 10.5:0.55/8.44, 11.0:0.54/9.00, 11.5:0.54/9.56
- drop 0.7: 3.5:0.38/0.41, 4.0:0.38/0.82, 4.5:0.40/1.25, 5.0:0.41/1.69, 5.5:0.42/2.14, 6.0:0.40/2.56, 6.5:0.38/2.97, 7.0:0.40/3.38, 7.5:0.41/3.81, 8.0:0.42/4.25, 8.5:0.40/4.66, 9.0:0.38/5.05, 9.5:0.39/5.46, 10.0:0.41/5.88, 10.5:0.42/6.31, 11.0:0.41/6.74, 11.5:0.38/7.14, 12.0:0.39/7.55, 12.5:0.41/7.99, 13.0:0.42/8.44, 13.5:0.41/8.87, 14.0:0.38/9.29, 14.5:0.39/9.71
- drop 0.6: 3.5:0.28/0.31, 4.0:0.32/0.65, 4.5:0.34/1.02, 5.0:0.31/1.36, 5.5:0.32/1.70, 6.0:0.34/2.06, 6.5:0.29/2.38, 7.0:0.33/2.73, 7.5:0.33/3.08, 8.0:0.30/3.39, 8.5:0.34/3.74, 9.0:0.32/4.07, 9.5:0.31/4.39, 10.0:0.34/4.74, 10.5:0.31/5.06, 11.0:0.31/5.39, 11.5:0.34/5.75, 12.0:0.30/6.07, 12.5:0.32/6.42, 13.0:0.33/6.78, 13.5:0.30/7.11, 14.0:0.33/7.47, 14.5:0.32/7.82, 15.0:0.30/8.15, 15.5:0.34/8.52, 16.0:0.31/8.85, 16.5:0.31/9.18, 17.0:0.34/9.53
- drop 0.5: 3.5:0.19/0.22, 4.0:0.29/0.54, 4.5:0.23/0.80, 5.0:0.26/1.09, 5.5:0.26/1.37, 6.0:0.25/1.64, 6.5:0.27/1.93, 7.0:0.24/2.20, 7.5:0.27/2.48, 8.0:0.24/2.73, 8.5:0.27/3.02, 9.0:0.24/3.26, 9.5:0.27/3.55, 10.0:0.24/3.79, 10.5:0.27/4.08, 11.0:0.23/4.33, 11.5:0.27/4.63, 12.0:0.23/4.88, 12.5:0.28/5.18, 13.0:0.23/5.44, 13.5:0.28/5.75, 14.0:0.23/6.00, 14.5:0.28/6.31, 15.0:0.23/6.56, 15.5:0.28/6.86, 16.0:0.23/7.11, 16.5:0.28/7.40, 17.0:0.22/7.64, 17.5:0.28/7.93, 18.0:0.22/8.16, 18.5:0.28/8.45, 19.0:0.22/8.69, 19.5:0.28/8.98, 20.0:0.22/9.22, 20.5:0.28/9.52
- drop 0.35: 3.5:0.07/0.09, 4.0:0.27/0.39, 4.5:0.10/0.52, 5.0:0.24/0.79, 5.5:0.12/0.94, 6.0:0.21/1.18, 6.5:0.13/1.33, 7.0:0.20/1.56, 7.5:0.13/1.71, 8.0:0.20/1.92, 8.5:0.14/2.07, 9.0:0.20/2.28, 9.5:0.15/2.44, 10.0:0.19/2.64, 10.5:0.15/2.80, 11.0:0.19/3.01, 11.5:0.15/3.17, 12.0:0.19/3.39, 12.5:0.15/3.57, 13.0:0.18/3.78, 13.5:0.15/3.96, 14.0:0.18/4.17, 14.5:0.15/4.36, 15.0:0.18/4.57, 15.5:0.16/4.75, 16.0:0.18/4.94, 16.5:0.16/5.12, 17.0:0.18/5.31, 17.5:0.16/5.48, 18.0:0.18/5.67, 18.5:0.16/5.84, 19.0:0.17/6.02, 19.5:0.16/6.20, 20.0:0.17/6.39, 20.5:0.16/6.58, 21.0:0.17/6.77, 21.5:0.16/6.96, 22.0:0.17/7.16, 22.5:0.17/7.36, 23.0:0.17/7.56, 23.5:0.16/7.75, 24.0:0.17/7.95, 24.5:0.16/8.14, 25.0:0.17/8.33, 25.5:0.17/8.51, 26.0:0.17/8.70, 26.5:0.17/8.87, 27.0:0.17/9.06, 27.5:0.16/9.23, 28.0:0.17/9.41, 28.5:0.17/9.60
