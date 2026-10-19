# Whole-map cut: real craters on roads, asphalt and concrete

Date: 2026-10-18. Status: PLAN, nothing built except Task 1's probe (NOT COMPILED).
Checkpoint before this plan's code: v219 (+ the probe edits after it).

## Goal (user 2026-10-18)
Cut the terrain out under roads, asphalt and concrete too, so our ground is everywhere except under buildings, power
poles and water. On those new cells ("armored" ground):
- no ruts, no shovel, no trench, no tunnel;
- explosions dig the normal deep pit crater;
- where a blast has broken the armor it is dirt: ruts, shovel, trenches and tunnels work there like offroad.
PvP only: AI pathing / navmesh does not matter (memory pvp-only-no-ai).

## What already exists (read from the code 2026-10-18)
- Map cut: MUD_KeepScan.c (TEMP, TEST_KIT) writes the keep mask + pin list on our holed map; Tools/mud_holes.py
  plan / verify / write cuts the .ttile holes into worlds/MudArland; mud_holes.py fit + bake writes Data/Mud/mud_heights.c.
  Today kept = real roads (not trails) + water + MUD_CutBoxEntity boxes + painted asphalt / concrete terrain (HARD_WORDS)
  + pins (Building, SCR_PowerPole). Last write: results/2026-10-17-write-map3.txt (holes 1400844 of 4194304 cells;
  mask3: road 103532, hard terrain cells 77439 - asphalt 13957, concrete 42285 kept).
- Skin (MUD_CoarseSkin.c ~566): per hole cell the gamemat of a WORLD trace at the cell centre goes into one of
  MAT_SLOTS = 6 slots per chunk; a 7th surface JOINS SLOT 0 (m_iJoined). A WORLD trace hits RoadEntity on roads
  (mud-gotchas), so a holed road cell should get the ROAD's gamemat - unmeasured.
- Heightfield hard rule (MUD_WorldGround m_sHardWords "asphalt,concrete", IsHardMat, BreakHard): hard hole cells take
  no ruts / shovel, explosions dig, a dug hard cell turns into m_CraterMaterial and soft. Only the heightfield knows it.
- Voxel ground (does all digging today, SURFACE_OPS true): a sample is m_aHard = kept terrain (or within 0.01 m of
  it), never dug by ANY op (Carve, CarveSphere, CarveRut, AheadCells, CarveTunnel, CutTrench, FillTrench, SmoothBox,
  FloorBase, ColTop, ColSurf). There is no "hard until blasted" state.
- Voxel look: tops draw MAT_TOP = default_MatTerrainObj1.emat (draws the terrain's painted layers where it is), dug /
  raised faces draw MAT_DIRT (GRASS_TOL rule). So painted asphalt / concrete should still look like asphalt after the cut
  (INFERENCE - Task 2 checks it); a broken spot draws dirt by the existing rule.
- Road meshes: RoadEntity lies on top of the ground. Over a crater it would float. Whether script can hide one, how long
  one entity is and whether it has a collider: unknown (Task 1).

## Open user calls (answer before Task 4)
- U1 Dirt / gravel ROADS: armored like asphalt (no ruts), or ruttable dirt like offroad? Default in this plan: by the
  road's gamemat - asphalt / concrete words = armored, anything else = soft ground.
  ANSWER (user 2026-10-18): dirt roads and trails are ARMORED too until blasted. So every road cell (any RoadEntity
  footprint, any gamemat) plus hard (asphalt / concrete) terrain cells = armored. Task 2 counts road cells by road, not
  by gamemat; Task 3 sets CELL_ARMOR on them.
- U2 Tunnels from a field under a road: blocked at the road edge (armor blocks the whole column) or allowed below the
  surface? Default: blocked until a blast breaks the road above.
  ANSWER (user 2026-10-18): blocked - armor blocks the whole column until a blast breaks the road above.
- U3 What a cratered road looks like where the road mesh had to go (depends on Task 1): road mesh hidden only over the
  crater, the whole road piece hidden, or our own asphalt look for road cells.

## Task 1 - Road hide probe: DONE 2026-10-18 (results/2026-10-18-road-hide-probe.md)
RESULT: hiding works (Show(false) and the flag), but one RoadEntity = a WHOLE road (78-550 m), and roads have NO
collider (physics 0) - vehicles already drive on the ground under them. So Task 7 cannot hide just the bit over a
crater; see Task 7 options.
Original task text:
Scripts/Game/Mud/MUD_RoadHideProbe.c, ticked from MUD_LookRuts.Tick (rendering machines). Crosshair on a road:
Numpad 7 = RoadEntity.Show(false), Numpad 9 = ClearFlags(EntityFlags.VISIBLE), Numpad 8 = undo all.
Each press logs 'MUD road probe: hit <class> ... bounds size <v> m, physics <0/1>, flags ...'.
User test (WB): main asphalt road, dirt road, town street; walk / drive where the road vanished.
Gate - record in results/2026-10-18-road-hide-probe.md, one row per road (class, bounds size, physics, 7 worked?,
9 worked?, what the user saw underneath):
- A road vanishes with 7 or 9 -> hiding works. Bounds size says one short piece (< ~50 m) or a whole spline.
- physics 1 -> the road has its own collider: vehicles would ride it over a crater like a bridge. Then Task 7 must
  also turn its collider off (probe that in Task 7, not assumed).
- Nothing vanishes with either -> stop; U3 becomes "draw roads ourselves", re-plan Task 7 with the user.

## Task 2 - Measure the premises before cutting (KeepScan dry run, committed flags)
Add to MUD_KeepScan (TEMP tool, archive after): `static const bool KEEP_ROADS`, `KEEP_HARD` (both true = today's mask)
and `static const bool MAT_REPORT`. With MAT_REPORT, every cell that today's mask keeps for road or hard terrain logs to
a histogram: 'MUD keepscan: armor road <gamemat> cells <n>' (WORLD trace at the centre - the same trace the skin
does, so this is the gamemat the skin will store) and 'MUD keepscan: armor terrain <gamemat> cells <n>'.
Also count per 128 m skin chunk how many distinct gamemats it would hold after the cut; log chunks over MAT_SLOTS.
Gate (write the WHOLE histogram into results/2026-10-18-armor-mats.txt, buckets sum = road + hard cells):
- Road cells: which gamemats (asphalt? dirt_road? gravel?). Rules on U1.
- Chunks over 6 surfaces: if any armored cell would join slot 0 under a soft gamemat, the armor flag must NOT come
  from the slot -> Task 3 uses a cell bit (planned that way anyway; this measures how often the old way would fail).
- Look check (one item for the user): pick one painted-asphalt town square, set its cells to holes in a TEST AREA only
  (mud_holes.py area --rect), confirm in WB that our ground there looks like the asphalt (MAT_TOP draws terrain paint).
  If it looks wrong -> stop and re-plan the look with the user before Task 8.

## Task 3 - Armor flag in the skin (every machine, static data)
MUD_CoarseSkin: a CELL_ARMOR bit in m_aCell next to CELL_TLBR, set at chunk build when the centre trace's gamemat
path contains a HARD_WORDS word (asphalt, concrete; one list shared with MUD_WorldGround m_sHardWords and KeepScan).
`bool IsArmorCell(int cx, int cz)`. Same world + same trace on server and clients = same bits (rule 3). Log count.
Check: the logged armor count equals Task 2's armored cells (road asphalt + hard terrain) within the margin cells.
U1 CHANGE (2026-10-18): dirt road / trail gamemats have no hard word, so the gamemat rule alone misses them. Armor =
hard gamemat OR cell centre within width / 2 of a baked road line (Data/Mud/mud_roads.c, 7A.2) whose ARMOR flag is set.
The bake sets it by material, not Type (Type 255 holds real Road_Dirt_01 / Road_Asphalt_E_03 roads too): material under
Structures/Infrastructure/Roads/ or RunwayConcrete, or no material; markings, flower beds, beach debris, panels no). Static file + same test = same bits everywhere (rule 3).

## Task 4 - Voxel ground: armored until blasted (MUD_VoxelProbe.c, CRLF - byte-safe edits)
- Column seed: `int m_aArmor[SIZE_N]` (int, as m_aHard), 1 where the skin cell IsArmorCell and the sample is not hard.
- Ops: every op that today skips m_aHard ALSO skips an armored sample, EXCEPT Carve (crater) and CarveSphere
  (blast under a roof): they dig it like soft ground. List to change (grep m_aHard[ to recheck): CarveRut, AheadCells,
  CarveTunnel, CutTrench, FillTrench, SmoothBox, FloorBase (rut floor), plus any trench / shovel server count
  (TrenchCols) so armored pieces show red / refuse like hard ones.
- Breaking: a sample stops being armored once a crater moved its surface by more than CARVE_TOL (rim or dug).
- MP / J2 rule: joiners take columns from snapshots, not by replaying ops. So "broken" must be readable from what a
  joiner has: DERIVE it - armored now = seed armor AND the column's top within CARVE_TOL of its seeded top - instead of
  storing a cleared flag. Check first (Task 4 step 1) what a J2 snapshot carries per column and that ColTop on a joiner
  equals the server's after a crater (existing op-hash tools: Tools/compare_op_hashes.py). If the top is not exact on
  joiners, store the armor bits in the snapshot instead.
- Crater fade (C3 CraterFade / HardSample) must NOT fade at armor - only at kept / hard samples.
- Look ruts (MUD_LookRuts, client visual cut) and DISPLACE_RUTS paths: skip armored samples (grep their own hard checks).

## Task 5 - Heightfield side
MUD_WorldGround already treats hard gamemat hole cells (IsHardMat -> no ruts / shovel, BreakHard on a crater). Check the
road cells reach it the same way (their skin gamemat from Task 2). Crater(): the kept-cell skip stays (buildings, water
banks, poles); road and asphalt cells are holes now, so they go to VoxelCrater like any field.
MUD_RoadCrater (debris, ENABLED false): archive to mud_backups/archive/road-debris-2026-10-18/ with a README of its
call site (MUD_WorldGround.Crater kept-cell branch) once Task 8 works.

## Task 6 - Shovel, trench plan, tunnel input
Client-side checks that show red / refuse (MUD_Dig, MUD_TrenchPlanner / MUD_TrenchLayout, tunnel input): grep where they
ask Hard / Kept today and add armor (server stays the authority: Task 4's op skips). Message for the player: none
needed beyond the existing red piece.

## Task 7 - Road meshes over craters
After Task 1 (whole roads only, no collider), options for U3:
- 7A Our own road copy, only for cratered roads: a World Editor plugin (as the archived MUD_RoadListPlugin) exports
  every road's spline points, width and material to a baked file. When the first crater hits a road, every machine
  hides that RoadEntity and draws OUR ribbon mesh of the same road (same material, width, UV along the road) in short
  pieces, with a hole cut where craters are. Untouched roads stay vanilla (no cost). Unknown: whether the plugin can
  read points / width / material (probe first), and how close our UVs get to the vanilla look.
  USER PICK 2026-10-18: 7A. Steps:
  7A.1 DONE 2026-10-18, PASS (results/2026-10-18-road-export.md; raw per-road file results/data/2026-10-18-mud_road_export.txt):
       468 roads, all with points + width; 9/9 probe hits inside their road; 11 roads lack a material; 0 parent shapes.
       Was: Scripts/WorkbenchGame/Mud/MUD_RoadExportPlugin.c, World Editor menu
       'MUD Export Roads' on MudArland -> $profile:mud_road_export.txt. Gate (results/2026-10-18-road-export.md, whole
       per-road table: width, material, spline count, parent class, tess count): every road has a material and a width,
       and either its own spline points or a parent shape whose tessellated points follow the road (plot 3 roads over
       the map: the line must run down the road's middle). Roads with neither -> list them; re-plan with the user.
       Match the export to play-time RoadEntity by origin (as MUD_KeepScan + the old road list did, MATCH 0.1 m).
  7A.2 DONE 2026-10-18: Tools/mud_roads.py bake (tests Tools/test_mud_roads.py, 10 pass) -> Data/Mud/mud_roads.c,
       text read by ReadLine (format in the tool's docstring): 468 roads, 168 ARMOR (51.6 of 75.6 km), 5004 points,
       176 KB. NOT resampled (raw points; curves open until 7A.3). `mud_roads.py check --hits
       results/data/2026-10-18-road-probe-hits.txt` = 9/9 inside. Plugin now writes world points via GetTransform
       (NOT COMPILED, re-export not needed for this bake). Was:
       Tools/mud_roads.py turns the export into Data/Mud/mud_roads.c (binary, '.c' for FileIO): per road origin,
       width, material, a centre line resampled every ~2 m. Same file on every machine (rule 3).
  7A.3 WRITTEN 2026-10-18, NOT COMPILED: Scripts/Game/Mud/MUD_RoadLookProbe.c (TEMP, ticked from MUD_LookRuts next to
       the hide probe). Numpad 4 = hide the road under the crosshair + draw our ribbon of the whole road from
       mud_roads.c (2 m steps, 5 verts across, 20 m pieces, y = traced ground + 2 cm, V = distance / width);
       numpad 6 = A/B swap vanilla / ours; numpad 5 = undo. 'MUD road look:' lines.
       LOOK 1 (user 2026-10-18 00:13, WB log: road 98, 239 samples, 24 pieces, 3-4 ms): two lanes with a grass strip
       between + streaks along the road. Emat = MatPBRRoad, NumVersions 2 (two road versions side by side, alpha
       between, GlobalMapsUVTransform TilingV 0.5) -> U 0-1 drew both. Added numpad 1 (V scale 1 / 0.25 / 4 / 16 /
       1/16) + numpad 2 (UV mode: U 0-1, U 0-0.5, U 0.5-1, swapped x2), default U 0-0.5.
       CURVES (user: "the roads are on splines"): export had 0 parent shapes, so the bake joins points straight.
       Plugin now also tries runtime parents (SplineShapeEntity > RoadGeneratorEntity > RoadEntity, per
       RoadGeneratorEntity.c) for the tessellated curve, writes it for ALL roads, and writes explicit tangents per
       point (Data[0] SplinePointData). Bake uses the curve when present ('curve <n>' in the done line). Was:
       Look probe before any crater logic: in WB, hide ONE road and draw our ribbon of it (MeshObject.Create, the
       road's own .emat, U across 0-1, V = distance / width along the line, y = our ground + 2 cm, pieces of ~20 m).
       User compares with the vanilla road (markings, edges, tiling). Gate: user says it looks close enough, else try
       V scale / edge blend once, then re-plan with the user.
  7A.4 Crater logic: when a crater op goes live (and from the broken-armor state for JIP, Task 4), every machine finds
       roads whose line passes within the crater reach: first time -> hide that RoadEntity, build our pieces of it;
       the pieces under the crater are rebuilt with the triangles inside the crater's dug area removed (rim edge
       ragged like the pit outline). Our pieces sit on our ground's height so they follow ruts / rims nowhere (armor:
       no ruts) - only the cut-out changes.
  7A.5 Cost: only cratered roads get our pieces; measure piece count / build ms on the DS, not WB.
- 7B Hide the whole road on its first crater; our ground draws terrain paint there (may not look like a road).
- 7C Leave roads, accept a floating strip over craters (no).
Original text (before Task 1):
- Short pieces: at crater go-live, every machine hides the RoadEntity pieces whose bounds touch the crater's reach
  (Show(false) or the flag that worked), and turns their collider off if Task 1 showed physics. Deterministic (op-driven,
  replayed / snapshot-derived on joiners: hide where any column under the road is broken).
- Whole splines only: U3 decides - hide the whole road (our ground draws terrain paint under it, may not look like a
  road) or keep the road and accept a floating strip (no).
- Probe first, then build: a hidden road's collider state is checked in WB (walk over the hidden part).

## Task 8 - Cut the map
1. Checkpoint save "before whole-map cut". mud_holes.py write backs up tiles (--backup <dir>); terrain is NOT in the
   checkpoint - keep that backup dir until the user signs off.
2. KeepScan with KEEP_ROADS false, KEEP_HARD false (water, cut boxes, pins stay) -> mask4 + pins4 (Play ~60 s, SP / WB).
3. `mud_holes.py plan` then `verify`: hole count expected ~1400844 + road + hard cells (Task 2 numbers) minus margin
   overlap; write the count table into results/2026-10-18-write-map4.txt with the checksum line (keep + holes = 4194304).
4. Close the world in WB, `mud_holes.py write ... --pins pins4`, then `fit` + `bake` (new mud_heights.c).
5. Roots on new holes (signs, lamp posts on roads) are RELATIVE_Y -> MUD_PinLift lifts them; check a few in WB.

## Task 9 - WB session (one launch, checklist)
1. Asphalt road, town square, concrete pad: look the same as before the cut (no hole, no grass texture on asphalt).
2. Drive the truck on asphalt: no ruts. Shovel / trench plan on asphalt: red / refused.
3. Mortar on an asphalt road: deep pit crater like offroad; road mesh gone over it (Task 7); rim of dirt.
4. Drive into and out of that crater; ruts inside the crater and on its rim; shovel and a trench inside it work.
5. Tunnel from inside the crater works; tunnel from a field under an unbroken road is blocked (U2 default).
6. Dirt road: behaves per U1.
7. Signs / posts on roads still stand on the ground (PinLift).
8. Read the WB log myself (armor count, 'MUD road probe' / road hide lines, errors).

## Task 10 - MP and perf (end of build, live server - CLAUDE.md testing rules)
Final MP list (plans/2026-10-17-build-3-plan.md): JIP joiner sees the same broken armor and the same hidden road pieces;
trench on a broken road from a client; snaps on the robot circle over a cratered road. Perf only on the DS / standalone
game: more hole cells (~+13 %, Task 2 numbers) = more skin and patch work - measure skin build time and frame cost there.

## Rules for building
- Grep before every edit; MUD_WorldGround.c, MUD_VoxelProbe.c, MUD_FinePatch.c are CRLF (byte-safe python).
- Checkpoint before each code task and after the user confirms it works.
- Adversarial review after Tasks 4, 6 and 7 (bigger Enforce changes).
- Code blocks in this plan: none on purpose - every task names the place, the coder reads the real code first.
