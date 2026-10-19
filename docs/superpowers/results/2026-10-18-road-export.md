# Road export probe (whole-map cut plan 7A.1)

World Editor plugin `MUD Export Roads` (Scripts/WorkbenchGame/Mud/MUD_RoadExportPlugin.c) run on MudArland by the
user 2026-10-18 23:48, Exp 1.9 Tools exe. Output: My Games/ArmaReforgerWorkbench/profile/mud_road_export.txt
(5473 lines, 284 KB). Copy with every per-road line: [data/2026-10-18-mud_road_export.txt](data/2026-10-18-mud_road_export.txt).

**GATE 7A.1: PASS.** A World Editor plugin can read every road's points, width and material.

## Counts (from the file's 'done' line, recounted from raw lines)
- 468 RoadEntity, all 468 have spline points (5004 'sp' lines), 0 without an entity.
- 0 parent shapes found: the parent is a RoadGeneratorEntity (not a ShapeEntity) and the grandparent lookup found no
  shape, so no 'tp' (tessellated) lines. Spline points alone are enough for now (see position check).
- Points cover x 533-3657, z 763-3554. Segment length between points: median 9.4 m, max 1217 m (runway), 4536 segments.
- 11 roads have NO material (width 3 or 4.5, type 2, 189 m).

## Position check (control: the road hide probe's real crosshair hits, logs_2026-10-18_22-31-10)
The probe logged where the crosshair hit each road. Distance from each hit to that road's exported centre line
(straight lines between spline points) must be under half the width.

| Hit x, z | Road # | Width m | Distance to its line m | Inside half width |
|---|---|---|---|---|
| 1227.76, 2385.0 | 133 | 40 | 6.1 | yes |
| 1229.46, 2382.98 | 133 | 40 | 8.5 | yes |
| 1237.13, 2389.4 | 133 | 40 | 4.2 | yes |
| 1237.14, 2389.4 | 133 | 40 | 4.2 | yes |
| 1458.41, 2396.66 | 98 | 8 | 2.0 | yes |
| 1460.52, 2305.69 | 98 | 8 | 1.6 | yes |
| 1461.41, 2309.47 | 98 | 8 | 2.4 | yes |
| 1462.13, 2306.92 | 98 | 8 | 3.2 | yes |
| 970.603, 2582.67 | 127 | 40 | 4.2 | yes |

9 of 9 inside. Each road's bounds also match the probe's bounds size (98: 140 x 435 vs 140 x 434; 127: 404 x 564 vs
398 x 548). Note: a road's GetOrigin is NOT its first spline point (98: origin x 1699.85, points x 1412-1545).
CoordToParent gives parent space; it lands right here, so the generators sit at the world origin. Fragile: 7A.2 should
use GetTransform (world) to be safe.

## Materials (all 468 roads)
Type = the road's Type field: 2 asphalt, 3 dirt road, 5 trail, 0 runway concrete (also 3 Road_Asphalt_E_03 roads), 255 = decal-like (markings,
flower beds, beach debris, panels).

| Material | Type | Roads | Length m | Widths m |
|---|---|---|---|---|
| Road_Dirt_01.emat | 3 | 33 | 14732 | 3.0, 4.0 |
| Road_Dirt_02.emat | 3 | 23 | 9669 | 4.0, 4.5 |
| RunwayBorder_02.emat | 255 | 18 | 7800 | 1.0, 1.5 |
| TrailDirt_01.emat | 5 | 16 | 7244 | 1.0, 1.75 |
| Road_Asphalt_E_02.emat | 2 | 9 | 5978 | 8.0 |
| TrailGravel_01.emat | 5 | 12 | 4090 | 1.75 |
| Runway_Line80_01.emat | 255 | 10 | 3682 | 0.9 |
| ConcretePanelRow_01_A.emat | 255 | 137 | 3068 | 4.0 |
| Road_Asphalt_E_03.emat | 0 | 3 | 1674 | 8.0 |
| FlowerBed_01.emat | 255 | 53 | 1553 | 4.0 |
| Runway_Line20_01.emat | 255 | 7 | 1534 | 0.3 |
| RunwayConcrete_01_40m.emat | 0 | 10 | 1512 | 40.0, 43.0 |
| Beach_NatureDebris_02.emat | 255 | 11 | 1371 | 12.0 |
| Road_Forest_01.emat | 3 | 6 | 1371 | 4.5 |
| RunwayConcrete_01_60m.emat | 0 | 3 | 1314 | 60.0 |
| Beach_NatureDebris_01.emat | 255 | 8 | 1102 | 12.0 |
| RunwayConcrete_01_20m.emat | 0 | 10 | 1085 | 20.0 |
| RunwaySkidMarks_01_Light.emat | 255 | 14 | 979 | 10.0, 20.0, 40.0, 60.0 |
| Road_Asphalt_E_03.emat | 2 | 6 | 903 | 8.0 |
| DrainChannel_02.emat | 255 | 5 | 780 | 1.0 |
| RunwaySkidMarks_01.emat | 255 | 5 | 678 | 40.0, 50.0, 65.0 |
| RunwayBorder_01.emat | 255 | 9 | 643 | 1.0 |
| Road_Asphalt_E_03.emat | 255 | 3 | 568 | 8.0 |
| Road_ConcretePanel_01.emat | 2 | 1 | 511 | 3.5 |
| Decal_DirtTracks_01.emat | 255 | 16 | 480 | 4.0 |
| FuelChannel_01.emat | 255 | 4 | 269 | 2.0 |
| Decal_Asphalt_Patch_Stripe_01.emat | 255 | 6 | 217 | 1.0, 1.5, 2.0, 2.5 |
| (none) | 2 | 11 | 189 | 3.0, 4.5 |
| ConcretePanelRow_01_A.emat | 3 | 1 | 154 | 4.0 |
| Road_Dirt_01.emat | 255 | 3 | 134 | 4.0 |
| ConcretePanelRow_01_B.emat | 255 | 11 | 119 | 4.0 |
| RunwayConcrete_01_10m.emat | 0 | 1 | 104 | 10.0 |
| ConcretePanelRow_01_B.emat | 2 | 1 | 48 | 4.0 |
| Road_Asphalt_E_02.emat | 255 | 1 | 39 | 8.0 |
| Road_Dirt_02.emat | 255 | 1 | 29 | 4.0 |

Checksum: rows sum to 468 roads (file says 468), 75623 m total.

## Bake (7A.2, 2026-10-18)
`python -I Tools/mud_roads.py bake <export> --out Data/Mud/mud_roads.c` -> 468 roads, 168 armor (51624 of 75623 m),
5004 points. Not armor (300 roads, sum checked): ConcretePanelRow_01_A 138, FlowerBed_01 53, RunwayBorder_02 18,
RunwaySkidMarks_01_Light 14, ConcretePanelRow_01_B 12, Beach_NatureDebris_02 11, Runway_Line80_01 10, RunwayBorder_01 9,
Beach_NatureDebris_01 8, Runway_Line20_01 7, Decal_Asphalt_Patch_Stripe_01 6, RunwaySkidMarks_01 5, DrainChannel_02 5,
FuelChannel_01 4 = 300. Armor rule (U1: dirt roads + trails armored): material under Structures/Infrastructure/Roads/,
RunwayConcrete, or none.

## Re-export with curves (2026-10-19 00:32, results/data/2026-10-19-mud_road_export.txt)
- Still 0 spline entities found (source AND runtime parents): the map has no SplineShapeEntity left over its roads.
  434 roads have parent RoadGeneratorEntity, 23 none, 11 lines with an empty material shift the column.
- 191 of 5004 spline points carry explicit tangents. In == out direction on all 191; |tangent| ~ the segment length
  (e.g. 21.2 vs 20.4 / 21.8 m, 64.5 vs 178 / 83 m) -> cubic Hermite on t 0-1 (INFERENCE). Points without tangents get
  Catmull-Rom (guess - the engine's auto rule is unknown).
- Bake: 468 roads all curved, 40702 points every ~2 m, 1.08 MB. Hits 9/9 inside; road 98 hits moved closer to the curve
  than to the straight line (1.6 -> 1.3, 2.4 -> 2.2, 3.2 -> 2.9 m; 2.0 same).
- Curve vs straight line, max per road: 42 of 468 roads bend more than half their width off the straight line. Top:
  RunwayBorder_02 road 109 37.0 m (w 1), Runway_Line20_01 303 18.1 m, RunwayBorder_02 111 15.4 m, Road_Asphalt_E_02
  99 10.4 m and 95 10.2 m (w 8), RunwayBorder_02 125 9.8 m, Road_Dirt_01 348 7.2 m and 361 5.9 m. Runway borders may be
  sharp-cornered shapes that Catmull-Rom rounds wrongly (not armor, decals) - check road 95 / 99 by eye.

## What this means for 7A
- Real asphalt roads (Road_Asphalt_E_02 / E_03) are 9.2 km of 75.6 km (5978 + 1674 + 903 + 568 + 39 m), runway
  concrete 4.0 km (1512 + 1314 + 1085 + 104 m). Most "roads" are dirt roads, trails
  and decals (runway marks, flower beds, beach debris). Only the asphalt / concrete ones need our copy (if U1 = dirt
  roads stay normal ground).
- Open: are points joined by straight lines or curves in the real mesh? On the 8 m road the hits sat 1.6-3.2 m off
  the straight line, which is inside the road but could be a curve. 7A.3 look probe will show it. If curves, export
  the point tangents too (SplinePoints have InTangent / OutTangent - unverified).
- 11 roads with no material: skip or draw with Road_Asphalt_E_02 (decide in 7A.2).
