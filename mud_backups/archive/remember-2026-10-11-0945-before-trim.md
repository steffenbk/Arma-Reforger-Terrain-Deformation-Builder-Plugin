## Now
Goal: deformable ground (ruts, craters, trenches, MP + JIP) over WHOLE Arland. Target Exp 192624. No git.
Ranking source of truth: docs/superpowers/backlog/2026-10-10-master-list.md. Plan 3 + ledger: docs/superpowers/plans/2026-10-09-mud-whole-map-holes-plan3-ground.md, .superpowers/sdd/2026-10-09-mud-whole-map-holes-plan3-ground/progress.md.
History: mud_backups/archive/remember-now-2026-10-11-0022.md (to 00:22), mud_backups/archive/remember-now-2026-10-11-0225.md (00:22-02:25: lazy, far patches, F4, P1a hold, jolt probes, deep dive).
All compiled + user-tested: craters by charge (MUD_WarheadComponent), crater walls stop blasts (MUD_BlastCover), prone box 0.6 0.4 0.6, lazy READY, client far patches (m_fClientBuildRadius 300), F4 send by distance (m_fSendRadius 500), JIP ask cap, P1a wheel hold (m_fWheelHold 0.15), NET limits 0.25/0.05/1.0/0.8 (MUD_Wheels.c).

CURRENT TOPIC: driver-only stutter in MP (passengers never; vanilla has a little too).
 Write-up: docs/superpowers/research/2026-10-11-driver-stutter-deep-dive.md (engine via IDA sec 1-5, run A sec 6, run B sec 7).
 Engine (IDA, proven): server never takes driver state; own sim; corrects when gap > SetAllowance; driver resets + replays. Lerp hides NOTHING (probe v2: visual = body within 1-2 cm) -> each correction = full ~0.25 m snap (= NET_DIST).
 FIXED 02:10 (compiled, run B 02:11): server Wheels() skipped vehicles < 0.2 m/s -> server hold expired, collider dropped 0.3 m, driver's stayed up -> correction every 150 ms while parked. Now server TouchWheel() on stopped vehicles that are awake or have a pilot (MUD_WorldGround.c Wheels ~903). Parked loop gone; slow corrections on ours 23 in 19 s -> 3 in 23 s.
 LEFT: ~1 correction per 7.6 s on our ground (road 1 in 13 s, small sample), all on dirty patches, collider 0.12-0.20 m off newest heights = ground changes under wheels at different moments on server vs driver while digging (inference).
 Skipped the wheel-hold 1.0 test: user picked the RUT LAYER SPLIT (09:22). Spec: docs/superpowers/specs/2026-10-11-rut-layer-split-design.md.
 WRITTEN 09:3x, NOT COMPILED (WB was in play): MUD_FinePatch.m_aRut (wheels only); m_aH = craters/trench/shovel/fill + rim plow (user: keep plow in physics). Mesh = H+Rut, collider = blur(PhysD = H + clamp(Rut, +-m_fRutPhysCap 0.05)). Crater/carve/fill SetH(..., true) wipes Rut. Net/JIP pack = 1 int per vertex (lo H mm, hi Rut mm), ApplyRect count check changed. Hash/seam/Heights() updated, GetH removed. Backup: mud_backups/2026-10-11-0925-pre-rut-split/.
 NEXT (exact): stop play, reload scripts, fix compile errors; offer adversarial review (net change); then run-B-style test (DS + PeerPlugin1, rut speed 3) and compare 'MUD corr 10 s' ours slow/mid/fast vs B (3/6/3 in 23/44/24 s). Then eye check: rut look, tyre hover (up to ~0.30 m on heavy), crater wipes ruts, JIP joiner sees ruts.
 Shot down: invisible box in front of wheel (digs where wheel is about to roll = worse).
 Probe: MUD_JoltProbe.c v2 ('MUD corr:' per correction, 'MUD corr 10 s' buckets ours/road x slow/mid/fast; along-path jitter filtered, server shows 0). Logs: server C:/Users/Steffen/Documents/My Games/server/logs, client .../PeerPlugin1/logs.

TEMP still in code: m_fRutSpeed 3 (release 1), MUD_JoltProbe v2 + its EOnFrame call + MUD_WorldGroundEntity.ProbeGround + MUD_FinePatch.m_fColChangeAt (remove when stutter done). Trench DEBUG_INSTANT=true (kept on purpose for bug testing; idea: hold Ctrl+F auto-finish as test option). m_bDebug default on.
Open: 0.6 prone box changes every soldier (check low walls, windows, under vehicles before release). Vehicle-blast craters PINNED in master list. Far-patch + catch-up untested with 2 players far apart. Per-player trench cap.
Later idea: preset trench layouts (mortar pit, bunker). Out of scope: character model dips through deformed ground while moving.

## Decisions / gotchas
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
