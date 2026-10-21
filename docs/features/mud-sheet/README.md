# Mud sheet + ONE GRID (mud zones) - PARKED THIRD TIME 2026-10-21

**PARKED AGAIN 2026-10-21 (user: 'park the sheet again, focus on ruts, tunnels, trenches; sheets later'): MUD_MudSheet.ENABLED false, FAR_PRESS false. v275 = last state (synced sheet whole-map DS test setup + FAR PRESS). Bring back: ENABLED true (SYNC true, SPARSE false = the 2026-10-21 DS setup), FAR_PRESS true, then compile + the far press WB look session.**

**2026-10-21: far track pop-in (research/2026-10-21-synced-sheet-server-research.md) - build plan for FAR_PRESS
(patch mesh pressed by the sync depth outside the ring) + MESH_CAP 1500 key: plans/2026-10-21-far-press-plan.md.
BUILT v275 (MUD_MudSheet FAR_PRESS / s_iMeshCap, MUD_MudSync.FarBox + FarPressMark hooks, MUD_WorldGround FarSetup /
FarAt / FarMark / FarRebuild, lab keys numpad / and -), adversarial review done, NOT COMPILED, WB look session next
(plan section "Workbench look session"). Open risks: asleep neighbour can show a short crack; m_sFarKeys and
m_sFieldPatches only grow (FieldDue scan cost - DS check).**

**Status: PARKED 2026-10-18 (user: 'park the mud zone, focus on the voxel ruts'; we may come back).** Switched off: `MUD_MudSheet.ENABLED = false` (gates the sync layer, the A2 patch lift, the under-sheet rut paths and the look-rut sheet hooks). Checkpoint v206 = last sheet state (SPARSE + A2 + D + paper tests 1-3 + LIFT_BY_DRAWN). Open when it comes back: the driven / undriven height step and the tile pop survived LIFT_BY_DRAWN (DS 16:26, user) - cause unproven; sync tiles are never freed (must-do cap).

## Lessons for the voxel ruts (from the mud sheet work, 2026-10-18)

Measured on the dedicated server unless marked otherwise; detail in research/2026-10-18-mud-mesh-cost-research-findings.md
and research/2026-10-18-fixes-2-4-and-brainstorm.md.
1. Mesh loops: hand-inline small helpers, read per-vertex flags once into a local int array, skip untouched cells
   = 1.10 -> 0.88 ms per tile build (-20 %, DS 16:16, same output checked). Bench recipe: N old + N new builds of
   the same data between two GetTickCount calls, compare the output count. Same recipe fits the patch mesh /
   collider builders and DispEmit.
2. Build what is next to the wheels first: wheel tiles first took the sheet delay from up to 9 s to ~80 ms
   (DS 12:24). A per-item rebuild limit (0.25 s) raised fps 58-72 -> 73-90 (DS 15:37) but must leave the
   near-wheel items free, or a "wave" chases the truck (DS 15:47 fix).
3. Measure lateness with a delay meter (wheel stamp -> drawn). Look ruts today: wheel -> look cut avg 400-530 ms,
   max 1.5-3 s (DS 16:16 / 16:27) - that is the gap behind the wheels.
4. Rut op cost: RelaxFloor is 5-6 ms per op; the crude cut (no relax, no ridges) took one op 15.7 -> 9.8 ms on the
   client and 14.4 -> 7.7 ms on the server (DS 13:38). The server 'MUD rut floor' Print was 2.2 ms per op (now
   behind OP_LOG). Re-ground exact + FLOOR_SPREAD removed the dirty-rebuild spikes (DS 13:52 / 14:11).
5. Skipping patch collider rebuilds under 2 cm change: only 0-4 skips per 10 s - not worth it (DS 16:16).
6. Far LOD works: far meshes at half resolution, back to full when the camera comes within 17 m (DS 16:16) -
   a candidate for far patch meshes.
7. Two systems drawing one visible surface (patch mesh + sheet tile, separate queues) gave pops and a height step
   that build order + a 1 cm lift did NOT cure (DS 16:26, user) - cause unproven. Keep one owner per surface.
8. Enforce: at most 64 locals in scope per method, parameters included (compile 2026-10-18).

Earlier status (before the 2026-10-18 park):

**Status: UN-PARKED 2026-10-18 as a look-only layer (synced sheet Stage 1 PASSED: compile + WB ok, user 2026-10-18 'looks good'; v177 = before, v178 = works). Stage 2 works WB (v181); Stages 3 + 4 WORK WB (v184; user 2026-10-18: craters + trench stay mud-free after walking 60 m away and back). Stage 5 late joiners BUILT, NOT COMPILED (v185 = before).**
**Hybrid feel (plans/2026-10-18-hybrid-sheet-feel-plan.md, results/2026-10-18-h1-snap-probe.md): H1 DONE (feel rut under the sheet, crude cut, lazy mesh, tile skirts, re-ground exact; user DS 13:52 'no more gaps', v199). BUILT, NOT COMPILED: H2 = MUD_LookRuts.SHEET_LOOK (client rut guess under the sheet, SHEET_GAP 0.1 m, drawn by the sheet via SheetFloorAt / LookChanged, no ease) + MUD_WorldGround.FLOOR_SPREAD (rut-hit building patch = 1 build step per frame).**
**STEP FIX LIFT_BY_DRAWN BUILT 2026-10-18 (NOT COMPILED, v205 = before; findings doc end): patch unlifted only under on-screen sheet tiles, lift 1 cm under the sheet top (was 4 cm), no groove at the edge. Paper tests DS 16:16: bench -20 % per build, LOD works, test 3 ~0 skips.**

**PAPER TESTS 1-3 BUILT 2026-10-18 (NOT COMPILED, v204 = before; research/2026-10-18-mud-mesh-cost-research-findings.md last section): s_bFastBuild + bench, LOD_ON far stored tiles 0.3 m, COL_SKIP_SMALL (MUD_WorldGround). One DS session next.**

**D RUT MAKING CUTS BUILT 2026-10-18 (NOT COMPILED, v203 = before; research/2026-10-18-fixes-2-4-and-brainstorm.md sec 7): D2 BuildTileMesh skips the TEMP gap probe (GAP_PROBE false), collider arrays when !COLLIDE and the per-quad Changed / DirtQuad calls SMOOTH_EDGE threw away; D1 SLOT_MIN_S 0.25 = a live slot with a mesh rebuilds at most every 0.25 s (wheel slots too). New MUD_Prof parts t setup / t quads / t skirt / t engine.**

**SPARSE SHEET A + A2 BUILT 2026-10-18 (NOT COMPILED, v202 = before; research/2026-10-18-fixes-2-4-and-brainstorm.md sec 5-6): MUD_MudSheet.SPARSE - radius tiles only where the sync layer has mud data; untouched covered ground = ground patches whose MESH is drawn THICK - PATCH_GAP higher (MUD_WorldGroundEntity.LiftSetup/LiftAt, times m_aLock); no lift over sync tiles or the live window (MUD_MudSync.HasTile, MUD_MudSheet.LiveTile; SheetLiftDirty on new sync tile / snapshot / window shift).**
**H3 TEST BUILT 2026-10-18 (NOT COMPILED, v200 = before): MUD_MudZoneEntity.ALL_OFFROAD - the sheet covers every 3 m tile whose middle is holed ground (offroad), zones or not (TileCovered).**
Stage 1 log (WB 01:00 session): 0 script errors, 0 'MUD mud sheet' WARNINGs, ReGround rebuilds 1-3 stored tiles per
rut op in 1-6 ms (one burst 20 tiles 32 ms - WB, not a perf number). 'MUD trench check ... WRONG' lines are old
(also in logs of 10-14 and 10-17 14:56).
Plan = the user's staged "Synced mud sheet" prompt (2026-10-17); proto: Tools/mud_sheet_sync_proto.py,
[results](../../superpowers/results/2026-10-17-mud-sheet-sync-proto.md). Rules: the sheet is pressed from the
replicated rut ops (Stage 2), each client predicts it at the tyres it sees, wheels and feet stand on the felt rut field.
User answers 2026-10-18: keep MUD_MudZoneEntity boxes (one already in Arlnd_Layers/default.layer at x 997 z 2730,
64 x 64, a sub-scene of MudArland - no zone to place); Stage 5 (late joiners) yes; look = drawn = felt ground + mud
depth (rut drops in 1.5 m behind the truck); sparse world tiles move into Stage 2; deterministic ground sampler (no
traces) for SYNC slump / sink; sink kept, from the op segment y.

Stage 1 (2026-10-18): `ENABLED` true; `COLLIDE`, `ONE_GRID`, `DRAG_TEST` false (no collider, the voxels / rut field
own the ground, no texture drag). Ground under the sheet over hidden cells = `HoleGround` (rut floor via
`MUD_WorldGroundEntity.DugUndug` where no voxel column - `MUD_VoxelProbe.ColumnAt`). Hook: `MUD_VoxelProbe.Submit`
calls `MUD_MudSheet.FloorChanged(lo, hi)` after FloorDirty -> `ReGround` re-reads the ground in that box (live window
cells: Trace; stored tiles: GroundDown + rebuild), mud depth kept. Guard: boxes > `REGROUND_MAX` 100 m are skipped
with a WARNING. DS: Tick returns early, no instance, hooks are no-ops. Open (review): J1 replay over a zone queues
many stored-tile builds built in one frame (hitch) -> Stage 3 storage rewrite; FlushOneBuild log still says
'MUD one grid'; ~600 lines of ONE_GRID-only code now dead (archive later).

Stage 2 (2026-10-18, BUILT, NOT COMPILED; v179 = before): the sync layer. Server: `MUD_VoxelProbe.DoRutOp` puts the
sheet tag in a kind-4 op's seed field (free under RUT_FIELD: rut ops convert no column), `MUD_MudSync.MakeTag` =
`m<veh>,<speed 0.1 m/s>,<radius cm>,<time 0.1 s>` (veh = per-track server counter `m_iSheetId`, speed = ground speed
at the flush, radius = largest wheel radius, time = GetTickCount / 100); Issue keeps it. Every machine but a DS:
`Submit` -> `MUD_MudSync.RutOp` (new file `MUD_MudSync.c`): sparse world tiles (TQ x TQ cells, zone tiles only), per
op segment the sheet's Stamp maths (BOX + SOFT + GRADUAL; pass rule per cell: other vehicle number or > 4 s op time
= new pass), sink = data ground beside the tyre (voxel TopAt, else DugUndug - no traces) minus the op segment y, then
`SLUMP_PER_OP` 3 slump passes over the changed box +1 (cells off zone tiles / without ground = walls; box >
`BOX_MAX` 120 cells: no slump, hash -1, WARNING). Test kit: `MUD sheet hash seq N: H` per op that reached a zone
(Tools/compare_op_hashes.py reads it; compare client A vs client B, a DS has none). Drawn side (`MUD_MudSheet.SYNC`):
own wheel stamps stay as prediction (`m_aPred`), no local slump; `SyncChanged` drops own stamps within `COVER_R`
0.3 m of the op's segments, restores the op box from the sync layer (window cells + stored tiles -> QueueBuild; bare
cells kept) and stamps the remaining own stamps again clipped to it. `ExpirePreds` (0.5 s): own stamps no op took go
after 5 s once their vehicle is gone or 10 m away, after 60 s anyway (parked truck: RUT_BEHIND never cuts its last
segments). `BuildZoneTile` reads the sync layer. Log: 'MUD mud sheet sync:' line every 5 s (held / taken / expired /
replayed stamps, restores, sync layer stats). Open: J2 joiners have no sheet history (Stage 5; their rut ops are
placeholders); J1 replay runs every op's restore in one frame (Stage 3); handoff morph (MORPH 0.85) not done; the
sync Stamp is a copy of the sheet's - keep both in step; a client whose WorldGround is not ready at an op gets a
different slump there (hash mismatch, check in MP).
Stage 2 first WB run (2026-10-18 01:25, user: 'fps had now a massive drop', feet a bit down in the grass): log
0 script errors, 78 hash lines, 0 untagged; sync layer 147-304 ms per 5 s while driving, 13-35 ms per op in ONE frame
(ops ~650 cells), of which the sheet restore + replay 57-143 ms per 5 s (816-1553 replayed stamps). Sheet frame
cost itself as in Stage 1 (upload 476-856 ms per 5 s, Stage 1 333-739). Fix 1 (not compiled): ops queue and run
DRAIN_MS 3 a frame (order kept) + a COST split in the sync line (stamps / slump + ground reads / hash / sheet
restore). Feet: nothing in Stage 2 moves characters or the felt ground; inference = the THICK 0.15 m layer (+ LIFT)
drawn over the ground hides the feet (user: only inside the mud zone).

Stage 2 WB run 2 (compile 01:34, Fix 1): queue max 1 - ops come ~one per frame, so the budget never splits anything;
one op = one 8-22 ms frame. COST per 5 s while driving (ops / total / stamps / slump (ground reads, cells) / hash /
sheet restore / worst frame): 19 / 257 / 68 / 80 (4323, 13237) / 10 / 95 / 18; 18 / 207 / 51 / 64 / 3 / 85 / 17;
12 / 156 / 45 / 46 / 5 / 58 / 22 (ms). Per op ~3.6 ms stamps, ~4 ms slump (~227 ground reads, ~700 cells), ~5 ms
restore + replay - spread evenly, no single hotspot. WB numbers (not perf). The run-2 'MUD mud sheet sync:' line came
out garbled (prefix lost, StatLine part twice) when Format + a call that Formats sat in one expression; fixed by
separate statements (not compiled).

OP SPLIT (user 2026-10-18 'split each op's work over several frames'; v180 = before), NOT COMPILED: MUD_MudSync works
an op in small steps across frames inside the DRAIN_MS 3 budget (JOB_STAMP one segment, JOB_GROUND one even-grid
row, JOB_CELLS, JOB_PASS one slump pass, JOB_WRITE, JOB_HASH, JOB_SHEET restore, JOB_REPLAY REPLAY_CHUNK 16 own
stamps). Same maths, same order, same look. Determinism: MUD_MudSync.Flush at the start of MUD_VoxelProbe.Submit (and
before J1 RangeRows) ends the open op before the next op changes the ground (DugUndug = raw rut field + heightfield,
updated in Submit), so a later op never leaks into an earlier op's ground reads. Bursts (several trucks, J1) flush =
one frame as before; the sync line counts 'flushes'. The drawn sheet holds its upload while a replay is unfinished
(failsafe REPLAY_HOLD_MS 500), so no frame shows the box without the own stamps. Expired own stamps are skipped by
a pending replay (m_bGone). Known: an ExpirePreds restore overlapping a pending replay drags those stamps twice
(look only).

OP SPLIT WB run 3 (compile 01:45:54, ~30 s drive, log logs_2026-10-17_22-45-55): 0 script errors, 73 hash lines,
log line clean. Per 5 s: ops 11 / 19 / 13 / 13 / 17; worst frame of op steps 3 / 4 / 4 / 5 / 4 ms (run 2: 17-22);
flushes 0 / 0 / 3 (3 ms) / 0 / 1 (0 ms); op work 142-337 ms (same total as run 2, now 65-126 frames). What is left
is the sheet's own mesh upload (Stage 1 code): 568-799 ms per 5 s, ~38 uploads, sheet frame max 31-34 ms = the
upload frames. WB numbers (not perf). Stage 3 (meshes within a camera radius) is where that is cut.

Stage 3 plan (memory + cost, to build): sparse tiles; sheet meshes only within a camera radius (start 40 m - user to
confirm); remove the zone pre-build; trim the TaperField cost; 10 s TEST_KIT log line. ADDED (user 2026-10-18): LAZY
RUT MESH under the sheet - in zone cells the sheet draws, the real rut patch MESH is hidden under the sheet and
rebuilt lazily / batched; the rut COLLIDER stays immediate (physics, MP). Hide, not only delay: a stale mesh is
higher than a pressed sheet (rut depth vs LIFT 0.01) and would poke through. Outside the camera radius (no sheet
mesh) the patch mesh must show again. Run 3 numbers to beat: 76-104 patch mesh rebuilds per 10 s (372-514 ms),
colliders 20-23 (118-145 ms) - WB only.
User answers 2026-10-18: camera radius 40 m; feet standing a bit in the mud layer - leave as is.

Stage 3 (2026-10-18, BUILT, NOT COMPILED; v182 = before). CAMERA RADIUS (`RADIUS_ON` true, off with ONE_GRID):
no zone pre-build at load ('MUD mud zones: N zones, N tiles, built within 40 m of the camera (no pre-build)').
`Radius()` every `RADIUS_EVERY` 0.25 s: `DropFar` hides stored meshes past `RADIUS` 40 + `RADIUS_HYST` 6 m (deleted
1 s later) and drops the store, unless it has bare cells (kept data-only); `QueueNear` lists zone tiles within 40 m
with no mesh, nearest built first, `RAD_BUILD_MS` 4 a frame (at least one tile). Untouched tiles keep NO data:
their mud comes from the MUD_MudSync layer (THICK where it has no tile) - `FillSlot` (window), `BuildZoneTile`
(stored), `MudAt`. Craters on a zone tile without a mesh go into a data-only store (`BlastKept`). TaperField only runs
near tiles that got bare cells (`MarkBareTiles` / `NearBareTile`); the zone-edge taper is unchanged.
LAZY RUT MESH (`LAZY_RUT_MESH` true): `MUD_MudSheet.Covers(box)` = every tile within EDGE is a zone tile, every tile
the box touches has a hole-free sheet mesh up (ready live slot, or a stored mesh built with every cell traced -
`m_iDrawn`) and no bare tile in reach. MUD_WorldGround.Rebuild: a mesh-dirty patch the sheet covers is HIDDEN
(VISIBLE flag off) and its mesh rebuild held; its collider still rebuilds at the normal pace. `SheetShow` (each frame)
shows a hidden patch again once the cover is gone (mesh dropped past the radius, crater, zone edge, live slot not
ready) - re-checked every 0.25 s and at once when `MUD_MudSheet.CoverGen` changes; its held mesh then rebuilds.
Logs (TEST_KIT, every 10 s): 'MUD sheet: tiles touched N, meshes N, builds N, mean N ms, worst N ms, sync stamps N,
replay stamps N | (stored + live meshes, kept data-only, dropped, to build, radius)'; the WorldGround patch line gets
' | lazy rut mesh true: hidden under the sheet N, held patch-frames N, shown again N'. PERF only on a DS / the game,
never judged in WB.
Self-review (2026-10-18): fixed - a stored tile frozen before its cells were traced has holes in its mesh, so it must
not count as cover (`m_iDrawn`). Known, left for Stage 4: a trench / dig on a zone tile with no mesh (past 40 m) is
not kept - the mud covers it when you come back (craters are kept); tiles once marked bare never lose the mark (the
lazy mesh stays off there); held patches keep 'oldest dirty' growing in the patch line; the mud ends with a step at
the 40 m radius (no taper there, by design - zone-edge taper only).

Stage 4 OP CLEARING (2026-10-18, BUILT, NOT COMPILED; v183 = before; Stage 3 is not compiled either - both go into
one compile + WB session). Craters / tunnels / trenches / fills (op kinds 0-3) now clear the SYNC layer too, in op
order, on every machine but a DS, so every client has the same holes and a dig past the 40 m radius is kept.
- `MUD_VoxelProbe.Submit`: `MUD_MudSync.ClearBefore(op)` just before ApplyOp (kinds 1-3: data ground of every
  zone-tile cell in the op box + 1 cell, read now - Submit flushed the open op work first, so it is this seq's
  ground); `MUD_MudSync.ClearOp(op)` after (kinds 0-3) queues the clear in the same op queue.
- Clear job (`JOB_CLEAR`, one box row per step inside DRAIN_MS): crater = cells `MUD_MudSheet.BlastKeep` takes (pure
  maths from the op, crest = `MUD_VoxelProbe.CraterCrest`, box = BlastReach); dig = cells whose data ground
  (`GroundY`) moved more than `MOVED` 0.08 m. Such cells: mud x keep (0), bare flag `m_aB` = 1. Then the hash line
  ('MUD sheet hash seq N' - the hash now includes the bare flag, for rut ops too), no sheet restore (the drawn sheet
  clears at the go-live as before, GroundChanged).
- Guards (WARNING, the same on every machine): dig box > `BOX_MAX` 120 cells (all its cells are read in one frame)
  or crater box > `CLEAR_MAX` 400 cells = not cleared; crater box > BOX_MAX = hash -1.
- Drawn sheet reads the bare flag: `MUD_MudSync.Get` has an `out int bare`; FillSlot, BuildZoneTile, RestoreBox take
  it (a sync-bare cell goes bare). With SYNC on, `DropFar` drops every store (no more data-only kept stores) and
  BlastNow no longer makes them (`BlastKept` only runs with SYNC off).
- Log: the 5 s 'MUD mud sheet sync:' line ends with ' | op clearing: N crater / dig ops, N cells bared, N ms'.
Adversarial review (2026-10-18): fixed - clear ops were counted as rut ops in the sync line. Known / accepted:
a rut-op restore between a crater's Submit and its go-live can show the bare cells a moment early (look only); a
big fill / dig box reads up to 120 x 120 grounds in one frame (trench pieces are a few m; watch the clear ms);
drawn bare (traced ground) and sync bare (data ground) can differ by a cell at a dig edge - after the tile leaves
the radius the sync version is what comes back; a client without zones loaded at an op skips its clear (hash
mismatch, as for rut ops); craters inside a tunnel still clear the surface mud (as BlastNow always did).
Stage 3 'Known' item done by this: trench / dig on a zone tile with no mesh is now kept.
Stages 3 + 4 WB run 1 (compile 09:20:53, log logs_2026-10-18_09-09-49): compiles, 0 MUD script errors / warnings,
166 'MUD sheet hash seq' lines. Radius works ('MUD mud zones: 1 zones, 462 tiles, built within 40 m', meshes 16-364,
dropped up to 343 per 10 s, builds mean ~1.5 ms, worst 2-3 ms; 0 kept data-only). Lazy rut mesh: hidden under the
sheet 1, held patch-frames 685-730, shown again 0. Crater seq 44 (crest 2.17 m): 1396 cells bared in 1 ms; a
second crater 758 cells, 1 ms. PROBLEM: one placed trench = 72 trench-cut ops in ~1.7 s -> 529992 ground reads,
71 flushes = 2469 ms, clear 2260 ms in one 5 s window (frames to 100 ms). Cause (log): every trench piece op carries
the WHOLE trench's box (~25-36 voxel columns = ~10 x 10 m), and ClearBefore read every sheet cell of it twice
(~7400 reads per op). WB numbers, but the cause is a count, not a timing.
FIX (2026-10-18, compiled, works - run 2 below): the voxel probe keeps a column's top just before the op's first Put there
(`MUD_VoxelProbe.KeepTop` while `m_bTopTrack`, through SmoothBox; map `m_mTopWas`); the clear job's box = the
columns whose top moved + 1 grid cell (`TopsMoved`), before = `TopWasAt` (TopAt with the kept tops), after =
GroundY. Same on every machine (Puts are). DigBefore no longer reads anything. Dig box guard now CLEAR_MAX.
WB run 2 (same log, from 09:33): 72 trench-cut ops + 7 blast ops; trench sheet hash lines ~25 ms apart (was a 2.5 s
stall); 10 s lines builds worst 2-3 ms; 0 MUD errors. Go-live dig now: 72 voxel changes, 32 tiles in 94 ms (one shot,
local). The 5 s 'MUD mud sheet sync' line did not print (nobody drove) - clear ms not read. User: craters + trench
stay mud-free after walking 60 m off and back. v184 = works. MP hash check (client A vs B) still to do on the live test.

Stage 5 LATE JOINERS (2026-10-18, BUILT, NOT COMPILED; v185 = before). Problem: a J2 joiner skips every rut op <= S
(placeholders), so it had no sheet history. Fix: the server keeps the sync layer too (a DS as well -
`MUD_MudSync.SERVER_LAYER` true; `MUD_MudSheet.Tick` on a DS calls `MUD_MudSync.DsTick` = Drain + a 5 s
'MUD mud sheet sync (server):' line while ops come). `MUD_VoxelProbe.MakeSnap` flushes the layer and adds it to the
snapshot as "M|S|i|..." parts after the rut tiles (same numbering / head count, `MUD_MudSync.SnapParts`: first item
"L<tiles>", then "key,array,start:tokens" per 100 cells of one of the 7 tile arrays; tokens "n<c>" default run,
"r<c>" repeat, else delta; every tile sends "key,0,0:" at least - tile existence matters for LimitDrag + hash).
Joiner: `ReceiveSnapHead` -> `MUD_MudSync.SnapComing(S)`: ops <= S do no sheet work here (no clears, no hash lines -
they would differ); `LoadSnap` (all ops <= S in) -> `MUD_MudSync.LoadSnap`: layer replaced, then
`MUD_MudSheet.SyncReload` (RestoreBox) per old + new tile; no "L" item = server keeps no layer, ours stays (WARNING).
Exactness: every op now ends with JOB_QUANT (its box's floats to 0.01 mm steps, `QUANT_F`; a value at its array's
default stays), so the text carries the exact floats (`FromQ` = the one expression on both sides) and later ops give
the same mud on the joiner. Token format round-trip checked offline (20000 random cases, scratch script).
Logs: server 'MUD sheet sync: snapshot at seq S: N tiles in P parts, B bytes ... layer hash H'; joiner 'MUD sheet
sync: snapshot loaded at seq S: N tiles, 0 bad parts ... layer hash H' (H must match); SP test kit every 30 s 'MUD
sheet snap self-test: ... 0 cells wrong, 0 missing, 0 extra, 0 bad parts'. Open: DS cost of the layer (live test,
not WB); a DS sheet hash line per op now prints too (server vs client compare possible); with SERVER_LAYER false a
joiner also misses crater / dig bares <= S.

History below = the parked full sheet (2026-10-15).

**Status: PARKED 2026-10-15 ~17:05 (user).** Switched off: `MUD_MudSheet.ENABLED = false`
(Scripts/Game/Mud/MUD_MudSheet.c). With it off the game runs the plain voxel ground + voxel ruts (v035 behaviour).
Last sheet state saved as **checkpoint v083** ("mud sheet latest before parking"). To resume: set ENABLED true,
recompile, then work the open list below first.

Full assessment (why it was hard, whole-map numbers, voxel alternatives): [2026-10-15-assessment.md](2026-10-15-assessment.md).

## Why parked
- USER 17:0x: ruts in a mud zone tank the frame rate, and it jittered on a dedicated server.
- USER: doing many things at once - park it, get the rest solid.

## What it is
- Client-only soft mud layer (0.15 m cells, 3 m tiles) inside placed `MUD_MudZoneEntity` boxes
  (Scripts/Game/Mud/MUD_MudZone.c). Wheels press ruts and push ridges; a live 12 m window follows the vehicle.
- ONE GRID: inside zones the sheet IS the ground. The skin cells it covers are hidden; its collider is the ground.
  Crater / trench / fill / rut ops in a zone change the sheet's ground (`MUD_MudSheet.TakeOp`), not the voxels.
- Plan: [../../superpowers/plans/2026-10-15-one-grid-proof.md](../../superpowers/plans/2026-10-15-one-grid-proof.md).
- Edge review: [../../superpowers/research/2026-10-15-sheet-edge-review.md](../../superpowers/research/2026-10-15-sheet-edge-review.md).
- Merge notes: [../../superpowers/research/2026-10-15-sheet-voxel-merge.md](../../superpowers/research/2026-10-15-sheet-voxel-merge.md).
- Freeze test (heightfield ruts under the sheet cost ~13 fps): [../../superpowers/results/2026-10-15-freeze-test.md](../../superpowers/results/2026-10-15-freeze-test.md).

## Where we got to (2026-10-15)
User-OK along the way: zones, crater hook, terrain look, no squares, SOFT ruts, SMOOTH_EDGE, trenches / shovel / fill
into the sheet (v078 = the "most stable" sheet version), tunnels with holes in the sheet.

Last session (checkpoints v081-v083), user: "nice now it's much better":
- Shell inside a tunnel -> voxels only (`InVoxels`, IN_VOX 0.3).
- Crater / trench meeting voxel columns (tunnel) or crossing the zone edge -> sheet AND voxels. The sheet part is
  HELD (`m_aPend`) and applied at the voxels' go-live (SYNC), so both halves show together.
- Voxels do not draw a crater / trench in sheet-covered cells (`MUD_VoxelProbe.m_bSheetOp`, `MudSheet.CoversCell`);
  DigNow only opens the sheet where the voxels draw (`MUD_VoxelProbe.DrawsAt`). Fixed rim strips + sky holes.
- Stored tiles built once per change (`QueueBuild` / `FlushOneBuild`), the frame after the go-live.
- TAPER 0.75 m across tile borders (`TaperField`) with a sine shoulder (`TAPER_CURVE`): the mud edge curves down.
- BuildTile `tv` overflow fixed (the cut cap reserves the grid verts still to come).
- Held ops applied PEND_STEP_MS 8 per frame, DigNow after (a 60-op trench beside a tunnel froze ~0.7 s).
  Written after the last test - NOT COMPILED, NOT TESTED.

## Check FIRST when the sheet comes back (user 2026-10-15)
- [ ] **DS stutter.** USER 17:09: with the sheet OFF the dedicated-server stutter was gone (v084). Turn the sheet
  back on, run the same DS test, and find what in the sheet makes it stutter BEFORE any new sheet work.
  Likely cause (INFERENCE from code, see [2026-10-15-assessment.md](2026-10-15-assessment.md) 2.2): ONE_GRID makes
  the client-only sheet the wheel ground, but a DS has no sheet - server and client drive on different ground.
  Test ONE_GRID false vs true on a DS (Diag exe corrections count).
- [ ] **Rut fps tank** in a mud zone (item 1 below) - measure it first.
- [ ] Compile + test the last untested code: held-op spreading (PEND_STEP_MS), tile builds a frame after go-live.

## Open (do these first when it comes back)
1. **Rut cost (USER: fps tank).** LOG 17:01 client: each rut op 4-12 ms every ~0.25 s (`op kind 4 ... tiles in X ms`,
   2-5 stored tiles rebuilt). INFERENCE (not measured): the live window now marks +-6 cells round every change
   (`MarkRect(... TAPER_PAD ...)` in ApplyGroundOp) and every live tile build runs `TaperField`, so a rut rebuilds
   more tiles than before. Try first: skip the taper padding for rut ops (kind 4 makes no bare cells), then measure.
2. **DS jitter (USER).** USER 2026-10-15 17:09: with the sheet OFF (v084) the DS stutter is gone - so the sheet
   caused it (how is not measured). Not looked at further. The sheet is client-only, so check whether the jitter is the sheet's frame
   cost or vehicle corrections (Diag exe: Vehicles > "Vehicle Networking and Reconciliation").
3. **Tunnel mortar stutter.** LOG: go-live frame ~100 ms (voxels ~50 + sheet ~50). Tile builds moved one frame
   later (untested).
4. **GM can't select the tunnel** easily. Not investigated (guess: the sheet collider over the mouth).
5. Earlier LATER list: ridge-side UV stretch, wall-collider probe, CollPending double rebuild.

## Files
- Scripts/Game/Mud/MUD_MudSheet.c - the sheet (TakeOp, ApplyGroundOp, DigNow, BuildTile, TaperField, ...).
- Scripts/Game/Mud/MUD_MudZone.c - zone boxes.
- Scripts/Game/Mud/MUD_VoxelProbe.c - Submit hook (`both`, HasCols, Holds), MarkCarved sheet skip, DrawsAt.
  These voxel-side hooks do nothing while ENABLED is false.

## Checkpoints
v077 pre-trench-into-sheet, v078 sheet base (user: most stable), v081 tunnel craters both (user: big improvement),
v082 before the sync/draw fix, v083 last state before parking.

## Details (moved from CLAUDE.md 2026-10-15, kept word for word)
- Mud sheet (MUD_MudSheet.c, client-only visual mud, 2026-10-14/15): soft layer THICK 0.15 m on our ground, 0.15 m cells, 3 m tiles (TQ 20). Mud only inside placed MUD_MudZoneEntity boxes: all zone tiles built at load (BUILD_MS 8/frame) and stored; a 12 m live window (FOLLOW) follows the nearest vehicle, takes stored tiles over and freezes them back. Wheels press ruts + push ridges (arch round tyre front/back, NOSE/ARCH_H), drag on vehicles. Ground height on hole cells = MUD_Surface.Y (GetSurfaceY is -256 there). Voxel hook: VoxelProbe.GoLive -> MUD_MudSheet.GroundChanged -> tiles redone 0.1 s later; crater blast clears mud (BlastKeep, BLAST_CLEAR), ground moved > MOVED 0.08 m clears mud (trenches). TOP_LOOK: untouched quads draw MAT_TOP (terrain look), touched ones dirt (2 sections, 2 emats). COLLIDE: per-tile static trimesh on CharCollide 0x80000 only (soldiers), live tiles max 1/COLL_S s; NOT FireGeo because all our ground reads are Projectile traces and would read the mud top. User-confirmed: zones, crater hook, mud on our ground, crater clearing, no squares, terrain look. 2026-10-15: ground ruts back to v035 voxel ruts (RUTS true, RUT_BEHIND_GAP 3, RUT_LOOK false) because the heightfield ruts under the sheet cost ~13 fps (docs/superpowers/results/2026-10-15-freeze-test.md).
- ONE GRID (2026-10-15, plan docs/superpowers/plans/2026-10-15-one-grid-proof.md): in mud zones the sheet IS the ground (zone skin cells VOX, collider = sheet ground verts on Terrain|Foliage); crater/trench/fill/rut ops in a zone -> MudSheet.TakeOp, voxels only for tunnels (BARE_VOX holes). Earlier user-OK sheet parts: GRADUAL press (PRESS_STEP 0.05), BLAST_NOW + RIM_BARE, BOX ruts (HALF_PER_R 0.38), MERGE (SOIL_TILE 12, TAPER 0.6, HOLE_SKIP), DIG_NOW, SOFT ruts, SMOOTH_EDGE.

- 2026-10-19 LOOK LIFT TEST (TEMP, NOT COMPILED, v242 = before): MUD_MudSheet.LOOK_LIFT_TEST draws the coarse skin hole
  cells and all patch meshes 0.16 m higher (mesh only) to judge redesign option 1 (research/2026-10-19-sheet-redesign.md).
  RESULT (DS 2026-10-19): ruts invisible with the lift - grass rides on any ground above the heightmap, so a lifted rut
  keeps its grass. Switch OFF again. Any lift route must press the lift out where the ground is cut.
