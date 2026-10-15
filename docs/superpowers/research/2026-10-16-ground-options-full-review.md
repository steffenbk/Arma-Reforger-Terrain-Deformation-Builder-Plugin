# Ground options - full review: ruts, craters, trenches, tunnels and more (2026-10-16)

User: "read through everything we have done, don't stop until you have a full grasp. Look FULL through all options we
have for ruts, craters, tunnels, trenches and more for Arma Reforger modding: are voxels the best, what can work with
voxels, what have we not tested, what could we test. Leave no stone unturned."

Read for this: CLAUDE.md, HANDOFF.md, every docs/features README, master list (new + old), all of research/,
results/, plans/, specs/, mud-gotchas.md (all 458 lines), the archive READMEs, and the code (MUD_VoxelProbe.c,
MUD_WorldGround.c crater / cover / hooks, trench, house, wheels). Code read at 6bb6738 (= v102-ish, rut field on).
Labels: CODE (read in our script today, line given), MEASURED / PROVEN (earlier doc, cited), READ (web), INFERENCE
(my reasoning, not tested), NEW (found in this read, not in any earlier doc). Nothing was changed in the code.

## 1. Short answer

**Are voxels the best? For tunnels: yes, they are the only option. For ruts: no, and we already moved off them. For
craters and trenches: good enough today, not the end state.**

- The engine leaves us exactly one way to change ground at runtime: our own script meshes + script trimesh
  colliders over a holed terrain (PROVEN: no runtime height / hole / layer edit, 13 terrain natives, mud-gotchas
  ~26, ~86-89). Inside that, the only real choice is the DATA shape: 2D (one height per x, z) or 3D (density).
- A rut is 2D (single-valued, shallow). The rut field (RUT_FIELD true, stage 1-5 robot PASSED, CODE
  MUD_VoxelProbe.c:276) already stores ruts as a 0.25 m 2D floor drawn by both grounds. That is the right answer;
  every serious rut system does it in 2D (Chrono SCM, MudRunner, Valheim - research/2026-10-15-rut-alternatives).
- A crater and a trench are ALSO single-valued (the FUNNEL rule even forbids undercuts, CODE :306). Voxels win here
  only because they meet tunnels without a hand-over seam. A 2D grid would be sharper (0.25 m vs 0.5 m) but the
  2026-10-13 hand-over tests (results/2026-10-13-voxel-vs-heightfield-problem.md) show what a 2-system seam costs.
  Keep voxels for craters / trenches unless the look is a real complaint; then "displace" crater rims / trench lips
  the same way ruts are displaced (section 4.2 C4) rather than switching systems.
- A tunnel is truly 3D. Voxels are the only representation we have that can do it, and it works (PROVEN 2026-10-13).
  Layered heightfields ("floor + ceiling per column") are the only alternative and were never tested; they give no
  gain over voxels here (section 3).
- So the architecture we have now - heightfield base + 2D rut field + sparse 4 m voxel bricks for craters, trenches,
  tunnels - is the right shape. **The biggest open items are no longer the representation. They are the untested
  gameplay gaps around it** (section 2), most of them cheap to check.

**Six things this read found that no earlier doc lists (NEW):**

| # | Finding | Why it matters | Evidence |
|---|---|---|---|
| N1 | **Voxel colliders have no bullet slab.** Bricks use Terrain \| Foliage only (CODE MakeBody :5372). The heightfield needed a downward FireGeo slab because "open script trimesh on Terrain preset lets bullets pass (rim -> next crater)" (PROVEN 2026-10-08 09:26). No test of rifle fire into a voxel crater rim or trench parapet exists in any doc or log. | Cover in craters / trenches is the point of the feature in PvP. If rounds pass, trenches are worthless as cover. | CODE + grep of docs/logs |
| N2 | **Voxel ops ignore the building lock.** The heightfield never moves ground inside a building's box (m_aLock, BuildingLock CODE MUD_WorldGround.c:2907). Carve / CutTrench / CarveTunnel / FillTrench only skip m_aHard (kept terrain) (CODE :2684, :3257, :3315, :3382). | A mortar next to a house, a trench dug to a wall, a tunnel under a house: the house can float over a pit. | CODE |
| N3 | **Nothing wakes objects over a new hole.** No SetActive / query after a voxel go-live (grep: only MUD_VanillaHoleFix touches SetActive). Trees, props, mines, dropped weapons, sandbags, parked or GM-placed vehicles at rest stay where they were. | Floating objects over craters; INFERENCE: the open Lane 3 bug "GM-placed vehicle hangs in air" may be this (a sleeping body whose ground went). Old item N8 (perf overview) was never closed. | CODE |
| N4 | **Voxel craters lost the heightfield crater's shaping.** Crater() hands only `full * scale` to MUD_VoxelProbe.Blast (CODE MUD_WorldGround.c:5380 / :5427) BEFORE the depth jitter, oval / lobe outline, stack cap (m_fCraterStackDepth 2.5 / factor 1.67), deepen gain and the 42 deg max wall slope are applied. Voxel Carve is a fixed half-ellipsoid (crest 2.2 x depth) + funnel + sine rim (CODE :2657). Repeated hits go down to FLOOR 3 m. | Answers the crater README "re-check old crater items against the voxel code": every crater looks the same again, walls near the crest are steep (ellipsoid edge goes vertical, soldier MaxSlope 50 deg), shelled ground sinks to 3 m. | CODE |
| N5 | **Voxel house is ENABLED for release but SP-only.** MUD_VoxelHouse.c header: "SP only"; no Rpc in MUD_VoxelHouse.c / MUD_WallPanels.c. In MP a client pressing J voxelizes the house LOCALLY, hides it and turns its collision off locally; the server keeps the real house. | Desync; client walks into walls the server still has (corrections / rubber banding), a cheat-like view through walls. | CODE (Lane 2 says "DONE, works" - it was tested in SP) |
| N6 | **Two docs call "terrain-look material on voxel tops" an unprobed option - it is already shipped.** MAT_TOP = default_MatTerrainObj1.emat (MatTerrainObject) on every brick top (CODE :351). | voxel-options-deep-dive option 3 and assessment 5.1 can drop it. What is still unprobed is GlobalLayer (angle blend) on walls. | CODE |

Smaller NEW notes: voxel FLOOR is a constant 3 m, DepthBox entities are ignored by voxels (no DepthAt in
MUD_VoxelProbe.c - deep tunnels / bunkers impossible even where a mission maker placed a depth box); ruts and the
whole wheel system cast to VehicleWheeledSimulation only (CODE MUD_WorldGround.c:1160), so tracked vehicles make no
ruts, and if 1.9's server-authority class (VehicleWheeledSimulation_SA_B, IDA 2026-10-15 item 5) is NOT a subclass,
ruts silently stop on SA vehicles after the update (untested, one-line check in Workbench).

## 2. Where we stand (state map)

| Feature | Data | Resolution | Collider | MP / JIP | Limits today |
|---|---|---|---|---|---|
| Base ground | coarse skin 2 m + fine patches 8 m (0.25 m verts) over a holed .ttile | 0.25 m | per patch trimesh + FireGeo slab + plate | heights / ops, JIP patch copies | no grass under deformed ground changes; roads / water kept terrain (never dug) |
| Ruts | 2D rut field (MUD_RutTile 16x16 per 4 m), drawn by patches and displaced into bricks | 0.25 m | = mesh (look = feel) | op 4 stream, deterministic hash proven | ~10 mesh + 10 collider patch rebuilds/s at 6-7 ms (LOG); JIP replays every rut op; no ridges; no healing |
| Craters | voxel bricks, op 0 | 0.5 m samples | brick trimesh Terrain\|Foliage, no FireGeo (N1) | op stream + seed, proven | fixed shape (N4), FLOOR 3 m, MAX_COLUMNS 6000, building lock ignored (N2), go-live 50-100 ms big blasts |
| Trenches / shovel / fill | voxel bricks, op 2 / 3 | 0.5 m | as craters | proven | 60 deg walls (SLOPE_RUN 0.58), max 2.5 m, no spoil / parapet, no revetment, MAX_SITES 20 |
| Tunnels | voxel bricks, op 1 capsule r 0.8, 2.5 m tall | 0.5 m | as craters | server issues, no client prediction | no roof / bridge rules (thin paper roofs), FLOOR 3 m, no AI, building lock ignored |
| Mud sheet | client-only 0.15 m second mesh | 0.15 m | CharCollide only | client-only | PARKED; proven dead end as a ground (assessment) |
| Voxel house | 0.25 m voxels + wall panels | 0.25 m | trimesh | NONE (N5) | SP-only, one house per session |
| AI navmesh | - | - | - | - | dead end for script bodies (PROVEN IDA 2026-10-12) |

## 3. Every representation we could use, and its verdict

| Representation | What it is | Status | Verdict |
|---|---|---|---|
| Edit the real terrain heightmap / holes at runtime | engine terrain | PROVEN impossible (no natives; made-up proto "not linked"; WorldEditorAPI is editor-only). IDA item 10 re-check on the 1.9 exe still open | dead - unless item 10 finds something |
| 2D heightfield patches (ours) | 33x33 grid per 8 m | shipped base | best for anything single-valued; cheapest collider (CreateTriMesh 2048 tris 0.28 ms) |
| 2D sparse field on top (rut field) | floor per 0.25 m, only touched tiles | shipped (stage 1-5) | the right rut answer; could carry ridges and more "channels" (architecture review f1) |
| Voxel SDF bricks, surface nets (ours) | 0.5 m density, 4 m bricks, sparse | shipped | only option for tunnels; fine for craters / trenches |
| Finer voxels 0.25 m | same, 8x samples | MEASURED 3.4-5.6x cost, 13.8 s worst latency | dead everywhere; maybe for a tunnel-only brick later |
| Adaptive / Transvoxel LOD | mixed sizes + transition cells | rejected (cost + crack risk), not built | dead for now |
| Dual contouring vertex | sharper vertex per cell | MEASURED: ruts no, craters -41 %, trench -48 % rms, 0-4 folds | parked polish for craters / trenches with a collider fold filter |
| Displacement of the coarse mesh by a fine 2D field | split + pull (displaced ruts) | shipped for ruts | the best tool we have for SHARPNESS on voxels; untried for crater rims / trench lips (4.2 C4, 4.3 T5) |
| Layered heightfield / column spans (floor + ceiling per column) | 2.5D with overhangs | never tested | no gain over voxels for our tunnels; would need its own mesher. Skip |
| Second mesh on top (mud sheet) | client-only look layer | PARKED, proven 3rd "two grounds" wall | dead as ground; maybe a look-only trail later |
| Prefab kit (.xob shells: crater bowls, trench segments) | hand-made meshes with baked colliders | never tested as GROUND (only vanilla sandbags) | worth a thought for ONE reason: an .xob with a baked Navmesh-layer collider is the only thing the navmesh generator reads (PROVEN IDA 2026-10-12). Cannot carve - it can only sit on our ground. AI was dropped (PvP), so low |
| GPU deformation (MudRunner, RotTR, RVT) | shaders / render targets | no custom shaders for mods (INFERENCE, consistent everywhere) | dead |
| Decals (World.CreateDecal on our meshes) | textured mesh decal clipped from the target's triangles | IDA: MeshObject accepts decals; dropped on every SetObject rebuild; our 2026-10-12 try used stretch 0 on the terrain | untested look add-on for finished ground (tracks, scorch, wet patches) |
| Forces only (mud depth -> sink / drag) | no geometry | probe 1 never run (MUD_RadiusProbe archived) | cheap, whole-map "feel"; complements any geometry |

Outside the engine (READ, this session): no Reforger workshop mod deforms ground at runtime. PRLX Trench (1.8.0.10)
piles up an earth embankment because its author says the engine can't dig; ACE Trenches places dirt-cover props
(~1.76 M downloads); "Breakable ACE Trenches" collapses those props. Players believe trenches can't be dug. Ours
digs real holes - nothing out there to copy, and nothing to compete with on that axis.

## 4. Per feature: what works with voxels, what is untested, what to test

### 4.1 Ruts

Works (shipped): rut field 0.25 m (look = feel, MP hash identical, robot 7 snaps / 7 runs vs 40 / 8), displaced into
bricks inside craters, soil walls, RUT_BEHIND 3 m (ground never changes under a tyre).

Not tested / open:
| ID | Item | Status | Test / build | Cost |
|---|---|---|---|---|
| R1 | Rut field JIP (late join after a rutted drive + a crater over a rut) | Stage 5 JIP open (ruts README) | DS + late joiner; compare disp hash, floor tiles, op count | 1 session |
| R2 | Raised side ridges | FIRST IN LINE parked idea; field has one value | 2nd field value (max blend, MudRunner PUSH) drawn by patches + bricks | medium |
| R3 | Rut cost ~20 ms per op, mesher run twice per patch (53 %) | LOG 2026-10-15 23:16; shared build reverted (6 % hit) | schedule the collider the frame after its mesh, then share the band build | small-medium |
| R4 | Grass flattening along tracks (AffectFoliage*) | IDA: 20-byte obstacle, 1000/frame, stays ~10 s, recovers ~5 s; never built | client-only, re-submit recent track segments near the camera every ~5 s | small, cheapest visible win |
| R5 | Wheel sink (WheelSetRadiusState) | IDA: radius lowers the body; MUD_RadiusProbe archived, ON=false, never run | probe 1-2 of sinking-deep-dive (all wheels -0.2 m, one side) | 1 session |
| R6 | Mud drag | rolling drag too weak (IDA); .vhcsurf RollingResistance unknown (IDA item 9 unanswered) | probe 3-4 of sinking-deep-dive, or IDA item 9 | small |
| R7 | Track / rut decals on finished ground | IDA item 1: TrackDecalFactory is terrain-only; CreateDecal works on MeshObject but drops on rebuild | CreateDecal with stretch 1, farclip 0.5-1, MatPBRDecal, entity = a patch that stopped changing; re-stamp after rebuilds | 1 session |
| R8 | Ruts heal over time (memory ceiling, F3) | no decay exists; JIP replays every rut op forever | fade old shallow floor samples back to NONE via a server "heal" op per tile; also lets JIP compact | medium |
| R9 | JIP compaction (send rutted tiles as floor data past N ops) | plan "Later" | Chrono-SCM style; our patch JIP already does this for heights | medium |
| R10 | Depth by ground type / wetness | m_sHardWords only; GetCurrentWetness exists (API) | gamemat -> step / cap table, x wetness | small |
| R11 | Tracked vehicles | NEW: ruts read VehicleWheeledSimulation only | decide; a tracked sim would need its own contact source | small to decide |
| R12 | 1.9 SA vehicles still found by FindComponent(VehicleWheeledSimulation)? | NEW | open a 1.9 truck prefab, check the class / a log line on first rut | minutes |
| R13 | Deeper = slipperier (gamemat by depth) | per-geom gamemat proven readable by WheelGetContactMaterial; IDA item 6 (per-triangle material) unanswered | patch: 2nd collider geom for "deep rut" cells with a mud gamemat | medium |
| R14 | Wheel-spin mud spray / dust | effects only | vanilla particle prefab at the wheel when slip x depth > t | small |

### 4.2 Craters

Works: voxel crater, size by charge (FullDepth from the warhead's TNT), HEAT factor, no crater at a vehicle or for
<= 0.3 kg, ground cover for blasts (GroundCovers counts bricks), no undercut (FUNNEL).

Not tested / open:
| ID | Item | Status | Test / build | Cost |
|---|---|---|---|---|
| C1 | Rifle / MG fire through a voxel crater rim (N1) | NEW, never tested | shoot a soldier lying in a voxel crater from outside, low over the rim; and a target behind a 1 m rim. If rounds pass: add the heightfield fix - a downward FireGeo copy of the top section ~1.5 m under it, or close the brick body bottom | 1 session; fix small |
| C2 | Crater shaping on the voxel path (N4) | NEW | pass jitter / lobes / stack cap / slope into the op (fields exist: m_fDepth, m_fB, seed) - the server picks the random values, so it stays deterministic | small-medium |
| C3 | Crater lip steeper than soldier MaxSlope 50 deg | old N2 / item 16, never re-checked on voxels | walk out of a mortar crater; Diag slope view; if stuck, widen crest (crest = depth x 2.6+) or flatten the ellipsoid near the crest | small |
| C4 | Sharper crater rims | DC measured -41 % rms but folds | cheaper idea (INFERENCE): reuse displaced-ruts split + pull with an analytic rim field (2D "rim height" per 0.25 m) - no QEF, no folds on tops | medium |
| C5 | Objects over a new crater (N3) | NEW / old N8 open | after go-live: QueryEntitiesByAABB over the changed box, SetActive on dynamic bodies; for static props / trees: delete, drop, or spawn "destroyed" variant; mines: detonate or drop | small-medium |
| C6 | Crater next to a house (N2) | NEW | mortar 2 m from a house wall; look for a floating house; fix = Hard-style skip for samples inside building boxes (reuse BuildingLock boxes) | small |
| C7 | Scorch / burn decal on the crater | vanilla explosion decals over our ground: never checked (they target the hit entity, which our rebuild replaces right after) | look check; if gone, CreateDecal on the bricks after go-live (they rarely rebuild again) | small |
| C8 | Big-blast go-live 50-100 ms | LOG | split the frame (brick swaps vs patch / skin rebuilds), pre-build the patch side hidden | medium |
| C9 | Crater merge (two hits 1-3 m apart) | heightfield M1 rim bug; voxel rim = density bump added, so a 2nd rim may refill the 1st bowl | look test: 2 mortars 2 m apart | small |
| C10 | Explosives with no crater (PG-7VL/VR, 25 mm HEIT, hand grenades, mines, M112) | old list | decide per item; warhead hook already sees them | design call |
| C11 | Vehicle blast craters | pinned (onVehicle skip) | design call | small |
| C12 | Crater on a road / kept terrain edge | hard columns never dug -> crater cut by a straight road edge | look test; maybe a "broken asphalt" prop ring | small |
| C13 | Ejecta / debris / dust when the ground changes | effects only | vanilla debris + dust prefabs at the crater | small |

### 4.3 Trenches, shovel, fill

Works: plan mode (curved layouts, ghosts, 1 m pieces, 0.25 m layers to 2.5 m), free dig / fill square, voxel ops,
MAX_SITES 20.

Not tested / open:
| ID | Item | Status | Test / build | Cost |
|---|---|---|---|---|
| T1 | Bullets through a trench parapet (N1) | NEW | as C1: soldier crouched in a 1.5 m trench, shooter 50 m away aims at the lip | 1 session |
| T2 | Spoil / parapet: dug soil piled on one side | not built; trench only removes | FillTrench-style raise in a band outside the chosen wall (the planner knows the "enemy" side from the plan direction or a key). Real trenches are half dug, half parapet | medium |
| T3 | Revetment props along the walls (sandbags, planks, fascines) | not built | trench layout came from CSK_SandbagWallLayout - place vanilla prefabs along pieces at layer >= N; hides voxel wall texture stretch, adds real .xob colliders (bullets + navmesh read them) | medium; entity count bounded by MAX_SITES |
| T4 | Fire step / firing ledge | not built | layout option: one side wider at 1.25 m depth | small |
| T5 | Sharper trench lips | DC -48 % rms measured | as C4: displaced lip from a 2D field (the planner's own outline is exact, so the field is analytic) | medium |
| T6 | Trench near a house (N2) | NEW | as C6 | - |
| T7 | Per-player trench-site cap | dropped by the user | - | - |
| T8 | Trench walls collapse under shelling | not built | stability rules R1-R3 (research/2026-10-15-voxel-stability) cover it | medium |
| T9 | Prone soldier in a trench, ragdoll in walls (N9) | old reports, not rechecked on voxels | look test | small |

### 4.4 Tunnels

Works: hold T, capsule every 300 ms, ramps by pitch, walkable, collider, net ops, JIP.

Not tested / open:
| ID | Item | Status | Test / build | Cost |
|---|---|---|---|---|
| U1 | Thin roofs / bridges / floating chunks | research 2026-10-15 stability: dig-time guard + R1-R3 collapse rules; nothing built | dig-time guard first (server, few samples), then R1 + R2 behind a switch, DISP_HASH-style hash per op for MP | small then medium |
| U2 | Tunnel under a house (N2) | NEW | as C6 | - |
| U3 | Shell over a tunnel | no roof rule -> thin roof left | covered by U1 R1 | - |
| U4 | Depth limit 3 m (FLOOR const), DepthBox ignored | NEW note | let FLOOR read MUD_DepthBoxEntity.DepthAt per column at seed time (deterministic: boxes are placed entities) | small |
| U5 | Inside look: light, interior probes, rain, sound | never checked | walk a 10 m tunnel at noon and in rain: too bright? rain inside? reverb? (MatTerrainObject has DisableInteriorProbes; audio "interior" detection unknown) | 1 look session |
| U6 | Client prediction of digs | deferred (needs rollback) | only if MP dig latency is felt | - |
| U7 | Bunker / tunnel-mouth prefabs ("poison" no-draw cells) | papers F1, needs a prefab | op kind "no-draw box" + a vanilla bunker entrance prefab | medium |
| U8 | Column budget: tunnels share MAX_COLUMNS 6000 with craters / trenches | ruts no longer use it (rut field) | log the column count over a long PvP match | test only |
| U9 | Grenade / blast inside a tunnel: cover and crater | GroundCovers counts bricks; a blast inside a tunnel carves a sphere? (Carve is a ground crater: half ellipsoid below the hit + funnel UP to the surface) | INFERENCE: a blast in a tunnel cuts a funnel up through the roof to the sky. Look test; maybe a sphere carve when the hit is under a roof | small |

### 4.5 More (things outside the four)

| ID | Item | Status | Note |
|---|---|---|---|
| M1 | Persistence across server restarts | not built; SCR_PersistenceSystem exists (API) | the op log IS a save format (ruts would be compacted first, R9) |
| M2 | Healing / budget | not built | R8; also craters slowly flattening for very long servers |
| M3 | Puddles / water in craters and ruts after rain | never tried | a flat script mesh at the crater floor with a water-look emat (vanilla water material on a script mesh: unknown, probe) |
| M4 | Footprints | never tried | decals (CreateDecal on patches) or nothing |
| M5 | Helicopter skid / landing marks, wrecks pressing ground | never tried | op 4 style press at contact points |
| M6 | Mud on vehicles | old list | vanilla dirt / wetness material params on the vehicle? unknown |
| M7 | Server-owner settings (on / off, depths, budgets) | F10 old list | product work |
| M8 | GM "repair ground" brush / GM place crater | F11 old list | an op from the GM with the same net path |
| M9 | AI | dead end for navmesh (PROVEN); PvP scope | only route: invisible .xob navmesh proxies - out of scope |
| M10 | Voxel house in MP (N5) | NEW risk | switch ENABLED off for release, or make it server-issued ops like the ground |

## 5. Engine questions still open (answers change options above)

From research/2026-10-15-ida-rut-questions.md (items 6-10 never answered) plus new ones:
| # | Question | Decides |
|---|---|---|
| Q1 | Per-triangle material on CreateTriMesh? (item 6) | R13 without extra geoms |
| Q2 | Trimesh internal-edge handling (item 7; IDA found no btTriangleInfoMap string) | bumps on split ruts for bellies / characters |
| Q3 | RemoveGeom / cheap AddGeom (item 8; G3 measured patch = rebuild cost) | already answered by G3: no gain. Close it |
| Q4 | Does the sim read .vhcsurf RollingResistance / RollingDrag? (item 9) | R6 drag for free |
| Q5 | New terrain natives in 1.9 (item 10) | everything (a height / hole write would change the architecture) |
| Q6 | How does projectile penetration find an exit on a trimesh? (NEW) | why open trimesh lets bullets pass; C1 / T1 fix shape |
| Q7 | Is VehicleWheeledSimulation_SA_B a VehicleWheeledSimulation subclass? (NEW) | R12 |
| Q8 | Does the clutter discontinuity mask clear grass in 0.1-0.35 m ruts? (2-bis) | grass in ruts look |
| Q9 | Is World.CreateDecal registered in the retail game / DS, or only diag builds? (1-ter) | R7 / C7 at all |
| Q10 | Any emat class with parallax / POM / height map (pak scan, like the 2026-10-14 triplanar scan) (NEW) | fake depth on rut / crater decals |

## 6. Suggested test order (user decides; feature freeze still on)

**A. Risk checks - cheap, do first, they protect what is shipped (one or two SP sessions):**
1. C1 + T1 bullets through voxel rim / parapet (N1).
2. C6 / T6 / U2 dig next to and under a house (N2).
3. C5 objects over a fresh crater: a tree, a parked truck, a dropped rifle, a GM-placed vehicle (N3; may explain the
   Lane 3 "vehicle hangs in air" bug).
4. N5 voxel house in MP (or just switch it off for release).
5. R12 / Q7 vehicle sim class on 1.9.
6. C3 walk out of a mortar crater.

**B. Finish what is in flight:** R1 rut field JIP, then Lane 1 stress test (F2), with C2 crater shaping folded in.

**C. Cheap look / feel probes (one session each, no MP risk):** R4 grass flattening, R5 wheel sink, R7 / C7 decals on
finished ground, U5 tunnel inside look, GlobalLayer angle blend on brick walls (voxel-options option 4).

**D. Bigger builds, in this order:** U1 tunnel guard + collapse rules, R2 ridges, T2 spoil parapet + T3 revetment
props, C4 / T5 displaced rims and lips, R8 / R9 healing + JIP compaction, M1 persistence.

## 7. Stale lines in the docs (fix when touched)
- voxel-options-deep-dive option 3 / assessment 5.1 "terrain-look material on voxel tops, never probed": already
  MAT_TOP (N6).
- craters README "re-check old crater items": answered by N4 / C2-C3 / C9 above.
- rut-alternatives section 1 "MAX_COLUMNS ~6-8 km of ruts per match": no longer true with RUT_FIELD true (ruts make
  0 columns, LOG 22:47); the cap now only limits craters, trenches and tunnels (U8).
- IDA item 8 (partial collider updates): answered by engine probe G3 (patching = rebuild cost) - close it.

## Sources
- Our docs as cited above (all under docs/).
- PRLX Trench - https://reforger.armaplatform.com/workshop/69FA76B7D8B6F785-Trench
- ACE Trenches - https://reforger.armaplatform.com/workshop/60EAEA0389DB3CC2
- Breakable ACE Trenches DEV - https://reforger.armaplatform.com/workshop/68EA1410BDF23924
- Trenches in Arma Reforger (forum) - https://forums.bohemia.net/forums/topic/243966-trenches-in-arma-reforger/
- GenericTerrainEntity script API (search snippet only; the page could not be opened from this session) -
  https://community.bistudio.com/wikidata/external-data/arma-reforger/EnfusionScriptAPIPublic/interfaceGenericTerrainEntity.html
- Bullet btRaycastCallback (backface filter flags) - https://www.staff.city.ac.uk/~andrey/INM377/bullet-2.82-html/html/btRaycastCallback_8h_source.html
