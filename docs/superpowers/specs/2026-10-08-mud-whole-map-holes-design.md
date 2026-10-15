# Mud ground: whole-map terrain holes - design

Date: 2026-10-08. Target: Arma Reforger Experimental 192624. Status: design approved section by section
in chat (brainstorming), spec awaiting user review.

## 1. Goal

Deformable ground (vehicle ruts by mass, craters, shovel trenches) anywhere on a real map (Arland first),
synced in MP + JIP, with:

- no raised pad: our ground sits at heightmap height + 1 mm, so props/vehicles never jump and grass clutter
  still shows on it;
- deep digging anywhere (floor default 4 m, deeper inside depth boxes);
- roads and water keep the real terrain.

Method: cut terrain holes over the whole map except roads/water (edit time, written into the map's
`.ttile` files), and fill every hole with our own ground.

## 2. Proven facts this design rests on (Exp 192624, 2026-10-08)

| Fact | Evidence |
|---|---|
| A terrain hole removes terrain render + collision; GetSurfaceY returns -256; WORLD trace still hits the heightmap and returns the surface gamemat | probe log 19:58, 20:01, 21:42 |
| Holes are per 2 m heightmap cell; edges on even world coords | 0.5 m scan 20:01 |
| Test map and Arland heightmaps are 2.0 m spacing (129 / 2049 verts) | `.terr` HEAD |
| `.ttile` HOLE chunk format fully decoded (see section 4.2); decoded cells == in-game scan (588/588); no X/Z swap | 21:08, 21:10 |
| Holes written by a Python tool load in game (16 new cells read as holes, no errors) | 21:19 |
| Grass clutter shows on our ground over a hole only if the ground is >= 0.001 m above the heightmap; at +0.3 m it rides on our tile; craters in it are clean with grass on the rim | user screenshots 21:50-21:59 |
| No script API can hide/remove part of the terrain at runtime; the exe registers exactly 13 terrain natives; a made-up proto is "Method not linked" and a silent no-op | 20:50 |
| Disabled blocks (BLCK chunk) do NOT read as holes | 20:40 scan |
| Addon override of a vanilla Arland `.ttile` by path works: holes load in play (soldier falls), Workbench packs the file byte-identical, no `.meta` needed | 22:49-23:03, results/2026-10-08-gate1-override.md |
| HOLE block coords (bx, bz) are terrain-GLOBAL: holes written into Terrain_1/16/18 landed at their written cells near the map corner, not in their own tile | census 22:49 + 23:01, MATCH global |
| Whole-Arland 2 m skin (4096 x 64 m chunks, 4.46 M verts, 8.39 M tris, WORLD-trace heights + diagonal trace): ~11 s script work / 22 s wall at 8 chunks per frame, +587 MB (collider only: 10.2 s / 20.3 s, +393 MB), frame +0.6..0.9 ms; heights are 80 % of the work; 99.93 % of non-flat cells use diagonal tl-br | 23:11-23:15, results/2026-10-08-gate2-skin-cost.md |

Untested (each has a task in section 7):
JIP message size limit; clutter behaviour in vehicle ruts; terrain-LOD cracks at hole edges.

## 3. Architecture overview

Two layers of ground, owned by one placed entity:

1. **Coarse skin** - whole map, built at start on every machine, exact heightmap triangles at 2 m, render +
   collision. Stands in for the terrain wherever it is holed.
2. **Fine patches** - 8 m x 8 m, 0.25 m spacing, created on demand near vehicles and where something digs.
   Only fine patches deform. A new patch starts with exactly the skin's shape, so the swap is invisible.

Holes come from an edit-time tool (section 4). Sync is server-authoritative deltas on one RplComponent
(section 6).

## 4. Hole tool (edit time)

### 4.1 Behaviour

Hole every 2 m cell of the map except "keep" cells:

- roads (from road generators/shapes, width + margin, default margin 1 m);
- water: rivers, lakes, ocean shore (from water generators / water bodies);
- cells inside hand-placed keep boxes.

Buildings do not keep terrain: the coarse skin is under them; deformation stays locked under buildings as today.

Two steps:

1. **Workbench plugin "Export hole mask"** (World Editor > Plugins): reads road/water generators and keep boxes
   of the open world, writes a per-cell keep mask file.
2. **Python tool "write holes"** (world closed in Workbench): reads the mask, backs up every `.ttile`, writes
   HOLE chunks. Grows from the proven tool `Tools/ttile_holes.py` (round-trip check before every write).
   May later move into the plugin if script binary writing (`FileHandle.Write`) works.

### 4.2 HOLE chunk format (measured)

`.ttile` = IFF `FORM` / `TERR`, chunk sizes big-endian, chunk order seen: VERS HGHT BERR [HOLE] [BLCK] LRS2 TMAT.
HOLE payload, little-endian: `u32 blockCount`, `u16 bytesPerBlock (128)`, then per block `u16 bx, u16 bz`,
32 rows x `u32`; bit c of row r = cell (bx*32+c, bz*32+r) is a hole. Only blocks with holes are stored.
Cell x = world X / 2, cell z = world Z / 2 (2 m cells). Arland: 16 x 16 tiles of 128 cells.

### 4.3 Edges

Where a hole meets kept terrain (roads, water), setting **Edge Mode**:

- **Locked border (default):** edge verts never move; dig depth fades 0 -> full over Edge Fade (default 1 m);
  a 0.3 m skirt (terrain look, mesh only) hangs down the edge to hide terrain-LOD cracks.
- **Dirt wall:** dig to the edge; a vertical wall (mesh + collider) from terrain height down to the dug height.

## 5. Ground entity

One placed entity (refactor of `MUD_GroundEntity`), replacing the per-32 m grounds and the pad mode.

### 5.1 Coarse skin

- 64 m chunks matching the 32 x 32-cell hole blocks; only hole cells get triangles.
- Heights: WORLD trace at the 2 m heightmap points + one mid-cell trace to pick the cell's diagonal, then
  exact triangles; + 1 mm (clutter). Never GetSurfaceY (returns -256 in holes).
- Collider: full map at 2 m, built during loading spread over frames; per-cell gamemat from WORLD trace;
  hard-surface rules as today.
- Render (clients only; server builds no meshes): 2 m mesh within ~500 m of the camera, 8 m mesh beyond,
  short skirts on chunk edges against LOD cracks. Terrain-look material (MatTerrainObj) as now.
- Estimated Arland cost: 4096 chunks, ~4.5 M verts, ~9 M collider tris, 5-10 s load, a few hundred MB -
  to be measured (task 2). Fallback: 4 m collider far from players.

### 5.2 Fine patches

- 8 m, 33 x 33 verts, 0.25 m; today's patch code reused; stored sparse, keyed by (px, pz).
- Created (same rules on every machine): cells within ~30 m of a vehicle; every patch a dig/crater/rut stamp
  touches (created before the stamp is applied, so stamps cross patch borders); a patch named in a server
  message the client does not have yet.
- On creation the coarse chunk hides those cells (mesh + collider rebuild of one 64 m chunk).
- Freed when never deformed and no vehicle near for 60 s; deformed patches live for the match (~35 KB each,
  log warning past a budget).
- Walking soldiers create nothing.

### 5.3 Floor

- Max Depth (default 4 m below surface); shovel keeps its own 1.5 m limit.
- Depth box entity: overrides Max Depth inside its box.
- Safety plate collider 0.5 m under the deepest floor under all hole cells.
- Bullet slab (FireGeo) follows the floor.

### 5.4 Kept from today

Ruts by mass, craters, shovel, surface grip per gamemat, hard surfaces, building lock + destruction fix,
vehicle get in/out handling (Foliage layer, TRACEABLE hiding), groove/reveal material, collider smoothing.

## 6. Multiplayer

- Server-authoritative: only the server computes deformation; clients apply received heights.
- Skin and hole layout are never sent (every machine builds them from the same terrain).
- One RplComponent on the placed entity (static RplId).
- Live: existing per-patch rect message plus patch key: `(px, pz, x0, x1, z0, z1, packed mm)`, heights as
  delta from the untouched surface. Range widened from 14 bits (+-8 m) to 16 bits (+-32 m).
- JIP: RplSave carries only the list of deformed patches; the server then sends their heights to that player
  in batches, nearest first. Size limit of RplSave to be measured (task 5).

## 7. Build order (each step checked in play before the next)

1. **Override test:** an addon ships a changed Arland `.ttile` (by path) -> in play, holes read in the scan.
   If this fails, option 2 works only on our own maps / an Arland copy - stop and re-plan.
2. **Coarse skin on Arland:** build time, memory, frame cost; decide 2 m vs 4 m far collider.
3. **Hole tool:** plugin mask export (roads, water, keep boxes) + Python writer; test map, then Arland.
4. **Ground refactor:** one sparse entity, on-demand patches, coarse swap, Edge Mode, depth boxes, floor.
5. **MP:** patch-keyed messages, JIP batching, dedicated server with 2 players.

Side issue before rut tests: a vehicle does not drive on the NEw test map at x 104, z 27.7 (suspect a
`default.emat` terrain surface without proper gamemat - unverified). Also remove the duplicate Faction /
Radio / Loadout managers in its default.layer.

## 8. Testing

Every task ends with an in-engine check that prints measurable log lines (probe pattern of
`MUD_TerrainOffProbe.c`), recorded with time and build. Throwaway probes are deleted when their task is done.

## 9. Out of scope

Runtime hole changes (impossible - no API); restoring terrain grass by our own grass meshes (not needed:
clutter shows on our ground); Stable build support.
