# Mud sheet: where the client frame goes on a DS, and other ways to get the soft look (2026-10-18)

User 2026-10-18 (after a DS test): "a lot better fps on a dedicated server, but 140 fps -> 90 fps, still huge. Is there
any other way than the sheet mesh to get the same soft look? Do wide research and find ideas."
Labels: MEASURED (logs below), READ (game files / API docs, read today), IDA (strings in the Experimental Diag exe,
today), INFERENCE (not tested), PROBE (needs an in-game test). Research only - no code changed.
Earlier research this builds on (not repeated here): research/2026-10-16-soft-mud-alternatives.md (rut-field
channels A-K), research/2026-10-18-mud-zone-rut-perf.md (perf steps 1-3c), research/2026-10-16-custom-shaders.md.

## 1. Short answer
- On the DS client the mud costs two things of similar size: **script ~2-3.5 ms a frame** and **render ~+3 ms a
  frame** (render 1.0 ms outside the zone, 3.1-5.6 ms inside). The render part shows up even while the script part
  is small (11:09:13: MUD script 1.1 ms, render 3.1 ms, 290 sheet meshes on screen). Likely cause (INFERENCE): the
  number of sheet tile entities drawn (462 kept, each with 2 materials) - per-object draw cost, not vertex count.
- **Inside the script part, one function is ~90 % of the sync stamp cost: LimitDrag** (drag limit 64-178 ms vs cells
  3-14 ms and side reads 0-9 ms per 5 s). It can be made several times cheaper with the SAME results.
- **Cheapest big wins that keep today's look:** (1) faster LimitDrag, (2) fewer, bigger sheet meshes (merge tiles) or
  draw only touched tiles, (3) cheaper tile builds (the build is script loops, ~2.5 ms a 3 m tile).
- **Other ways to the soft look, without a sheet mesh:**
  - (A) the sheet's shape on the rut-field patches. A patch rebuild costs ~1 ms per 8 m patch (64 m2), while a sheet
    tile costs ~2.5 ms per 3 m tile (9 m2): ~18x cheaper per m2. This was already researched and is still the main
    whole-map route.
  - (B, NEW) the engine's own tyre-track decal ribbons (`TrackDecal`) with a mud material that has a height map +
    parallax + water accumulation (vanilla Decal_Tracks_01 does this). Gives tread, wetness and puddles in the track
    for almost no script cost, but no real shape.
  - (C, NEW, speculative) a skinned grid mesh moved by bones (`IEntity.SetBone`) instead of rebuilding meshes.
  - Best guess for the final look: A for the shape + B for the wet tread detail.

## 2. DS test numbers (MEASURED, PeerPlugin1 client logs_2026-10-18_11-08-00, server logs_2026-10-18_11-07-58)
Client, one row per 5 s 'MUD prof 5 s' line (engine numbers from System.GetProfileData). Zone entered ~11:09:00.
| time | fps | frame ms | render ms | simulation ms | MUD script ms |
|---|---|---|---|---|---|
| 11:08:33 |131|7.6|1.8|2.5|0.1|
| 11:08:38 |144|7.0|1.0|1.0|0.1|
| 11:08:43 |144|7.0|1.0|0.9|0.1|
| 11:08:48 |144|7.0|1.0|1.0|0.0|
| 11:08:53 |144|6.9|1.0|1.0|0.0|
| 11:08:58 |125|8.0|1.5|1.4|0.1|
| 11:09:03 |79|12.6|5.6|3.4|1.2|
| 11:09:08 |92|10.9|4.1|3.0|1.1|
| 11:09:13 |99|10.1|3.1|3.1|1.1|
| 11:09:18 |94|10.7|3.2|5.7|3.6|
| 11:09:23 |84|11.9|4.5|5.6|3.4|
| 11:09:28 |88|11.4|4.2|5.5|3.4|
| 11:09:33 |91|11.0|3.5|5.4|3.4|
| 11:09:38 |90|11.1|4.3|5.4|3.3|
| 11:09:43 |98|10.2|3.2|4.9|2.8|
| 11:09:48 |88|11.3|4.3|4.2|2.1|
| 11:09:53 |98|10.2|4.3|3.8|1.8|
| 11:09:58 |95|10.5|4.1|4.2|2.2|
| 11:10:03 |92|10.9|3.9|5.1|3.0|
| 11:10:08 |98|10.2|3.7|4.5|2.5|
| 11:10:13 |93|10.7|3.6|4.8|2.7|
| 11:10:18 |85|11.8|4.6|5.2|3.1|
| 11:10:23 |89|11.2|4.5|4.8|2.7|
| 11:10:28 |111|9.0|3.1|4.2|2.2|
| 11:10:33 |94|10.6|3.7|4.3|2.3|
| 11:10:38 |79|12.7|5.4|5.3|3.3|

Client 'MUD prof parts' while driving in the zone (11:09:18-11:10:18): sheet sync 0.35-0.95, sheet 0.8-2.1 (follow
0.1-0.3, wheels 0.15-0.46, upload 0.42-0.66, stored 0.05-0.2, zones ~0), ahead 0.4-0.66 ms a frame.
'MUD sheet:' 10 s lines: meshes 462 (all zone tiles kept, MESH_CAP), 0 dropped, builds 3-54 per 10 s (WB before the
mesh keep: ~750). So the mesh keep removed the build churn; the remaining cost is drawing + the sync layer.

Client sync STAMP split, ms per 5 s (all lines with stamps):
| side reads | cells | walked | stamped | drag limit |
|---|---|---|---|---|
|3|6|13384|6499|122|
|7|13|15574|7254|139|
|2|12|16282|7484|150|
|5|14|19546|9084|178|
|9|12|17030|7989|148|
|7|7|14420|6772|130|
|1|5|7900|3492|73|
|3|4|7689|3425|75|
|1|5|7557|3355|70|
|2|8|13510|6321|124|
|6|6|13236|6293|119|
|5|6|12061|5496|110|
|2|8|13685|6252|129|
|3|3|6874|3273|64|
Drag limit = 84-93 % of each row. Slump in the same lines: 44-141 ms per 5 s; sheet restore 33-68.

Server (DS, 30 fps cap): MUD script 0.2-0.4 ms idle, 1.6-5 ms while a vehicle drives (wheels 0.3-2 ms, max 34 ms one
frame). The DS keeps 30 fps throughout.

## 3. Perf ideas that keep today's look (ranked by gain / risk)
| # | Idea | Why | Same result everywhere? | Effort |
|---|---|---|---|---|
| P1 | **LimitDrag with direct neighbours**: inside a tile, neighbour = index +-1 / +-TQ (one TileOf per cell, a second only on tile edges); today 2 sweeps x (cell + 4 neighbours) x TileOf over the whole UNclipped box | 84-93 % of sync stamp time (table above). Expect sync stamps 5-10x cheaper (INFERENCE) | yes - same cells, same order, same maths | small |
| P2 | Same check on the sheet's own stamp (wheels + replay): if it has the same LimitDrag pattern, same fix | 'sh wheels' 0.15-0.46 ms + replay | look only | small |
| P3 | **Measure the render part first**: A/B MESH_CAP 600 vs 150 on the DS client (render ms with all 462 vs ~150 meshes), plus diag Statistics > 'MeshObject stats' (count / faces) | render +3 ms is the biggest single item but its cause is INFERENCE | - | test only |
| P4 | **Fewer, bigger meshes**: one entity per 2 x 2 tiles (6 m) = 4x fewer draws; or 4 x 4 (12 m) = 16x | per-object draw cost (if P3 confirms) | look same | medium: a changed tile rebuilds a bigger mesh |
| P5 | **Draw only touched tiles**: untouched tiles show the plain ground. Needs the untouched sheet to sit AT the ground (today: ground + THICK 0.15 + LIFT 0.01, CODE MUD_MudSheet.c) and a ramp at the edge of touched tiles | at whole-map scale nothing else scales: meshes only where vehicles drove | look change (a 15 cm layer goes) | big, design |
| P6 | Far tiles at half resolution (11 x 11 instead of 21 x 21 verts) | build ~4x cheaper, fewer verts | look only far away | medium |
| P7 | Faster BuildTile script: the engine calls are small (MeshObject.Create ~0, UpdateIndices + UpdateVerts 0.24 ms, MEASURED 2026-10-12, mud-gotchas.md); the rest is script loops | ~2.5 ms a tile now | look same | medium |
| P8 | Slump only cells that changed (active list) instead of the op box | slump 44-141 ms per 5 s, 4000-7000 ground reads | yes if done the same way everywhere | medium |
| P9 | Real join snapshot over several frames (hold the layer still while it builds) | ~250 ms hitch per join on the server, grows | yes | medium, MP test |

## 4. Other ways to get the soft look
| # | Way | What it gives | What it cannot do | Cost / scale | Status |
|---|---|---|---|---|---|
| A | **Sheet shape on the rut-field patches** (2026-10-16 doc, options A-D: tyre-width press + soft wall + ridges as a 2nd channel, texture drag UVs, wet section, slump near the camera) | real shape (felt and seen), one ground, no 2nd mesh | flat box floor of light-vehicle ruts (0.25 m grid keeps ~90 % of the ridge, MEASURED offline 2026-10-16) | patch rebuild ~1 ms per 64 m2 vs sheet ~2.5 ms per 9 m2; data 1 KB per 16 m2 + channels | researched, not built |
| B | **Engine track decals** (`TrackDecal`: AddPointToTrackDecal / FinalizeTrackDecal / SetGlobalTrackParams, READ API) as used by vanilla `SCR_VehicleTrackDecal` (`TrackDecalFactory(material, 0.25, 10)` per wheel, READ exp script) with a mud MatPBRDecal | tread pattern, wet dark mud, puddles in the track (WaterAccumulation), fake depth (parallax). Vanilla Decal_Tracks_01.emat (READ): HeightMap + ParallaxScale 0.02 + ParallaxShadowScale 0.2 + WaterAccumulation 0.65 | no silhouette, no ridges, no smearing; parallax breaks at steep angles | engine side, script only adds points per wheel; client only | PROBE: does it stick to our MeshObject ground? (World.CreateDecal works on MeshObject entities but goes when the mesh is rebuilt - IDA 2026-10-16; on the hidden terrain it returns null - PROVEN 2026-10-17) |
| C | **Skinned grid mesh moved by bones** (`IEntity.SetBone(bone, angles, trans, scale)` / `Animation.SetBoneMatrix`, READ API): an imported 3 m grid with ~200 bones (vanilla skeletons have 231 bones, 4 influences per vertex - KB character-gear-and-anim-data.md) | smooth deformation on the GPU, no mesh rebuild; skinning blends between bones | resolution = bone grid (~0.2 m for 15 x 15 bones per 3 m); normals only turn with bone rotation - script must tilt each bone by the slope | ~200 bone calls per changed tile vs a rebuild | PROBE: does SetBone work on a static skinned mesh with no animation graph? cost per call? |
| D | **Parallax material on a coarser mesh**: MatPBR2Layers / MatPBRDecal take HeightMap + ParallaxScale (READ: Piles_01_Parallax.emat 0.025) | small soft detail from the texture, so the mesh can be coarser (0.3 m: 4x fewer verts) | big shapes (ridges) still need verts | cheaper builds + draws | PROBE: does MatTerrainObject / our dirt take a height map? |
| E | **Material deform function**: IDA string `DeformFunc@MaterialSource` (engine material vertex deform, e.g. wind sway) | maybe vertex movement without rebuilds | unknown which materials expose it, no custom shaders for mods (2026-10-16 doc) | - | IDA lead only (unverified) |

Dead ends already proven (not options): GPU / custom shader deformation (no custom shaders), DYNAMIC_VERTEXES
in-place updates (broken normals, IDA 2026-10-15), own normals / colours / 2nd UV set in the script mesh API, terrain
heightmap changes at runtime (no API; IDA strings today show no terrain-deform or snow-trail feature - only
TrackDecal, ProcAnim bones, RenderTarget widgets, ocean displacement).

## 5. Suggested order (user decides)
1. P1 + P2 (LimitDrag) - same results, small, biggest script item. Then one DS session with P3 (render A/B) in it.
2. If P3 shows draws are the render cost: P4 (merge tiles).
3. One probe session for the look: B (track decal on our ground, wet weather), C (SetBone on a skinned grid), D.
4. Then decide the whole-map route: sheet + P5 (draw only touched tiles), or A + B (shape on the patches + track
   decals), with the sheet kept only where it is still worth it.

## 5b. Built 2026-10-18 (v190 = before, not compiled)
- P1: MUD_MudSync.LimitDrag reads the 4 neighbours straight from the cell's own tile when all 4 lie in it (cells not on
  a tile edge, and away from 0 where TileOf's int division differs); edge cells keep the TileOf / GetI path. Same
  values, same order -> same results on every machine.
- P2: not needed - MUD_MudSheet.LimitDrag already reads one flat window array (no tile lookups).
- P3 render A/B: CLIENT test-kit setting `sheet_hide_ab = 30` in $profile:MUD_TestKit.txt (read at start). Every 30 s
  MUD_MudSheet.RenderAB hides all visible stored sheet meshes, then shows only those again (live slots stay; nothing is
  dropped or rebuilt). Log: 'MUD render A/B: phase 0/1 - N stored sheet meshes shown/HIDDEN', and every 'MUD prof 5 s'
  line ends '| render A/B: sheet meshes shown / HIDDEN'. Compare render ms between the two.

## 6. Not verified
- That the render +3 ms comes from the number of sheet entities (INFERENCE; P3 tests it).
- TrackDecal on MeshObject ground, SetBone on non-animated skinned meshes, height maps on our materials (all PROBE).
- GPU ms from GetProfileData looks odd (3.9 -> 0.8-1.9 in the zone); not used for any conclusion.

## 7. DS test 2 (MEASURED, PeerPlugin1 client logs_2026-10-18_11-21-43, v190 + P1 + render A/B, sheet_hide_ab 30)

Compiled fine. P1 drag limit per 5 s while driving: 14-72 ms (was 64-178 in test 1). Stamp cells 1-12 ms, side reads 0-7 ms.
Other sync costs per 5 s are unchanged: stamps ~60, slump ~70, sheet restore ~50.

Render A/B ('MUD prof 5 s' client rows; HIDDEN = stored sheet meshes hidden, 354-742 of them; live slot meshes near the
truck stay visible). Every 5 s row (time, fps, render, GPU, simulation, MUD script ms, phase):

```
11:22:12 130 1.89 3.39 2.68 0.06 shown   (spawn, not driving)
11:22:17 141 1.11 3.64 1.04 0.07 shown   (not driving)
11:22:22  99 4.16 1.88 2.26 0.25 shown
11:22:27 111 3.24 1.07 3.05 0.98 shown
11:22:32  96 3.33 3.51 2.59 0.71 shown
11:22:37  95 2.93 1.38 5.30 3.29 HIDDEN
11:22:42 118 2.57 0.44 4.67 2.63 HIDDEN
11:22:47  98 3.83 0.95 4.43 2.47 HIDDEN
11:22:52 103 3.03 1.34 4.49 2.33 HIDDEN
11:22:57  93 4.55 0.72 4.57 2.36 HIDDEN
11:23:02 100 3.98 1.23 3.90 1.90 HIDDEN
11:23:07 104 2.87 1.95 3.92 1.87 shown
11:23:12 104 3.31 1.93 3.54 1.58 shown
11:23:17  96 3.38 1.97 4.14 2.16 shown
11:23:22  91 4.21 1.17 4.73 2.62 shown
11:23:27  98 3.73 0.49 4.60 2.34 shown
11:23:32  94 3.94 0.66 4.67 2.41 shown
11:23:37  96 3.37 1.94 4.04 2.00 HIDDEN
11:23:42 106 2.82 1.96 3.69 1.75 HIDDEN
11:23:47  98 3.50 1.17 4.64 2.58 HIDDEN
11:23:52 114 3.18 1.83 2.98 1.01 HIDDEN
11:23:57  86 5.14 1.31 4.24 2.17 HIDDEN
11:24:02  98 3.51 1.06 4.72 2.57 HIDDEN
11:24:07 100 2.93 0.95 4.51 2.41 shown
11:24:12  91 4.46 0.80 4.80 2.55 shown
11:24:17 102 3.42 0.70 4.90 2.84 shown
11:24:22  90 4.05 1.53 4.57 2.24 shown
11:24:27  90 3.70 1.55 4.89 2.57 shown
11:24:32  87 4.29 2.98 3.36 1.27 shown
11:24:37  84 4.91 2.14 3.97 1.84 HIDDEN
11:24:42  91 3.73 1.77 4.55 2.39 HIDDEN
11:24:47  92 4.11 1.56 4.34 2.19 HIDDEN
11:24:52  81 4.84 1.08 5.51 3.42 HIDDEN
11:24:57  93 3.53 1.07 5.27 3.10 HIDDEN
11:25:02  86 4.85 1.10 4.70 2.54 HIDDEN
11:25:07  85 4.95 1.49 4.43 2.31 shown
11:25:12  80 5.49 1.87 4.22 2.05 shown
11:25:17  81 5.67 1.91 3.89 1.80 shown
11:25:22  99 3.72 2.56 3.01 0.98 shown
```

Averages over rows with MUD script > 1 ms (driving): HIDDEN 18 rows: fps 96.2, render 3.80, GPU 1.34, sim 4.45, MUD 2.36.
shown 15 rows: fps 92.9, render 4.03, GPU 1.46, sim 4.34, MUD 2.20. Checksum: 18 + 15 = 33 driving rows of 38.

Result: hiding the stored sheet meshes saves ~0.2 ms render = inside the noise. The stored meshes are NOT the render
cost, so P4 (merge tiles for render) is not worth it now. GPU stays ~1-2 ms (not GPU bound). Render still goes from
~1.1-1.9 (standing) to ~3.8 (driving) with the meshes hidden; cause UNRESOLVED (live slot meshes, or vanilla driving
cost - needs a control drive outside the zone). Script parts (11:22:47-11:23:17): sheet 0.7-1.25 (upload 0.4-0.56,
wheels 0.1-0.34), ahead 0.45-0.53, sheet sync 0.17-0.45 ms/frame.

## 8. DS control drive (MEASURED, PeerPlugin1 client logs_2026-10-18_11-29-18; circles OFF the zone first, then IN it)

Every 5 s row (time, fps, render, GPU, simulation, MUD script ms | sync ops, sync ms, drag limit ms per 5 s | sheet,
ahead, sh upload ms/frame). sheet_hide_ab was still 30.

```
11:29:48  133  1.62  3.71  2.47  0.06 | - - - | 0.01 0.01 -     (standing)
11:29:53   98  4.34  2.49  1.90  0.22 | - - - | 0.01 0.03 -
11:29:58   99  4.40  2.02  1.91  0.31 | - - - | 0.01 0.03 -
11:30:03  113  3.06  1.61  2.43  0.33 | - - - | 0.02 0.03 -
11:30:08   87  4.89  2.75  2.86  0.92 | 0 0 0 | 0.04 0.48 -
11:30:13   98  3.28  1.73  4.34  2.36 | 0 0 0 | 0.08 0.66 -     OFF zone circles
11:30:18   85  4.49  1.25  5.10  3.10 | 0 0 0 | 0.11 0.62 -
11:30:23   86  4.10  1.32  5.35  3.31 | 0 0 0 | 0.14 0.56 -
11:30:28  100  2.91  1.45  4.79  2.78 | 0 0 0 | 0.11 0.47 -
11:30:33   96  2.72  1.66  5.15  3.22 | 0 0 0 | 0.12 0.54 0.00
11:30:38   81  4.66  1.36  5.45  3.40 | 0 0 0 | 0.14 0.59 0.00
11:30:43   94  3.04  1.53  5.22  3.24 | 0 0 0 | 0.12 0.49 -
11:30:48   82  4.64  1.14  5.57  3.47 | 0 0 0 | 0.17 0.74 -
11:30:53  102  3.02  2.37  3.54  1.66 | 0 0 0 | 0.30 0.17 0.00
11:30:58   91  3.03  0.72  6.43  4.51 | 3 89 18 | 3.11 0.75 0.37  IN zone
11:31:03   84  4.60  0.94  5.48  3.31 | 7 290 68 | 1.76 0.58 0.61
11:31:08   98  3.12  1.41  4.88  2.85 | 6 249 62 | 1.47 0.57 0.53
11:31:13   86  4.36  1.09  5.36  3.25 | 7 307 74 | 1.72 0.51 0.62
11:31:18   93  3.83  0.98  5.23  3.12 | 7 361 73 | 1.51 0.47 0.53
11:31:23  107  2.82  0.88  4.71  2.72 | 7 294 76 | 1.34 0.50 0.50
```

OFF zone 11:30:13-11:30:48 (8 rows): fps 81-100, render 2.7-4.7, MUD script 2.4-3.5 ms. Off-zone parts (11:30:13-23):
rebuild (patch Rebuild + nav refresh) 1.0-1.9, ahead 0.56-0.66, vox 0.31-0.44, sheet 0.08-0.14 ms/frame.
IN zone 11:31:03-11:31:23 (5 rows): fps 84-107, render 2.8-4.6, MUD script 2.7-3.3 ms (sheet 1.3-1.8, sheet sync
0.5-0.7, ahead ~0.5, rebuild ~0).

Result: the zone is no longer worse than plain offroad - fps and MUD cost are about the same in and out. Off the zone the
heightfield rut path (rebuild + vox) costs what the sheet costs in the zone. Driving raises render from ~1.6 to ~3-4.5 ms
in BOTH places, so that render rise is not the sheet (vanilla driving or our heightfield patches - UNRESOLVED, a run
with the mod off would settle it). Remaining MUD script ~3 ms/frame = biggest part we own.

## 9. Off-zone rebuild cost (MEASURED, same client log logs_2026-10-18_11-29-18, circles off the zone 11:30:05-11:30:53)

Per 10 s, 'MUD world: last 10 s' (all ground patch mesh / collider builds) next to 'MUD look ruts: 10 s' (client look rut
mesh builds, a subset of the world meshes):

```
11:30:13 world: meshes 101 (491 ms), colliders 21 (115 ms)
11:30:13 look ruts: look meshes 84 (417 ms), replaced by server rut 24
11:30:23 world: meshes 247 (1343 ms), colliders 35 (212 ms)
11:30:23 look ruts: look meshes 224 (1235 ms), replaced by server rut 377
11:30:33 world: meshes 275 (1582 ms), colliders 30 (214 ms)
11:30:33 look ruts: look meshes 250 (1440 ms), replaced by server rut 534
11:30:43 world: meshes 282 (1556 ms), colliders 33 (227 ms)
11:30:43 look ruts: look meshes 257 (1426 ms), replaced by server rut 464
11:30:53 world: meshes 215 (1068 ms), colliders 37 (209 ms)
11:30:53 look ruts: look meshes 182 (921 ms), replaced by server rut 337
```

Sums over the 5 windows: world meshes 1120 (6040 ms), look meshes 997 (5439 ms) = 89 % of the mesh builds and 90 % of
the mesh ms. Colliders 156 (977 ms). Mesh build ~5.4 ms each. Rut ops from the server: 150 in 11:30:10-11:30:50
(~3.75 / s), each 'rut floor' 2-4 ms (= most of the 'vox' part).

Result: the off-zone 'rebuild' part is mostly CLIENT LOOK RUTS (MUD_LookRuts): ~25 look mesh builds / s at ~5.4 ms,
then many of the same patches build again when the server rut arrives ('replaced by a shallower server rut' 24-534 per
10 s). Server ruts alone are ~2-3 mesh builds / s. In the zone the sheet skips look ruts, so this cost moves there.
Fix options: (L1) A/B look_ruts 0 on the client (no code); (L2) look meshes per patch at most every 0.3-0.5 s instead
of the shared 0.1 s m_fMeshInterval; (L3) skip a look build when the look floor is < ~1-2 cm deeper than what is shown;
(V1, smaller) rut floor 2-4 ms per op on the client.

## 10. L1 test: look_ruts 0 on the client (MEASURED, PeerPlugin1 client logs_2026-10-18_11-37-50; user: "much better")

Off zone 11:38:45-11:39:55, in zone 11:40:40-11:41:41. Rows picked = driving rows (MUD script > ~0.8 ms) of each stretch;
the look_ruts 1 rows are the section 8 rows (logs_2026-10-18_11-29-18). Averages of the 5 s 'MUD prof' rows:

```
OFF zone, look ruts 1:  8 rows  fps  90.2  render 3.73  sim 5.12  MUD script 3.11 ms
OFF zone, look ruts 0: 15 rows  fps  94.5  render 3.98  sim 3.32  MUD script 1.26 ms
IN zone,  look ruts 1:  5 rows  fps  93.6  render 3.75  sim 5.13  MUD script 3.05 ms
IN zone,  look ruts 0: 13 rows  fps 101.4  render 3.61  sim 4.19  MUD script 2.13 ms
```

Off zone ground patch builds per 10 s dropped from 215-282 meshes (1068-1582 ms) to 13-34 meshes (48-143 ms); colliders
about the same (13-34, 72-189 ms). 'rebuild' part 0.10-0.41 ms/frame (was 1.0-2.0). In zone: 0 patch builds.
Render (3.6-4.0 ms) does not move with our script cost; late standing rows (11:43:11-11:43:51, MUD 0.3 ms) show render
4.5-5.8 and fps 72-83 with nothing of ours running - render swings are the view, not the mod.
Result: look ruts cost ~1.9 ms/frame off zone. Look decision (keep off / L2 + L3 cheaper look ruts) is the user's.

## 11. L2 + L3 DS test (MEASURED, PeerPlugin1 client logs_2026-10-18_11-51-25; user: "so much better", fps smoother, ruts still pop a bit)

Compiled fine, look ruts 1. Most of the run was IN the zone (look cuts skipped under the sheet: 264-1149 segments per
10 s). Off zone only ~11:53:11-11:53:31. 'MUD look ruts: 10 s' rows with look work:

```
11:53:11 cuts 12  written 174  caught 57   expired 0   dirty 19   look meshes 4  (19 ms)   replaced 0  near 0
11:53:21 cuts 34  written 814  caught 751  expired 8   dirty 127  look meshes 64 (307 ms)  replaced 72 near 14
11:53:31 cuts 0   written 0    caught 0    expired 20  dirty 18   look meshes 14 (73 ms)   replaced 0  near 0
```

11:53:21 (one ~10 s off-zone pass): 64 look meshes vs 182-257 per 10 s with L2/L3 off (sec 9) - about 3x fewer, but
only one window, so a thin sample. 11:54:01-11:54:16: cuts 53-96 with 0 samples written, MUD 0.5 ms, no sync ops =
likely on kept terrain (road), no ruts there (inference). In zone MUD script 1.9-2.9 ms, fps 81-100.
Pop-in sources (inference): off zone = 0.25 s look rebuild steps, 'expired' (server cut less than the look guess -> mesh
steps up) and 'replaced'; in zone = the sheet's own stamps vs server op replay. Where the user saw it decides the fix.

## 12. TEMP rut delay meter (user 2026-10-18: ruts show a little after the wheel, off zone and in zone; v192 = before, NOT COMPILED)

- Off zone, 'MUD delay look 10 s (TEMP)' (MUD_LookRuts): wheel at a segment's end -> its look cut (GAP 0.4 m behind the
  rearmost wheel + EVERY 0.1 s), and wheel at the oldest segment -> the patch mesh drawn (adds LOOK_MESH_EVERY 0.25 s and
  the one-mesh-per-frame Rebuild queue); m = that delay x the vehicle speed. Fields MUD_FinePatch.m_fLookSince / m_fLookSpeed.
- In zone, 'MUD delay sheet 10 s (TEMP)' (MUD_MudSheet, with the Log10 line): own wheel stamp -> its live tile mesh drawn
  (UPLOAD_EVERY 0.1 s, UploadPick put-offs, replay holds), m; replay holds (count, avg / max ms, stuck at REPLAY_HOLD_MS)
  and frames an upload was due but held.
- Remove: grep 'TEMP delay meter'.

## 13. Delay meter DS run (MEASURED, PeerPlugin1 client logs_2026-10-18_12-15-18; user screenshot: front wheel has no mud under it, then it pops in)

Every delay line of the run (look = off zone heightfield look ruts, sheet = in zone):

```
12:16:03 look 10 s: wheel -> look cut avg 653 max 2036 ms (220 segments) | wheel -> drawn mesh avg 1567 max 2275 ms, ~5.74 m behind the wheel (20 meshes) | GAP 0.4 m, LOOK_MESH_EVERY 0.25 s
12:16:13 look 10 s: wheel -> look cut avg 439 max 1369 ms (1402 segments) | wheel -> drawn mesh avg 1086 max 1633 ms, ~7.72 m behind the wheel (59 meshes) | GAP 0.4 m, LOOK_MESH_EVERY 0.25 s
12:16:23 look 10 s: wheel -> look cut avg 436 max 1092 ms (1414 segments) | wheel -> drawn mesh avg 1094 max 1302 ms, ~7.35 m behind the wheel (83 meshes) | GAP 0.4 m, LOOK_MESH_EVERY 0.25 s
12:16:33 look 10 s: wheel -> look cut avg 397 max 959 ms (1445 segments) | wheel -> drawn mesh avg 1004 max 1204 ms, ~7.69 m behind the wheel (72 meshes) | GAP 0.4 m, LOOK_MESH_EVERY 0.25 s
12:16:43 sheet 10 s: own wheel stamp -> drawn tile avg 67 max 117 ms, ~0.40 m behind the wheel (16 tiles) | upload held by replay: 0 holds, avg 0 max 0 ms, 0 stuck, 0 frames an upload was due but held | UPLOAD_EVERY 0.1 s
12:16:43 look 10 s: wheel -> look cut avg 362 max 902 ms (1652 segments) | wheel -> drawn mesh avg 944 max 1012 ms, ~8.22 m behind the wheel (7 meshes) | GAP 0.4 m, LOOK_MESH_EVERY 0.25 s
12:16:53 sheet 10 s: own wheel stamp -> drawn tile avg 223 max 1499 ms, ~1.04 m behind the wheel (206 tiles) | upload held by replay: 12 holds, avg 31 max 61 ms, 0 stuck, 6 frames an upload was due but held | UPLOAD_EVERY 0.1 s
12:16:53 look 10 s: wheel -> look cut avg 415 max 936 ms (15 segments) | wheel -> drawn mesh avg 0 max 0 ms, ~0.00 m behind the wheel (0 meshes) | GAP 0.4 m, LOOK_MESH_EVERY 0.25 s
12:17:03 sheet 10 s: own wheel stamp -> drawn tile avg 172 max 2224 ms, ~0.80 m behind the wheel (223 tiles) | upload held by replay: 13 holds, avg 30 max 41 ms, 0 stuck, 9 frames an upload was due but held | UPLOAD_EVERY 0.1 s
12:17:13 sheet 10 s: own wheel stamp -> drawn tile avg 181 max 1343 ms, ~0.99 m behind the wheel (215 tiles) | upload held by replay: 12 holds, avg 26 max 49 ms, 0 stuck, 5 frames an upload was due but held | UPLOAD_EVERY 0.1 s
12:17:23 sheet 10 s: own wheel stamp -> drawn tile avg 175 max 1292 ms, ~0.82 m behind the wheel (143 tiles) | upload held by replay: 13 holds, avg 23 max 37 ms, 0 stuck, 5 frames an upload was due but held | UPLOAD_EVERY 0.1 s
12:17:23 look 10 s: wheel -> look cut avg 660 max 1567 ms (361 segments) | wheel -> drawn mesh avg 1526 max 1738 ms, ~6.33 m behind the wheel (17 meshes) | GAP 0.4 m, LOOK_MESH_EVERY 0.25 s
12:17:33 sheet 10 s: own wheel stamp -> drawn tile avg 147 max 855 ms, ~1.14 m behind the wheel (166 tiles) | upload held by replay: 8 holds, avg 26 max 43 ms, 0 stuck, 5 frames an upload was due but held | UPLOAD_EVERY 0.1 s
12:17:33 look 10 s: wheel -> look cut avg 459 max 1152 ms (468 segments) | wheel -> drawn mesh avg 1170 max 1374 ms, ~8.60 m behind the wheel (11 meshes) | GAP 0.4 m, LOOK_MESH_EVERY 0.25 s
12:17:43 sheet 10 s: own wheel stamp -> drawn tile avg 672 max 8916 ms, ~4.96 m behind the wheel (215 tiles) | upload held by replay: 10 holds, avg 29 max 55 ms, 0 stuck, 5 frames an upload was due but held | UPLOAD_EVERY 0.1 s
12:17:53 sheet 10 s: own wheel stamp -> drawn tile avg 323 max 8969 ms, ~1.47 m behind the wheel (180 tiles) | upload held by replay: 12 holds, avg 30 max 51 ms, 0 stuck, 7 frames an upload was due but held | UPLOAD_EVERY 0.1 s
```

Off zone (look ruts): wheel -> drawn mesh avg 0.94-1.57 s, ~5.7-8.6 m behind the wheel. Of that, wheel -> cut avg
0.36-0.66 s: a segment waits until the REARMOST wheel is GAP 0.4 m past it (MUD_LookRuts.Cut, m_vTail) - front wheel
segments wait for the whole truck. The rest (~0.6 s) = LOOK_MESH_EVERY 0.25 s + the one-mesh-per-frame Rebuild queue.
A per-wheel GAP version exists (checkpoint v176, never tested alone).
In zone (sheet): own stamp -> drawn tile avg 0.07-0.67 s (~0.4-5 m), but max 0.86-8.97 s. Replay holds are NOT the
cause (8-13 holds per 10 s, avg 23-31 ms, 0 stuck, 5-9 frames). Likely cause (inference): UploadPick builds at most
UPLOAD_SLOTS 3 zone slots per upload, longest-waiting first, so the slot under a wheel can wait - matches the screenshot.
Sheet collider fact check (docs only): the one measured sheet collider number is the 2026-10-15 WB perf A/B (COLLIDE off =
same fps, 38-48 driving; MUD_MudSheet.COLLIDE comment). No doc shows it FELT worse - unverified. The synced sheet is look
only by design: its tyre marks are client guesses, so a collider from them would differ per machine (snaps).

## 14. WHEEL TILES FIRST (v193 = before, NOT COMPILED)

MUD_MudSheet.UploadPick -> PickWheelSlots: every dirty zone slot with a wheel contact builds at each upload (at most
WHEEL_SLOTS_MAX 6, on top of UPLOAD_SLOTS 3; the longest-waiting slot still builds). Before, the 'nearest' pick used the
vehicle ORIGIN (truck centre), so a front-wheel tile 3+ m ahead could lose to other tiles. Check: 'MUD delay sheet 10 s'
max well below 0.9-9 s, 'wheel tiles picked first' > 0; sheet upload ms in 'MUD prof parts' (sh upload) not much up.

## 15. WHEEL TILES FIRST DS run (MEASURED, PeerPlugin1 client logs_2026-10-18_12-24-39; user: "fps feels good, a little lower, looked better")

Compiled fine. 'MUD delay sheet 10 s' (in zone):

```
12:26:06 own wheel stamp -> drawn tile avg 85 max 950 ms, ~0.38 m behind the wheel (239 tiles) | upload held by replay: 6 holds, avg 32 max 40 ms, 0 stuck, 4 frames an upload was due but held | UPLOAD_EVERY 0.1 s | wheel tiles picked first 233 (WHEEL_SLOTS_MAX 6)
12:26:16 own wheel stamp -> drawn tile avg 174 max 9527 ms, ~0.89 m behind the wheel (350 tiles) | upload held by replay: 10 holds, avg 25 max 39 ms, 0 stuck, 10 frames an upload was due but held | UPLOAD_EVERY 0.1 s | wheel tiles picked first 342 (WHEEL_SLOTS_MAX 6)
12:26:26 own wheel stamp -> drawn tile avg 80 max 617 ms, ~0.41 m behind the wheel (123 tiles) | upload held by replay: 4 holds, avg 22 max 28 ms, 0 stuck, 1 frames an upload was due but held | UPLOAD_EVERY 0.1 s | wheel tiles picked first 121 (WHEEL_SLOTS_MAX 6)
12:26:36 own wheel stamp -> drawn tile avg 77 max 664 ms, ~0.29 m behind the wheel (167 tiles) | upload held by replay: 5 holds, avg 27 max 35 ms, 0 stuck, 3 frames an upload was due but held | UPLOAD_EVERY 0.1 s | wheel tiles picked first 169 (WHEEL_SLOTS_MAX 6)
12:26:46 own wheel stamp -> drawn tile avg 196 max 10327 ms, ~1.02 m behind the wheel (427 tiles) | upload held by replay: 13 holds, avg 33 max 63 ms, 0 stuck, 6 frames an upload was due but held | UPLOAD_EVERY 0.1 s | wheel tiles picked first 432 (WHEEL_SLOTS_MAX 6)
12:26:56 own wheel stamp -> drawn tile avg 147 max 5607 ms, ~0.66 m behind the wheel (293 tiles) | upload held by replay: 8 holds, avg 25 max 56 ms, 0 stuck, 4 frames an upload was due but held | UPLOAD_EVERY 0.1 s | wheel tiles picked first 294 (WHEEL_SLOTS_MAX 6)
12:27:06 own wheel stamp -> drawn tile avg 126 max 5608 ms, ~0.34 m behind the wheel (301 tiles) | upload held by replay: 11 holds, avg 22 max 41 ms, 0 stuck, 4 frames an upload was due but held | UPLOAD_EVERY 0.1 s | wheel tiles picked first 303 (WHEEL_SLOTS_MAX 6)
```

Own stamp -> drawn tile avg 77-196 ms, ~0.3-1.0 m behind the wheel (was 147-672 ms / 0.8-5 m at 12:15, sec 13).
Max still 0.6-10.3 s: likely a METER artifact (inference) - a tile stamped, then out of the live window, keeps its map
entry up to ~15 s (DlLog drops entries > 5 s old only every 10 s) and is timed when it comes back. So the 12:15 max
values are not proof of a 9 s pop either; the averages and the user's eye are the evidence.
In-zone driving rows ('MUD prof 5 s' with MUD script > 1 ms and sheet > 0.5 ms):
12:24 run 12 rows: fps 86.5, render 4.03, sim 4.52, MUD 2.36, sheet 1.30, sh upload 0.84 ms/frame.
12:15 run 15 rows: fps 87.0, render 4.40, sim 4.72, MUD 2.52, sheet 1.28, sh upload 0.52 ms/frame.
Cost: sh upload +0.3 ms/frame; fps within noise.
