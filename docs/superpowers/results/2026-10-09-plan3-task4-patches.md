# Plan 3 Task 4 - fine patches in play (2026-10-09, Exp 192624)

Play started 12:04:33 (log logs_2026-10-09_09-07-37). M923 driven off-road from 1246 / 2175.

## Log lines (whole)

```
12:04:33.120   SCRIPT       : MUD world: ready, patch grid 512 x 512, fine 0.25 m, render 1, groove 1, edge mode LOCKED, rpl 0
12:04:52.434   SCRIPT       : MUD vehicle access: mud hidden from traces while getting in/out
12:04:52.948 SCRIPT       : MUD vehicle access: done after 516 ms, mud traceable again
12:04:53.200   SCRIPT       : MUD world: last 10 s - made 7 ahead + 0 by stamps, freed 0, meshes 7 (16 ms), colliders 7 (25 ms), smoothing 0 ms, skin swaps 9 ms
12:04:53.200   SCRIPT       : MUD world: patches 7 (deformed 0), mem +121117 KB since ready, worst frame 21 ms, skips (height) 0 | check new: points 16, hit 16, within 5 mm 16, worst 3.8147e-06 m
12:05:13.207   SCRIPT       : MUD world: last 10 s - made 36 ahead + 0 by stamps, freed 0, meshes 36 (81 ms), colliders 36 (168 ms), smoothing 0 ms, skin swaps 65 ms
12:05:13.207   SCRIPT       : MUD world: patches 43 (deformed 0), mem +152680 KB since ready, worst frame 28 ms, skips (height) 0 | check new: points 160, hit 160, within 5 mm 160, worst 7.62939e-06 m
12:05:23.208   SCRIPT       : MUD world: last 10 s - made 24 ahead + 0 by stamps, freed 0, meshes 24 (57 ms), colliders 24 (109 ms), smoothing 0 ms, skin swaps 38 ms
12:05:23.209   SCRIPT       : MUD world: patches 67 (deformed 0), mem +165370 KB since ready, worst frame 27 ms, skips (height) 0 | check new: points 254, hit 254, within 5 mm 254, worst 7.62939e-06 m
12:05:33.217   SCRIPT       : MUD world: last 10 s - made 20 ahead + 0 by stamps, freed 0, meshes 20 (45 ms), colliders 20 (95 ms), smoothing 0 ms, skin swaps 33 ms
12:05:33.217   SCRIPT       : MUD world: patches 87 (deformed 0), mem +178807 KB since ready, worst frame 29 ms, skips (height) 0 | check new: points 334, hit 334, within 5 mm 334, worst 7.62939e-06 m
12:05:43.226   SCRIPT       : MUD world: last 10 s - made 1 ahead + 0 by stamps, freed 0, meshes 1 (2 ms), colliders 1 (5 ms), smoothing 0 ms, skin swaps 1 ms
12:05:43.227   SCRIPT       : MUD world: patches 88 (deformed 0), mem +180776 KB since ready, worst frame 13 ms, skips (height) 0 | check new: points 338, hit 338, within 5 mm 338, worst 7.62939e-06 m
12:05:53.231   SCRIPT       : MUD world: last 10 s - made 1 ahead + 0 by stamps, freed 0, meshes 1 (2 ms), colliders 1 (5 ms), smoothing 0 ms, skin swaps 1 ms
12:05:53.231   SCRIPT       : MUD world: patches 89 (deformed 0), mem +181894 KB since ready, worst frame 14 ms, skips (height) 0 | check new: points 342, hit 342, within 5 mm 342, worst 7.62939e-06 m
12:06:03.239   SCRIPT       : MUD world: last 10 s - made 13 ahead + 0 by stamps, freed 0, meshes 13 (27 ms), colliders 13 (63 ms), smoothing 0 ms, skin swaps 21 ms
12:06:03.239   SCRIPT       : MUD world: patches 102 (deformed 0), mem +188706 KB since ready, worst frame 26 ms, skips (height) 0 | check new: points 392, hit 392, within 5 mm 392, worst 7.62939e-06 m
12:06:13.250   SCRIPT       : MUD world: last 10 s - made 26 ahead + 0 by stamps, freed 20, meshes 26 (60 ms), colliders 26 (101 ms), smoothing 0 ms, skin swaps 71 ms
12:06:13.250   SCRIPT       : MUD world: patches 108 (deformed 0), mem +201113 KB since ready, worst frame 16 ms, skips (height) 0 | check new: points 469, hit 469, within 5 mm 469, worst 7.62939e-06 m
12:06:23.266   SCRIPT       : MUD world: last 10 s - made 41 ahead + 0 by stamps, freed 26, meshes 41 (93 ms), colliders 41 (174 ms), smoothing 0 ms, skin swaps 102 ms
12:06:23.266   SCRIPT       : MUD world: patches 123 (deformed 0), mem +218976 KB since ready, worst frame 27 ms, skips (height) 0 | check new: points 604, hit 604, within 5 mm 604, worst 7.62939e-06 m
```

## Per stats line

| time | made ahead | stamps | freed | meshes | ms | ms/mesh | colliders | ms | ms/collider | swaps ms | patches | worst frame ms | points | hit | within 5 mm | worst m |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 12:04:53 | 7 | 0 | 0 | 7 | 16 | 2.29 | 7 | 25 | 3.57 | 9 | 7 | 21 | 16 | 16 | 16 | 3.8147e-06 |
| 12:05:13 | 36 | 0 | 0 | 36 | 81 | 2.25 | 36 | 168 | 4.67 | 65 | 43 | 28 | 160 | 160 | 160 | 7.62939e-06 |
| 12:05:23 | 24 | 0 | 0 | 24 | 57 | 2.38 | 24 | 109 | 4.54 | 38 | 67 | 27 | 254 | 254 | 254 | 7.62939e-06 |
| 12:05:33 | 20 | 0 | 0 | 20 | 45 | 2.25 | 20 | 95 | 4.75 | 33 | 87 | 29 | 334 | 334 | 334 | 7.62939e-06 |
| 12:05:43 | 1 | 0 | 0 | 1 | 2 | 2.00 | 1 | 5 | 5.00 | 1 | 88 | 13 | 338 | 338 | 338 | 7.62939e-06 |
| 12:05:53 | 1 | 0 | 0 | 1 | 2 | 2.00 | 1 | 5 | 5.00 | 1 | 89 | 14 | 342 | 342 | 342 | 7.62939e-06 |
| 12:06:03 | 13 | 0 | 0 | 13 | 27 | 2.08 | 13 | 63 | 4.85 | 21 | 102 | 26 | 392 | 392 | 392 | 7.62939e-06 |
| 12:06:13 | 26 | 0 | 20 | 26 | 60 | 2.31 | 26 | 101 | 3.88 | 71 | 108 | 16 | 469 | 469 | 469 | 7.62939e-06 |
| 12:06:23 | 41 | 0 | 26 | 41 | 93 | 2.27 | 41 | 174 | 4.24 | 102 | 123 | 27 | 604 | 604 | 604 | 7.62939e-06 |

Checksum: made 7 + 36 + 24 + 20 + 1 + 1 + 13 + 26 + 41 = 169; freed 0 + 0 + 0 + 0 + 0 + 0 + 0 + 20 + 26 = 46; 169 - 46 = 123; last patches = 123 -> OK.
Check new: every line hit = points = within 5 mm (last cumulative 604 / 604 / 604, worst 7.6e-06 m).
Totals: meshes 383 ms / 169, colliders 745 ms / 169, skin swaps 341 ms -> about 8.7 ms per new patch.

## Gate

- hit = points = within 5 mm on all 9 stats lines: PASS.
- freed > 0: PASS (20 at 12:06:13, 26 at 12:06:23, while driving; no separate 70 s stand-still window in the log - see ledger ruling).
- get in / out on the patch: PASS (user: could get in on off-road ground; 1 access pair, hidden 516 ms). R6 from Task 1 fixed.
- seams: user saw no crack or flicker where pieces meet. No screenshot saved (seams.png missing).

## Stutter (user: very mild while driving)

Worst frame 21-29 ms in windows that built 7-41 patches; 13-14 ms in windows that built 1. Build cost about 9 ms per patch, colliders the largest share (about 4 ms each). The stutter matches patch builds landing in one frame. Not a gate; noted for later (spread colliders over frames).
