# Local IDA prompt: "global layer + volumetric" snow - what it is, and can it help the mud? (2026-10-22)

You are the local research AI for the Arma Reforger mud mod (Enfusion script, addon "Testing plan vech msh"). You have
IDA on the game exe (ArmaReforgerSteamDiag.exe - say which build / branch you read) plus the vanilla data (pak /
Workbench). Research only: no build, no edits under Scripts/. Label every claim IDA (sub_ / string address),
DATA (vanilla file path), [repo], WEB or INFERENCE. One pass, short report. If you run low, finish Q1-Q3 first.

## Why
Another modder has snow working "not spawned like our sheet", in his words "global layer and volumetric". His
screenshot: docs/superpowers/research/img/2026-10-22-snow-pit.webp. It shows a rippled snow surface and a
rectangular, prone-sized pit with snow walls around a lying soldier, at night under a light beam.
Our current reading (chat 2026-10-22, NOT proven):
- "Global layer" is the GlobalLayer* slot of the PBR materials. Our own Assets/Mud/MUD_TrackDecal.emat carries
  GlobalLayerContrast / GlobalLayerGlobalAO / GlobalLayerHeightPower. Mods use it for snow:
  - Winterize Terrain: "use global layers for winterizing plants, rocks and buildings".
  - Novka: "Global layer (snow) on rock terrain material".
  - AMT-DynamicSnow: driven by weather / the wetness override.
- That layer is a uniform, weather-driven paint, so it cannot place a track or a pit. The pit is then a placed
  prefab (foxhole / fighting position), terrain sculpted in Workbench, or spawned script meshes / voxels.
Prove or kill this. What matters for us: anything that varies BY POSITION at runtime without spawning meshes, or a
cheap far / near mud look for our patch and sheet meshes.

## Read first
- CLAUDE.md (hard limits, TESTING RULES, PvP only).
- docs/superpowers/research/mud-gotchas.md: grep for 'IDA', 'MeshObject', 'decal', 'emat', 'SetObject', 'NOSHADOW'.
- docs/superpowers/research/2026-10-15-rut-options-round2.md sec E: the earlier exe sweep found no footprints, no
  deformable snow and no runtime heightmap edit. Re-check it on the build you have.
- docs/superpowers/research/2026-10-21-synced-sheet-server-research.md: Problem 1, the far pop-in. The far press
  (option A) and decals (blocked) are the context for Q6.

## Hard limits (do not propose around them)
- No custom shaders. Script meshes have no vertex colour. A script mesh change = a new MeshObject.
- World.CreateDecal returns null on our script meshes (MEASURED).
- Ruts stay voxel. Performance is judged only on a DS / standalone game.

## Questions
**Q1. What is the global layer in the PBR shaders (IDA + DATA)?**
- Find the GlobalLayer* parameter names in the exe: the material class registration (MatPBRBasic, MatPBRMulti,
  MatPBRDecal, MatTerrain*, MatTerrainObject) and their shader constant upload.
- List every global-layer parameter, and every texture or input the layer reads: layer BCR / NMO / height maps,
  a mask map, UV channel 2, the height map with HeightPower, normal-up slope, AO.
- Which material classes have it? Does MatTerrainObject have it? Our ground patches / skin / sheet use
  `{D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat` (MUD_MudSheet.MAT_TOP).
- Find one vanilla winter or snow emat that sets the layer and quote its lines (DATA).

**Q2. What sets the AMOUNT, and can it vary by position or entity?**
- Trace where the global-layer amount comes from at draw time:
  - a world / weather constant (TimeAndWeatherManagerEntity, weather states: snow, wetness, puddles)?
  - the Workbench "wetness override" render setting?
  - a per-entity or per-material instance value?
  - a world-space mask texture (terrain layer, "snow mask", rain / wetness occlusion map)?
- Is it synced in MP? It is weather, so it should be: say how.
- **Key question:** our emats have `OccludeWetness` (Assets/Mud/MUD_TrenchGhost*.emat). Is there a top-down
  wetness / rain occlusion map that objects render into, and does the global layer (snow) read it too? If so, a
  mesh that writes only into that occlusion map would remove snow (or mud) under it by position. Could such a mesh
  be invisible in the main view? A material flag, or a pass mask on a script MeshObject?
- Find any script API that sets a material parameter on one entity at runtime: Material.SetParam,
  ParametricMaterialInstanceComponent, SetObject remap / material override, BaseWorld global material params. Name
  the class and method, and check them against the script API dump.

**Q3. What does "volumetric" mean here?**
- Search the strings: volumetric, Volumetric, POM, parallax, tessellat, displace, snow, Snow, footprint, trail,
  deform, heightfield update, terrain edit.
- Separate these cases: volumetric fog / clouds (unrelated); parallax / height-map blend in the global layer (look
  only, flat at grazing angles); real displacement or tessellation (geometry from a texture); voxels.
- Does any engine path change the ground's real geometry or collider at runtime from data (not a spawned entity)?
  If yes: the API, whether script can reach it, and whether it is synced.

**Q4. The pit in the screenshot.**
- Search vanilla data for prefabs that make a hole around a prone soldier: foxhole, fighting position, dugout,
  trench, entrenchment, the Conflict building compositions. For each, give the .et path, whether its .xob has
  collision, and whether its material has a global layer.
- Say which explanation fits the screenshot best: prefab, terrain sculpted in Workbench, script mesh / voxel, or
  parallax only. List the visible tells you used (lit wall faces, silhouette at the near lip, soldier height vs the
  rim).

**Q5. Do our script meshes get the layer?**
- In the MeshObject.Create path, does a script mesh with an emat that has a global layer draw that layer? Any vertex
  input the layer needs that script meshes lack (vertex colour, UV2, tangent)?
- Does the global-layer amount on an entity follow its world position, or a per-object constant?

**Q6. What this could give the mud (INFERENCE, ranked).** For each idea: works or not, what it costs, MP risk, and
the smallest Workbench probe that proves it.
- A mud / wet global layer on the patch / skin / sheet material, driven by rain. A map-wide look, no spawning.
- Far track look outside the sheet ring:
  - if Q2 finds a position input (occlusion map or mask), a cheap "no snow / no grass-look strip" along tracks;
  - otherwise only the far press from the 2026-10-21 report.
- A height-map / parallax global layer on our sheet or patch material for small relief (ripples, tyre tread) with no
  extra vertices.
- Anything Q3 found that changes ground geometry without spawning (would be a big change of plan - prove it in IDA).

## Report back
Write docs/superpowers/research/2026-10-22-snow-global-layer-ida.md:
1. Answers to Q1-Q6, labelled.
2. One table of global-layer parameters and inputs (name, type, what it does, per-material / per-entity / global).
3. A yes / no line: "position-dependent snow or mud without spawned meshes is possible" (and how), or not.
4. At most 3 Workbench probes for one combined session, written as a numbered checklist (CLAUDE.md TESTING RULES:
   one session, look only, no perf numbers in Workbench).
5. Questions to ask the other modder, if IDA cannot settle it.
Add each PROVEN fact as one line in docs/superpowers/research/mud-gotchas.md.
