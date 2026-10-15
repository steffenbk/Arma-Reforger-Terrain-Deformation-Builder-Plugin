# Plan 3 Task 1 - carried risks on the holed Arland

Build: Experimental 192624, Workbench port 5776. Log: `logs/logs_2026-10-09_09-07-37/console.log`, play from 11:35:54
(compile 11:34:47, clean). Probe `MUD_RiskProbeEntity` at 1250 40 2190. User did: wait, threw grenades, M923 get out / in,
free camera at ~300 m (screenshot `2026-10-09-plan3-task1-300m.webp`, user image saved as webp, not png).

## Points

`MUD risk: hole point <1263,39,2203> GetSurfaceY -256 heightmap 39 | SCR_TerrainHelper.GetTerrainY -256 | control <1251,39,2191> GetSurfaceY 39`

## Rows

| Row | Risk | Evidence line(s) | Number | PASS / FAIL |
|---|---|---|---|---|
| R1 | AI group on a hole point | watch 'hole point' +3/+10/+30 s | new 9 / 54 / 54, sunk 0 / 0 / 0 | PASS |
| R2 | AI group on a kept point (control) | watch 'control point' +3/+10/+30 s | new 9 / 73 / 73, sunk 0 / 0 / 0 | PASS (probe valid) |
| R3 | runtime soldier on a hole cell | `soldier spawned 1 at <1266,39.2,2203>`; same watch | sunk 0 | PASS |
| R4 | destroyed house: debris / sink | `house ...House_Village_E_1I01_beige.et at <1260.78,39.4,2209.11> killed 1, holes in its 3 x 3 cells 8 / 9`; watch 'house' | new 9 / 50 / 50, sunk 0 / 0 / 0 (+30 s). Later explosion watches near it (11:39:20): GenericEntity <1260,36.95,2211.32> dy -2.05 hole 1, <1253.94,37.76,2209.06> dy -1.24 hole 0, same position on every check (resting) | PASS (see note) |
| R5 | explosion on a hole cell | 14 `explosion at` lines: hole 1 = 12 (all hit MUD_CoarseChunkEntity), hole 0 = 2 (LakeGeneratorEntity 1, GenericTerrainEntity 1); 12 + 2 = 14 | non-MUD sunk 25 unique entities: hole 0 = 10 (min dy -10.20), hole 1 = 15 (min dy -7.07); 10 + 15 = 25. Lowest y of any = 31.41 (a kept cell) | PASS (see note) |
| R6 | M923 get in / out from the skin | user: get out works, get in does NOT ("same issue as before"); `MUD vehicle access` lines 0 | works: no | FAIL |
| R7 | cracks at hole edges from 300 m | user + screenshot: road edges clean, no gap / light line | seen: no | PASS |
| R8 | `TERRAIN (E)` / `index buffer` warnings | console.log after the 11:34 compile | 0 / 0 | info |
| R9 | shipped callers + RELATIVE_Y prefabs | `2026-10-09-plan3-task1-callers.md` | 109 calls / 78 files / 0 relY prefabs (only 14 of 15457 prefabs have an own Flags line) | PASS (weak) |
| R10 | `SCR_TerrainHelper.GetTerrainY` on a hole point | first risk line | -256 | FAIL (helper returns the hole floor) |

Checksum: rows 10; PASS 7 (R1 R2 R3 R4 R5 R7 R9) + FAIL 2 (R6 R10) + info 1 (R8) = 10.

## Notes

- "Sunk" (origin > 1 m under the heightmap) is not a fall-through signal on this map: kept cells (hole 0) show it as
  often and as deep (10 entities, to -10.20 m) as hole cells (15, to -7.07 m) - ruin pieces, debris, rocks whose origin
  sits under the ground by design. A thing that snapped to the hole floor would read y about -256; the lowest y seen is
  31.41 m. So no debris / effect / character snapped to -256 in this run.
- Residual (inference, not measured): an entity put at -256 (under the terrain box floor -163) could be removed by the
  engine before the +3 s check and never be seen.
- MUD_CoarseChunkEntity showed up as "new" near explosions (up to 19 times at the same origin): our skin chunk entities
  near the camera are made after the baseline. Our own class, excluded from the counts above.
- Explosions did not dent the ground (user): expected, nothing deforms the skin until Task 6.
- Carried ledger items 4 (90663 road hits) and 6 (+85 MB spread build): not pursued: spread build is not the default (Plan 2 ruling).
