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
