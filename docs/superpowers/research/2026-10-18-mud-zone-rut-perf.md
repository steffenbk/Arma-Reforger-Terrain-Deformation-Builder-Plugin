# Mud zone rut cost: where the time goes and what to cut (2026-10-18)

## 0. Goal and what we are doing (user 2026-10-18)

GOAL: the synced mud sheet on over ALL offroad terrain of Arland, not only inside MUD_MudZoneEntity boxes. The zone
at x 997 z 2730 is a test area. So every fix below is judged at whole-map scale, and written as "under the sheet"
(one place that says where the sheet is: `MUD_MudZoneEntity.SheetCovers` today, an offroad rule later), never as a
separate zone check. Switching zones -> offroad then changes that one place.

What changes at whole-map scale:
- The sheet's own cost (upload, sync layer per op) runs wherever a vehicle drives offroad -> S1 becomes key.
- The physical ruts sit under the sheet almost everywhere -> R3 / R7 (cheaper or shallower felt ruts) are the big
  savings, but they change the feel: user decides how deep the felt rut under the mud should be.
- Unbounded growth: sync tiles, the J2 snapshot and DS memory grow with every metre driven -> needs a limit (mud heals
  back over time, or old tracks drop) before the switch.
- "Offroad" needs a rule (ground material soil / grass, not roads; the keep mask already knows roads).

ORDER (user OK 2026-10-18):
| # | Step | Status |
|---|---|---|
| 1 | Stage 5 late joiners: compile + test (mud-sheet README) | built, not compiled |
| 2 | R1 + R2 + R4 / R5 below, written as "under the sheet" (one test session together with step 1) | WB 10:24 run PASSED function (self-test 0 wrong, rut op ~every 0.78 s, 0 dropped, look ruts skipped, cover yes ~88 %, rest = zone edge) - R5 needs no fix; user: fps still drops in the zone |
| 3 | S1 sheet upload rewrite | compiled, WB 10:36 caps live builds, fps still bad -> MUD_Prof run 10:48 -> 3b |
| 3b | Four sheet fixes (below, v188 = before): hidden parts measured, stored rebuild cap, radius cap, sync stamp speed-up | compiled; WB 11:00 user: a little better, circling still ~25 fps |
| 3c | Mesh keep, snapshot self-test 120 s, stamp split timers (below, v189 = before) | built, not compiled |
| 4 | User: felt rut depth under the mud -> R3 / R7 | user call |
| 5 | Growth limit plan (sync layer, snapshot, DS memory) | plan |
| 6 | Zones -> all offroad (needs the feature freeze lifted for it) | user go |

Step 2 as built 2026-10-18 (one place for "under the sheet" = MUD_MudZoneEntity.SheetCovers, in MUD_MudZone.c):
- R1 (MUD_VoxelProbe, server): under the sheet a vehicle's felt rut op goes at most every RUT_SHEET_MS 750 (or at once
  when RUT_MAX_SEGS 40 segments are ready). Convert ahead + look ops keep 250 ms. A full interval no longer drops
  segments (they join the RUT_BEHIND wait list). Log: 'rut behind - ..., N sheet waits, N dropped'.
- R2 (MUD_VoxelProbe.Submit): MudSheet.FloorChanged gets the changed floor-sample box (+1 sample), not the op box.
  Second half (double QueueBuild) not needed - QueueBuild already skips duplicates (m_aOneBuild).
- R4 (MUD_LookRuts.SHEET_SKIP): no look ruts under the sheet. Log: 'segments under the sheet (skipped) N'.
- R5 (first half): MudSheet.Covers counts why it says no; the ground's 10 s 'lazy rut mesh' part ends with
  '- cover asked: yes, too big, zone edge, bare near, live slot not ready, stored tile no mesh'. The fix waits for
  those numbers. (Checked in code: rut ops never mark bare tiles - only kinds 0-3 call GroundChanged.)

WB 10:24 run (2026-10-18, function only - not perf): sheet 'ms in 5 s' upload 200-632, wheels 18-91, grounds 12-31;
'MUD sheet:' builds 453-779 per 10 s at 1.46-1.57 ms mean (~67 live slot builds/s of the max 160 = 16 slots x 10 Hz).
Sync layer 118-197 ms per 5 s (max 34-53 ms per op, spread, worst frame 4 ms). Snapshot 159596 bytes at 54 tiles.

S1 as built 2026-10-18 (MUD_MudSheet.UPLOAD_SLOTS 3, 0 = as before): Upload builds at most 3 dirty zone slots - the
longest-waiting one, then the nearest to a vehicle; slots without a mesh and non-zone slots always go. A slot put
off and then leaving the window builds its stored mesh in Freeze. Log: 'upload N (slot builds put off N)' in the 5 s
'ms in 5 s' part. Expect 'MUD sheet:' builds ~30-40/s instead of ~75. Second S1 half (skip slots whose mud moved
< TOUCH after a restore) not done - see the numbers first.

WB 10:36 run with S1 (function only): live slot builds capped ('put off' 234-534 per 5 s) but 'MUD sheet:' builds per
10 s still 343-592 at 2.6 ms mean (was 1.5); sheet wheels 65-182, follow 18-138 ms per 5 s (both ~2x); fps 35-45 while
driving (user: still bad). The ground's 'worst frame split - wheels' (up to 157 ms) is NOT the wheels alone: it covers
VoxelProbe.Tick + MudSheet.Tick + VoxelHouse + Wheels + LookRuts + AheadView (MUD_WorldGround EOnFrame).

Next tools (2026-10-18, not compiled):
- SHEET-ONLY TEST (MUD_VoxelProbe.RUT_SHEET_ONLY_TEST true, TEMP): a rut op with every segment end under the sheet
  cuts no floor on any machine (sheet still pressed). 'MUD rut field' line ends 'sheet-only test 1: N rut ops not
  carved'. Off-sheet / mixed ops carve as before.
- MUD_Prof.c (TEMP): per-part ms of every per-frame MUD system + System.GetProfileData engine frame / render / GPU /
  simulation ms. 5 s lines 'MUD prof 5 s', 'MUD prof parts', 'MUD prof spikes', 'MUD prof worst frame'; per-frame
  CSV $profile:MUD_prof_<tick>.csv. GetTickCount is 1 ms steps - read averages.
- IDA 2026-10-18 (Experimental ArmaReforgerSteamDiag.exe, read from the binary): diag menu 'Statistics' holds
  'Script profiler' (off / frame / average / memory), 'Script prof. external', 'MeshObject stats' (none / count /
  mesh / faces / verts), 'Stats page' 0-19, 'Resource profiling', 'Resource category stats'. Engine start options
  near 'diagMenu': 'profiler', 'debugger', 'scriptExtProfiler'. Script API seen: System.GetProfileData, GetFPS,
  GetFrameTimeS; BaseWorld.GetFrameNumber, GetPhysicsTimeSlice. Unverified in game which screen each shows.

WB 10:48 MUD_Prof run (function only, relative): felt ruts off under the sheet changed nothing. MUD script 6-19 ms
a frame; engine simulation ~ MUD script + 3; render 5-7. Sheet 3-12 + sheet sync 1-3.5 ms a frame; the sheet total was
2-5 ms above its measured parts. Spikes: 'sh stored' up to 32 ms, 'sh follow' up to 22 ms in one frame. Sync per 5 s:
stamps 128-285 ms (~1 ms a stamp), slump 80-197, quant + hash 19-54, restore 38-66. Cost grows over a run.

Step 3b as built 2026-10-18 (v188 = before; all in MUD_MudSheet.c / MUD_MudSync.c):
- Hidden parts: MUD_Prof parts 'sh pre' (StepNow / ApplyPending), 'sh zones' (BuildZones), 'sh jobs' (GroundJobs),
  'sh expire' (ExpirePreds).
- STORED_MS 3: FlushOneBuild stops after 3 ms (at least one tile); the rest build next frame. 0 = all at once. RISK:
  the BUILD ONCE note says a 6 ms budget once looked like 'loading weirdly, not instant' for craters / trenches
  (user 2026-10-15) - check blasts and trenches.
- RAD_BUILD_MS 4 -> 2 (camera radius approach builds).
- MUD_MudSync.Stamp: plain float maths instead of Vector + 2 vector.Dot a cell, and each row walks only the cells the
  stamp strip can reach (+1 cell margin); the per-cell test is unchanged, so the same cells are stamped. Slump not
  touched (a ground-read cache was ruled out: risk to same-result-everywhere).

WB 11:00 circle drive after 3b (function only, relative): MUD script 17-21 ms a frame while circling; sheet 11-12 =
sh zones 3-4.5 + upload 2.6-3 + wheels 1.9 + follow 1.2-1.9 + stored 1.2-1.5; sheet sync 3.2-3.6. 'MUD sheet:' 10 s:
190-302 meshes DROPPED and ~750 builds at ~2.5 ms - circling drops tiles past 46 m and builds the same tiles again
(zone = 462 tiles). Sync stamps still ~0.93 ms each (248 ms / 266 stamps) - perf fix 4 gave little. 'vox' 431-464 ms
once every 30 s = the J2 test-kit SnapSelfTest (SP / WB only; sheet layer part 213-248 ms at 129-139 tiles, 600 KB).
Felt ruts, look ruts, patch rebuilds ~0.

Step 3c as built 2026-10-18 (v189 = before):
- MESH KEEP (MUD_MudSheet.MESH_CAP 600): a built mesh stays until more than 600 stored meshes exist; then the farthest
  past RADIUS + RADIUS_HYST go first (DropFar -> DropMesh). 0 = as before. Nothing drops in the test zone (462 tiles).
  Watch: render ms with all zone meshes shown.
- MUD_VoxelProbe.SNAP_TEST_MS 30000 -> 120000 (test-kit self-test only). The real join snapshot is still built in one
  go in ServerJipAsk (~250 ms at 139 tiles, grows with the layer) - spreading it needs the layer held still while it
  builds (op clears run straight from Submit) -> belongs to the growth limit plan (step 5).
- MUD_MudSync STAMP split in the sheet sync 5 s line: '| STAMP split ms: side reads, cells (walked, stamped), drag limit'.

Research only. No code changed. All ms numbers below are from logs. Workbench (WB) numbers show WHERE time goes,
not how fast the game runs (CLAUDE.md testing rule). The only standalone-game numbers are from a client on 2026-10-17,
outside any mud zone, with the sheet off. No dedicated-server (DS) log exists with the synced sheet on.

## 1. Short answer

- In the newest WB run (logs_2026-10-18_09-09-49, driving in the zone at x 997 z 2730), the physical ruts themselves
  are NOT the biggest cost. Per 5 s of driving: the sheet's own mesh upload takes 358-724 ms, the sync layer 142-282 ms,
  the rut ops (Submit) ~140 ms, patch mesh + collider rebuilds ~80-145 ms, look ruts ~10-60 ms.
- A rut op costs nearly 2x more inside the zone than outside it, in the same session: 10.3 ms vs 5.7 ms per op. The
  rut floor carve is the same in both (2.5 vs 2.4 ms). The extra ~4.6 ms is the sheet hook in Submit
  (`MUD_MudSheet.FloorChanged` -> `ReGround`) - inference, it is the only zone-only work inside the timed part.
- Every rut op also drives sync work (stamps, slump, quant, hash, sheet restore + replay): 13-19 ms per op (WB).
  Fewer, bigger rut ops in zones cut the per-op part of this.
- Two zone costs do nothing useful today: client look ruts still run in the zone (the sheet already shows the rut
  at the tyre), and the LAZY RUT MESH hides only 1-5 patches per 10 s while 13-42 patch meshes are built.

## 2. The per-op cost path

### 2.1 Server: making the op
1. `MUD_WorldGroundEntity` wheel loop (MUD_WorldGround.c ~1240-1368) -> `MUD_VoxelProbe.RutWheel` per wheel: a segment
   every `RUT_STEP` 0.5 m of travel. NOTE: new segments over `RUT_MAX_SEGS` 40 per flush interval are DROPPED here
   (line ~1056), not kept.
2. `RutFlush` at most every `RUT_EVERY_MS` 250 ms per vehicle. `Behind` (RUT_BEHIND) moves segments to a wait list
   and lets out those the rearmost wheel is `RUT_BEHIND_GAP` 1.5 m past (or > 4 s old and 6 m away, or past
   `RUT_PEND_MAX` 200), at most 40 per op.
3. `DoRutOp` packs the segments as cm text, adds the sheet tag (`SheetTag`: vehicle, speed, tyre radius, time) in the
   seed field, then `Issue` (seq, broadcast, op log). One Print per flush on the server ('rut behind' line).

### 2.2 Every machine: `MUD_VoxelProbe.Submit` (MUD_VoxelProbe.c 3607-3872), in seq order
| Step | What it does | Machines |
|---|---|---|
| `MUD_MudSync.Flush()` | finishes the open sync op in ONE frame if it is not done yet | all with the layer (DS too with SERVER_LAYER) |
| `Prepare` / `PrepareRut` | parses the segments; box = segments + 1.5 m (ridge reach) | all |
| `CarveFloor` | cut: every 0.25 m floor sample within RUT_R 0.3 m of a segment; `FloorBase` per sample (column / Kept / Hard / UndugY) | all |
| `RelaxFloor` | reads FloorBase for every written sample + 2 rings (5 x 5), 4 relax passes over written + 1 ring, sorted | all |
| `GrowRidges` | band 0.5-1.0 m beside each segment, FloorBase per band sample, sorted | all |
| `ClearFloor` | only for ops with Puts (not rut ops under RUT_FIELD) | all |
| `MUD_WorldGroundEntity.FloorDirty` | `FloorFinish` (may finish / wake a patch at once, wheel cap 1 a frame) + `MarkFloorDirty` (mesh + collider dirty, FLOOR_SETTLE hold) | all (mesh only where render) |
| `MUD_MudSheet.FloorChanged` -> `ReGround` | re-reads the sheet ground in the op box (+0.3 m): live slots Trace + FillAround per even cell; stored tiles GroundDown per even cell, FillTile, QueueBuild | rendering machines in/near a zone |
| `MUD_MudSync.RutOp` | queues the op for the sync layer | all with the layer |
| `DispOpDone`, 2-3 Prints (OP_LOG) | counters, log | all |

### 2.3 Later frames: patch rebuilds (`MUD_WorldGroundEntity.Rebuild`, MUD_WorldGround.c 4367)
- At most one mesh and one collider per frame. FLOOR_SETTLE holds a floor-only patch until 0.4 s quiet or 1.5 s max.
- LAZY_RUT_MESH: a patch the sheet `Covers` is hidden and its mesh held; its collider still builds.
- A DS builds colliders only (render 0).

### 2.4 Later frames: sync layer work per op (`MUD_MudSync`, split in jobs, `DRAIN_MS` 3 a frame)
JOB_STAMP (one segment) -> JOB_GROUND / CELLS / PASS x3 / WRITE (slump on the op box + 1, GroundY = data ground) ->
JOB_QUANT (box floats to 0.01 mm) -> JOB_HASH (test kit) -> JOB_SHEET (drop own stamps the op covers, restore the box
from the layer, QueueBuild stored tiles) -> JOB_REPLAY (own stamps again, 16 a step). A DS (Stage 5, NOT COMPILED)
runs everything up to the hash, no sheet steps.

### 2.5 Sheet per frame while driving (`MUD_MudSheet.Frame`, rendering machines)
Follow (live window move), Wheels (own prediction stamps at every tyre, every frame), Grounds (traces of new cells),
ExpirePreds, Upload every `UPLOAD_EVERY` 0.1 s: every dirty slot of the 4 x 4 live window is rebuilt (Pack + BuildTile).

## 3. Numbers

### 3.1 Per rut op, WB host (logs_2026-10-18_09-09-49, 126 ops; zone box x 965-1029, z 2698-2762)
Columns: mean / median / max / sum. Checksum: 79 + 47 = 126 ops.

| Per op | In zone (79 ops) | Outside (47 ops) |
|---|---|---|
| segments | 7.1 / 7 / 17 / 558 | 8.6 / 9 / 16 / 402 |
| Submit ms ('rut seq' line) | 10.3 / 10 / 27 / 811 | 5.7 / 4 / 17 / 266 |
| floor ms (cut + relax + ridges) | 2.5 / 3 / 5 / 201 | 2.4 / 2 / 6 / 111 |
| cut samples | 28.1 / 29 / 66 / 2220 | 27.2 / 21 / 90 / 1279 |
| relaxed samples | 0.5 / 0 / 9 / 37 | 0 / 0 / 0 / 0 |
| ridge samples | 22.5 / 21 / 64 / 1778 | 35.6 / 34 / 89 / 1674 |
| field writes ('rut field' line) | 51.1 / 48 / 122 / 4035 | 62.8 / 55 / 153 / 2953 |
| patches marked dirty | 2.1 / 2 / 4 / 169 | 1.4 / 1 / 4 / 66 |

Control: floor ms is the same in and out of the zone, so the +4.6 ms in the zone is not the carve.
It matches the Stage 1 note "ReGround rebuilds 1-3 stored tiles per rut op in 1-6 ms" (mud-sheet README).

### 3.2 Per rut op, other logs (no mud zone, sheet off at that time)
| Log | ops | Submit ms mean / max | floor ms mean | cut / relaxed / ridged per op | patches dirty |
|---|---|---|---|---|---|
| Standalone game client, real server, 23:41-23:46 (ArmaReforger/logs_2026-10-17_23-07-36) | 522 | 3.2 / 13 | 2.2 | 28.0 / 1.8 / 33.3 | 2.1 |
| Local DS (server/logs_2026-10-18_00-16-14, peer tool, same PC) | 33 | 9.0 / 19 | 4.3 | 46.3 / 0 / 48.9 | 2.5 |
| Peer client, same session | 33 | 6.8 / 18 | 3.9 | 46.3 / 0 / 48.9 | 2.5 |

The relax writes almost nothing: 37 of 3499 samples (WB), 924 of 14562 (game, 6 %), 0 of 1527 (DS). Its reads still run
every op (cost not split out in the log - see test plan). Ridges are ~half of all field writes.

### 3.3 Per 5 s of zone driving, WB host (same log; windows with sync ops)
| Window end | rut ops | sync total ms (stamps / slump / hash / restore) | restores, replayed stamps | sheet upload ms (uploads) | sheet wheels ms | follow ms |
|---|---|---|---|---|---|---|
| 09:21:34 | 19 | 231 (69 / 72 / 15 / 75) | 19, 1216 | 717 (48) | 88 | 21 |
| 09:21:39 | 11 | 205 (52 / 59 / 12 / 82) | 13, 1016 | 518 (34) | 58 | 11 |
| 09:23:34 | 12 | 203 (58 / 69 / 15 / 70) | 12, 781 | 613 (40) | 74 | 13 |
| 09:23:39 | 11 | 142 (27 / 38 / 9 / 59) | 11, 874 | 358 (31) | 43 | 7 |
| 09:23:44 | 15 | 282 (77 / 89 / 17 / 112) | 19, 1460 | 724 (45) | 77 | 19 |
| 09:23:49 | 13 | 207 (64 / 66 / 15 / 49) | 13, 554 | 549 (36) | 48 | 9 |

Sync per op: 13-19 ms. Replayed own stamps per op: 43-97. Upload: ~12-16 ms per upload (inference: ~8-10 dirty live
slots at ~1.5 ms per tile build, from the 'MUD sheet:' line mean build 1.46-1.52 ms).

### 3.4 Per 10 s, patch rebuilds and look ruts
| Log, window | meshes (ms) | colliders (ms) | look meshes (ms) | lazy rut mesh: hidden / held patch-frames |
|---|---|---|---|---|
| WB 09:21:34 (zone) | 15 (77) | 14 (86) | 7 (39) | 4 / 413 |
| WB 09:21:44 (zone) | 13 (63) | 18 (109) | 12 (58) | 2 / 2240 |
| WB 09:23:44 (zone) | 26 (118) | 27 (171) | 23 (106) | 5 / 2019 |
| WB 09:23:54 (zone) | 42 (167) | 21 (101) | 13 (66) | 3 / 1896 |
| Game client 23:42:13 (no zone) | 177 (558) | 25 (100) | 159 (502) | - |
| Game client 23:42:33 (no zone) | 226 (811) | 25 (107) | 205 (734) | - |

Game client per build: mesh ~3.1-3.6 ms, collider ~4 ms. Most game meshes were look-rut meshes (that run used per-wheel
look ruts; the code is back to v174 now, still `MUD_LookRuts.ENABLED` true).

### 3.5 Rough split of mud-zone script time while driving (WB, per 5 s, means of the windows above)
| Part | ms per 5 s | Driven by |
|---|---|---|
| Sheet upload | ~580 | own wheel stamps + sync restores marking live slots dirty |
| Sync layer | ~210 | rut ops (per op) |
| Rut op Submit in zone | ~135 (13 ops x 10.3) | rut ops; ~60 of it ReGround (inference) |
| Patch meshes + colliders | ~80-145 (half of the 10 s sums in 3.4) | rut floor (FLOOR_SETTLE merges per crossing) |
| Sheet wheels + follow + grounds | ~90 | per frame |
| Look ruts (cut + meshes) | ~20-60 | per frame, client only |

Checksum: ~1150-1220 ms per 5 s = ~23-24 % of wall time in WB. WB only - proportions, not perf.
Rut-op-driven work (Submit + sync + rebuilds) is roughly 40 %; the upload alone is roughly 50 %.

## 4. Options

Each option: what changes, saving (with evidence or marked guess), risks, and the four constraints:
D = MP determinism (same ops, same seq, same ground reads), J = J2 snapshot rules (m_sRows, ops floor-blind),
F = felt ground (wheels, physics, AI), Z = feature freeze.

### R1. Fewer, bigger rut ops in zones (server flush interval)
- Change: `MUD_VoxelProbe.RutFlush` uses `RUT_EVERY_MS_ZONE` (e.g. 750 ms) when the vehicle is in or near a zone
  (`MUD_MudZoneEntity.GetInWorld` + `Contains`, server side), and also flushes early when 40 segments are ready.
  `RutWheel` must stop dropping segments past 40 per interval (move new segments to the wait list each frame, or a
  separate collect cap). Op cap stays `RUT_MAX_SEGS` 40 (`PrepareRut` refuses more).
- Saving: per-op fixed work drops ~3x in zones. Per-op parts (from 3.3): sheet restore + replay 4-8 ms per op,
  quant + hash ~1 ms per op, Submit fixed part, one net message + op log entry per op, one 'rut behind' Print.
  Stamps and carve scale with segments (no saving). Slump scales with box area (small saving from less box overlap).
  GUESS: 25-40 % of rut-op-driven work in zones, on clients and on the DS (its sync layer too).
- Risks: ruts land up to 0.5 s x speed later (2.5 m at 5 m/s) - in a zone the sheet prediction already shows the rut.
  One op = ~3x carve in one frame (WB ~7.5 ms instead of 2.5; the carve is not split). Busy trucks (6 wheels, 10 m/s =
  ~120 segments/s) fill 40 segments in ~0.33 s, so the early flush keeps ops near today's rate there.
- D: server-only choice of op content; every machine applies the same op at its seq. OK.
- J: rows still recorded per op (m_bRowLog); snapshot tiles hold the result. OK.
- F: same cuts, a bit later behind the truck. Following vehicles meet the fresh rut later. OK.
- Z: perf tuning of a shipped feature. OK.

### R2. Cheaper ReGround (client sheet hook)
- Change: `MUD_VoxelProbe.Submit` passes the real changed floor box (`m_iFi0..m_iFi1`, `m_iFk0..m_iFk1` + 1 sample, to
  world) instead of `op.m_vLo / m_vHi` (segments + 1.5 m margin). In `MUD_MudSheet.ReGround`, a stored tile whose box
  the same op's sync restore will cover sets `m_aG` but skips its own `QueueBuild` (the restore builds it anyway).
- Saving: in-zone Submit is 10.3 vs 5.7 ms out of zone (3.1) - up to ~4.6 ms per op, ~60 ms per 5 s. Box area GUESS
  ~half; double stored-tile builds per op removed (inference from code: ReGround and JOB_SHEET both QueueBuild).
- Risks: a stored tile built once with old ground if the restore is skipped (op far from the zone tiles, box too big).
  Keep the build when no sync job follows.
- D, J, F: look only, rendering machines. OK. Z: OK.

### R3. No ridges and no relax for rut ops in zones (server flag in the op)
- Change: the server adds a zone flag to the kind-4 op (e.g. one more field in the sheet tag, set in `DoRutOp`).
  `CarveFloor` skips `RelaxFloor` and `GrowRidges` when the flag is set. Clients read the flag; they never do their
  own zone lookup (a client whose zones are not loaded yet would differ).
- Saving: ridges are 22.5 of 51.1 field writes per op in the zone (3.1); relax writes 0.5. The FloorChanged box gets
  ~0.7 m narrower each side (ridge reach 1.0 m vs cut + relax 0.8 m). Carve ms GUESS: -40-60 % of the 2.5 ms
  (relax reads 5 x 5 FloorBase per written sample; not split out yet). Fewer patches marked: GUESS -0-1 per op.
- Risks: look and feel change in zones: no 8 cm ridge under the mud; rut walls may be steeper (relax only moved 6 % of
  samples in the game log, so small). The sheet's own mud ridges stay. User must OK this (feel change).
- D: decided by the server, carried in the op, same on every machine. Old ops without the flag behave as today. OK.
- J: fewer tile writes, nothing new to send; ops stay floor-blind (no new floor reads anywhere). OK.
- F: smaller bumps, still the same on all machines. OK.
- Z: tuning, not a new feature, but it changes the look - ask.

### R4. Look ruts off inside mud zones (client)
- Change: `MUD_LookRuts` skips vehicles whose cut point is on a zone tile (`MUD_MudZoneEntity.SheetCovers`).
- Saving: WB zone windows: look meshes 7-23 per 10 s (39-106 ms) + cut 16-25 ms per 10 s; game client (outside zones,
  per-wheel version) up to 205 meshes (734 ms) per 10 s. In a zone the sheet's own wheel stamps already show the rut.
- Risks: none for the felt ground. If the sheet is far (past the 40 m radius) the zone shows no look rut until the real
  one lands - same as before the look layer.
- D, J, F: look only, never in ops / colliders / hashes. OK. Z: OK.

### R5. Make LAZY RUT MESH actually hold zone patches (client)
- Change: count why `MUD_MudSheet.CoversBox` says no (not zone tile within EDGE / near bare tile / live slot not ready /
  stored tile not drawn) in the 10 s line; then fix the main reason. Possible reasons (inference, not checked): live
  slots are not `ready` until every cell is traced (`m_aGs`), the 8 m patch spans 3-4 sheet tiles that must all be
  ready, and patches at the zone edge never qualify.
- Saving: WB zone windows built 13-42 patch meshes per 10 s while 1-5 were hidden. Each mesh ~3-4.5 ms
  (game 3.1-3.6, WB 4-5). Up to ~60-170 ms per 10 s on clients.
- Risks: a hidden stale mesh must show again when the cover goes (SheetShow exists). Mesh only.
- D, J: look only. F: collider unchanged. Z: finishing a shipped Stage 3 item. OK.

### R6. Longer collider settle in zones (all machines, incl. DS)
- Change: in `MarkFloorDirty` / `Rebuild`, a patch inside a zone uses a longer FLOOR_SETTLE (e.g. QUIET 1.0 s,
  MAX_WAIT 4 s).
- Saving: game client settle stats: 'max wait' releases are 30-50 % of all releases (e.g. 23:42:13 quiet 10 / max 8),
  so those patches rebuild again on the same crossing. GUESS -20-30 % colliders in zones (~4 ms each).
  The DS builds colliders only, so this is one of few rut savings on the DS.
- Risks: the felt rut lags up to 4 s behind the drawn sheet rut for other vehicles and soldiers. RutWheel's contact y
  then reads the older collider: rut depth <= 0.35 m < RUT_ON 0.6 m, so the cut still runs.
- D: ops read data (tiles, UndugY, TopAt), never colliders; segment y comes from the server and is in the op. OK.
- J: nothing in snapshots. F: the felt ground is late, not different. Z: OK.

### R7. No physical ruts in zones at all (max saving, feel loss)
- Change: zone flag (as R3) makes `CarveFloor` write nothing; ops still go out (the sync sheet is pressed from them).
- Saving: all carve, FloorDirty, ReGround and rut patch rebuilds in zones (~200-250 ms per 5 s WB: Submit ~135 + rebuilds ~80-145, minus the op fixed part). Sync work stays.
- Risks: no felt rut, no steering pull, no getting stuck in mud zones - where mud feel matters most. Look stays
  consistent: drawn = felt ground + mud depth, the mud is pressed to ~0 under the tyre, so tyres sit on the drawn
  surface. Repeat passes give lower mud ridges (sync `sink` reads ground beside the tyre minus segment y).
- D, J: as R3. F: consistent but flat. Z: changes the feature - user call. Listed for completeness.

### S1. Sheet upload (client, biggest WB item)
- Change: in `MUD_MudSheet.Upload` cap the slots per upload (nearest to a wheel first, the rest next upload); after a
  sync restore + replay, compare the slot data to before and do not dirty a slot whose mud moved < TOUCH.
- Saving: upload 358-724 ms per 5 s (3.3). GUESS: -30-50 %.
- Risks: far slots refresh a little later (look only). D, J, F: look only. Z: mud sheet is the active work item. OK.

### S2. DS sync layer (Stage 5, not compiled)
- Fact: with `SERVER_LAYER` true a DS runs stamps + slump + quant + hash for every rut op near a zone. WB per op:
  stamps ~3.6, slump ~4, quant + hash ~1 ms. Not measured on a DS yet. R1 cuts the per-op part. Keep the
  'MUD mud sheet sync (server)' line in the test.

### Small items
- OP_LOG prints 2-3 lines per op on every machine plus the server's 'rut behind' line: ~4 lines per op x ~4 ops/s per
  truck. Cost not measured (guess: small). Could be TEST_KIT only.
- Lower rut cap in zones (`m_fB`): ruts stop changing sooner on repeat passes (an op that writes nothing skips
  FloorDirty and ReGround). Only helps on tracks driven many times. Minor.

## 5. Recommendation

1. R1 - fewer, bigger rut ops in zones (server flush 750 ms + early flush at 40 ready segments + fix the 40-segment
   drop in RutWheel). Biggest per-op saving across client, host and DS. No determinism risk: the server only
   changes what goes into an op.
2. R4 + R5 together - look ruts off in zones, and fix LAZY RUT MESH cover. Both client-only, look-only, zero MP risk.
   They remove most patch MESH rebuilds in zones (13-42 meshes per 10 s in WB, ~3-4 ms each).
3. R2 - cheaper ReGround (real changed box, no double stored-tile build). Up to ~4.6 ms per in-zone op on every
   rendering machine; look only.

Hold R3 / R7 until the user decides on the look and feel change. S1 (upload) is likely the biggest single client
saving but it is sheet work, not rut work - suggest it as the next sheet item.

## 6. Test session plan (one session, after a build; DS + standalone game client with the mod)

Setup: local DS + game client (or uploaded mod on a real DS). Truck spawned fresh at a FIXED spot inside the zone
(x 997 z 2730). Robot: driver seat, stopped, hold Space 3 s -> 20 s reverse circle. At least 6 runs per setting
(snap and fps rates vary between 3-run blocks). A/B switches in the test kit file (R1 flush ms, R4 look ruts in zones,
R5 on / off) so one build covers every setting.

Logs to add before the session (all behind MUD_TestKit.TEST_KIT):
- 'MUD rut floor' line: split ms into cut / relax / ridges (sum per 10 s, GetTickCount is 1 ms coarse).
- Submit split per 10 s: FloorDirty ms, ReGround ms, ReGround stored tiles queued, live cells re-traced.
- CoversBox fail reasons per 10 s (R5).
- Per 10 s: rut ops per vehicle, mean segments per op, segments dropped in RutWheel (should be 0).

Numbers to read:
| Where | Line | Numbers |
|---|---|---|
| DS console.log | 'MUD voxel probe: rut seq', 'MUD rut floor', 'MUD rut field' | op count per 10 s, Submit ms, floor ms split, patches dirty |
| DS | 'MUD world: last 10 s', 'MUD patches:' | colliders (ms), floor settle releases quiet / max wait, worst frame |
| DS | 'MUD mud sheet sync (server)' (Stage 5) | sync ms per op, flushes |
| DS | server frame time (existing server fps log) | mean + worst frame while 1, then 3 trucks drive in the zone |
| Game client | 'MUD patches:' fps, 'MUD mud sheet PERF' | fps, worst frame |
| Game client | 'MUD world: last 10 s', 'MUD look ruts: 10 s', lazy rut mesh part | meshes / colliders / look meshes (ms), hidden / held |
| Game client | 'MUD mud sheet:' 5 s, 'MUD mud sheet sync:' | upload, wheels, sync COST split, restores, replayed stamps |
| Both | 'MUD sheet hash seq N' + 'MUD disp hash seq N' | Tools/compare_op_hashes.py: client A vs server - must match for R1 / R3 |

Pass marks: same hashes per seq on all machines; 0 dropped segments; client fps and DS worst frame better than the
same-session baseline over >= 6 runs; no new vehicle snaps (client CLIENT CORRECTING count).

## 7. Not verified
- No DS or standalone-game numbers with the synced sheet on. All zone numbers are WB.
- The +4.6 ms per in-zone op is attributed to ReGround by elimination, not by a timer.
- Relax and ridge ms are not split out; their share of the 2.5 ms carve is a guess.
- Why `CoversBox` fails so often is not checked (only that it does: 1-5 hidden vs 13-42 meshes).
- Upload slots per call (~8-10) is inferred from ms per upload / mean tile build ms.
- Stage 5 DS sync layer cost: code not compiled, never measured.
- Savings marked GUESS are estimates from counts, not measurements.
