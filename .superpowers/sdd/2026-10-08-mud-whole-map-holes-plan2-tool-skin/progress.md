# SDD ledger — plan: docs/superpowers/plans/2026-10-08-mud-whole-map-holes-plan2-tool-skin.md

Started 2026-10-08 23:40. Executor: inline (executing-plans). Spec: docs/superpowers/specs/2026-10-08-mud-whole-map-holes-design.md (read).
No git (user rule): no commits, no worktree; "commits" fields read "no-git".
User rules for this run: world closed before any .ttile write; tile backups -> My Games/ArmaReforgerWorkbench/mud_backups/tiles; user compiles .c and focuses the game for wb_play; Task 2 gate: mask control on a road must log road 1, else stop and re-plan roads to the plugin route; adversarial-reviewer on MUD_HoleMask.c + MUD_CoarseSkin.c before Task 3 done.

Pre-flight:
- T1 -> T2: mask format. T1 parse_mask regexes (BOX `MUD mask box \S+ \S+ cells N N cell S`, KEEP, DONE `keep N runs N`) vs T2 MUD_HoleMask writes `MUD mask box %1 %2 cells %3 %4 cell %5` (floats x0 z0), `keep z %1 x %2 %3`, `done keep .. runs .. road ..`; extra lines `MUD mask 1` / `control` match no regex. Consistent.
- T1 -> T2: census format. verify CBOX `.* cells N x N`, CDONE `holes N runs N ms N truncated D` vs T2 step 2 census edits (same strings). Consistent.
- T2 -> T3: hole cell = GetSurfaceY < -255, census H = skin hole cells. Consistent.
Task 1: Ruling: task-start/task-done scripts need git; no git here (user rule) — ledger lines written by hand with the same fields — cost if wrong: none (record only).
Task 1: RED seen (ModuleNotFoundError mud_holes), GREEN 34 passed; smoke: runs 119325 secs 0.4 keep 1413502 holes 2780802 blocks 4096 tiles 256 / 36934 39060 (= plan Expected).
Task 1: complete (commits no-git, tests: cd Tools && python -m pytest -q → 34 passed)
Task 2: steps 3-4: user compiled MUD_HoleMask.c + MUD_HoleCensus.c, no errors (23:43); layer block swapped to MUD_HoleMaskEntity, awaiting road placement.
Task 2: step 4: mask entity on tarmac road at x 1420.572 y 38.572 z 2185.829 (user, 23:44).
Task 2: step 5 run 23:44 (log logs_2026-10-08_23-08-28): control line `MUD mask control x 1420.57 z 2185.83 road 0 water 0`; box `0 0 cells 2048 2048 cell 2`; progress 256 keep 524288 (= every cell of rows 0-255), 512 keep 960722, 768 keep 1242181; then 1524 x "NULL pointer to instance" in EmitRun; mask file only 12 bytes.
Task 2: Ruling: plan code declared `protected FileHandle m_File;` - FileHandle inherits Managed (api_search), so a plain member is a weak ref and the handle is freed right after Begin() -> NULL in EmitRun. Fixed to `ref FileHandle` in MUD_HoleMask.c and MUD_HoleCensus.c (step 2 edit had the same block) — cost if wrong: one recompile. Needs user recompile.
Task 2: GATE FAILED: control on tarmac road logged road 0 (sample runs before any file write, so the file bug does not explain it). Per user rule: STOP, re-plan roads to the editor plugin route. Also open: rows 0-255 all kept - water or another flag fires everywhere in the south strip (unexplained, no per-cell data since the file died).
Task 2: Ruling: user chose probe first (23:46) before re-planning roads. TEMP Probe() + m_bProbeOnly (default 1) added to MUD_HoleMask.c: near-entity list (6 m), 7 traces by flags/mask/filter, water full+simple at control + south strip — delete once roads decided — cost if wrong: one recompile.
Task 2: PROBE 23:48 (logs_2026-10-08_23-08-28 lines 20588-20609) at road x1420.57 z2185.83: AABB 6 m finds no RoadEntity (Tree x2, SCR_DestructibleEntity x2); every ENTS trace (Projectile / all / Terrain masks, with or without RoadOnly) misses (frac 1); WORLD trace (Projectile and all) hits at y 38.5722 (ground 38.5723) with TraceEnt RoadEntity. Water: control 0; south strip z 50/300/500 and (2000,300), (1,1) all simple 1 full 1 type 1 surfY ~-0.1..-0.2 over ground -66..-144 = real sea; (2000,2000) 0. => rows 0-255 all kept is the sea, correct.
Task 2: Ruling: roads are WORLD geometry, not ENTS -> Sample() road test = WORLD trace + RoadOnly filter + TraceEnt.Type()==RoadEntity check (the check guards against the filter not applying to terrain); plan's ENTS trace was wrong. Gate re-run decides: control must log road 1 — cost if wrong: roads under terrain may be missed (walk check in step 11 catches it). m_bProbeOnly default set to 0, probe code kept until the gate passes.
Task 2: step 5 scan 23:49:58-23:50:15: control `road 1 water 0`, box `0 0 cells 2048 2048 cell 2`, 8 progress lines, done `keep 2724878 runs 19252 road 115440 water 2609474 box 0 ms 9996`, no new errors. File 19256 lines.
Task 2: step 6 GATE PASS (control road 1, road>0, water>0). plan: `grid 2048 x 2048 = 4194304 cells, margin 1: keep 2797926 + holes 1396378 = 4194304 (ok), 1697 blocks in 132 tiles`. Mask copied to docs/superpowers/results/2026-10-08-mask.txt.
Task 2: step 7 picture docs/superpowers/results/2026-10-08-mask.png: road network, airfield runway/taxiways, sea, SW bay, small ponds white; fields/forest black. Shown to user 23:52, awaiting missing-road check.
Task 2: step 8 first write REFUSED by round-trip guard (nothing written): 28 vanilla tiles (39,40,41,42,54,59,75,84,93,94,99,101,102,110,129,157,177,178,189,190,195,202,211,212,216,218,227,229) have an odd chunk (GCTD 43/289 bytes) with NO IFF pad byte; ttile_holes.chunks/build padded.
Task 2: Ruling: plan said ttile_holes.py "uses unchanged"; changed chunks()+build() to unpadded (measured from Exp pak Terrain_39: LRS2 starts at GCTD end). New test test_merge_tile_round_trips_odd_chunk_without_pad_byte RED (round trip failed) -> GREEN; suite 35/35; all 256 Exp pak tiles round-trip — cost if wrong: none seen, every vanilla tile round-trips byte-exact.
Task 2: step 8 write OK: wrote 132 tiles, hole cells 1396378 (ok), per-tile sum 1396378 = plan holes; 0 backups (no prior addon tiles). Table: docs/superpowers/results/2026-10-08-write.txt. Step 9: layer -> MUD_HoleCensusEntity m_sFile mud_census.txt at road coords.
Task 2: step 9 census 23:56: done holes 1396378 runs 16839 ms 767 truncated 0 (= step 8). Step 10: verify -> expected holes 1396378 observed 1396378 missing 0 extra 0, VERIFY OK. Census copied to results/2026-10-08-census.txt.
Task 2: step 11 walk (user 23:59): tarmac ok, dirt path ok, holes everywhere else; some buildings/trees/objects fall through (expected without skin; Task 3 RF4). TEMP probe removed from MUD_HoleMask.c (needs recompile, done with Task 3 compile). Results: docs/superpowers/results/2026-10-08-plan2-task2-holes.md (full per-tile table + checksum).
Task 2: complete (commits no-git, tests: cd Tools && python -m pytest -q → 35 passed; in-engine: census VERIFY OK 1396378/1396378)
Task 3: steps 1-2 written 00:05: MUD_CoarseSkin.c from plan; MUD_BuildingDestructionFix.c line 10 + MUD_CoarseChunkEntity.
Task 3: Ruling: plan HeightAt used a WORLD trace with no filter; probe proved WORLD traces hit RoadEntity, so skin heights near roads / under static objects could come from non-terrain geometry. Added TerrainOnly filter (GenericTerrainEntity) + counter line `MUD coarse: height traces on non-terrain <n> (classes: ..)` — measures whether the filter works on WORLD hits — cost if wrong: filter could drop terrain hits -> trace misses > 0 shows it.
Task 3: Ruling: compile error MUD_CoarseSkin.c(552) Unknown operator '%' - plan Check() wrote (idx % m_iNCX + 0.5) inside a float expression; split into int icx/icz first (also keeps idx / m_iNCX an int division) — cost if wrong: none, same maths.
Task 3: step 3 compiled clean (user 00:04). Step 4: layer -> MUD_CoarseSkinEntity at 1420.572 38.572 2185.829 (defaults: build at init). M923A1 coords before play: 1246.054 39 2175.409.
Task 3: step 5 run 00:05:23 (user played, "fine", M923 did not drop): `built 1697 chunks with holes + 2399 without = 4096 | hole cells 1396378, tris 2792756 | via init`; `work 7610 ms (heights 5322, meshes 424, colliders 1852), wall 7610 ms, 1 frames`; `trace misses 0, joined 0, mem +268016 KB`; `height traces on non-terrain 0`; check `2000 hole cells, skin hit 940, within 5 cm 940, worst 0.000480652 m`. Hole cells 1396378 = census. Check hit 940 != 2000 (Expected 2000).
Task 3: Hypothesis (unverified): Check() runs in the build call before physics registers the new static bodies. Added late Check("late") 5 s after done in EOnFrame; both lines now tagged (build/late). Needs recompile + play.
Task 3: 00:09 late check = `2000 hole cells, skin hit 940, within 5 cm 940, worst 0.000480652 m` - identical to build check => timing hypothesis REJECTED, miss is deterministic.
Task 3: Hypothesis 2 (unverified): only collider slot 0 (first PhysicsGeomDef) of a multi-material chunk is traceable. TEMP diag added to Check(): hit/miss split by "cell material == chunk slot-0 material", geom-count histogram per chunk, first 3 misses traced unfiltered (ENTS mask -1). Needs recompile + play.
Task 3: 00:11 diag: `hit slot0 594 other 346 | miss slot0 562 other 498` => slot-0 hypothesis REJECTED. Geoms per chunk 1:16 2:325 3:585 4:582 5:189 (sum 1697 = chunks with holes). Misses x1525 z1023 (gravel), x2087 z1125 (grass_lush_tall), x2649 z1227 (soil_forest_coniferous): unfiltered ENTS mask -1 trace hits nothing (or a Tree above) - no traceable body under the cell.
Task 3: Next diag: per missed cell, find its chunk entity -> exists?, physics?, world bounds, transform axes, ENTS|WORLD SkinOnly trace; total chunk entity count.
Task 3: 00:12 diag (log lines 22676-22685): all 3 missed cells: chunk entity exists, physics 1, bounds correct, axes identity; ENTS|WORLD SkinOnly trace hits (x1525 z1023 frac 0.450585 -> y 9.06 = skinY 9.0635). 1697 chunk entities. ROOT CAUSE: part of the static chunk bodies is only visible to WORLD traces; skin is complete, the plan's ENTS-only Check() was wrong.
Task 3: Ruling: Check() traces ENTS|WORLD with SkinOnly and counts a hit only if TraceEnt is MUD_CoarseChunkEntity; TEMP diag removed; late check kept (cheap, proves bodies stay) — cost if wrong: none for gameplay. Plan 3 note: any ENTS-only trace for ground (e.g. MUD_GroundEntity probes) will miss ~half the skin - use ENTS|WORLD.
Task 3: step 6 play 00:14 (user, ran OLD Check - not recompiled, check still 940): walking ok everywhere; turning the camera makes whole skin areas vanish + "a lot of cracks"; buildings seen "under map"; no ruts/craters (expected: deformation is Plan 3, no MUD_GroundEntity in Arland layer).
Task 3: Ruling (inference, verify in play): chunks get SetObject after SpawnEntity without Update() -> scene keeps spawn-time empty bounds -> frustum culling drops chunks. Added ent.Update() after mesh + collider (shipped pattern: SCR_DestructionBaseHandler.c 262/267 SetObject then Update; SCR_TerrainMeshEntity sets its mesh in the constructor) — cost if wrong: one more play; next idea would be spawning with the mesh set before insertion.
Task 3: step 5 re-run 2026-10-09 08:26 (new code incl. Check fix + ent.Update): built 1697 + 2399 = 4096, hole cells 1396378, work 5477 ms (heights 3711, meshes 476, colliders 1282), misses 0, non-terrain 0, joined 0, mem +268067 KB; check (build) AND (late) 2000/2000 hit, within 5 cm 2000, worst 0.00048 m. Check PASS. Culling fix (ent.Update) awaits user visual report.
Task 3: step 6 play 08:26 (user): no see-through found after ent.Update (culling fix looks right); STILL: lots of missing grass, buildings look under ground. Log: `TERRAIN (E): Maximum size of index buffer exceeded (A of B)` - first seen 23:55:54 right after the holed tiles loaded, never in vanilla sessions (logs 21:28..22:53 = 0); limit grows each time (750000 -> 2212348 -> 2761192 -> 3141128; today 750000 / 2163460) => caused by our holes, engine seems to regrow the buffer (unverified whether a frame drops terrain). `DataProgression is not ready` = vanilla noise.
Task 3: 08:30 more buffer warnings (user paste): 3030208/3020200, 3304624/3280208, 3593472/3554624, 3849476/3843472 -> new limit = last needed + 250000 each time (3030208+250000=3280208, 3304624+250000=3554624, 3593472+250000=3843472): engine regrows by 250k, need rises as more holed terrain is in view. ~3.8 M indices x 4 B ~ 15 MB. Open: does the exceeding frame drop terrain (flicker)?
Task 3: 2026-10-09 08:40 debug OPEN 2 (buildings under ground): user case = building at x1988.832 y0.269 z3026.14 (editor screenshot, buried in play). Arland.ent in the pak is binary (can't read stored Y offline). Hypothesis (unverified): entities whose height is resolved from the terrain at load get GetSurfaceY = -256 inside holes. TEMP ProbeBuried() added to MUD_CoarseSkin.c (runs after late check): entities within 25 m of the point -> class, prefab, parent, origin, bounds Y, heightmap (WORLD trace) and GetSurfaceY under origin, origin-heightmap. Needs recompile + play.
Task 3: 08:43 probe (logs_2026-10-09_08-26-06): point heightmap 51.6562, surfaceY -256; 25 m sphere at ground height -> 21 entities, 13 non-skin printed (3 bushes, 3 troughs, 2 PoleFence_02, sacks, 2 garbage stacks, StaticModel under SCR_SiteSlotEntity, Powerline). All sit on the heightmap: origin-heightmap 0.0003..0.03 (fence A -0.07, powerline +9 = pole top). NO building in the list => the house is not at ground level at the point; skin height there is right. Probe widened to a full-height AABB column (m_fBottom-300 .. m_fTop+300), trees skipped. Needs recompile + play.
Task 3: 08:45 column probe (user paste): 344 entities; SCR_DestructibleBuildingEntity CowShed_E_01_long {8ED47AC94995D71A} origin <1988.83,0.269,3026.14> = the editor's y exactly, heightmap 51.6562 => 51.39 m low; its Building/GenericEntity parts and nearby props also at y 0.25..5.7 (all ~-46..-51 under heightmap). Not -256+0.269. Earlier sphere: troughs/fences/bushes 15+ m away sit on the heightmap.
Task 3: Found (pak scripts/WorkbenchGame/WorldEditor/SCR_EntityFlagsManagerPlugin.c): EntityFlags.RELATIVE_Y = "ATL": coords Y is an offset, world Y = TryGetTerrainSurfaceY + offset. Hypothesis (unverified): over a hole cell the load-time terrain height is 0 -> RELATIVE_Y roots on holes sit at their raw offset. Probe now prints relY per entity + ProbeMap(): whole-map root census relY/absolute x hole/kept x buried (>1 m under heightmap) + buried-on-hole by class. Control = absolute roots on holes must be 0 buried, relY on kept cells must be 0 buried. Needs recompile + play.
Task 3: 08:50 map census (logs_2026-10-09_08-26-06, 08:50:10): 191263 entities, 180610 roots. CowShed relY 1. relY on hole 58802, buried 57409, all 57409 with |y| < 20 m; relY on kept 7472, buried 64. absolute on hole 91628 buried 10571; absolute on kept 22708 buried 722. Buried-on-hole by class: relY Tree 46254, relY SCR_IndestructibleEnvironmentalEntity 8670, relY DecalEntity 940, relY StaticModelEntity 678, relY SCR_DestructibleBuildingEntity 433, relY SCR_DestructibleEntity 379, relY GenericEntity 43, relY SCR_PowerPole 10, relY SCR_JunctionPowerPole 2 (relY sum 57409 = relY-hole buried, ok); absolute: GenericEntity 6377, SCR_IndestructibleEnvironmentalEntity 3908, SCR_DestructibleEntity 127, StaticModelEntity 77, SCR_DestructibleBuildingEntity 63, SCR_PrefabSpawnPoint 9, GameEntity 7, LakeGenerator 1, SCR_LargeDestructible 1, Building 1 (sum 10571 = abs-hole buried, ok).
Task 3: ROOT CAUSE OPEN 2 CONFIRMED (in play): RELATIVE_Y roots whose origin cell is a hole load at their raw offset (terrain height taken as 0); control relY on kept cells 64/7472 buried. Open: abs-hole buried 11.5% vs abs-kept 3.2% (unexplained), relY-kept 64. Probe extended: depth buckets per group, |y|<20 per group, relY-kept hole-neighbour split + 5 samples, 8 abs-hole samples. Needs recompile + play.
Task 3: 08:54 probe: depth buckets relY-hole d1-3 3447 / d3-10 7441 / d>10 46521 (sum 57409 ok); abs-hole 1828 / 2772 / 5971 (sum 10571 ok); abs-kept 247 / 173 / 302 (sum 722 ok); relY-kept 54 / 10 / 0 (sum 64 ok). relY-kept 64 = 5+1 buildings, 21+37 rocks; samples are coast rocks at heightmap -1.5..5.9 sunk 1.7..7.6 m, 26 with no hole in the 3x3 => vanilla design, not ours. abs-hole first 8 samples = coast GraniteSurfaceStone_03/04 1-3 m down (design) + LakeGenerator. Open: abs-hole d>10 5971 vs abs-kept d>10 302. Probe now tallies abs-hole d>10 by class+prefab and prints every 300th (flags, id). Needs recompile + play.
Task 3: 08:56 abs-hole d>10 tally (5971): prefab-less GenericEntity 5205 (flags 1027, samples y 0.1..6.25 over heightmap 11..88 = same "offset over 0" look); GraniteCliff_02/03/01 249/228/195, GraniteRock_02/01/03 39/23/13 (cliffs deep by design, flags 66819); SCR_PrefabSpawnPoint 5, SCR_DestructibleEntity 4, WateringCan 3, GameEntity 2, + 1 each (radar x2, BoxMortar, FlagPole, LargeDestructible). Probe now prints for every 300th: vobject, child count, components, EntityFlags.RELATIVE_Y value. Needs recompile + play.
Task 3: 08:58 samples (EntityFlags.RELATIVE_Y = 524288; flags 1027 and 66819 lack it): the 5205 prefab-less GenericEntity = building interior props (CounterShop, Broom, RadiatorCastIron, Piano, Rack, LightSwitch, Dresser, Armchair, Chair, Toilet, CabinetMetal, FileOffice; some with RplComponent / SCR_DestructionMultiPhaseComponent / ActionsManager), no children, y 0.1..6.3 over heightmap 11..88. Inference: stored absolute values cannot move with the terrain, so these must be placed from their (buried, relY) building's transform at load -> secondary to the relY cause. Cliffs/rocks d>10 (GraniteCliff/Rock, flags 66819) = sunk by design.
Task 3: OPEN 2 summary: one cause - relY roots over hole cells load with terrain height 0; interiors follow their building. Fix options to user: A keep the 2 m cell under every relY root origin (export XZ list in play, mud_holes.py keep list, rewrite 132 tiles; zero runtime) vs B lift at load.
Task 3: Ruling: user chose "check engine first (IDA)" before picking fix A/B (09:00).
Task 3: IDA 2026-10-09 (Exp 192624 ArmaReforgerSteamDiag.exe): script GetSurfaceY = sub_140B7FCB0 -> world height sub_140A016D0: loops terrains calling sub_140A30820(terrain, x, z, &h); none -> returns false and h = -256 (default vector 0x141BCCD10 = (-34000, -256, -34000, 34000)). sub_140A30820: cell ix=(int)((x-org.x)*invCell), iz=(int)((z-org.z)*invCell); out of grid or sub_1410210F0(terrain, ix, iz) (hole test, that one cell only) -> false; else triangle-interpolated height from the cell's 4 heightmap samples * scale + offset.
Task 3: IDA: entity-source transform build sub_140A1CC20: only for a ROOT source (a2[+0x30] == 0) with Flags & 0x80000 (RELATIVE_Y): if sub_140A016D0(world, coords.x, coords.z, &h) is true -> coords.y += h; if false (origin cell is a hole) -> coords.y stays the raw stored offset. Matches CowShed y 0.269. => keeping exactly the one cell floor(x/2), floor(z/2) under each relY root origin is enough for the load (fix A). Other 0x80000 + height users not read: sub_1413C9290, sub_1413D0DE0, sub_1413D0EC0, sub_1413D43D0, sub_1413D5C50 (likely editor/runtime paths).
Task 3: Ruling: user picked fix A (keep the one cell under each relY root origin), 2026-10-09 ~09:05. Pins NOT grown by the margin (IDA: lookup reads only the origin's own cell).
Task 3: fix A step 1: Tools/mud_holes.py --pins (parse_pins with box/done checksum, duplicate + outside guards; apply_pins after dilate; stats pins_new; verify pins_text; plan/write/verify CLI). Tests: 9 new RED (AttributeError/TypeError: no parse_pins / pins_text) -> GREEN, suite 44 passed.
Task 3: fix A step 2: new Scripts/Game/Mud/MUD_RelYPins.c (whole-map AABB query, root + RELATIVE_Y, cell floor((x-x0)/cell), dedupe, `MUD pins` lines). Called TEMP from MUD_CoarseSkin ProbeBuried (-> $logs:mud_pins.txt, no layer edit) and from MUD_HoleMask Begin (attr m_sPinsFile, default mud_pins.txt). Needs compile + play; then plan with --pins, user closes world, write tiles, census, verify --pins, probe map (relY on hole must be 0).
Task 3: fix A step 3 (09:05 play): `MUD pins done pins 62481 roots 66274 outside 0 ms 328` (roots 66274 = relY hole 58802 + kept 7472, ok). File copied to docs/superpowers/results/2026-10-09-pins.txt. plan without pins = keep 2797926 holes 1396378 (= Task 2); with pins: `pins 62481, of those holes before 55751`, keep 2853677 + holes 1340627 = 4194304 ok (1396378 - 55751 = 1340627 ok), 1697 blocks in 132 tiles. crash.log 08:39 = Workbench "Resources are leaking" assert at play stop (vanilla BlockListApi leak), not ours.
Task 3: fix A step 4 (09:06, user closed world): write --pins exit 0, `wrote 132 tiles, hole cells 1340627 (ok)`; 132 pre-pin addon tiles backed up to mud_backups/tiles. Table: docs/superpowers/results/2026-10-09-write-pins.txt. Check plan (no census layer swap): skin log `hole cells` must be 1340627 and probe map `relY on hole` must be 0 (tests the pin cell indexing directly).
Task 3: fix A VERIFIED in play 09:08 (logs_2026-10-09_09-07-37): skin `hole cells 1340627` (= write), checks build+late 2000/2000; pins 62481 roots 66274; probe map: relY on hole 0 (buried 0), relY on kept 66274 buried 413 (d1-3 381, d3-10 32; 0.6% vs 0.9% pre-fix kept = design sinking); absolute on hole 89987 buried 4031 (was 10571), abs-hole d>10 741 (was 5971; = cliffs/rocks 747 before), prefab-less GenericEntity d>10 line absent = 0 (interiors followed their buildings -> inference confirmed); abs-kept buried 574. CowShed origin y 51.9253 = 51.6562 + 0.269 exact. User: "it seems it worked". OPEN 2 CLOSED.
Task 3: lesson -> check: mud_holes.py write refuses without --pins (or --no-pins on purpose). Test test_write_refuses_without_pins RED (went on to import pakread) -> GREEN; suite 45 passed.

## 2026-10-09 09:17 OPEN 1 (grass) closed + probe clean-up
- User, after the pins write + play: "grass and clutter seems to be all there". OPEN 1 closed with no fix. Inference, not
  measured: the missing clutter was the ~46k RELATIVE_Y trees/bushes buried by holes, fixed by the pins.
- KB saved: world-environment.md holes section, runtime-procedural-mesh.md s4b, 2 probes in recheck_kb.py (24 probes, 0 FAIL).
- MUD_CoarseSkin.c: TEMP ProbeBuried / ProbeMap / m_vProbe / m_fProbeR / m_aProbeHits and its MUD_RelYPins call removed
  (790 -> 613 lines). Ruling: the pins export lives only in MUD_HoleMask.Begin (m_sPinsFile), the tool that feeds a hole write.
- Next: Task 3 step 7 (spread build, budgets 4/8/16 ms), layer edit with world closed.

## 2026-10-09 09:23 Task 3 step 7 run 1: budget 4 ms (layer m_bBuildAtInit 0, m_iBudgetMs 4)
Log logs_2026-10-09_09-07-37/script.log 09:22:56-09:23:24:
- built 1697 + 2399 = 4096, hole cells 1340627, via frames
- work 5804 ms (heights 3981, meshes 533, colliders 1253), wall 22665 ms, 1126 frames, worst frame 7 ms, mem +347187 KB
- trace misses 0; check build 2000/2000/2000 worst 0.00048; late 2000/2000/2000
- NEW: height traces on non-terrain 90663 (RoadEntity). Init build had 0 (roads not built yet at EOnInit). So in a spread
  build 90663 skin heights came from the road top, not the heightmap. The check is blind to it: it samples hole-cell centres
  and its reference is HeightAt itself.
- mem +347 MB vs +264 MB at init: not explained yet (inference: per-frame garbage not yet freed).
- Fix (MUD_CoarseSkin.HeightAt): on a non-terrain hit, trace again from 1 cm under the hit (max 4) until the terrain; log
  count, mean and max gap. Row 1 to be re-run with this code so the 3 budget rows are comparable.
- 09:19:48 Workbench assertion "Resources are leaking" (crash.log), PacejkaPlotTool leaked script instance: a Workbench
  tool on world close, not our scripts (no MUD class named).

## 2026-10-09 09:26 step 7 run 1b: budget 4 ms, with the road retrace (this is the 4 ms row)
- work 6126 ms (heights 4368, meshes 412, colliders 1318), wall 24329 ms, 1208 frames, worst frame 7 ms, mem +353148 KB
- non-terrain 0, retraced under a road 0 (mean 0, max 0); check build + late 2000/2000/2000 worst 0.00048. M923 did not drop (user).
- UNRESOLVED: the 90663 road hits of run 1 did not come back, so the retrace path is not yet exercised and the road gap is
  not measured. Difference between runs (inference): run 1 played right after a world re-open (09:19 reload, 09:22:35 and
  09:22:54 game reloads), run 1b straight after a compile. The retrace stays (no cost when nothing hits a road); the two log
  lines stay so a later run that hits roads measures the gap.
- mem: spread build +347/+353 MB vs init +264 MB, two runs agree; ~+85 MB for the spread build, cause not measured.

## 2026-10-09 09:36 step 7 run 2: budget 8 ms (played after a world re-open)
- work 5813 ms (heights 4101, meshes 436, colliders 1251), wall 15262 ms, 597 frames, worst frame 12 ms, mem +344748 KB
- non-terrain 0, retraced 0; check build + late 2000/2000/2000 worst 0.00048. M923 did not drop (user).
- Road hits still 0 after a world re-open, so the run-1 guess (re-open causes them) is weakened; 90663 remains unexplained.
- 09:34:38 a play started with the old 4 ms layer and stopped mid-build (no "built" line) - a play before the close, not a
  layer problem.

## 2026-10-09 09:45 step 7 run 3: budget 16 ms
- work 5844 ms (heights 4122, meshes 432, colliders 1263), wall 12519 ms, 341 frames, worst frame 19 ms, mem +341292 KB
- non-terrain 0, retraced 0; checks 2000/2000/2000 worst 0.00048. M923 did not drop (user). Step 7 done.
- Step 8: results file docs/superpowers/results/2026-10-09-plan2-task3-skin.md written (all rows + init + run 1 with road
  hits). Waiting for the user's pick of the default.

## 2026-10-09 09:51 step 8 pick
- User: "build at load, add the baked heights later". Ruling: m_bBuildAtInit default already 1 (no code change); layer block
  to coords only (needs world closed). Baked heights = later task (mud_holes.py reads tile heights at write time -> data
  file, skin loads it instead of tracing; est. 5.2 s -> ~1.6 s, not measured).

## 2026-10-09 09:52 step 8 layer reset + step 9
- World closed (user): default.layer skin block back to coords only (build at load = attribute default 1).
- Step 9: user "archive rather than delete" (then "or delete it is fine"); archived: MUD_WorldSkin.c -> mud_backups/archive/
  scripts/. Ruling: removed `|| MUD_WorldSkinChunkEntity.Cast(entity)` from MUD_BuildingDestructionFix.c line 10, or it
  would not compile. grep: no WorldSkin refs left in .c/.layer/.et. Needs user compile.
- Next: adversarial-reviewer on MUD_HoleMask.c + MUD_CoarseSkin.c + MUD_RelYPins.c, then final whole-plan review.

## 2026-10-09 10:00 adversarial review (MUD_HoleMask.c, MUD_CoarseSkin.c, MUD_RelYPins.c) - verdict CONCERNS
W1 shipped scripts read GetSurfaceY: 72 of 5627 scripts/Game/*.c, 134 calls (pak Exp, 2026-10-09; top SCR_AIGroup 12,
   SCR_DestructibleBuildingComponent 8, SCR_TerrainHelper 8; SCR_TerrainHelper.GetTerrainY falls back to GetSurfaceY when
   its optional trace is not given). On a hole cell they get -256. Not a defect of these files; untested risk -> Plan 3 tests.
W2 HeightAt: road retrace with nothing under the road keeps the road height and clears TraceEnt, so it is counted nowhere.
W3 HeightAt: on a trace miss the fallback is GetSurfaceY, which is -256 exactly in a hole -> a 300 m spike. Counted (misses 0).
W4 BuildChunk: map not a multiple of 32 cells -> cells past the edge read -256 (no terrain) and get skin. Arland 2048 ok.
W5 Check() reference = HeightAt (same source as the skin): proves placement, blind to wrong heights (missed the road hits).
N1 RelYPins query uses default query flags; verified equal to the ALL-flags census only on Arland (relY on hole 0).
N2 Pins cover entities present at scan time; runtime-spawned RELATIVE_Y prefabs on holes untested (KB open item).
N3 m_iCheckCells 2000 on by default: 4000 traces per load in a shipped mod. N4 MeshObject.Create failure turns render off
   for the rest (solid but invisible). N5 no automated tests on the Enforce side (python tool has 45).
Review fixes (user: "fix 2, 3 and 4 now"), MUD_CoarseSkin.c:
- W2: m_iRoadKept counts a road hit with no terrain under it; printed at the end of the "retraced under a road" line.
- W3: trace miss -> GetSurfaceY only if > -255, else m_fLastGood (last traced height, set on every hit; a neighbour sample in
  the build loops). Still counted in trace misses.
- W4: BuildChunk skips cells with ox + hx >= m_iNCX or oz + hz >= m_iNCZ (never holes). Arland: no change expected.
- W1 -> top of Plan 3 task 1 (AI spawn, destroyed building, explosion on a hole cell). W5 + N3 -> baked-heights task.
Needs compile + one play: expect same numbers as 09:16 (4096 / 1340627 / 2000) plus "road height kept ... 0".
10:01 play after review fixes (build at load): 4096 chunks, hole cells 1340627, work 5942 ms, misses 0, non-terrain 0,
 retraced 0, road kept 0, mem +263778 KB, checks build + late 2000/2000/2000 worst 0.00048. Same as 09:16: fixes changed nothing
 on Arland (as expected). "Resources are leaking" asserts (09:19, 09:58, 10:00) on script reload name only PacejkaPlotTool /
 BlockListApi - not in our scripts (grep), also in the 2026-10-08 23:08 session and in results/census-baseline.log (vanilla
 control). Not ours. Task 3 code done; next: final whole-plan review.

## 2026-10-09 10:05 FINAL WHOLE-PLAN REVIEW - Plan 2 DONE
- Every step has ledger evidence: T1 1-5 (lines 12-13), T2 1-11 (14-30), T3 1-9 (34-125). 25/25 plan checkboxes ticked.
- Tests: cd Tools && python -m pytest -q -> 45 passed (10:04). In-engine 10:01: 4096 chunks, 1340627 hole cells (= write
  --pins), checks 2000/2000, misses 0. No TEMP code left in Scripts/Game/Mud or Scripts/WorkbenchGame/Mud (grep).
- Goal met: Arland holed except roads/water/pinned cells, playable on the skin (walk, M923, buildings/trees/grass back).
- Deviations beyond the plan: holes 1396378 -> 1340627 (pins, fix A); road retrace + miss fallback + edge guard in HeightAt /
  BuildChunk; MUD_WorldSkin.c archived to mud_backups/archive/scripts (not deleted).
- Carried to Plan 3 (none blocks Plan 2):
  1. Review W1: 72 shipped scripts / 134 calls read GetSurfaceY (-256 on holes): test AI spawn, destroyed building,
     explosion on a hole cell first.
  2. Runtime-spawned RELATIVE_Y prefabs on hole cells untested (pins cover scan-time entities only).
  3. Step 6 item 3 (cracks / LOD at hole edges from 300 m up) never looked at on purpose.
  4. 90663 road hits in one spread run, unexplained (default init build not affected; retrace now covers it).
  5. Baked heights task (user 09:51): replaces the sampler, fixes Check blindness (W5), set m_iCheckCells default 0 (N3).
  6. TERRAIN (E) index buffer warnings (engine regrows); spread build +85 MB unexplained (spread not default).
  7. MP / JIP untested (spec step 5).
