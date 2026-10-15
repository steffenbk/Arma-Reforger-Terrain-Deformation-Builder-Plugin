# Plan 3 Task 5 - ruts: results (steps 6-9)

Build: Experimental 192624. Map: Arland (holed). Run 2026-10-09 15:19-15:22 (compile 15:19:12), measured from console.log.
Earlier runs (steps 6-7, perf fixes A/B/C, sleep) are in the ledger: .superpowers/sdd/2026-10-09-mud-whole-map-holes-plan3-ground/progress.md.

## Stats per 10 s window

| time | presses | made ahead | made stamps | freed | meshes (ms) | colliders (ms) | smoothing ms | skin swaps ms | patches | deformed | live | asleep | slept | sleep KB back | mem KB | KB per patch change (d mem / d patches) | worst frame ms | skips (height, running total) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 15:20:51.131 |  | 8 | 0 | 0 | 8 (8) | 8 (7) | 0 | 20 | 8 | 0 | 8 | 0 | 0 | 0 | 113439 |  | 4 | 0 |
| 15:21:01.142 |  | 25 | 0 | 0 | 29 (27) | 29 (37) | 2 | 57 | 33 | 1 | 33 | 0 | 0 | 0 | 134231 | 831 | 5 | 844 |
| 15:21:11.157 |  | 2 | 0 | 0 | 14 (12) | 14 (14) | 3 | 4 | 35 | 3 | 35 | 0 | 0 | 0 | 135730 | 749 | 4 | 4371 |
| 15:21:21.158 |  | 0 | 0 | 0 | 16 (16) | 16 (19) | 4 | 0 | 35 | 3 | 35 | 0 | 0 | 0 | 136557 |  | 4 | 7474 |
| 15:21:31.165 |  | 23 | 0 | 0 | 211 (223) | 158 (207) | 74 | 64 | 58 | 10 | 58 | 0 | 0 | 0 | 148053 | 499 | 8 | 7950 |
| 15:21:41.177 |  | 30 | 4 | 0 | 260 (281) | 183 (251) | 115 | 85 | 92 | 18 | 92 | 0 | 0 | 0 | 164549 | 485 | 12 | 7950 |
| 15:21:51.181 |  | 61 | 0 | 0 | 280 (310) | 213 (295) | 117 | 157 | 153 | 41 | 153 | 0 | 0 | 0 | 190682 | 428 | 8 | 7950 |
| 15:22:01.196 |  | 63 | 0 | 9 | 300 (314) | 225 (331) | 127 | 173 | 207 | 60 | 204 | 3 | 3 | 765 | 212218 | 398 | 9 | 7950 |
| 15:22:11.210 |  | 62 | 0 | 0 | 256 (270) | 201 (256) | 101 | 173 | 269 | 82 | 257 | 12 | 9 | 2353 | 237773 | 412 | 9 | 7950 |
| 15:22:21.225 |  | 51 | 0 | 3 | 261 (284) | 197 (264) | 109 | 176 | 317 | 98 | 284 | 33 | 21 | 5708 | 253158 | 320 | 9 | 7950 |
| 15:22:31.236 |  | 34 | 0 | 18 | 276 (300) | 202 (295) | 88 | 156 | 333 | 110 | 281 | 52 | 19 | 5178 | 263030 | 617 | 9 | 7950 |

Checksum: made 8 + 25 + 2 + 0 + 23 + 34 + 61 + 63 + 62 + 51 + 34 = 363; freed 0 + 0 + 0 + 0 + 0 + 0 + 0 + 9 + 0 + 3 + 18 = 30; 363 - 30 = 333; last `patches` = 333. MATCH.
KB per patch change = (mem - previous mem) / (patches - previous patches); blank when patches did not change. Whole drive 15:21:21 -> 15:22:31: (263030 - 136557) / (333 - 35) = 126473 / 298 = 424 KB per patch, with 52 of them asleep at the end (mem also holds new index sets, skin rebuilds, other game allocations).

## Results

- Step 8 (raised object): user drove onto a bridge over a hole field (no ramp placed). `skips (height)` running total 0 -> 844 -> 4371 -> 7474 -> 7950, then flat once off the bridge. Log part PASS; user look under the bridge (no rut): asked 15:30, pending.
- Step 9 budget warning (m_iBudget TEMP 5, back to 2000 after): `MUD world: 6 deformed patches, past the budget 5 (kept for the match; live ~340-400 KB, asleep less)` once, 15:21:25.659. PASS.
- Step 9 drive: ~70 s of off-road driving (15:21:21-15:22:31), not the planned 3 min / 2 km; deformed 3 -> 110, patches 35 -> 333. Worst frame 4-9 ms on every line except 12 ms once (15:21:41): wheels 12 = a wheel made a patch at once (index + collider + cover in one frame), the user-ruled case. Gate <= 10: PASS except that ruled case.
- Memory: the plan expected ~35-40 KB per deformed patch (plan defect, ledger ruling 15:25). Measured: a live patch ~340-400 KB (frees 14:13); a sleep gives back 255-272 KB in this run (229-284 in the sleep runs). mem/patches at the end: see table.
- Seams: every `MUD world seams` line mismatched 0, deformed verts on a border to the coarse skin 0. Check new: hit = points every line, within 5 mm.
- `hole 0` wheel lines: all on dirt_road.gamemat (kept road cells, expected).

## Log lines (whole, from the last compile)

```
15:19:12.244  PROFILING    : Compiling Game scripts took: 1705.445000 ms
15:19:38.299  ENGINE       : Game destroyed.
15:20:45.715   SCRIPT       : MUD wheel: touches {7C49B6F04E7320A3}Common/Materials/Game/dirt_road.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2379.36,64.3369,2771.67>
15:20:46.724   SCRIPT       : MUD wheel: touches {7C49B6F04E7320A3}Common/Materials/Game/dirt_road.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2379.43,64.3743,2771.87>
15:20:51.131   SCRIPT       : MUD world: last 10 s - made 8 ahead + 0 by stamps, freed 0, meshes 8 (8 ms), colliders 8 (7 ms), smoothing 0 ms, skin swaps 20 ms
15:20:51.131   SCRIPT       : MUD world: patches 8 (deformed 0), mem +113439 KB since ready, worst frame 4 ms, skips (height) 0 | check new: points 23, hit 23, within 5 mm 23, worst 7.62939e-06 m
15:20:51.131   SCRIPT       : MUD world: worst frame split (ms): wheels 0, ahead 4 (cover ), rebuild 0 (), free 0 (0 patches), skin swaps in it 3
15:20:51.131   SCRIPT       : MUD world: sleep - live 8, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
15:20:51.132   SCRIPT       : MUD world seams: pairs 7, shared verts 231, mismatched 0 (worst 0 m), deformed verts on a border to the coarse skin 0
15:20:51.621   SCRIPT       : MUD wheel: touches {7C49B6F04E7320A3}Common/Materials/Game/dirt_road.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2379.43,64.3783,2771.8>
15:20:52.633   SCRIPT       : MUD wheel: touches {7C49B6F04E7320A3}Common/Materials/Game/dirt_road.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2379.41,64.409,2770>
15:20:53.635   SCRIPT       : MUD wheel: touches {7C49B6F04E7320A3}Common/Materials/Game/dirt_road.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2378.99,64.4807,2768.29>
15:20:54.649   SCRIPT       : MUD wheel: touches {7C49B6F04E7320A3}Common/Materials/Game/dirt_road.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2378.86,64.5014,2767.97>
15:20:55.655   SCRIPT       : MUD wheel: touches {7C49B6F04E7320A3}Common/Materials/Game/dirt_road.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2380.48,64.297,2769.71>
15:20:56.654   SCRIPT       : MUD wheel: touches {7C49B6F04E7320A3}Common/Materials/Game/dirt_road.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2384.86,63.7806,2772.12>
15:20:57.664   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2390.93,63.2046,2773.68>
15:20:58.666   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2397.46,62.9277,2774.71>
15:20:59.680   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2403.26,62.678,2775.7>
15:21:00.687   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2405.85,62.5677,2776.18>
15:21:01.142   SCRIPT       : MUD world: last 10 s - made 25 ahead + 0 by stamps, freed 0, meshes 29 (27 ms), colliders 29 (37 ms), smoothing 2 ms, skin swaps 57 ms
15:21:01.142   SCRIPT       : MUD world: patches 33 (deformed 1), mem +134231 KB since ready, worst frame 5 ms, skips (height) 844 | check new: points 105, hit 105, within 5 mm 105, worst 1.52588e-05 m
15:21:01.142   SCRIPT       : MUD world: worst frame split (ms): wheels 0, ahead 5 (index ), rebuild 0 (), free 0 (0 patches), skin swaps in it 0
15:21:01.142   SCRIPT       : MUD world: sleep - live 33, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
15:21:01.144   SCRIPT       : MUD world seams: pairs 46, shared verts 1518, mismatched 0 (worst 0 m), deformed verts on a border to the coarse skin 0
15:21:01.694   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2405.43,62.5856,2776.1>
15:21:02.702   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2403.44,62.6708,2775.74>
15:21:03.704   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2401.34,62.7608,2775.35>
15:21:04.714   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2399.91,62.822,2775.06>
15:21:05.802   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2399.76,62.8287,2774.98>
15:21:06.812   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2400.97,62.7753,2775.61>
15:21:07.813   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2403.65,62.66,2776.23>
15:21:08.827   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2406.33,62.5453,2776.67>
15:21:09.830   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2408.91,62.4353,2777.1>
15:21:10.843   SCRIPT       : MUD wheel: touches {C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2410.64,62.3546,2777.37>
15:21:11.157   SCRIPT       : MUD world: last 10 s - made 2 ahead + 0 by stamps, freed 0, meshes 14 (12 ms), colliders 14 (14 ms), smoothing 3 ms, skin swaps 4 ms
15:21:11.157   SCRIPT       : MUD world: patches 35 (deformed 3), mem +135730 KB since ready, worst frame 4 ms, skips (height) 4371 | check new: points 110, hit 110, within 5 mm 110, worst 1.52588e-05 m
15:21:11.157   SCRIPT       : MUD world: worst frame split (ms): wheels 0, ahead 4 (setup ), rebuild 0 (), free 0 (0 patches), skin swaps in it 0
15:21:11.158   SCRIPT       : MUD world: sleep - live 35, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
15:21:11.159   SCRIPT       : MUD world seams: pairs 48, shared verts 1584, mismatched 0 (worst 0 m), deformed verts on a border to the coarse skin 0
15:21:11.853   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2410.04,62.3868,2777.27>
15:21:12.861   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2407.75,62.4852,2776.87>
15:21:13.873   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2405.43,62.5841,2776.44>
15:21:14.886   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2403.14,62.6822,2776.02>
15:21:15.890   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2401.31,62.7607,2775.67>
15:21:17.178   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2400.98,62.7749,2775.61>
15:21:18.183   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2401.83,62.7386,2775.77>
15:21:19.187   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2403.28,62.6763,2776.04>
15:21:20.291   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2403.48,62.6677,2776.08>
15:21:21.158   SCRIPT       : MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 0, meshes 16 (16 ms), colliders 16 (19 ms), smoothing 4 ms, skin swaps 0 ms
15:21:21.158   SCRIPT       : MUD world: patches 35 (deformed 3), mem +136557 KB since ready, worst frame 4 ms, skips (height) 7474 | check new: points 110, hit 110, within 5 mm 110, worst 1.52588e-05 m
15:21:21.158   SCRIPT       : MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 4 (index 4 ), free 0 (0 patches), skin swaps in it 0
15:21:21.159   SCRIPT       : MUD world: sleep - live 35, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
15:21:21.160   SCRIPT       : MUD world seams: pairs 48, shared verts 1584, mismatched 0 (worst 0 m), deformed verts on a border to the coarse skin 0
15:21:21.304   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2405.47,62.5823,2776.46>
15:21:22.319   SCRIPT       : MUD wheel: touches {EA270CE454C419FD}Common/Materials/Game/wood.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2409.7,62.402,2777.24>
15:21:23.327   SCRIPT       : MUD wheel: touches {7C49B6F04E7320A3}Common/Materials/Game/dirt_road.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2415.26,62.1486,2776.64>
15:21:24.335   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2419.33,62.4217,2773.76>
15:21:25.336   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2422.72,63.4993,2770.38>
15:21:25.659   SCRIPT    (W): MUD world: 6 deformed patches, past the budget 5 (kept for the match; live ~340-400 KB, asleep less)
15:21:26.354   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2424.42,63.9578,2766>
15:21:27.365   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2425.67,64.432,2761.2>
15:21:28.375   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2426.76,64.8819,2756.75>
15:21:29.381   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2427.56,65.2287,2753.38>
15:21:30.389   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2428.16,65.4602,2750.85>
15:21:31.165   SCRIPT       : MUD world: last 10 s - made 23 ahead + 0 by stamps, freed 0, meshes 211 (223 ms), colliders 158 (207 ms), smoothing 74 ms, skin swaps 64 ms
15:21:31.165   SCRIPT       : MUD world: patches 58 (deformed 10), mem +148053 KB since ready, worst frame 8 ms, skips (height) 7950 | check new: points 196, hit 196, within 5 mm 196, worst 1.52588e-05 m
15:21:31.165   SCRIPT       : MUD world: worst frame split (ms): wheels 1, ahead 4 (index ), rebuild 3 (smooth 2 collider 1 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
15:21:31.166   SCRIPT       : MUD world: sleep - live 58, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
15:21:31.168   SCRIPT       : MUD world seams: pairs 89, shared verts 2937, mismatched 0 (worst 3.78676e-06 m), deformed verts on a border to the coarse skin 0
15:21:31.402   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2429.07,65.8048,2747.1>
15:21:32.423   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2430.12,66.1553,2743.33>
15:21:33.427   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2430.96,66.4343,2740.38>
15:21:34.430   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2431.44,66.5796,2738.69>
15:21:35.441   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2328.24,88.1108,2445.25>
15:21:36.959   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2328.06,87.9617,2445.76>
15:21:37.959   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2327.84,88.1452,2444.53>
15:21:38.977   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2326.78,88.4146,2441.67>
15:21:39.978   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2324.27,88.4804,2438.14>
15:21:40.995   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2319.53,87.9999,2434.96>
15:21:41.177   SCRIPT       : MUD world: last 10 s - made 30 ahead + 4 by stamps, freed 0, meshes 260 (281 ms), colliders 183 (251 ms), smoothing 115 ms, skin swaps 85 ms
15:21:41.178   SCRIPT       : MUD world: patches 92 (deformed 18), mem +164549 KB since ready, worst frame 12 ms, skips (height) 7950 | check new: points 313, hit 313, within 5 mm 313, worst 1.52588e-05 m
15:21:41.178   SCRIPT       : MUD world: worst frame split (ms): wheels 12, ahead 0 (index collider cover ), rebuild 0 (), free 0 (0 patches), skin swaps in it 3
15:21:41.178   SCRIPT       : MUD world: sleep - live 92, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
15:21:41.181   SCRIPT       : MUD world seams: pairs 144, shared verts 4752, mismatched 0 (worst 3.78676e-06 m), deformed verts on a border to the coarse skin 0
15:21:42.001   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2312.47,86.5642,2433.66>
15:21:43.015   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2304.36,84.549,2434.61>
15:21:44.024   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2295.88,82.2942,2437.5>
15:21:45.026   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2286.41,79.9519,2440.91>
15:21:46.042   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2275.5,77.6941,2444.09>
15:21:47.048   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2264.08,75.8204,2447.95>
15:21:48.060   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2253.03,74.6113,2451.76>
15:21:49.079   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2242.55,73.6994,2455.55>
15:21:50.089   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2232.42,72.9163,2459.15>
15:21:51.102   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2222.43,72.2437,2462.72>
15:21:51.181   SCRIPT       : MUD world: last 10 s - made 61 ahead + 0 by stamps, freed 0, meshes 280 (310 ms), colliders 213 (295 ms), smoothing 117 ms, skin swaps 157 ms
15:21:51.181   SCRIPT       : MUD world: patches 153 (deformed 41), mem +190682 KB since ready, worst frame 8 ms, skips (height) 7950 | check new: points 558, hit 558, within 5 mm 558, worst 1.52588e-05 m
15:21:51.182   SCRIPT       : MUD world: worst frame split (ms): wheels 1, ahead 4 (cover ), rebuild 3 (smooth 2 collider 1 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 3
15:21:51.182   SCRIPT       : MUD world: sleep - live 153, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
15:21:51.187   SCRIPT       : MUD world seams: pairs 252, shared verts 8316, mismatched 0 (worst 3.81283e-06 m), deformed verts on a border to the coarse skin 0
15:21:52.111   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2212.87,71.6986,2466.25>
15:21:53.120   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2203.49,71.217,2469.8>
15:21:54.124   SCRIPT       : MUD wheel: touches {A5388C69AEDCBF4A}Common/Materials/Game/grass_lush.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2194.53,70.7696,2472.99>
15:21:55.134   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2185.7,70.3347,2476.07>
15:21:56.145   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2177.26,69.8148,2479.14>
15:21:57.164   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2168.85,69.2631,2482.04>
15:21:58.178   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2160.53,68.6614,2484.84>
15:21:59.196   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2152.26,68.0427,2487.69>
15:22:00.212   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2144.03,67.4409,2490.67>
15:22:01.196   SCRIPT       : MUD world: last 10 s - made 63 ahead + 0 by stamps, freed 9, meshes 300 (314 ms), colliders 225 (331 ms), smoothing 127 ms, skin swaps 173 ms
15:22:01.196   SCRIPT       : MUD world: patches 207 (deformed 60), mem +212218 KB since ready, worst frame 9 ms, skips (height) 7950 | check new: points 809, hit 809, within 5 mm 809, worst 1.52588e-05 m
15:22:01.196   SCRIPT       : MUD world: worst frame split (ms): wheels 1, ahead 4 (index ), rebuild 4 (smooth 2 collider 2 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
15:22:01.197   SCRIPT       : MUD world: sleep - live 204, asleep 3 | slept 3 (gave back 765 KB, 255 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
15:22:01.204   SCRIPT       : MUD world seams: pairs 351, shared verts 11583, mismatched 0 (worst 3.81283e-06 m), deformed verts on a border to the coarse skin 0
15:22:01.222   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2135.98,66.7413,2493.75>
15:22:02.222   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2127.82,65.9405,2496.98>
15:22:03.239   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2119.49,64.826,2500.59>
15:22:04.242   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2112.61,63.0135,2507.18>
15:22:05.255   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2105.7,61.2007,2514.74>
15:22:06.274   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2098.3,59.4055,2522.7>
15:22:07.283   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2091.19,57.9134,2530.24>
15:22:08.287   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2085.17,57.2894,2537.08>
15:22:09.292   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2079.22,55.5583,2543.42>
15:22:10.302   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2072.08,53.0503,2550.25>
15:22:11.210   SCRIPT       : MUD world: last 10 s - made 62 ahead + 0 by stamps, freed 0, meshes 256 (270 ms), colliders 201 (256 ms), smoothing 101 ms, skin swaps 173 ms
15:22:11.210   SCRIPT       : MUD world: patches 269 (deformed 82), mem +237773 KB since ready, worst frame 9 ms, skips (height) 7950 | check new: points 1024, hit 1024, within 5 mm 1024, worst 1.52588e-05 m
15:22:11.210   SCRIPT       : MUD world: worst frame split (ms): wheels 1, ahead 4 (index ), rebuild 4 (index 4 ), free 0 (0 patches), skin swaps in it 0
15:22:11.210   SCRIPT       : MUD world: sleep - live 257, asleep 12 | slept 9 (gave back 2353 KB, 261 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
15:22:11.219   SCRIPT       : MUD world seams: pairs 456, shared verts 15048, mismatched 0 (worst 3.81283e-06 m), deformed verts on a border to the coarse skin 0
15:22:11.301   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2063.99,50.2981,2557.6>
15:22:12.318   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2054.93,48.4079,2564.92>
15:22:13.331   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2045.9,46.1395,2567.04>
15:22:14.344   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2038.25,46.0116,2561.11>
15:22:15.356   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2031.83,46.6966,2555.04>
15:22:16.368   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2023.41,46.4613,2554.44>
15:22:17.384   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2016.78,45.9296,2558.43>
15:22:18.967   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2011.38,45.0002,2568.1>
15:22:19.974   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2010.05,44.6311,2574.07>
15:22:20.991   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2008.51,44.191,2579.85>
15:22:21.225   SCRIPT       : MUD world: last 10 s - made 51 ahead + 0 by stamps, freed 3, meshes 261 (284 ms), colliders 197 (264 ms), smoothing 109 ms, skin swaps 176 ms
15:22:21.225   SCRIPT       : MUD world: patches 317 (deformed 98), mem +253158 KB since ready, worst frame 9 ms, skips (height) 7950 | check new: points 1213, hit 1213, within 5 mm 1213, worst 1.52588e-05 m
15:22:21.225   SCRIPT       : MUD world: worst frame split (ms): wheels 1, ahead 4 (index ), rebuild 4 (index 4 ), free 0 (0 patches), skin swaps in it 0
15:22:21.225   SCRIPT       : MUD world: sleep - live 284, asleep 33 | slept 21 (gave back 5708 KB, 271 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
15:22:21.237   SCRIPT       : MUD world seams: pairs 543, shared verts 17919, mismatched 0 (worst 3.81283e-06 m), deformed verts on a border to the coarse skin 0
15:22:22.008   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <2006.41,43.8508,2585.63>
15:22:23.027   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2004.32,43.472,2591.51>
15:22:24.037   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2002.3,43.1896,2597.46>
15:22:25.050   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2000.09,42.993,2603.17>
15:22:26.063   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <1995.78,42.8542,2606.65>
15:22:27.069   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <1991.15,42.8955,2609.47>
15:22:28.081   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <1988.47,43.0172,2613.9>
15:22:29.095   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <1988.04,43.1563,2618.02>
15:22:30.107   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 0 | mass 11690 kg, rut depth 0.35 m | at <1988.8,43.0927,2621.49>
15:22:31.116   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <1991.25,42.6329,2625.59>
15:22:31.236   SCRIPT       : MUD world: last 10 s - made 34 ahead + 0 by stamps, freed 18, meshes 276 (300 ms), colliders 202 (295 ms), smoothing 88 ms, skin swaps 156 ms
15:22:31.237   SCRIPT       : MUD world: patches 333 (deformed 110), mem +263030 KB since ready, worst frame 9 ms, skips (height) 7950 | check new: points 1334, hit 1334, within 5 mm 1334, worst 1.52588e-05 m
15:22:31.237   SCRIPT       : MUD world: worst frame split (ms): wheels 1, ahead 2 (collider ), rebuild 3 (mesh 1 smooth 1 collider 1 (old body 0, new body 0) ), free 3 (1 patches), skin swaps in it 3
15:22:31.237   SCRIPT       : MUD world: sleep - live 281, asleep 52 | slept 19 (gave back 5178 KB, 272 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
15:22:31.249   SCRIPT       : MUD world seams: pairs 571, shared verts 18843, mismatched 0 (worst 3.81283e-06 m), deformed verts on a border to the coarse skin 0
15:22:32.133   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <1995.34,41.8583,2629.99>
15:22:33.144   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2000.55,41.7779,2633.79>
15:22:34.148   SCRIPT       : MUD wheel: touches {D95CA83488B14558}Common/Materials/Game/grass_lush_tall.gamemat | hole 1 | mass 11690 kg, rut depth 0.35 m | at <2004.23,41.6896,2635.96>
15:22:35.202  ENGINE       : Game destroyed.
```
