# Rut cells - build + test plan (2026-10-16)

Where it sits in the overall plan (2026-10-16): plans/2026-10-16-next-steps.md - Stage 0 = session 0-WB #5 +
0-PERF; Stages 2-6 = Build 1 if 0-PERF shows the rut cost is still too high after FLOOR_SETTLE.

User picked option A (research/2026-10-15-rut-cells-deep-dive.md, design A1). Ruts get their own small tiles: a 2 m
skin cell the rut floor reaches becomes a RUT cell, the patch skips it (as it skips VOX cells) and a light "rut layer"
per patch draws it from base ground + rut floor. A rut op then rebuilds a few small tiles, not whole 8 m patches.
Switch: `MUD_WorldGroundEntity.RUT_CELLS` (const, false = today's rut field exactly). Needs RUT_FIELD.
Labels: CODE (branch displaced-ruts-partial-split), MEASURED, INFERENCE.

Paste to the local Claude:
> Read CLAUDE.md, docs/superpowers/research/mud-gotchas.md, docs/features/ruts/README.md,
> docs/superpowers/research/2026-10-15-rut-cells-deep-dive.md and this plan
> (docs/superpowers/plans/2026-10-16-rut-cells-plan.md). Map every name here to the real code and tell me where it
> differs before coding. Checkpoint before each code stage. Stop after each stage so I can compile and test.
> Enforce traps: no variable name declared twice in one function, no ternary, never name a variable "set",
> static arrays <= 65536, class <= 256 KB. If the code disagrees with the plan, stop and ask.

## Facts it rests on (CODE / MEASURED)
- Patch cells: MUD_FinePatch.m_aCell[16] = slot | TLBR 16 | VOX 32 | LOOK 64. VOX cells are skipped by the patch
  mesh + collider (grid BuildMesh / BuildCollider `cell & VOX`, band FillBandOn slot -1). A flip (DoVoxCells) rebuilds
  the touched patches' collider + mesh in that frame.
- Floor: MUD_RutTile per 4 m (16 x 16 at 0.25 m), FloorSample / FieldFloor / FieldFloorAt / FieldInBox, written by
  CarveFloor, deterministic on every machine (disp hash matched 517 / 645 ops).
- Patch base heights m_aBase = skin triangles (+1 mm), edge lock m_aLock; the patch applies the floor as
  min(m_aH, (floor - base) * lock) (RutFieldOn paths).
- Grid path per 8 m patch: mesh 0.86-1.10 ms, collider 1.37-1.58 ms; band 2.9-5.5 ms (results/2026-10-13-sdf-phase2.md);
  today's rut field rebuilds ~6-7 ms each, ~10 + 10 per second per vehicle (ruts README).

## Stage 0 - grid mesher A/B (no code; also the baseline for everything below)
- `m_bBandMesher 0` vs 1 in Prefabs/Mud/MudWorldGround.et, same rutted drive (+ a crater, a trench, a road edge).
- First check what the band mesher still does that the grid does not (results/2026-10-13-sdf-phase1.md / phase2,
  band comments): skin-seam skirts, kept-terrain cells, partial cells. Report before the test.
- Read: 'MUD band:' mesh / collider ms + the split lines + 'grid path' lines, fps, 'MUD patches' line.
  Look for: cracks at patch / skin seams, VOX borders, road edges, the groove look, get in / out.
- If clean: keep 0 (rut cells build their patch flips on the grid path). Results to
  docs/superpowers/results/2026-10-16-grid-mesher-ab.md.

## Stage 1 - offline prototype (Python, Tools/rut_cells_proto.py, reuse displace_proto / dc_vertex_proto helpers)
- Synthetic 8 m patch: base ground (top_of), rut floor from the game's CarveFloor profile (fall = 1 - k^2, step / cap
  as in game), 2-3 crossing / curving tracks + one along a cell line.
- Build: patch grid at 0.25 m skipping RUT cells; rut layer tiles at 0.25 m AND at 0.125 m.
- RUT cell rule: every 2 m cell whose floor support (+1 floor sample) touches it, + 1 ring. Measure how many cells per
  metre of track and how often the ring is needed.
- Checks: cracks on every RUT / normal border (open edges, max gap), T-junction count at 0.125 m, tri counts per tile,
  surface error vs the true floor (as displace-partial.md), wall angles at 0.25 vs 0.125.
- Pass: 0 cracks; report counts. Results to docs/superpowers/results/2026-10-16-rut-cells-proto.md.

## Stage 2 - data + flips (no rut layer drawing yet)
- New patch cell bit RUT (128). A cell is RUT when the Stage 1 rule says so (from the floor tiles - deterministic).
- The patch mesh AND collider skip RUT cells (as VOX). Nothing draws them yet -> with RUT_CELLS true this stage
  shows holes on purpose; test only that the flips happen where expected (debug draw: RUT cells as wireframe boxes,
  diag menu next to the Displaced ruts switches) and that RUT_CELLS false is unchanged.
- Flip path as DoVoxCells (patch collider + mesh rebuilt in the flip frame), on the grid path from Stage 0.
- VOX wins over RUT (a crater over a rut: VOX bit, bricks draw it with displaced ruts as today).

## Stage 3 - the rut layer
- One entity per patch that has RUT cells (new class MUD_RutLayerEntity): one MeshObject (top = the patch's top
  material, groove = soil section as the patch's FIELD_SOIL_D rule) + one static trimesh body on the patch's layer
  (Terrain | Foliage) and game material.
- Per RUT cell a grid at RUT_TILE_STEP (const 0.25 or 0.125, Stage 1 decides): y = base + min(0, (floor - base) * lock),
  base and lock exactly as the patch computes them at the shared border vertices (same function, same inputs).
- Rebuild on a rut op: FloorDirty marks the rut LAYERS of the touched patches (not the patches); the patch itself only
  rebuilds when a cell flips. Budget like patch rebuilds (one per frame, interval), oldest first.
- New entity class in every list our ground entities are in: HideFromTraces / SetTraceHidden (get in / out - the v093
  lesson), MUD_Surface.IsOurs, GroundCovers, MUD_BuildingDestructionFix (grep where MUD_VoxelSubEntity /
  MUD_FinePatchEntity are listed). Hole check (review F3) must accept it.
- Server: colliders only (headless, no mesh) - as patches.
- Sleep / free with its patch (patch sleeps -> layer body drops; patch freed -> layer freed; rebuilt from the floor
  tiles with the patch).

## Stage 4 - fewer first-pass flips
- A cell may flip to RUT before any floor exists (the tile then equals the patch surface - no visible / physical
  change). When Ahead builds or wakes a patch for a moving vehicle, pre-flip the cells along each wheel's projected
  path (e.g. 2-3 s ahead, the cells within RUT_R + 1 sample of the path) in ONE patch rebuild, only on soft ground
  (CarveFloor's hard / kept rules).
- Measure: patch rebuilds per second on a fresh-ground drive, before vs after.

## Stage 5 - SP test (checkpoint first)
1. Look: ruts as sharp as today or sharper (0.125 m), soil groove, no cracks at RUT borders (hole check on, drive
   along and across cell lines, end a rut just short of a cell line).
2. Seams: RUT next to VOX (a rut into a crater and out), RUT next to a road (kept terrain edge).
3. Cost: rut layer builds x ms, patch flip rebuilds x ms, total ms/s vs Stage 0 (grid) and vs today's rut field.
   One vehicle, then the 3-5 vehicle stress run.
4. Feel: Diag Vehicles > Show raycast / contacts over a 0.125 m rut (one centre ray, raw triangle normal - IDA item 5).
5. Get in / out of a truck parked in a rut.

## Stage 6 - MP gate
- Robot test >= 6 runs per setting (RUT_CELLS false vs true) - corrections not worse.
- Late join: disp hash lines match, the joiner's RUT cell count = the server's, join time.
- Stress run once more on a DS + 1-2 clients.

## Added after IDA answers 5b + 6-10 (2026-10-16, research/2026-10-15-ida-rut-questions.md)
- **Grass in ruts (2-bis):** rut depth alone never removes grass - grass is planted at HEIGHTMAP height on the GPU,
  and the clutter discontinuity mask only reads meshes with ClutterOccluder on (our runtime entities have none). So a
  0.1-0.35 m rut can show grass standing at the old ground height. CHECK in Stage 5 (look along a rut at low angle).
  Rut cells give a clean fix: spawn the rut layer entity from a PREFAB whose MeshObject component has ClutterOccluder
  on (answer 2: INFERENCE the flag survives SetObject) -> grass cut only in RUT cells, not under whole bricks. Probe it
  in Stage 3. Fallback: AffectFoliage flattening along recent tracks (answer 3).
- **Mud friction in ruts (6, 9):** a trimesh has ONE material per geom (no per-triangle material), and the vehicle sim
  never reads surface RollingResistance / RollingDrag. Friction, roughness and steering noise DO come from the hit
  geom's surface. The rut layer is its own geom -> give it a "rut / mud" game material (lower tread friction, some
  roughness): slipping in ruts for free, on every machine (server collider). Drag still needs script
  (WheelSetRollingDrag, weak) - out of scope here.
- **Partial updates (8):** AddGeom works on a live body but there is NO RemoveGeom / replace, so "one body, swap the
  touched sub-geom" is impossible. Rebuilding a smaller piece = a separate body per piece. Per-patch rut layer (one
  body) stays the default; per-cell bodies only if the Stage 5 split lines show the layer body rebuild dominates.
- **Internal edges (7):** no internal-edge smoothing on our trimeshes. More, smaller triangles (0.125 m tiles) = more
  edge bumps for vehicle bellies / debris (wheels use rays, not affected). Prefer 0.25 m unless 0.125 m clearly looks
  better in Stage 1 / 5.
- **16-bit indices (6 side note):** one script trimesh must stay under 65536 vertices - a rut layer is far below.

## Later
- A2 (rut tiles independent of patches: the coarse skin skips RUT cells too) - fixes ruts for players on foot and the
  floor-patch memory without the patch-memory prompt's camera-range rule. Decide after Stage 6.
- IDA item 8 (partial collider updates) could let the rut layer keep one body and swap only the changed cell's geom.

## Rollback
RUT_CELLS false = today's rut field. Checkpoint before Stage 2.
