# Ground architecture review - voxel + mud sheet (2026-10-15)

Independent review asked by the user ("not going the right direction"). No code was changed.
Labels: PROVEN (doc / file / line cited), MEASURED (numbers cited), CODE (read in our script today),
USER (user said / saw it), INFERENCE (my reasoning, not tested).

## 1. Verdict in short

Yes, the two-layer setup fights itself. Not because of bugs, but by design:
two different meshes, built from two different grids, have to look like one ground. Every edit edge needs a
stitch rule, and every stitch rule moves the seam somewhere else. This is the THIRD time the project has
hit the same wall (heightfield vs voxel on 2026-10-13, look field vs feel field on 2026-10-13/14, voxel vs
sheet now). The fix is not another stitch. It is: **one visible surface per spot, built from one grid.
Extra layers live as extra numbers on that grid, not as extra meshes.**

## 2. What the evidence says

### 2.1 The sheet sync IS the mortar stutter (MEASURED)
Log `ArmaReforgerWorkbench/logs/logs_2026-10-15_11-54-08/console.log`, lines 21152-21250 (mortars in the zone):

| seq | sheet "blast now" ms | sheet "dig now" ms | voxel go-live frame ms | go-live minus sheet |
|---|---|---|---|---|
| 1 | 2 | 21 | 26 | 3 |
| 2 | 4 | 48 | 55 | 3 |
| 3 | 2 | 33 | 41 | 6 |
| 4 (0 sheet tiles) | 0 | 0 | 3 | 3 |
| 5 | 3 | 39 | 48 | 6 |
| 6 | 3 | 45 | 52 | 4 |
| 7 | 4 | 39 | 45 | 2 |

Seq 4 is the control: no mud tiles in reach -> go-live frame 3 ms. With mud tiles the frame is 41-55 ms.
So the voxel go-live itself costs ~2-6 ms; the mirroring into the sheet costs 21-52 ms in the same frame.
(The hook: CODE MUD_VoxelProbe.c 3154-3155 calls MudSheet.GroundChanged + FlushNow inside go-live.)

### 2.2 The two layers can never match exactly (PROVEN by code, INFERENCE for the look)
- The voxel mesh is surface nets on 0.5 m samples (CODE MUD_VoxelProbe.c 241-242 SUB 8, CELL 0.5). Its vertex
  sits at the mean of edge crossings inside each cell.
- The sheet reads the SAME field differently: bilinear over 4 sample columns, "first solid from above"
  (CODE MUD_VoxelProbe.c 3660-3700 SurfaceAt / ColSurf), at 0.15 m (CODE MUD_MudSheet.c 34).
- Two rebuilds of one field by two methods give two different surfaces. MEASURED gap of the voxel mesh vs the
  true shape: rms 0.03-0.04 m, max 0.12-0.29 m on ruts / craters (docs/superpowers/results/2026-10-14-dc-proto.md table rows
  "mean"). The sheet follows the true shape more closely, so the gap between the two layers is of that size.
  That is the "height step" and "floating tile" class of bug. No stitch rule removes it; it only hides it.
- TAPER is counted inside one tile only (CODE MUD_MudSheet.c comment above line 106: "a bare cell just over a
  tile border still gives a step there"). HOLE_SKIP drops whole 0.15 m quads (line 121). INFERENCE: these two
  are why the latest screenshot shows straight, tile-edge-shaped cuts into the crater edge.

### 2.3 The stitch-rule count keeps growing (CODE)
In one day MUD_MudSheet.c got BLAST_NOW (96), RIM_BARE (101), TAPER (106), DIG_NOW (111), TRACE_REDO (117),
FIELD_EPS (118), HOLE_SKIP (121), plus voxel SOIL_TILE 8 -> 12 (MUD_VoxelProbe.c 305) and the voxel "no grass
under the sheet" rule (MUD_VoxelProbe.c 3841-3843). The user report (each fix exposed the next seam) matches.

### 2.4 Look and feel are split by design (PROVEN)
- The wheel ray (VehicleCast) hits only Default, Static, Terrain (mud-gotchas line 118-120).
- The sheet collider is CharCollide | Foliage (CODE MUD_MudSheet.c 88), so wheels never feel it.
- In MP the vehicle runs on the server; client-only shapes can't push wheels without snaps
  (2026-10-15-raised-ruts-ideas.md "MP rules").
- So "sheet ruts look good but can't be felt" is not a bug to fix. It is what a client-only layer is.

### 2.5 Same pattern before
- 2026-10-13: heightfield + voxel, "every 2 m skin cell must belong to ONE of them"
  (results/2026-10-13-voxel-vs-heightfield-problem.md section 3). Ruts vanished at every hand-over.
- 2026-10-13/14: RUT_LOOK look field + feel field: MEASURED build 41.6 vs 20.0 ms/s, 3 corrections vs 0
  (mud-gotchas line 373-377), dirt "pops in" (USER).

## 3. Which problems are inherent vs fixable

| Problem | Inherent to two layers? | Why |
|---|---|---|
| Height step / floating tile at crater and dig edges | Inherent | Two rebuilds of one field (2.2). Can be made smaller, never zero. |
| Jagged straight cut at crater edge | Fixable (but part of the same family) | TAPER across tile borders + smoother HOLE_SKIP would help. |
| Mortar stutter in zones | Mostly inherent | Every op is done twice: voxel mesh + sheet re-sample (2.1). Can be spread over frames, then the layers disagree for those frames ("loading away" again). |
| Sheet ruts not felt | Inherent | Client-only (2.4). |
| Voxel ruts coarse | Inherent to 0.5 m voxels | DC can't fix it (docs/superpowers/results/2026-10-14-dc-proto.md "Ruts: NO-GO"); 0.25 m costs 3.4x (docs/superpowers/results/2026-10-14-cell-size.md, 196.6 vs 57.6 ms/s). |
| Texture grain mismatch | Fixed | SOIL_TILE 12 = sheet SIZE (CODE). |
| Tiles "loading away" | Fixed by DIG_NOW, at the price of the stutter | Trade-off between 3rd and 4th row. |

## 4. Engine limits every option must live with (PROVEN)
- Script mesh: one UV set, UVs clamped 0..1, no vertex colours, no own normals; normals rebuilt per section
  (2026-10-14-ida-voxel-feasibility.md sections 1-2). So no displacement maps, no per-vertex blend.
- Each mesh section needs its own .emat path, or crash (mud-gotchas line 416).
- No collider refit; collider patching costs the same as a rebuild (docs/superpowers/results/2026-10-14-engine-probes.md G3).
  CreateTriMesh 512 tris 0.06 ms, 2048 tris 0.28 ms; script loops dominate (mud-gotchas line 35-37).
- DYNAMIC_VERTEXES gives broken normals: rebuild meshes, don't update in place (mud-gotchas line 434-437).
- The real terrain heightmap can't be changed at runtime (mud-gotchas line 26).
- No triplanar / world-space material is shipped (docs/superpowers/results/2026-10-14-material-search.md).
- No custom shaders for mods: INFERENCE (nothing in the KB or docs shows a way; materials are fixed classes).
- MudRunner has no mud collider; wheels sit on hard ground, mud adds forces (2026-10-14-mudrunner-ida.md).

## 5. Options

Cost words: "cheap / medium / heavy" are INFERENCE unless a number is cited.

### (a) One voxel surface, finer detail baked into the voxel mesh
- Look: ruts stay 0.5 m soft unless cells get finer. A "displacement pass" on the voxel mesh is not possible:
  no displacement / normal input on script meshes (PROVEN, section 4). The only way to add detail is more
  vertices = finer cells (that is option b).
- Feel: solid, as today (PROVEN v035, 0 corrections / 90 s, mud-gotchas line 358-364).
- MP / JIP: as today.
- CPU: as today at 0.5 m.
- Verdict: clean (one surface) but stays coarse. Good fallback, not the goal.

### (b) Adaptive / two-resolution voxels (fine only where ruts are)
- Look: sharp ruts at 0.25 m (MEASURED rms -41..-47 %, walls 44-45 deg vs 28-35, cell-size.md).
- Cost: fine bricks MEASURED 3.4x the 0.5 m cost (cell-size.md same-brick A/B). And ruts are where most bricks
  are: rut ops were 96 % of the JIP ops (docs/superpowers/results/2026-10-14-jip-size.md). So "fine only at ruts" saves little.
- Seams between 0.5 and 0.25 bricks need transition cells (Transvoxel-style) or they crack
  (cell-size.md "two resolutions side by side"). Big, risky work in Enforce.
- Verdict: the most work for the least gain. Don't.

### (c) Heightfield for every single-valued spot, voxels only for overhangs (tunnels)
- Idea: ruts, craters, trenches, shovel, fill all write into ONE fine height grid (0.15-0.25 m). Voxels are
  used only where a column really has air under solid (a tunnel). Each spot is drawn by exactly one of them.
- Look: 0.25 m ruts with sharp edges and ridges, the sheet's quality (the sheet IS a 0.15 m heightfield).
  Crater and trench walls: a heightfield stretches texture on steep walls (USER report on the sheet: "UV
  stretch ... steep sides", master list Lane 1 #5). Fix: per-quad UV plane pick, as the voxel mesher already
  does (CODE MUD_VoxelProbe.c ~3827-3850). No undercut craters - not wanted anyway (FUNNEL, CODE line 261).
- Feel: the collider is the same grid -> what you see is what you feel. Rut walls and ridges are felt.
- MP / JIP: keep today's op stream (kind 0-4) and replay it into the grid on every machine, like the voxel ops.
  Deterministic replay of rut cuts already works for voxels (v035, PROVEN). No new net data.
- CPU: MEASURED warning: the OLD heightfield rut path cost ~13 fps vs voxel ruts (64.1 vs 48.4 fps,
  docs/superpowers/results/2026-10-15-freeze-test.md line 86) - mesh + collider rebuilds were 10-17 % of real time (line 55).
  INFERENCE: that cost came from HOW it ran (a press every frame per wheel, 832 presses / 10 s, 8 m patches,
  band mesher) and not from being a heightfield. Today's voxel wins (cut behind in 250 ms batches, small units,
  build behind the truck) can be used for a heightfield too. Sheet tiles (3 m, 0.15 m) build in ~0.8 ms each
  (CODE comment MUD_MudSheet.c 122). Must be measured, not assumed.
- Risk / history: on 2026-10-13 test 6 (craters back on the heightfield, voxels for tunnels only) the user said
  "even worse", no details (results/2026-10-13-voxel-vs-heightfield-problem.md table row 6). And voxel craters
  were called "much better" (mud-gotchas line 305). INFERENCE: both reports came from the 2 m-cell hand-over
  and the old heightfield crater shape, not from heightfield-vs-voxel as such. This must be checked with the
  user before choosing c.
- Seam left: only at tunnel mouths (rare).

### (d) "Volumetric" - what it can mean in this engine
1. A density field meshed on the CPU = what we already have (voxel bricks). PROVEN works.
2. GPU volumetric rendering (raymarching, Teardown-style): needs own shaders. INFERENCE: not possible for mods.
3. Engine voxel support: none. The "Voxel" strings in the exe are occlusion culling (ida-voxel-feasibility.md
   section 5, PROVEN-IDA).
4. "Volumetric mud" as a game idea (MudRunner): a mud DEPTH per spot that drives forces and sinking, not
   geometry. That IS possible here: wheel radius sink (vanilla flat-tyre pattern, WheelSetRadiusState,
   2026-10-15-sinking-deep-dive.md section 2, PROVEN-IDA) + drag. It gives the "truck sits in the mud" feel
   with no fine collider at all.
- Verdict: more voxels won't fix the look. "Volumetric" in the MudRunner sense (depth -> forces/sink) is a good
  add-on to any option.

### (e) Sheet owns the visible top in zones, voxels collider-only there
- Look: no double surface on flat ground. But a heightfield sheet can't draw crater walls the voxels own, so at
  every crater the hand-over stays (the screenshot problem is exactly there).
- Feel: the voxel collider and the sheet still differ by the rebuild gap (2.2), so wheels float or sink into
  the drawn mud by a few cm. In mud that is acceptable (MudRunner wheels also sink into drawn mud, PROVEN-IDA).
- CPU: still two systems per op; the mortar stutter stays (2.1).
- Verdict: fixes the least-bad part. Not the crater edge, not the stutter.

### (f) Other ideas
- **f1 One grid, several channels (my main pick, the shape of option c).** One fine height grid per tile.
  Channels: base (undug ground), deformation (replicated: ruts, craters, trenches; drives the collider), and a
  client-only cosmetic mud offset (ridges, slump, gradual press; drawn only, max THICK 0.15 m). The mesh draws
  base + deformation + mud. The collider uses base + deformation. One mesh, so no seam can exist between look
  and feel; they differ only by the mud offset, which is the MudRunner model. The sheet's best code (Stamp, box
  ruts, ridges, Slump, TOP_LOOK) moves into the cosmetic channel as is.
- **f2 Decals for detail.** Vanilla tyre tracks exist (SCR_VehicleTrackDecal, World.CreateTrackDecal,
  api_search 2026-10-15). A texture-only track has no height, so no seam. Unproven on our script ground: an
  earlier decal try showed nothing (mud-gotchas line 228-233, cause unsolved). Cheap probe; nice-to-have.
- **f3 Wheel radius sink** (see d4) so the feel of soft mud doesn't need fine collider geometry at all.
  Probe 1 in sinking-deep-dive.md was never run.

### How shipped games do it (READ / INFERENCE - from docs where cited, else general knowledge)
| Game | Ground | Deform |
|---|---|---|
| Spintires / MudRunner | heightmap + mud layer per 16 m block, height relative to terrain | GPU min/max stamps; no mud collider, forces only (2026-10-14-mudrunner-ida.md, sinking-deep-dive.md) |
| SnowRunner | heightmap, "extrudable" ground types, snow/mud depth | sink to hard ground; capped depth (sinking-deep-dive.md table) |
| BeamNG | static heightmap (INFERENCE, general knowledge) | none; soft-body vehicles only |
| Battlefield / Frostbite | heightfield (INFERENCE, general knowledge) | crater = heightfield edit + decals |
| Teardown / Deep Rock Galactic | full voxels (INFERENCE, general knowledge) | own engines, GPU or threaded meshing |
Pattern: one representation per visible surface. Heightfield + depth layer for vehicle mud games; full voxels
only in games built around caves, on engines that allow it.

## 6. Ranking

| Rank | Option | Look | Feel | MP/JIP | CPU | Engine risk |
|---|---|---|---|---|---|---|
| 1 | **f1 / c: one grid, channels; voxels only for tunnels** | sheet quality everywhere, sharp rims | = look (minus mud offset) | same op stream, deterministic replay | must measure; old path was 13 fps worse, new path INFERENCE cheaper | low: heightfield meshes + trimesh proven |
| 2 | **a + d4: voxels only, add radius sink + maybe decals** | coarse ruts | solid + sinking | as today + 1 byte/wheel | as today | low; drop the sheet in craters |
| 3 | **e: sheet owns top in zones** | better flat ground, crater edge seam stays | few cm off | as today | stutter stays | low, but keeps the fight |
| - | b: adaptive voxels | best | best | as today | 3.4x on rut bricks + transition cells | high |

## 7. Recommendation

**Direction:** one visible surface per spot. A fine height grid (sheet style: 3 m tiles, 0.15-0.25 m) is the
ground wherever the ground is single-valued. It holds the replicated deformation (drives the collider) and a
client-only mud offset (look only). Voxels stay only for columns with real overhangs (tunnels).

**First proof step (SP, one mud zone, one day, behind a switch, checkpoint first):**
1. Inside one zone, send crater (kind 0) and rut (kind 4) ops into the sheet's ground array directly (same
   formulas as Carve / CarveRut), not into voxels. Voxels skip that zone.
2. Give the sheet tiles a second, wheel collider on Terrain | Foliage (Foliage: get-in fix, mud-gotchas line
   96-97), built from ground + deformation only, behind the truck (RUT_BEHIND rule). Keep its own ground traces
   from seeing it.
3. Draw steep quads with the soil UV plane (per-quad pick, as the voxel mesher).
4. Measure: mortar frame ms (target: under the 3 ms voxel-only control + ~1 ms per tile), rut-cut ms/s vs v035
   (20 ms/s client, mud-gotchas line 395), fps over the same loop as freeze-test, and look at a rut driven
   through a fresh crater. User check: one ground at the crater edge? ruts felt?
Pass = no seam at the crater edge, ruts felt, mortar frame under ~16 ms. Fail on cost -> fall back to rank 2.

**Stop doing:**
- Adding sheet <-> voxel stitch rules (TAPER, HOLE_SKIP, RIM_BARE ...). Each one moves the seam.
- Re-sampling the voxel field into the sheet in the go-live frame: it is 85-96 % of the mortar spike (2.1).
- Finer voxels, DC or smoothing for ruts: measured dead ends (cell-size.md, dc-proto.md, smoothing parked).
- Two separate meshes for look and feel anywhere. Use two numbers per vertex in one mesh instead.

**Note on the feature freeze:** this is a direction change, not a new feature. It removes a system (the
sheet <-> voxel bridge) instead of adding one. The user decides.
