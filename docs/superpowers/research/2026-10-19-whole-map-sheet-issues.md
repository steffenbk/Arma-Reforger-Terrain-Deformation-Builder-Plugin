# Whole-map mud sheet: pop-in, late craters, sheet over holes (research, 2026-10-19)

Answer to `prompts/2026-10-19-research-whole-map-sheet.md`, based on `results/2026-10-19-whole-map-sheet-test.md`
(WB host + PeerTool, SPARSE false, ALL_OFFROAD true). Research only; no code changed. Line numbers are
`Scripts/Game/Mud/MUD_MudSheet.c` at 6e53b94 unless named otherwise.
Labels: PROVEN-CODE = the mechanism is in the code (whether it is what the user saw is still a test), MEASURED (log),
[repo] (our docs), INFERENCE.

Two corrections to the prompt first:
- `BARE_VOX` (2) is ONE_GRID-only. With ONE_GRID false (today) a voxel dig bares a sheet cell with bare = 1 only when
  its ground moved more than MOVED 0.08 m (DigNow 3420-3432, MUD_MudSync.c 642 for the sync layer).
- The test ran with SPARSE false. That setting draws a sheet tile over every offroad tile in the radius, untouched or
  not. Most of issue 1 comes from that choice (below).

---

## Issue 1 - pop-in while driving

### 1.1 Can every offroad tile be built once and kept? No.
- Holed ground on Arland: 1 400 844 of 4 194 304 two-metre terrain cells (last hole write,
  plans/2026-10-18-whole-map-cut-plan.md line 18) = 5.60 km2 [repo]. In 3 m tiles: 5 603 376 / 9 = ~622 600 tiles.
- Memory per tile [INFERENCE from IDA facts]: ~700-1000 vertices x 28 B kept on the CPU for life (script meshes keep
  their CPU arrays, IDA 2026-10-18) + the same on the GPU + ~5 KB indices each side + 10.6 KB tile data (6 x 441
  floats) + the entity = ~60-75 KB. All tiles: ~37-47 GB. The one measured figure (~178 KB per tile incl. collider,
  2026-10-15 assessment, noisy) would make it worse.
- Build time: 622 600 x 0.55 ms (WB BuildTileMesh mean, mesh only) = ~5.7 min; with the ground and sync reads a whole
  approach tile was ~2.9 ms on the DS (all-offroad deep dive) = ~30 min. And ~622 000 entities against ~66 000
  RELATIVE_Y roots in the whole vanilla map (gotchas PinProbe).
- A 0.6 m far version cuts the mesh ~16x but the per-tile data and entity stay: still several GB.
Verdict: not within reason for any client.

### 1.2 What gives "loaded but static" at a cost that works
The untouched sheet is the same terrain material lifted THICK + LIFT = 0.16 m (all-offroad deep dive sec 3.2). It
carries no information. Two meshes that already exist everywhere can draw that layer, built once and static:
- the ground patches (8 m, built ahead of vehicles / the camera), and
- the coarse skin (64 m chunks, all 1697 holed chunks built once at load [repo plan 3]).

Proposal, ranked:
| # | what | fixes | cost | risk |
|---|---|---|---|---|
| a | SPARSE true (already built, v202): sheet tiles only where the sync layer has mud; untouched ground = patch MESH drawn 0.15 m higher (A2 + LIFT_BY_DRAWN, colliders unchanged) | no sheet tile at all on untouched ground, so nothing to draw in; 'sh zones' went to ~0.03 ms on the DS (fixes-2-4 sec 7) | none new | the driven / undriven step and tile pop seen on DS 16:26 are still open (mud-sheet README) - fix that before whole-map |
| b | Lift the coarse skin's hole cells by the same 0.15 m once at load (with the same lock fade to 0 at kept terrain) | the patch / skin seam step A2 left (fixes-2-4 sec 6 risk 2) | one-time build at load, no per-frame cost | the skin is the far fallback; check the seam by eye |
| c | Keep touched-tile meshes instead of dropping them: MESH_CAP sized to memory (~60-75 KB a tile: 3000 tiles ~ 200 MB [INFERENCE]), drop only far behind | no rebuild churn on driven areas | memory | measure with System.MemoryAllocationKB |
| d | Build touched tiles with a fixed large radius (60-80 m) | touched tiles lie along tracks, so demand is per track metre, not per area | small | none known |
| e | No LOD for approach builds (see 1.3) | the second build + detail pop | none | - |

Not recommended:
- "Hide new tiles until built, old ground under them": under SPARSE false the ground under an untouched tile is 0.16 m
  lower, so the appearing tile is still a 16 cm pop. Under SPARSE true there is nothing to hide.
- Engine LOD / view distance for script entities: a script MeshObject has one LOD ("custom", IDA 2026-10-18 Q4), so
  SetFixedLOD / LOD Factors do nothing for it. The engine frustum-culls by bounds anyway, and the render A/B showed
  354-742 hidden sheet meshes save ~0.2 ms [repo]. Dropping meshes is about memory, never about render.

Look risk shared by the sheet and by option a/b (INFERENCE, check by eye): every offroad surface is drawn 0.15-0.16 m
above the heightmap. Grass clutter is planted at heightmap height (mud-gotchas: clutter shows over hole cells when the
ground is >= 1 mm above the heightmap), so short grass would sit partly under the drawn ground. The SPARSE-false test
had this everywhere too.

### 1.3 Why the build radius shrinks with speed, and the rule
PROVEN-CODE (comment at 224-226, code 1818-1819): build radius = SPEED_RADIUS_K 320 / speed, clamped 20-40 m.
Demand at radius R is ~0.141 x R x v tiles/s (deep dive: 5.65 v at 40 m); the builder supplies ~1 tile a frame
(RAD_BUILD_MS 2 lets only the first tile through at ~0.55-2.9 ms each, 1648-1657). R = 320 / v keeps demand at
~0.141 x 320 = ~45 tiles/s whatever the speed. So it trades a closer edge for no backlog.
It is the wrong lever for looks: the edge comes closer exactly when pop-in is most visible.
Rule I propose:
- with SPARSE (option a): no speed rule; untouched ground never needs a tile, touched tiles use a fixed 60-80 m radius.
- if SPARSE stays off: the radius must not shrink; the only honest options are fewer builds (option e) or accepting
  that 10-20 m/s outruns the builder (the test: queue 284 at 22:11:15).

Why the test built 900-1700 tiles per 5 s when ~230 approach tiles would do at 10 m/s, R ~33 m [INFERENCE from code]:
- LOD: an approach tile built while the camera is > LOD_M 20 m away is built LOW (5707-5712); when the camera comes
  within 17 m, LodUpgrade queues it again (5674-5691) -> every approached tile is built twice, and the second build
  is a visible detail change - part of the "loading effect".
- MESH_CAP 600 against a ring of ~470 holed tiles within 46 m plus the turning circles of the drive -> drops and
  rebuilds (DropFar 1825-1872).
- the live window (16 slots at up to 10 Hz) and stored rebuilds from rut ops / restores come on top.
Log to settle it: count builds by reason on the 'MUD sheet:' line - approach-low, approach-full, LOD upgrade, live,
stored (rut / restore), rebuilt-after-drop.

---

## Issue 2 - the crater shows ~1 s late

### 2.1 Cause
PROVEN-CODE, two paths that can add up to ~1 s without any log line saying so:
1. **Crater tiles in the LIVE window wait for upload picks.** BlastNow only marks a live slot dirty (MarkRect, 3506),
   and DigNow does the same for live slots (with TAPER_PAD). Live slots are built only by Upload every 0.1 s, and
   UploadPick builds at most UPLOAD_SLOTS 3 non-wheel slots per upload, longest-waiting first, wheel slots first
   (WHEEL_SLOTS_MAX 6), and holds any slot built less than SLOT_MIN_S 0.25 s ago (5297-5312). A crater reaches
   r + max(0.5, r / 2.2) = 4.1 m for the 2.81 m crest (BlastReach) -> up to 3-4 x 3-4 tiles; inside the 4 x 4 live
   window that is up to 16 dirty slots -> 5+ uploads (0.5 s) plus the 0.25 s holds plus wheel slots first while
   driving. The 'blast now: 12 tiles in 2 ms' line is data only (2 ms for 12 tiles); the live part is not logged.
   The earlier in-zone delay meter saw stamp -> drawn max 0.86-9 s on live slots [repo sheet-alternatives sec 13-15].
2. **Taper neighbours queue behind everything else.** Stored crater tiles are built at once in DigNow (3446), but
   neighbours queued by QueueBuild go to the END of the m_aOneBuild FIFO (2665-2669, RemoveOrdered(0) at 739), which
   FlushOneBuild drains at STORED_MS 3 a frame (~5 tiles). In the whole-map test that queue also holds every LOD
   upgrade and rut-op stored rebuild (issue 1), so a crater's edge tiles can wait many frames.
Less likely (INFERENCE): a live-window shift before the sync layer's crater clear runs (ClearOp is queued behind rut
op steps, DRAIN_MS 3) refills slots from the uncleared sync layer (FillSlot) - that would cover the hole for longer
than 1 s, not delay it; the blast smoke (our MUD_GroundFollow hides the blast ParticleEffectEntity, gotchas 2026-10-17,
so likely not); first-draw GPU upload is per mesh and inside a frame (IDA 2026-10-18), not 1 s.

### 2.2 One log line that settles it
Per crater op, client only, printed when every tile it touched is built or after 3 s:
`MUD crater timeline seq S: live slots L (first built +a ms, last +b ms, uploads n, put off by UPLOAD_SLOTS x, by
SLOT_MIN_S y, wheel slots first z) | stored S (built +c ms) | taper queued Q (queue ahead at blast k, last built +d ms)
| voxel shown +v ms | sync clear done +w ms | slots refilled from sync before the clear r`
(t0 = BlastNow; store the slot indices and tile keys it marked, stamp them in Upload / FlushOneBuild / DigNow.)

### 2.3 Hide at once?
Not recommended: hiding the tiles over the blast drops the whole area 0.16 m for a moment (SPARSE false) and, under
LAZY_RUT_MESH, shows patches whose meshes were held stale (see-through risk, mud-sheet README Stage 3).
Instead build in the blast frame: every live slot BlastNow / DigNow marked is built in that frame, ignoring
UPLOAD_SLOTS and SLOT_MIN_S, and blast / dig taper neighbours go to the FRONT of m_aOneBuild (or are built in the same
frame). Cost: ~9-16 tile builds once per crater (~5-9 ms WB, ~2x on a DS client [INFERENCE]) on top of the blast frame.

---

## Issue 3 - sheet sticks out of a trench (and sometimes a crater hole)

### 3.1 Cause
- **Taper is at 0 at the bared vertex** (TaperFade 5602-5608: distance 0 -> fade 0; vy = G - SINK there, 5894-5896),
  so the taper itself does not stand above a bared cell. At 1 cell out the sheet is ~G + 0.01, at 2 cells ~G + 0.07,
  at 0.75 m G + 0.16 [arithmetic from the code].
- PROVEN-CODE, the likeliest look bug: **DigNow rebuilds a hit stored tile without its taper neighbours** (3446:
  `BuildTile` directly, no QueueTaperNeighbours; BlastNow's stored path and the other ground-op path do queue them).
  A neighbour tile whose own vertices did not move is not rebuilt, so its taper never learns about the new bare cells
  (TaperField reads 6 cells across tile sides). Where a trench comes within ~0.9 m of a tile side, the neighbour tile
  keeps full 0.16 m thickness right up to the hole side while the dug tile tapers -> a 16 cm lip standing over the
  trench edge until that neighbour rebuilds for some other reason.
- PROVEN-CODE: **MOVED 0.08 m threshold**: rim cells whose ground moved 0.005-0.08 m get the new ground but keep their
  mud (3420-3432), so the rim keeps a 0.15 m layer on a ground that already slopes into the cut.
- INFERENCE: **SurfaceAt interpolates across the voxel wall**: the sheet samples the voxel top at 0.15 m with a
  bilinear surface over voxel columns, while the voxel mesh draws a steep wall. Rim samples get a ground between the
  rim top and the trench floor; quads between a bared and an unbared vertex with < HOLE_SKIP 0.2 m height range are
  still drawn, as a skin leaning over the wall.
- PROVEN-CODE, transient: live slots marked by DigNow wait for upload picks (as 2.1), so during a 71-op trench near
  the vehicle the live tiles show the old surface over the new hole for up to ~0.5-1 s per change.

### 3.2 Fixes (drawn side = client-only look; sync side must stay the same code on every machine)
| # | fix | where | cost |
|---|---|---|---|
| 1 | DigNow stored path: QueueTaperNeighbours (front of the queue) or build the neighbours in the same frame | MUD_MudSheet.DigNow ~3446 | a few extra tile builds per dig |
| 2 | Dig ops bare at a lower threshold (FIELD_EPS-2 cm instead of MOVED 0.08) plus a 1-cell ring around bared cells | MUD_MudSheet.DigNow (drawn) AND MUD_MudSync StepClear (642) with the same rule, so a tile rebuilt from the sync layer agrees | none; the sync change re-baselines the hash |
| 3 | Build dig / blast live slots in that frame (as 2.3) | Upload / UploadPick | once per dig go-live |
| 4 | Skip any quad with a bared corner whose far corner is more than ~0.05 m above the voxel top there (a lip test instead of HOLE_SKIP 0.2) | BuildTileMesh quad loop | a compare per mixed quad |
"Drop the sheet to the voxel top inside a dig box" is what 2 does in practice (mud 0 where the ground moved).

### 3.3 The 17 ms per op flush on the trench path
MEASURED (WB, so the size is not a perf number): 71 trench ops -> 'op clearing ... 1272 ms', '71 flushes (1187 ms)'.
Each Submit flushes the previous op's open clear job (Flush at the start of MUD_VoxelProbe.Submit), so a burst of
digs puts its whole clear cost on the op-apply frames. It is a one-off per trench, but it lands as up to 71 hitched
frames over a few seconds - not acceptable as is.
Batching that stays deterministic: clear once per trench, not per piece. That needs every machine to agree where the
burst ends - from op content (a trench id + "last piece" in the op), not arrival time. That touches the voxel op
format, which this prompt rules out, so: propose it to the user as a separate change. Cheaper inside MUD_MudSync
only: TopsMoved already limits each clear to columns whose top moved; check that consecutive trench pieces do not
re-clear the same cells (each piece op carries the whole trench box - gotchas / README Stage 4).
The drawn side (19 'dig now' rebuilds, 14-30 ms each) is client-only and can be coalesced freely: rebuild a tile at
most once per ~0.2 s during a dig burst, but build fix 1's neighbours and the bared tiles immediately.

### 3.4 What to log
`MUD dig check: tiles hit N, neighbours rebuilt for taper N (missed N), rim vertices above voxel top + 0.05 m: N
(max M m), live slots dirty and unbuilt N (oldest A ms)` per dig go-live - the existing DIAG in DoDigNow (3279) does
part of this but only with ONE_GRID on.

---

## Summary for the user
- Pop-in: building every tile is impossible (~622 600 tiles, tens of GB, 6-30 min). The cheap route is SPARSE (no
  sheet on untouched ground; patches and the coarse skin drawn 0.15 m higher once) - it was built and worked for cost;
  its open driven / undriven step must be fixed first. Drop the speed radius and the LOD double build.
- Late crater: most likely the live-window upload limits (3 slots per 0.1 s, 0.25 s hold) and the FIFO build queue;
  fix = build the crater's tiles in the blast frame. One timeline log line confirms it.
- Sheet over holes: most likely DigNow not rebuilding taper neighbours (code-proven) plus the 8 cm MOVED threshold;
  fix 1 + 2 above. The 17 ms per op trench flush needs a per-trench clear (op format change - your call).
