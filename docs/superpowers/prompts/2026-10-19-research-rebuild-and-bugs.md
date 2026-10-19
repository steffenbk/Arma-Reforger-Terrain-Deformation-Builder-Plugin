# Prompt for the research AI: rut / rebuild perf + bug hunt (2026-10-19, after DS run 11)

You are doing research and code review only on the Arma Reforger mud mod (repo steffenbk/Terrain-deformation, branch
`displaced-ruts-partial-split`). You can read code and docs. You cannot run Workbench, the game, a dedicated server
(DS) or IDA. Do not change anything under `Scripts/`. Every change you suggest is a proposal the local AI builds and
tests on the DS later.

## Read first (in this order)
1. `CLAUDE.md` - project rules: TESTING RULES (perf only on the DS, never Workbench), FEATURE FREEZE, PvP only (AI
   pathing never matters), TERRAIN RULE, ARMOR RULES, RUTS STAY VOXEL, RUT WEAR.
2. `docs/superpowers/research/mud-gotchas.md` - proven engine facts and Enforce Script limits (64 locals in scope per
   method, params included; no '%' inside a float expression; 'set' / 'debug' reserved; string.Format max 9 values;
   int[] not bool[]; Math.Min returns float). Any code you write must obey these.
3. `docs/superpowers/results/2026-10-19-mesh-vs-voxel-ruts-ds.md` - every DS run of the day, runs 1-11. Run 10 and run
   11 are the baseline for this prompt.
4. `docs/superpowers/plans/2026-10-18-perf-plan.md` - the B items, B7, and the last section "REBUILD 5".
5. `docs/superpowers/plans/2026-10-19-rut-wear-perf-plan.md` - ideas 1-4 (all kept).
6. `docs/features/README.md`, then `docs/features/ruts/README.md` and `docs/features/voxel-ground/README.md`.

## Where things stand (short)
- Ruts are voxel (MUD_VoxelProbe rut field, replicated as ops; J2 = late joiners get a snapshot and must end up with
  bit-identical floors). The heightfield patches (MUD_WorldGround + MUD_FinePatch, 33 x 33 vertices, 8 m) are the base
  ground; the rut floor goes into the patch mesh / collider INPUTS, never into m_aH.
- Client-side look ruts (MUD_LookRuts) draw ruts at the wheel at once; the server floor replaces them later.
- Mud sheet (MUD_MudSheet, MUD_MudSync, MUD_MudZone) is PARKED: `MUD_MudSheet.ENABLED` false. Ignore it unless a bug in
  it can fire while it is off.
- DS run 11 (client, robot 2 min reverse circle on grass): MUD script ~1.7 ms/frame driving. Biggest part = patch
  rebuilds 0.86 ms/frame (rb mesh 0.58, rb collider 0.30): ~110 band mesh builds per 10 s at ~4 ms each, ~35
  colliders per 10 s at ~5 ms. Then ahead 0.27 (a view 0.19), vox 0.10, look ruts 0.09. Server: ~2.3 ms/frame at 30 fps.
- Mesh build split per build: input ~0.6-0.8 ms, mesher core ~2.0, reveal + UV ~0.7, MeshObject create + upload ~0.5.
  Collider: input ~0.8, mesher ~2.2, trimeshes ~2.0.
- Engine facts that limit the options (IDA, see gotchas): a flags-0 MeshObject uploads to the GPU once - in-place
  UpdateVerts never shows, so every visual change needs a new MeshObject; there is no script heightfield collider;
  script meshes have no vertex colour; engine work is only 6-11 % of a mesh build (the rest is our script).
- REBUILD 5 result: kept COL_MERGE (collider 2 x 2 merge, -25 % tris) and MESHER_FAST (mesher on locals, bench -9 %).
  Turned OFF with no measurable win: RUT_PACE (slower rut rebuild pace only delays builds), SAME_SKIP (~60 skips / 10 s,
  but each still paid the input pass), PARTIAL_IN (box-only input pass: ~0.55 vs ~0.7 ms - fixed costs dominate). The
  code is still there behind those switches.

## Tasks
1. **Why so many mesh rebuilds?** ~110 per 10 s while one truck circles. Trace every path that sets
   `m_bMeshDirty` / `m_bLookDirty` (MUD_WorldGround: MarkDirty, MarkFloorDirty, MarkLookDirty, the direct setters,
   hideOwed) and every caller (MUD_VoxelProbe floor ops / FloorDirty, MUD_LookRuts LookDirty + sweep, FloorFinish).
   For one wheel pass over one patch, count how many rebuilds it causes (look cut, server floor arrival, ridge /
   relax, look sample replace / expire, seam neighbours). Table: source -> rebuilds per pass -> why. Then propose ways
   to cut the COUNT (merge marks, skip a rebuild whose change is fully covered by the look floor, etc.). Name for each
   which machine it saves on (client / server) and the risk (seams, holes, late ruts - see the gotchas about holes at
   the wheels and see-through gaps).
2. **Cheaper per build**: read `MUD_BandMesher.c` (Build / Verts / Quads / Skirts, and BuildRef = old code) and the
   build functions in MUD_WorldGround (BuildMeshBand, BandMeshIn, the reveal + UV loop, BuildColliderBand + trimesh
   pieces). Find work done per build that could be cached per patch (UVs depend only on position; the skin layout;
   index lists when the cell kinds did not change; the plate). Estimate ms saved per build from the split numbers above.
   Mark every code sketch "unreviewed plan code".
3. **Bug hunt** in the rut / ground path: MUD_WorldGround.c, MUD_FinePatch.c, MUD_BandMesher.c, MUD_VoxelProbe.c (rut
   field, CarveFloor, wear, J2 snapshot TileText / RunText / parse, op hash), MUD_LookRuts.c, MUD_GroundFollow.c.
   Look for: state that can go stale (flags never cleared, boxes not reset, caches not invalidated), MP divergence
   (anything on an op path that reads machine-local state - J2 rule in mud-gotchas: ops must stay floor-blind),
   off-by-one at patch / tile / skin-cell borders, int division and float rounding, null access after a patch is
   freed or asleep, arrays read past their size, and Enforce limits (count locals in the biggest methods - flag any
   method near 64). For each finding: file + line, a concrete failure scenario, severity, and a fix sketch. Separate
   CONFIRMED (you traced it end to end) from PLAUSIBLE.
4. **Review REBUILD 5's kept code**: MergeMask / EmitMerged (collider merge - can a merged quad leave a hole or a
   step a wheel can catch, T-junctions at merged block edges, slots / rut mud per skin cell) and the new Verts / Quads
   (does it match BuildRef in every path, overflow handling).
5. **Ahead 'a view' 0.19 ms/frame and look ruts 0.09**: smaller, but say if there is an easy cut.
6. **Write the local test prompt**: `docs/superpowers/prompts/2026-10-19-local-rebuild-followup.md` for the local AI
   that builds your top 3-5 items. One DS session (TESTING RULES), each item behind its own switch with an A/B log
   line, the robot route (hold Space 3 s in the driver seat = 2 min reverse circle), what numbers decide keep / drop,
   and a numbered check list for the user. Ask the user before anything that needs two launches.

## Rules
- Label every claim: [repo] (file + line, or log line from a results doc), [IDA] (from gotchas / results), [doc],
  [code] (API dump / public code), [inf] (yours). Never cite a GUID, file name, method or line from memory - grep it.
- Per-item tables go into the doc in full, with a checksum line where you bucket counts.
- Rank ideas by expected ms/frame saved on the client while driving, and say how sure you are.
- Short plain sentences; no emojis.
- Output: `docs/superpowers/research/2026-10-19-rebuild-and-bugs-research.md` (sections = tasks 1-5) plus the local
  prompt (task 6). Commit both to this branch and push. No PR. Do not edit Scripts/.
