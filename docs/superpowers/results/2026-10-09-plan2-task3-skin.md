# Plan 2 Task 3 - coarse skin over the holed map

Date 2026-10-09. Build: Arma Reforger Experimental 192624 (Workbench 1.9.0.10, built 2026-09-20).
Script: `Scripts/Game/Mud/MUD_CoarseSkin.c` (entity `MUD_CoarseSkinEntity`, first block of `Arlnd_Layers/default.layer`).
Logs: `logs/logs_2026-10-09_09-07-37/script.log` unless noted.

## Holes under the skin
Tiles rewritten 2026-10-09 09:06 with `mud_holes.py write --pins` (fix for RELATIVE_Y entities buried by holes, see ledger):
1396378 -> 1340627 hole cells, 62481 pins of which 55751 were holes. Table: `2026-10-09-write-pins.txt`.

## Init-build log lines (09:16, build at init)
```
MUD coarse: terrain <0,-163,0> .. <4096,148.375,4096>, 64 x 64 chunks of 64 m, render 1, build at init 1, budget 8 ms
MUD coarse: built 1697 chunks with holes + 2399 without = 4096 | hole cells 1340627, tris 2681254 | via init
MUD coarse: work 5230 ms (heights 3594, meshes 423, colliders 1202), wall 5230 ms, 1 frames, worst frame 5230 ms
MUD coarse: trace misses 0, cells joined to slot 0 (> 6 surfaces) 0, mem +263778 KB
MUD coarse: height traces on non-terrain 0 (classes:)
MUD coarse check (build): 2000 hole cells, skin hit 2000, within 5 cm 2000, worst 0.000480652 m
MUD coarse check (late): 2000 hole cells, skin hit 2000, within 5 cm 2000, worst 0.000480652 m
```

Checksums:
- `hole cells 1340627 (skin) = 1340627 (write --pins)`. No census re-run after the pins write; the skin counts every
  cell where `GetSurfaceY < -255`, the same test the census uses.
- `chunks with holes 1697 + without 2399 = 4096` (64 x 64).

## Play-check answers (Task 3 step 6)
- Falls / M923: walking fine everywhere; M923A1 never dropped (08:26, 09:26, 09:36, 09:45 runs).
- See-through / vanishing skin: chunks vanished on camera turn until `ent.Update()` after `SetObject`; after it none found (08:26).
- Buildings under ground: cause = RELATIVE_Y roots on hole cells load at their raw offset. Fixed with pins; 0 RELATIVE_Y roots
  on holes after the fix, CowShed back at 51.9253 (09:08).
- Grass clutter: "seems to be all there" after the pins (09:17). Inference: the missing clutter was the ~46k buried trees/bushes.
- Cracks at roads / shore at distance: none reported. Not checked from 300 m up in a dedicated look.
- Log: `TERRAIN (E): Maximum size of index buffer exceeded` while holed terrain comes into view; the engine regrows by
  250000 each time. No flicker reported.

## Budget table
| run | time | work ms | wall ms | frames | worst frame ms | mem KB | M923 dropped | road hits / retraced |
|---|---|---|---|---|---|---|---|---|
| init | 09:16 | 5230 | 5230 | 1 | 5230 | +263778 | no (user, 08:26 run) | 0 / - |
| 4 ms (run 1, old code) | 09:23 | 5804 | 22665 | 1126 | 7 | +347187 | not asked | 90663 / - |
| 4 ms | 09:26 | 6126 | 24329 | 1208 | 7 | +353148 | no | 0 / 0 |
| 8 ms | 09:36 | 5813 | 15262 | 597 | 12 | +344748 | no | 0 / 0 |
| 16 ms | 09:45 | 5844 | 12519 | 341 | 19 | +341292 | no | 0 / 0 |

Every run: 4096 chunks, 1340627 hole cells, trace misses 0, check build and late 2000 / 2000 / 2000 within 5 cm, worst 0.00048 m.
Worst frame = budget + ~3-4 ms (one chunk), as the plan expected. Wall falls as the budget rises.

## Open
- Run 1 (09:23) took 90663 skin heights from road tops (`RoadEntity`); later runs 0. Not reproduced, cause unknown. HeightAt
  now traces again under a road hit and logs count / mean / max gap; that path has not run yet.
- Spread build uses ~+80-90 MB more than init (+341..353 MB vs +264 MB, 4 runs agree). Not measured why.

## Pick
Default: **build at load** (user, 2026-10-09 09:51). `m_bBuildAtInit` default was already 1; layer block back to
`coords` only. Later: bake skin heights to a data file at hole-write time (heights are ~70% of the build; also removes
the road-hit risk).

## Probe clean-up (Task 3 step 9)
User (09:52): yes, archive rather than delete. `MUD_WorldSkin.c` moved out of the addon to
`My Games/ArmaReforgerWorkbench/mud_backups/archive/scripts/MUD_WorldSkin.c` (keeps the terrain-off experiment).
Its one reference (`MUD_WorldSkinChunkEntity.Cast` in `MUD_BuildingDestructionFix.c`) removed.
`MUD_HoleCensus.c`, `Tools/hole_census.py` and its test stay.
