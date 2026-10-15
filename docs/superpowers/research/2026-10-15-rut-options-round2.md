# Ruts - options we have NOT explored yet (round 2, 2026-10-15 late)

User: "I feel like we haven't explored enough other options that can possibly give us ruts."
Earlier rounds: voxel ruts (v035), displaced ruts (v097), rut field on patches (built), mud sheet (parked), engine
track decals (terrain only - IDA), World.CreateDecal (mesh decal, lost on rebuild - IDA), grass flattening
(AffectFoliage - IDA), prefab pieces, forces only. See research/2026-10-15-rut-alternatives-deep-dive.md.
Labels: CODE, MEASURED, PROVEN, IDA, INFERENCE (not tested). Nothing here is built.

## The pattern so far
Every built option changes a BIG mesh when a rut changes: an 8 m patch (band mesher + several trimeshes, 6-7 ms,
~10 / s per vehicle - ruts README Stage 4) or a 4 m brick. The cost and the memory problems both come from that.
The ruts themselves are small: a track is ~1 m wide. Options below either make the changing mesh small, or move the
rut out of geometry into texture / engine systems.

## A. "Rut cells" - small 2D rut tiles that replace the ground in their cell (most promising)
- Same trick that already works for craters: a 2 m skin cell can be VOX - the patch skips it (FillBandOn slot -1)
  and the bricks draw it (CODE). Add a second cell kind, RUT: the patch skips it and a tiny rut tile draws it.
- Rut tile = a 2 x 2 m height grid at 0.125 m (17 x 17 verts, 512 tris) or 0.25 m (9 x 9, 128 tris): base ground
  + min(floor). One MeshObject + one trimesh, nothing else (no band mesher, no slab / plate / per-material geoms).
- Crack-free by construction: a cell becomes RUT when the floor reaches within one sample of it, so the floor is
  NONE on every border between a RUT cell and a patch cell -> both sides are the plain base ground there, same
  vertices (patch border verts at 0.25 m; a 0.125 m tile puts its extra border verts on the straight edge between
  them = T-junction on a line, no crack). Same rule the offline test proved for bricks.
- Cost (INFERENCE): a rut op rebuilds 1-3 tiny tiles (~128-512 tris each; CreateTriMesh 2048 tris = 0.28 ms PROVEN,
  so the script loop dominates - estimate well under 1 ms per tile) instead of 1-2 whole patches at 6-7 ms. The
  patch rebuilds only when a cell FIRST turns RUT (one skip-mask change), not on every pass.
- Memory: ~17 x 17 floats + mesh per rutted 2 m cell, freed / rebuilt from the floor tiles like anything else.
  Patches stay plain (could even go back to being freed - the F2 problem mostly disappears).
- Finer than now for free: 0.125 m tiles = sharper walls than the 0.25 m field (offline disp4: 57-59 deg walls), only
  where ruts are.
- Risks: a 3rd ground kind next to patches and bricks (VOX cell + RUT cell meeting: bricks already read the floor,
  so a RUT cell next to a VOX cell needs the same border rule); skin / patch "cell kind" plumbing (VoxCellsChanged
  style); get-in / out hiding (new entity class -> HideFromTraces, the v093 lesson); band mesher must accept a 2nd skip
  kind. Effort: medium-large, but most parts exist (cell masks, floor tiles, Displace-style pull, debug tools).

## B. Tread pattern in the texture (look, no extra geometry)
- Today the soil in a rut uses world-aligned UVs (KindAB). Give the groove triangles TRACK-aligned UVs instead:
  u across the rut (0 at one edge, 1 at the other), v along the track - and a mod emat with a tyre-tread albedo +
  normal map (vanilla tyre track textures exist for the track decals; reuse the textures in our own MatPBR emat -
  not MatPBRDecal). The normal map gives tread relief and fake depth on the real groove.
- Needs per floor sample: the track direction + the distance from the track centre (CarveFloor already computes
  both: u along the segment, dist). Store 2 bytes more per sample, or recompute in the mesher from the op segments.
- UVs are clamped 0..1 (PROVEN): tile along v per segment as the soil tiles do (one tile per quad, tiling in the emat).
- Fits A or the current field; cheap at runtime (UVs only). INFERENCE: biggest look win per hour of work.

## C. Vanilla tyre tracks where we make no ruts
- IDA (rut questions item 1): TrackDecalFactory works on the TERRAIN entity and follows the heightmap. On ground we
  never dig (roads, hard surfaces, kept terrain), our surface IS the heightmap (+1 mm) - the vanilla tracks may just
  work there. Probe: is SCR_VehicleTrackDecal on our test vehicles, and do its tracks show on kept terrain / on our
  undug skin over holes?
- Gives: tracks on roads / hard ground (where ruts are wrong anyway) for free; ruts only on soft ground (G).

## D. Engine spline / road meshes at runtime (IDA question)
- Roads and rivers are engine meshes that follow the ground (RiverMeshObject accepts decals - IDA item 1). If a
  road / shape / spline entity can be SPAWNED and given points at runtime and builds its mesh then, a "track road"
  with a muddy tyre-track material along each vehicle path is an engine-drawn, ground-following rut look.
- Unknown: runtime generation (likely editor-only, Workbench generators). IDA item 11.

## E. Hidden engine deformation features (IDA sweep)
- Enfusion ships snow / mud looks in other titles; maybe something exists here unused: footprints, deformable
  snow / mud, displacement maps, tessellation on terrain, a runtime heightmap edit not exposed as GenericTerrainEntity
  methods. A 30-minute string sweep settles it. IDA item 12.

## F. Rut ageing (scale)
- Ruts slowly heal (floor rises back over N minutes without a new pass) and tiles / cells are dropped when flat.
  Bounds memory and JIP over a long match, looks natural (rain, settling). Deterministic: heal by op seq / match time
  ticks sent by the server, not local time. Small.

## G. Ruts only where the ground is soft
- CarveFloor already rejects hard / kept ground. Add a game-material table (terrain gamemat under the wheel - PROVEN
  readable by a WORLD trace, mud-gotchas 2026-10-08): roads / gravel / rock = no rut op at all (C draws tracks
  there), grass = shallow, dirt / field / mud = deep. Fewer ops = less of everything (cost, JIP, memory) and more
  realistic. Small, server-side, no new net data (it decides step / cap in the op it already sends).

## H. Already listed, still valid
Grass flattening (cheap look, whole map), wheel sink (feel), World.CreateDecal on ground that stopped changing.

## IDA results 11-13 (2026-10-16, exe 1.9.0.10 Diag - research/2026-10-15-ida-rut-questions.md)
- **D (runtime road / spline mesh): dead.** RoadEntity builds its mesh in the game only from saved prefab attributes;
  ShapeEntity.SetPoints and all generators go through the Workbench-only world editor API (INFERENCE: SetPoints in the
  game would crash). Editing a road prefab's BaseContainer before spawning is possible but no better than our own
  MeshObject.Create path.
- **E (hidden deformation): none.** No footprints / deformable snow / heightmap edit in the game. Two side finds:
  - **Parallax mapping** on MatPBRBasic / MatPBR2Layers / MatPBRDecal (ParallaxScale, ParallaxMidlePlane,
    ParallaxShadowScale). Joins option B: the groove soil section with track-aligned UVs AND a tread height map with
    parallax = tread relief and fake depth on the real groove, no extra triangles. Probe: a mod emat (MatPBRBasic) with
    a height map on the rut soil section.
  - **Vehicle mud look from script:** VehicleWheelEffectComponent Set/GetWheelMud, Set/GetBodyMud (+ dust). Cheap
    "mud on the vehicle" after driving through ruts / soft ground (old parked idea) - client-side look only.
- **C (vanilla tracks where we don't dig): dead on our ground.** Track decals start only when the wheel's contact
  entity IS the terrain; over our ground the wheel hits our MUD_ entity, so nothing starts. They do still work on real
  (kept) terrain - roads, where they already run natively (StaticTrackMaterial on muddiness / dustiness > 0.25).
  Edge case: a strip from a wheel on kept terrain can float over a crater at the old height (a thin sliver).

### Updated ranking after IDA
G (soft ground only) and B (tread texture, now + parallax) stay first. Step 0 of the rut-cells deep dive (grid mesher)
is the cost fix to try before A. C / D / E are closed; tracks on our ground = our own mesh, or World.CreateDecal on
ground that stopped changing (answer 1b).

## Ranking (INFERENCE)
| | Option | Look | Feel | Cost | Effort |
|---|---|---|---|---|---|
| 1 | G soft-ground-only | = | = | much less (fewer ops) | small |
| 2 | B tread texture | big win | - | ~0 | small-medium |
| 3 | A rut cells | sharper (0.125 m) | = | ~10x less per op (estimate) | medium-large |
| 4 | F ageing | - | - | bounds memory / JIP | small |
| 5 | C vanilla tracks on hard ground | tracks on roads | - | engine | probe |
| 6 | D / E IDA | unknown | unknown | unknown | 30-60 min IDA |

Suggested: G + B first (small, big effect), run the IDA sweep (D, E) in parallel, then decide A vs the review's
Stage 1 cost fix for the current field - A replaces most of that fix if it holds.

## IDA questions (add to research/2026-10-15-ida-rut-questions.md)
- 11. Runtime spline / road / river mesh: can a road / shape / spline entity (RoadEntity, ShapeEntity, generator
  entities, RiverEntity) be spawned and given points at runtime and generate its mesh in the GAME (not Workbench)?
  Which script API reaches it?
- 12. String / class sweep for deformation features: "deform", "footprint", "snow", "mud", "displacement",
  "tessellat", "track" (non-decal), "SetHeight", "HeightmapEdit", "PaintLayer", "TerrainModif". For each hit: what it
  is, reachable from script?
- 13. Vanilla track decals on our map: does SCR_VehicleTrackDecal's terrain path draw on hole cells (our ground over a
  hole) or skip them? (terrain hole test in the decal clip, sub_140B20280 area)
