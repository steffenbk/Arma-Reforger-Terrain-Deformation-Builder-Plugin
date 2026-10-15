# Ruts - alternative ways, whole map (2026-10-15 ~20:30)

User: "our mud sheet rut was good, but with our voxel system it was hard to implement, and it would be impossible to
have on the whole map. Do some deep research into other ways to actually do ruts."
Labels: CODE (read in our script today), MEASURED (our logs / offline tools, file given), PROVEN (earlier doc),
READ (web search result text - this session's network policy blocked opening the pages themselves, so only the
search snippets were read), USER, INFERENCE (my reasoning, not tested).
Companions: research/2026-10-15-voxel-options-deep-dive.md, features/mud-sheet/2026-10-15-assessment.md,
plans/2026-10-15-displaced-ruts-plan.md.

## 1. Short answer

- **New finding: today's voxel ruts can't cover the whole map either.** Every rut op converts 4 m voxel columns
  along the track (Issue -> ConvCols: the op box + one ring), and the match has a cap: `MAX_COLUMNS = 6000`
  (CODE MUD_VoxelProbe.c:274, refused in Issue ~1781: "column cap reached ... the heightfield digs it"; with RUTS
  true the heightfield takes no ruts, so ruts just stop). 6000 columns x 16 m2 = 0.096 km2 = ~0.6 % of Arland
  (4096 x 4096 m). INFERENCE: a track converts roughly 0.75-1 column per metre driven (box +-1 m + a ring each side),
  so ~6-8 km of fresh driving per MATCH, all vehicles together, and then no more ruts. Displaced ruts (the current
  plan) keep this limit - they still cut through voxel bricks.
- **So the real question is not "sheet vs voxels" but "what does a rut need?"** A rut is single-valued (one height
  per x, z), shallow (< 0.4 m) and only near the surface. It needs a fine 2D grid, not 3D voxels. Every serious
  rut system found does it in 2D: MudRunner (heightfield blocks), Chrono SCM (grid of vertical nodes, only touched
  nodes stored), PhysX heightfield edits, the column model in "Animating Sand, Mud, and Snow".
- **Recommended direction: ONE sparse 2D rut field, drawn by whichever ground is there.**
  - Store: rut floor (+ ridge) per 0.25 m sample, only in touched 8 m cells (hash map, like Chrono SCM).
  - Plain ground (most of the map): our heightfield fine patches already ARE a 0.25 m grid over the whole map
    (CODE MUD_WorldGround.c:88-91 PCELLS 32, FINE 8 -> 8 m patches, 0.25 m cells) with MP + JIP for deformed patches
    (MUD_FinePatch.m_bDeformed "kept for the match, sent to joiners"). The rut field maps 1:1 onto their vertices.
  - Voxel ground (crater / trench / tunnel areas only): displaced ruts - the plan we already have, unchanged.
  - Ruts then stop converting columns at all: no MAX_COLUMNS drain, no seed text, far fewer bricks.
  - That is "the sheet's quality, on the ground itself, on the server too" - the thing the sheet could not be.
- **Add-ons that make it look like the sheet without more geometry:** engine track decals on our ground entities
  (you already use `World.CreateTrackDecal` in the spray-can mod - needs one probe on our entities), and grass
  flattening along tracks (`AffectFoliage*`, vanilla API).
- **Ruled out for whole map:** the sheet as a second mesh (assessment.md: 340 GB, two grounds in MP), prefab track
  pieces (your Roadforger caps 2000 entities), anything GPU (render-target / virtual-texture / tessellation
  deformation - no custom shaders for mods).

## 2. Why the sheet looked good, and which parts can move to the ground

| Sheet feature (USER-OK) | Why it looked good | Can the ground have it? |
|---|---|---|
| 0.15 m cells | sharp rut walls | 0.25 m on the heightfield (fine patches) / displaced bricks. MEASURED offline: 0.25 m floor gives walls ~45 deg vs 28-35 today (displace-partial.md). 0.15 m not needed for the shape, maybe for the ridge crest |
| SOFT ruts, cosine profile | no V-notch, rounded floor | yes - just the profile function in the rut op |
| Side ridges (arch, ONE_W0, PUSH) | the "pushed mud" read | yes - a 2nd value per sample (max blend), MudRunner PUSH; heightfield has berms already (old berm 0.9 m / 0.08 m, raised-ruts-ideas.md) |
| GRADUAL press | ruts build up per pass | yes - voxel ruts already step 0.08 m / pass (CODE CarveRut), same rule |
| TOP_LOOK: untouched = terrain look, touched = dirt | the rut reads as a track | partly: one material per mesh section; a 2nd section for "touched" quads (own .emat - PROVEN rule) + track decals on top |
| Mud on vehicles, drag | feel | separate (sinking deep dive), not geometry |

What the sheet could NOT do and the ground can: be the same on every machine (server physics), cover any spot the
op stream reaches, and be ONE surface (no seam rules - review.md "third time we hit this wall").

## 3. Options, ranked

| # | Option | Look | Feel | MP / JIP | Whole map | Status |
|---|---|---|---|---|---|---|
| 1 | **2D rut field on the heightfield + displaced on bricks** | 0.25 m, ridges, soft profile | = look (one collider) | same op stream; JIP = op log (or send touched cells, as Chrono / our patch JIP) | yes, sparse | design (section 4) |
| 2 | Displaced ruts on voxels only (current plan) | as 1 | as 1 | as today | NO - MAX_COLUMNS ~6-8 km per match | being built |
| 3 | Track decals on our ground entities | colour / tread, maybe normal-map depth | none | client-local (vanilla) or from ops | yes, engine-managed lifetime | PROBE (API used in your spray mod) |
| 4 | Grass flattening along tracks (AffectFoliage*) | grass pressed in the track | none | client-local is fine (look only) | yes | PROBE |
| 5 | Wheel sink + drag (forces) | tyre half buried | sinking | vanilla flat-tyre pattern | yes | PROBE 1 never run (sinking-deep-dive.md) |
| 6 | Sparse script ribbon along tracks (sheet only where wheels went) | sheet quality | look only | client-only | yes, sparse | two-mesh seam again (review 2.2) - only if 1 + 3 are not enough |
| 7 | Prefab track pieces (Roadforger style) | modelled tread | optional collider | server spawned, replicated | no - entity count (your mod caps 2000 global) | known |
| 8 | GPU deformation (MudRunner, Batman AO snow, RotTR, UE RVT trails) | best | - | - | - | not possible for mods |

## 4. Option 1 in detail - one 2D rut field, two consumers

Data (INFERENCE, design):
- Key = 8 m heightfield patch (same grid as MUD_FinePatch, 33 x 33 vertices at 0.25 m). Per touched patch:
  `floor[1089]` (world y, NONE = no rut) and optionally `ridge[1089]`. ~8.7 KB per touched patch with both.
  Sparse map like the voxel columns. 10 km of tracks ~ 1,900 patches ~ 16 MB (INFERENCE: 10000 / 8 x ~1.5).
  Chrono SCM does the same: "only nodes that have been deformed are kept in a hash map; nodes are displaced only
  vertically" (READ, Chrono SynChrono terrain sync docs).
- The rut op (kind 4) stays as it is on the wire (segments, step, cap). On apply it writes the floor field instead
  of cutting voxels. Every machine gets the same field (same op order). No column conversion, no seed.

Consumers:
- Heightfield patch (most of the map): when a patch is built, vertex y = min(ground y, floor) (+ ridge). The patch
  vertex grid IS the floor grid, so no split, no midpoints, no T-junctions. Collider = the same vertices.
- Voxel brick (a crater / trench / tunnel converted it): displaced ruts as planned (split + pull), reading the same
  field. A rut that drives into a crater just continues - no hand-over.
- Readers (SurfaceAt / ColumnDepth / MUD_Surface.Y / trench planner) read min(ground, floor) - as plan Step 3.

Cost (what is known):
- MEASURED (freeze-test.md): heightfield ruts in the OLD way cost ~1.0-1.7 s per 10 s - but that was presses under
  the wheel at full rate (300+ band mesh builds x 3.2 ms + 120-150 colliders x 4.3 ms per 10 s). Voxel ruts became
  smooth because of RUT_BEHIND (cut 3 m behind) + one op per vehicle per 250 ms, not because of voxels
  (MEASURED v035: 0 corrections / 90 s, 64 fps).
- INFERENCE: field ruts with the SAME batching rebuild 1-2 patches per op (a 250 ms op covers ~2-3 m of track).
  Compare with today's go-live 6.3 ms per op (rut-collision-options.md LOG). The 3-4 ms per patch build is script
  time (CreateTriMesh itself is 0.28 ms for 2048 tris - PROVEN engine-probes) - worth one profile pass.
- Risk: the old heightfield rut code was REVERTED once for look reasons (rut layer split 2026-10-11, user: "ruts were
  more gradual" before) - that was about capping the collider, not the 2D idea. Keep look = feel this time.

What it fixes at once: the whole-map limit (no MAX_COLUMNS for ruts), JIP size (ruts are 96 % of the voxel JIP ops -
jip-size.md; the patch JIP path sends patch data instead, which stops growing once a patch is fully rutted),
and the brick count (ruts were most of the bricks made in the freeze-test "After" table).

## 5. Option 3 in detail - track decals (cheap look, whole map)

- READ (Enfusion script API, search snippets): `TrackDecalFactory(material, width, lifetime)`, one per wheel;
  `AddPoint(entity, worldPos, worldNormal, alpha)` "suitable to be called each frame, point deduplication and track
  segment merging are done internally"; `FinalizeDecal(fadeoutLength)`. Vanilla component `SCR_VehicleTrackDecal`
  (UpdateTrack per wheel, min lateral / longitudinal slip thresholds).
- CODE (your repo steffenbk/Arma-reforger-steffenbk-mod-open-source, "Spray tagging can"):
  `world.CreateTrackDecal(pHitEntity, hitPos, hitNormal, size, lifetime, material, null, 1.0)` - works on a hit
  ENTITY, and the mod skips when the entity is null (terrain). Your workshop mod "Vehicle Track System" (READ:
  "persistent wheel and chopper tracks for multiplayer") uses the same family.
- Our earlier miss (mud-gotchas 2026-10-12): `World.CreateDecal` + DecalEntity from the heightmap - hung under the
  map on hole cells, nothing visible. Different call, and it projected onto the terrain, not our entity.
- PROBE (one session): TrackDecalFactory.AddPoint / CreateTrackDecal with entity = our MUD_VoxelSubEntity and our
  fine patch entity, at the wheel contact. Questions: visible on a MeshObject.Create mesh? survives the mesh being
  rebuilt (SetObject new mesh) or must be re-laid? normal-map depth visible?
- Fits the RUT_BEHIND gap nicely: the decal appears under the tyre at once, the geometry 3 m later.

## 6. What the outside world does (READ) and the fit

| Who / what | How | Fit |
|---|---|---|
| MudRunner / SnowRunner | mud = heightfield layer per 16 m block (25 x 25 base + 128 x 128 mud texture only where mud), deformed on GPU; SnowRunner: viscosity per surface type, darker = more viscous | 2D field per block = option 1's data shape; viscosity per surface = gamemat table (assessment 5.1) |
| Chrono SCM (vehicle terramechanics) | integer grid, vertical displacement only, touched nodes in a hash map; bulldozing pushes soil to the rut edges; sync sends modified nodes | option 1 data + sync; bulldozing = our ridges |
| "Animating Sand, Mud, and Snow" (arXiv 2302.08683) | ground as vertical columns, displacement + compression, tunable per material | per-pass compaction rule (gradual press) |
| PhysX heightfield modifications (NVIDIA) | runtime heightfield edits to carve tracks in mud | physics side only; we have no script heightfield collider (PROVEN, trimesh only) |
| Batman: Arkham Origins snow (GDC 2014), Rise of the Tomb Raider, UE5 RVT trail packs | render-target / virtual-texture displacement on the GPU | no shaders for mods |
| Unity SnowRunner-like prototype (80.lv) | chunked terrain, per-chunk track storage, per-frame deformation by wheel load | same as option 1 |

## 6b. Follow-up findings (same session, later)
- **Why the old heightfield ruts cost 3-4 ms per patch build** (CODE MUD_WorldGround.c BuildMeshBand ~5980,
  BuildColliderBand ~6112): every rebuild runs the band mesher (layered-SDF surface nets over the 33 x 33 patch,
  `m_Band.Build`), then a 2nd mesh section (groove), and on the collider side ONE TRIMESH PER MATERIAL SLOT + a
  reversed "bullet slab" trimesh on FireGeo + a safety "plate" trimesh, then destroy + CreateStaticEx. Plus the
  smoothed heights (UpdateSmooth). So the cost is the extras, not the 0.25 m grid. INFERENCE: a rut-field patch
  build that skips what a rut does not need could be much cheaper - profile `m_iBandCoreMs` vs the rest first
  (the counters already exist in BandStats).
- **Heightfield MP already syncs data, not ops** (CODE: RpcDo_Heights / SendChanges / ApplyRect send packed mm
  height rects; JIP sends deformed patches - QueueJip / SendJip). The rut field can use either: the voxel op stream
  (small, deterministic) or this patch path (bounded per patch). Chrono SCM syncs modified nodes the same way.
- **AffectFoliage is per frame** (READ API: "submit an obstacle in this frame", flatten / sway / strength / falloff):
  grass springs back when we stop calling it. Only useful if re-submitted every frame for tracks near the camera -
  IDA item 3 checks the decay.
- **Grass already reacts to our ground** (PROVEN mud-gotchas ~75: clutter on our ground, crater pits come out clean).
  If ruts can clear it too (IDA item 2), that is a whole-map track look with no new system.
- **Vehicles moved to server authority** (READ: Bohemia "server authoritative vehicles"; prefabs use
  VehicleWheeledSimulation_SA / SCR_CarControllerComponent_SA; 1.9 notes mention wheeled simulation changes). Our
  stutter research (2026-10-11-driver-stutter-deep-dive.md) was on the older path - re-check which class the test
  vehicles use before the displaced-ruts MP test (IDA item 5).
- IDA questions for all of this: [2026-10-15-ida-rut-questions.md](2026-10-15-ida-rut-questions.md).

## 6c. IDA answers 1-5 (exe 1.9.0.10 Diag, research/2026-10-15-ida-rut-questions.md) - what they change
- **Track decals (TrackDecalFactory): terrain only.** A decal starts only on the world's terrain entity and is clipped
  to the terrain HEIGHTMAP - over our dug ground it would follow the old surface. Dead for ruts on our ground.
  OPEN: IDA found no `CreateTrackDecal` string, yet the spray-can mod calls `world.CreateTrackDecal(...)` - does that
  mod still compile / work on 1.9? (2-minute check; if it works, the IDA reading missed a binding.)
- **World.CreateDecal: a mesh decal clipped from the TARGET entity's own triangles, and MeshObject accepts decals**
  (INFERENCE: our script meshes are the same class). Our 2026-10-12 miss: stretch 0 + the decal went to the terrain.
  But the decal hangs off the OLD mesh: every SetObject rebuild drops it. Use only on ground that stopped changing, or
  re-stamp the touched area after each rebuild (we own the rebuilds; the engine dedupes stamps closer than
  min((size/2)^2, 0.1) - cheap). Probe values: stretch 1, nearclip ~0, farclip 0.5-1, project = -normal, a
  MatPBRDecal material, entity = the brick / patch entity.
- **Grass is GPU clutter from the terrain**, not placed on our mesh (the old ObjectsHeightMap guess was wrong - that is
  far-shadow data). No script way to clear it along a track. INFERENCE: the clutter mask pass "computing heightmap
  discontinuities" (ClutterMaskDepthRT) is probably what cleans crater pits - so DEEP ruts may already lose grass by
  the same rule and shallow ones keep grass standing at the old height. Look with Render > "Grass/Clutter/Obstacles"
  = "discontinuity mask" over a v035 rut.
- **AffectFoliage works for tracks:** one 20-byte obstacle per call, max 1000 per frame, flattened grass stays down
  ~10 s after the last submit and recovers over ~5 s (Render diag defaults FlattenOnset 0.4 / FlattenLying 10 /
  FlattenRecovery 5, INFERENCE seconds). So: client-only, re-submit the recent track segments within the mask radius
  every ~5 s -> pressed-grass tracks on the whole map, no net, no geometry. Cheapest visible win found.
- **No own normals** - confirmed again (no flag reaches the format byte). Sharp-looking ruts need real geometry.
- **Wheel = one centre ray, raw triangle normal.** This shapes the rut PROFILE more than anything else: rounded,
  step-free, walls <= ~45 deg where wheels run. Displaced-ruts plan updated (Step 1 profile, Step 4 feel).
- **Server authority:** the class in this exe is VehicleWheeledSimulation_SA_B; same wheel-cast core
  (INFERENCE). Check our test vehicles' prefab simulation component before Step 5.

Updated order: (1) AffectFoliage track flattening (client-only, cheap, whole map) can go in any time;
(2) displaced ruts with the smooth profile; (3) CreateDecal probe on a brick that no longer changes;
(4) rut field on the heightfield (section 4).

## 7. Suggested order (user decides)
1. Let the displaced-ruts build finish - its Displace() is option 1's voxel half anyway, and its debug tools carry
   over. Change its storage later from per-column to per-patch if option 1 goes ahead.
2. Probes, one session, no risk: track decal on our entities (option 3), AffectFoliage along a track (option 4),
   wheel radius sink (option 5).
3. Option 1 heightfield half behind a switch: rut op writes the field, fine patches apply min(), voxel conversion
   for ruts off. Measure patch build ms with RUT_BEHIND batching vs v035, then the MP robot test.
4. Then ridges as a second field value (both consumers at once).

## Sources (READ via search results; pages could not be opened from this session)
- TrackDecalFactory API: https://community.bistudio.com/wikidata/external-data/arma-reforger/EnfusionScriptAPIPublic/interfaceTrackDecalFactory.html
- SCR_VehicleTrackDecal: https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__VehicleTrackDecal.html
- Vehicle Track System (workshop): https://reforger.armaplatform.com/workshop/668AD0DF0171A2B7-VehicleTrackSystem
- Chrono deformable terrain synchronization: https://api.projectchrono.org/terrain_synchronization.html
- Chrono SCM terrain class: https://api.projectchrono.org/6.0.0/classchrono_1_1vehicle_1_1_s_c_m_deformable_terrain.html
- Snake robot on SCM terrain (nodes created on touch): https://arxiv.org/pdf/2512.05008
- Animating Sand, Mud, and Snow: https://arxiv.org/pdf/2302.08683
- PhysX dynamic heightfield modifications: https://developer.nvidia.com/content/physx-dynamic-heightfield-modifications
- The Mud of MudRunner: https://www.mudrunnermods.com/the-mud-of-mudrunner/ ; SnowRunner terrain physics: https://www.mudrunnermods.com/?p=40604
- Deformable Snow Rendering in Batman: Arkham Origins: https://gdcvault.com/play/1020177/Deformable-Snow-Rendering-in-Batman
- SnowRunner-like Unity prototype: https://80.lv/articles/check-out-this-physics-driven-off-road-prototype-in-unity/
- Code: steffenbk/Arma-reforger-steffenbk-mod-open-source (Spray tagging can/Scripts/Game/SCR_SprayPaintDecalEffect.c,
  Roadforger/Singetrackspawner.c)
