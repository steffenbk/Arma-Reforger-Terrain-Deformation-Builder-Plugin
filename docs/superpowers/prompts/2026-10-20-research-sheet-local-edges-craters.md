# Research prompt: sheet local - the raised tile-edge wall + the crater delay on mud ruts (2026-10-20)

You are the research AI for the Arma Reforger mud mod (Enfusion script, addon "Testing plan vech msh"). Research
and propose; do not write the build. Label every claim PROVEN-CODE (file:line), MEASURED (log), [repo] or INFERENCE.

Read first:
- docs/superpowers/results/2026-10-20-mud-lab-stage1-ds.md (the first DS run: every 10 s lab row, findings)
- docs/superpowers/results/2026-10-20-mud-lab-stage1-build.md (what parts A-E built, deviations, risks)
- docs/superpowers/prompts/2026-10-20-local-mud-lab-stage1.md (the stage 1 build prompt and its hard rules)
- docs/superpowers/research/2026-10-20-mud-sheet-deep-dive.md (your deep dive: band, levers, far data)
- docs/features/mud-sheet/README.md, docs/superpowers/research/mud-gotchas.md
- Code: Scripts/Game/Mud/MUD_MudSheet.c (sheet local = m_bLocal), MUD_MudSync.c (RutOp / ClearOp gates),
  MUD_VoxelProbe.c (GoLive ~6727 -> MUD_MudSheet.GroundChanged + FlushNow ~6750, TakeOp call ~4083)

Hard limits: the voxel ruts stay as they are (RUTS STAY VOXEL, RUT WEAR v234); rut ops stay floor-blind (J2); sheet
local is client only, nothing on a DS, nothing writes J2 / ops / snapshots; defaults off, one static switch per
change; no custom shaders; a script mesh change = a new MeshObject (in-place UpdateVerts never shows); 64 locals per
method, 80 KB stack, 256 KB per class (MUD_MudSheetField is at 236 KB); perf only on a DS / standalone; PvP only.

## Problem 1: "still quite obvious where the mesh is being raised"

The user likes the cost ("didn't kill my performance") but sees a striped vertical wall beside the Ural's rut. It runs
in straight lines along tile borders (3 m tiles, TQ 20 cells of 0.15 m) with a square corner; the sheet top is above
the grass on one side, nothing on the other.

What we know:
- PROVEN-CODE: the stripes are the ONE_SKIRT curtain (0.45 m, SKIRT_ALL true): every border edge whose two verts a
  drawn quad uses hangs a down-facing quad, UV copied from the top vert, so the texture streaks (BuildTileMesh, the
  full skirt after the quad loop and the low-build skirt). It shows only when the tile beside is lower or not drawn.
- PROVEN-CODE: sheet local has no EDGE fade (HasTile returns true in local); the height is
  G - BAND_UNDER + (BAND_UNDER + LIFT + D) * band. Quads whose 4 corner bands are 0 are skipped (under the ground).
  The design relies on the band reaching 0 inside born tiles: BandMarkWorld births every tile within BandMargin of a
  stamp's touched box.
- MEASURED (DS client): every 10 s row has 1-5 born live slots with no mesh, also while 'born slots waited for
  traces' was 0. Expected for slots fully outside the band - not proven to be the wall.
- NOT PROVEN: which case makes the wall. Candidates: (a) tile beside not born; (b) born, no mesh (live slot never
  built / hidden, stored data-only, Freeze made it data-only because a cell was untraced); (c) its border verts
  untraced (LAZY: untraced quads are not drawn); (d) its band at the shared border is stale / 0 (a store's own copy of
  the shared side cells vs the window field; FillSlot / Unpack overwriting shared cells; BandStored re-band without a
  rebuild); (e) the drawn neighbour was built before the band grew and is waiting in UploadPick / m_aOneBuild.
- Built, not yet run: EDGE AUDIT meter (EDGE_AUDIT, EdgeRecord / EdgeDrawn / EdgeAudit) - the 10 s 'MUD sheet
  builds:' line will say per raised border vert: ok / step > 3 cm / open (beside not born / no mesh / vert untraced /
  band 0). The next WB run gives these numbers; use them if they are in the repo when you start.

Questions:
1. From the code, which of (a)-(e) can happen in steady state (truck stopped, camera near), and which only for a
   frame or two? Trace the shared-side data for live-live, live-stored and stored-stored neighbours (Follow, Unpack,
   FillSlot, Freeze / FreezeLocal, BandTile, BandStored, QueueBuild / FlushOneBuild, QueueNear).
2. Fixes that make the edge invisible whatever the cause, ranked by look and cost. At least:
   - skirt only where the tile beside is drawn (or a band-0 tile edge never raised): no curtain at an open side;
   - an EDGE-style fade in local mode toward a side whose neighbour has no drawn mesh (and what must rebuild when
     that neighbour appears);
   - one owner per shared border vert (band, D, G from one place) so both tiles always agree;
   - a tile beside a raised border is always built (born + mesh) before or with the raised one;
   - skirt UVs that do not streak (bottom verts get a UV offset down the slope) as a cosmetic fallback.
   For each: frames of wrong look, extra builds per 10 s, memory, MP risk (none allowed), what it cannot fix.

## Problem 2: the small delay before a crater on mud ruts shows in the mud

User report (not measured yet): a crater blown on the ruts where the sheet is shows its hole in the mud a little late.

What we know:
- PROVEN-CODE: a crater reaches the sheet only at the voxel go-live: MUD_VoxelProbe.GoLive -> GroundChanged per
  change -> FlushNow -> MUD_MudSheet.DoDigNow (DIG NOW). In sheet local GroundChanged sets m_bMarkNow, so DigNow + BuildNowSlots rebuild holed
  LIVE slots in the go-live frame; stored tiles are built next Frame(s) via FlushOneBuild. BLAST_NOW (mud off in the
  go-live frame, no traces) and RIM_BARE were the 2026-10-15 fixes for the old ~0.3 s fade; JOB_DELAY 0.1 s before the
  ground re-read. In sheet local MUD_MudSync.ClearOp is only counted (MUD_MudSheet.OpClear) - the sheet takes craters
  at go-live, not at the op.
- Unknown: how long from the shot to the voxel go-live, how long from go-live to the sheet hole on screen, and whether
  the delay is the voxel side, the sheet's stored-tile path, the ground re-read, or the band / skirt around the hole.

Questions:
3. List every step from the projectile hit to the sheet hole on screen, with the frame / time each can add (code
   refs). Which of them does sheet local add on top of the old synced sheet?
4. How to show the hole in the mud in the same frame as the voxel crater (or earlier, from the op, client side only),
   without a second clear and without touching J2? Compare: build stored tiles in the go-live frame too (cost cap),
   a look-only hole at the op (hide the mud quads in the blast radius at once, real data at go-live), pre-building
   the mesh at the op and swapping it at go-live, others you find.
5. What to log to measure it (one 10 s line part or a per-crater line: shot time, op time, go-live time, sheet hole
   build time, tiles live / stored).

## Report back

Short answers to 1 and 3 (each point labelled). Then one ranked table per problem: option, how it works, frames of
wrong look, cost per frame / per event, memory, MP risk, what to measure first. End with one recommended fix per
problem and the smallest test session (one Workbench run for the look, one DS run for cost, a numbered checklist)
that would prove or kill it.
