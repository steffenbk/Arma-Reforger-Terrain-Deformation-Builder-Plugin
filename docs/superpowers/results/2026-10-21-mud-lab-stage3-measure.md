# Mud lab stage 3 - Part 0 measure (2026-10-21)

Plan: `plans/2026-10-21-mud-lab-stage3-plan.md` sec 0. Checkpoint v265 = before stage 3.
Status: meters written (print only, TEST_KIT), compiled clean in WB 2026-10-21 18:17 (no new warnings), not run yet. Rows marked **NEXT** are filled by the next launch.

## 1. New meters (TEST_KIT only, print only, no behaviour change)
| meter | where | log |
|---|---|---|
| A TURN METER | MUD_MudSheet: TurnRate (every sheet frame, followed vehicle heading -> smoothed deg/s; bucket straight / turning >= 8 deg/s / stopped < 1 m/s), TurnEdges (end of Upload when a live slot was built: every drawn live slot's edge record vs the drawn neighbour's, j = 0..TQ, corners counted apart), TurnNote at drape cut (+ skirt quads left), co-build side copied over > 3 cm (A-2), born wait | appended to `MUD sheet builds:` as `TURN METER (... ms ...) \| straight: ... \| turning: ... \| stopped: ...` |
| B under sheet | MUD_WorldGround BuildMeshBand (sheet local, groove on): revealed verts vs `MUD_MudSheet.FullCoverAt`; groove section upload ms; B10 'would hold' test on queued builds (BHoldMeter, before KeepMeshY) | new line `MUD world B under sheet 10 s:` after the `MUD band: grid path` line |
| B8 | MUD_LookRuts: segments whose two lead-shifted ends pass `MUD_MudSheet.LocalStampsAt` | appended to `MUD look ruts: 10 s` as `B8 meter: segments sheet local stamps itself N` |
| D crater notes | MUD_MudSheet PreNote -> PreMeter: notes, joining (no local controlled entity), most in one sheet frame, dropped at the 32 cap | appended to `MUD sheet prebuild:` and `MUD sheet builds:` as `PART D notes ...` |

Notes on the meters (INFERENCE until a run shows them working):
- Edge records exist only with EDGE_MATCH / EDGE_AUDIT / EDGE_COBUILD on (sets 11+). With them off the TURN METER shows frames only.
- FullCoverAt counts stored tiles as NOT covered and needs the neighbour's edge record within 4 rows of a side
  (conservative: under-counts cover). NearBareTile (any bare tile in the 3 x 3 round) = not covered.
- Groove upload ms uses GetTickCount (1 ms steps): one build reads 0 or 1, the 10 s sum is right on average.
- The B meter calls FullCoverAt per revealed vert - TEST_KIT cost; do not compare its ms rows to builds without it.

## 2. Before numbers (from the repo, copied)
- Crater go-live frame on the sheet ruts, DS (stage 2 report end): set 12 69-91 ms; set 14 ~35-50; set 15 ~12-25
  (swap / voxel / sheet 1/6/9, 1/5/18, 1/7/5; off ruts 0/2/0). Set 15 split: colliders 5/4/4 ms in the go-live frame,
  meshes later 4/2/1 one per frame (12/6/2 ms total, worst 4). Ahead dig DS 12-21 ms, close 5-18 ms.
- Client rut cost without the sheet (results/2026-10-19-mesh-vs-voxel-ruts-ds.md): rebuild 1.33-2.01 ms/frame
  (rb mesh 0.85-1.33, rb collider 0.46-0.67), ahead 0.56-0.78; 86-205 patch meshes / 10 s; 4-7 ms per build.
- Sheet local cost (results/2026-10-20-mud-lab-stage1-ds.md): 2.3-5.3 ms MUD script/frame at presets 1-3; upload
  0.9-2.9, band 0.4-1.8 ms; ~600-950 tile builds / 10 s, 19-40 % would skip.

## 3. Existing log, read 2026-10-21: DS client PeerPlugin1/logs/logs_2026-10-21_00-08-31 (sheet local preset 1, set 15)
Fix sets in the lab line: 00:09:32-00:10:32 and 00:11:02-00:11:12 = set 15 (`vox_mesh_later` listed); 00:10:42 and
00:10:52 = set 14 (no `vox_mesh_later`). No turn / straight split exists in this log - that is what the TURN METER adds.

### 3.1 Edges, per 10 s (`MUD sheet builds:`; edge audit = 10 s snapshot, corners left out)
| time | drape cut | copied | moved+co-built | faded | raised | ok | step>3cm | step max cm | open | open max cm | no mesh |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 00:09:32 | 153 | 1645 | 312 | 863 | 1986 | 1986 | 0 | 0 | 0 | 0 | 0 |
| 00:09:42 | 60 | 524 | 83 | 205 | 2216 | 2216 | 0 | 0 | 0 | 0 | 0 |
| 00:09:52 | 24 | 378 | 30 | 136 | 1384 | 1384 | 0 | 0 | 0 | 0 | 0 |
| 00:10:02 | 71 | 618 | 99 | 291 | 1780 | 1780 | 0 | 0 | 0 | 0 | 0 |
| 00:10:12 | 129 | 2393 | 379 | 1188 | 2924 | 2918 | 0 | 0 | 6 | 15 | 6 |
| 00:10:22 | 197 | 2195 | 329 | 1188 | 3196 | 3162 | 0 | 0 | 34 | 22 | 34 |
| 00:10:32 | 74 | 1422 | 142 | 660 | 3064 | 3032 | 0 | 0 | 32 | 24 | 32 |
| 00:10:42 | 0 | 0 | 0 | 0 | 3064 | 3032 | 0 | 0 | 32 | 24 | 32 |
| 00:10:52 | 2 | 366 | 63 | 399 | 2818 | 2771 | 47 | 19 | 0 | 0 | 0 |
| 00:11:02 | 2 | 233 | 38 | 81 | 1602 | 1525 | 30 | 17 | 47 | 18 | 47 |
| 00:11:12 | 0 | 220 | 51 | 225 | 1532 | 1477 | 47 | 19 | 8 | 18 | 8 |
Checksum (ok + step + open = raised): 00:10:12 2918+0+6 = 2924; 00:10:52 2771+47+0 = 2818; 00:11:02 1525+30+47 = 1602;
00:11:12 1477+47+8 = 1532 - all match.
Read: drape cut at cap is high while driving (24-197 per 10 s; A-1 candidate is live). Steps > 3 cm (max 17-19 cm)
show only from 00:10:52 on; open borders are all 'no mesh' (none untraced / unborn / band 0).

### 3.2 Sheet cost and craters, per 10 s (`MUD lab 10s:`)
| time | fps avg | MUD ms | upload ms | band ms | craters | recv->golive avg/max | golive frame max |
|---|---|---|---|---|---|---|---|
| 00:09:32 | 77.1 | 4.28 | 1.59 | 0.79 | - | - | - |
| 00:09:42 | 87.4 | 1.38 | 0.26 | 0.12 | 1 | 89/89 | 35 |
| 00:09:52 | 85.4 | 1.00 | 0.06 | 0.00 | 3 | 149/158 | 24 |
| 00:10:02 | 85.7 | 1.47 | 0.35 | 0.19 | 1 | 54/54 | 8 |
| 00:10:12 | 73.5 | 4.35 | 1.48 | 0.80 | 1 | 145/145 | 47 |
| 00:10:22 | 78.0 | 3.61 | 1.32 | 0.57 | 3 | 104/146 | 31 |
| 00:10:32 | 83.9 | 2.31 | 0.78 | 0.32 | 1 | 83/83 | 42 |
| 00:10:42 | 90.2 | 0.52 | 0.00 | 0.00 | - | - | - |
| 00:10:52 | 87.5 | 1.08 | 0.01 | 0.04 | 2 | 128/132 | 52 |
| 00:11:02 | 87.9 | 0.98 | 0.00 | 0.00 | 3 | 157/160 | 40 |
| 00:11:12 | 91.7 | 0.89 | 0.00 | 0.03 | 2 | 131/154 | 24 |
Crater count checksum: 1+3+1+1+3+1+2+3+2 = 17; `MUD sheet prebuild:` lines in the log = 18 (one go-live closed in
the 00:11:12 - 00:11:15 tail after the last 10 s line).

### 3.3 Ahead dig per crater (`MUD sheet prebuild:`, all 18)
dig ahead worst frame ms: 10, 20, 15, 13, 0, 17, 18, 1, 0, 14, 15, 14, 18, 18, 18, 21, 12, 0 (max 21; 3 craters
had nothing held: off the sheet). close ms: 23, 9, 3, 2, 0, 23, 20, 1, 0, 23, 15, 8, 15, 8, 9, 18, 5, 0.

### 3.4 Look ruts on this client
`MUD look ruts: 10 s` lines show cuts (e.g. 96, 13) but `look meshes 0` in this log - no rut patch mesh rebuilt by
look cuts here (no driving over old ruts in this run). B rows below need a run that drives on ruts.

## 4. Tables the next launch fills (NEXT)
- **A** per bucket per 10 s: frames, checks, raised, step > 3 cm (max), open, corner step, drape cut (+ quads left),
  co-copy over 3 cm, born waits - sets 15 / 16 / 17, straight 20 s / tight circle 20 s / stop 12 s.
- **B** builds metered, none revealed / all covered / some uncovered, groove upload ms, B10 would hold of queued,
  B8 segments - driving on old ruts, sheet local, set 15.
- **C** `MUD prof parts` for driving on ruts, crater on ruts, crater off ruts, tunnel dig, trench (DS, >= 6 runs per
  row + an off row), top 5 costs.
- **D** join with 30+ craters: PART D notes (joining, most in one frame, cap drops), prebuild worst frame.

## 5. Combined test session (sets 16-21 built, compiled clean WB 18:30, commit 05de5f9)

Switches, all default off; numpad * steps one set, the on-screen FIX SET line names them:
16 = 15 + drape_fit (A10) | 17 = 16 + co_side + corner (A11, A12) | 18 = 17 + pre_live_only (D1) |
19 = 18 + look_under_skip (B8) | 20 = 19 + groove_under_off (B9) | 21 = 20 + mesh_under_hold (B10).
Log lines to read after: `MUD sheet builds:` (edges + TURN METER), `MUD lab 10s:` (PART D), `MUD sheet prebuild:`,
`MUD world B under sheet 10 s:` (B9 groove off / watch list / rebuilt from it, B10 held), LookRuts 10 s (B8 count).

WB - look / function only (no ms):
1. Play, spawn a fresh truck. Numpad 3 to sheet local. Numpad * to set 15, then step 16 ... 21: each name on screen.
2. Sets 15, 16, 17 each: drive straight 20 s, tight circle 20 s, stop 12 s. Striped wedges / tile edges on the turn?
3. Set 19: drive over fresh grass and old ruts in the band - any rut missing under the sheet or popping late at its edge?
4. Set 20, then 21: drive over old ruts. Look at the band edge, a crater on the ruts, a tunnel mouth, drive 80 m away
   and back. Any grass-coloured rut, rut poking through the mud, or hole in the ground?
5. Set 21: numpad 3 off sheet local while standing on ruts - ruts must come back with dirt inside within ~1 s.
6. Set 18: craters on the ruts - hole there at once, as with set 17?

DS - ms only (>= 6 runs per row, robot: driver seat, stopped, hold Space 3 s):
7. Rows off / 15 / 17 / 19 / 20 / 21: robot drive on ruts (6 runs each).
8. Same rows: crater on the ruts, crater off the ruts, tunnel dig, trench (one of each per run).
9. Part D join (a): 30+ craters on the sheet ruts, second client joins, THEN sheet local + set 15. Holes all there?
10. Part D join (b): rejoin, turn sheet local + set 15 as fast as possible during the join, then set 18. Holes all there?
11. Two clients side by side, both sheet local + set 21: same crater - same hole, same rut look, no truck floating or
    sinking on either.

## 6. DS session 2026-10-21 19:52-20:02 (client PeerPlugin1/logs/logs_2026-10-21_19-52-30, sheet local preset 1)

One client only: no join test (items 9-10) and no two-client test (11) in this session. No sheet-off row.
User skipped item 4 'set 20 on old ruts' by eye; mortar craters done on every set.
CAVEAT: MUD_StutterRobot flips VIEW_TRIM / LOOK_PACE / LAYOUT_SPLIT per run (odd runs 1, even runs 0), so runs 3 and 5
differ from 2 / 4 / 6 in that too. Runs 3-6 were stopped early (83 / 54 / 43 / 33 s); cost climbs as a run lays more
ruts, so 'first 6' = the first six 5 s windows of every run (same time since start).

### 6.1 Robot reverse circle per set (client, 5 s 'MUD prof' windows, ms per frame)

| run | set | VT/LP | windows | MUD all | MUD first 6 | sheet first 6 | rebuild first 6 | rb mesh first 6 | rebuild all | look meshes / 10 s (ms) |
|---|---|---|---|---|---|---|---|---|---|---|
| 2 | 15 | 0 | 23 | 6.22 | 6.05 | 4.25 | 0.82 | 0.56 | 1.07 | 122 (514 ms) |
| 3 | 17 | 1 | 16 | 6.99 | 6.64 | 4.70 | 0.93 | 0.61 | 1.20 | 130 (533 ms) |
| 4 | 19 | 0 | 11 | 5.67 | 5.47 | 3.98 | 0.50 | 0.27 | 0.59 | 18 (70 ms) |
| 5 | 20 | 1 | 8 | 6.47 | 6.51 | 5.01 | 0.55 | 0.28 | 0.58 | 21 (79 ms) |
| 6 | 21 | 0 | 6 | 5.99 | 5.99 | 4.36 | 0.66 | 0.38 | 0.66 | 33 (129 ms) |

(look meshes column = mean of the 10 s look-rut lines after each run's first one.)

### 6.2 What each switch did
- B8 (set 19): look-rut cuts 168-175 -> 9-18 per 10 s, look meshes ~125 (~520 ms) -> ~20 (~75 ms) per 10 s,
  rebuild part ~1.1 -> ~0.6 ms/frame. The one clear saving (~0.45 ms/frame). Look by eye not checked this session.
- B9 (set 20): groove section upload is only 20-38 ms per 10 s at set 15 (~0.03 ms/frame) = the most B9 can save.
  It fired at the end of run 5 only (14 builds without groove, 1 patch listed, 6 rebuilt from the list). Negligible.
- B10 (set 21): 'would hold' 0-3 of 57-155 queued per 10 s at set 15, held 1 in the whole session. Nearly never
  passes; no saving. Not worth keeping as built.
- A10-A12 (sets 16-17, in every later set): WORSE edges. 10 s edge audit at set 15: step > 3 cm 0, open 0 in all 11
  samples. From set 17 on: step > 3 cm up to 193 (max 32 cm), open up to 152. TURN METER turning step > 3 cm: set 15
  0-5 per 10 s, sets 17-21 36-304. 'co-build sides sent later' 85-354 per 10 s. Set 16 (A10 alone) was on for 0.5 s
  only, so A10 vs A11 / A12 is not split by this log (inference: the 'sent later' sides = A11 is the main suspect).
- D1 (set 18): no join this session - untested. Craters live on set 18+ took the pre-build as before (lines below).
- The sheet is the big cost: 'sheet' 3.4-5.7 ms of MUD 5-7 ms per frame while driving (sh upload ~2 ms). Ruts
  (rebuild + look ruts + vox) are ~1-1.6 ms/frame.

### 6.3 Mortar craters per set (client; every crater 1 op, pre-build on)

| set | craters | go-live after (ms) | builds held | close (ms) | dig ahead worst frame (ms) | worst frame ms (MUD ms) in the crater 5 s windows |
|---|---|---|---|---|---|---|
| 21 (before reset) | 2 | 157, 124 | 25, 24 | 16, 18 | 15, 16 | 94.8 (28), 66.3 (42) |
| 15 | 2 | 135, 159 | 34, 32 | 29, 15 | 13, 16 | 79.7 (8), 67.6 (25) |
| 17 | 3 | 158, 120, 113 | 37, 32, 27 | 24, 11, 21 | 16, 14, 14 | 76.7 (23), 72.7 (26), 76.8 (22) |
| 19 | 2 | 131, 87 | 28, 23 | 12, 27 | 15, 11 | 82.6 (23), 65.9 (8) |
| 20 | 3 | 106, 119, 115 | 24, 19, 27 | 19, 0, 22 | 11, 8, 18 | 75.1 (21) |
| 21 | 8 | 105, 124, 97, 118, 78, 103, 88, 107 | 25, 34, 19, 29, 23, 24, 22, 25 | 16, 28, 9, 22, 30, 10, 10, 13 | 15, 20, 11, 17, 11, 15, 10, 9 | 54.3 (7), 85.4 (24), 69.1 (24), 67.3 (45), 86.1 (22), 63.8 (23), 56.3 (28) |

Checksum: 2 + 2 + 3 + 2 + 3 + 8 = 20 = the 20 'MUD sheet prebuild' lines in the log. Sets 16-21 change nothing on
craters (none of them targets the crater path except D1, which only acts while joining / in bursts). Crater worst
frames are 55-95 ms with MUD 7-45 ms of it: the rest is the engine (explosion), same on every set.

## 7. v010 (pre-voxel, 2026-10-12 22:58) - code read 2026-10-21, NOT measured

Backup: Versions backup/v010_2026-10-12_2258_pre-voxel-test. Same project GUID 6A8F47B45144E6D6 as today.
- Ruts: wheel Press / Cut straight into the fine patch heights (m_aH); server sends changed rects every
  m_fColliderInterval (SendChanges / OnLive / ApplyRect). No voxel probe, no J2 ops, no look ruts, no band mesher,
  no sheet. WorldGround 6344 lines (today 10085).
- Rebuild: at most ONE mesh and ONE collider per frame (Rebuild picks the oldest due patch of each, MayRun budget,
  COST_MESH 1 / COST_COLLIDER 4), each patch at most every m_fMeshInterval / m_fColliderInterval. Mesh = fixed
  33 x 33 grid (1089 verts, 6144 indices, cached index list), groove section always on.
- No 'MUD prof' meter (MUD_Prof does not exist there) and the robot runs 20 s (RUN_SEC 20): a v010 DS run today would
  not give numbers comparable to section 6 without a small meter port.
- No DS log of the v010 days survives (oldest DS / client logs: 2026-10-20). The 10-19 'mesh vs voxel' test
  (results/2026-10-19-mesh-vs-voxel-ruts-ds.md) was the heightfield path INSIDE today's code (RUTS false, v228), not
  v010: client 3.35 vs voxel 2.53, server 5.61 vs 3.27 ms/frame, 6 vs 0 corrections.
- 'No performance issues in WB' is a WB impression; testing rules: WB is never a perf measure. Today's numbers say the
  difference is mostly the sheet (3.4-5.7 ms/frame) - v010 has no sheet at all.

## 8. v010 test copy + 3-row DS session (built 2026-10-21, NOT compiled yet)

Copy: addons/Mud v010 test (outside this repo, no git). The v010 backup as it was, plus this project's Assets and
worlds/MudArland (missing from the backup; 0 refs left unresolved after the copy). addon.gproj: ID MudV010Test, GUID
9022FE7FD05E72D3, title "Mud v010 test", same dependency. Same resource GUIDs as this project: open ONE of the two at a time.
Changes in the copy, nothing else:
- MUD_Prof.c copied from this build (SwitchTag -> " v010"). Same 5 s / parts / spikes / worst lines + per-frame CSV.
- WorldGround EOnFrame: WHEELS (Wheels), AHEAD (AheadView + Ahead), REBUILD (Rebuild + nav; RB_NAV inside), FREE_NET
  (FreeIdle + net), EndFrame. Rebuild: RB_INDEX (IdxSet), RB_MESH (BuildMesh), RB_SMOOTH (BeforeCollider + SmoothPatch),
  RB_COLLIDER (BuildCollider). VOX / SHEET / LOOK / HOUSE / KEEPSCAN stay 0 (v010 has none of them).
- Robot RUN_SEC 20 -> 120. MUD_AllowanceTest.ON and MUD_PredictionTest.OFF are both false (stock netcode).
- Inference: worlds/MudArland is today's copy, not the 10-12 one (that one was never backed up).

Session (DS, one client, robot 2 runs per row = 4 min; the current robot flips VIEW_TRIM / LOOK_PACE / LAYOUT_SPLIT
between odd and even runs, so one of each):
- Row A: current build, sheet OFF (no numpad 3 = parked default), fresh truck on grass, 2 robot runs.
- Row B: current build, sheet local ON (numpad 3) + numpad * to set 19, same spot, 2 robot runs.
- Row C: Mud v010 test project, fresh truck, same spot, 2 robot runs.
Compare per row: 'MUD prof 5 s' MUD share + frame avg, 'MUD prof parts' (wheels / ahead / rebuild / rb mesh / rb collider),
spikes, and the robot STOP corrections.

## 9. DS session 2026-10-21 20:25-20:34: rows A / B / C, ONE robot run each (user: enough, 2 min each)

Logs: current build client PeerPlugin1/logs/logs_2026-10-21_20-25-16, server server/logs/logs_2026-10-21_20-25-14
(-addons Testingplanvechmsh); v010 client + server logs_2026-10-21_20-30-51 (-addons MudV010Test, CLI line checked).
Windows = robot START -> STOP (client clock): A 20:25:58.9-20:27:58.9 (run 1, odd: VIEW_TRIM / LOOK_PACE / LAYOUT_SPLIT 1),
B 20:28:12.8-20:30:12.8 (run 2, even: those 0; sheet local preset 1 20:28:05, 'set 19' line 20:28:09), C 20:31:45.3-20:33:45.3.
Only 'MUD prof 5 s' blocks fully inside a window count, frame-weighted (parser scratchpad ds4.py).

Client (the driving player):

| Row | blocks | frame avg ms (fps) | frame max | engine render / GPU / sim ms | MUD avg ms | MUD worst ms | frames > 33 ms (MUD half+) | robot corrections |
|---|---|---|---|---|---|---|---|---|
| A current, sheet off | 24 | 11.17 (90) | 44 | 4.01 / 2.91 / 3.47 | 1.49 | 34 | 5 (0) | 0 |
| B current, sheet local set 19 | 23 | 12.82 (78) | 61 | 3.65 / 1.01 / 7.42 | 5.36 | 53 | 258 (8) | 10 ('mine 1' lines in the whole 20:25 log: 12; v010 log: 0) |
| C v010 | 23 | 10.40 (96) | 36 | 3.75 / 2.26 / 3.65 | 1.69 | 6 | 1 (0) | 0 |

Client parts (avg ms per frame / worst ms in one frame):
- A: rebuild 0.72/10 (rb mesh 0.50/7, rb collider 0.21/7), ahead 0.24/7, vox 0.10/3, op all 0.08/3, look ruts 0.08/1.
- B: sheet 3.87/52 (sh upload 1.77/21, sh wheels 0.60/5, t quads 0.48/17, t engine 0.45/13, t setup 0.39/13, sh follow
  0.17/9), rebuild 0.60/9, ahead 0.25/8, vox 0.19/6, op all 0.17/5, look ruts 0.06/1.
- C: ahead 0.81/5, rebuild 0.60/5 (rb mesh 0.29/2, rb collider 0.21/2, rb smooth 0.07/2, rb index 0.03/3).

Server (DS runs at a 30 fps cap: frame 33.33 ms in every row, so its 'over 33 ms' count means nothing):

| Row | MUD avg ms | MUD worst ms | top parts (avg / worst) |
|---|---|---|---|
| A | 1.98 | 12 | wheels 0.81/7, rebuild 0.74/10 (rb collider 0.73/10), op all 0.30/6, ahead 0.14/7 |
| B | 2.01 | 12 | wheels 0.85/8, rebuild 0.76/9 (rb collider 0.75/9), op all 0.32/7, ahead 0.10/8 |
| C | 2.30 | 26 | rebuild 1.11/25 (rb collider 0.80/25, rb smooth 0.21/4, rb index 0.10/5), wheels 0.86/9, ahead 0.10/5 |

Read:
- The current ground with the sheet off costs about what v010 costs. Client MUD script 1.49 vs 1.69 ms, server 1.98 vs 2.30;
  v010 has the worse single frames on the server (rebuild 25 ms worst). Client whole frame 11.17 vs 10.40 ms: the 0.8 ms
  gap is not MUD script (MUD is 0.2 ms LOWER in A); it sits in render / GPU (4.01 / 2.91 vs 3.75 / 2.26) - inference: the
  voxel brick / look-rut meshes drawn, unmeasured.
- The sheet is the cost: B adds +3.9 ms MUD on the client (sheet 3.87), engine sim 3.47 -> 7.42, 258 frames over 33 ms, and
  is the only row with robot corrections (10). The server does not run the sheet (B = A there).
- Limits: one run per row. A and B differ in the robot's odd / even flip (VIEW_TRIM / LOOK_PACE / LAYOUT_SPLIT) and B started
  ~24 m from A (<1038.7, 2565.2> vs <1045.5, 2589.3>; C <1035.8, 2605.4>). The world in C is today's copy.

## 10. v010 + today's sheet (row D) - built 2026-10-21, NOT compiled yet

Into addons/Mud v010 test (outside this repo): MUD_MudSheet.c, MUD_MudLab.c, MUD_CraterTrace.c, MUD_MudZone.c,
MUD_TestKit.c copied unchanged from this build (TEST_KIT true: numpad 3 / numpad * work as here). New MUD_V010Shim.c
stands in for what v010 lacks, answering as this build does where none of it exists: MUD_VoxelProbe (no column, no rut
field: TopAt / SurfaceAt / ColumnAt / DrawsAt false, FieldFloorAt FLOOR_NONE), MUD_LookRuts (SheetFloorAt FLOOR_NONE),
MUD_MudSync (empty layer, as sheet local with SYNC false), MUD_VoxelOp / MUD_VoxelSubEntity (types only).
v010 MUD_WorldGround: the fix-set statics; DugUndug = v010 Heights (the sheet's ground on hole cells); SheetLiftDirty /
VoxCellsChanged / PressLog empty; EOnFrame runs MUD_MudSheet.Tick + MUD_MudLab.Tick before Wheels (as here);
MarkDirty grows one box a frame and the next frame calls MUD_MudSheet.FloorChanged on it (as the rut op re-ground here).
MUD_CoarseSkin: empty SetVoxCells (ONE_GRID only). Static name check (scratchpad v010check.py, control run on this build):
no missing class or member left.
Row D = v010 project, numpad 3 (sheet local), numpad * to set 19, fresh truck, one robot run; compare with row B.
Compiled clean in WB 2026-10-21 20:55:57 (no script error; the DS / client logs below hold 1 SCRIPT (E) each: coarse
baked heights '$Testingplanvechmsh:Data/Mud/mud_heights.c' did not open - tracing, the same line as row C, so C and D
both trace the coarse skin).

## 11. DS session 2026-10-21 20:57-21:01: row D (v010 + today's sheet local, set 19), ONE robot run

Logs: client PeerPlugin1/logs/logs_2026-10-21_20-57-18, server server/logs/logs_2026-10-21_20-57-16 (-addons MudV010Test,
CLI line checked). Sheet local preset 1 20:58:40, 'set 19' line 20:58:54; robot run 1 (odd: VIEW_TRIM / LOOK_PACE /
LAYOUT_SPLIT 1) 20:58:57.7-21:00:57.7 at <1033.3, 2595.0>, 'driving on ours 119.2 s, road 0 s'. Same parsers as sec 9
(ds4.py; engine split eng.py); control: re-running both on row B's log gives sec 9's B row exactly (5.36 / 12.82 /
3.65 / 1.01 / 7.42). The set 19 line here lacks 'pre_live_only look_under_skip' (B has them): v010 has no look ruts, so
B8 has nothing to act on there.

Client (the driving player), B and C copied from sec 9:

| Row | blocks | frame avg ms (fps) | frame max | engine render / GPU / sim ms | MUD avg ms | MUD worst ms | frames > 33 ms (MUD half+) | robot corrections |
|---|---|---|---|---|---|---|---|---|
| B current, sheet local set 19 | 23 | 12.82 (78) | 61 | 3.65 / 1.01 / 7.42 | 5.36 | 53 | 258 (8) | 10 |
| C v010 | 23 | 10.40 (96) | 36 | 3.75 / 2.26 / 3.65 | 1.69 | 6 | 1 (0) | 0 |
| D v010 + sheet local set 19 | 23 | 12.88 (78) | 63 | 3.53 / 0.33 / 8.22 | 6.14 | 56 | 291 (7) | 7 ('mine 1' lines in the whole log: 7) |

Client parts D (avg / worst): sheet 4.23/56 (sh upload 1.84/22, t quads 0.56/22, sh wheels 0.55/5, t engine 0.52/16,
t setup 0.44/12, sh stored 0.20/7, t skirt 0.19/7, sh follow 0.16/10), ahead 0.80/4, rebuild 0.61/4 (rb mesh 0.30/2,
rb collider 0.23/2, rb smooth 0.07/2).

Server: D MUD 2.11 ms avg, worst 10 (rebuild 0.94/6, rb collider 0.73/5, wheels 0.85/10, ahead 0.12/5); C 2.30 / 26.
Sheet local does not run on the server, so D = C there within one run's noise.

Read:
- The sheet costs the same on the old ground: C -> D adds +4.45 ms client MUD (sheet 4.23), B's sheet was 3.87. The ground
  under it (voxel or v010 heightfield) is not what makes the sheet slow.
- Engine 'simulation' rises with it (C 3.65 -> D 8.22, +4.57 vs MUD +4.45). INFERENCE: the script frame is counted inside
  'simulation', so this is the same cost, not a second one.
- Robot corrections came back with the sheet on v010 too (C 0 -> D 7; B 10). The sheet has no collider (COLLIDE false), so
  INFERENCE: the corrections follow the long frames (291 over 33 ms), not the sheet's shape. Unproven: one run per row.
- Limits: one run per row; D is an odd robot run, B an even one; D started ~10 m from C.
- Next: the synced sheet DS test (main project, MUD_MudSheet.ENABLED true since v271), not run yet.

## 12. Synced sheet on the hosted server, standalone game 2026-10-21 21:07-21:10 (fresh-agent review)

Log: client only, ArmaReforger/logs/logs_2026-10-21_21-07-21 (server "Terrain deformation test", mod published as
"Terrain deformation" at 21:05 = main project with ENABLED true, SYNC true, SPARSE false; log lines 'sheet sync',
'all offroad 1' confirm). The folder addons/Terrain deformation was overwritten at 21:10 with 10-12 scripts (no sheet)
and published again 21:11 - not what was played. Human driver, 5-19 m/s, ~750 m of loops, crossed own tracks.
User: decent fps, a lot of far-away track pop-in.

| 5 s window end | fps | frame ms | MUD ms | sheet | sheet sync | speed m/s | note |
|---|---|---|---|---|---|---|---|
| 21:08:12 | 138.3 | 7.23 | 0.77 | 0.69 | - | - | loading |
| 21:08:17 | 154.1 | 6.49 | 0.09 | 0.03 | - | - | |
| 21:08:22 | 110.7 | 9.03 | 1.33 | 1.16 | 0.00 | - | spawned |
| 21:08:27 | 99.3 | 10.07 | 1.38 | 1.12 | 0.00 | 0.0 | first ring, 471 to build |
| 21:08:32 | 81.9 | 12.20 | 1.79 | 1.53 | - | 0.0 | |
| 21:08:37 | 89.7 | 11.15 | 1.54 | 0.76 | 0.15 | 5.2 | offroad driving from here |
| 21:08:42 | 90.5 | 11.05 | 2.91 | 1.85 | 0.37 | 10.0 | |
| 21:08:47 | 91.6 | 10.92 | 3.36 | 2.18 | 0.43 | 10.0 | |
| 21:08:52 | 103.2 | 9.69 | 3.03 | 1.82 | 0.56 | 11.0 | one 52 ms frame (sync worst 39) |
| 21:08:57 | 100.8 | 9.93 | 3.43 | 1.98 | 0.57 | 13.8 | |
| 21:09:02 | 90.7 | 11.03 | 2.87 | 1.51 | 0.48 | 11.7 | |
| 21:09:07 | 91.1 | 10.97 | 3.29 | 1.95 | 0.61 | 13.1 | |
| 21:09:12 | 86.1 | 11.61 | 4.35 | 2.86 | 0.62 | 14.7 | |
| 21:09:17 | 85.0 | 11.77 | 4.34 | 2.66 | 0.78 | 14.9 | |
| 21:09:22 | 94.2 | 10.61 | 2.99 | 1.82 | 0.48 | 10.6 | |
| 21:09:27 | 89.7 | 11.14 | 4.04 | 2.83 | 0.51 | 12.1 | |
| 21:09:32 | 76.6 | 13.06 | 4.63 | 3.23 | 0.55 | 10.6 | |
| 21:09:37 | 94.3 | 10.61 | 3.60 | 2.29 | 0.49 | 9.0 | |
| 21:09:42 | 91.3 | 10.95 | 3.66 | 2.30 | 0.53 | 10.8 | |
| 21:09:47 | 96.0 | 10.42 | 1.64 | 0.75 | 0.21 | 6.6 | onto road |
| 21:09:52 | 103.8 | 9.63 | 0.57 | 0.13 | 0.02 | 4.8 | road |
| 21:09:57 | 104.7 | 9.56 | 0.95 | 0.50 | 0.01 | 3.9 | road |
| 21:10:02 | 103.1 | 9.70 | 0.88 | 0.27 | 0.02 | 3.2 | road |

Checksum: 23 windows = 11347 frames = 115.2 s. Offroad driving 21:08:37-21:09:42 (6382 frames, frame-weighted, also
matched by profile/MUD_prof_46781.csv): 91.0 fps, 10.98 ms frame, MUD 3.41 ms (31 %), sheet 2.12 + sync 0.51. 3 frames
over 33 ms, 112 over 20 ms. 4 'mine 1' corrections (21:08:50 0.52 m at the 52 ms frame; 21:09:30; 2x 21:09:45).
Not an A/B with DS rows B / D (different setup: game alone, human driver, synced not local).

Pop-in (checked against the log 2026-10-21):
- PROVEN mechanism, INFERENCE that it is what the user saw: tracks live almost only in the sheet. 'MUD rut wear 10 s'
  sums worn-only 16138 vs cut 1715 samples (WEAR_PASSES 1), so ~90 % of a drive-through cuts nothing into the real
  ground. Sheet tiles exist only inside the build ring; 'build radius' lines read 40 m x6, 39.0, 29.7, 27.2, 25.3, 20
  (SPEED_RADIUS shrinks it when fast); meshes drop past 46 m. 'to build 0' while driving = no backlog: tracks pop AT the ring.
- PROVEN runs, smaller pop: LOD_M 20 m tiles built at 0.3 m quads, 'queued back to full' 251-305 per 10 s inside 17 m.
- PROVEN runs, likely minor: LAZY_RUT_MESH holds patch meshes under the sheet ('held patch-frames' 2084-7850 / 10 s).
- Ruled out by the log: look ruts' own meshes ('look meshes 0'), build backlog, far-patch hold, patch sleep.
Other problems: MUD_MudSync m_mTiles is only ever Set (lines 867, 1639, 2045), never removed - 'tiles touched' 702 at
the end (~7.9 MB at 11.2 KB a tile, INFERENCE from the code comment). Every client runs every rut op (no distance
check, INFERENCE: cost grows with trucks map-wide). Published addon ID 'Terraindeformation' but MUD_CoarseSkin.c:92 and
MUD_RoadLookProbe.c:17 hard-code '$Testingplanvechmsh:' - log: 'baked heights ... did not open - tracing', then
'MUD coarse: work 5114 ms ... 1 frames' = a 5 s freeze at load.
