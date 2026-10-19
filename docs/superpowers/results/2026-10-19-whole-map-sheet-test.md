# Whole-map mud sheet test (2026-10-19, WB host + PeerTool client)

Quick look test: can the sheet cover all offroad ground. Function / look only (WB) - the ms numbers below are
WB numbers and are NOT perf results (project rule: perf only on a DS / standalone).

## Setup (working copy only, never committed)

- MUD_MudSheet.ENABLED true, SPARSE false (every offroad tile inside RADIUS, not only tiles with mud).
- RADIUS_ON true, RADIUS 40 m, RADIUS_HYST 6 m, RADIUS_MIN 20 m, RAD_BUILD_MS 2, STORED_MS 3.
- MUD_MudZoneEntity.ALL_OFFROAD true. Sheet switches A1 / A2 / A5 off, A3 server layer 1 (DS).
- Logs: server/logs/logs_2026-10-19_22-10-26, PeerPlugin1/logs/logs_2026-10-19_22-10-28 (the client draws the sheet;
  the server log has no 'MUD sheet:' lines, as expected).
- No script errors in either log.

## What the user saw (user eyes)

1. Pop-in: tiles visibly draw in ahead of the truck while driving ("loading effect", looks very bad).
2. Crater: the hole shows about 1 s after the blast.
3. Trench dig (and sometimes a crater hole): the sheet mesh sits higher than the cut and sticks out of the hole.

## MEASURED - radius streaming (client 'MUD sheet:' lines, one per 5 s)

| time | meshes (stored + live) | builds / 5 s | mean ms | dropped / 5 s | to build | build radius at speed | last queue |
|---|---|---|---|---|---|---|---|
| 22:11:05 | 635 + 0 | 978 | 0.54 | 264 | 72 | 34.7 m at 9.2 m/s | 107 |
| 22:11:15 | 637 + 0 | 891 | 0.56 | 770 | 284 | 40 m at 1.1 m/s | 321 |
| 22:11:25 | 627 + 16 | 1174 | 0.56 | 892 | 0 | 40 m at 7.5 m/s | 19 |
| 22:11:35 | 621 + 16 | 1342 | 0.67 | 395 | 14 | 33.0 m at 9.7 m/s | 35 |
| 22:11:45 | 601 + 16 | 1638 | 0.67 | 543 | 0 | 32.8 m at 9.8 m/s | 1 |
| 22:11:55 | 600 + 16 | 1673 | 0.65 | 571 | 0 | 20 m at 20.1 m/s | 0 |
| 22:12:05 | 607 + 16 | 306 | 0.58 | 133 | 0 | 40 m at 0.7 m/s | 0 |
| 22:12:15 | 611 + 16 | 330 | 0.53 | 227 | 0 | 40 m at 6.5 m/s | 9 |
| 22:12:45 | 600 + 16 | 249 | 1.18 | 44 | 0 | 40 m at 4.9 m/s | 0 |
| 22:12:55 | 600 + 16 | 63 | 0.51 | 57 | 0 | 40 m at 0.2 m/s | 0 |

- About 600-640 tile meshes are kept at once (tile = 20 x 20 quads of 0.15 m = 3 m). Driving at ~10 m/s means
  ~900-1700 tile builds and ~400-900 drops per 5 s: the ring around the truck is torn down behind and rebuilt in
  front all the time.
- The build radius shrinks with speed (40 m -> 20 m at 20 m/s), so at speed the visible edge comes closer.
- The to-build queue reached 284 (last queue 321) at 22:11:15: tiles inside the radius not built yet = the pop-in.
- WB 'MUD prof parts': 'sh zones' (the radius builds) 0.6-1.9 ms/frame avg, worst 4-6 ms, the largest sheet part.

## MEASURED - crater timing (seq 45, the first crater)

| time | side | line |
|---|---|---|
| 22:12:09.417 | server | 'MUD world: blast 0.99 kg TNT' (M821 HE shell) |
| 22:12:09.498 | server | 'MUD sheet hash seq 45' |
| 22:12:09.548 | server | voxel probe live 118 ms after seq 45, build over 3 frames |
| 22:12:09.593 | client | 4 patches took the voxel cells (28 ms) |
| 22:12:09.595 | client | sheet 'voxel ground changed ... blast 2.81 m -> blast now: 12 tiles in 2 ms' |
| 22:12:09.630 | client | sheet 'dig now: 1 voxel changes, 20 tiles in 34 ms' |
| 22:12:09.631 | client | voxel probe live 155 ms after seq 45, 14 new meshes shown, build over 3 frames |
| 22:12:10.842 | client | sync layer 'op clearing: 1 crater / dig ops, 2332 cells bared, 2 ms' (5 s window) |

Second crater (seq 46, 22:12:17.0): the same pattern - blast now 9 tiles 1 ms, dig now 12 tiles 24 ms, voxel live
138 ms after seq.

- By the logs, both server and client have the crater and the sheet tiles rebuilt about 0.2 s after the blast
  (server and client share the PC clock).
- UNRESOLVED: the user sees about 1 s. Nothing in the log shows a 1 s step. Candidates (INFERENCE, not checked):
  the sheet mesh over the hole is a stored tile rebuilt later through FlushOneBuild (STORED_MS 3 ms budget); the
  new MeshObject only uploads on its first draw (IDA 2026-10-18); the blast smoke / dust hides the hole; a later
  tile re-cover from the sync layer clear (JOB_CLEAR rows through Drain, DRAIN_MS 3) re-draws the sheet over part of
  the hole and a second build bares it. Needs a log line with the frame time when each crater tile mesh is set.

## MEASURED - trench dig (22:12:33-22:12:41, instant trench, 12 pieces)

- 'MUD trench plan: confirm ... pieces 12, instant 1' at 22:12:35.4.
- 19 'dig now' sheet lines between 22:12:37 and 22:12:41 (5-20 tiles each, 14-30 ms each), plus many
  'voxel ground changed ... 0 mud tiles to redo' lines.
- Sync layer window ending 22:12:40.9: 'op clearing: 71 crater / dig ops, 10178 cells bared, 1272 ms', '71 flushes
  (1187 ms)' - about 17 ms per op flush, run on the op path.
- UNRESOLVED (look bug 3): the sheet sticking out. INFERENCE: the sheet is drawn THICK (0.15 m) above the ground, and
  the taper (TAPER 0.75 m, TAPER_PAD 6 cells) / skirt (SKIRT_ALL) next to bared cells still stand above the cut rim;
  or a tile built before the trench's later pieces keeps the old ground height until its next rebuild. Not checked.

## Other numbers (WB, NOT perf)

- 'MUD mud sheet sync' while driving: 4-7 rut ops per 5 s, up to 76 ms per op spread over frames; slump 65-176 ms
  per 5 s.
- Server (DS): MUD script 0.48-0.68 ms/frame at 30 fps.
