# Offline gap checks: ruts, craters, trenches, tunnels (2026-10-16)

Tool: `python Tools/gap_checks.py ruts | rutr | tunnel | blast | road | trench | stack` (game formulas copied from
MUD_VoxelProbe.c CarveFloor / CarveTunnel / Carve / CutTrench + hard columns, MUD_WorldGround.c rut cap; meshing =
Tools/dc_vertex_proto.py, surface-nets mean vertex as CellPos). Labels: MEASURED = this run, offline, the game's own
formulas; nothing here was seen in game yet. Ground = dc_vertex_proto's gentle test slope (0.04 x + 0.02 z + waves).

## 1. Rut wall slopes on the 0.25 m floor grid (CarveFloor, 8 passes of 0.08 m, one wheel track)
Cap = MUD_WorldGround.c:1191 `min(0.08 + t x 0.025, 0.35)`. Patch cells split on a diagonal (both splits counted).
Tracks at 0 / 15 / 30 / 45 deg to the grid, run past the grid (no rut ends).

| Vehicle | Cap m | Profile | Deepest m | Rut wall slope p90 deg (worst of the 4 angles) | Steepest triangle deg |
|---|---|---|---|---|---|
| UAZ ~2.5 t | 0.143 | 1-k2 (code today) | 0.143 | 23.1 | 24.7 |
| UAZ ~2.5 t | 0.143 | (1-k2)^2 (plan Step 4) | 0.143 | 22.6 | 22.6 |
| UAZ ~2.5 t | 0.143 | cosine | 0.143 | 22.9 | 22.9 |
| M923 / Ural ~11 t | 0.35 | 1-k2 (code today) | 0.35 | 46.4 | 48.5 |
| M923 / Ural ~11 t | 0.35 | (1-k2)^2 (plan Step 4) | 0.35 | 45.6 | 45.6 |
| M923 / Ural ~11 t | 0.35 | cosine | 0.35 | 46.1 | 46.1 |

Rut START / END (vehicle starts or stops, track ends inside the grid): steepest triangle 60.4 deg - the rut ends
in a 0.35 m drop over ~0.25 m (CarveFloor skips samples past a segment end, `u >= 1`).

Rut radius sweep (cap 0.35, code profile, no ends):
| RUT_R m | Rut wall slope p90 deg | Steepest triangle deg |
|---|---|---|
| 0.5 (today) | 46.4 | 48.5 |
| 0.6 | 40.7 | 44.3 |
| 0.75 | 37.9 | 39.1 |
| 1.0 | 31.5 | 32.3 |

Reading: the profile shape barely matters; depth / radius decides the wall. The plan's "<= ~45 deg where wheels run"
(displaced-ruts plan Step 4, from the one-ray wheel, IDA 2026-10-15 item 5) is missed by heavy trucks at RUT_R 0.5
and met from RUT_R 0.6. Light vehicles are far under it.

## 2. A tunnel dug to the deepest floor (CarveTunnel, FLOOR 3 m)
Player on foot holding T walks +x, one dig per 0.5 m, looks down (pitch -0.7) until the floor stops sinking, then
level. Feet reached undug - 3.0 m after 4 digs. Field = sample columns at 0.5 m along the axis.

| x m | Roof thickness (sample field) m | Clearance m | Floor under undug m | Solid samples in the roof |
|---|---|---|---|---|
| 6.0 | 0.86 | 2.14 | 3.0 | 2 |
| 6.5 | 1.12 | 1.88 | 3.0 | 2 |
| 7.0 - 9.5 | 0.84 - 0.86 | 2.14 - 2.16 | 3.0 | 2 |
| 10.0 - 13.5 | 0.84 - 0.87 | 2.13 - 2.16 | 3.0 | 1 - 2 |
| 14.0 - 16.0 | 0.86 - 0.87 | 2.13 - 2.14 | 3.0 | 2 |

Drawn mesh (rays down through the polygonized field, level part): roof 0.94 - 1.22 m near the axis, 1.15 - 1.79 m at
the tunnel sides. No hole in the roof; the 7 one-surface spots on the axis are the open entrance ramp (x 4.0 - 5.5).
Reading: on flat ground a full-height tunnel has ~0.85 m of roof in the field (1-2 solid samples), ~1 m drawn.
A roof rule of "under 1.0 m collapses" would collapse nearly all of it.

## 3. A blast inside that tunnel (Carve with the hit 0.3 m over the tunnel floor at x 12)
Counted: sample columns that were NOT open before and are open to the sky after (air from the tunnel floor up).
| Charge (FullDepth) | Depth m | New open columns (0.5 m grid) | Widest open radius m |
|---|---|---|---|
| RPG HEAT 0.51 kg x 0.6 | 0.60 | 52 | 2.09 |
| a 0.76 kg charge (example) | 1.27 | 150 | 3.55 |
| 81 mm 0.99 kg | 1.43 | 185 | 4.02 |
Reading: Carve's funnel above the hit (`up > 0`) is air up to wp + FLOOR + rim + 1.5 m, so any crater-making blast in
a tunnel opens a hole to the sky ~4-8 m across. (Mortars cannot land in a tunnel; rockets and charges can. Charges
<= 0.3 kg make no crater at all - m_fCraterMinTnt.)

## 4. A crater next to a road (hard columns never dug)
Road = kept terrain for x < 8; the column on the line is hard; crater centre 1 / 2 / 3 m from the road edge.
| Crater | Centre from road edge m | Drop from the road edge to the first dug sample (0.5 m in) m | Slope over that 0.5 m deg |
|---|---|---|---|
| O-832DU 0.86 m | 1.0 | 0.81 | 58 |
| O-832DU 0.86 m | 2.0 | 0.47 | 43 |
| O-832DU 0.86 m | 3.0 | -0.10 (rim) | - |
| 81 mm 1.43 m | 1.0 | 1.39 | 70 |
| 81 mm 1.43 m | 2.0 | 1.22 | 68 |
| 81 mm 1.43 m | 3.0 | 0.79 | 58 |
| Hydra 2.04 m | 1.0 | 2.01 | 76 |
| Hydra 2.04 m | 2.0 | 1.89 | 75 |
| Hydra 2.04 m | 3.0 | 1.63 | 73 |
Reading: the voxel crater stops dead at the road edge - a 0.8-2 m drop within 0.5 m. The old heightfield faded depth
to 0 over m_fEdgeFade 1 m with a smoothstep (CODE MUD_WorldGround.c EdgeLock); the voxel path has no fade.

## 5. Trench cross-sections (CutTrench, 6 layers = 1.5 m, one straight 8 m piece)
"Drawn floor" = mesh within 5 cm of its own bottom, averaged over 4 cross-sections (range in brackets).
| Floor width planned m | Angle deg | Centre offset m | Drawn floor width m | Bottom m (planned 1.5) | Wall slope p50 / max deg (planned 60) |
|---|---|---|---|---|---|
| 0.5 | 0 | 0.0 | 0.34 (0.26-0.44) | 1.41 | 49 / 60 |
| 0.5 | 0 | 0.25 | 0.47 (0.34-0.52) | 1.46 | 56 / 60 |
| 0.5 | 30 | 0.0 | 0.50 (0.42-0.54) | 1.42 | 56 / 60 |
| 1.0 | 0 | 0.0 | 0.72 (0.64-0.78) | 1.50 | 56 / 60 |
| 1.0 | 0 | 0.25 | 0.46 (0.34-0.54) | 1.50 | 49 / 60 |
| 1.0 | 30 | 0.0 | 0.62 (0.48-0.74) | 1.49 | 57 / 61 |
| 1.5 | 0 | 0.0 | 0.92 (0.80-1.02) | 1.50 | 49 / 60 |
| 1.5 | 0 | 0.25 | 1.22 (1.16-1.28) | 1.50 | 56 / 60 |
| 1.5 | 30 | 0.0 | 0.97 (0.84-1.08) | 1.50 | 56 / 60 |
| 2.0 | 0 | 0.0 | 1.71 (1.64-1.78) | 1.50 | 56 / 60 |
| 2.0 | 0 | 0.25 | 1.44 (1.32-1.54) | 1.50 | 49 / 60 |
| 2.0 | 30 | 0.0 | 1.55 (1.46-1.70) | 1.50 | 56 / 60 |
Reading: the widths were picked as multiples of the old 0.25 m heightfield grid (MUD_TrenchLayout.c WidthOf comment);
on 0.5 m voxel samples the drawn floor is 0.25-0.55 m narrower than planned and changes with where the trench lies on
the grid (1.0 m plan: 0.46 m at one offset, 0.72 m at another). The default 1.0 m trench can draw a floor narrower
than the prone collider (0.6 m, our Character_Base.et override). Depth is right; walls draw ~49-57 deg.

## 6. Repeated hits on one spot (voxel path, hits scattered +-1 m)
| Charge | Hit | Deepest point under undug m | Old heightfield stack cap m |
|---|---|---|---|
| 81 mm 1.43 m | 1 | 1.44 | 2.39 |
| 81 mm 1.43 m | 2 | 2.88 | 2.39 |
| 81 mm 1.43 m | 3-6 | 3.00 (FLOOR) | 2.39 |
| O-832DU 0.86 m | 1 | 0.85 | 1.44 |
| O-832DU 0.86 m | 2 | 1.56 | 1.44 |
| O-832DU 0.86 m | 3 | 2.21 | 1.44 |
| O-832DU 0.86 m | 4-6 | 3.00 (FLOOR) | 1.44 |
Reading: each hit is a fresh Carve centred on the current (already dug) surface, so depth adds up to FLOOR. The
heightfield's stack cap (min(2.5, 1.67 x full)) is not applied on the voxel path (Crater() calls Blast before it).
