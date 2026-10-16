# Review fixes - Stage 0 baseline (2026-10-15)

Prompt: "fix the 2026-10-15 code review findings" (F1-F6), Stage 0 = measure, no behaviour change.
Includes patch-memory prompt Stage 0 (docs/superpowers/prompts/2026-10-15-patch-memory.md, in git HEAD).
Checkpoint v107 = before the Stage 0 log edit.

## Code checked against the findings (2026-10-15, before any fix)
- F1: split lines exist - MUD_WorldGround.BandStats ('MUD band: mesh split ms' / 'collider split ms'), every 10 s
  while m_bBandMesher or m_bDebug.
- F5: PatchCounts is called only from Stats, and Stats only runs when m_bDebug (EOnFrame). Comment says "always" - wrong.
  MudWorldGround.et has m_bDebug 1, so the line prints in all test runs.
- F6: MUD_RpcAsk_PlaceTrench (MUD_TrenchPlanner.c:518) uses the client's `instant` flag:
  `else if (instant || MUD_TrenchSiteComponent.DEBUG_INSTANT) site.DebugDigAll();` (line 561). No server check.

## Log change for Stage 0 (log only)
'MUD patches' line now ends with ` | fps N, vehicles moving N of N` (fps = frames / seconds over the 10 s Stats
window; moving = MUD_Wheels vehicle faster than 2 m/s at print time). Debug-only like the rest of Stats.

## Lines to read (10 s each)
- `MUD patches: live, asleep, floor-only, deformed, heights-only, building, total, mem KB | fps, vehicles moving`
- `MUD band: last 10 s - ... band mesh N builds, mean ms, worst | band collider N, mean ms, worst | mesher itself ms`
- `MUD band: mesh split ms - input, mesher, reveal + uv, mesh object, rest (of total)`
- `MUD band: collider split ms - input, mesher, trimeshes (geoms), drop, static, rest (of total)`
- `MUD rut field: N samples, M patches dirty, 0 new columns` (per op)

## Run A - SP 2-3 km rutted drive (one vehicle, control)
Per 10 s window (whole table, not just the summary):

| time | fps | moving | live | asleep | floor-only | deformed | total | mem KB | mesh builds / ms | col builds / ms | mesh split (in/mesher/uv/obj/rest) | col split (in/mesher/geom/drop/static/rest) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|

## Run B - stress, 3-5 vehicles on fresh ground at once, 2-3 min
| time | fps | moving | live | asleep | floor-only | deformed | total | mem KB | mesh builds / ms | col builds / ms | mesh split | col split |
|---|---|---|---|---|---|---|---|---|---|---|---|---|

## Run C - late join (rutted drive + crater over a rut, then join)
| item | server | joiner |
|---|---|---|
| join time | | |
| patches built during join | | |
| 'MUD disp hash' lines match | | |
| columns | | |
| rut ops replayed | | |

## Existing numbers (user skipped the new runs for now, 2026-10-16)
Source: WB log logs_2026-10-15_21-35-43 (one session 21:35-23:24, SP, one truck, RUT_FIELD on from Stage 3).

Split lines, all 15 windows that have them (ms per 10 s):

| window | mesh in | mesher | reveal+uv | mesh obj | rest | mesh total | col in | col mesher | trimeshes (geoms) | drop | static | rest | col total |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 30 | 127 | 20 | 19 | 0 | 196 | 35 | 113 | 90 (138) | 0 | 2 | 2 | 242 |
| 2 | 50 | 165 | 50 | 17 | 1 | 283 | 55 | 161 | 133 (180) | 2 | 1 | 2 | 354 |
| 3 | 85 | 291 | 73 | 44 | 3 | 496 | 88 | 276 | 221 (332) | 1 | 2 | 6 | 594 |
| 4 | 58 | 202 | 52 | 29 | 1 | 342 | 66 | 193 | 158 (216) | 0 | 2 | 6 | 425 |
| 5 | 6 | 25 | 7 | 5 | 0 | 43 | 11 | 30 | 24 (32) | 0 | 0 | 0 | 65 |
| 6 | 68 | 242 | 58 | 36 | 2 | 406 | 75 | 224 | 187 (261) | 0 | 4 | 9 | 499 |
| 7 | 44 | 199 | 39 | 22 | 0 | 304 | 53 | 193 | 131 (186) | 1 | 5 | 0 | 383 |
| 8 | 3 | 41 | 5 | 1 | 0 | 50 | 7 | 40 | 9 (24) | 0 | 0 | 1 | 57 |
| 9 | 62 | 228 | 57 | 22 | 1 | 370 | 67 | 219 | 176 (289) | 2 | 3 | 8 | 475 |
| 10 | 53 | 221 | 47 | 33 | 1 | 355 | 58 | 214 | 158 (248) | 1 | 3 | 3 | 437 |
| 11 | 26 | 127 | 27 | 12 | 0 | 192 | 33 | 121 | 91 (130) | 0 | 1 | 4 | 250 |
| 12 | 13 | 47 | 14 | 5 | 0 | 79 | 13 | 42 | 33 (54) | 1 | 0 | 4 | 93 |
| 13 | 64 | 240 | 60 | 24 | 1 | 389 | 75 | 232 | 188 (301) | 2 | 0 | 10 | 507 |
| 14 | 50 | 182 | 34 | 30 | 0 | 296 | 50 | 148 | 127 (209) | 3 | 1 | 7 | 336 |
| 15 | 77 | 311 | 79 | 42 | 3 | 512 | 99 | 289 | 244 (358) | 0 | 6 | 5 | 643 |
| sum | 689 | 2648 | 622 | 341 | 13 | 4313 | 785 | 2495 | 1970 | 13 | 30 | 67 | 5360 |

Checksum: mesh parts 689 + 2648 + 622 + 341 + 13 = 4313 (= total). Collider parts 785 + 2495 + 1970 + 13 + 30 + 67 = 5360 (= total).
Share: mesh = mesher 61 %, input 16 %, reveal + uv 14 %, mesh object 8 %. Collider = mesher 47 %, trimeshes 37 %
(per-material + slab + plate), input 15 %, drop + static 1 %.

Patches dirtied per rut op (whole session, 2219 ops; 0 = Stage 2 runs before patches drew the floor):
0: 272, 1: 619, 2: 946, 3: 2, 4: 380 -> mean of ops that dirty = (619 + 1892 + 6 + 1520) / 1947 = 2.07.

Build rate while driving (band mesh / collider builds per 10 s, mean ms): 22:53-22:54 windows 47-87 mesh at 7.8-8.9 ms
and 47-86 collider at 9.3-10.4 ms (= ~120-150 ms/s); 23:16-23:24 windows 32-92 at 5.1-6.1 ms / 6.4-7.7 ms
(= ~70-110 ms/s). Mesh and collider build counts track each other within a few per window: every floor dirt
rebuilds both.

## Stage 1 BUILT 2026-10-16 (user OK), not compiled yet - checkpoint v109 = before
MUD_WorldGroundEntity.FLOOR_SETTLE true (FLOOR_QUIET 0.4, FLOOR_MAX_WAIT 1.5). MUD_FinePatch: m_bFloorWait,
m_fFloorFirstAt, m_fFloorDirtAt. MarkFloorDirty holds a patch only when it has no other dirt owed; MarkDirty clears
the hold; Rebuild skips held patches until quiet / max wait, then the normal mesh / collider intervals apply.
Log: 'MUD patches: ...' ends with '| floor settle true: released quiet N, max wait N, held patch-frames N'.
Compare after: 'MUD band: last 10 s' builds per 10 s + mean ms while driving vs the windows above.

### Stage 1 + 2 SP test 2026-10-16 00:18-00:19 (same WB log, compiled 00:17:58, one truck, ~60 s on grass)
Per 10 s window: band builds paired with the 'MUD disp 10 s' rut ops of the same window (only windows with >= 15 ops).
ms/s = mesh builds x mean + collider builds x mean.

| window | rut ops | mesh builds | col builds | ms/s | builds / op | ms / op | settle released quiet / max |
|---|---|---|---|---|---|---|---|
| AFTER 00:18:38 | 39 | 32 | 31 | 37.8 | 1.62 | 9.7 | 12 / 5 |
| AFTER 00:18:48 | 39 | 43 | 42 | 51.8 | 2.18 | 13.3 | 17 / 6 |
| AFTER 00:18:58 | 38 | 27 | 28 | 36.6 | 1.45 | 9.6 | 13 / 6 |
| AFTER 00:19:08 | 39 | 28 | 27 | 35.4 | 1.41 | 9.1 | 12 / 8 |
| AFTER 00:19:18 | 39 | 43 | 47 | 59.1 | 2.31 | 15.2 | 16 / 8 |
| before 22:40:16 | 15 | 36 | 35 | 43.4 | 4.73 | 28.9 | - |
| before 22:44:52 | 38 | 69 | 69 | 84.7 | 3.63 | 22.3 | - |
| before 22:48:12 | 27 | 50 | 50 | 57.7 | 3.70 | 21.4 | - |
| before 22:48:22 | 39 | 104 | 104 | 132.1 | 5.33 | 33.9 | - |
| before 23:16:24 | 31 | 55 | 55 | 63.7 | 3.55 | 20.5 | - |
| before 23:16:34 | 39 | 91 | 88 | 109.0 | 4.59 | 27.9 | - |
| before 23:16:44 | 29 | 63 | 65 | 76.7 | 4.41 | 26.4 | - |
| before 23:17:04 | 37 | 75 | 75 | 90.5 | 4.05 | 24.5 | - |
| before 23:17:14 | 39 | 51 | 51 | 68.7 | 2.62 | 17.6 | - |
| before 23:17:24 | 36 | 6 | 6 | 10.7 | 0.33 | 3.0 | - (odd: ops logged, almost no builds) |
| before 23:21:09 | 27 | 67 | 67 | 84.5 | 4.96 | 31.3 | - |
| before 23:21:19 | 24 | 58 | 57 | 79.2 | 4.79 | 33.0 | - |
| before 23:23:07 | 28 | 70 | 72 | 89.6 | 5.07 | 32.0 | - |
| before 23:24:00 | 20 | 53 | 50 | 63.2 | 5.15 | 31.6 | - |
| before 23:24:10 | 39 | 92 | 95 | 115.5 | 4.79 | 29.6 | - |

Result: ms per rut op 9.1-15.2 after vs 17.6-33.9 before (the 23:17:24 window left out) - about a third. At full op
rate (39 ops / 10 s) 35-59 ms/s after vs 69-132 ms/s before. Mean build cost unchanged (mesh 5.3-6.1 ms, collider
6.7-7.2 ms): the saving is all build count. Caveat: different drives, 5 windows after, SP only; look not judged by the user yet.
Settle released 17-24 patches per 10 s, 26-37 % of them by the 1.5 s max wait.

Stage 2 in the same run: holes 0 in 1577 plain checks (844 + 576 + 157). Seam checks 0 and 0 carved cells - no crater
was crossed, so the seam path is still untested.

Seen, not caused by settle (open, for Stages 3-4): 'building' patches climbed 22 -> 54 -> 92 -> 95 -> 104 -> 108 while
live went 1 -> 61; total 23 -> 175 patches, mem 2913457 -> 2978818 KB (+65 MB in 50 s, ~430 KB per new patch).

## Stage 1 proposal (F1) - as approved
What dominates: the band mesher core (~half of both), then collider trimeshes. Both scale with the NUMBER of builds,
and the number of builds is driven by the op stream: ~4 rut ops/s x ~2 patches each, and each dirt rebuilds mesh
(0.1 s interval) AND collider (0.2 s) - so a patch a truck is crossing rebuilds both on nearly every op.

Proposal: settle floor-only dirt. MarkFloorDirty sets a new p.m_fFloorDirtAt (+ m_fFloorFirstAt on the first unbuilt
one) instead of building at once; Rebuild skips a patch whose ONLY dirt is floor dirt until it has had no new floor dirt
for FLOOR_QUIET (0.4 s) or FLOOR_MAX_WAIT (1.5 s) has passed since its first unbuilt floor dirt. Wheel presses /
craters / MarkDirty keep today's timing (a patch with other dirt builds as now and takes the floor with it).
Behind a const MUD_WorldGroundEntity.FLOOR_SETTLE (false = today).
- Why safe: RUT_BEHIND already lays ruts behind the wheels, so the laying truck never drives on the newest floor; the
  collider for a following vehicle lags by <= 0.4-1.5 s. Server and clients use the same rule.
- Expected (INFERENCE): about one build per patch crossing instead of one per op -> builds/s roughly 1/2 to 1/3.
  Mesh and collider then usually fall in the same quiet window, which would also make the reverted shared band
  build (gotcha 2026-10-15, 6 % hits) worth a second look - not in this stage.
- Not proposed: skipping slab / plate (slab must follow the floor for bullets; plate geoms are destroyed with the
  body, reuse is an engine question), client collider only near vehicles (the laying truck is always near).
- Check after: same drive, 'MUD band' builds per 10 s and ms/s vs the table above; look: ruts appear <= ~0.5 s later.

## Stage 2 (F3) - built 2026-10-16, not compiled yet (checkpoint v108 = before)
- MUD_VoxelProbe.DispHoles: carved points as before (bricks only). RUT_FIELD + DISPLACE_RUTS adds:
  plain points (not carved, skin hole cell, patch collider live + not dirty - MUD_WorldGroundEntity.PatchColliderLive)
  expected = DugUndug dug; seam points 0.05 m either side of every carved / not carved cell line inside the 9 x 9 grid,
  each side its own expected top. Plain + seam traces accept bricks OR MUD_FinePatchEntity (DispOursAll).
  New helpers DispExpect / DispTraceCheck. HOLE lines say ' (plain)' / ' (seam)'.
- 'MUD disp N s:' summary ends with ' (plain H in C, seam H in C)'.
- DispFloorList: no voxel column -> depth under UndugY (grey / yellow / red), white only if neither reads.
- Known blind spot: a patch with a collider rebuild owed is skipped (its collider lags on purpose) - with the Stage 1
  settle idea that would skip more points; count stays visible in the plain checks number.

## 'building' 22 -> 108 (log 21-35-43) - explained 2026-10-16
- PatchCounts counted lazy patches (made ahead, index lists only, skin carries them, IsLazyReady) as 'building'.
  They are freed after m_fFreeAfter 60 s; the run was 50 s, so none had gone yet. Now its own 'lazy' count.
- Found on the way: a patch made over an OLD rut stopped lazy (m_bFloor was only set by FloorDirty / a mesh build),
  so the rut did not draw until something stamped it. Fixed in Stage 4 (StartPatch -> RutFieldOn).

## Stage 3 (patch memory Stage 1) - built 2026-10-16, not compiled yet (checkpoint v110 = before)
- MUD_FinePatch: 9 press / ahead / scrape arrays moved into MUD_PatchPress (m_Press, made on the first write,
  accessors return the old defaults while null). SetShow skips a 0 into a missing holder. ~39 KB per patch saved
  where nothing pressed / hid / scraped.

## Stage 4 (patch memory Stage 2) - built 2026-10-16, not compiled yet (checkpoint v111 = before)
- MUD_WorldGroundEntity.FLOOR_FREE true (false = v111): FreeIdle frees floor-only patches (FreeDue): no vehicle /
  player near for m_fFreeAfter AND out of FieldDue range that long. FreePatch now uncovers an asleep patch and
  counts m_iAsleep down.
- FieldDue (once a second, in Ahead after FarDue): rut patch keys (m_sFieldPatches, filled by FloorDirty) within the
  camera (drawn only, m_fClientBuildRadius 300, 0 = any), a player (m_fPlayerRadius 12) or a wheeled vehicle at any
  speed (m_fAheadRadius 16). Existing ones there are kept (m_fFieldSeenAt); each frame the nearest missing / building
  one joins Ahead's candidates (camera ones ranked + m_fAheadRadius, as FarDue).
- FloorDirty: FloorFinish = EnsurePatch for EXISTING patches only (finish / wake, wheel cap) + remember the keys;
  no new patches (JIP replay storm gone).
- Server: the prompt expected no change. One added on purpose: wheeled vehicles at ANY speed count in FieldDue -
  Ahead makes patches only for vehicles above m_fAheadSpeed (1 m/s), so a slow / parked (AI) vehicle on a freed rut
  would sit on the skin, no rut collider.
- Log: 'MUD patches: ...' line ends ' | floor free true: rut patch keys N, in range N, made near N, floor-only freed N'.

## Stages 3-4 run - 2026-10-16 (log logs_2026-10-16_23-19-11, SP, 23:29-23:32, user: "worked fine")
- Compiles clean (after the FieldDue stale-index fix). 20 'MUD patches' lines.
- Floor free works: 'floor-only freed 32' at 23:32:03, then 'in range 30, made near 3' (23:32:13) and 'made near 29'
  (23:32:23) - freed rut patches came back on return.
- Patch total: 22 -> 382 (23:31:23) -> 309 at the end; asleep up to 127. Mem 1666803 -> 1841015 KB (+170 MB in 3.5
  min) - still rising at the end, run too short to say it levels off.
- Seam test NOT covered: no 'MUD disp N s:' summary line in the log, 0 seam HOLE lines.
- UNRESOLVED: 20 'HOLE (plain) ... hit none' lines 23:32:46-23:32:52, all at one spot, a cross around
  1039.5 2533.5 (x 1038..1040 at z 2533.5, z 2532..2534 at x 1039.5), expected 38.61-38.67. 'hit none' = the trace
  hit neither bricks nor a patch. Came ~25 s after the freed patches were remade near; cause not checked.

## Stage 5 (F4) - option (a) BUILT 2026-10-16, not compiled (checkpoint v113 = before)
- m_aLock (MUD_FinePatch.c:27) = StartPatch: EdgeLock / EdgeLockCached (skin hole cells = deterministic), then
  BuildingLock (QueryEntitiesByAABB, Building + MUD_CutBoxEntity boxes + m_fBuildingMargin -> 0). The building part
  depends on what that machine has loaded / destroyed at the moment the patch is made.
- Rut floor uses it in 5 places: patch mesh + collider (MUD_WorldGround.c ~6044, 6168, 6342, 6496) and Heights via
  LockAt (~4390). LockAt falls back to EdgeLock (no buildings) when the patch does not exist, so Heights/DugUndug
  already differ by "patch made or not" - and since Stage 4 frees patches, on ONE machine over time too.
- Bricks (MUD_VoxelProbe) apply no lock at all.
- Proposal (a): rut floor uses the edge-only lock everywhere (new m_aEdgeLock in the patch, copied before
  BuildingLock, ~4 KB per patch; LockAt reads it; bricks' field floor applies the same EdgeLock). Craters / trenches
  (m_aH) keep the building lock - not in scope. Cost: a rut can now dip inside the building margin next to a wall.
- Built: MUD_FinePatch.m_aEdgeLock (copied from m_aLock in StartPatch just before BuildingLock); the 4 rut floor
  reads in mesh / collider + LockAt use it; new static MUD_WorldGroundEntity.FieldEdgeLocked(gx, gz, f) (LockAt +
  BaseAt) applied to every floor sample in MUD_VoxelProbe.Displace (m_aDispF).
- Check: ruts beside a building now cut inside the building margin (expected); rut crossing patch -> brick next to a
  road (kept terrain) shows no step; hole check still 0. Note: the edge lock is a snapshot of skin hole cells at patch
  start, as m_aLock was.

## Stage 5 run - 2026-10-16 (same log logs_2026-10-16_23-19-11, scripts recompiled 23:40:35, run 23:42-23:44, user: done)
- Compiles clean, no MUD script errors. 'MUD world': craters 0, digs 0 the whole run - so no brick (Displace) was
  rutted in the log; the brick side of F4 is not exercised by this run. Building / road look: user's eyes only.
- Patches total 22 -> 81 -> 59, floor-only 20.
- Hole check: 5 more 'HOLE (plain) ... hit none' at 23:43:28, a row 1051..1053 x at z 2536 (expected 38.24-38.28).
  Same kind as the 20 before Stage 5 (1039.5 2533.5) -> not caused by F4; still unresolved.

## Stage 6 (F5 + F6) - built 2026-10-16, not compiled (checkpoint v114 = before)
- F5: PatchCounts comment already says 'while m_bDebug' (no change). New MUD_VoxelProbe.OP_LOG (true) gates the op
  line ('MUD voxel probe: ...'), 'MUD rut field: N samples ...' and 'MUD rut floor: ...'. The one-time 'floor grid on
  the patch vertices' line stays. Overflow log kept as is.
- F6: new MUD_TrenchSiteComponent.ALLOW_INSTANT_DIG (true). MUD_RpcAsk_PlaceTrench: instant from a client with it
  false (and DEBUG_INSTANT false) -> site placed, no DebugDigAll, WARNING 'instant dig refused'. Ctrl+F unchanged now.
- Not flipped: m_bDebug, OP_LOG, ALLOW_INSTANT_DIG -> master list Lane 2 'flip before release'.

## Summary
(fill after the runs: ms/s of patch rebuilds per vehicle, which split part dominates, memory per km)
