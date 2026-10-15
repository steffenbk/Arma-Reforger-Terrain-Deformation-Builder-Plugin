# Plan 3 Task 9 - MP live sync (no JIP) - results

Run 2026-10-10 02:39-02:45, Arma Reforger Experimental 192624, Workbench peer tool: local dedicated server + 1 client (PeerPlugin1), both in from the start. `m_fHashEvery` 20 (TEMP default).

- Server log: `Documents/My Games/server/logs/logs_2026-10-10_02-39-19/console.log`
- Client log: `Documents/My Games/PeerPlugin1/logs/logs_2026-10-10_02-39-21/console.log`

Client actions: M923 ruts on a field, 2 grenades (server: `craters 2 (skipped 0)`), 12 shovel swings (2 free-dig squares x 6 layers, then `already 1.5 m deep` x 2).

## Gate

`python -I Tools/compare_hashes.py <server> <client>`:

```
server patches 44, client patches 44, missing 0, extra 0, mismatched 0
```
exit 0

Checksum: server `sent` summed = 4 + 54 + 136 + 142 + 154 + 162 + 147 + 39 + 143 + 34 + 9 + 2 + 16 + 8 + 4 + 0 + 0 + 0 + 0 = **1054**; client `got` summed = 4 + 117 + 135 + 153 + 158 + 146 + 117 + 71 + 114 + 3 + 8 + 8 + 16 + 4 + 0 + 0 + 0 + 0 + 0 = **1054**. Equal: yes. Server final seq 1054. Client bad summed 0.

## Server net lines (19)

| time | sent msgs | ints | ints / msg | got | bad | seq |
|---|---|---|---|---|---|---|
| 02:40:05 | 4 | 418 | 104.5 | 0 | 0 | 4 |
| 02:40:15 | 54 | 6347 | 117.5 | 0 | 0 | 58 |
| 02:40:25 | 136 | 15590 | 114.6 | 0 | 0 | 194 |
| 02:40:35 | 142 | 16620 | 117.0 | 0 | 0 | 336 |
| 02:40:45 | 154 | 16440 | 106.8 | 0 | 0 | 490 |
| 02:40:55 | 162 | 17856 | 110.2 | 0 | 0 | 652 |
| 02:41:05 | 147 | 16473 | 112.1 | 0 | 0 | 799 |
| 02:41:15 | 39 | 4373 | 112.1 | 0 | 0 | 838 |
| 02:41:25 | 143 | 14739 | 103.1 | 0 | 0 | 981 |
| 02:41:35 | 34 | 3139 | 92.3 | 0 | 0 | 1015 |
| 02:41:45 | 9 | 183 | 20.3 | 0 | 0 | 1024 |
| 02:41:55 | 2 | 67 | 33.5 | 0 | 0 | 1026 |
| 02:42:05 | 16 | 134 | 8.4 | 0 | 0 | 1042 |
| 02:42:15 | 8 | 144 | 18.0 | 0 | 0 | 1050 |
| 02:42:25 | 4 | 248 | 62.0 | 0 | 0 | 1054 |
| 02:43:25 | 0 | 0 | - | 0 | 0 | 1054 |
| 02:44:15 | 0 | 0 | - | 0 | 0 | 1054 |
| 02:44:35 | 0 | 0 | - | 0 | 0 | 1054 |
| 02:44:45 | 0 | 0 | - | 0 | 0 | 1054 |
| **sum** | **1054** | **112771** | 107.0 | | | |

## Client net lines (19)

| time | sent | got | bad | JIP held | replayed | dropped | seq |
|---|---|---|---|---|---|---|---|
| 02:40:09 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| 02:40:19 | 0 | 117 | 0 | 0 | 0 | 0 | 0 |
| 02:40:29 | 0 | 135 | 0 | 0 | 0 | 0 | 0 |
| 02:40:39 | 0 | 153 | 0 | 0 | 0 | 0 | 0 |
| 02:40:49 | 0 | 158 | 0 | 0 | 0 | 0 | 0 |
| 02:40:59 | 0 | 146 | 0 | 0 | 0 | 0 | 0 |
| 02:41:09 | 0 | 117 | 0 | 0 | 0 | 0 | 0 |
| 02:41:19 | 0 | 71 | 0 | 0 | 0 | 0 | 0 |
| 02:41:29 | 0 | 114 | 0 | 0 | 0 | 0 | 0 |
| 02:41:39 | 0 | 3 | 0 | 0 | 0 | 0 | 0 |
| 02:41:49 | 0 | 8 | 0 | 0 | 0 | 0 | 0 |
| 02:41:59 | 0 | 8 | 0 | 0 | 0 | 0 | 0 |
| 02:42:09 | 0 | 16 | 0 | 0 | 0 | 0 | 0 |
| 02:42:19 | 0 | 4 | 0 | 0 | 0 | 0 | 0 |
| 02:42:29 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 02:43:19 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 02:43:29 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 02:44:19 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 02:44:39 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| **sum** | | **1054** | **0** | | | | |

## Found on the way

- `HashLines` used `Print(l)`: both machines logged `string l =    'MUD hash ...'`, so the first compare said `no complete MUD hash block` (exit 2). Fixed: `Print(string.Format("%1", l))` in `MUD_WorldGround.c`; `compare_hashes.py` also accepts the old wrapped line (test added, Tools suite 59 passed). The gate above is on the raw logs.
- Open (user 02:45, not measured): shot characters sometimes leave a leg / limbs stuck in trench walls or slopes.

## Every `MUD world net` / `MUD world:` line

### Server (174 lines)

```
MUD world: ready, patch grid 512 x 512, fine 0.25 m, render 0, groove 0, edge mode LOCKED, rpl 1
MUD world: last 10 s - made 0 ahead + 4 by stamps, freed 0, meshes 0 (0 ms), colliders 8 (11 ms), smoothing 2 ms, skin swaps 8 ms
MUD world: patches 4 (deformed 3), mem +13138 KB since ready, worst frame 11 ms, skips (height) 0 | check new: points 16, hit 16, within 5 mm 16, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 11, ahead 0 (index collider cover ), rebuild 0 (), free 0 (0 patches), skin swaps in it 3
MUD world: presses 79, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 3 in 10 s
MUD world: make 7 ms (new patches without their builds) | index sets: hits 8, misses 4, cached 4
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 3, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 11
MUD world: sleep - live 4, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 4 msgs 418 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 4
MUD world: last 10 s - made 19 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 74 (121 ms), smoothing 30 ms, skin swaps 32 ms
MUD world: patches 23 (deformed 7), mem +24400 KB since ready, worst frame 7 ms, skips (height) 0 | check new: points 92, hit 92, within 5 mm 92, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 1, ahead 2 (collider ), rebuild 4 (smooth 1 collider 3 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 612, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 3 in 10 s
MUD world: make 28 ms (new patches without their builds) | index sets: hits 74, misses 23, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 11, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 23, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 54 msgs 6347 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 58
MUD world: last 10 s - made 3 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 160 (243 ms), smoothing 95 ms, skin swaps 5 ms
MUD world: patches 26 (deformed 7), mem +25308 KB since ready, worst frame 6 ms, skips (height) 0 | check new: points 104, hit 104, within 5 mm 104, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 1, ahead 1 (collider ), rebuild 4 (smooth 3 collider 1 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 1602, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 3 in 10 s
MUD world: make 5 ms (new patches without their builds) | index sets: hits 160, misses 6, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 3, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 26, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 136 msgs 15590 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 194
MUD world: last 10 s - made 3 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 165 (261 ms), smoothing 83 ms, skin swaps 6 ms
MUD world: patches 29 (deformed 7), mem +26222 KB since ready, worst frame 7 ms, skips (height) 0 | check new: points 116, hit 116, within 5 mm 116, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 3, ahead 0 (), rebuild 4 (smooth 1 collider 3 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 1614, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 3 in 10 s
MUD world: make 4 ms (new patches without their builds) | index sets: hits 165, misses 3, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 2, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 29, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 142 msgs 16620 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 336
MUD world: last 10 s - made 9 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 175 (266 ms), smoothing 79 ms, skin swaps 14 ms
MUD world: patches 38 (deformed 12), mem +29016 KB since ready, worst frame 7 ms, skips (height) 0 | check new: points 152, hit 152, within 5 mm 152, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 3, ahead 0 (), rebuild 3 (smooth 2 collider 1 (old body 0, new body 0) ), free 1 (0 patches), skin swaps in it 0
MUD world: presses 1674, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 14 ms (new patches without their builds) | index sets: hits 175, misses 18, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 5, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 38, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 154 msgs 16440 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 490
MUD world: last 10 s - made 18 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 194 (284 ms), smoothing 95 ms, skin swaps 30 ms
MUD world: patches 56 (deformed 23), mem +34414 KB since ready, worst frame 7 ms, skips (height) 0 | check new: points 223, hit 223, within 5 mm 223, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 0, ahead 3 (index ), rebuild 4 (index 4 ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 1699, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 34 ms (new patches without their builds) | index sets: hits 194, misses 35, cached 16
MUD world: budget 4 ms - waits: ahead 1, mesh 0, collider 9, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 56, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 162 msgs 17856 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 652
MUD world: last 10 s - made 1 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 172 (255 ms), smoothing 86 ms, skin swaps 1 ms
MUD world: patches 57 (deformed 25), mem +34719 KB since ready, worst frame 6 ms, skips (height) 0 | check new: points 227, hit 227, within 5 mm 227, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 3, ahead 0 (), rebuild 3 (smooth 1 collider 2 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 1648, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 3 in 10 s
MUD world: make 2 ms (new patches without their builds) | index sets: hits 172, misses 3, cached 16
MUD world: budget 4 ms - waits: ahead 1, mesh 0, collider 3, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 57, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 147 msgs 16473 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 799
MUD world: crater at <1575.92,39.5772,2483.64>, depth 2 m, floor at centre -4 m, centre now -1.9942 m
MUD world: crater at <1560.53,40.0193,2478.15>, depth 2 m, floor at centre -4 m, centre now -1.99629 m
MUD world: last 10 s - made 5 ahead + 6 by stamps, freed 0, meshes 0 (0 ms), colliders 49 (79 ms), smoothing 21 ms, skin swaps 17 ms
MUD world: patches 68 (deformed 34), mem +38301 KB since ready, worst frame 6 ms, skips (height) 0 | check new: points 271, hit 271, within 5 mm 271, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 1, ahead 2 (collider ), rebuild 3 (smooth 1 collider 2 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 312, craters 2 (skipped 0, 50 ms, worst 33 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 18 ms (new patches without their builds) | index sets: hits 49, misses 14, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 3, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 68, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 39 msgs 4373 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 838
MUD world: last 10 s - made 20 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 179 (281 ms), smoothing 85 ms, skin swaps 40 ms
MUD world: patches 88 (deformed 39), mem +42416 KB since ready, worst frame 7 ms, skips (height) 0 | check new: points 350, hit 350, within 5 mm 350, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 1, ahead 2 (setup ), rebuild 3 (smooth 0 collider 3 (old body 0, new body 0) ), free 1 (0 patches), skin swaps in it 1
MUD world: presses 1471, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 42 ms (new patches without their builds) | index sets: hits 180, misses 35, cached 16
MUD world: budget 4 ms - waits: ahead 1, mesh 0, collider 12, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 81, asleep 7 | slept 9 (gave back 2450 KB, 272 each) | woke 2 (0 at once by a wheel / stamp; took 548 KB, 274 each)
MUD world: sleep waits 8
MUD world net: sent 143 msgs 14739 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 981
MUD world: last 10 s - made 1 ahead + 0 by stamps, freed 3, meshes 0 (0 ms), colliders 43 (63 ms), smoothing 25 ms, skin swaps 14 ms
MUD world: patches 86 (deformed 40), mem +40697 KB since ready, worst frame 6 ms, skips (height) 0 | check new: points 354, hit 354, within 5 mm 354, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 2, ahead 0 (), rebuild 4 (smooth 2 collider 2 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 364, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 1 ms (new patches without their builds) | index sets: hits 43, misses 2, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 75, asleep 11 | slept 4 (gave back 1080 KB, 270 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 34 msgs 3139 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 1015
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 12 (16 ms), smoothing 2 ms, skin swaps 4 ms
MUD world: patches 86 (deformed 41), mem +40158 KB since ready, worst frame 4 ms, skips (height) 0 | check new: points 354, hit 354, within 5 mm 354, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 4 (index 4 ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 5, most collider pieces 4 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 12, misses 2, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 73, asleep 13 | slept 2 (gave back 540 KB, 270 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 9 msgs 183 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 1024
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 15, meshes 0 (0 ms), colliders 4 (7 ms), smoothing 0 ms, skin swaps 47 ms
MUD world: patches 71 (deformed 41), mem +30766 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 354, hit 354, within 5 mm 354, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 0 (), free 3 (1 patches), skin swaps in it 3
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 1, most collider pieces 4 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 4, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 40, asleep 31 | slept 18 (gave back 4887 KB, 271 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 2 msgs 67 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 1026
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 2, meshes 0 (0 ms), colliders 16 (28 ms), smoothing 2 ms, skin swaps 7 ms
MUD world: patches 69 (deformed 42), mem +29883 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 354, hit 354, within 5 mm 354, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 3 (smooth 0 collider 3 (old body 0, new body 1) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 4, most collider pieces 4 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 16, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 37, asleep 32 | slept 1 (gave back 284 KB, 284 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 16 msgs 134 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 1042
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 3, meshes 0 (0 ms), colliders 8 (10 ms), smoothing 0 ms, skin swaps 5 ms
MUD world: patches 66 (deformed 42), mem +29396 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 354, hit 354, within 5 mm 354, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 0 (), free 3 (1 patches), skin swaps in it 2
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 2, most collider pieces 4 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 8, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 34, asleep 32 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 8 msgs 144 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 1050
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 10, meshes 0 (0 ms), colliders 5 (9 ms), smoothing 0 ms, skin swaps 17 ms
MUD world: patches 56 (deformed 44), mem +26653 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 354, hit 354, within 5 mm 354, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 1, ahead 0 (), rebuild 2 (index 2 ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 42, most collider pieces 4 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 5, misses 3, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 25, asleep 31 | slept 0 (gave back 0 KB, 0 each) | woke 1 (0 at once by a wheel / stamp; took 283 KB, 283 each)
MUD world: sleep waits 0
MUD world net: sent 4 msgs 248 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 1054
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 3, meshes 0 (0 ms), colliders 0 (0 ms), smoothing 0 ms, skin swaps 5 ms
MUD world: patches 53 (deformed 44), mem +39906 KB since ready, worst frame 2 ms, skips (height) 0 | check new: points 354, hit 354, within 5 mm 354, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 1, ahead 1 (), rebuild 0 (), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 0 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 0, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 22, asleep 31 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 1054
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 0 (0 ms), smoothing 0 ms, skin swaps 3 ms
MUD world: patches 53 (deformed 44), mem +39065 KB since ready, worst frame 2 ms, skips (height) 0 | check new: points 354, hit 354, within 5 mm 354, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 0, ahead 2 (), rebuild 0 (), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 0 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 0, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 19, asleep 34 | slept 3 (gave back 823 KB, 274 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 1054
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 0 (0 ms), smoothing 0 ms, skin swaps 12 ms
MUD world: patches 53 (deformed 44), mem +36848 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 354, hit 354, within 5 mm 354, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 0 (), free 3 (0 patches), skin swaps in it 2
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 0 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 0, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 11, asleep 42 | slept 8 (gave back 2212 KB, 276 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 1054
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 0 (0 ms), smoothing 0 ms, skin swaps 4 ms
MUD world: patches 53 (deformed 44), mem +32007 KB since ready, worst frame 2 ms, skips (height) 0 | check new: points 354, hit 354, within 5 mm 354, worst 3.8147e-06 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 0 (), free 2 (0 patches), skin swaps in it 2
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 0 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 0, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 9, asleep 44 | slept 2 (gave back 553 KB, 276 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 1054
```

### Client (172 lines)

```
MUD world: ready, patch grid 512 x 512, fine 0.25 m, render 1, groove 1, edge mode LOCKED, rpl 1
MUD world: last 10 s - made 0 ahead + 4 by stamps, freed 0, meshes 8 (6 ms), colliders 8 (10 ms), smoothing 4 ms, skin swaps 7 ms
MUD world: patches 4 (deformed 3), mem +79916 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 16, hit 16, within 5 mm 15, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 3 (mesh 1 smooth 1 collider 1 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 3 in 10 s
MUD world: make 9 ms (new patches without their builds) | index sets: hits 16, misses 4, cached 4
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 4, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 4 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 21 ahead + 0 by stamps, freed 0, meshes 149 (114 ms), colliders 171 (222 ms), smoothing 83 ms, skin swaps 32 ms
MUD world: patches 25 (deformed 7), mem +116767 KB since ready, worst frame 5 ms, skips (height) 0 | check new: points 100, hit 100, within 5 mm 99, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 2 (index ), rebuild 3 (mesh 1 smooth 0 collider 2 (old body 0, new body 1) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 3 in 10 s
MUD world: make 29 ms (new patches without their builds) | index sets: hits 320, misses 29, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 1, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 25, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 117 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 1 ahead + 0 by stamps, freed 0, meshes 152 (124 ms), colliders 176 (233 ms), smoothing 90 ms, skin swaps 1 ms
MUD world: patches 26 (deformed 7), mem +117846 KB since ready, worst frame 4 ms, skips (height) 0 | check new: points 104, hit 104, within 5 mm 103, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 4 (mesh 1 smooth 1 collider 2 (old body 0, new body 1) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 3 in 10 s
MUD world: make 2 ms (new patches without their builds) | index sets: hits 328, misses 1, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 26, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 135 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 4 ahead + 0 by stamps, freed 0, meshes 172 (147 ms), colliders 185 (248 ms), smoothing 81 ms, skin swaps 7 ms
MUD world: patches 30 (deformed 7), mem +120340 KB since ready, worst frame 5 ms, skips (height) 0 | check new: points 120, hit 120, within 5 mm 119, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 2 (cover ), rebuild 3 (mesh 1 smooth 0 collider 2 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 1
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 3 in 10 s
MUD world: make 5 ms (new patches without their builds) | index sets: hits 357, misses 5, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 30, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 153 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 26 ahead + 0 by stamps, freed 0, meshes 192 (162 ms), colliders 208 (259 ms), smoothing 94 ms, skin swaps 37 ms
MUD world: patches 56 (deformed 20), mem +131688 KB since ready, worst frame 5 ms, skips (height) 0 | check new: points 224, hit 224, within 5 mm 223, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 2 (index ), rebuild 3 (mesh 1 smooth 1 collider 1 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 33 ms (new patches without their builds) | index sets: hits 400, misses 40, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 1, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 56, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 158 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 1 ahead + 0 by stamps, freed 0, meshes 161 (148 ms), colliders 188 (243 ms), smoothing 93 ms, skin swaps 2 ms
MUD world: patches 57 (deformed 23), mem +134411 KB since ready, worst frame 4 ms, skips (height) 0 | check new: points 228, hit 228, within 5 mm 227, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 4 (mesh 2 smooth 0 collider 2 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 3 in 10 s
MUD world: make 1 ms (new patches without their builds) | index sets: hits 349, misses 11, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 57, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 146 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 6 ahead + 0 by stamps, freed 0, meshes 133 (117 ms), colliders 140 (179 ms), smoothing 64 ms, skin swaps 8 ms
MUD world: patches 63 (deformed 26), mem +140173 KB since ready, worst frame 5 ms, skips (height) 0 | check new: points 252, hit 252, within 5 mm 251, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 3 (index ), rebuild 2 (mesh 0 smooth 1 collider 1 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 10 ms (new patches without their builds) | index sets: hits 273, misses 9, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 63, asleep 0 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 117 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 7 ahead + 6 by stamps, freed 0, meshes 93 (74 ms), colliders 101 (135 ms), smoothing 57 ms, skin swaps 22 ms
MUD world: patches 76 (deformed 38), mem +154423 KB since ready, worst frame 5 ms, skips (height) 0 | check new: points 304, hit 304, within 5 mm 303, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 5 (mesh 0 smooth 3 collider 2 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 27 ms (new patches without their builds) | index sets: hits 195, misses 26, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 75, asleep 1 | slept 3 (gave back 830 KB, 276 each) | woke 2 (0 at once by a wheel / stamp; took 547 KB, 273 each)
MUD world: sleep waits 1
MUD world net: sent 0 msgs 0 ints, got 71 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 14 ahead + 0 by stamps, freed 0, meshes 134 (106 ms), colliders 148 (200 ms), smoothing 61 ms, skin swaps 24 ms
MUD world: patches 90 (deformed 40), mem +162903 KB since ready, worst frame 5 ms, skips (height) 0 | check new: points 358, hit 358, within 5 mm 357, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 2 (cover ), rebuild 3 (mesh 1 smooth 0 collider 2 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 1
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 22 ms (new patches without their builds) | index sets: hits 282, misses 23, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 83, asleep 7 | slept 6 (gave back 1621 KB, 270 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 114 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 3, meshes 3 (3 ms), colliders 4 (5 ms), smoothing 0 ms, skin swaps 14 ms
MUD world: patches 87 (deformed 41), mem +168396 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 358, hit 358, within 5 mm 357, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 3 (mesh 1 index 2 ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 7, misses 1, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 74, asleep 13 | slept 6 (gave back 1620 KB, 270 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 3 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 10, meshes 8 (6 ms), colliders 12 (18 ms), smoothing 1 ms, skin swaps 25 ms
MUD world: patches 77 (deformed 41), mem +164828 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 358, hit 358, within 5 mm 357, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 3 (mesh 1 smooth 0 collider 2 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 20, misses 1, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 56, asleep 21 | slept 8 (gave back 2160 KB, 270 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 8 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 6, meshes 8 (5 ms), colliders 8 (10 ms), smoothing 2 ms, skin swaps 25 ms
MUD world: patches 71 (deformed 42), mem +161140 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 358, hit 358, within 5 mm 357, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 1 (), rebuild 2 (mesh 0 smooth 1 collider 1 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 16, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 39, asleep 32 | slept 11 (gave back 3231 KB, 293 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 8 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 5, meshes 16 (11 ms), colliders 16 (24 ms), smoothing 0 ms, skin swaps 8 ms
MUD world: patches 66 (deformed 42), mem +159811 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 358, hit 358, within 5 mm 357, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 3 (mesh 1 smooth 0 collider 2 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 32, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 34, asleep 32 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 16 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 7, meshes 4 (3 ms), colliders 4 (5 ms), smoothing 1 ms, skin swaps 8 ms
MUD world: patches 59 (deformed 44), mem +159682 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 358, hit 358, within 5 mm 357, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 3 (mesh 1 smooth 0 collider 2 (old body 0, new body 0) ), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 3 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 8, misses 2, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 27, asleep 32 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 4 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 3, meshes 0 (0 ms), colliders 1 (1 ms), smoothing 0 ms, skin swaps 5 ms
MUD world: patches 56 (deformed 44), mem +162743 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 358, hit 358, within 5 mm 357, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 3 (index ), rebuild 0 (), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 4 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 1, misses 1, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 25, asleep 31 | slept 0 (gave back 0 KB, 0 each) | woke 1 (0 at once by a wheel / stamp; took 284 KB, 284 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 1, meshes 0 (0 ms), colliders 0 (0 ms), smoothing 0 ms, skin swaps 1 ms
MUD world: patches 55 (deformed 44), mem +201015 KB since ready, worst frame 1 ms, skips (height) 0 | check new: points 358, hit 358, within 5 mm 357, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 1 (), rebuild 0 (), free 0 (0 patches), skin swaps in it 0
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 0 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 0, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 24, asleep 31 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 2, meshes 0 (0 ms), colliders 0 (0 ms), smoothing 0 ms, skin swaps 3 ms
MUD world: patches 53 (deformed 44), mem +201046 KB since ready, worst frame 2 ms, skips (height) 0 | check new: points 358, hit 358, within 5 mm 357, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 0 (), free 2 (1 patches), skin swaps in it 2
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 0 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 0, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 22, asleep 31 | slept 0 (gave back 0 KB, 0 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 0 (0 ms), smoothing 0 ms, skin swaps 5 ms
MUD world: patches 53 (deformed 44), mem +195942 KB since ready, worst frame 3 ms, skips (height) 0 | check new: points 358, hit 358, within 5 mm 357, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 0 (), free 3 (0 patches), skin swaps in it 2
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 0 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 0, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 19, asleep 34 | slept 3 (gave back 824 KB, 274 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
MUD world: last 10 s - made 0 ahead + 0 by stamps, freed 0, meshes 0 (0 ms), colliders 0 (0 ms), smoothing 0 ms, skin swaps 16 ms
MUD world: patches 53 (deformed 44), mem +193962 KB since ready, worst frame 2 ms, skips (height) 0 | check new: points 358, hit 358, within 5 mm 357, worst 0.0127258 m
MUD world: worst frame split (ms): wheels 0, ahead 0 (), rebuild 0 (), free 2 (0 patches), skin swaps in it 2
MUD world: presses 0, craters 0 (skipped 0, 0 ms, worst 0 ms), digs 0, most collider pieces 0 in 10 s
MUD world: make 0 ms (new patches without their builds) | index sets: hits 0, misses 0, cached 16
MUD world: budget 4 ms - waits: ahead 0, mesh 0, collider 0, free 0 | rushed 0 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) 0
MUD world: sleep - live 9, asleep 44 | slept 10 (gave back 2766 KB, 276 each) | woke 0 (0 at once by a wheel / stamp; took 0 KB, 0 each)
MUD world: sleep waits 0
MUD world net: sent 0 msgs 0 ints, got 0 (bad 0), JIP held 0 replayed 0 dropped 0, seq 0
```

