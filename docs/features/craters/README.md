# Craters

**Status: SHIPPED.** Short page - add to it when we work on this feature.

## Files (Scripts/Game/Mud)
MUD_WarheadComponent.c, MUD_VoxelProbe.c (Carve), MUD_BlastCover.c

## Docs
- [2026-10-15-master-list.md](../../superpowers/backlog/2026-10-15-master-list.md)

## Open
- Old crater items (rim, size by charge, hit-entity check) need a re-check against the voxel crater code.
  Answered 2026-10-16 ([full review](../../superpowers/research/2026-10-16-ground-options-full-review.md) N4, C1-C13):
  size by charge + vehicle skip carry over; jitter, outline lobes, stack cap and the 42 deg wall rule do NOT (voxel
  Carve gets only the depth). Untested: bullets through voxel rims, craters next to houses, objects over new craters.
- PIT SHAPE BACK (user 2026-10-18, v215 = before, NOT COMPILED): the Build 1 cosine bowl (walls <= 42 deg) looked like
  a soft saucer next to a real shell-crater photo; the stable backup (2026-10-13, Backups/Terrain duestrionc/Terrain
  deformation 3 stabil) looked closer. MUD_VoxelProbe.CRATER_PIT true = that backup's Carve profile again: half ellipsoid
  (crest 2.2 x depth = steep lip, flat-ish floor) + funnel above the hit + sine rim (0.2 x depth high, depth wide).
  Outline stretch, C3 road fade, C4 roof sphere, U1 tunnel / floor guards kept. false = cosine bowl.
  User check: better, but too wide + rim mounds too wide -> (v216 = before, NOT COMPILED) PIT_CREST 2.2 -> 1.6,
  PIT_RIM_W 1.0 -> 0.5 x depth.
- DEEP HOLES (user 2026-10-18, v216 = before, NOT COMPILED): a mortar in a 2.5 m trench dug nothing (stack cap
  min(2.5, 1.67 x depth) ~2.4 m + FLOOR 3). Now DoBlast: ground already dug to the cap or deeper -> full crater depth
  below the hit; Carve floor = CRATER_FLOOR 5 m under the undug ground (sea guard). Side effect: repeated hits in one
  crater can also go on past the cap, down to 5 m.
- BUMPY RIM + UNEVEN FLOOR (user 2026-10-18, v219 = before, NOT COMPILED): MUD_VoxelProbe.CraterNoise (sum of
  sines, phases from the op's outline roll = same on every machine). PIT_RIM_NOISE 0.5 (rim x 0.5-1.5),
  PIT_FLOOR_NOISE 0.12 x depth in the middle, 0 at the crest.
- ROAD CRATERS, option A (user 2026-10-18, v219 = before, NOT COMPILED): MUD_RoadCrater.c. A blast whose centre is on
  a kept cell (road, asphalt / concrete gamemat, building foot), not an air burst, -> server spawns
  Prefabs/Mud/MudRoadCrater.et (RplComponent; RplProps radius = 1.2 x soft-ground depth, seed). Every machine lays
  LOCALLY from the seed: MudRoadCraterDecal.et (mortar impact emat, size 2.4 x radius, not on a DS) + 4-10 chunks
  MudRoadChunk_V1-4.et (vanilla DebrisPile_Concrete_01_Debris xobs, static ModelGeometry collider, scale 0.25-0.55)
  only on kept cells. MAX 150 live, oldest deleted. No vanilla asphalt chunk exists (pak 2026-10-18).
  UNVERIFIED: collider follows the spawn scale; decal size follows the root scale; vanilla mortar decal may show too.
  Known gap: a ground-floor hit inside a house (kept cell) lays chunks indoors.
  USER 2026-10-18: NO - wants the real deep craters on roads too. MUD_RoadCrater ENABLED false (files kept).
- REAL ROAD CRATERS, option B (user 2026-10-18; PvP only, AI pathing does not matter): hole road / asphalt cells in
  Tools/mud_holes.py too (our ground's hard-cell rule already gives no ruts / no shovel, blasts dig + crater material),
  then the RoadEntity mesh would float over a crater. Step 1 = MUD_RoadHideProbe.c (TEMP, NOT COMPILED): numpad 7
  Show(false), 9 ClearFlags VISIBLE, 8 undo, on the road under the crosshair; 'MUD road probe:' lines give the
  entity's bounds size = one piece or a whole spline. Next depends on it: hide per piece / per crater, or draw roads
  ourselves. FULL PLAN: [2026-10-18-whole-map-cut-plan.md](../../superpowers/plans/2026-10-18-whole-map-cut-plan.md)
  (user idea: cut the whole map, asphalt / concrete 'armored' = no ruts / shovel until a blast breaks it).
  Task 1 DONE: one RoadEntity = a whole road, hiding is all or nothing. User picked 7A = draw our own copy of a
  cratered road. 7A.1 probe: Scripts/WorkbenchGame/Mud/MUD_RoadExportPlugin.c (World Editor 'MUD Export Roads',
  user ran it 2026-10-18) -> PASS: 468 roads with points, width, material; positions match real hits
  ([results](../../superpowers/results/2026-10-18-road-export.md)). Asphalt = 9.2 km, runway concrete 4.0 km.
  U1 (user 2026-10-18): dirt roads + trails armored too. 7A.2 bake DONE: Tools/mud_roads.py -> Data/Mud/mud_roads.c
  (168 armor roads, 51.6 km). U2 (user): no tunnel under an unbroken road.
  7A.3 look probe WRITTEN, NOT COMPILED: MUD_RoadLookProbe.c (numpad 4 copy / 6 A/B / 5 undo).
- More variation (user 2026-10-17, v171 = before, NOT TESTED): Depth Jitter 0.15 -> 0.3 (size 70-130 %, width follows depth), Shape Jitter 0.15 -> 0.35 and each voxel crater rolls 20-100 % of it (MUD_WorldGround.VoxelCrater). Rolled on the server, carried in the op (MP-safe). Softer grass edge: see voxel-ground README.
