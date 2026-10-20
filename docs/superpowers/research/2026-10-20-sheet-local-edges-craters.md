# Sheet local: the tile-edge wall, the crater delay, and the cost (research, 2026-10-20)

Answer to `prompts/2026-10-20-research-sheet-local-edges-craters.md`, plus a cost pass on
`results/2026-10-20-mud-lab-stage1-ds.md`. Code read only (three passes); no EDGE AUDIT numbers in the repo yet. Lines
are `Scripts/Game/Mud/MUD_MudSheet.c` unless named. I re-checked these claims myself:
- band time sits inside the upload timer (BandTile call at 7259 inside `Upload`);
- BuildNowSlots skips untraced slots (1883);
- the skirt is on every side (8399);
- UPLOAD_SLOTS 3 / WHEEL_SLOTS_MAX 6 / SLOT_MIN_S 0.25 (396-403).
Labels: PROVEN-CODE (file:line), MEASURED, INFERENCE.

## Short answers
- **Wall.** The band and touch DATA agree across tile borders. The band is world-consistent: BandMarkWorld births
  every tile the band can reach, and the chamfer over tile + margin is exact. What goes stale is the neighbour's
  MESH. While driving, the wheel slot rebuilds every 0.1 s, but a neighbour the band just grew into waits in the build
  throttles:
  - UPLOAD_SLOTS 3, with the Ural's 6 wheels often filling them, leaves one "oldest" pick per upload;
  - SLOT_MIN_S 0.25 s;
  - BandStored -> FlushOneBuild at 3 ms/frame for stored tiles.
  Its border quads were skipped at its last build (all band 0), so nothing is drawn on its side, while the fresh tile
  hangs its 0.45 m ONE_SKIRT curtain there. The curtain is on every side in sheet local (8399, SKIRT_ALL; no EDGE fade,
  HasTile true at 3153), and its UVs are copied from the top vertex, which makes the stripes. Two stale neighbours make
  the square corner. Once stopped, every path clears within ~1 s. Fix: co-build the border + an apron skirt (sec 1.3).
- **Crater.** Sheet local already builds traced live slots and meshed stored tiles in the voxel go-live frame. That
  fixed the old ~1 s from the synced sheet, where the log called a live tile done when it was only marked. What can
  still be late:
  - R1: the voxel path itself, ~0.15-0.25 s from the client's own boom + half the ping;
  - R2: an untraced live slot is skipped in the go-live frame (1883), then waits in Upload;
  - R3: stored taper neighbours 1-3 frames late;
  - R4: a running ease pulls the rim ground back.
  First ask the user: does the crater show late after the flash, or does the grass part show before the mud part?
  Then add a per-crater log line and fix R2-R4 (O1).
- **Cost.** The table double-counts: `BandTile` adds to the band column AND runs inside `Upload`, which is timed as
  'upload' (1047-1050, 7259, 7303). So mesh builds are ~0.6-1.5 ms, not 0.9-2.9 (INFERENCE). The band step is a full
  chamfer recomputed per tile, about once per build: 1369-2401 cells, `TouchAt` calls and `Math.Min` per cell,
  ~0.7-1 ms each, as costly as a tile build. None of the levers touch it. An incremental distance field fixes most of
  it (sec 3).

## 1. Problem 1 - the wall
### 1.1 Candidates (a)-(e)
| case | steady state (stopped)? | while driving | PROVEN-CODE |
|---|---|---|---|
| (a) tile beside not born | no: a cell with band > 0 has chamfer distance < margin, so it lies inside the birth box | no | BandMarkWorld 1544-1585, Birth 1508-1515; the chamfer metric is never shorter than the true distance |
| (b) born, no mesh | no: picked as meshless next upload (7319-7322); outside the window, QueueNear every 0.25 s; Freeze data-only only for a slot born 1-3 frames before it left | frames to ~0.25 s | 3071-3088, 3495-3524 |
| (c) border verts untraced | frames (121 traces vs 150 per frame); one possible persistent case: on a slope > ~4 m within the window the trace starts underground (7171-7178) and never recovers (INFERENCE) | frames | ZoneAtCellAny 1835-1865 |
| (d) band stale at the shared border | data no (both sides read the same touched set; Unpack copies are current; FillSlot skips traced cells). The MESH is stale: border quads skipped at the last build (8197) | yes, via (e) | BandTile 1643-1747 |
| (e) neighbour built before the band grew, still queued | no: when stopped, nDirty <= 3, so every dirty slot is picked (BW_FIT 7341-7351) | **yes, the main cause** | UploadPick 7325, 7350-7372; BandStored 1750-1765 -> QueueBuild; STORED_MS 860 |
Why the band crosses borders so often: the raised band is ~5 m wide around a truck track on 3 m tiles (1.5 m from each
tyre centre at preset 1).
Minor true divergences: op stamps write only stored copies at the window edge (OpTile 2860-2862); slump D in the
window-edge columns; the 11 x 11 build of tiles first built past FAR_R 40 m (never upgraded, 7756-7763); the
steep-slope trace above.
Audit blind spots: EdgeAudit skips corner verts (2047), and it is a 10 s snapshot. Read driving and stopped rows apart.

### 1.2 Why the skirt is there at all
It was added for see-through gaps while patch meshes were hidden (LAZY_RUT_MESH, comment 202-204). In sheet local
`LazyOn` needs SYNC (3314), so patches always draw. The curtain's reason is gone. It now only turns any lag into a
full-height striped wall.

### 1.3 Fixes, ranked (none has MP risk: client mesh scheduling / look)
| # | fix | wrong-look frames | cost | memory | cannot fix |
|---|---|---|---|---|---|
| 1 | **Co-build the border.** After BuildTile of T, compare each side's border y (EdgeRecord already keeps it) with the neighbour's record. If the step is > EDGE_STEP 3 cm, or the neighbour is open: a live neighbour is built in the same Upload (ignoring UPLOAD_SLOTS / SLOT_MIN_S, still needing SlotTraced); a stored neighbour is built at once (cap); a stored -> live pair marks the live one "build next upload". Cheaper first step: in UploadPick, treat dirty WHY_BAND or WHY_STAMP neighbours of picked wheel slots as wheel slots (no SLOT_MIN_S) | live-live 0, stored 0-1 | est. +50-150 builds per 10 s while driving (~+0.2-0.5 ms/frame), 0 when stopped | border record always on (~0.3 KB per tile) | an untraced or not-yet-built neighbour (#2 / #3) |
| 2 | **Apron skirt.** Bottom skirt verts move outward by about the drop (~45°) and down to G - BAND_UNDER - 2 cm; UVs from the bottom verts' world xz (UV64 frame) | any leftover lag reads as a soft mud bank, no stripes, no vertical wall | 0 builds, same vert count | 0 | still a straight edge for the lag time |
| 3 | local EDGE clamp toward a neighbour that cannot co-build (untraced, data-only, not built): border band = min(own, neighbour drawn), faded over ~0.6 m; that neighbour's build re-dirties the clamped tile. Never clamp toward a neighbour that is being co-built | a mud end instead of a wall | +1 build per event | a flag per side | only needed if the audit shows #1 misses cases |
| 4 | born tiles outside the window that the band reaches go to the FRONT of m_aBuildTx | removes the 0.25 s Radius lag | an existing ~2.9 ms build moved earlier | 0 | live lag |
| 5 | non-streak vertical skirt (bottom UV offset) | stripes gone, the wall stays | 0 | 0 | the wall |
| 6 | skirt off in local | a hovering lip with grass under it | 0 | 0 | the step |
Recommended: #1 (start with the cheap UploadPick variant, then the full co-build) + #2.

## 2. Problem 2 - the crater
### 2.1 Every step from the hit to the hole (60 fps frames)
| step | adds | PROVEN-CODE |
|---|---|---|
| S1 warhead init on the server -> Blast | 0 | WarheadComponent.c 22-26, WorldGround 5963-5975 |
| S2 crater op: Crater -> VoxelCrater -> Blast -> DoBlast -> Issue -> Submit -> VoxelBroadcast | the server op time, the RPC at the next send (<= 1 server frame) | WorldGround 6384-6403, VoxelProbe 817, 3276, 3591-3649, WorldGround 8961-8971 |
| N1 network | half the ping (MEASURED old WB: ~46 ms) | RpcDo_VoxelOp 8975 -> Receive 1618 |
| C1 Drain | 0-1 frame; + ~1 frame per op over 4 ms queued ahead | VoxelProbe 1690, DRAIN_MS 525 |
| C2 Submit: the field is written NOW (ApplyOp -> Carve 4189) | 0 | SubmitBody 4064-4220 |
| C3 brick build until the batch is empty | 101-163 ms over 4-5 frames on fresh ground (MEASURED, gotchas 328); later ops' bricks in the batch postpone it | Step 6664-6694, URGENT_MS 523 |
| C4 GoLive (UpdateVox ~28 ms WB) | the go-live frame is long (MEASURED >= 66 ms old WB) | 6727-6748 |
| H1 GroundChanged -> BlastNow (live: D x keep, bare; stored with a mesh -> now-tiles; data-only -> BlastKept; compact -> FarTake, an O(n) Find over up to 20,000 keys) | 0 | 3970, 4306-4322, 5413-5477, 2408-2468 |
| H2 FlushNow -> DigNow (stored hit tiles built now; taper neighbours to the queue front) | 0 | 5174-5381, QueueBuild 4576-4590 |
| H3 BuildNowSlots: builds now-slots, **skips untraced / unborn / no entity** | R2: a skipped slot waits for the trace, then the SLOT_MIN_S hold, then UPLOAD_SLOTS = 0.1-0.5 s+ | 1871-1895, Upload 7223-7324 |
| H4 FlushOneBuild: at least 1 front taper tile, then 3 ms | R3: 1-3 frames for the rest | 860-930 |
| H5 first draw uploads the mesh in-frame | 0 | IDA 2026-10-18 |
| R4 an ease running in the box writes the pre-crater floor back when it ends (DigNow writes m_aG without EaseDrop) | rim fringe and partly-bare quads too high until the next re-trace (~7 s for the window, INFERENCE) | EaseTick 4247-4276, DigNow 5288, FreezeLocal 1805-1820 |
Checked, not a cause: the band never re-raises a hole (a crater never touches cells; bare vertices are drawn at
G - SINK, all-bare quads skipped 7958, 8201-8204); slump (stamp box only, RIM_BARE out to BlastReach); JOB_DELAY (off
the path, 4338); no double clear (ClearOp is only counted in local).
Old sheet vs local: live holed slots used to wait in UploadPick for 0.1-1 s, and now build in the go-live frame
(5199). Taper neighbours used to sit at the end of the FIFO, and now go to the front.

### 2.2 Options
| # | option | wrong-look frames | cost | MP risk | measure first |
|---|---|---|---|---|---|
| O1 | **fix the stragglers**: (a) BuildNowSlots traces / fills an untraced now-slot's missing cells in place (Trace / HoleGround) and builds it; (b) right after BuildNowSlots, drain the BW_HOLE front entries of m_aOneBuild (cap ~12 tiles / 15 ms); (c) EaseDrop on every vertex DigNow writes | 0 (1 if the cap hits) | +0-8 builds + <= 441 traces per untraced slot, in the go-live frame | none | per-crater line: skipped untraced, taper late, eases in box |
| O4 | shorten the voxel path on this client: drain up to a pending crater in one frame; URGENT_MS 25 for a batch with a crater; close the batch after a crater so later rut bricks cannot postpone GoLive | none new | one longer frame (+10-15 ms), saves 1-2 frames | none (later rut ops land 1-3 frames later here) | recv -> submit, submit -> go-live |
| O2 | look-only hole at op arrival: BlastNow + builds at Submit (the footprint is pure maths from the op: Prepare 3833-3840, CraterCrest 868, BlastReach 5407; skip when RoofOver), and skip BlastNow at go-live | Submit -> go-live, 3-10 frames of mud-free but not-yet-cratered ground | ~2x builds | none | only if the log shows recv -> go-live > ~100 ms AND the user sees "mud stays" |
| O3 | pre-build stored tiles at Submit, SetObject at go-live | 0 (shorter go-live frame) | same builds, earlier; ~0.5-1 MB for ~150 ms | none | go-live frame ms split |
| O5 | predict at the client's own warhead init | false holes on craters the server skips | small | look can be wrong | not recommended |
Recommended: log first, then O1. O4 only if recv -> go-live dominates.

### 2.3 Log
A new small file `MUD_CraterTrace.c` (MUD_MudSheet is near its class-size limit): seq -> record.
- Server line: `MUD crater srv: seq S at P kg K | hit->issue a ms`.
- Client line, printed when every tracked tile is built or after 3 s:
  `MUD crater seq S: boom(local) -x | recv 0 (ahead n) submit +a | golive +b (fr, worst ms; golive frame ms: swap / vox / sheet) | sheet: live now built n / skipped n (untraced / unborn / no ent) -> built +c by upload | stored now n, data-only n, far-taken n, low 11x11 n | taper front n: same frame n, last +d fr | eases in box n`.
- 10 s line part:
  `| craters N: recv->golive avg/max, golive frame max, now-slots skipped, taper late, eases hit`.

## 3. Cost - why the first DS run was 2.3-5.3 ms, and the path down
- **Measurement.** Band time is counted in both 'band' and 'upload' (above). FreezeLocal's BandTile is in 'follow' and
  BuildZoneTile's in 'zones'; neither is in the table. 'MUD ms - mode ms' (non-sheet MUD work) is already 0.85-2.2 ms
  in every row, and there was no "off" row. So MUD <= 2 ms total would need the sheet at <= ~0.5-1 ms.
- **Band.** BandMarkWorld marks every tile in stamp box ± margin. BandTile then reruns the chamfer from scratch per
  tile:
  - w = 21 + 2m: 1369 cells at preset 1, 1849 at presets 2-3, 2401 at preset 4;
  - every live-cell read is a `TouchAt` call (1597);
  - 2 passes with up to 4 `Math.Min` each, plus `Math.Cos` in the fade.
  It runs ~40-60 times/s while driving, plus Freeze, new stored tiles and m_aBandQ. Preset 4 marks ~1.6x more tiles
  with ~1.75x more cells each, so ~2.5x (MEASURED 1.64-1.81 vs 0.4-0.72).
  Touched flags only go 0 -> 1 (6730-6733), so an incremental update is exact.
- **Band fix (b), recommended.** Keep a distance field (window field + tile data, carried by shift / Pack / Unpack like
  m_aBand). Once per Upload:
  - take the box of cells newly touched since the last upload, + m;
  - seed it from the stored distances (no TouchAt), with new cells = 0;
  - run the two chamfer passes inside the box only;
  - apply a band lookup table only to cells whose distance changed;
  - dirty only tiles whose band actually changed (replacing "all tiles in stamp box + margin").
  This gives the same result. The box is ~874 cells per upload at preset 1 (~1750 at preset 4) against ~4-12k today,
  so band 0.4-1.8 -> ~0.05-0.25 ms (INFERENCE). Keep BandTile only for tiles born outside the window and for queued
  stored neighbours, with micro-fixes (a): direct `m_F.m_aTouch` reads when tile + margin is in the window, if-compares
  instead of `Math.Min`, the band LUT.
  **Class-size limit:** MUD_MudSheetField is at 236 KB of 256 KB. A 6561-float distance array is ~26 KB, so it must
  live in its own holder class.
- **Builds.** More than the old sheet:
  - band-only neighbours are dirtied (WHY_BAND) and rebuilt as often as SLOT_MIN_S allows;
  - stored band neighbours have no throttle (BandStored -> QueueBuild every upload);
  - with L1 off, the blanket shift dirty + Freeze builds still apply.
  Local-only extra per build:
  - TOP_UV64 (8504-8530): ~3-4k natives, ~10-20 % of a build;
  - EdgeRecord (~3-5 %; turn EDGE_AUDIT off after the wall is settled);
  - the would-skip meter (~3-4 %, cheap).
  The builds-by-reason / would-skip counts are already in the same client log ('MUD sheet builds:' every 10 s). Pull
  them before coding.
- **Levers on** (all OFF in that run): L1 -25-40 % of builds, L2 wheels 0.1-0.58 -> 0.03-0.2 (part moves into upload,
  since LimitDragCells runs there), L3 slump -> ~0.02-0.07, L6 0-0.3 on repeat passes, L8 ~0 with one truck. Together
  ~-0.4 to -1.2 ms. No bad interaction with the band was found (StoreCurrent 2165 treats a queued band as not current).
- **Path, ranked:**
  1. read the builds-by-reason lines from the 21:14 log (zero code), and add an "off" row and >= 6 runs per row next
     time;
  2. levers L1 + L2 + L3 + L6 on (numpad 0 "all");
  3. band fix (b) + (a), plus the profiling fix (band outside 'upload');
  4. L5 as an early-out: the would-skip compare already runs before the quad loop (7968-7980), so return the old mesh
     when the max change is < 3 mm and the bare / hole classes and traces are unchanged;
  5. TOP_UV64 written straight into a member holder;
  6. SLOT_MIN_S for stored band-only rebuilds;
  7. then L4 (build rewrite).
  The far LOD ring is not a priority (it already exists at 11 x 11 past 40 m).
  Rough end state at preset 1 (INFERENCE): mode ~1.0-1.4 ms, MUD ~2-3 ms with the non-sheet work.

## 4. Smallest sessions
**WB (look, one launch):**
1. EDGE AUDIT, fixes off: drive >= 20 s, then stop >= 12 s, so one 10 s line falls entirely in the stop. Predicted:
   stopped = open 0 / step 0; driving = mostly "band 0" opens and "step, beside band lower"; "not born" 0.
2. Co-build + apron on: same drive. Is the wall gone by eye, and how do the audit numbers move?
3. Craters:
   - 3 on ruts with the truck stopped;
   - 3 while driving;
   - 3 within 1 s behind the moving truck (ease case);
   - 3 more than 40 m from the camera.
   For each, the user says: crater late, or mud late? Read the per-crater lines.
**DS (cost, one launch, >= 6 runs per row):**
1. off;
2. sheet local, levers off;
3. levers on;
4. levers + band (b);
5. levers + band (b) + L5 early-out;
6. the same 12 shells, for the crater timings.
