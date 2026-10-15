# Custom shaders - can a mod make them? (2026-10-16)

Answer: NO. A mod can only pick one of the built-in material classes in an .emat and set its values.

Sources: IDA on `Arma Reforger Experimental/ArmaReforgerSteamDiag.exe`, the shipped packs read with a pak lister
(scratchpad paklist.py, FORM/PAC1, file entry = 24 bytes), and the game console log 2026-10-15 09:06.

## Evidence
| Check | Result | How |
|---|---|---|
| Shader compiler linked in game exe | None. Imports only `d3d12` (D3D12CreateDevice) + `dxgi` | IDA import list |
| Shader compiler in Workbench | None. Workbench exe has no dxcompiler / D3DCompile / DxcCreateInstance string; no dxcompiler.dll in the Workbench folder. Only shader-like string: `FFX_HLSL` | grep on the exe, folder listing |
| Where shaders live | `addons/core/shaders-dx12.pak` (87.8 MB): 4642 files under `system/.shadercacheDX12/`: 2574 .pso, 1640 .vso, 427 .cso, 1 version.txt (= "10"). No shader source files. (Plus shaders-dx12-pdb.pak, 1.04 GB, debug info) | pak lister |
| How shaders load | sub_141154D40 builds `<root>/.shadercacheDX12/<name>`, reads the binary, and on failure logs "Can't load shader binary '%s'." "ShaderCompile" (sub_1411541B0) is only a worker job name for that load. It ends with "%d new shaders detected" / "Loaded %d newer shader binaries" | IDA decompile |
| Version lock | sub_141155490 reads `.shadercacheDX12/version.txt`, needs == 10, else "Invalid %s shader version vs build version" | IDA decompile |
| When shaders load | Game log: "Enfusion shader version 13", "ShaderListReload", "Loaded 3383 newer shader binaries" at 09:06:47, right after mounting core + data and BEFORE any mod is mounted | console.log logs_2026-10-15_09-06-43 |
| Material types | Fixed list of native classes (strings in exe): MatPBRBasic, MatPBRMulti, MatPBR2Layers, MatPBRDecal, MatTerrainObject, MatPBRRoad, MatParticle ... Each one picks named variants (`ps_pbrfull$...`, `ps_enfterrain_detail$...$TERR_OBJ`) | IDA strings, pak names |
| Shipped .emat classes | 10361 emats: MatPBRMulti 4244, MatPBRBasic 3594, MatPBRDecal 640, MatPBRTreeCrown 468, ... MatTerrainObject 3 | scan of all data paks |

Theoretical hack (NOT viable): replace a shipped .pso with hand-made DXIL. Needs the exact resource layout with no
source, signed DXIL, version lock per game build, and shaders load before mods mount. Rejected.

## What IS available: MatTerrainObject options (we already use it)
The IDA exe description for MatTerrainObject is "Terrain material used on standard objects". Our skin uses
`{D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat` (PROVEN 08:01 in mud-gotchas). Settings the
exe registers for it (sub_140BF3DC0):
- Own maps: BCRMap, NMOMap, GlobalMapsUVTransform, roughness / metal / AO scale, FallbackColor ("when there is no
  terrain under object"), ClutterThresholdAngle, UseOnTerrainHeightmapShadows.
- **Material blending**: MaterialBlend, MaterialBlendUseVertexColor, MaterialBlendUseTerrainDistance,
  MaterialBlendMinDistance / MaxDistance / Exponent, MaterialBlendUseHeightMap.
- **Normal blending**: NormalBlendMin/MaxDistance, NormalBlendDistanceCoef, NormalBlendMin/MaxAngle, NormalBlendAngleCoef.

No shipped emat uses any MaterialBlend* key (0 of 10361). The 3 shipped MatTerrainObject emats only set
DisableInteriorProbes, UseOnTerrainHeightmapShadows, ClutterThresholdAngle and the NormalBlend values.

INFERENCE, untested: MaterialBlendUseTerrainDistance might blend from the terrain look to the emat's own BCR/NMO
maps by the distance between the mesh and the real terrain heightmap. Our ruts sit below the untouched
heightmap, so that distance = rut depth -> a mud texture in the ruts for free. Vertex colour is out (MeshObject.Create
has no vertex colours, 2026-10-14-ida-voxel-feasibility.md). No `MATERIALBLEND` key in the shader variant names, so it
is either a shader constant or not used by the ps_enfterrain TERR_OBJ path - only a test can tell.
Parked in the master list.

## Test 1 (built 2026-10-16, v112 = before, waits for user run)
IDA exe tooltips (sub_140BF3DC0): MaterialBlendUseTerrainDistance (checkbox, default 0) "Uses distance from terrain to
blend terrain material and object material"; MinDistance default 0, MaxDistance default 2 m, Exponent default 3
(1 = linear); MaterialBlendUseHeightMap default 1 "use terrain detail maps height map for blending";
MaterialBlendUseVertexColor combobox "0 = terrain, 1 = object".
Edit: `Common/Materials/default_MatTerrainObj1.emat` (same path, so skin + voxel tops + rut field all get it) adds
BCRMap/NMOMap = ST_Mud_01 ({AE37874A11911124} / {96EF3CD8C8E643BA}, shipped, used by RakeManure_01.emat),
UseTerrainDistance 1, Min 0.05, Max 0.3, Exponent 1, UseHeightMap 0. Undo: delete those 6 lines or restore v112.
Result (USER screenshot 2026-10-16 ~00:45, WB log: emat rebuilt 00:43:56, no texture errors): the blend WORKS -
the raised crater rim (> 0.3 m from terrain) switched off the terrain look, the parts near terrain level kept it.
But the object part renders plain WHITE, not mud. Crater floor is the separate DirtCover groove material (unchanged).
USER confirmed: ring was not white before the change. Try 2: BCR/NMO swapped to terrain Dirt_01_BCR {C6A0995C60084476} / Dirt_01_NHO {E197D218634E0261} (still white = maps not sampled; dirt shows = ST_Mud_01 was the issue).
Try 2 result (USER, WB rebuild 00:46:20): rim back to the normal grass terrain look - no dirt, no white. Try 3 = control: ST_Mud_01 put back (does white return?).
STOPPED by user 2026-10-16 before try 3 ran; emat restored to the original 7 lines. Unresolved: is the blend real (try 1 white) or a one-off (try 2 nothing)? Restart = try 3 control.
