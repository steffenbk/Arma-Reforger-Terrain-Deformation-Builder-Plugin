# Mud / deformable ground - handoff

## Now
Arma Reforger (Experimental, engine 192624) mod "Testing plan vech msh":
C:\Users\Steffen\Documents\My Games\ArmaReforgerWorkbench\addons\Testing plan vech msh

**Built and working in-engine (2026-10-07/08):** Scripts/Game/Mud/MUD_MudTileEntity.c on Prefabs/Mud/MudTile.et ({1906CDD6DE59C924}).
- 8 m, 82x82 grid built with MeshObject.Create, draped on terrain +0.12 m. Wheels press ruts + berms.
- Static mesh (flags 0) rebuilt on change, max 10/s. DYNAMIC_VERTEXES shades scaly - do not use.
- Coarse 21x21 collider for soldiers/bullets, holes cut around vehicles (rebuilt 0.15 s).
**In the code, compiles, not checked in play:** m_fActiveDistance 80 m (camera range), dedicated server builds nothing,
explosion craters (SCR_ExplosionAmmoEffect hook), MUD_RutTrailEntity.c + Prefabs/Mud/RutTrail.et (strip idea, parked).
**Known issues:** Mud.emat ({A29AE750B3F1E371}) renders near-white -> tint. Leftover test attrs m_bUseImportedMesh,
m_bFlipWinding, m_bFlipV (all off) -> remove. Far players miss ruts made while away (option 2 offered, not built).
MP unverified: proxy vehicles' WheelHasContact on clients; collider only client-side (server has none).

**2026-10-08 step 1 written, NOT compiled yet:** Scripts/Game/Mud/MUD_GroundEntity.c + Prefabs/Mud/MudGround.et
({00605A3AF910747A}). For an empty world (no terrain). 32 m, 129x129 grid (0.25 m), 4x4 patches of 8 m, each a
spawned GenericEntity with static mesh + static trimesh on EPhysicsLayerPresets.Terrain. Ruts = sink 3 cm per pass
(pass = vertex free of wheels 0.6 s), cap 0.3 m; berms cap 0.08 m. Craters 2.5 m radius, 2 m deep, floor -4 m.
Edit mode shows one flat quad only. Dedicated server builds colliders, no mesh. Deformation local per machine (no sync).
Known: normals at 8 m patch seams use one side only -> possible shading line on slopes across a seam.
- 2026-10-08 IDA (Experimental exe): BaseWorld.GetSurfaceY = sub_140B7FCB0 -> sub_140A016D0; with no terrain it
  returns the default -256.0 (xmmword_141BCCD10, second float). Not tested in-engine.
- 2026-10-08 ArmaReforger.gproj (Experimental install, loose): LayerPreset Terrain = 0x200080 (Terrain|Navmesh).
  Terrain row interacts with VehicleCast, Dynamic, Debris, Ragdoll, CharNoCollide, Camera, CharacterAI,
  VehicleSimple, Weapon; Character and Projectile rows list Terrain. Shipped PBR_MASTER_GridPlane.ent uses preset Terrain.
- 2026-10-08 pak: SCR_SpawnPoint -> SCR_WorldTools.FindEmptyTerrainPosition traces 10 m down from the point and takes
  max(hit, GetSurfaceY). m_bForcedPosition skips that and spawns at the point's origin.

**NEXT - "mesh battlefield" (user's direction, PvP only, no AI/navmesh):**
Flat deformable plane chunks (~32 m) over terrain LOWERED in the World Editor; plane level with surrounding ground,
depth limit = gap to lowered terrain; skirt only at the zone's outer rim. Vehicles + soldiers both collide with the mesh.
Coarse when untouched, fine where hit. Server-authoritative deformation events (crater pos/radius, trench line) + JIP list.
Losses: no grass/clutter, plainer texturing, needs LODs.
**Step 1:** one plane in a 1 m pit, vehicle-solid finer collider (~41x41), ruts = sink per wheel pass (capped),
one truck across -> check for jitter/bumps when the collider is rebuilt under it.
Later: mud feel (WheelSetRollingDrag / friction), grass flattening (GenericTerrainEntity.AffectFoliage*).

KB: ~/.claude/arma-knowledge/patterns/Scripting_And_Core/runtime-procedural-mesh.md

## Decisions / gotchas
- 2026-10-07, IDA on Experimental exe: MeshObject natives registered in sub_140C35020: Create, UpdateVerts, UpdateIndices, GetNumGeoms, HasValidMask, HasLayerMask.
- UpdateVerts (sub_140C38540) writes position + UV only (no normals/colors/weights); UVs packed as int16 via uv*65534-32767 clamped -> UVs must be 0..1 (no tiling via UV). No physics/collider update in that path.
- Shipped pattern (pak, Experimental): SCR_TerrainMeshEntity / SCR_GenericBoxEntity / SCR_Shape use MeshObject.Create(1, numVerts[], numIndices[], materials[], 0) -> UpdateVerts -> UpdateIndices -> SetObject(mesh,""). Static arrays.
- Shipped SCR_TerrainMeshEntity has int-division bug in UVs (x / resolution1); use float divisor.
- Vehicles use VehicleWheeledSimulation (UAZ469 inspected); API has WheelCount/WheelHasContact/WheelGetContactPosition.
- PROVEN in-engine 2026-10-07: script class members max 256KB ("class too large" compile error) - split big static arrays into helper classes.
- PROVEN in-engine: MeshObject.Create returns null if any material fails to load (empty string included). Matches IDA: material-load loop breaks out before creating.
- PROVEN in-engine: Create with flags 0 = static; later UpdateVerts not drawn. MeshObjectFlags (IDA sub_1409A0F00): CUSTOM=4, DYNAMIC_VERTEXES=16, DUMMY_TANGENTS=32, UPDATE_BOUNDBOX=128. DYNAMIC_VERTEXES|UPDATE_BOUNDBOX -> updates visible.
- Working test material GUID (from shipped script SCR_RefPreviewEntity): {D0126AF0E6A27141}Common/Materials/Colors/colorRed.emat.
- Engine .pak does not contain .meta files (pakread get *.emat.meta -> KeyError); find GUIDs from shipped scripts/prefabs instead.
- PROVEN in-engine 2026-10-07: imported .xob ToMeshObject -> GetNumGeoms 0, UpdateVerts no-op. Only MeshObject.Create meshes deform.
- PROVEN in-engine 2026-10-07: DYNAMIC_VERTEXES mesh shades scaly; flags 0 mesh shades fine (same mesh/material). Workaround: static + rebuild on change.
- PROVEN in-engine 2026-10-07: static mesh + throttled full rebuild (Create/UpdateIndices/UpdateVerts/SetObject at 10/s) shows ruts with correct shading.
- PROVEN in-engine 2026-10-08: a Character|Projectile static trimesh collider (PhysicsGeom.CreateTriMesh + Physics.CreateStaticEx) is solid for soldiers AND blocks vehicles. Vehicle-hole workaround (rebuild every 0.15 s, hole = bounds circle +1.5 m + speed*0.6) works; lags at high speed.
- 2026-10-08 IDA: no script-callable runtime terrain height edit. GenericTerrainEntity script API is read-only + AffectFoliage* (grass push). "AdjustHeightMap" = editor-time building property only.
- 2026-10-08 pak: explosion hook = static SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect(), pos = outMat[0]; only 13 shipped ammo prefabs carry the effect (no hand grenades).
- 2026-10-08 IDA (STABLE Tools Workbench exe, Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe): script PhysicsGeom has only
  CreateBox/Sphere/Capsule/Cylinder/TriMesh (registrar sub_14242D450) -> no script heightfield collider (EnfHeightfieldTerrainShape is terrain-only).
  Physics.AddGeom(name, geom, frame[4], material, layer) exists -> can add a geom to an existing body.
  Physics.UpdateGeometries (impl sub_142029690) rebuilds from the entity's VObject physics data and returns early if unchanged;
  inference (not proven): useless for MeshObject.Create meshes, which carry no physics geometry.
- 2026-10-08 IDA (STABLE Tools Workbench exe), collider rebuild cost: physics = Bullet. PhysicsGeom.CreateTriMesh (native sub_142430C60
  -> backend sub_14228BE30, in vtable 0x1432753c8; signature matches, vtable slot offset not checked) copies verts,
  computes bounds, copies indices as 16-bit, then ALWAYS builds a new quantized BVH (sub_1426587F0(...,1,1) ->
  buildOptimizedBvh sub_142658740). No refit/in-place update reachable from script. Cost ~ O(tris log tris) per call;
  our 8 m patch = 1089 verts / 2048 tris. Inference (not proven): >65535 verts per trimesh would wrap indices
  (no check seen on this path; the 0xFFFF "Trimesh collider exceeded" check is in the import code sub_140332D10).
  Physics.CreateStaticEx (sub_142430670) only copies the geom list and calls the world's create-body - cheap.
  Real ms: read the "patch colliders (N ms total)" log line in play.
- PROVEN in-engine 2026-10-08 07:21-07:24 (Experimental WB 192624, MpTest subscene, MudGround touching terrain, M923A1):
  tires press visible ruts, mortar crater forms, truck collides with the crater rim. Log: "patch colliders" peak
  40 colliders / 29 ms per 2 s (~0.7 ms each, ~15 ms/s worst), typical 20-30 / 13-20 ms. Mesh build time NOT logged.
  Bug seen: PhysicsGeomDef material "material/default" -> "Unknown resource type, missing extension!" 691x (one per
  collider build). Fixed 07:3x: attribute m_GameMaterial = {C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat
  (GUID from Experimental pak, 60 refs e.g. FlowerpotStreet_01_Soil.emat). PROVEN 07:34: 0 resource errors,
  wheel dust visible; colliders now 32-35 per 2 s in 7-14 ms (~0.2-0.4 ms each - error logging was part of the old cost).
  MUD_MudTileEntity still uses "material/default".
- 2026-10-08 07:5x MUD_GroundEntity: added (NOT compiled yet) m_bFollowTerrain (base = GetSurfaceY per vertex + m_fTerrainLift,
  stored in new MUD_GroundBase; deformation m_aH stays relative to it; GetSurfaceY <= -255 -> flat), m_bAutoGameMaterial
  (per patch: TraceFlags.WORLD trace like shipped SCR_MineAnimationComponent, param.SurfaceProps.GetResourceName(),
  fallback m_GameMaterial). Debug prints "on terrain" + each patch's game material.
  New Prefabs/Mud/MudGround_TerrainLook.et ({1F5DC58EDF919FB6}, child of MudGround) with m_Material =
  {D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat (user's copy of shipped default_MatTerrainObj.emat).
  Known limit: terrain collider + terrain render sit above anything dug below the terrain -> lower terrain under the
  zone by X and set lift X. check_guid_refs: new refs OK; DirtCover_01.emat flagged unknown_path but it loads in play.
- 2026-10-08 07:5x user: follow-terrain works on a slope (compiled + seen in play). Then added (NOT compiled) pad profile
  replacing m_fTerrainLift: PadLift = -m_fEdgeSink + (m_fPadHeight + m_fEdgeSink) * smoothstep(x) * smoothstep(z) over
  m_fEdgeFalloff band; defaults pad 0.4 m, sink 0.15 m, falloff 5 m. Rut defaults raised: step 0.05, max depth 0.35.
- 2026-10-08 07:57 in play (TerrainLook prefab on grass terrain): auto gamemat WORKS (all 16 patches
  {A5388C69AEDCBF4A}Common/Materials/Game/grass_lush.gamemat), pad edges hide under terrain. Ruts NOT visible to user,
  yet log shows 30-37 patch rebuilds per 2 s while driving -> heights do change. Suspect MatTerrainObject normal blend
  (terrain normals override ours). 08:0x: added NormalBlend* = 0 values (copied from shipped
  default_MatTerrainObj_TerrShadows_disabledclutter.emat) to default_MatTerrainObj1.emat. 08:01 user: ruts visible on
  TerrainLook, "just subtle". Unclear if the NormalBlend change helped or they were always there; no A/B done.
- 2026-10-08 08:03 user: suspension jitters a lot driving in a made rut. Hypotheses: A = collider destroy+create every
  0.2 s + 5 cm steps under the wheel; B = rut shape (rut radius 0.25 m = grid step -> one-vertex V notch).
  Added debug attr m_fFreezeAfter (EOnFrame returns after N s, logs "FROZEN"). Test: make ruts, drive them frozen.
- 2026-10-08 08:08 user (in play): jitter = jagged/spiky grooves (cause B, rut shape). Fix written (NOT compiled):
  collider built from FillPatchSmooth = binomial blur of the deformation only (m_iColliderSmooth, default 2 = 5x5,
  1.25 m), base/terrain unblurred, reads full grid so seams match; mesh keeps raw detail. Blur time is added into the
  "patch colliders (N ms)" debug number (25 samples/vertex in script - watch it; separable blur if too slow).
- 2026-10-08 08:12 MEASURED in play: whole-patch 5x5 blur in script per collider build -> ~7.5 ms per collider
  (36 colliders = 279 ms / 2 s), user felt the hitching; jitter mostly fixed. 08:1x rewrite (NOT compiled): persistent
  m_aSmooth grid in MUD_GroundBase, re-blurred only around each change (UpdateSmooth from MarkDirty, range + r);
  FillPatchSmooth just copies. Debug line now: meshes (ms), colliders (ms), smoothing ms.
- 2026-10-08 14:3x perf (log logs_2026-10-08_12-52-21, one ground driven, Exp 192624): while driving, per 10 s
  ~280 patch meshes 184 ms + ~160 colliders 240 ms + smoothing 387-418 ms = ~80 ms/s, all landing on the
  0.1/0.2 s tick frames (~15 ms spike every 0.2 s) -> felt as dips. Fix (NOT compiled): UpdateSmooth separable
  (same result, 10 taps not 25 at r 2), EOnFrame builds at most 1 mesh + 1 collider per frame per ground,
  oldest first, per-patch interval (m_fMeshAt/m_fColAt). Backup %TEMP%\MUD_GroundEntity.c.perf.bak.
  The 4-ground 14:25 run logged no driving stats. Wheel Press cost per frame is not measured.
