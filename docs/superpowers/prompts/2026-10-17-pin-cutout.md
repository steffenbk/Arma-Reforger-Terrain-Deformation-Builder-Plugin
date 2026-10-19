Paste everything below the line into a new chat in the mod folder.
-------------------------------------------------------------------------------------------------------------------
Pin cut-out + craters on roads. Handoff: .remember/remember.md (loads by itself; its '## Now' has the probe numbers).

Never overwrite, revert or stash my local changes. No commits unless I ask.

The problem: when the map was holed (2026-10-09, Tools/mud_holes.py --pins) one 2 m cell of REAL terrain was kept
under every RELATIVE_Y root (66274: trees + bushes, rocks, decals, props, buildings), because the engine skips the
terrain height for such an entity on a hole cell and it loads buried. Over a crater those squares float (my
screenshots 2026-10-17). Roads / water / keep boxes are kept too (MUD_HoleMask, archived).

My rule: real terrain stays ONLY under asphalt, concrete, structures (buildings) and real roads. Everything else gets
holed: trees, bushes, rocks, decals, props AND trail roads (RG_Trail*_01: RoadEntity Type Path, width 1.75,
Trail*_01.emat - same dirt_road.gamemat as dirt roads, and RoadEntity has no material / width in script).

Part A - cut out the pins:
1. First ask me: did the rock lifted by MUD_PinProbe block me? (probe: after SetOrigin +3 m the boulder's collision
   was not hit at the old or new height). If collision is lost, find a move that keeps static collision before
   anything else (WB probe, small).
2. Trails: a Workbench editor plugin lists trail RoadEntity sources (Material contains 'Trail') -> file of origins /
   spline points; the mask scan or mud_holes.py drops road keep cells that only a trail made.
3. mud_holes.py: pins only for Building roots (+ the road keep minus trails). Load-time script: every machine lifts
   the non-building RELATIVE_Y roots on hole cells by the undug ground y (raw y + undug), before anything reads them.
4. Small test area first (tiles of one square), WB check, then the whole map (re-run the tool chain: mask -> write ->
   fit -> bake; world closed in WB; see the tool headers).

Part B - craters on roads / asphalt / concrete (new, after A):
Today kept cells are hard for voxel ops (MUD_VoxelProbe.Kept / HardSample, CraterFade to 0 within 1 m), so a crater
never digs a road. I want craters there too. Known limits: the terrain can't be holed or lowered at runtime
(GenericTerrainEntity is read-only, IDA 2026-10-12), and a RoadEntity mesh is one whole spline. Research first and
explain options in plain words (e.g. hole road cells too and draw the road ourselves, or a crater look that sits on
top of kept terrain), with a recommendation, before building.

How to work:
- Read first: project CLAUDE.md, docs/superpowers/research/mud-gotchas.md (holes, RELATIVE_Y, navmesh, -256 heights),
  Tools/mud_holes.py + Tools/mud_world_copy.py headers, docs/superpowers/results/2026-10-09-plan2-task3-skin.md.
- Checkpoint before each code stage (mud_backups/checkpoint.ps1 save "name") and after I confirm it works.
- CRLF files (MUD_WorldGround.c, MUD_VoxelProbe.c, MUD_FinePatch.c, plans): byte-safe python edits. Grep before editing.
- Testing: WB only, combined sessions; never measure perf in WB (load-time cost of the lift = PERF on DS / game).
  MP items go to the Build 3 plan's "Final MP" list. Read the newest logs/*/console.log after the last
  'Compiling Game scripts took' line myself.
- Archive MUD_PinProbe.c (and its call in MUD_WorldGround EOnFrame) when done: mud_backups/archive/<name>-<date>/.
- Bigger Enforce changes: run the adversarial-reviewer skill before calling them done.
