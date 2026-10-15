# A4 - Triplanar / angle-blend material search (plan item 7)

Measured 2026-10-14 from the Experimental install paks (G:\SteamLibrary\...\Arma Reforger Experimental\addons\data),
script: scratchpad emat_scan.py / emat2.py / emat3.py (pakread.Pak over every .emat).

## Result: no triplanar / world-space material is shipped

- 10361 .emat files scanned. 0 hits for "triplanar", "worldspace", "world_space", "WorldUV", "WorldMapping".
- Every UV source parameter in the paks is a mesh UV set, never world space:

| Parameter | Values found (files) |
|---|---|
| UVSrcGlobalMaps | "UV set 2" 3441, "UV set 1" 2 |
| UVSrcGlobNormal | "UV set 2" 408 |
| UVSrcDetail | "UV set 2" 364 |
| UVSrcMaskMap | "UV set 2" 101 |
| UVSrcCamoAlbedoMap | "UV set 2" 55 |
| UVSrcMCMap | "UV set 2" 31 |
| UVSrcDetailNormalMap | "UV set 2" 29 |
| UVSrcBCRMap2 | "UV set 2" 4 |
| UVSrcDetailNormalMap2 | "UV set 2" 5 |
| GlobalLayerUVSrc | "UV set 2" 3 |
| UVSrcCamoCRMNormalMap | "UV set 2" 2 |

## Candidates that blend by angle or by the terrain (not triplanar)

| Candidate | Class | What it is | Used by | Params |
|---|---|---|---|---|
| Common/Materials/default_MatTerrainObj.emat (+ _TerrShadows, _TerrShadows_disabledclutter) | MatTerrainObject (3 emats) | a mesh drawn with the terrain's OWN painted surface | 43 xobs (graves e.g. Grave_USSR_01/02) + 6 xobs (ErodedEmbankment_01 dirt_*); 3 prefabs ErodedEmbankment_01_terrainmat_{straight,turnin,wave}.et assign it via MeshObject MaterialAssignClass | NormalBlendMinAngle, NormalBlendMaxAngle, NormalBlendAngleCoef, NormalBlendMaxDistance, NormalBlendDistanceCoef, ClutterThresholdAngle, UseOnTerrainHeightmapShadows, DisableInteriorProbes |
| GlobalLayer on MatPBRBasic / MatPBRMulti / MatPBR2Layers | material layer | a second texture layered on by surface angle vs a direction | ~29 emats with GlobalLayerMode Manual (wrecks, remains) | GlobalLayerMode, GlobalLayerDirection, GlobalLayerAngleStart/End, GlobalLayerPosition, GlobalLayerBlending, GlobalLayerUVTransform, GlobalLayerNoiseMap |
| TopDownProjectionAngleEnd | MatPBRMulti | unclear (one file only) | Assets/Props/Industrial/Mining/Mine_01_train/Data/BND30_Body.emat | TopDownProjectionAngleEnd 76.76 |

## What it means for item 7b / 10 (inference, not tested)

- True triplanar is not available from shipped materials.
- MatTerrainObject is the most interesting: our voxel mesh could take Arland's own painted ground at that spot
  (no colour step between heightfield and voxel). Unknown: how it textures steep crater walls (likely top-down
  terrain projection = stretched on walls) and whether it works on a script-made mesh. Needs a probe: assign
  default_MatTerrainObj_TerrShadows.emat to one brick and look.
- GlobalLayer could put a "top" texture on up-facing faces and soil on walls in ONE material, by angle, instead of
  our per-quad UV kind pick. Also needs a probe.
