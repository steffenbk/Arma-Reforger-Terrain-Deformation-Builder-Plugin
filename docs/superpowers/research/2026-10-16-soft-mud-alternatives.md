# Soft soil / fluid mud without the mud sheet - alternatives (2026-10-16)

User: "We tried the mud sheet and found it is not possible on a large scale, takes too much data. What other
alternatives do we have for a similar effect? The ruts we have now can't show soft soil or more dynamic fluid mud."
Labels: CODE / VERIFIED-CODE (read in Scripts/ today), MEASURED (offline, results/2026-10-16-mud-look-checks.md;
or earlier logs, cited), PROVEN / IDA (earlier docs), READ (web search text - the network policy blocked the pages),
ESTIMATE, INFERENCE. Plan only - nothing built. Feature freeze: these are new look features; the user decides.

## 1. Short answer
- **CORRECTION 2026-10-17:** the 0.25 m grid check below measured ridge HEIGHT only. The look prototype
  (results/2026-10-17-mud-look-proto.md) shows the sheet's groove does not survive on the 0.25 m mesh; the sheet look
  needs a ~0.125 m mesh near the camera.
- **The sheet's data was never the problem - its MESHES and its being a second ground were.** Its per-tile DATA is
  10.6 KB per 3 m tile (6 arrays x 441 floats, CODE MUD_MudSheetTileData); the measured cost was ~178 KB per tile
  (assessment 2026-10-15, noisy) because every zone tile was pre-built as a mesh + collider and kept. Our rut field
  already stores ruts map-wide at 1 KB per 4 m tile (MUD_RutTile, 256 floats, CODE) - 20x less per m2 than the
  sheet's data, ~300x less than its measured memory.
- **The sheet's shape fits on the ground we have.** MEASURED: the 0.25 m patch grid keeps ~90 % of the sheet's side
  ridge (worst 71-79 %; 93-98 % with a 1.6x wider ridge band), at about twice the sheet's shape error (rms 0.02-0.03
  m vs 0.014-0.017). What it loses is the flat box floor of light-vehicle ruts.
- **So: put the sheet's ingredients onto the rut field as extra numbers ("channels"), draw them with the existing
  patch and brick meshes, and keep meshes only near players.** The fluid part (texture drag, slump, wet look,
  particles, clods) is client-only look; the shape part (tyre-width press + ridges) goes in the replicated rut op so
  it is also felt. This is the architecture review's "one grid, several channels" (f1), now with numbers.
- **Free or nearly free vanilla effects to probe first (READ, 1.9 Experimental):** vehicles show MUD particles
  instead of dust on WET surfaces, and vehicles get muddy (cleaned by rain / wading); the weather manager scripts
  water accumulation into "cracks and puddles". If these work on our ground, much of the "wet mud" look costs
  nothing.
- **Large-scale catch found on the way (VERIFIED-CODE):** rut-field patches are never freed and do not count in the
  2000-patch budget warning; ~206 patches per km of ruts (MEASURED offline) at ~56-400 KB each (MEASURED 2026-10-09).
  At whole-map scale the patch MESHES are the cost, exactly like the sheet. Fix: keep the field (data) everywhere,
  drop far patches' meshes / colliders, rebuild them on approach (section 4).

## 2. What made the sheet look soft and fluid (VERIFIED-CODE, MUD_MudSheet.c)
| Ingredient | Code | Look it gave | Needs |
|---|---|---|---|
| Tyre-width press | BOX, HALF_PER_R 0.38 x radius (0.11-0.28 half width), SOFT_WALL 0.35 | a tyre-shaped groove, not a 1 m bowl | shape |
| Side ridges + bow wave | BAND 0.32, H0 0.05 + H_SPEED 0.012/(m/s) + H_SINK 0.8 x sink, max HMAX 0.28; ARCH_H 0.8 round the tyre front | pushed mud | shape |
| Gradual press, first pass narrower | GRADUAL, PRESS_STEP 0.05, W_FIRST 0.55 | ruts build up | shape |
| Texture drag | DRAG 0.5 m per m of tyre travel, DRAG_MAX 0.3 m, per-vertex UV offsets | smeared, dragged mud - the "fluid" cue | per-vertex UV |
| Slump | REPOSE 1.3 rise/run, SLUMP 0.25 of the excess per call at 10 Hz | mud settling / flowing back | animation (mesh rebuilds) |
| Touched = dirt material | TOP_LOOK, TOUCH 0.01 | fresh mud vs grass | material section |
| Mud drag on vehicles | DRAG_TEST, SLOW 0.4, EFF_LOSS 0.35 (SP only, CODE: returns in MP) | bogging feel | forces |
MudRunner's own fluid look is the same list on the GPU: mud height, "mud offset" (texture dragged with the wheel),
track blend, mud particles, mud chunks, splash decals (READ, research/2026-10-14-mudrunner-mud-article.md).

## 3. The options
| # | Option | Gives | How (engine facts) | Data at scale | MP | Status |
|---|---|---|---|---|---|---|
| A | **Sheet-shaped ruts on the rut field**: tyre-width box press + soft wall + side ridges + bow wave, gradual press | soft, pushed-mud SHAPE, felt | CarveFloor writes a 2nd value per floor sample (ridge height, max blend; ridges can only go UP where ground is - patches and brick tops); the press uses the sheet's box profile; both inside the same op 4 | +1 KB per 4 m tile (a 2nd float array) | deterministic (same op, same order) - felt by the server | offline MEASURED fit (section 1); not built |
| B | **Texture drag channel** | the smeared "fluid" cue | per-vertex UVs are ours (PROVEN, clamped 0..1); patch soil UVs are a fixed 0..1 per 8 m patch today (CODE MUD_WorldGround.c:807) -> the patch keeps its own UV buffer for the soil section (1089 x 2 floats) with base + drag; 0.3 m drag = 0.04 UV on an 8 m tile, so the clamp only bites at patch edges | +2 KB per tile, CLIENT-only | none (look only) | not built |
| C | **Slump / settle animation** near the camera | mud visibly settling after the tyre | a client-only offset channel relaxed toward REPOSE for a few seconds after a pass; rebuild only patches within ~15 m at 5-10 Hz. Grid mesh rebuild 0.9-1.1 ms per patch (MEASURED 2026-10-13, band mesher off) | +1 KB per tile, client-only, short-lived | look only: mesh != collider by <= the offset (the MudRunner model: wheels on hard ground, mud drawn above) | not built |
| D | **Wet / fresh-mud material** | dark, wet look in fresh ruts | a 3rd mesh section with its own emat (rule: each section a DISTINCT emat path, PROVEN crash otherwise) for quads whose rut is fresh / deep | +1 byte-ish age per sample, client-only | look only | not built; material to pick |
| E | **Engine wetness + puddles on our ground** | puddles in ruts, wet sheen after rain, for free | READ: BaseWeatherManagerEntity scripts current wetness and "water accumulation" for cracks and puddles; Wetness Render Settings (cracks / puddles flood sharpness). Our tops use MatTerrainObject (renders the terrain's own surface, PROVEN) - INFERENCE: may inherit the terrain wetness; DirtCover_01 (soil) unknown | none | weather is server-set and replicated (vanilla) | PROBE |
| F | **Vanilla 1.9 mud particles + muddy vehicles** | mud spray from wheels, mud-covered trucks | READ: 1.9 Experimental "vehicles now show mud particle effect instead of dust when surfaces are wet"; Dev Report #24: vehicles get dirty with dust or mud depending on the environment, cleaned by rain / wading. Wheel particles come per surface (VehicleDustPerWheelComponent / SCR_VehicleDust map surface -> particle, READ API). Our patches carry the terrain's gamemat (WORLD trace, PROVEN 2026-10-08); bricks dirt.gamemat | none | vanilla | PROBE (does it trigger on our ground?) |
| G | **Mud clods** from spinning wheels in deep wet ruts | flying chunks (MudRunner layer 3) | READ: SCR_DebrisSmallEntity / SCR_DebrisSpawnable: lifetime min/max, priority, a self-capped pool ("never exceed the maximum amount") | none (pool) | client-only effect | PROBE |
| H | **Wheel sink** (WheelSetRadiusState) | tyre visibly IN the soft soil; soft-soil feel | PROVEN-IDA: the runtime radius moves the suspension ray -> body drops; vanilla flat-tyre pattern replicates a small state; tyre mesh likely keeps its size -> drawn sunk (INFERENCE) | 1 byte per wheel | vanilla pattern; snaps to measure | PROBE 1 never run (MUD_RadiusProbe archived) |
| I | **Grass pressed along tracks** (AffectFoliage*) | the track reads in grass, whole map | IDA 2026-10-15: 20-byte obstacle, 1000 / frame, grass stays down ~10 s, recovers ~5 s | none (re-submit near the camera) | client-only | not built |
| J | **Splash / wet decals** beside ruts | mud flecks on grass | IDA: World.CreateDecal builds on MeshObject entities; dropped when the mesh is rebuilt; engine dedupes close stamps | none (engine) | client-only | PROBE (stretch 1, MatPBRDecal) |
| K | **Mud drag forces** | bogging | sheet DRAG_TEST (SP only); MudRunner formulas (IDA 2026-10-14) | per wheel | risky: low grip / forces = vanilla snaps (PROVEN 2026-10-11) | later |

Not options (MEASURED / PROVEN dead ends): a second mud mesh (the sheet: seams, double work, client-only ground in
MP), 0.15 m cells everywhere (patch grid is 33 x 33 per 8 m by design; voxels at 0.25 m cost 3.4-5.6x), GPU / shader
deformation (no custom shaders for mods), own vertex normals / colours / 2nd UV set (IDA: not in the script mesh API),
DYNAMIC_VERTEXES in-place updates (broken normals, IDA 2026-10-15).

## 4. Making it large-scale (the part the sheet never solved)
| | Sheet (as built) | Rut field today | Proposed |
|---|---|---|---|
| Data | 10.6 KB / 9 m2 (CODE) | 1 KB / 16 m2 (CODE) | ~4-5 KB / 16 m2 with ridge + drag + slump + age (client-only channels only near players if wanted) |
| Meshes | every zone tile pre-built and kept (~178 KB / 9 m2 measured incl. meshes) | every rutted patch kept for the match, never freed, may sleep; NOT counted in the 2000-patch warning (VERIFIED-CODE :3817 / :3842 / :3854 / :976) | meshes + colliders only near players; far patches keep the field only and rebuild on approach (HeightsOnly / FarDue machinery exists, CODE :7049-7100) |
| Per 10 km of ruts | (zones only) | ~2063 patches (MEASURED offline) x 56-400 KB = ~0.12-0.8 GB (ESTIMATE) | field ~19-24 MB (ESTIMATE: 4747 rut tiles per 10 km, MEASURED offline in improve-checks J2, x 4-5 KB) + meshes only round players |
| Net | client-only (none) | op stream (JIP grows with time) | same op stream; snapshot JIP later (improvement plan J2) |
The server needs colliders where vehicles drive, not meshes (a DS builds none); clients need meshes near the camera.
Dropping far patch meshes is the same streaming idea the sheet's first "live window" had (assessment section 3:
"what CAN scale"), but the DATA stays everywhere so nothing is lost.

## 5. Tensions to decide (found while checking)
- **Tyre-width ruts vs smooth walls.** The sheet's box rut is tyre-width (0.22-0.56 m) with a steep soft wall
  (SOFT_WALL 0.35 x half); today's rut is 1 m wide (RUT_R 0.5) and the improvement plan R3 proposed WIDER (0.6) to
  keep walls under 45 deg for the one-ray wheel (IDA). Option A brings narrow steep walls back, so the wheel-feel risk
  returns. Way out (INFERENCE, to test): replicate a WIDE soft floor (feel) and draw the narrow tyre groove + ridges
  as look channels a few cm deep (MudRunner: drawn mud above hard ground).
- **Felt vs look-only ridges.** Ridges in the replicated op are felt (climb-out walls - user asked for that,
  raised-ruts-ideas 2026-10-15); slump / drag must stay look-only (client timing differs).
- **Fresh vs settled.** "Dynamic" needs time: an age per sample (client) drives wet material, slump and decals; the
  server never needs it.

## 6. Suggested order (user decides)
1. Probes, one session, no code risk: E (set water accumulation high, look at ruts on patches and bricks), F (wet
   weather, drive on our ground: mud particles? truck gets muddy?), H (radius probe from the archive), I (grass).
2. Large-scale fix first (section 4: far patch meshes dropped, field kept) - it protects everything after it.
3. A (shape channel: ridges + tyre profile) behind a switch; SP look, then MP robot (>= 6 runs).
4. B + D (texture drag + wet section) - client-only.
5. C (slump) near the camera; G (clods) and J (decals) if the probes worked.
6. K (drag forces) last - MP risk.

## Sources (READ via search results; pages blocked by the network policy)
- 1.9 Experimental changelog: mud particles on wet surfaces - https://steamcommunity.com/app/1890860
- Dev Report #24 (vehicles get dirty / cleaned) - https://reforger.armaplatform.com/news/dev-report-24 ; 1.9.0.5 notes - https://reforger.armaplatform.com/news/experimental-september-15-2026
- BaseWeatherManagerEntity API - https://community.bistudio.com/wikidata/external-data/arma-reforger/EnfusionScriptAPIPublic/interfaceBaseWeatherManagerEntity.html
- Resource Manager options (Wetness Render Settings) - https://community.bistudio.com/wiki/Arma_Reforger:Resource_Manager:_Options ; Diag Menu - https://community.bistudio.com/wiki/Arma_Reforger:Diag_Menu
- VehicleDust / SCR_VehicleDust - https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__VehicleDust.html
- SCR_DebrisSmallEntity / SCR_DebrisSpawnable - https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__DebrisSmallEntity.html , https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceSCR__DebrisSpawnable-members.html
- Atmospheric Weather Mod (puddle drying times) - https://reforger.armaplatform.com/workshop/64ED6553B8AF6B62-AtmosphericWeatherMod
- Our earlier reads: research/2026-10-14-mudrunner-mud-article.md (MudRunner layers), 2026-10-15-ida-rut-questions.md (decals, AffectFoliage), 2026-10-15-sinking-deep-dive.md (radius), features/mud-sheet/2026-10-15-assessment.md (sheet costs).
