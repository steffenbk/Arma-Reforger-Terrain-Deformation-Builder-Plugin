# Mud ground: proven gotchas (moved from .remember/remember.md 2026-10-11)

Proven in-engine / measured facts for the mud system. Newest first. Add new ones here, not in the handoff.

- PROVEN 2026-10-10 23:49 (user screenshot): agent Shape Capsule is always upright - useless for prone. One movement shape per stance; prone Box via Character_Base.et override: 1.2 better than vanilla 1.8; FINAL 0.6x0.4x0.6 = smooth crawl, no head clip, old cosine craters fine (user 2026-10-11 00:19 + 0 prone fall lines). Crater shape was not the key.
- PROVEN 2026-10-10 (log + pak Exp 192624): prone collider is a BOX 0.6x0.4x1.8 (Character_Base.et ColliderProne); on crater walls it sinks into our thin trimesh and gets shoved through (no push-back like terrain). Script cover (HijackDamageHandling + ENTS trace, front-face normal guard) stops fragments through our ground.
- PROVEN 2026-10-10 23:02-23:09 (WB play, Exp): blast FRAGMENTATION damage passes through our patch ground (5-10 m, up to 542 dmg); putting the surface trimesh on FireGeo did NOT stop it. A TraceFlags.WORLD-only trace (Projectile mask) misses our runtime-spawned patch entities; ENTS hits them.
- PROVEN 2026-10-10 22:53 (play, Exp): SCR_CharacterDamageManagerComponent.OnDamage gets damageSource NULL for EXPLOSIVE and FRAGMENTATION damage; fragment impactVelocity ~1e6 m/s (instant). Every bullet hit spawns a bare Warhead_Base (pak: Ammo_Bullet_Base EffectPrefab).
- PROVEN 2026-10-10 22:10 (F7, Exp): 'modded class ExplosionDamageContainer' = compile error 'Engine class cannot be modded'. Hook explosions via Warhead_Base.et override + script component instead.
- MEASURED 2026-10-10 (IDA ArmaReforgerSteamDiag as loaded + pak Stable): blast trace (ExplosionDamageTrace, flags ENTS|WORLD|ALL_CONTACTS|0x80) counts a hit as cover if entity is GenericTerrainEntity OR gamemat StopExplosionTrace (engine default "1"). All 32 ground gamemats resolve to 1 (only vegetation/wood/metal/glass... set 0). Soldier MaxSlope default 50.0, "Move max slope angle" 60.0, not overridden in Character_Base.et.
- PROVEN 2026-10-10 (Exp 192624, WB log + in-game): a world finds its terrain by LOCATION, <dir of .ent owning the terrain entity>/<entity name>/. Copied world = own tiles. GM layer EBIN parent is GUID-only: after a copy RESTART Workbench or the game loads no world (SCR_MapLocator null crash).
- Game FileIO opens addon files only by extension: .c/.conf/.et yes, .bin/.json/.txt/.ttile/.ent no (in-engine 2026-10-10, KB serialization.md).
- Arland .ttile HGHT -> world height = u16 / 32 - 204.781 m, x fastest, no flips (fit of 2000 traced samples, 2026-10-10 Exp 192624). Peer-play logs live in ~/Documents/My Games/server|PeerPlugin1/logs.
- MEASURED 2026-10-10 (IDA, ArmaReforgerSteamDiag Exp 192624): wheeled vehicles use NwkCarMovementComponent (Wheeled_Base.et pak). Server move check limits: position speed/300 (max 1.5, <0.001 -> 1e-6), angle 0.003 rad, speed 0.3 m/s, spin 0.25. Script SetAllowance overrides per vehicle.
- PROVEN in-engine 2026-10-10: MP reverse stutter over ruts = server corrections (client-only jumps); SetAllowance 15 cm cuts max snap 0.61 -> 0.18 m.
- PROVEN 2026-10-10: a 3rd/2nd submesh 'skirt' (528 verts, 1536 idx, same material, double-sided) in the fine patch MeshObject crashed Workbench at the first rut. Reverted.
- PROVEN 03:02 (log): holed tiles at worlds/Arland/Terrain/.Data hit EVERY Arland world (vanilla GM_Arland got holes, no skin). Peer tool: a 2nd server start fails 'Unable to start replication' (port 2001 busy) - ignore that log.
- PROVEN 2026-10-10 01:38-01:47 (Exp 192624): a CR inside a .meta GUID ("{C015...71<CR>}", from bash `read` of Windows python output) -> 'ResourceDB: invalid GUID' + 'Wrong GUID ... inherited-name', entity loads without its prefab components (rpl 0). Strip CR when generating GUIDs. High first hex digit is fine ({DAED...} loads).
- PROVEN 2026-10-10 01:58 (Exp 192624, our holed Arland): bald grass squares are NOT a height issue - skin +5 mm above heightmap still bald.
- PROVEN (CSK 2026-10-04; here 2026-10-10 00:57): a mod's Configs/System/chimeraInputCommon.conf only works when its .meta carries the VANILLA GUID {795184CF9AD764DB} (override + merge, as CSK). register_in_workbench gives it a NEW GUID -> file ignored, LMB goes to vanilla fire (shovel -> gun, plan mode ends). Then restart Workbench fully.
- PROVEN 2026-10-12 20:21 (Exp 192624, WB host, holed Arland x1112-1124 z2454-2466): AIWorld.RequestNavmeshRebuild over a shovel hole -> log "Navmesh build: Could not prepare erode" x2 + "Tile doesn't have navmesh - number of contours = 0" x2; navmesh diag shows the whole tile EMPTY (neighbours keep the baked mesh). The generator does not see our script ground (patch / skin bodies, layer Terrain|Foliage) over terrain holes. Any rebuild there (ours or vanilla GM placing an object) deletes AI navmesh. Cause of the miss not yet probed.
  PROVEN 2026-10-12 ~20:40 control (user, GM sandbag): a vanilla GM sandbag on our holed grass ALSO empties the tiles (no mesh even around the sandbag); a sandbag on the road (kept terrain) rebuilds fine with cut-out + cover points. The rebuilt mesh ends in a 2 m staircase edge = the kept/holed heightmap cell line. So the generator walks only the unholed heightmap + prefab objects; our script ground is invisible to it although its layer has the Soldiers PhysicsInteractionLayer bit (0x200000, ArmaReforger.gproj; BTRlike 0xa00000). Open: does it see ANY script-made static trimesh (probe on kept terrain)?
  PROVEN 2026-10-12 20:4x (MUD_NavProbe, runway x999-1013 z2635, kept terrain, user screenshot): 4 script static bodies (CreateStaticEx, no VObject) - trimesh plate + PhysicsGeom box, each on Terrain|Foliage and on Building 0xA00002 - rebuild ran clean (4x "Finished Modification Job", no errors) and the navmesh stays FLAT through all four: no cut-out, no raised patch, no cover points. The generator ignores script physics bodies without a mesh object, whatever the layer. Untested: script body WITH a MeshObject (our real patches have one on render machines).
  MEASURED 2026-10-12 ~21:0x (same probe + E trimesh plate + MeshObject (= our ground recipe), F box + MeshObject, runway x989-1013 z2655-2663): 3 s after the rebuild NavmeshWorldComponent.GetReachablePoint(centre, 0.5 m) found navmesh within 0.5 m of EVERY body centre on all 3 navmesh components (dy +0.09, LowRes +0.48 = same as the open-tarmac control and as before the rebuild). A 3 x 3 m cut-out would put the nearest mesh >= 1.5 m away; a walkable top would give dy ~+1. => script static bodies are ignored by the navmesh generator even with a MeshObject. GetReachablePoint returns a RANDOM point within the distance (4 m results scatter 0.02-3.97 m), not the nearest. No positive control (vanilla prefab) was run through this check.
- IDA 2026-10-12 (ArmaReforgerSteamDiag.exe, Experimental): WHY the navmesh generator ignores script bodies. Tile build sub_141216DF0 -> entity gather sub_141218650: for each entity in the tile it takes the entity's VObject (entity+0x50), its loaded mesh resource (sub_140AB66C0, type-checked), and reads the COLLISION GEOMS BAKED INTO THAT MODEL (resource+336); each geom is kept if (navmesh project layer mask & geom layer) != 0, then rasterized (sub_14121A720, 'NavMesh: Missing triangulation of Bullet geometry'). The live physics body (Physics.CreateStatic/Ex) is never read. So: no .xob with colliders = invisible to the navmesh. A MeshObject.Create mesh has no colliders, which matches probe E/F. The sandbag counts because Sandbag_01_long.xob carries its own collider. Only way to put something in the navmesh from script: spawn an entity whose .xob has a collider on a Navmesh layer.
- IDA 2026-10-12: navmesh TERRAIN step (sub_141217DA0) walks the terrain grid; any cell where the hole test (sub_1410210F0) is true is SKIPPED (no triangles) - that is why rebuilt tiles over our holed map come out empty. Other cells take heights straight from the terrain's live uint16 heightmap (sub_140A30820, terrain+48). Script can't write that heightmap or the holes at runtime: GenericTerrainEntity script API (core GameLib generated/Entities/GenericTerrainEntity.c) is read-only (TryGetHeightTC, WorldToTerrCoord, tile info, AffectFoliage*). Height/hole editing exists only as Workbench TerrainToolDesc_* tools.
- PROVEN 2026-10-12 (Exp 192624 compile): `debug` is an Enforce keyword too - a parameter named `debug` gives "Expected name, not a keyword 'debug'".
- PROVEN 2026-10-10 00:24 (Exp 192624 compile): `owned` is an Enforce keyword - a local named `owned` gives "Broken expression (missing ';'?)". Same class as `out`.
- PROVEN 2026-10-09 22:16 (Exp 192624): shovel height check vs DUG surface refuses from the rim of a >1.5 m hole (feet 49.77, floor ~48.1). Check feet against the undug surface too.
- PROVEN 2026-10-09 13:12-13:20 (Exp 192624): at play stop our entity dtors run while the world's entities are still
  alive (107/107 patches). Deleting the entity then geom.Destroy() = heap corruption 0xc0000374 on play stop; skipping the
  free = no crash; Physics.Destroy (fresh GetPhysics) -> geom.Destroy -> delete entity = no crash. Skin dtor saw
  ~213 of 1697 chunk entities already gone.
- Enforce 2026-10-09: `Math.Max(a, b & MASK)` = compile error "Unknown operator '&'" - do the & in its own int first.
- MEASURED 2026-10-09 12:54 (Exp 192624, 50-call bench on a deformed 33x33 patch): FillIndices 1.36 ms, BuildMesh 2.26,
  BuildCollider 4.6 (3 geoms), full-patch smoothing 2.8, CreateTriMesh 2048 tris 0.28 / 512 tris 0.06, MeshObject.Create ~0,
  UpdateIndices+UpdateVerts 0.24. Script loops dominate, not engine calls. 'Resources are leaking' assertion on script
  reload is in every crash.log since 2026-10-08 21:30 - not from the geom fix.
- MEASURED 2026-10-09 12:21-12:23 (Exp 192624, MemoryAllocationKB probe): rebuilding a Physics.CreateStaticEx body from
  fresh PhysicsGeom.CreateTriMesh keeps ~272 KB per rebuild after old Physics.Destroy(); MeshObject.Create rebuild with
  the old Resource dropped keeps ~0. PhysicsGeom is global_pointer with its own Destroy(); pak never calls it.
  PROVEN 12:29-12:30 A/B: geom.Destroy() AFTER Physics.Destroy() frees it (~0 KB/build vs ~270), 1000+ calls, no crash.
- PROVEN 2026-10-09 11:35-11:39 (Exp 192624, Arland holed, probe): AI group, runtime soldier, killed house (8/9 hole
  cells), 14 explosions -> nothing snapped to the hole floor (lowest y 31.4). Origin ">1 m under heightmap" is NOT a
  fall signal: kept cells show it as often (10 vs 15 entities, to -10.2 m). SCR_TerrainHelper.GetTerrainY = -256 on holes.
  Get-in to M923 from the skin fails (get-out works), MUD_VehicleAccess hook never fired.
- PROVEN 2026-10-09 (Exp pak): 72 of 5627 scripts/Game/*.c (134 calls) read GetSurfaceY / SCR_TerrainHelper.GetTerrainY
  (falls back to GetSurfaceY without a trace) -> -256 on hole cells. Biggest untested whole-map-holes risk.
- 2026-10-12 pak (Exp) + user report: GM symptoms of the -256: placing in TERRAIN mode = SCR_TerrainHelper.GetTerrainY(no trace) -> object spawns under the map; SCR_TerrainCollisionManualCameraComponent clamps to GetSurfaceY -> camera passes through; SCR_ATLManualCameraComponent uses max(GetSurfaceY,0) -> over holes ATL is from sea level, crossing kept terrain the camera jumps up. Fix MUD_VanillaHoleFix.c (modded copies, GetSurfaceY -> MUD_Surface.SafeY) - user-tested OK 2026-10-12 (placing, camera stop incl. trench floor, no jump, lightning). SafeY must trace fine patches FIRST: a plain 'first of ours' vertical trace hit something 1.2-1.5 m above a trench floor (probe 21:48).
- PROVEN 2026-10-12 21:18 (WB log): a `modded class` of a [BaseContainerProps] class (prefab/config container, e.g. SCR_*ManualCameraComponent) WITHOUT repeating [BaseContainerProps(...)] compiles fine but the prefab load logs 'WORLD (E): Unknown class ...' and DROPS that component silently. Always repeat the attribute on the modded class.
- MEASURED 2026-10-12 21:40 (lightning probe log): with `override static GetTerrainNormal` in modded SCR_TerrainHelper, vanilla SnapToTerrain (-> GetTerrainBasis -> GetTerrainNormal, unqualified calls) still snapped to y 0 on our ground (vanilla path) while outside qualified calls to GetTerrainY used ours. Inference: unqualified static calls inside a vanilla class stay bound to vanilla; override the whole chain and call qualified. CONFIRMED 21:44: after overriding the chain with qualified calls the lightning snapped to our ground.
- 2026-10-09: a spread (per-frame) skin build once took 90663 heights from RoadEntity tops; build at init never does.
- PROVEN 2026-10-08 23:48 (Exp 192624, Arland probe): roads are WORLD geometry - ENTS traces (any mask) miss them,
  a WORLD trace hits with TraceEnt RoadEntity; AABB query does not return the RoadEntity. FileHandle is Managed:
  store as `ref FileHandle` or it dies after the opening function. .ttile odd-length chunks (GCTD 43/289) have
  NO IFF pad byte (28 Arland tiles) - ttile_holes.py fixed + test.
- PROVEN 2026-10-09 00:05 (Exp 192624): full-Arland coarse skin over 1396378 hole cells builds in EOnInit in
  7.6 s (heights 5.3 / meshes 0.4 / colliders 1.9 s), +262 MB, before physics: placed M923 did not drop.
  WORLD trace with a GenericTerrainEntity-only filter: 0 non-terrain hits, 0 misses. Enforce: no '%' inside a
  float expression (compile error) - do int maths in its own int first.
- PROVEN 2026-10-09 00:12 (Exp 192624): script-made static bodies (Physics.CreateStaticEx on spawned GenericEntity)
  are partly WORLD geometry: an ENTS-only trace hit 940/2000 skin cells, ENTS|WORLD hits the rest at exact
  height. Trace our ground with ENTS|WORLD and check TraceEnt.
- PROVEN 2026-10-09 08:26 (Exp 192624): a GenericEntity spawned first and given a MeshObject later (SetObject) is culled
  by stale bounds (ground vanished on camera turn); ent.Update() after SetObject fixes it (shipped pattern:
  SCR_DestructionBaseHandler SetObject then Update).
- PROVEN in play 2026-10-08 22:49-23:03 (Exp 192624, Arland): addon file worlds/Arland/Terrain/.Data/Terrain_<i>.ttile
  overrides the pak tile; holes load (census 25 then 41 cells, user fell through at x1280-1288 z2200-2208); no .meta
  needed; WB publish-pack includes it byte-identical. HOLE block coords are terrain-GLOBAL (not tile-local): blocks
  written into tiles 1/16/18 landed at their written cells. Reopening the world picks up a rewritten tile (no restart).
  Packed pak also carries .remember/backups/*.ttile + test maps - exclude before shipping.
- MEASURED 2026-10-08 (Exp pak): worlds/Arland/Terrain/.Data = 256 .ttile + 765 .edds, no .meta files. Tiles 0/1/16/255:
  VERS 9, HGHT 33282 (129x129 u16 + 2), BERR 576, LRS2, TMAT; no HOLE chunk. Tile order + local/global block coords unknown.
- PROVEN in play 2026-10-08 21:50-21:59 (Exp 192624, NEw map, user screenshots): grass CLUTTER shows on our ground over
  a terrain hole only if the ground sits >= 0.001 m above the heightmap (exactly coplanar = no clutter). At +0.3 m the
  clutter rides on top of our tile. Mortar craters in it are clean (no grass in the pit, grass on the rim).
  Inference (not proven): clutter is planted via the engine's ObjectsHeightMap (log "ObjectsHeightMap::Build").
  Ground in a hole must take height from WORLD trace (GetSurfaceY = -256 there; ground went "on terrain 0").
- MEASURED 2026-10-08 21:08 (Exp 192624, NEw map Terrain_0.ttile, decodes to exactly 534/534 bytes): .ttile = IFF FORM TERR,
  chunk sizes big-endian. HOLE chunk = u32 LE block count, u16 LE bytes per block (128), then per block: u16 bx, u16 bz
  (32x32-cell blocks), 32 rows x u32 LE, bit c of row r = cell (bx*32+c, bz*32+r) is a hole. Only blocks with holes
  are listed. BLCK chunk = u32 count + u16 bx,u16 bz per disabled block (does NOT read as hole).
  PROVEN in play 21:10: decoded cells == GetSurfaceY scan (588/588), cell x = world X/2, cell z = world Z/2 (no swap).
  PROVEN in play 21:19: holes WRITTEN by a Python tool (scratchpad ttile_holes.py, world closed in WB while writing)
  load in game: 16 new cells at world 200-208 read as holes (604 total), no load errors.
- PROVEN in play 2026-10-08 20:50 (Exp 192624): a made-up proto (SetHoleAt) added to an addon copy of
  Scripts/GameLib/generated/Entities/GenericTerrainEntity.c compiles with warning "Method not linked" and the call
  is a silent no-op. Script cannot add natives; exe registers exactly the 13 terrain natives in that file.
  Terrain tile = 128 cells (test map = 1 tile). Block Enable/Disable test 20:39 wiped painted holes (cause unknown).
- PROVEN in play 2026-10-08 20:01 (Exp 192624, NEw map, 0.5 m scan): terrain holes are whole heightmap cells - smallest
  2x2 m; edges on even world coords (holes x30-34 z100-102 and x34-36 z92-94). MEASURED .terr HEAD: test map 129x129
  verts 2.0 m; Arland (pak) 2049x2049, 2.0 m. Our 0.25 m grid snapped to 2 m puts hole edges on our vertex lines.
- 2026-10-08 (Exp 192624, proven from saved layer): wb_entity_create with a path but no {GUID} sometimes saves
  the entity with NO prefab link (FogHaze_Arland, SCR_CameraManager did; Lighting/EnvProbe/TimeAndWeather linked).
  Always pass {GUID}path, then check the saved .layer. Creating by bare class name (no prefab) did NOT stick at all (19:27).
- PROVEN in play 2026-10-08 10:50 (Exp 192624): a script static collider on preset Terrain makes vehicle get in/out
  "Obstructed"; adding EPhysicsLayerDefs.Foliage (0x40000 = ExcludePhysicsLayer in CompartmentAccess_base.conf) fixes it.
- 2026-10-15 (WB log 20-14-47, 21:13-21:15): get in / out blocked in a voxel rut although 'mud hidden from traces' ran 6x:
  HideFromTraces never covered voxel bricks (MUD_VoxelSubEntity). EVERY new ground entity type must join
  MUD_GroundEntity.HideFromTraces (fix MUD_VoxelProbe.SetTraceHidden, v092 = before; user-tested OK 21:2x).
- PROVEN DS 2026-10-19 23:35 (logs both sides): on a DS our modded SCR_GetIn/GetOutAction.PerformAction runs on the
  CLIENT only - the server logged 'PerformActions' + door states 3x but not one 'MUD vehicle access' line (TEMP
  LogStart prints on every call). So a server-side hide gap is NOT the get out cause (OnMud always-true change v244).
  Get out still fails once and works on the 2nd try (both DS sessions). Get in tries that did not start never
  reached the server, and the player ended ~4 m under the ground (server editor log y 33.3, ground ~37.6). Open:
  when the fall happens (TEMP fall probe in MUD_VehicleAccess, v245 = before).
- PROVEN DS 2026-10-19 23:41 (fall probe, client log): the fall is a get in that STARTS (IsGettingIn 1) and is then
  cancelled while our ground is hidden: the soldier is put 4.5 m down in < 0.2 s (y 37.8 -> 33.26, our ground 37.77)
  and stays under it after the unhide. 1st get in worked, 2nd-4th fell. Fix: MUD_VehicleAccess.Rescue (Teleport back
  on our ground once traceable; v246 = with it, NOT COMPILED). Why the get in cancels: open.
- PROVEN in play 2026-10-08 09:26 (Exp 192624, dedicated): open script trimesh on Terrain preset lets bullets pass
  (rim -> next crater). Adding a lowered, downward-facing trimesh copy on EPhysicsLayerPresets.FireGeo stops them.
  FireGeo compiles as an enum value.
- MEASURED 08:17: re-blurring on every wheel press (~6 wheels x 90 fps, ~11x11 verts x 25 taps) = 150-330 ms per 2 s
  -> FPS 90+ to 59. Batch smoothing per patch per collider rebuild instead.
- PROVEN 08:01: MatTerrainObject emat on a MeshObject.Create mesh renders terrain look and still shows ruts (subtle).
- PROVEN in play 2026-10-08 (Exp 192624): TraceFlags.WORLD trace -> param.SurfaceProps.GetResourceName() returns the
  terrain's gamemat (grass_lush {A5388C69AEDCBF4A}). Rut jitter = jagged one-vertex grooves; blurred collider fixes most of it.
- MEASURED 08:12: 5x5 blur over a whole 33x33 patch in script = ~7.5 ms per collider build (vs ~0.3 ms without) -> hitches.
- PROVEN in-engine 2026-10-08 07:34 (Exp 192624): PhysicsGeomDef material "material/default" logs "Unknown resource type,
  missing extension!" per build; a real .gamemat ({C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat) fixes it + gives wheel dust.
- 2026-10-08 IDA (Experimental exe): BaseWorld.GetSurfaceY with no terrain returns -256.0 (sub_140A016D0 default).
  PROVEN in play 19:58: inside a painted terrain hole GetSurfaceY also returns -256.
- 2026-10-08 ArmaReforger.gproj (Experimental install): LayerPreset Terrain = 0x200080 (Terrain|Navmesh); Terrain row
  interacts with VehicleCast/VehicleSimple/CharacterAI etc.; Character and Projectile rows include Terrain.
- IDA 2026-10-13 (ArmaReforgerSteamDiag.exe Experimental): physics is enf::BulletWorld. The gproj matrix is loaded by
  sub_140B925B0 -> sub_141715010, which ORs each row into BOTH directions (matrix is symmetric at runtime). Pair filter
  sub_140B96730: two bodies collide if any layer bit of one has a table row overlapping the other's mask.
- MEASURED 2026-10-13 (Experimental gproj matrix, symmetric): CharCollide (0x80000) collides only with Character,
  CharacterAI and the Wheel preset (0x104000, via its CharacterAI bit). FireGeometry (0x4000) only with Projectile.
  Wheel suspension raycast = RaycastLayer "VehicleCast" (VehicleWheeledSimulation_Base.ct + Tracked, extract 2026-09-26;
  engine attr "Wheel raycast physics layer preset", stored at sim+6644). VehicleCast hits only Default, Static, Terrain
  - and ALL three are also solid for Character + CharacterAI. So: a soldier/bullet-only collider (CharCollide|FireGeo)
  exists, but NO vehicle-only layer exists for wheel raycasts: with a collider split, soldiers touch both and stand on
  the higher one (raw lips, blurred pit/trench floors). GroundCovers traces with Projectile -> sees FireGeo + Terrain.
  Unproven: whether Wheel-preset geoms really contact static ground (they already can hit Terrain the same way).
- 2026-10-08 pak: SCR_SpawnPoint uses FindEmptyTerrainPosition (10 m trace down, max with GetSurfaceY);
  m_bForcedPosition spawns exactly at the point's origin.
- 2026-10-08 pak: shipped no-terrain world worlds/Showcase/PBR_MASTER.ent = GenericWorldEntity(SkyPreset) + light + PP + camera manager.
- PROVEN 2026-10-08 08:44 (Exp 192624, dedicated): a placed entity whose prefab GUID is missing from the addon's
  resourceDatabase.rdb loads by path but is NOT replicated ("invalid prefab resource GUID"), so its Rpc calls fail.
  Files + .meta made outside Workbench were missing from the rdb; a Workbench restart added them (fixed 08:56).
- PROVEN in play 2026-10-08 13:2x (Exp 192624): native vehicle get in/out check (CanGetInCompartmentViaDoor
  sub_1401D3120 -> sub_1401D74F0 -> VehicleActionObstructionTrace sub_1408BD7E0, flags ENTS|ANY_CONTACT, no WORLD)
  sweeps the char stance-0 AABB whose bottom = feet (Character_Base ColliderStand Offset Y 0), so it grazes any
  script entity ground the soldier stands on (log: ok at +0.06 mm, blocked at -0.3..-2 mm). Real terrain is never
  tested. Clearing EntityFlags.TRACEABLE on the ground patches makes it pass.
- PROVEN in play 2026-10-08 13:10 (Exp 192624): GetInVehicle native re-runs the trace, and the started
  CharacterVehicleActionGetInAnimated (vtable 0x141c6f908 slot 2 = sub_1408B9E60) re-checks during the
  animation -> hiding only around the call = animation starts and stops. Keep hidden until !IsGettingIn/Out.
- PROVEN in play 15:13 (Exp 192624): one static body with several trimesh geoms, each its own gamemat ->
  WheelGetContactMaterial reports the piece's gamemat. Surface map 1024 WORLD traces = 1 ms.
- PROVEN in play 15:25: Math.Min/Max(int,int) used DIRECTLY as an array index -> VM exception (index = raw float bits).
  Assign to an int var first or use Math.ClampInt.
- PROVEN 16:42 (Exp 192624): a fresh `bool m_aCell[16384]` script member read TRUE at every index in `if (arr[i])`.
  Use int arrays, zero-filled, explicit != 0.
- 2026-10-08 pak: destroyed building (SCR_DestructibleBuildingComponent) reparents + sinks + deletes every physics
  entity in its box -> deleted our patch (user 16:45). MUD_BuildingDestructionFix.c (modded QueryFilterCallback) skips ours.
- PROVEN 16:25 (user): with the edit-mode preview collider, World Editor right-click "snap to ground" lands objects on our surface.
- PROVEN in play 19:29-19:35 (Exp 192624, test map + Arland): GenericTerrainEntity.GetPhysics() exists (layer -1,
  1 geom). SetInteractionLayer(0) + SetGeomInteractionLayer(i,0) -> soldier free-falls on bare terrain, our ground
  holds. ClearFlags(VISIBLE) hides the terrain; our MatTerrainObj mesh still renders the terrain look. WORLD trace
  still hits terrain + returns gamemat with collision off. Restore read back layer 0, not -1 (restore unreliable).
- PROVEN in play 19:4x (user, Arland): hiding the terrain removes ALL grass/clutter; a 4 m skin mesh flickers (likely
  vs road meshes - unverified cause). MEASURED 19:40: whole Arland (4096 m) skin = 1024 chunks of 128 m, 1.1 M verts,
  2 M tris, 2016 ms wall over 128 frames (heights 152 ms, meshes 234 ms, colliders 391 ms).
- PROVEN in play 19:58 (Exp 192624): in a painted terrain hole, WORLD trace still HITS the heightmap (Y + gamemat),
  but GetSurfaceY returns -256 and the soldier falls through (user). Hole detection = GetSurfaceY < -255.
- 2026-10-08 Workbench exe strings (Experimental Tools): terrain edit page "Holes" (TerrainEditPageHoles,
  ToolHolesAddRemove, ToolHolesBlockEnableDisable, ModifyTerrainHolesCommand). No script API for holes in pak scripts.
- 2026-10-08 pak (subagent): Experimental removed WorldEditorAPI.ModifyHeightMap*/TerrainToolDesc_* (Stable still has
  them; api_search index stale). Only Get/SetTerrainSurfaceTile remain for plugin height edits.
- FreeIdle freeing all idle patches in one frame = 13-26 ms burst (~1.6 ms per free, all skin swaps). One per frame fixes it (proven in play 2026-10-09 14:12, Exp 192624).
- MEASURED 2026-10-09 14:46-14:57 (Exp 192624): building a new 8 m patch in one frame costs ~9-12 ms (index miss ~4, collider
  ~2, cover ~2-4); 2 wheel-made patches in one frame 22-31 ms. One build step per frame + 4 ms budget + 9 ms cap -> 6-9 ms.
- PROVEN 2026-10-09 14:50 (Exp 192624, 108 rounds x 9 down-traces, mask 0x7FFFFFFF ENTS|WORLD): no collider of ours above the
  ground; physics-debug sheets overhead are not our bodies (user: navmesh).
- MEASURED 2026-10-09 15:12-15:22 (Exp 192624): sleeping a dug patch (collider + 5 geoms dropped, skin chunk rebuilt with
  collider in those cells) gives back 229-284 KB; waking takes 208-282 KB. Live patch ~340-400 KB; d mem / d patches while
  driving fresh ground ~424 KB.
- PROVEN 2026-10-09 15:42 (Exp 192624): MUD_DepthBoxEntity.DepthAt picks up a placed box in play (floor -8 m inside an 8 m box).
- PROVEN 2026-10-09 20:38 (Exp 192624, DS): SCR_ExplosionAmmoEffect outMat[0] has y = 0 on a dedicated server (x/z right).
  DS skin chunks (render 0, no mesh) are not hit by ENTS|WORLD traces; the trace hits terrain 1 mm under them.
  PROVEN 20:47: a GenericEntity with physics but no VObject has world bounds = a point (vobject 0); projectiles pass it.
- MEASURED 2026-10-09 15:51 (Exp 192624, DS peer tool): without RplComponent ('rpl 0') server and client each run
  wheels + explosions on their own ground; server skipped all 27 craters the client made (cause open).
- PROVEN 2026-10-09 15:29-15:34 (Exp 192624): SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect() fires for mortar shells on the
  server/host; outMat[0] is the hit position (39 craters landed where shells hit).
- MEASURED 2026-10-10 11:31 (Exp 192624, peer tool localhost, scratchpad lag.py): live height message send->client got median 22 ms; client collider median 55 ms after send; server's own collider median 103 ms after send. The client ground is not behind the server's.
- MEASURED 2026-10-10 11:09-11:26: RplSave of 100000 int keys (400 KB) joins fine on the peer tool (59 ms) - but peer clients use -rpl-timeout-disable, real-link timeout untested.
- 2026-10-10: Rebuild() builds at most ONE collider per frame (oldest due) - read from code.
- USER TESTED 2026-10-10 ~21:3x on a REAL dedicated server: the stutter/lag is felt only by the DRIVER; passengers feel none. Fits the proven cause (server corrections to the owner's car prediction), so any fix targets the driver's client.
- 2026-10-10 21:5x (logs + layer read): a world save can silently drop MUD_WorldGroundEntity from default.layer (cause unproven; maybe saved while scripts failed to compile at 21:14). Symptom: nothing deforms, no 'MUD world: ready' line. Use the prefab form (RplComponent), bare class gives rpl 0.

## Driver stutter (2026-10-11, Exp 192624, robot runs + IDA; tables in 2026-10-11-driver-stutter-deep-dive.md)
- PROVEN: driver snaps on loose ground are vanilla and caused by low grip / wheel spin. Vanilla GM_Arland dirt 8 / 39.5 s, tarmac 0; asphalt grip on dirt = 0 / 59.5 s; NoiseSteer/Roughness 0 changed nothing.
- PROVEN: mud-caused extra snaps only from ruts moving the collider: cap 0.05 on rutted ground 9 / 20 s; cap 0 5 / 69 s; static skin only 9 / 59.8 s.
- PROVEN: Arland dirt has no vanilla terrain in our world (holed map, coarse skin) - vanilla control needs an unholed world.
- READ (IDA Exp game exe): server snaps when position gap > speed/300 m (6.7 mm at 2 m/s), angle > 0.003 rad, |v| diff > 0.3, |w| diff > 0.25. Script NwkCarMovementComponent.SetAllowance(speedScaled, divisor, angle, lin, ang) changes it; vanilla never calls it.
- PROVEN: script DiagMenu.RegisterItem id >= 512 = VM error "Too many Diags (maximum is 512)".
- PROVEN: engine vehicle diag items only draw on screen (no log/CSV, 09:55 run); diagmenu.save (F1 with menu open) keeps them across restarts.
- PROVEN 11:45 (MUD_FrameLog CSVs + user): the Diag collider/physics debug draw jolts and "catches up" while driving even on a smooth
  WB host: it sits on the physics body (fixed 60 Hz steps, no move on ~20 % of 75 fps frames), the mesh is interpolated. Don't read it as stutter.
- MEASURED 11:45 (WB host, robot reverse circle): front-wheel RPM stick-slip (~2 <-> ~65 rpm per physics step) on ~40 % of steps on
  vanilla dirt, 0 on tarmac. Link to the client snaps is still inference.
- READ (IDA Exp 192624 Diag exe, 12:00): Vehicles > Vehicle Networking and Reconciliation > Determinism testing > "SimState to CSV"
  (dword_14248E428) writes $logs:VehicleWheeledStateDebug_<id>.txt, one line per move: MoveId, 0Server/1Client, (S)Generated,
  (S)Corrected, inputs, engine, clutch, per-wheel suspension/tyre/contact, body pos + axes. Written by server move processing
  (sub_140841BE0) and two client paths. "Debug path correction" (qword_14248E470) gates the per-check NVMC log lines.
  Diag values are per process: a dedicated server has no menu, so server-side output there is unlikely (inference).
- PROVEN 12:04 (client diag "C: Disable Correction", vanilla dirt, robot): 0 snaps in 60 s vs 0.20-0.30/s with it off, stick-slip
  unchanged. The driver stutter is the client applying server corrections. Each snap = 6-10 physics steps of travel ALONG the path.
- (IDA read, not run, Exp 192624 Diag exe) NwkCarMovementComponent.SetAllowance(scaled, divisor, angle, linear, angular): divisor
  is ALSO the fixed distance limit in metres when scaled = false; scaled = true gives |v| / divisor clamped 0.001..1.5 m. The
  linear/angular tests are signed (server slower than client never fails). Nothing found that resets the values after EOnInit.
  Replay delay / max rtt / resend rate have no script setter. Details: stutter deep-dive 'Engine read: server side'.
- PROVEN 2026-10-11 (IDA read + logs, Exp 192624): engine NVMC vehicle-net lines are log level 4 (debug); default threshold is
  normal, so they only reach console.log with launch param "-logLevel debug" on THAT process (WB host and/or peer client).
  "NO MOVE TO CONSUME" needs diag "Debug history"; CORRECTING/APPROVING need "Debug path correction".
- PROVEN 2026-10-11 13:23: Workbench started from Claude's shell (Start-Process) got IdentityID all zeros -> the peer client
  was dropped "Another connected player with the same identity". Started by the user (13:42) -> real ID, client joins.
- MEASURED 2026-10-11 13:46-13:48 (WB host + client, full NVMC, robot on dirt): with "S: Replay delay" 8 the server queue never ran
  dry (0 NO MOVE) and the client still got 0.18 snaps/s -> queue starvation is NOT the stutter cause. Server corrections can be
  PHYSICS fails (dist + angle, 17/17 here) or ENGINE-state fails (28/28 in the 12:56 session); every client snap followed a
  CLIENT CORRECTING by 12-30 ms. A WB host stalls while unfocused (10 s lines missing 44-55 s) - check host health first.
- MEASURED 2026-10-11 (string grep of the exes, Exp 192624): ArmaReforgerServerDiag.exe has NO "Debug path correction",
  "Process Result", "SERVER CORRECTING" or "VehicleWheeledStateDebug" strings (the game Diag exe and WB exe have them). So a
  dedicated server can never log server-side NVMC checks or the server SimState CSV, whatever -logLevel / diagmenu.save say.
  On a DS only the client side (CLIENT CORRECTING, client CSV) is countable.
- MEASURED 2026-10-11 15:12-15:20 (DS + client, 16 runs): client "S: Replay delay" at any value still gives ~0.18 snaps/s.
  Quiet stretches of 0 snaps over 3-7 runs happen by themselves -> never judge a setting from one 3-run block.
  (IDA read) the item writes server +352 = v+2 and CLIENT +344 = v+5 only when v != 0 (last nonzero value sticks).
- PROVEN 16:05 (81 robot runs): the truck's START PLACE matters - GM_Arland dirt x1040-1095 z<2560 0.55 snaps/run, the
  test spot x1020-1090 z2565-2635 2.2/run. Keep the start spot fixed when comparing settings. Server frame/step timing
  (MUD_ServerLog: 30 fps, 60 steps/s, flat) is the same in noisy and quiet runs.
- USER TESTED 2026-10-11 (Exp 192624, MP): driving over FINISHED ruts gives no extra snaps; snaps come while ruts are MADE under the wheel. Pressing ruts a lead ahead (2 m + 0.5 s x speed) on the server = "so little stutter now". Then the belly touched the higher rut crown -> hard stutter again (ground changing under the body). Rule: any ground change near a moving vehicle must be finished before the vehicle reaches it.
- Robot driver pattern (TESTEX): set Car* actions in modded SCR_CharacterControllerComponent.OnPrepareControls before super; brake held at a stop = reverse.
- (proven 2026-10-11, Exp 192624) Moving a resource override (file + .meta with the vanilla GUID) to another folder
  INSIDE the addon keeps it live: Workbench maps the GUID to the new path. Rename or move it out of the addon.
- USER TESTED 2026-10-11 ~19:45 (DS, Ural + M923): arc ahead, rut fill (trailing axle fills to one front pass) and end axles give even ruts; offroad through craters had no stutter. Ahead presses use a fixed minimum lead (2 m), so a stopping vehicle left ~2 m of rut in front of it -> lead now capped by stopping distance (BrakeCap). Engine body collisions (bumper on a crater lip) stop the vehicle dead - body scrape only cuts what its prediction reaches.
- MEASURED 2026-10-12 22:59 (Exp 192624, Arland ~1415,2154, TEMP MUD_DecalProbe, deleted): vanilla DecalEntity (Mat in
  prefab, e.g. Decal_DirtPatch_04.emat) builds its mesh from the heightmap; on hole cells it hangs under the map.
  QueryEntitiesBySphere finds them (74-75 in 150 m) and prefab "Mat" reads fine, but IEntity.GetBounds gives ~zero
  extent (size 1 x 0) - the decal size is not readable from script. World.CreateDecal returned a decal for all 74,
  yet nothing visible (stretch 0, and the ENTS|WORLD trace hit GenericTerrainEntity, not our ground). Dropped on
  user request ("if not work fast skip"): not proven impossible, size/target unsolved. Result: bare dirt there.
- PROVEN 2026-10-12 (compile, Exp 192624): a script class holds at most 256 KB of member variables ("Can't add more variables ... too large (maximum is 256KB)"). Big static mesh buffers go in separate holder classes (MUD_VoxelProbe.c).
- MEASURED 2026-10-12 23:05 (WB play, Exp 192624, Arland ~1152,2418, TEMP MUD_VoxelProbe; user: 'it worked'): script voxel chunk 16^3 cells x 0.4 m, marching tetrahedra -> ~4.6-4.9k verts / 4.0-4.4k tris. Per full rebuild: polygonize 54-61 ms (all of the cost), MeshObject 0-1 ms, CreateTriMesh 0-1 ms, CreateStaticEx 0 ms, sphere dig 1-2 ms. First build +466 KB, rebuilds +0-11 KB (no leak with body + geom Destroy). Box faces on exact sample points gave ~5.3k zero-area tris (skipped) and 3792 open edges.
- MEASURED 2026-10-12 23:19 (WB play, Exp 192624, Arland, TEMP MUD_VoxelProbe run 2; user: collider works, walked into the cave): voxel chunk in the ground split into 8 sub-blocks of 8^3 cells (0.5 m), stamped edge cache instead of map, one sub-block rebuilt per frame: 80 builds max 4 ms, mean 1.8 ms (run 1 full 16^3 remesh 54-61 ms). Memory +0-8 KB per build. Looks: marching tetrahedra at 0.5 m gave jagged rims + slivers; per-vertex planar UV picked per vertex mixed projections inside one triangle -> stretched 'dragged' texture.
- PROVEN 2026-10-12 (compile, Exp 192624): Enforce 'Unknown operator %' for `Vector((id % S) * CELL, ...)` (int % inside a float expression / Vector args). Assign the % result to an int first; `int j = (id / S) % S;` compiles.
- Voxel probe run 3 (2026-10-12 23:27, Exp 192624, SP Arland): surface nets (vertex per cell, quad per sign-change
  edge, one UV projection per quad) - 88 sub-block builds, max 2 ms, mean 0.68 ms, 0 degenerate tris, no cracks
  (user). Texture looked "butter smooth": DirtCover_01 at 1 tile / 4 m is too blurry for walls, and the top used
  1 UV / 8 m while our fine patches use 0..1 per 64 m skin chunk. Engine-built normals give soft, faceted light.
- Vanilla tiling soil for script meshes (read from data005.pak, Exp 192624): ErodedEmbankment_01_dirt_soil.emat
  is MatPBRMulti with global maps on "UV set 2" (our meshes have one UV set) - not usable as is. Its layer 1:
  {72D059DB76507057}Assets/_SharedData/Soil/Soil_01_BCR.edds + {3796ACEE6478C91C}Assets/_SharedData/Soil/ST_Soil_01_1m_NMO.edds,
  tiling 10, Color 0.863 0.751 0.721. Our Assets/Mud/Data/MUD_VoxelSoil.emat wraps them as MatPBRBasic (not yet seen in game).
- Voxel run 4 (2026-10-12 23:36, Exp 192624): MUD_VoxelSoil.emat loaded (no log errors), but with UVs in WORLD
  metres (~1150) the soil showed as one flat dark colour. INFERENCE (unproven): script-mesh UVs are stored at low
  precision, so big UV values snap to whole tiles. Run 5 uses chunk-local metres (0..8) - result decides it.
- Voxel run 5 (2026-10-12 23:38): chunk-local soil UVs (0..8) look exactly as flat -> the low-precision UV inference
  above is RULED OUT. Run 6 = control: walls get MUD_VoxelCheck.emat (vanilla door checker BCR {B75A4E8B21A587D0},
  data003.pak) to see whether UVs reach the script mesh at all.
- Voxel run 6 (23:41): door MLOD checker BCR showed flat beige + shiny ("chrome") - INCONCLUSIVE (an MLOD BCR may
  itself be near-plain; no NMO/roughness -> shiny). Run 2 did show a stretched DirtCover texture, so UVs can reach
  these meshes. Run 7 = vanilla bricks {ABF409BE2A5CFA20}ST_Bricks_02_BCR + {932CB22CF32BA8BE}_NMO (data005.pak).
- Voxel run 7 (23:4x): vanilla bricks MatPBRBasic showed PURE WHITE (texture not loaded; Color only). Our MatPBRBasic
  property names match vanilla ones (pak). Hypothesis (unproven): textures used only by a script MeshObject never
  stream in (white, or lowest mip = one average colour: soil run 4/5 flat, door run 6 beige). Run 2 DirtCover showed
  detail because its layers use terrain Dirt_01, which the terrain keeps loaded. Run 8: MatPBRBasic with only
  {C6A0995C60084476}Terrains/Common/Surfaces/Data/Dirt_01_BCR.edds (user's DirtCover_01_test_boxels.emat).
- CAUSE of the flat voxel texture (runs 3-8, 2026-10-12): script-mesh UVs are packed to int16 and CLAMPED to 0..1
  (already in KB runtime-procedural-mesh.md, IDA 2026-10-07). All our wall UVs were > 1 -> one texel per wall. The
  streaming hypothesis above is WITHDRAWN (bricks white / soil brown = that one texel). Run 9: soil UVs 0..1 across
  the 8 m chunk, TilingU/V 8 in MUD_VoxelSoil.emat GlobalMapsUVTransform. Read the KB mesh page before mesh work.
- Voxel run 9 (2026-10-12 ~23:53, user: "much better"): soil UVs 0..1 across the chunk + TilingU/V 8 in
  MUD_VoxelSoil.emat -> textured walls; planar projection still smears on curved cave walls. Checkpoint v013.
  Run 10: vanilla DirtCover_01 at 0..1 per 8 m (same scale as our groove layer, which the user prefers).
- Voxel run 10 (2026-10-12 23:55, user: "it WORKED"): vanilla DirtCover_01 at 0..1 per 8 m chunk = same look as
  our groove layer; 88 builds max 1 ms mean 0.51 ms. Checkpoint v014. Test emats (soil, bricks) deleted.
- Voxel crater test run 1 (2026-10-13 00:07, SP Arland, mortar 0.99 kg): first blast made a 32 m area - seed 27 ms
  (one frame), 128 sub-blocks / 5947 verts built in 65 ms over 22 frames (worst frame 3 ms), live 569 ms after the
  blast. BUG: blasts INSIDE the area were skipped by Crater's height check (our heightfield there is sunk 4 m ->
  "air burst"); blasts outside overlapped -> heightfield craters. Fixed: InArea hook before the height checks, 64 m areas.
- Voxel crater test run 2 (2026-10-13 00:09, SP Arland, mortar barrage): one 64 m area centred on the first blast.
  Seed 137 ms in ONE frame (hitch); 512 sub-blocks / 22434 verts built in 251 ms over 84 frames (worst frame 3 ms),
  live 2213 ms after the blast. Later blasts: carve 1 ms, 16-82 sub-blocks, blast to done 69-305 ms, worst frame 3 ms.
  "Half craters" (user screenshot): blasts 2 and 3 hit 1.4-1.5 m inside the area's south edge (z 2442.2 vs edge
  2440.75) with crest 3.1 m - the area edge cuts the crater (no mesh past the edge, the 0.75 m full-height ring shows
  as a grass wall). 17 blasts just outside got heightfield craters. Fix (not yet tested): 64 m tiles on a world grid
  63.5 m apart sharing one cell column, world-aligned samples, a blast makes every tile its crater reaches, seeding
  spread over frames, ring sunk toward live neighbours. Measured seed cost: ~2.5 us per ground column (SurfaceY),
  ~0.23 us per density sample (from runs 1 + 2: 4225 cols / 71825 samples = 27 ms, 16641 / 416025 = 137 ms).
- Voxel crater test run 3 (2026-10-13 00:24, SP Arland, tiled build, 51 blasts all in tile <17,38>, so no seam was
  tested): seed 91 ms over 31 frames (worst frame 3 ms) - the one-frame hitch is gone; live 2522 ms after the first
  blast. Barrage batches: up to 236 sub-blocks / 46014 verts per batch, worst frame 5 ms, worst sub-block 3 ms,
  carve 0-2 ms. User saw "odd merges" and BRIDGES: the blast heights in the log show why - ground ~38.2 but hits at
  36.2, 35.5, 34.8, 34.1 (shells landing inside earlier craters). A full ellipsoid centred on a deep hit is widest at
  that height and undercuts the walls, leaving roofs/bridges; hits at 34.1 also reached the voxel block bottom
  (~34.0). Fix (untested): half ellipsoid below the hit, funnel widening 0.5 m per m above it, floor 3 m under the
  undug ground.
- PROVEN (log 2026-10-13 00:38-00:39, Exp 192624, SP): voxel TUNNEL dig (hold T, capsule r 0.8 every 300 ms) works; each dig
  carves 8 sub-blocks, done in 15-23 ms over 2 frames (worst frame 3 ms). The FIRST dig/blast on new ground had one 323-594 ms
  frame when the tile went live: the sink's EnsureRange made + finished every heightfield patch under the 64 m tile (~35-81,
  "index collider cover" x N in the worst-frame split) in one frame. Fix written 00:45 (untested): ProbeWarm builds 1 patch per
  frame while the tile seeds/builds; the sink waits for it. Live line now prints sink ms + warm stats.
- PROVEN (log 2026-10-13 00:43, SP): ProbeWarm (1 patch/frame) cut the tile-live frame from 323-594 ms to 58 ms (sink of a whole
  64 m tile = 46-56 ms; warm 53 patches, worst 8-9 ms per frame). The user still saw the crater "build itself slowly": a
  whole-tile ProbeSink only marks ~80 patches dirty and Rebuild does ONE mesh + ONE collider per frame, oldest first.
  Rework 00:50 (untested): per-patch sink in rings around the blast (ProbeSinkNow = sink + mesh/collider rebuild in the
  same call, 1 patch per frame per tile, only once the voxel sub-blocks over it are built); sub-blocks build nearest first.
- PROVEN (log 2026-10-13 00:51-00:53, SP): per-patch sink works (72 patches over 72 frames, worst 7-8 ms; warm worst 8-10 ms;
  tile-live frame gone). User: a blast on a NEW tile is still slow to show, a blast on a live tile is fast (34-212 ms to
  done). Seeding 78-109 ms took 26-37 frames (~0.8 s at a 3 ms budget). A neighbour going live made a 108 ms frame
  ("index collider wake" x N): the strip ProbeSink's EnsureRange woke every sleeping patch of the strip at once.
  Fix 00:56 (untested): SEED_MS 8, strips warm 1 patch per frame first, log line 'patch under the blast sunk N ms'.
- PROVEN (log 2026-10-13 01:19, SP, voxel GROUND mode, user: craters "much better" but "when it explodes i can see under
  the map"): with 7 tiles loading in parallel each seeds in 179-222 ms over 23-29 frames. Two causes of the see-through
  (inference from the code, not from a picture): (a) a crater carved under kept terrain columns opened a gap at the
  terrain cut edge; (b) live-tile rebuilds swapped sub-blocks in one by one, so part of a crater showed cracks for
  frames. Round 2 (01:41, checkpoint v020, UNTESTED): hard columns (any skin point in a non-hole cell) are never dug;
  live rebuilds are built hidden and swapped in together ('swap - N rebuilt sub-blocks' line); tiles load one at a
  time; trench tool + shovel go through voxel ops; ops networked (seq broadcast + JIP batches via SCR_PlayerController).
- PROVEN (crash 2026-10-13 19:19, Exp 192624, WB play stop, TEMP MUD_VoxelHouse): a Physics kept in a class member (compile warning 'Pointer type Physics can only be used with local variables') and Destroy()ed from ~MUD_WorldGroundEntity on play stop = access violation. Destroy the body only via the live entity (ent.GetPhysics()); entity gone = leave body + geoms alone (as MUD_VoxelProbe.FreeArea).
- MEASURED 2026-10-13 19:19 (SP Arland, TEMP MUD_VoxelHouse, House_Village_E_1I01 10 x 15 x 12 m bounds): voxelizing its Projectile collider at 0.25 m (TraceSphere r 0.14 TracePosition, Include = house) = 142609 traces, 150 ms over 19 frames; surface nets 275 sub-blocks, 46.5k verts, 65k tris, 278 ms over 46 frames; live 1.7 s after J. A 1.5 m hole: 18 sub-blocks rebuilt in 22 ms (one frame). User: 'it kinda worked'.
- MEASURED 2026-10-13 19:39-19:40 (SP Arland, voxel OVERLAY mode, log logs_2026-10-13_19-21-15/console.log): a blast or tunnel on fresh ground goes live 1087-1299 ms after the op (whole 64 m tile: seed ~122-132 ms, 512 sub-blocks built in ~235 ms over ~76 frames = 0.46 ms per 8^3 sub-block). Ops inside a live tile: 4-63 ms. User: the swap is very visible, ruts vanish (tile seeds from undug), first tunnel dig waits. Fix direction: sparse 4 m bricks, spec docs/superpowers/specs/2026-10-13-voxel-bricks-design.md.
- MEASURED 2026-10-13 20:22-20:24 (Exp 192624, SP, bricks per specs/2026-10-13-voxel-bricks-design.md): mortar crater on fresh ground live 101-163 ms after the blast (84 bricks built over 4-5 frames, worst frame 10 ms, go-live frame 2-12 ms); tunnel dig 4-8 ms, first one 45 ms. Whole-tile overlay was 1087-1299 ms.
- PROVEN 2026-10-13 20:24 (user screenshot + code): MUD_WorldGroundEntity.Rebuild skips asleep patches (and does one mesh per frame, Mesh Interval). A patch whose cells go VOX must get BuildMesh in the go-live frame (DoVoxCells does now), else its old surface + ruts lie over the voxel crater.

## Layered-SDF band mesher (2026-10-13, Exp 192624; results docs/superpowers/results/2026-10-13-sdf-phase1.md)
- MEASURED 2026-10-13 (offline Python, Tools/sdf_patch_proto.py, synthetic ground from the game's Press / Crater / trench formulas): band surface nets (0.25 m x/z, 0.5 m y on world planes) per 8 m patch = 1172-1378 verts and 2204-2620 tris (grid 1089 / 2048), plus ~270 skirt tris. Seams exact (0 cracks, 2 x 2 union) with ghost cells clamped onto the shared face; control with a 1 mm border offset shows 76 open edges. Worst case (random -4..+0.3 m) 6739 verts / 14103 tris.
- MEASURED 2026-10-13 (same): dual verts sit up to 9 mm off today's surface on undeformed ground (2 m skin knees), rut berm crests up to 56 mm lower (rms 6.9 mm). A 5 mm surface check (CheckNew) will flag knees when the band mesher is on.
- PROVEN (math, 2026-10-13): with layer height 0.5 (a power of two) `ceil(y * 2)` and `j * 0.5` are exact in float, so the band range and the crossing test never disagree (0 missing quads).
- MEASURED 2026-10-13 21:43-21:47 (Exp 192624, WB host + PeerPlugin1 client, robot 3 + 3 runs; logs ArmaReforgerWorkbench/logs_2026-10-13_21-35-34, PeerPlugin1 logs_2026-10-13_21-41-12 (off) / 21-44-53 (on)): band mesher in game = mesh 8.0-9.5 ms per patch mean (worst 11-25), collider 8.8-9.9 ms mean (worst 10-13); the mesher itself ~7 ms of it (sum 'mesher itself' / builds). Cells 1156-1270 mean, max 1489; tris 2178-2405 mean, max 2848; 0 overflow, 0 missing. Budget (mesh <= 3, collider <= 2) missed ~3-4x. Client fps during runs 57-59 on vs 89-93 off.
- MEASURED same session: client truck body jumps 14-21 m about every 9 s (2 per 20 s run, all 3 off runs, probe_corr 0) while the WB host ran ~5 fps (MUD corr '10 s' window = 291 frames over 58 s real, 21:42:53-21:43:51) - user watched the client window, so WB was in the background and stalled. On runs: host windows every 10 s real, no jumps. An A/B is only valid with the host's 10 s windows ~10 s apart in real time.

### Band mesher speed-up, MP A/B (proven 2026-10-13 22:07, Exp 192624)
- FAST single-layer cells + FAST y-quads cut the band mesh from 9.3 to 2.9-4.7 ms and the collider from 9.3 to
  4.2-5.5 ms per patch (grid: 0.9 / 1.45). The mesher core alone is ~2.1-2.5 ms in Enforce; time grows with
  deformation (fewer FAST cells). Full tables: results/2026-10-13-sdf-phase2.md.
- With the WB host window in front, the grid batch had 0 body jumps (the 21:41 teleports were the host stall).

### All-voxel test: voxel ruts (op 4), first runs (proven 2026-10-13 22:36-22:42, Exp 192624)
- Logs: local DS server/logs_2026-10-13_22-36-22 + PeerPlugin1 22-36-24; game client on a server ArmaReforger/logs_2026-10-13_22-38-42.
- Client (live server): 257 rut ops + 2 blasts, 0 refused. Rut op apply mean 0.7 ms, max 5; segments per op mean 10,
  max 23; ~1.1 new columns per op; op -> live mean 9 ms, max 36. End: 339 columns, 607 bricks. Client corrections
  over ~90 s driving: slow 1, mid 6, fast 10. Framelog run1 (20 s, <= 6.5 m/s): 89 fps, worst frame 26 ms, 0 body jumps.
- Local DS: rut op mean 1.5 ms, max 9; op -> live mean 60 ms, max 142 (DS frame rate).
- User: stutter "not that bad", worse when turning, very good at low speed; ruts too bumpy; "better than our first ruts".
- Inference (not tested): the cut follows the wheel's contact height (wheel y - step), so a bouncing wheel writes its
  bounce into the ground and the next pass bounces more.
- Smoothing fix (cut from the column's own surface, once per column per op) run 2026-10-13 22:48 local DS
  (server/logs_2026-10-13_22-48-43, PeerPlugin1 same): 220 rut ops, 0 refused, 0 script errors; rut op mean
  1.1 ms server / 0.8 ms client (max 7 / 6); op -> live client mean 16.6 ms, max 34; carved cells per op 1.2
  (was 1.6). Client corrections slow/mid/fast 0/3/5 over 68 s driving (the 22:36 local run: 0/0/0 over 33 s).
  User: "kinda smoother after we penetrated the grass layer".
- Convert ahead (op 5: 1.5 m strip ahead of the 2 leading wheels goes VOX before they get there) run 2026-10-13
  22:58 local DS (server/logs_2026-10-13_22-58-34, PeerPlugin1 22-58-35): 161 convert-ahead + 429 rut ops, 0 refused,
  0 script errors. Server: rut op mean 0.5 ms (max 3), convert ahead 2.2 ms (max 19). Client: rut 0.4 ms, ahead
  1.7 ms (max 8), op -> live mean 17 ms (max 54). Client corrections slow/mid/fast 0/0/9 over 116 s driving.
  User: "close to stutter free". Checkpoint v033 = this working state.
- PROVEN crash 2026-10-13 23:17 (Exp 192624, client PeerPlugin1/logs_2026-10-13_23-15-44, cut-behind run): access
  violation, read at 0x7e50. Stack resolved in IDA from the dump's exe base 0x7ff77ff80000: sub_140B1ED70 (+0x47,
  `mov rax,[rdx+0D8h]; mov rax,[rax+7E50h]`) <- sub_140B1C020 <- sub_14070D0B0 <- sub_14070B1A0 <- slot 12 of
  .?AVVehicleDustSystem@gamecode@@. The pointer comes from sub_140750290 = per-wheel contact + 48. Crash 8 ms after a
  go-live that swapped 16 brick entities (SwapPending deleted the old entity + body at once).
  Inference (not proven): a wheel's cached contact still pointed at a brick deleted that frame. Fix 23:21 (v034 =
  before): old / dropped brick entities are buried - hidden, interaction layer 0 - and deleted GRAVE_MS 1 s later
  (MUD_VoxelGrave, Bury, Graves). Untested.
- Cut behind (RUT_BEHIND, RUT_AHEAD off: rut segments cut only once the rearmost wheel is 3 m past them, timeout
  only > 6 m from the vehicle) + delayed brick delete (Bury, 1 s) run 2026-10-13 23:25 local DS
  (server/logs_2026-10-13_23-24-48, PeerPlugin1 23-24-50): 314 rut ops, 0 refused, no crash over ~2 min (the 23:17
  crash came after ~1.5 min - one run, not proof). Rut op mean 1.0 ms server / 0.9 ms client (max 9 / 8); client op
  -> live mean 8 ms (max 29); 2865 brick swaps on the client. Client corrections slow/mid/fast 0/0/0 over 90 s
  driving (10 / 22 / 58 s), visual-only snaps 99 (convert-ahead run 22:58: 9 real corrections over 116 s, 157
  visual-only). User: "by far the smoothest of them all, no lag". Checkpoint v035 = this working state.
- DESIGN, UNTESTED (2026-10-13 ~00:10, Exp 192624, v036 = before): look-now / feel-later ruts (RUT_LOOK, user idea).
  Op 6 = rut look: the same cut as op 4 but into a second per-brick field m_aV (PutLook, never marks a cell
  carved); the MESH draws min(m_aD, m_aV), BODIES only m_aD. Look segments are each wheel's previous segment
  (held one 0.5 m step so the tyre covers where it starts), sent before the feel op in the same flush. The feel
  cut (op 4) still waits for RUT_BEHIND (smart gap 1 m on voxel cells, else 3 m). A brick queued only by look
  ops keeps its body and swaps just its mesh (m_bMeshOnly); with no look cut reaching it, it is skipped. Needs
  RUT_AHEAD on (look ruts draw only in already-voxel cells). Expected tradeoff: rear tyres ride ~8 cm above the
  front tyres' drawn rut until the feel cut lands. Brick memory +2 KB each (m_aV[512]); JIP replays the look ops too.
- MEASURED (2026-10-13 23:55 run, Exp 192624, look/feel split v036+ with RUT_AHEAD on + smart gap 1 m; client
  PeerPlugin1/logs_2026-10-13_23-55-44 vs v035 run 23-24-50): no script errors; client brick builds 7224 in 144 s
  (50/s) vs 3370 in 93 s (36/s); build time 6013 ms vs 1870 ms (42 vs 20 ms per second); go-live worst-frame mean
  9.4 vs 6.0 ms; real corrections 3 (one 0.5 m body jump at 6.8 m/s) vs 0; 2187 look meshes, 990 kept their body.
  User: felt some lag; dirt "pops in" late (2 m skin cells only flipped to voxel at the feel cut).
- DESIGN, UNTESTED (2026-10-14 00:10, v037 = before): LOOK cells. A look op (6) that moves the drawn surface marks
  its skin cells look (column m_aLook, rendering machines only). At GoLive the patch gets the LOOK bit (MUD_WorldGround
  LOOK = 64): FillIndices drops the cell from the MESH index list only, the collider slot stays - wheels keep the
  heightfield collider. Bricks draw their mesh over carved + look cells (look field), the body over carved cells only
  (feel field). RUT_AHEAD off again; RUT_BEHIND gap 4 m both (smart gap off).
- PROVEN crash, cause UNRESOLVED (2026-10-14 00:08:33, Exp 192624, look-cells build, client PeerPlugin1
  logs_2026-10-14_00-07-36): RENDER 0x887A0006 DXGI_ERROR_DEVICE_HUNG ("badly formed commands"), Wait4GPU 4665 ms,
  then ENGINE (F) Crashed. Last script line 00:08:27.156 (a go-live), ~6 s silent before. Only ~7 s of driving (25
  go-lives, 0 real corrections). The SAME hang hit the client once before at 2026-10-13 22:58:27
  (logs_2026-10-13_22-57-41) with NO look code and almost no ground activity (24 bricks, 11 s after join) - so not
  shown to be the look-cell change. Never seen on the WB host. Inference only: second game instance on the same GPU
  (nofocus window) or a voxel brick mesh the GPU chokes on.
- MEASURED (2026-10-14 00:10 run, Exp 192624, look cells, client PeerPlugin1 logs_2026-10-14_00-10-04, 250 s, no
  GPU hang this time): client builds 37.2/s, build 34.7 ms per s, go-live worst-frame mean 9.7 ms (max 13), 3145
  look meshes, 1468 kept their body, 248 'look cells live' lines. Driving slow 20 s / 0 corr, mid 43 s / 0, fast
  43 s / 3 - the 3 are ONE event at 00:14:45.435 in a 79 ms client frame (no ground op in that frame; cause of the
  hitch unknown). Same table: v035 23-24-50 36.1/s, 20.0 ms/s, 6.0 ms, 0 corr; look/feel 23-55-44 50.0/s, 41.6 ms/s,
  9.4 ms, 3 corr (one event). Server (DS) 32.2 builds/s, 20.6 ms/s, 0 look work.
- PROVEN-IDA 2026-10-14 (ArmaReforgerSteamDiag.exe, same build as the KB mesh page; full write-up
  research/2026-10-14-ida-voxel-feasibility.md): script-mesh NORMALS are always rebuilt by UpdateVerts
  (sub_140AB3270) from the triangles of that one SECTION: unit face normals, weight 1 each (not area-weighted),
  zero-area triangles add nothing. No way to pass our own normals; bricks and our top/dirt sections never share
  normals at their borders. Call UpdateIndices BEFORE UpdateVerts (normals use the indices present then).
- PROVEN-IDA 2026-10-14: script meshes have ONE UV set and NO vertex colours (Create path allocates pos, UV0, u16
  indices, normals only). Indices and per-section vertex counts are u16: UpdateIndices and CreateTriMesh keep only
  the low 16 bits of each int (silent wrap past 65535).
- PROVEN-IDA 2026-10-14: MeshObject.Create de-duplicates materials by path (sub_140AB53A0) but section i still uses
  slot i, so two sections with the SAME material leave a slot unfilled. Strong inference: the 2026-10-10 skirt
  crash above. Give every section its own distinct .emat path.
- PROVEN-IDA 2026-10-14: PhysicsGeom.CreateTriMesh copies the data into a Bullet btBvhTriangleMeshShape (quantized
  BVH) with NO triangle filtering - degenerate / folded tris go into the collider. No refit / partial update in
  script; Physics has AddGeom + SetGeomInteractionLayer (switch off) but no RemoveGeom.
- PROVEN (2026-10-14 19:20, Exp 192624, compile errors in WB console logs_2026-10-14_18-07-47): Enforce static
  arrays hold at most 65536 entries ("Maximum static array index is 65535 'm_aI'"), and one class holds at most
  256 KB of members ("Can't add more variables to 'MUD_VoxelVerts' class, it's too large (maximum is 256KB)" at
  32000 vectors + 64000 floats). Fine voxel mode (0.25 m) sized to fit: MAX_V 10000, MAX_I 30000, probe cell cache
  MUD_VoxelSlots (2 slots per cell + UV kind).

- PROVEN 2026-10-14 (Exp 192624, WB SP, MUD_EngineProbes G2): a MeshObject.Create with the SAME .emat on two sections crashes Workbench (access violation, read at 0xffffffffffffffff) ~3 ms after SetObject; every script call returns fine first. Control with two different emats survives. Each section needs its own emat (IDA: Create de-dups materials by name). Likely the 2026-10-10 skirt crash. docs/superpowers/results/2026-10-14-engine-probes.md
- PROVEN 2026-10-14 (Exp 192624, WB SP, MUD_EngineProbes G3): Physics.AddGeom on a live static body + SetGeomInteractionLayer(old, 0) works (traces skip the old piece, 200 pieces on one body, no slowdown), but it costs the same as Destroy + CreateTriMesh + CreateStaticEx (2048 tris: 0.49 vs 0.465 ms incl. 0.185 ms grid fill). The cost is CreateTriMesh. Collider patching gives no gain. docs/superpowers/results/2026-10-14-engine-probes.md

- MeshObject.UpdateVerts rebuilds normals in a STACK buffer of 12 x (vertex count given to Create) and indexes it
  with the index buffer WITHOUT a range check (IDA 2026-10-14: Stable WB sub_1422EE290 -> sub_141F29110; Exp WB
  rva 0x2424f30 -> 0x20659e0). Any index >= the Create vertex count smashes the stack; the crash then shows up as
  "Illegal read at 0xffffffffffffffff" inside UpdateVerts. The index buffer must never reach past the vertex count.
  The 4 mud-sheet tile crashes (21:49-21:58) all had a mesh created with 6 vertices but indices up to 440.

- 2026-10-14 (proven, crash): Math.Min/Max return float. Used inline as an array index (arr[Math.Min(a, b) * 4 + c]) the index is the float's raw bits ("Index out of range (1093822055 >= 16)"). Assign to an int local first.

- PROVEN 2026-10-15 00:27 (Exp 192624, WB SP, MUD_MudSheet UpdateSlots diag): a class MEMBER fixed-size bool array
  ('protected bool m_aSlotZone[16]') does not read back what was written. Same loop filled an int[16], a bool[16]
  and counted a direct TileInZone call: int 8 / bool 16 / direct 8, int 12 / bool 16 / direct 12; when all were
  true, all read 16. The bool copy read true for slots that were written false (also seen 23:35 with the window
  never placed: '16 of 16'). Symptom: mud window tiles drawn outside every zone (4 x 4 faded squares round any
  vehicle). Fix: use int[] (0 / 1) for member static arrays of flags. Cause in the VM not probed (inference: 1-byte
  bool write, wider read). Local bool arrays and array<bool> not tested. MUD_WallPanels m_aPx[8] converted too.
- 2026-10-15 [IDA, Exp 192624]: MeshObject DYNAMIC_VERTEXES (in-place UpdateVerts) cannot fix the mud sheet cost.
  Its draw prep (sub_140AADE70) copies every visible dynamic section into a per-frame ring buffer each frame and
  never writes the 4-byte normal slot (stride 20, writes pos + UV only) -> garbage normals = the scaly shading.
  Engine bug, no script workaround. Keep flags 0 + rebuild. Details: KB runtime-procedural-mesh.md.
- PROVEN 2026-10-15 (compile error): all locals of ONE Enforce method must fit an 80 KB VM stack ("Can't add more variables to ... method, it's too large so it can't fit on VM stack (80KB)"). Local static arrays count fully (vector 12 B, int/float 4 B going by the sizes: ~56 KB compiled, ~94 KB refused). MudSheet.BuildTile sits near ~69 KB - check before growing its arrays. Also: '&' in '(hsh & 1023) / 1023.0' gave "Unknown operator '&'", and 'Vector((m % TV) * CELL, ...)' gave "Unknown operator '%'" (PROVEN 2026-10-15): an int-only operator in parentheses inside a float expression is refused - put the int part in an int local first.
- MEASURED 2026-10-17/18 (WB + DS logs, ~1700 'MUD decal:' lines, MUD_VoxelProbe.CraterDecal): World.CreateDecal never made a decal on OUR ground. Run 1: the trace hit the hidden GenericTerrainEntity (old ground y), CreateDecal on it = null. Run 2: CreateDecal straight onto our shown voxel brick entities (script MeshObject) = null every time ('decals made 0'). Why is unknown (material / entity setup / projection not checked). CreateDecal does work on vanilla entities (addons/Spray tagging can, SCR_SprayRaycastComponent.c).
- PROVEN 2026-10-18 (compile error 'Maximum of 64 local variables exceeded' / 'Can't create variable', MUD_MudSheet.BuildTileMesh after paper tests 1-2): a second limit beside the 80 KB stack - at most 64 locals IN SCOPE at one point, and the method's parameters count (failed at the 65th = 59 visible locals + 6 params). Locals of a closed sibling block (an if / for body that has ended) are freed, so a block-local is cheap and a method-wide one is not. BuildTileMesh peaks at 63 now (57 + 6 params, Mud scratch counter) - new method-wide locals there need one removed or the code moved into its own block. A failed mod compile also prints vanilla errors (Tuple1, SCR_ScenarioFrameworkParam ...) - most likely follow-on noise (inference, not proven): fix the MUD error first and recompile.
- PROVEN 2026-10-15 (rut field Stage 3, user look test + logs 21-30-16 / 21-35-43): the cut was the same size as v097
  (41 floor samples per rut op vs 40-82), yet ruts on patches looked ~2x wide - the patch groove reveal starts soil at
  m_fRevealDepth 0.01 m, so almost the whole 1 m RUT_R trough went soil (v097 bricks: soil from ~0.03 m). Field ruts
  now reveal from FIELD_SOIL_D 0.03 (user: right). Also proven: a no-column hard check needs Convert's FULL rule
  (Kept || Hard); Hard alone let ruts cut onto asphalt / concrete edges.
- PROVEN 2026-10-15 (WB log 20-14-47, run 1 vs 3): voxel brick vertices (m_Vb) are brick-LOCAL (origin m_vBOrg);
  anything stored in world y (rut floor) must subtract m_vBOrg[1] before comparing - else 0 split bricks, ruts invisible.
- PROVEN 2026-10-15 (rut field, user screenshots + WB log 22:47): anything that lowers patch verts outside m_aH must
  scale the depth by p.m_aLock[v] like presses do - unscaled, a rut at a kept-terrain edge (road / path) dips the band
  rim while the terrain stays: a see-through V gap. And brick soil measured only against the SEEDED ground leaves a rut
  over a raised crater rim grass next to soil patches; soil by pull depth (m_aDispY - y > 0.03) fixed it (soil walls
  0-82 -> up to 733 per batch).
- PROVEN 2026-10-15 (WB log 23:21-23:24, shared band build test, reverted to v102): a patch's band mesh and collider
  never rebuild in the same frame (frame budget; mesh interval 0.1 s, collider 0.2 s). Reusing the mesh's band build
  for the collider when inputs match caught 14 of 217 collider builds (~6%): a new rut changes the inputs first.
  Halving the mesher cost needs a schedule change (collider right after its mesh), not a cache.
- Enforce array.Remove(i) moves the LAST entry into slot i (unordered). Never keep an index across a Remove in the
  same loop - keep the key / value (FieldDue crash 'Index out of bounds' 2026-10-16). RemoveOrdered keeps order.
- MEASURED 2026-10-17 (Exp 1.9 data007.pak gamemats): land gamemats have NO wheel mud spray - ParticleEffectInfo holds only Dustiness / Muddiness + WheelsDustEffect (grass 0.2/0.2, dirt + soil 0.9/0.9); only Core/water_base.gamemat has a mud effect. So 'no mud tossed up' in rain is vanilla. Wheel mud / body mud counters DO fill on our patches (WB log 14-56-38). PROVEN WB dry run 15:32: wheel dust spawns on our patches (dust > 0.05 on mud_rut 708/708, grass 198/218, dirt 11/11;
  user sees it). A collider gamemat nothing holds gets freed ('Usage of released BaseResourceObject', vhcsurf null) -
  hold it: ref Resource = Resource.Load(name) (m_RutMudRes; 278 errors -> 0).
- IDA 2026-10-17 (ArmaReforgerSteamDiag.exe, stable_1.9.0 build path): sun shadow caster gather (enf::ShadowCasterIterator, leaf visit sub_140C4AAE0) SKIPS any entity whose VObject instance (entity+0x50) has flag bit 8 (VCFlags NOSHADOW) at +0xC. Prefab MeshObject component maps CastShadow 0 -> 8, CastFarShadow 0 -> 0x800 (sub_140AA69A0). Script IEntity.SetVComponentFlags (sub_140E4ED80) only ORs into those instance flags and RETURNS them - SetVComponentFlags(0) is a read; there is no clear. SetObject(obj, "") passes no component props (sub_1409B5610). MEASURED WB 18:02 (21 lids): a script MeshObject lid (MAT_DIRT) reads 8388608 = 0x800000, bit 8 NOT set - script meshes are not NOSHADOW by default.
- PROVEN WB 2026-10-17 (log 19:09 + user confirm 19:30): tunnel darkness probes - any tunnel space outside every probe ProbeVolume box gets full OUTDOOR ambient (looks like day inside, a blown-out white blob from the dark tunnel via eye adaptation; diag Scene lighting > Ambient / Diffuse lighting off removes it, Specular does not). One probe per 2 m cell at the cell's first dig left lower digs' floors under the 3 m box. Fix: a probe at the CENTRE of every grid cell the dig capsule touches (TunnelDarkCell). Probe debug: diag menu Clustering > Probes shapes / Disable local probes. Not the cause: sun, NOSHADOW, CastShadow, lids, NearPlane, capture refresh / feedback, TopColorEV, wall material.
- PROVEN WB 2026-10-17 (user confirm 20:1x): a lit spot INSIDE probe cover (deeper side branch of a 3-way tunnel, every dig inside a level 3-4 box by log math, Ambient off removes it) went away when the probe grid went from 2 m (5x3x5 boxes, up to ~18 over one point) to 4 m across / 2.5 m up with BoxLerp 0.15. Keep probe boxes from piling up. LATER the spot came back in wider tunnels (view-dependent, seen facing east) - see next. Why the engine drops light there (per-cluster probe cap?) is inference, not checked in IDA.
- PROVEN WB 2026-10-17 (user confirm 20:4x): lit spots on tunnel walls inside full probe cover are AMBIENT on our wall material; probe settings (ExtProbeDiffuseWeight 0, Priority 0, PriorityProbe 1, fog off) did NOT help. A wall material with DiffuseIBL 0 0 0 1 + SpecularIBL 0 0 0 1 (MatWorldClass props on every material = 'ambient IBL modificator', white default; IDA strings 0x141c7d8f0) killed them - but all black looked bad. IDA: probe props also include UserEVMode (0 Automatic / 1 Relative EV / 2 Absolute LV), UserEV_LV, EnableHDRModifications.
- MEASURED 2026-10-17 21:43 (WB, MudArland, MUD_PinProbe): 66274 RELATIVE_Y roots = 51492 BaseTree, 10974 SCR_IndestructibleEnvironmentalEntity (rocks), 1727 DecalEntity, 920 StaticModelEntity, 541 SCR_DestructibleEntity, 526 Building (SCR_DestructibleBuildingEntity), 76 GenericEntity, 10 SCR_PowerPole + 2 SCR_JunctionPowerPole, 6 GameEntity; 0 on hole cells (pins). PROVEN (user 2026-10-17): SetOrigin + Update on a static rock keeps its collision (lifted Granite_Boulder_02 blocked the player; the horizontal ENTS trace probe missed it - trace was wrong, not the collision).
- 2026-10-17 (pak + probe): trails = RG_Trail{Dirt,Forest,Gravel,Coal}_01.et, RoadGeneratorEntity with a child RoadEntity Type Path, Width 1.75, Material Trail*_01.emat, same dirt_road.gamemat as dirt roads. In play a RoadEntity has no prefab, name, parent, VObject, and script has no material / width -> tell trails apart by origin from a World Editor plugin list (MUD_RoadListPlugin).
- PROVEN WB 2026-10-17 22:52 (Exp 1.9, MUD_BlastFxProbe): our Warhead_Base.et override (only MUD_WarheadComponent in it) MERGES - a spawned Warhead_Shell_HE_M821 still has HitEffectComponent, BaseTriggerComponent, SoundComponent. Shells burst AT the ground: on hole cells ENTS|WORLD hits MUD_CoarseChunkEntity at the blast y, WORLD-only the hidden GenericTerrainEntity 1 mm lower, both with the painted gamemat (grass_lush); roads hit RoadEntity, kept cells the terrain (asphalt). Every crater logged '1 props hidden' = (inference, fix untested) the blast's ParticleEffectEntity hidden by MUD_GroundFollow -> no vanilla dirt-throw.
- READ FROM BINARY 2026-10-18 (IDA, Experimental ArmaReforgerSteamDiag.exe): diag menu 'Statistics' has 'Script profiler' (off / frame / average / memory), 'MeshObject stats' (none / count / mesh / faces / verts), 'Stats page' 0-19, 'Resource profiling'. Script API System.GetProfileData() -> ProfileData FPS / FrameTimeMs / RenderTimeMs / GPUTimeMs / SimulationTimeMs. System.GetTickCount is the only script clock (1 ms steps; IDA 2026-10-18: native sub_140E6F880 reads a high-res performance counter, elapsed = floor(now exact - start whole ms), so one reading is coarse but sums / averages over many calls are unbiased - per-part MUD_Prof sums are fine). In game screens unverified.
- PROVEN WB 2026-10-18 20:32 (Exp Tools 1.9, MUD_DecalProbe, 8 tries, log logs_2026-10-18_20-10-50): World.CreateDecal / CreateDecal2 returned null on EVERYTHING - the wheel contact entity (7x MUD_CoarseChunkEntity, 1x MUD_FinePatchEntity), our patch, the vanilla vehicle body (control), and fresh never-rebuilt script quads with MatPBRMulti (DirtCover_01) and MatTerrainObject. Params as the spray-can mod (stretch 1, near 0, far 1, project -normal, MatPBRDecal mortar emat). TrackDecalFactory.AddPoint on our patch = false, no active decal. No 'Unable to load material' line. So the null is not about our meshes; why CreateDecal fails even on a vehicle here is unknown (untested: a vanilla static building / rock hit with the spray mod's Blood10 emat). Wheels offroad touch MUD_CoarseChunkEntity most of the time, not patches.
- PROVEN WB 2026-10-18 ~20:50 (MUD_DecalProbe run 2, log logs_2026-10-18_20-10-50, user saw them): World.CreateDecal RETURNS NULL EVEN WHEN THE DECAL IS MADE AND SHOWS. All 21 run-2 probes logged 0 on every target (contact GenericTerrainEntity / RoadEntity on the road, MUD_CoarseChunkEntity / MUD_FinePatchEntity offroad, vehicle body), yet the user saw the red Blood10 decals on OUR ground. So the null return is no success test in this build, and the run-1 and ~1700 crater 'decals made 0' lines prove nothing - judge decals by eye only. Call that showed: spray-mod form (origin 0.2 m off along the normal, project -normal, near 0, far 1, stretch 1), Decal_Blood10 {9EE38D3540663BCF}. Offroad wheel contact = MUD_CoarseChunkEntity mostly (5 of 7), MUD_FinePatchEntity 2.
- PROVEN WB 2026-10-18 23:37 (MUD_RoadHideProbe, results/2026-10-18-road-hide-probe.md): one RoadEntity = a WHOLE road spline (bounds 78 x 58 to 398 x 548 m); RoadEntity.Show(false) and ClearFlags(VISIBLE) both hide it, all or nothing; GetPhysics() null on all 3 = roads have no collider of their own (vehicles drive on the ground under them).
- Enforce: a static const array (static const float A[] = {...}) has no .Count() - 'Undefined function float.Count' (compile 2026-10-19, MUD_RoadLookProbe); keep a separate const count.
- PROVEN WB 2026-10-19 (user eyes, rut debugger, Ural): VehicleWheeledSimulation.WheelGetContactPosition does NOT follow a
  steered front tyre. WheelGetPosition(w) is VEHICLE-LOCAL (WB 00:38 log: constant x 1.02 z 2.65 while driving; read
  as world it cut ruts near the map corner = no ruts) - use CoordToParent. Whether its hub follows steering: by eye
  pending (user named the world-read ball first, which cannot sit on the tyre).
- IDA 2026-10-19 (Exp Diag 1.9, results/2026-10-19-sheet-cost-probes.md D1): IEntity.SetBone (sub_140E3BAE0) needs a
  MeshObject whose .xob has a skeleton AND its skeleton instance; without either it does nothing, silently. The instance
  is made by IEntity.GetAnimation() (sub_140E4C680 -> sub_140A47C10) - call it once on a bare entity. SetBone stores a
  per-bone override (zero angles + trans, scale 1 = clear) and marks the entity dirty; main thread only. Bounds are NOT
  updated: the MeshObject property 'BoundingBox' is the "override of bounding volume, if object is animated and can
  exceed its static bounds".
- IDA 2026-10-19 (D2): SignalsManagerComponent.SetSignalValue only writes a float (no changed flag). ProcAnimSystem
  evaluates EVERY registered ProcAnim every frame (jobs of 4, worker threads), no distance / visibility / signal-changed
  test - cost grows with the number of ProcAnim pieces, not with changes. ProcAnimComponent has 'OnFrameUpdate'.
- IDA 2026-10-19 (D4): SpawnEntityPrefab's native takes an already-loaded Resource and calls the game (no resource
  load inside); keep one Resource from Resource.Load. The Render diag "Shape instancing" is debug Shapes only.
- IDA 2026-10-19 (D5): GenericComponent.Deactivate(owner) on a ProcAnimComponent calls ProcAnimSystem::Unregister
  (vtable +48 = sub_1402A0560) - the component leaves the per-frame list; Activate re-registers it. OnFrameUpdate is a
  prefab-only property (0 = never registered).
- IDA 2026-10-19 (D6/D7): a drawn skinned MeshObject copies its bone palette (48 B per bone) every draw, no unchanged
  skip found (MeshObject draw-prepare sub_140AAF5A0). Pose / bounds code uses fixed 384-bone buffers.
- IDA 2026-10-19 (D8): IEntity.SetObject with a different VObject or remap builds a NEW MeshObject and releases the old
  one - the skeleton instance and all SetBone overrides are lost (call GetAnimation + SetBone again). Same object +
  same remap = no-op.
- IDA 2026-10-19 (D9): a skeleton mesh is a MeshObjectAnimatedComponent; its bounds follow the bones ONLY while its own
  flag 0x20 is set, which animation systems set (ProcAnim job when bones moved, doors, Cinematic Tool) - SetBone does
  not. SetBone-moved meshes keep the bind-pose box: use the MeshObject BoundingBox override.
- PROVEN 2026-10-19 (all 10361 .emat in the Experimental data pak + exe string search): no world-space normal map
  option. Normal maps use only "UV set 1" / "UV set 2" (UVSrc*); ApplyWorldNormal is a MatPBRBasicGlass refraction
  flag; no triplanar / world-UV setting. With no custom shaders, smooth tile seams must come from our vertex normals.
- PROVEN DS 2026-10-19 (look lift test, user eyes + logs: 288 samples cut, trail probe 4-6 cm deep): a ground drawn
  higher than the collider (TEMP LOOK_LIFT_TEST, 0.16 m on skin + patches) makes ruts INVISIBLE - the rut floor stays
  above the heightmap, so grass clutter grows in it (clutter rule above), and dirt only starts at FIELD_SOIL_D 0.06.
  Any drawn lift must drop to ground + floor where the ground is cut (research/2026-10-19-ruts-invisible.md).
