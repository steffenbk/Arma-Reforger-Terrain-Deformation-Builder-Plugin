# Voxel options deep dive - what else could work, after everything we learned (2026-10-15 ~18:30)

User: "do a deep search into voxels and see what other possibilities we could use after all what we have learned".
Labels: MEASURED (numbers with their source), PROVEN (earlier doc that proved it in engine / IDA), READ (paper / web,
not tested by us), INFERENCE (my reasoning, not tested). Companion: docs/features/mud-sheet/2026-10-15-assessment.md.

## 1. Short answer

- **Best new find: press fine ruts INTO the voxel mesh instead of on top of it ("displaced ruts").** Keep 0.5 m voxels
  for craters / trenches / tunnels. Ruts go into a 2D "rut floor" grid (0.25 m). When a brick is meshed, its
  triangles near a rut are split 2 x 2 and pulled down to the rut floor. One mesh, one collider, so no seam, look =
  feel, same op stream, so MP / JIP stay as they are.
  MEASURED offline today (section 3): as sharp as 0.25 m voxels (rms 0.015 vs 0.020 m, rut walls 45 vs 44 deg)
  without the 3D sample cost; today's 0.5 m voxel ruts are 0.038 m / 30 deg.
- It is the idea the mud sheet was reaching for (a fine 0.15 m grid), but welded into the voxel mesh, not laid over it.
- Other options still open, in order: mud as numbers + wheel sink (feel), terrain-look material on voxel tops (probe),
  sharper crater / trench edges (DC vertex, measured -41 / -48 % rms), lazy JIP queues (scale).
- Ruled out (measured or engine-proven): 0.25 m voxels everywhere, DC for ruts, smoothing, anything needing our own
  shaders (GPU deformation like MudRunner / Tomb Raider, raymarched voxels, triplanar), a second ground mesh.
- Side finding (READ): no Arma Reforger workshop mod deforms terrain at runtime - the existing "trench" and
  "crater" mods are props, and players believe the engine can't. Ours does.

## 2. What we already know (the limits every option must live with)

| Fact | Label / source |
|---|---|
| Rut sharpness is limited by the HORIZONTAL sample spacing: a 0.5 m grid can't hold a ~0.17 m step between columns, even with exact normals | MEASURED results/2026-10-14-dc-proto.md (DC upper bound 31-42 deg) |
| 0.25 m voxels make ruts sharp but cost ~4-8x per brick (samples 8x, tris ~4x); mixing sizes needs transition cells | MEASURED results/2026-10-14-cell-size.md |
| Script mesh: positions + one UV set (0..1) only; normals rebuilt by the engine from our triangles (equal weight), no vertex colour, no 2nd UV, no displacement / shaders | PROVEN-IDA research/2026-10-14-ida-voxel-feasibility.md |
| Each mesh section needs its own .emat or crash; no collider refit (patch = rebuild cost) | PROVEN results/2026-10-14-engine-probes.md |
| CreateTriMesh 2048 tris 0.28 ms; script loops are the real cost | MEASURED engine-probes G3, mud-gotchas |
| Two grounds drawn in one spot always seam (heightfield vs voxel 10-13, look vs feel field, voxel vs sheet 10-15) | MEASURED / USER, research/2026-10-15-ground-architecture-review.md |
| Two different grounds under one vehicle in MP = corrections (DS stutter with the sheet) | INFERENCE strong, assessment 2.2 |
| Voxel ruts v035 (cut 3 m behind, 250 ms batches) are MP-solid: 0 corrections / 90 s, 64 fps driving | MEASURED CLAUDE.md history, results/2026-10-15-freeze-test.md |

## 3. Displaced ruts - offline proof (reachability first)

Tool: Tools/displace_proto.py (new, uses Tools/dc_vertex_proto.py's game-copied op formulas, mesher and metrics).
Methods: vox05 = today (ruts cut into 0.5 m voxels, mean vertex) = CONTROL; disp2 = coarse mesh without ruts, each
triangle split 2 x 2, vertex y = min(y, rut floor), floor grid 0.25 m; disp4 = split 4 x 4, grid 0.125 m; disp4g25 =
split 4 x 4, grid 0.25 m. "True" = the op formulas evaluated continuously. Distances in m.

**Control check:** vox05 reproduces the 2026-10-14 'mean' rows exactly: rut_straight 0.038 / 0.056 / 30 deg,
rut_diag 0.030 / 0.049 / 28, rut_arc 0.029 / 0.052 / 35. So the tool is measuring the same thing as before.

Full table (also in docs/superpowers/results/2026-10-15-displace-proto/table.md; pictures there, one per shape):

| Shape | Method | Tris | Slivers | mesh->true rms | mesh->true max | true->mesh rms | true->mesh max | Rut wall p90 deg |
|---|---|---|---|---|---|---|---|---|
| rut_straight | vox05 | 1208 | 6 | 0.038 | 0.119 | 0.056 | 0.155 | 30 |
| rut_straight | disp2 | 4448 | 0 | 0.015 | 0.089 | 0.028 | 0.132 | 45 |
| rut_straight | disp4 | 17792 | 0 | 0.012 | 0.064 | 0.017 | 0.092 | 57 |
| rut_straight | disp4g25 | 17792 | 0 | 0.016 | 0.089 | 0.024 | 0.129 | 45 |
| rut_diag | vox05 | 1160 | 0 | 0.030 | 0.124 | 0.049 | 0.154 | 28 |
| rut_diag | disp2 | 4448 | 0 | 0.014 | 0.101 | 0.025 | 0.144 | 45 |
| rut_diag | disp4 | 17792 | 0 | 0.011 | 0.077 | 0.013 | 0.096 | 57 |
| rut_diag | disp4g25 | 17792 | 0 | 0.014 | 0.090 | 0.018 | 0.137 | 44 |
| rut_arc | vox05 | 1240 | 12 | 0.029 | 0.124 | 0.052 | 0.286 | 35 |
| rut_arc | disp2 | 4448 | 0 | 0.012 | 0.153 | 0.037 | 0.318 | 49 |
| rut_arc | disp4 | 17792 | 0 | 0.011 | 0.109 | 0.031 | 0.322 | 59 |
| rut_arc | disp4g25 | 17792 | 0 | 0.014 | 0.135 | 0.034 | 0.316 | 46 |
| rut_x_crater | vox05 | 1456 | 0 | 0.052 | 0.221 | 0.076 | 0.366 | 47 |
| rut_x_crater | disp2 | 5824 | 0 | 0.044 | 0.218 | 0.064 | 0.337 | 49 |
| rut_x_crater | disp4 | 23296 | 18 | 0.042 | 0.217 | 0.056 | 0.265 | 54 |
| rut_x_crater | disp4g25 | 23296 | 2 | 0.044 | 0.218 | 0.061 | 0.312 | 48 |

Compared with 0.25 m voxels (cell-size.md, mean): rut_straight rms 0.020 / true->mesh 0.029 / wall 44 deg / 5170 tris.
disp2 gets 0.015 / 0.028 / 45 deg with 4448 tris, and its data is 2D (no extra 3D samples).

What it shows:
- disp2 halves the rut error of today (-53 to -61 % rms on the 3 rut shapes) and makes the rut walls 45-49 deg
  instead of 28-35. disp4 is sharper still (57-59 deg) at 4x more triangles again - probably not worth it.
- disp4g25 = disp2 quality: the 0.25 m rut DATA is the limit, not the mesh split. So 2 x 2 split + 0.25 m floor is
  the right pair.
- rut_x_crater: the crater part is unchanged (still 0.5 m voxels), so the gain there is small (0.052 -> 0.044).
- 0 slivers for disp2 (vox05 had 0-12). The engine weights every triangle's normal the same (PROVEN-IDA), so clean
  triangles matter.
- Weak spot: rut_arc true->mesh max got a bit worse (0.286 -> 0.318). INFERENCE: the gaps between arc segments in the
  rut formula (u >= 1 skipped) - same in all methods, a test-shape issue, not the method.
- Python ms in the table are NOT game costs (pure Python, whole grid split). In game only triangles near a rut would be
  split: here 376 of 2401 floor samples (16 %) had a rut on rut_straight.

### How it would sit in our code (INFERENCE - design sketch, nothing built)
- Rut op (kind 4) stops cutting voxel samples (CarveRut). It writes the rut floor (world y, min) into a sparse 2D grid,
  per 4 m brick column: 17 x 17 floats at 0.25 m (~1.2 KB per rutted column). Same op stream, same order, so every
  machine gets the same floor (MP / JIP unchanged in format; ruts are 96 % of JIP ops - jip-size.md).
- Mesher: after the surface-nets quads are built, split each triangle that has a rut-floor sample within it (+ 1 ring)
  2 x 2 and set y = min(y, floor). Unsplit neighbours: their shared edge has no rut pull at its ends (ring), so the
  split edge's middle vertex lies on the straight edge - no crack (T-junction on a straight line).
- Guards: pull only vertices less than ~1 m above the floor (tunnel ceilings, overhang lips stay); craters / trenches /
  fill clear the rut floor in their box (a fresh rim covers old ruts; a fill fills them).
- Collider = the same triangles (CreateTriMesh). Look = feel. A rutted brick has ~4x the triangles where ruts are;
  CreateTriMesh 2048 tris 0.28 ms (PROVEN), MAX_V 8000 per brick build (CODE MUD_VoxelProbe.c line 243) - check the
  busiest brick.
- A rut on a crater floor: the rut floor is "wheel contact - depth", an absolute y, so it works on any ground the
  wheel stands on (the voxel ruts already find the surface near the wheel).
- Raised side ridges (master list "FIRST IN LINE") fit the same grid as a second value (max blend = MudRunner PUSH)
  - but a raised ridge needs the vertex pushed UP, which only works where the coarse mesh is (ground top), fine.

### Risks / unknowns
- The ruts get sharper, so wheels feel more bump: v035 was MP-solid partly because ruts were soft. Must re-run the MP
  robot test (corrections count) before calling it done.
- More triangles in rutted bricks: real cost must be measured in game (rut ms/s vs v035's ~20 ms/s client).
- Script work to split + evaluate floor per vertex in Enforce (loops are the expensive part).

## 4. Other options, ranked

| # | Option | Gives | Status / evidence | Cost / risk |
|---|---|---|---|---|
| 1 | **Displaced ruts** (section 3) | sharp ruts, look = feel, one mesh | MEASURED offline today | medium build; MP re-test |
| 2 | **Mud as numbers + wheel sink** (assessment 5.1): mud depth per spot from the terrain gamemat, sink by WheelSetRadiusState, MudRunner drag | the mud FEEL, whole map, no mesh | PROVEN parts: gamemat via trace (mud-gotchas 2026-10-08), radius lowers the body (PROVEN-IDA sinking-deep-dive); probe 1 never run | low-medium |
| 3 | **Terrain-look material on voxel tops** (MatTerrainObject, used by vanilla graves / ErodedEmbankment) | voxel ground takes Arland's own painted look, no dirt "squares" | found in paks, never probed on a script mesh (results/2026-10-14-material-search.md) | low: one probe |
| 4 | **GlobalLayer angle material** (top texture on flat faces, soil on walls, one material) | nicer crater / trench walls without per-quad UV pick | found in paks, never probed | low: one probe |
| 5 | **DC vertex (dc_cd) for craters / trenches only** | crater rim / trench lip 41 / 48 % closer | MEASURED dc-proto.md; 0-4 folded tris -> needs a collider filter | medium |
| 6 | **Lazy JIP queues per brick** (papers Step D) | big matches join fast | READ (van Ruschen 2018), design not done | medium |
| 7 | **RLE / clamped seeds** | smaller JIP text | READ (Lengyel 33:1, van Ruschen 6:1) | low |
| 8 | **Poison material** (no-ground cells for a placed bunker / tunnel entrance prefab) | clean prefab holes | READ (Lengyel ch. 5) | low |
| 9 | **Track decals** (vanilla SCR_VehicleTrackDecal) | colour of tyre tracks, no height | earlier try showed nothing, cause unknown (mud-gotchas) | low probe |

Same idea, bigger: if displaced ruts work, trenches and the shovel could use the same 2D floor while no tunnel is near
(they are single-valued too). That walks back toward "one fine height grid + voxels only for tunnels"
(architecture review option c) - but on top of the voxel mesh, so there is never a hand-over seam. Decide after 1.

## 5. What the outside world does (READ) and why most of it doesn't fit us

| Approach | Who | Fits us? |
|---|---|---|
| Mud = the ground heightfield, deformed on the GPU (min / max stamps), physics on separate data, hidden "extrusion" depth map that drives passability | MudRunner / SnowRunner (Saber modding docs, mudrunnermods article) | Only the cheap half: a hidden depth number per spot that drives physics = option 2. The GPU half: no shaders for mods (INFERENCE) |
| Deferred deformation: a deformation heightmap applied while drawing the snow mesh, push down + raise sides, 4 MB | Rise of the Tomb Raider (SIGGRAPH 2015 talk) | Shader-side - no. The "push down + raise sides as two blends" idea is what option 1 + ridges do on the CPU |
| Coarse voxel surface + per-patch heightmap displacement for detail | Koca & Gudukbay 2014 (IJGIS 28(9), hybrid terrain) | YES - the source of option 1. They use Bezier patches; we split triangles (simpler, crack-free by world-xz floor) |
| Heightmap for the world + SDF / voxels only where caves are (5-10 % of chunks) | general practice (cinevva guide, forums) | = what we have (heightfield + sparse 4 m bricks) |
| Stacked heightfield layers / column spans (floor + ceiling per spot) | Voxlap-style engines, forums | Possible representation for tunnels, but a new mesher for no gain over voxels + displaced ruts |
| Raymarched sparse voxel trees, GPU meshing | Aokana 2025, Teardown-style | Needs compute shaders - no |
| Dual contouring / adaptive octrees for sharp features | many voxel engines | Measured: helps craters / trenches, not ruts (option 5) |

## 6. Suggested order (user decides; feature freeze - these go to Parked ideas until the freeze ends)
1. Cheap probes first, one session: MatTerrainObject on one brick (option 3) and GlobalLayer (option 4) - look only.
2. Displaced ruts in game behind a switch (checkpoint first): SP look + rut ms/s vs v035, then the MP robot test.
3. Wheel sink probe 1 (option 2), then the gamemat -> mud depth table.

## Sources
- Koca, Gudukbay 2014, "A hybrid representation for modeling, interactive editing, and real-time visualization of
  terrains with volumetric features", IJGIS 28(9) 1821-1847 -
  https://repository.bilkent.edu.tr/bitstream/11693/26473/1/A%20hybrid%20representation%20for%20modeling%2c%20interactive%20editing%2c%20and%20real-time%20visualization%20of%20terrains%20with%20volumetric%20features.pdf
  (section 3.1 read: coarse voxel surface as patches, each patch displaced by a heightmap for resolution)
- Saber modding docs, mud brush / extrudes - https://expeditions-guides.saber.games/map_modding/creating_a_map/terrain/geometry_brushes_for_terrain/mud/
- "The Mud of MudRunner" - https://www.mudrunnermods.com/the-mud-of-mudrunner/ ; "SnowRunner: Terrain Physics" - https://www.snowrunnermods.com/snowrunner-terrain-physics/
- Rise of the Tomb Raider rendering talk (SIGGRAPH 2015) - https://history.siggraph.org/learning/labs-rd-rendering-techniques-in-rise-of-the-tomb-raider
- Heightmap + SDF hybrid guide - https://app.cinevva.com/guides/landscape-generation-browser ; layered heightfield threads -
  https://gamedev.net/forums/topic/556744-realistic-cliffs-caves-etc-for-terrain/4576211/ , https://community.khronos.org/t/heightfields-and-caves-overhangs/44866
- Surface nets / sharp features - https://0fps.net/2012/07/12/smooth-voxel-terrain-part-2/ , https://github.com/Zylann/godot_voxel/issues/24
- Reforger mods (no runtime deformation) - https://reforger.armaplatform.com/workshop/69FA76B7D8B6F785-Trench ,
  https://reforger.armaplatform.com/workshop/5EFACBB486FD2D3F , https://forums.bohemia.net/forums/topic/243966-trenches-in-arma-reforger/
- Earlier reads: research/2026-10-14-lengyel-voxel-terrain-notes.md, research/2026-10-14-voxel-papers-batch2-notes.md.
