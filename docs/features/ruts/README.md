# Ruts (voxel ruts, v035)

**Status: SHIPPED.** Short page - add to it when we work on this feature.

## Lessons for the voxel ruts (from the mud sheet work, 2026-10-18)

Measured on the dedicated server unless marked otherwise; detail in research/2026-10-18-mud-mesh-cost-research-findings.md
and research/2026-10-18-fixes-2-4-and-brainstorm.md.
1. Mesh loops: hand-inline small helpers, read per-vertex flags once into a local int array, skip untouched cells
   = 1.10 -> 0.88 ms per tile build (-20 %, DS 16:16, same output checked). Bench recipe: N old + N new builds of
   the same data between two GetTickCount calls, compare the output count. Same recipe fits the patch mesh /
   collider builders and DispEmit.
2. Build what is next to the wheels first: wheel tiles first took the sheet delay from up to 9 s to ~80 ms
   (DS 12:24). A per-item rebuild limit (0.25 s) raised fps 58-72 -> 73-90 (DS 15:37) but must leave the
   near-wheel items free, or a "wave" chases the truck (DS 15:47 fix).
3. Measure lateness with a delay meter (wheel stamp -> drawn). Look ruts today: wheel -> look cut avg 400-530 ms,
   max 1.5-3 s (DS 16:16 / 16:27) - that is the gap behind the wheels.
4. Rut op cost: RelaxFloor is 5-6 ms per op; the crude cut (no relax, no ridges) took one op 15.7 -> 9.8 ms on the
   client and 14.4 -> 7.7 ms on the server (DS 13:38). The server 'MUD rut floor' Print was 2.2 ms per op (now
   behind OP_LOG). Re-ground exact + FLOOR_SPREAD removed the dirty-rebuild spikes (DS 13:52 / 14:11).
5. Skipping patch collider rebuilds under 2 cm change: only 0-4 skips per 10 s - not worth it (DS 16:16).
6. Far LOD works: far meshes at half resolution, back to full when the camera comes within 17 m (DS 16:16) -
   a candidate for far patch meshes.
7. Two systems drawing one visible surface (patch mesh + sheet tile, separate queues) gave pops and a height step
   that build order + a 1 cm lift did NOT cure (DS 16:26, user) - cause unproven. Keep one owner per surface.
8. Enforce: at most 64 locals in scope per method, parameters included (compile 2026-10-18).

Next (2026-10-18): wheel-to-rut track mark - prompt docs/superpowers/prompts/2026-10-18-wheel-to-rut-decal.md (new chat).

## Wheel-to-rut track mark (started 2026-10-18, v207 = before, NOT COMPILED)
- Step 1 DECAL PROBE: TEMP MUD_DecalProbe.c (ENABLED, called first in MUD_LookRuts.Tick = rendering machines only).
  8 tries, 2 s apart, wheel 0 of the vehicle nearest the camera: CreateDecal on the contact entity / our patch
  entity, CreateDecal2 on the patch, TrackDecalFactory.AddPoint on the patch; CONTROLS: the vehicle body, a script
  quad with MatPBRMulti (DirtCover_01), a script quad with MatTerrainObject. Log 'MUD decal probe N/8'. Tries are
  >= 0.6 m apart (engine dedupes a same-material decal within ~0.32 m). Materials have no NoDecals line (checked
  default_MatTerrainObj1.emat, DirtCover_01.emat). Patch decals vanish on the next patch rebuild (IDA) - look right away.
- Found: vanilla Wheeled_Base.et already has native tyre tracks (VehicleWheelEffectComponent SlidingTrackMaterial
  {AE248EE9E164EB4B}Assets/Decals/TrackDecal.emat); IDA 2026-10-16 says track decals start only on the terrain entity.
- Quick wins in the same build: MUD_VoxelProbe.OP_LOG false (per-op 'MUD voxel probe / rut field / rut floor' lines
  gone, hash lines unaffected), MUD_WorldGround.COL_SKIP_SMALL false. Loop speedups (quick win 3) not started.
- Fallback if decals fail: own ribbon mesh per wheel (wheel contact -> look rut start), a few mm over the patch mesh.
- PROBE RESULT (WB 2026-10-18 ~20:50, user eyes; the return value is always null, mud-gotchas): decals SHOW on our
  ground (MUD_CoarseChunkEntity / MUD_FinePatchEntity) and on the road, spray-mod call at the wheel; the mortar one too.
  They VANISH when the rut cut gets close (patch rebuild, as IDA said). Grass / clutter is not removed, so the mark is
  hard to see in grass.
- Step 2 GRASS-CUT PROBE (2026-10-18, v208 = before, NOT COMPILED): TEMP MUD_GrassCutProbe.c (MUD_DecalProbe now
  false), called from MUD_LookRuts.Tick. Every 4 s, 15 sets, beside the vehicle nearest the camera (out of the
  wheels' own grass flattening): A left 3 m = Prefabs/Mud/MudGrassCutDecal.et (GenericEntity + DecalEntity child,
  ClutterOccluder 1, red Blood10, as RoadForger's track prefabs); B right 3 m = Prefabs/Mud/MudGrassCutMesh.et
  (GenericEntity + MeshObject ClutterOccluder 1 / clip plane 2, as RoadForger's Flower_hider) + our 0.6 x 3 m dirt
  strip via SetObject; C right 5.5 m = the same strip on a plain GenericEntity = control. Log 'MUD grass cut N/15'
  (traced entity under each). Marks live 180 s. Read: B no grass + C grass = the flag survives SetObject.
  Prefab .meta files hand-written (as MudTunnelProbe_*); if 'prefab not loaded' shows, WB has not registered them.
  RUN 1 (WB 20:55-20:58, log + user): all marks made 15/15 (our chunk / patch, road). A (DecalEntity occluder) shows on
  asphalt ONLY - offroad it is drawn on the holed heightmap under our ground, as feared. B (occluder-mesh strip) shows
  offroad and on asphalt; C too (user corrected 2026-10-18: both strips showed offroad, not only B).
  Run 2 change: C = MudGrassCutMeshOff.et (same prefab, ClutterOccluder 0) so B vs C differ only in the flag.
  RUN 2 (WB 21:00, compile 21:00:12, user eyes): grass pokes through B AND C -> ClutterOccluder on the prefab
  MeshObject does NOT cut grass once SetObject puts our mesh in (or occluders do not work over our ground at all -
  unresolved; no control with a real xob on our ground yet). Grass under A (invisible offroad) not judged.
- RoadForger decal look (read 2026-10-18): spawnable prefabs = GenericEntity + DecalEntity child (ClutterOccluder 1,
  scale ~8.4, FarPlane 2-5). Their emats are MatPBRDecal: vanilla Road_Dirt_01_BCR / Road_Dirt_02_NMO / Road_Dirt_01_H
  + an OpacityMap = a black / white track-shaped mask (Assets/Road textures/*.tif -> edds), AlphaTest 0.4,
  AlphaMul 2.2, BlendMode AlphaBlend, DecalCategory Static, SurfaceProperties grass_road1.gamemat. The DecalEntity
  prefabs hide under our ground offroad (run 1 A), but the EMAT can go into World.CreateDecal, which draws on our
  ground (probe run 2) -> track-shaped decal stamps.
- Step 3 TRACK STAMPS (2026-10-18, NOT COMPILED, texture not imported yet): MUD_TrackStamps.c (ENABLED, LOG), called
  from MUD_LookRuts.Tick (MUD_GrassCutProbe now false). Every wheel within 80 m of the camera: one CreateDecal per
  1.0 m rolled, on the contact entity, at the middle of the last step, angle = heading + ANGLE_OFF (0; untested
  which way the texture's V points), SIZE 1.4 m, LIFE 120 s. Material Assets/Mud/MUD_TrackDecal.emat
  {4253A1BA2A9FAF38} = RoadForger's Double_small car.emat minus TCModRotate and DecalCategory Static (Blood10 has
  neither), OpacityMap Assets/Mud/MUD_TrackMask.edds {EBA6950208263F29} (hand-written meta) from MUD_TrackMask.tif =
  RoadForger Singles/Left_track.tif, track blob cropped + centered in a 512 square (track 0.39 of the width).
  Log 'MUD track stamps: N made'. Known before testing: stamps vanish when the patch rebuilds; grass pokes through.
- DECAL TRACK MARK PARKED 2026-10-18 (user, before testing step 3): all three probes / stamps ENABLED false, files kept.
  Focus instead: voxel / look ruts that form right at the wheels (see 'Ruts at the wheel').

## Ruts at the wheel (started 2026-10-18, v209 = before)
Goal (user): the client look rut forms right at the wheels (instead of a decal mark). Today (WB delay meter, logs
2026-10-18 20:58-21:11, driving): wheel -> drawn look mesh ~1.2-2.2 s, ~4-8 m behind; wheel -> look cut ~0.5-1 s
(the cut waits for the REARMOST wheel to be GAP 0.4 m past - by design, visual only; spikes = stops / reversing).
- Step 1 SPLIT DELAY METER (TEMP, NOT COMPILED): one 'MUD delay split 10 s (TEMP)' line next to 'MUD delay look'.
  mark = patch state at its first look mark after its last mesh (ready / building / asleep; MISSING = no fine patch
  there, counted every mark: the look floor is not drawn at all). wait = patch-frames a look-dirty patch's mesh did
  not build, by reason: building, asleep, sheet, every (LOOK_MESH_EVERY 0.25 / Mesh Interval not up), lost (another
  patch got the frame's one mesh), budget (MayRun no), index (index-list job first). mark -> built = cut to mesh
  (queue = rebuild queue, other = a patch's own build steps). Code: MUD_LookRuts NoteMark / NoteWait / NoteBuilt,
  MUD_WorldGround MarkLookDirty + Rebuild + both mesh builders, MUD_FinePatch.m_fLookCutAt. WB numbers = where the
  wait goes, not perf (TESTING RULES).
  RESULT (WB 21:28-21:30, results/2026-10-18-look-rut-delay-split.md, full table): mesh side ~0.1 s (mark -> built
  median 105 ms, MISSING 0); the delay is the cut rule (wait for the rearmost wheel + GAP 0.4 m). ~7 m behind the front
  wheel, inference ~1.3 m behind the rear wheel at ~6 m/s.
- Step 2 PER_WHEEL look cut (user rule 2026-10-18, v210 = before, NOT COMPILED): MUD_LookRuts.PER_WHEEL true - each
  wheel's segment is cut once THAT wheel is GAP 0.4 m past it (front wheel cuts at once). A sample this vehicle wrote
  within TTL (MUD_LookTile.m_aOwn = MUD_LookTrack.m_iId) is measured from the uncut ground and only goes deeper if the
  new cut is deeper - aligned rear wheels add no step; off the front's rut (turning) they cut as usual. Log: 'MUD look
  ruts' line ends 'PER_WHEEL 1, aligned (no extra step) N'. Watch: rear wheels may look a few cm high over the front's
  look rut (visual only, server collider); 'replaced' may rise if the server stacks overlapping cuts and we no longer do.
  RESULT (WB, user screenshot 2026-10-18 ~21:36): look ruts show behind each wheel, but the wheels float over them
  and they still come in late; white / sky-coloured triangles at the rut ends (cause unproven - maybe gaps at a patch
  edge next to an unlowered neighbour).
- Step 3 REAL CUT AT THE WHEEL (v211 = before, NOT COMPILED): MUD_VoxelProbe.RUT_BEHIND false (server cuts the
  segments the wheels just drove, every RUT_EVERY_MS 250 ms - tyres sink into their own rut); look cut GAP 0.4 -> 0.2,
  EVERY 0.1 -> 0.05. User tests on a LIVE server (old stutter was DS / own PC only). Next knob if still late:
  LOOK_MESH_EVERY 0.25 (perf cap).
  WB 21:42 (user): ruts form, but at speed the rear wheel floats then drops. Log 21:43:33: floor settle held the
  collider (12 max-wait releases, 1383 held patch-frames / 10 s, oldest dirty 0.84 s) - ops never go quiet while
  driving. Step 4 (v212 = before, NOT COMPILED): MUD_WorldGround.FLOOR_SETTLE false. If still late: speed-scaled
  RUT_EVERY_MS, or cut ahead of the wheel by speed x delay.
- Step 5 RAGGED DIRT EDGE (v213 = before, NOT COMPILED; user: step 4 much better, but dirt pops in with no grass ->
  dirt transition): MUD_WorldGround.RAGGED - the groove reveal threshold gets world-fixed noise (RevealT / RagNoise,
  both mesh paths), so rut rims turn to dirt in broken patches. Knobs RAG_D 0.035, RAG_BLEND 0.03, RAG_CELLS 3,
  RAG_WHITE 0.35. No extra mesh builds. Voxel brick ground keeps its hard edge. Next if still poppy: short ease drop.
  WB (user): a little better.
- Step 6 LOOK CUT UNDER THE TYRE (v214 = before, NOT COMPILED; user: if it happened under the wheels we could mask
  it): MUD_LookRuts.LEAD - each wheel lays its look segment LEAD_MIN 0.1 + speed x LEAD_S 0.2 (max LEAD_MAX 1.5) m
  ahead and cuts it at once, so the drawn dirt lands under the tyre. Trade-offs: dirt ahead of the tyre after a hard
  stop; tyre may look a few cm high over the rut until the server rut lands.
  WB 21:59 (user: somewhat better, still slow): meter wheel -> drawn avg ~0.2 s, max ~0.4 s; waits = LOOK_MESH_EVERY
  cap + one mesh per frame (perf limits). User pick: LEAD_S 0.2 -> 0.4, LEAD_MAX 1.5 -> 2 (no extra builds), NOT
  COMPILED. Paid option if still slow: LOOK_MESH_EVERY 0.1 / 2 meshes per frame, judged on a live server only.
- Step 7 SKIRT DEPTH (v217 = before, NOT COMPILED; user: much better, still see-through gaps while driving):
  BuildMeshBand skirt = max(Band Skirt 0.3, Rut Max Depth 0.35 + Reveal Gap x 3 + 0.1) = 0.54 m. Inference (not
  proven): a rut drawn on one patch before its neighbour opens a seam crack deeper than the old 0.3 m skirt. Check:
  gaps only on 8 m patch lines? gone now? Collider build uses skirt 0 (unchanged).
- Step 8 RUT RAMP (v221 = before, NOT COMPILED; user 2026-10-18 DS: 'when we make the ruts it shouldnt bump so much'):
  CarveFloor cut full depth up to the tyre every 250 ms flush and stopped dead = a step under the tyre each flush
  (inference: the pumping). MUD_VoxelProbe.RUT_RAMP: each op fades its cut out over the last RUT_RAMP_LEN 1 m of each
  wheel chain and in over 1 m behind the chain start (RampChains / RampW / RampBehind). Weights add to 1 (offline check:
  0 mismatches in 2000 random points, op lengths 0.5-2.5 m), so final depth is unchanged. Op data only (J2 safe).
  Tune: RUT_RAMP_LEN longer = smoother, shorter = more pumping. false = old behaviour.
- DS 2026-10-18 23:40 (steps 7-8 + perf batch compiled): only 2 net corrections in the whole drive (bumps = our ground,
  not netcode); seams mismatched 0; 'replaced by a shallower server rut' 15 (was 160-322). User still sees under the
  ground when driving fast - cause OPEN (needs the user's description).
- Step 9 LEAD AXLE RUTS (user decision, v223 = before, NOT COMPILED): MUD_VoxelProbe.RUT_LEAD_AXLE - only the leading
  axle cuts (front wheels forward, rear wheels reversing; vehicle-local z within RUT_AXLE_TOL 0.5 m), server feel
  ruts and client look ruts alike (LeadAxle; trailing wheels RutWheelOff). One pass per drive-through now, not two.
- Step 10 RUT PASS MAX (user 'first rut smooth', v223 = before, NOT COMPILED): one pass cuts at most 0.06 m (was
  max(0.08, 0.4 x cap) = 0.11 m for 8 t), CarveFloor and the look cut alike; ruts deepen with traffic up to the cap.
- Step 11 LOOK SPREAD (user: holes right at / just ahead of the wheels at speed, v224 = before, NOT COMPILED):
  inference - lazy patches (made, skin still draws them) only went live when the SERVER floor reached them
  (FloorFinish -> floor spread); the client look rut, up to LEAD_MAX 2 m ahead, reached them first, so the live
  neighbour drew the groove up to their border and the skirtless skin left an open step. MarkLookDirty now sends a
  lazy / building patch to the floor spread too (MUD_WorldGround.LOOK_SPREAD). Ruled out: skirt winding (proto test
  264 / 264 skirt tris face out on a border rut patch), mesher holes (log missing 0, overflow 0), MISSING patches (0).
  Check: 'floor spread: N patches (look M)' in 'MUD patches:'; holes gone at speed.
  DS 2026-10-19 00:01 (steps 9-11 compiled): user 'much better', holes gone; client driving 90-107 fps, MUD script
  ~1.1-2.5 ms/frame (was ~3.9 at 22:31); server MUD 2.1-3.0 ms; floor spread look adds 0-6 per 10 s.
- Step 12 FIRST PASS TO DIRT (user: 'the first rut should go down to the dirt and then slowly after each pass we go
  deeper', v225 = before, NOT COMPILED): MUD_VoxelProbe.RUT_FIRST_D 0.15 (user: 0.10 -> 0.12 -> 0.15) x profile on the first pass (6 cm left grass
  patches: the groove is full dirt from ~0.09 m with RAG_D noise), then RUT_PASS_MAX 0.03 per pass (was 0.06) up to the
  cap. Rule by depth so far, not pass count; server cuts plain RUT_FIRST_D (adds up under RUT_RAMP), look tops up.
  Fix 2026-10-19 (user screenshot: spotty dirt in a turn): the server first cut used cf (= fall x ramp weight) without
  / rw, so it got w x w = half depth where two ops fade over each other. Offline sim Tools/rut_first_pass_sim.py (one
  wheel, ops every 250 ms, RampBehind straight back): before - slow r 7 m turn min 0.07 m, 100 % of the centre line
  under the 0.09 m dirt line; straight 4 m/s 50 %. After - min 0.13-0.15 m, 0 % under 0.09 on all four cases.
- Step 13 TRAIL CUT (user 00:14 screenshot: turning, the rear wheels run inside the front rut on grass; v226 = before,
  NOT COMPILED): a trailing wheel also cuts when the ground 0.5 m ahead of it is not a rut >= 0.7 x RUT_FIRST_D deep
  (MUD_VoxelProbe.RUT_TRAIL_CUT / InLeadRut; server = real floor, look = MeshFloor). Straight = rear wheels still ride.
  Open: no hysteresis - a wheel at the rut edge may switch on / off and leave short faded bits (watch turn entry / exit).
  WB 00:19 (user): rear wheels cut driving STRAIGHT too, turn ruts still off the wheels - cause unknown (no counter).
  TEMP probe (MUD_VoxelProbe.TRAIL_PROBE, test kit): 'MUD trail probe:' line every 0.5 s per side - depth 0.5 m ahead
  of a trailing wheel, in rut yes / no, deepest rut across +-0.8 m and its side offset. Archive it after.
- RUT / WHEEL DEBUGGER (user 2026-10-19, NOT COMPILED): Scripts/Game/Mud/MUD_RutDebug.c, Diag exe / WB only, diag menu
  GameCode > Mud > Ruts and wheels: wheel role spheres (green leads / orange off-rut cuts / blue rides; solid server,
  wire look) + arrow to the TRAIL probe point, rut-across line (red = deepest), rut pieces 4 s (yellow server op, cyan
  look), depth dots 3 m (server or drawn floor), DbgUI text table. LeadAxle now gives 2 for a TRAIL CUT wheel.
  WB 00:30 trail-probe log (209 lines, one long right turn ~15-25 m radius): server side 'depth ahead' 0 on every
  line, deepest rut at side -0.5 .. -0.8 m (left = outside of the turn) = rear wheels track inside the front rut,
  TRAIL CUT correct there. Look side often 'in rut 1' at side 0.0 = its OWN look cut (laid up to LEAD_MAX 2 m
  ahead) - on / off flicker. Fix (NOT COMPILED): InLeadRut reads the real rut floor on both sides.
  User: contact balls do not follow the steered front tyres. Debugger TEMP: magenta / violet = WheelGetPosition read
  as world / local + wheel names + steering in the table - find the call that sits on the tyre.
- Step 14 RUT AT THE HUB (user 2026-10-19: the contact ball does not follow the steered tyre; v227 = before):
  first build read WheelGetPosition as world (user named the magenta ball) -> NO ruts; WB 00:38 log: the value is
  vehicle-local (constant x 1.02 z 2.65). Now CoordToParent (NOT COMPILED).
  MUD_VoxelProbe.RutPoint = hub x / z on the contact height, used by server RutWheel, look ruts, LeadAxle, TRAIL CUT
  and the debugger. Physics code (wheel hold, touch) keeps the contact point.

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
  Patch memory (SP tested 2026-10-16, user OK; 20 plain holes at one spot unresolved, seam not run): press arrays only on first write (v110 = before); FLOOR_FREE frees
  floor-only patches out of range, FieldDue rebuilds them near camera / player / vehicle, FloorDirty makes no new
  patches (v111 = before).
  F4 (built 2026-10-16, not compiled, v113 = before): rut floor uses m_aEdgeLock (no buildings) in patches, LockAt
  and bricks (FieldEdgeLocked in Displace) - same on every machine; a rut may now cut inside the building margin.

## Client look ruts (MUD_LookRuts.ENABLED true, built 2026-10-17, NOT COMPILED, v172 = before)
- User 2026-10-17: ruts close to the wheel and at once on the client; the server keeps its batched RUT_BEHIND cut.
- Every rendering machine (clients, WB host; not a DS) cuts a LOCAL look floor GAP 0.4 m behind the rearmost wheel of
  each moving vehicle within 150 m of the camera (CarveFloor's rule, no relax / ridges), every 0.1 s per vehicle.
  Patch MESHES draw min(rut floor, look floor) (MeshFloor in BuildMesh / BuildMeshBand); colliders, hashes, ops, JIP
  never see it. Look-dirty patches build their mesh past the FLOOR_SETTLE hold. A sample drops when the real floor
  reaches it (CATCH 0.02, no rebuild) or after TTL 8 s (rebuild). Plain RUT FIELD ground only (no voxel column).
- Hooks: MUD_VoxelProbe.LookOrigin / LookBase, MUD_WorldGroundEntity.LookDirty + Rebuild bypass, MUD_FinePatch.m_bLookDirty.
- PER-WHEEL GAP (user 2026-10-18, v174 = before): each wheel's look rut is cut GAP 0.4 m behind THAT wheel (front
  ruts show under the truck, not after the rear wheels pass). Look only; the felt server rut keeps 1.5 m behind the
  rearmost wheel. Rear tyres may hover over the drawn groove until the server rut lands (user accepted). NOT COMPILED.
  2026-10-18 later: GAP 0.4 -> -0.05 (user: 'show right under the tyre') - a segment is cut as soon as it is made.
  REVERTED 2026-10-18 with the look layer (user stopped the look work): MUD_LookRuts.c = v174 again (rearmost wheel,
  GAP 0.4). Never tested on its own. The per-wheel version is in checkpoint v176.
- R4 2026-10-18 (research/2026-10-18-mud-zone-rut-perf.md, NOT COMPILED): SHEET_SKIP true = no look ruts where the
  mud sheet draws (the sheet shows the track); 10 s line adds 'segments under the sheet (skipped)'. Server R1 the same
  day: under the sheet the felt rut op goes every 750 ms instead of 250 (RUT_SHEET_MS).
- PERF 2026-10-18 (DS, research/2026-10-18-sheet-alternatives-and-perf.md sec 9-10): look ruts = ~1.9 ms/frame off the
  zone (~25 look meshes / s at ~5.4 ms). look_ruts 0 test: much better fps but the user saw ruts pop in late -> keep them.
  NOT COMPILED (v191 = before): L2 MUD_WorldGroundEntity.LOOK_MESH_EVERY 0.25 s (look-only patch mesh rebuild rate, was
  0.1); L3 MUD_LookRuts.REPLACE_MIN 0.03 (a server rut within 3 cm of the look floor drops the sample with no rebuild;
  10 s line '(+ N within REPLACE_MIN, no rebuild)').
- Log: 'MUD look ruts: 10 s - ...' (no contact = moving vehicles without wheel contacts, e.g. proxies; expired = the
  server cut less than the guess -> mesh pops up).
- Risks: client mesh rebuild cost (perf on DS / game only); proxy vehicles may give no contacts (unverified).
- MP run 1 (2026-10-17, results/2026-10-17-look-ruts-mp.md): works on the own truck; look meshes up to ~75 ms/s
  while driving; 15-30 % samples expired -> REPLACE 3 s fix (not compiled); client crashed on leaving (unresolved).
  Settings switch: look_ruts 1/0 in the CLIENT $profile:MUD_TestKit.txt (A/B still to run).

## Open
- Rut field Stage 5 JIP (user: later): late join after a rutted drive + crater crossing a rut. Robot code is
  re-archived: for disp hash on a DS set DISP_HASH_ALWAYS true; heightfield hashes need Hash Every > 0.
- Options after the freeze (displaced ruts, terrain-look material probes, DC for craters): docs/superpowers/research/2026-10-15-voxel-options-deep-dive.md
- Parked idea first in line: raised side ridges on voxel ruts (master list).
- Soft / fluid mud without the sheet (2026-10-16): docs/superpowers/research/2026-10-16-soft-mud-alternatives.md -
  sheet ingredients as rut-field channels (ridges felt; texture drag, slump, wet look client-only), vanilla 1.9 wet-mud
  particles / muddy vehicles / puddles to probe, and the large-scale catch: rut-field patches are never freed and are not
  counted in the 2000-patch warning (~206 patches per km of ruts).

## OLD RUT ZONE A/B (TEMP, 2026-10-19, v229 = before, NOT COMPILED)
User idea: old pre-voxel heightfield ruts (v001-v009) felt smoother. DS numbers of the RUTS false run (results/2026-10-19-mesh-vs-voxel-ruts-ds.md)
did not show it. Side-by-side test: MUD_OldRutZone.c box x 1048-1118, z 2480-2550 (red wire box + 8 m corner poles, rendering machines) -
wheels inside press heightfield ruts (MUD_WorldGround.Wheels: RutWheelOff + the old Press path), no client look ruts there
(MUD_LookRuts). Outside: voxel ruts. Craters / trenches / tunnels voxel in both. MUD_OldRutZone.ENABLED false = off.
- ARCHIVED 2026-10-19: mud_backups/archive/old-rut-zone-2026-10-19 (run 4 result: heightfield ruts cost more; keep voxel).

## WEAR - grass first, dirt after repeated passes (user 2026-10-19, v230 = before; WORKS user-ok DS 20:20, results/2026-10-19-mesh-vs-voxel-ruts-ds.md run 5)
User: first drive over grass should not break the ground; repeated use does. MUD_VoxelProbe.RUT_FIRST_D 0.15 -> 0 (off), so
every pass cuts at most RUT_PASS_MAX 0.03 at the centre. MUD_WorldGround.FIELD_SOIL_D 0.03 -> 0.08 (soil look on patch mesh +
voxel bricks) and RUT_MAT_D 0.05 -> 0.08 (mud gamemat), so dirt + mud grip start at ~pass 3, ragged noise adds up to 0.035.
Trail rule: a one-pass rut (0.7 x 0.03) counts as a rut to ride in (was 0.05 with FIRST_D off). Knobs: RUT_PASS_MAX = depth per
pass, FIELD_SOIL_D = passes before dirt.

## WEAR COUNT - first pass changes nothing (perf idea 1, 2026-10-19, v233 = before; NOT COMPILED)
Plan: docs/superpowers/plans/2026-10-19-rut-wear-perf-plan.md. MUD_RutTile.m_aWear counts passes on a sample with no rut
floor yet (+ the RUT_RAMP weight, whole 0.01). Below MUD_VoxelProbe.WEAR_PASSES (1) a pass only counts (WearPass): no floor
write, no FloorDirty / QueueBox = no patch rebuild. Pass 2 cuts 0.03, pass 3 0.06 = dirt (FIELD_SOIL_D / RUT_MAT_D 0.08 ->
0.06, so dirt still shows on pass 3). J2: snapshot tile text "key:<floor>;<ridge>~<wear>", self-test compares wear, disp
hash adds it (WEAR_SALT). Trail rule: rear wheels ride any worn track (wear > 0; a full-pass test let them add wear in the
front wheel's ramp fade). Look ruts skip worn-only samples (WearOnly; 'worn only' count in the look ruts 10 s line).
Log: 'MUD rut wear 10 s: N samples only worn (no cut), M samples cut'. WEAR_PASSES 0 = off.
- DIRT HIGHER (user 2026-10-21 'move dirt very close to the grass, like 0.5 cm'; v276 = before, NOT COMPILED):
  MUD_WorldGround.FIELD_SOIL_D 0.06 -> 0.005. Pass 1 still only wears (nothing drawn); the first cut (pass 2, 3 cm)
  now shows broken dirt (ragged noise up to RAG_D 0.035), full dirt by pass 3. RUT_MAT_D stays 0.06 = mud grip unchanged.
