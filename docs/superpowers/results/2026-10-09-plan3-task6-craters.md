# Plan 3 Task 6 - craters, depth boxes, hard ground, floor, bullet slab (results)

Date 2026-10-09, game Experimental 192624, map Arland (holed), mortar shells instead of grenades (user).
Measured from console.log of each run (recounted by script from the raw lines, 21:25). No git.

## Runs

| Run | Log | Crater lines | Stats craters | Stats skipped | Floors at centre | Hard broken lines | Seam lines (non-zero) | MUD errors |
|---|---|---|---|---|---|---|---|---|
| Single player 15:28-15:43 (3 plays) | ArmaReforgerWorkbench/logs/logs_2026-10-09_13-01-43 lines 14151-15628 | 65 | 65 | 1 | -4 m x64, -8 m x1 | 0 | 23 (0) | 0 |
| DS 20:37 client | PeerPlugin1/logs/logs_2026-10-09_20-37-15 | 9 | 9 | 1 | -4 m x5, -0.5 m x4 | 22 | 8 (0) | 0 |
| DS 20:37 server | server/logs/logs_2026-10-09_20-37-14 | 0 | 0 | 10 | - | 0 | 11 (0) | 0 |
| DS 20:47 client | PeerPlugin1/logs/logs_2026-10-09_20-47-30 | 1 | 1 | 0 | -4 m x1 | 2 | 9 (0) | 0 |
| DS 21:19 server (bounds fix) | server/logs/logs_2026-10-09_21-18-59 | 3 | 3 | 0 | -0.5 m x2, -4 m x1 | 4 | 10 (0) | 0 |
| DS 21:19 client (bounds fix) | PeerPlugin1/logs/logs_2026-10-09_21-19-01 | 3 | 3 | 0 | -0.5 m x2, -4 m x1 | 4 | 10 (0) | 0 |

Checksums: crater lines = stats craters on every run (65=65, 9=9, 0=0, 1=1, 3=3, 3=3).
DS 20:37 server: 13 'crater skip' probe lines vs stats skipped 10 - the last 3 blasts came after the last 10 s stats
line before play stop (stats lines print every 10 s).
Single player run 2 was first counted as 32 crater lines vs 27 - a miscount; recount 27 = 27 (ledger).

## Cases

| Case | Expected | Logged | Visual (user) | Result |
|---|---|---|---|---|
| (a) field bowl | depth 2 m, floor -4 m, centre ~ -2 m | 'depth 2 m, floor at centre -4 m, centre now -1.991..-1.999 m' | "worked super well" | PASS |
| (b) near a kept edge | lock < 1, shallower | centre now -1.94 m x2 (2892-2895 / 3025-3043) | no step at road | PASS |
| (c) house / border lock | no ground moved inside | 'centre now 0 m' x4 (2947-2954 / 2415-2425, 3073.73 / 2036.28) | no hole under walls | PASS |
| (d) centre on kept cell (road) | skipped | stats skipped 1 (SP); 'crater skipped (kept cell 836 1503)' (DS 20:37 client) | - | PASS |
| depth box 8 m | floor -8 m | 1 crater 'floor at centre -8 m' at <1486.51, 38.64, 2326.75>, centre -2 m (depth 2 < floor) | - | PASS |
| depth box 0.5 m (20 x 20 m at 2140 / 2728) | 'depth 0.5 m, floor at centre -0.5 m, centre now -0.5 m' | 4 craters 'centre now -0.4992..-0.4995 m' (DS 20:37 client), 2 more on server + client 21:19 | - | PASS |
| hard ground (asphalt, flagpole at 1683 / 3011) | 'hard ground broken in patch ...', truck ruts in it after | patches 192722 / 192721 / 192210 / 192209 broken by blasts; truck rutted the broken part | user drove in / out | PASS |
| bullet slab | bullets stop in the crater wall | - | "bullet got stopped" | PASS |
| walk in / out | no fall, no stuck | - | "butter smooth" | PASS |
| drive in / out | no drop through | - | SP fine; DS fine after bounds fix (21:19) | PASS |

## Defects found and fixed in this task

1. Ruts crept over unbroken asphalt (user 20:39). Proven: 'hard ground broken' kept firing while driving, patches
   192209 -> 191697 -> 191696 (only -x / -z). VertexSoft read one cell; a vertex on the shared edge of the hard cell
   on the -x / -z side was pressed, BreakHard broke that cell, and so on. Fix: VertexSoft = lowest softness of the
   cells touching the vertex. DS 20:47: after the blast's own 2 lines, no more 'hard ground broken' while driving; user:
   "a lot less rut, drags a little from the rim then stops" - accepted.
2. Dedicated server made no craters and the truck jumped near craters. Proven: server ground entities had no mesh, so
   their world bounds were a point (<1408,0,3584> .. <1408,0,3584>, vobject 0); traces skipped them: server skin check
   hit 0 of 2000, blasts 'hit null' with y 0 (the shell fell into the terrain hole), wheels rode the heightmap 1 mm
   below. Fix: MUD_BoundsMesh.c - a 2-triangle mesh spanning each chunk / patch on the dedicated server. 21:19: server
   skin hit 2000 / 2000, server craters = client craters (centre -0.498275 vs -0.498413, -1.99918 vs -1.99916), user:
   "100000x smoother".

## Still open (carried)

- Server and client each run their own ground (rpl 0): ruts differ between machines until Task 9 (sync).
- Coarse skin teardown leaves 767 geoms per play stop (known, warning only).
- Worst frame 11 ms on a wheel-made patch at once (ruled case since Task 5).
