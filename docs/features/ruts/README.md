# Ruts (voxel ruts, v035)

**Status: SHIPPED.** Short page - add to it when we work on this feature.

## Files (Scripts/Game/Mud)
MUD_VoxelProbe.c (RutWheel / RutFlush, RUT_BEHIND), MUD_Wheels.c

## Docs
- [2026-10-15-raised-ruts-ideas.md](../../superpowers/research/2026-10-15-raised-ruts-ideas.md)
- [2026-10-15-rut-collision-options.md](../../superpowers/research/2026-10-15-rut-collision-options.md)

## Displaced ruts (DISPLACE_RUTS true, shipped 2026-10-15)
- DISPLACED RUTS (plan on PR branch displaced-ruts-partial-split, docs/superpowers/plans/2026-10-15-displaced-ruts-plan.md):
  Stage A code in 2026-10-15 behind MUD_VoxelProbe.DISPLACE_RUTS (false) - CarveFloor, ClearFloor, FloorSample /
  FloorMin / UndugAt / FieldSurfAt, Displace (+ DispFloor / DispMid / DispTri / DispEmit), ColSurf + ColumnDepth min.
  Stage A passed (v087), Stage B debug tools passed (v089): diag menu GameCode > Mud > Displaced ruts (summary
  log, draw floor, draw overflow/fold bricks, hole check, floor hash). DISPLACE_RUTS true since Stage C.
  Step 4 SP PASSED 2026-10-15 (v094): walls sharper than v035, no brick-line cracks (user); log holes 0, 0 overflow,
  0 slow brick, displace 6-15 ms/s. Fixes on the way: Displace compares brick-LOCAL y (m_Vb is local to m_vBOrg);
  CarveFloor edge fall = 1 - k*k (0 at the rim); voxel bricks join HideFromTraces (get in / out in a rut, v093).
  Step 5 MP (user accepted 2026-10-15, v095): robot 2 runs per setting, client snaps true 6 (one burst) vs
  false 1 - too few runs to judge (docs/superpowers/results/2026-10-15-disp-step5-robot.md). JIP + F2 stress
  SKIPPED by the user (untested: does a late joiner get the rut floor?). Robot code re-archived.
  Soil rut walls (user pick 2026-10-15, v096 = before; user OK 21:48 'looks good' -> v097, log ~570 soil tris per 4-brick batch): DispEmit sends a top tri the pull made
  steeper than Quad's grass rule (n.y <= 0.7 |n|) OR more than DISP_SOIL_CUT 0.05 m under the seeded ground
  (run 1: steep alone = 2-3 tris per batch, a few patches) to soil, on soil-UV copies of its corners (DispSoilV); count
  'soil walls' in the 'MUD voxel probe' batch line.

## Rut field (RUT_FIELD true, stages 1-5 robot PASSED 2026-10-15; JIP test open)
- Plan: docs/superpowers/plans/2026-10-15-rut-field-plan.md (branch displaced-ruts-partial-split). Checkpoint v098 = before.
  Goal: ruts stop converting voxel columns; one sparse floor field drawn by heightfield patches AND voxel bricks.
- Stage 1 PASSED (user 2026-10-15: compiles, drives like v097): floor moved out of MUD_VoxelColumn into its own tile
  map (MUD_RutTile, m_mRutTiles, ColKey, made at the first write - RutTileMake). RUT_FIELD false = v097.
- Stage 2 PASSED (log 2026-10-15 21-35-43: 107 rut ops, every one '0 new columns', floor tiles 28, columns flat 30):
  RUT_FIELD true; Issue / Submit convert nothing for op 4; CarveFloor plain rule (no column or FieldSurfAt fails) =
  min(UndugY, floor), hard = Kept || Hard; never MarkCarved. JIP gate: no code (probe starts only after m_bReady).
- Stage 3 PASSED (user 2026-10-15: width + asphalt edge right): patches draw the floor - FloorDirty / MarkFloorDirty
  (no m_aH, no net), mesh + collider inputs (band and grid), Heights dug = min floor, FinePatch.m_bFloor (not freed,
  may sleep, not lazy). Fixes: soil only from FIELD_SOIL_D 0.03 deep (reveal's 0.01 made ruts look ~2x wide);
  no-column hard rule needs Kept too (Hard alone cut ruts onto asphalt / concrete).
- Stage 4 PASSED (user 2026-10-15 22:48, v100; DS run fps fine, get in/out OK; log 22:47 session: 86 rut ops all
  '0 new columns', soil walls up to 733 per batch). Cost ~10 mesh + 10 collider patch rebuilds/s at 6-7 ms each.
  Fixes: (a) see-through V gap at a road / path edge - floor depth * p.m_aLock[v] (edge lock, as presses) in band +
  grid mesh, both colliders and Heights (LockAt); (b) crater rim grass over patch soil - DispEmit also soil where
  the floor pulled a top tri > FIELD_SOIL_D under the voxel surface (RUT_FIELD only).
- Stage 5 robot PASSED (2026-10-15 22:50-23:00, docs/superpowers/results/2026-10-15-rut-field-stage5-robot.md):
  true 7 snaps / 7 runs vs false 40 / 8; disp hash identical on both machines for every op (517 + 645). JIP part
  still to do (late join after ruts + crater; joiner columns, hashes, op count).
- Decided with the user before coding (gaps in the plan): floor-only patches get m_bFloor (not freed, may sleep, not
  JIP / hash); grid fallback mesher draws the floor too; no column -> Hard(x, z) and UndugY (not UndugAt / m_fLastG);
  FieldSurfAt fails -> plain rule; RUT_FIELD never marks new carved cells; JIP clients must wait for the ground to be
  ready before applying ops; m_aShow stored before the floor min.

- Review fixes 2026-10-16 (docs/superpowers/results/2026-10-15-review-baseline.md): F1 floor settle
  (MUD_WorldGroundEntity.FLOOR_SETTLE true, v109 = before) SP PASSED on cost: rebuild ms per rut op 9-15 vs 18-34
  before (~1/3), same per-build ms; MP not tested. F3 hole check plain + seam points and draw floor plain colours
  (v108 = before): 0 holes in 1577 plain checks; seam path not exercised yet (no crater crossed).
  Patch memory (built 2026-10-16, not tested): press arrays only on first write (v110 = before); FLOOR_FREE frees
  floor-only patches out of range, FieldDue rebuilds them near camera / player / vehicle, FloorDirty makes no new
  patches (v111 = before).

## Open
- Rut field Stage 5 JIP (user: later): late join after a rutted drive + crater crossing a rut. Robot code is
  re-archived: for disp hash on a DS set DISP_HASH_ALWAYS true; heightfield hashes need Hash Every > 0.
- Options after the freeze (displaced ruts, terrain-look material probes, DC for craters): docs/superpowers/research/2026-10-15-voxel-options-deep-dive.md
- Parked idea first in line: raised side ridges on voxel ruts (master list).
