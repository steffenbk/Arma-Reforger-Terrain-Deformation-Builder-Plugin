# Mud ground performance assessment (2026-10-18)

Read-only review of where CPU time goes per frame and per rut op, on the server (DS) and on a rendering client.
No code or prefab was changed. Companion doc: [2026-10-18-perf-research-ida-web.md](2026-10-18-perf-research-ida-web.md)
(engine side: IsRendered gate on look rebuilds, one-pass collider tri sort).

Evidence rules used here:
- Performance numbers come only from DS / game-client logs, quoted with file + timestamp.
- Workbench (WB) logs are used for COUNTS only (builds, ops per 10 s), never for ms (project TESTING RULES).
- Everything not measured is labelled **inference**.
- Older logs quoted in earlier docs (DS 11:29, 11:37, 13:14, 13:38, 14:11) are rotated off disk; they are cited as
  "quoted in <doc>".

## 1. Summary (plain language)

1. The biggest server cost is rebuilding patch colliders: about 8 ms each on the DS, and with FLOOR_SETTLE off a
   driving vehicle now causes about 3 colliders per rut op (4 ops/s), so roughly 100 ms of every second (inference).
2. Each rut op itself costs 6-14 ms on the DS; half of that is the relax step, and with the mud sheet parked every op
   is a "full" op again (no crude path).
3. Debug Prints are expensive on the DS (proven about 2 ms per Print line), and three per-op Prints are still on.
4. On clients most of the mesh work is look ruts plus a second rebuild when the real server rut lands on top.
5. Several things grow forever (op list, rut tiles, rut patch set) and one cap (one collider per frame) saturates at
   about 2-3 driving vehicles on a 30 fps DS (inference).

## 2. Ranked optimisations

Gain = expected saving on the stated machine. Risk = chance of breaking feel, determinism (J2 / op hashes) or MP.

| # | Change | Gain | Risk | Effort | Files / functions | Evidence |
|---|---|---|---|---|---|---|
| 1 | Stop per-op Prints on the DS: (a) the TEST_KIT `MUD op time` Print for every kind-4 op, (b) the unconditional `MUD voxel probe: live ...` Print in GoLive (one per batch = one per rut op), (c) prefab `m_bDebug 1` -> 0 (`MUD rutmat` lines), (d) JoltProbe `MUD corr visual only` lines | about 2 ms per Print line on the DS; (a)+(b) = about 4 ms per rut op = about 16 ms/s per driving vehicle | Low (logs only; keep the lines behind a flag for test runs) | Small | [MUD_VoxelProbe.c](../../../Scripts/Game/Mud/MUD_VoxelProbe.c) Submit (`if (all >= OP_SLOW_MS \|\| (op.m_iKind == 4 && MUD_TestKit.TEST_KIT))`, line 3812), GoLive (Print line 6200); [MudWorldGround.et](../../../Prefabs/Mud/MudWorldGround.et) `m_bDebug 1`; [MUD_WorldGround.c](../../../Scripts/Game/Mud/MUD_WorldGround.c) RutMudFits / rutmat Prints (6712, 6731); [MUD_JoltProbe.c](../../../Scripts/Game/Mud/MUD_JoltProbe.c) line 242 | Measured: the 'MUD rut floor' Print = 2.19 ms per op on DS vs 0.06 ms on client (h1 run 5, DS 14:11, quoted in [h1-snap-probe](../results/2026-10-18-h1-snap-probe.md)). Per-op saving for the other Prints = inference (same Print path). DS 16:16 log has 111 'live', 111 'MUD op time', 199 'MUD rutmat', 148 'corr visual only' lines |
| 2 | Fewer collider rebuilds while driving: rebuild the collider only for patches with a wheel on them each op, defer the rest (a short settle only for wheel-free patches), or coalesce one collider per patch per N ops | colliders/op 3.09 -> about 1.4-2.2 (the settle-on counts) = about 35-55 ms/s per vehicle on the DS (inference) | Medium (feel: FLOOR_SETTLE was turned off because the rear wheel floated; the hold must never cover a wheel patch) | Medium | MUD_WorldGround.c MarkFloorDirty (4288), FloorFinish (4101), Rebuild (4501), BuildColliderBand (7171) | Counts (WB 20:10 log): settle off 3.09 colliders/op, settle on 2.24, RUT_BEHIND true 1.43 (table 3.2). Cost 8.0 ms per collider (DS 16:16, table 3.1). Projection = inference |
| 3 | Cheaper RelaxFloor: cache FloorBase per (fi, fk) inside one op where no column sits there (result does not depend on nearY), reuse maps/arrays between ops instead of `new` per op, early-out when pass 1 moves nothing | relax was 6.05 of 14.43 ms per full server op; maybe half of that (inference) = about 3 ms per op | Medium (must stay bit-identical: check with Tools/compare_op_hashes.py and the J2 self-test) | Medium | MUD_VoxelProbe.c RelaxFloor (4822), CarveFloor (4623), FloorBase | Measured DS 13:38 run 3 (quoted in h1-snap-probe): full op 14.43 ms = carve 12.41 (relax 6.05, ridge 1.83, write 2.19). Saving = inference |
| 4 | Client: skip the second mesh rebuild when the server rut lands where the look rut already drew the same or deeper floor; IsRendered / distance gate on look rebuilds; replace per-vertex map lookups in BuildMeshBand with a per-patch row cache | look meshes are about 80 % of client meshes; 155-246 'replaced' per 10 s; per-vertex 3 map lookups x 1089 verts | Low-Medium (look must still match the felt ground within the REPLACE window) | Medium | MUD_WorldGround.c BuildMeshBand (6987), MarkLookDirty (4244); [MUD_LookRuts.c](../../../Scripts/Game/Mud/MUD_LookRuts.c) MeshFloor | Measured DS client 11:29 (quoted in [sheet-alternatives-and-perf](2026-10-18-sheet-alternatives-and-perf.md) sec 9-11): look meshes about 25/s at about 5.4 ms; look_ruts 0 cut MUD script 3.11 -> 1.26 ms. Game client 23:41 ([mud-zone-rut-perf](2026-10-18-mud-zone-rut-perf.md)): look meshes 159 (502 ms) and 205 (734 ms) per 10 s. Counts WB 22:04: table 3.3 |
| 5 | Release build: TEST_KIT false, MUD_Prof ON/CSV false, MUD_KeepScan archived | removes per-frame JoltProbe (every vehicle, 25 patch lookups per contact wheel), the 49-column CSV row + GetProfileData every frame, the 5 s file reopen, and the per-op Prints of #1 | Low (test tools only) | Small | [MUD_TestKit.c](../../../Scripts/Game/Mud/MUD_TestKit.c) `TEST_KIT = true` (11); [MUD_Prof.c](../../../Scripts/Game/Mud/MUD_Prof.c) `ON`, `CSV` (13-14); MUD_JoltProbe.Tick | Measured DS 13:14 run 1 (quoted in h1-snap-probe): MUD parts summed 1.66 of 2.74 ms/frame; the 1.08 ms outside the parts includes JoltProbe and MUD_Prof itself (inference which share) |
| 6 | Share the band mesher core between mesh and collider when both are dirty in one frame (one m_Band.Build, two outputs) | about half of the mesher time on frames that build both (inference) | Medium (mesh and collider read the floor at different times today) | Medium | BuildMeshBand (6987), BuildColliderBand (7171) | Inference (code read: both call m_Band.Build on the same patch). Engine share of mesh ms = 6-11 % (IDA doc), so the script mesher core is most of it |
| 7 | Smaller ops and less parse work: send segments relative to the op origin (short ints), drop the double encode/decode/Prepare in Issue | network about 300-400 B -> about half per op (inference); one less parse per op on the server | Low-Medium (op text format is the J2 / snapshot format) | Medium | MUD_VoxelProbe.c Issue (3350), DoRutOp plan string, MUD_WorldGround.c VoxelBroadcast (7903) / RpcDo_VoxelOp (7917) | Counts: 87812 B / 253 rut ops = 347 B per op (WB logs_2026-10-17_14-56-38 15:03:04 self-test line); table 3.4 |
| 8 | Scaling guards: FieldDue scan only near patches (spatial bucket) instead of all m_sFieldPatches x all sources; cap / compact m_aOps (keep a snapshot + tail); free rut tiles far from everyone; count dropped segments (RUT_MAX_SEGS) in the 10 s line | prevents slow growth over a long match; no gain in a short test (inference) | Medium (JIP replay uses m_aOps) | Medium-Large | MUD_WorldGround.c FieldDue (4131), FloorFinish (4101); MUD_VoxelProbe.c m_aOps (569), RutFlush (`t.m_iDropped++`, 1124) | Inference from code; counts in table 3.5 |

## 3. Measurements

### 3.1 DS server, one vehicle driving (FLOOR_SETTLE true, mud sheet on, crude ops at 750 ms)

Source: `My Games/server/logs/logs_2026-10-18_16-16-18/console.log`. The DS runs at 30 fps. This was BEFORE
FLOOR_SETTLE false / RUT_BEHIND false / sheet parked, so it is the cheap end of today's cost.

'MUD world: last 10 s' collider lines:

| 10 s window | colliders | collider ms | ms each |
|---|---|---|---|
| 1 | 17 | 140 | 8.2 |
| 2 | 52 | 383 | 7.4 |
| 3 | 33 | 288 | 8.7 |
| 4 | 30 | 247 | 8.2 |
| 5 | 33 | 261 | 7.9 |
| 6 | 34 | 258 | 7.6 |
| 7 | 41 | 342 | 8.3 |
| sum | 240 | 1919 | 8.0 |

Checksum: 17+52+33+30+33+34+41 = 240; 140+383+288+247+261+258+342 = 1919; 1919 / 240 = 8.0 ms.

'MUD prof' per-frame parts at the peak (16:17:24): MUD script 6.73 ms = ahead 1.78, rebuild 0.90 (rb collider 0.90),
wheels 0.71 (w flush 0.49), op all 0.37 (the rest is outside the named parts). At 16:18:14: 5.06 ms.
Range over the drive: 2.2-6.7 ms per frame.

'MUD op time' lines (one per rut op, crude ops): 111 ops, mean 5.88 ms, median 6, max 10, sum 653 ms
(653 / 111 = 5.88). Carve mean 4.59 ms. Patches dirtied per op: mean 2.83, median 2, max 6, sum 314 (314 / 111 = 2.83).
Floor box per op: mean 414 samples, max 1152.

'MUD patches' memory: 1349874 KB at 16:16:43 (22 patches) -> 1388521 KB at 16:17:44 (119 patches) = +38647 KB for
+97 patches = about 398 KB per patch (inference that the patches are what grew). Rut patch keys 6 -> 82 in about 1.5 min;
'floor-only freed' 0 until 16:18 (then 6); 'MUD world ... freed 23' at 16:18:34.

Print line counts in this log (each one costs about 2 ms on a DS, item 1): 'MUD corr visual only' 148,
'MUD op time' 111, 'rut seq' 111, 'rut behind' 111, 'MUD voxel probe: live' 111, 'MUD rutmat' 199.

### 3.2 Collider rebuilds per op (WB counts only, one driving vehicle)

Source: `ArmaReforgerWorkbench/logs/logs_2026-10-18_20-10-50/console.log`, 'MUD world: last 10 s' lines. Counts only.

| build (compile) | settings | window | ops | colliders | meshes |
|---|---|---|---|---|---|
| 21:36 | RUT_BEHIND true, settle on | 21:36:41 | 38 | 52 | 84 |
| 21:36 | | 21:36:51 | 39 | 69 | 111 |
| 21:36 | | 21:37:01 | 39 | 45 | 81 |
| 21:42 | step 3, RUT_BEHIND false, settle on | 21:43:13 | 35 | 95 | 161 |
| 21:42 | | 21:43:23 | 39 | 116 | 275 |
| 21:42 | | 21:43:33 | 39 | 56 | 137 |
| 21:42 | | 21:43:43 | 39 | 90 | 196 |
| 21:47 | settle off | 21:48:05 | 38 | 107 | 209 |
| 21:47 | | 21:48:15 | 35 | 106 | 218 |
| 21:47 | | 21:48:31 (15 s) | 31 | 98 | 154 |
| 21:47 | | 21:48:41 | 37 | 136 | 234 |
| 21:53 | settle off | 21:53:59 | 39 | 114 | 230 |
| 21:53 | | 21:54:09 | 39 | 122 | 236 |
| 21:53 | | 21:54:19 | 39 | 104 | 150 |
| 21:53 | | 21:54:29 | 36 | 118 | 179 |
| 21:59 | settle off | 21:59:40 | 39 | 117 | 233 |
| 21:59 | | 21:59:50 | 38 | 125 | 272 |
| 22:02 | settle off (latest) | 22:04:40 | 38 | 111 | 258 |
| 22:02 | | 22:04:50 | 39 | 124 | 304 |
| 22:02 | | 22:05:00 | 31 | 98 | 224 |

Ratios (checksums):
- settle off: colliders 447 + 458 + 242 + 333 = 1480, ops 141 + 153 + 77 + 108 = 479 -> **3.09 colliders per op**.
- settle on, RUT_BEHIND false (21:43:23-21:43:43): 116 + 56 + 90 = 262 / 39 + 39 + 39 = 117 -> **2.24**.
  (21:43:13 left out: first window after spawn.)
- RUT_BEHIND true, settle on: 52 + 69 + 45 = 166 / 38 + 39 + 39 = 116 -> **1.43**.

What FLOOR_SETTLE false changed: about +38 % colliders per op versus settle on (3.09 vs 2.24), with the same op rate
(about 38 per 10 s = 4 per s, matching RUT_EVERY_MS 250). With 8.0 ms per DS collider (3.1) this projects to about
12 colliders/s x 8 ms = about 99 ms/s per driving vehicle on the DS (inference: DS cost per collider taken from the
settle-on 16:16 run).

### 3.3 Client look meshes (WB counts only, 22:04 run)

Same log, 'MUD look ruts: 10 s' lines:

| window | look meshes | all meshes | look share | 'replaced by a shallower server rut' |
|---|---|---|---|---|
| 22:04:40 | 208 | 258 | 81 % | 246 |
| 22:04:50 | 236 | 304 | 78 % | 235 |
| 22:05:00 | 180 | 224 | 80 % | 155 |
| sum | 624 | 786 | 79 % | 636 |

Checksum: 208+236+180 = 624; 258+304+224 = 786; 624 / 786 = 0.79. Delay wheel -> drawn avg 179-256 ms. Patch-frames
waiting: 'lost' 450-1276, 'building' 1063-3208 per 10 s.

Duplicate work on a client per rut op (code read + DS 13:38 numbers): the client applies the full server op (carve
8.65 ms incl. relax 5.09 on the client, DS 13:38 run 3 quoted in h1-snap-probe) AND has already cut its own look rut AND
rebuilds the mesh again when the server floor lands ('replaced' rows above). Client hitches: 'vox' frames >= 10 ms,
143 frames, next-frame dt median 35.7 ms; 9 of 10 snaps 0.03-0.06 s after a rut op (DS 13:14 run 1, quoted in
h1-snap-probe).

### 3.4 Op size and network volume

'MUD snapshot self-test' lines (WB counts only, bytes are not timing):

| log | time | rut ops | op log bytes | bytes per op | tile bytes |
|---|---|---|---|---|---|
| logs_2026-10-17_14-56-38 | 15:03:04 | 253 | 87812 | 347 | 10473 |
| logs_2026-10-17_14-56-38 | 15:24:53 | 204 | 83198 | 408 | 36836 |
| logs_2026-10-17_22-45-55 | 00:21:11 | 224 | 61802 | 276 | 32526 |

Op text: `seq|kind|x|y|z|a|b|piece|plan|seed`, each segment as cm ints `x,y,z,x,y,z;` (DoRutOp). Segments per op
grow with speed (4 wheels, RUT_STEP 0.5 m, 250 ms): about 2 x speed in m/s (inference). Ops go out by a reliable
broadcast RPC (VoxelBroadcast -> RpcDo_VoxelOp) to every client. At 4 ops/s and 276-408 B per op: about 1.1-1.6 KB/s
per driving vehicle to every client, before RPC overhead (inference). Over RUT_MAX_SEGS 40 segments the rest is dropped
(`t.m_iDropped++`), and with RUT_BEHIND false that count is no longer printed anywhere.

Issue (3350) encodes and decodes the op and runs Prepare twice; Submit's own Prepare parses a third time (code read;
cost not measured).

### 3.5 Per-frame work that runs even when nothing changes

| item | runs on | what it does every frame | switch | cost evidence |
|---|---|---|---|---|
| MUD_Prof | all, incl. DS | builds a 49-column CSV row, WriteLine, System.GetProfileData; closes/reopens file every 5 s; its own time is outside the 'MUD script' ms | MUD_Prof.ON / CSV (true) | inference (not self-measured) |
| MUD_JoltProbe.Tick | all | every registered vehicle (parked too): FindComponent + GetDirectWorldTransform; per contact wheel ProbeGround = 25 patch lookups; 4 map Sets; 'corr visual only' Prints; also StutterRobot.Tick (FindComponent) and FrameLog.Tick | TEST_KIT (true) | DS 13:14: 1.08 of 2.74 ms/frame outside named parts (quoted in h1-snap-probe); share = inference |
| Wheels | server + owner | every registered vehicle, FindComponent each; a stopped vehicle still calls RutFlush | always | DS 16:16 peak: wheels 0.71 ms (w flush 0.49) |
| FieldDue | all | once a second: all m_sFieldPatches x (camera + players + vehicles) PatchDist; the set only grows | always | inference: O(rut patches x sources) in one frame |
| Rebuild | all | one mesh + one collider per frame max; string.Format into m_sFrameRebuild on every build | always | DS 16:16: rebuild 0.90 ms at peak |
| MUD_LookRuts.Tick | render only | per wheel lead cut (EVERY 0.05, RANGE 150); calls the parked DecalProbe / GrassCutProbe / TrackStamps (early return) | ENABLED true | DS client 11:37 look_ruts 0: 3.11 -> 1.26 ms (quoted in sheet-alternatives-and-perf) |
| MUD_KeepScan | server / SP | TEMP scan; idle (state 2) once done | TEST_KIT | cheap after the scan (code read) |
| MUD_MudSheet.Tick | all | returns at once | ENABLED false | none |
| MUD_VoxelHouse.Tick | SP only | - | - | none in MP |

### 3.6 Memory and allocation churn (code read; growth from the DS 16:16 log)

- About 398 KB per fine patch (3.1). Patches free after m_fFreeAfter 60 s away; DS 16:16 freed 23 at 16:18:34.
- Every mesh rebuild calls MeshObject.Create (new Resource); every collider rebuild makes new geoms / mats / layers
  arrays and CreateTriMesh per slot (BuildMeshBand 6987, BuildColliderBand 7171).
- Every rut op: CarveFloor makes new maps (best, atY, bestSeg); RelaxFloor makes a map + 7 arrays.
- m_aOps keeps every op with its text and parsed segment arrays on every machine for the match (JIP): about
  14,400 ops per hour per driving vehicle at 4 ops/s, about 4-6 MB of op text per vehicle-hour (inference from 3.4).
- MUD_RutTile (m_aFloor[256] + m_aRidge[256] floats, about 2 KB each) is never freed (J2 snapshot); m_sFieldPatches
  only grows.

## 4. Scaling (inference unless noted)

- **Vehicles:** server cost is per driving vehicle: about 4 rut ops/s x (6-14 ms op + about 3 colliders x 8 ms). Rebuild
  is capped at one collider per frame = 30/s on a 30 fps DS; at about 12 colliders/s per vehicle the cap is reached at
  about 2.5 driving vehicles, after that felt ruts lag behind the look ruts. Parked vehicles still cost Wheels + JoltProbe
  each frame.
- **Players:** every client applies every op (full carve + relax) whether or not it is near; network is per client
  (reliable broadcast). FieldDue scales with sources (players + vehicles).
- **Distance driven:** m_aOps, rut tiles and m_sFieldPatches grow with distance and never shrink; the JIP snapshot grows
  with them (tile bytes 10473 -> 36836 in one 22-minute WB session, table 3.4).

## 5. Only measurable on a live server (DS + game client, mod uploaded)

Run each with 1 and then 3 vehicles driving, at least 6 runs per setting (snap rate varies between run blocks).

| what | machine | exact log line to read |
|---|---|---|
| frame cost and parts | DS + client | `MUD prof 5 s` and `MUD prof parts` (rb collider, w flush, op all, ahead) |
| collider count and ms | DS | `MUD world: last 10 s` (colliders, ms) |
| op split | DS + client | `MUD op time: seq %1 kind %2, %3 ms \| flush %4 pre %5 carve %6 smooth %7 dirty %8 reground %9` |
| memory, rut patches, ops kept | DS + client | `MUD patches:` (mem KB, rut patch keys, ops) |
| look mesh share and replace | client | `MUD look ruts: 10 s` ('replaced by a shallower server rut'), `MUD delay look` |
| snaps | driving client | `MUD corr: ... mine 1` inside robot START / STOP |
| op bytes / JIP size | server | `MUD snapshot self-test: tiles ... vs rut op log ... bytes (... rut ops ...)` |
| client hitches | client | `vox` column >= 10 ms in `profile/MUD_framelog_*.csv` / MUD_Prof CSV |
| gain of item 1 | DS | `MUD op time` mean before/after with the Prints off (needs one run with them on to compare, then off) |
| collider cap saturation | DS | `MUD world: last 10 s` colliders near 300 per 10 s with 3 vehicles = cap hit |

## Applied 2026-10-18 (v220 = before, NOT COMPILED)

- Item 1 (prints): MUD_VoxelProbe.OP_PRINTS false gates the test-kit 'MUD op time' per rut op, GoLive 'MUD voxel
  probe: live' on batches with no bricks made, 'MUD rutmat' (both), JoltProbe 'MUD corr visual only'. TEST_KIT left
  true (user decides). Slow ops (OP_SLOW_MS) still print.
- Item 4 (client): MUD_WorldGroundEntity.LOOK_COVERS - a rut op whose changed floor samples are all at or under a
  client look sample (CATCH, no ridge) marks only the collider dirty (MUD_LookRuts.Covers, MUD_VoxelProbe.LookCoversOp).
  RENDER_GATE - patches get EntityFlags.SET_RENDERED (checked in the Experimental core pak EntityFlags.c) and a mesh
  rebuild waits while !IsRendered() and > RENDER_NEAR 25 m from the camera. Counters in the 'MUD patches:' line:
  'render gate ... mesh waits', 'look covers ... rut ops mesh kept'.
- Known gap (inference): LOOK_COVERS cannot see the OLD real floor; a sample whose real floor was deeper than the look
  and got lifted keeps the deeper drawing until the patch rebuilds again.
