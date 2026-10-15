# Voxel bricks: design spec (2026-10-13)

Status: SPEC, not built. Replaces the 64 m tile layer of the voxel OVERLAY mode
(`Scripts/Game/Mud/MUD_VoxelProbe.c`, brief: `docs/superpowers/results/2026-10-13-voxel-overlay-review-brief.md`).
Inputs: user test 2026-10-13 ~19:40, a ChatGPT redesign proposal, a ChatGPT review of the overlay brief. Every
reviewer claim used here was checked against the code (section 9).

## 1. Problem (measured, log `logs_2026-10-13_19-21-15/console.log`, overlay run 19:39-19:40, SP)

| Event | Time to visible | Why |
|---|---|---|
| Blast 1 on fresh ground | 1299 ms | whole 64 m tile: seed 122 ms + 512 sub-blocks (build 233 ms) at 4 ms/frame over 76 frames |
| Blast 4 on fresh ground | 1277 ms | same |
| Tunnel dig 15 on fresh ground | ~1087 ms before digs show | same |
| Blast 2 / tunnel digs inside a live tile | 4-63 ms | only 4-32 sub-blocks rebuilt |

User-reported (same run): the heightfield shows for a long, very visible time before the voxels swap in. Ruts vanish
when a tile goes live (tile seeded from undug heights). The first tunnel dig on new ground waits a long time.

Root cause: the unit of conversion is a 64 m tile. A ~10 m crater pays for 4096 m^2.

Measured unit cost: 235 ms / 512 sub-blocks = **0.46 ms per 8^3 sub-block** (same log, "built - 512 sub-blocks ... build
235 ms").

## 2. Goal

- An op (crater, tunnel, trench, fill) converts only the ground it touches, plus a pad, and shows within ~2-4 frames.
- Ruts, berms and earlier heightfield craters carry into the voxels.
- The result is identical on every machine (server ops in seq order, the same seed bytes).
- Joining clients build everything before they spawn (kept from the overlay). Memory scales with what was dug.

## 3. Core idea: the sub-block IS the brick

The existing sub-block is 8 x 8 x 8 cells at 0.5 m, which is a 4 m cube. Keep the mesher (`BuildSub`, surface nets, UV
rules, `MakeMesh`, `MakeBody`), the op log and seq order, decoding on the server before applying, hidden builds and the
all-at-once swap (`SwapPending`), hard / kept columns, and `JOIN_MS`.

What changes: the density moves from one big array per tile (`MUD_VoxelArea.m_aD`) to sparse bricks on one world grid.

### 3.1 Grid

- Brick grid origin = the skin grid corner (`MUD_CoarseSkinEntity.GetGrid` x0, z0). Bricks are 4 m, so every brick
  sits on exactly 2 x 2 skin cells, and 2 x 2 bricks make one 8 m fine patch. The tile pitch of 63.5 m that put edges
  mid-cell goes away.
- y: world multiples of 4 m (sample y = j * 0.5 world-aligned, as now).
- Global sample index (gi, gj, gk): x = X0 + gi * 0.5, y = gj * 0.5, z = Z0 + gk * 0.5.

### 3.2 Sample ownership (no shared or duplicated samples)

- Brick (bi, bj, bk) owns samples gi in [8bi, 8bi+7], and the same for j and k. Each sample is stored exactly once.
  This removes the seam-copy problem: a new brick next to a dug one never duplicates its edge samples.
- `float m_aD[512]` per brick (2 KB) for now. int8 in a narrow surface band is a later memory step and not needed:
  a crater is ~50-75 bricks, about 150 KB.
- `Sample(gi, gj, gk)` looks up the brick (map key from bi, bj, bk) and returns its value. With no brick there:
  - the column is converted (has a seed) but has no brick at this height: below the band = solid (+1), above = air
    (-1). Rows are made lazily when an op writes there (3.4).
  - the column is not converted: no value. Mesh cells that need it are not emitted (3.5).

### 3.3 Columns and conversion

- **Column** = a 4 m x 4 m brick column (bi, bk). Converted columns are kept in `map<int, ref MUD_VoxelColumn>`
  holding:
  - the seed ground height per owned sample column (8 x 8 floats, world y);
  - kept / hard flags per sample column (as `m_aKept` / `m_aHard` now);
  - the brick rows made.
- **Conversion set of an op** = every column the op's x, z box (`Prepare`: `m_vLo` .. `m_vHi`) touches, plus **one
  ring of pad columns** around it. The op never carves the pad (box + ring), so the outer edge of a converted region
  always lies on untouched ground. That is the only place voxel meets heightfield.
- **New columns** = conversion set minus the columns already converted. Deterministic: the same ops in the same order
  make the same sets on every machine.
- **Band of a new column**: rows covering [min seed y - FLOOR - 1 cell, max seed y + rim + 1.5 m], as the tile's
  `GroundDone` did. FLOOR (3 m) already caps every carve under the undug ground.

### 3.4 Seeding from the server's dug heights (sent with the op)

- On the server, `Issue` works out the new columns of the op (3.3) and attaches their seed: for each new column in
  ascending key order, 8 x 8 values of **dug - undug in mm** (int), from `MUD_WorldGroundEntity.Heights` (dug includes
  ruts, berms and heightfield craters).
- Every machine seeds from `undug(x, z) + delta`. undug comes from `MUD_WorldGroundEntity.UndugY`: skin triangles, or
  `GetSurfaceY` on kept terrain. Both are terrain data, not entity traces, so they are the same on every machine.
- Encoding (10th op text field): values joined with `,`, with runs of zeros written as `zN`. Untouched ground is all
  zeros, so a fresh crater's 5 x 5 new columns (1600 values) come out at a few characters.
- Size cap: if the seed text passes SEED_CHARS (start at 6000), quantise to cm. If it still passes, refuse the op as
  voxels (see 3.7). Measure the real size on a rutted field before tuning.
- Columns converted earlier ignore later seeds. Ruts pressed later inside a converted area no longer show (accepted:
  they are hidden heightfield).
- Why server bytes and not each client's own heightfield: heightfield rects and voxel ops travel separately, so a
  client may not have the ruts yet when the op arrives.

### 3.5 Meshing across bricks

- `BuildSub` reads samples -1 .. 8 around the brick (cells -1 .. 7, `MUD_VoxelCells`). It now reads them through
  `Sample()`, so neighbour bricks are read in place.
- A cell is emitted only if all 8 of its corner samples have a value. At the edge of the converted region the mesh
  stops short there. That is fine, because the outer overlap strip (3.6) shows heightfield under it.
- A sample write marks every brick whose cells use it (the current `QueueBox` grow-by-2 rule, in global indices).

### 3.6 Seam and VOX cells (the main risk)

- Of the converted region's skin cells (2 m), the ones in the **outer 2 m strip** (the outer half of the pad ring)
  stay heightfield: shown, with their collider on. The voxel mesh also covers them, LIFT (2 cm) above. Only the
  cells inside that strip get VOX (skin and fine patch hidden, mesh and collider).
- So at the inner edge of the visible heightfield the voxel surface lies on top. No crack can be seen from above, and
  the pad is never carved, so none can be seen from the side.
- Risk to measure: the seed is sampled at 0.5 m but the heightfield is at 0.25 m, so a rut can poke through the 2 cm
  LIFT in the strip. If the screenshots show z-fighting, raise LIFT in the pad columns only (e.g. 4 cm).
- With bricks, VOX is computed over the union of converted columns, so a crater across what used to be a tile seam
  has no leftover skin strip or collider lid. Both reviews flagged this for tiles; bricks remove it by construction.

### 3.7 Op outcome rule (fixes the half-crater case)

- An op is either **fully voxel** or **fully heightfield**, never both.
- The server logs and sends it (and `Submit` returns true) only if every column of its conversion set is converted or
  can be (under the brick cap, seed under the size cap). Otherwise it is not logged, the seq is rolled back (already
  in `Issue`), and the caller digs the heightfield version.
- Deterministic: the cap check uses only the op log, which is the same everywhere.
- Cap: `MAX_BRICKS` (start at 20000, ~40 MB of density plus meshes, measure). The shovel and tunnel may convert too:
  a swing converts ~3 x 3 columns, not a 64 m tile.

### 3.8 Go-live per op group

- An op's new bricks are built hidden. Bricks it changed in existing columns are built hidden as pending (the
  current `m_bNew` path). When every brick of that op (and of any ops queued behind it on the same bricks) is built,
  then in **one frame**: show the new bricks, swap the pending ones, set skin `CELL_VOX` on the newly carried cells,
  and update the patches (3.9).
- Build order: urgent budget (`URGENT_MS` 10 ms), nearest to the op first. Expected cost of a fresh mortar crater:
  5 x 5 columns x 2-3 rows = 50-75 bricks x 0.46 ms = 23-35 ms, so 3-4 frames.
- A tunnel: capsule box ~2.1 m + pad gives 3 x 3 to 4 x 4 columns, ~20-40 bricks, ~10-18 ms, so 1-2 frames plus
  network delay on a client. No client-side prediction for now (it needs rollback; revisit after the test).

### 3.9 Patches in the go-live frame (the reviewer's spike fix, adopted)

- A patch whose cells are all VOX or kept: drop its body (cheap, the voxels carry it). Don't rebuild it.
- A patch with mixed cells: rebuild its collider now (few of them, since one op covers ~2-4 patches).
- A building patch: `BUILD_COVER` goes back to `BUILD_COLLIDER` (already in `DoVoxCells`). An asleep patch: only
  its `m_aCell` changes (already).
- Mesh through the rebuild queue.
- **Per-patch index set reference** (adopted): `MUD_FinePatch` keeps a ref to its `MUD_IdxSet` and recomputes only
  when its `m_aCell` changes. The 16-slot shared cache stays only to share memory. VOX patterns at 2 m granularity add
  many layouts, and each cache miss is ~2.5 ms.

## 4. Data and code changes (MUD_VoxelProbe.c unless noted)

- **Remove:** `MUD_VoxelArea` as the density holder, `NewArea` steepness / tile logic, the tile grid
  (`PITCH`, `TileArea`, `RoomFor`), `Stream` and ground mode (GROUND stays false; delete it later), the heightfield
  sink path.
- **Add:**
  - `MUD_VoxelBrick` (bi, bj, bk, `float m_aD[512]`, the sub fields of today's `MUD_VoxelSub`);
  - `MUD_VoxelColumn` (seed, kept / hard, rows);
  - `map<int, ref MUD_VoxelBrick>` and `map<int, ref MUD_VoxelColumn>`;
  - `Sample()`, `SetSample()`;
  - `ConversionSet(op)`, `EncodeSeed` / `DecodeSeed`;
  - `MUD_VoxelGroup` (an op's bricks still building, for go-live).
- **Carve / CarveTunnel / CutTrench / FillTrench:** loop over global sample indices in the op box. `ColumnDepth`
  reads the column seed.
- **Op text:** a 10th field, seed (`-` = none). `Decode` accepts 9 fields, for old JIP.
- **Brick entities:** spawn as a new `MUD_VoxelSubEntity : GenericEntity` class, so that `MUD_Surface.IsOurs`
  (MUD_Surface.c:45) and `GroundCovers` (MUD_WorldGround.c:4860) count voxel walls as cover. Today a grenade next to a
  soldier in a voxel crater ignores the wall.

## 5. Other systems in VOX cells

| System | Today | Change |
|---|---|---|
| `AtGround` (shovel allowed) | tol 2.75 m (MUD_WorldGround.c:4032; MAX_LAYERS 10 x 0.25 + 0.25) < FLOOR 3 m: refused at the bottom of a deep crater | in VOX cells tol = FLOOR + 0.25 |
| `GroundRescue` | skips VOX cells (done) | keep |
| `GroundCovers` | only patch / skin entities | add `MUD_VoxelSubEntity` (section 4) |
| Rut pressing, ahead ruts, belly scrape | press the hidden heightfield, networked | server: skip vertices in VOX cells (saves bandwidth; no visible change) |
| Approach ramp (`m_bRamp`, MUD_WorldGround.c:308) | cuts the hidden heightfield, so no ramp at voxel lips: trucks may stop dead | OPEN: needs a voxel-side answer (ramp as voxel op, or a vehicle-only proxy). Not in this spec; test the truck first |
| Crater air check | uses undug inside voxels (`InArea` branch) | `InArea` = point in a converted column |
| Trench ghost / site (`SurfaceY`) | read the hidden heightfield | later: a voxel top height query |

## 6. Join and memory

- JIP replays ops with their seeds. A client builds only bricks, so a join with N craters costs ~N x 30 ms of
  building, not 48 x 400 ms. `JOIN_MS` stays.
- `Joining()` is also true while dead or spectating. Harmless; the log line can show up there.

## 7. Order of work

1. Brick store, `Sample` / `SetSample`, conversion set, seeding from undug only (no seed field yet). SP crater test:
   time to visible, seam look.
2. Go-live per group, VOX = interior cells, patch frame rules (3.9), per-patch index ref.
3. Seed field: server encode, client decode, cap. Test: crater on a rutted field keeps its ruts; seed text size.
4. Op outcome rule + `MAX_BRICKS`. Trench / shovel / tunnel through bricks.
5. `MUD_VoxelSubEntity` + cover, `AtGround` tolerance, skip rut pressing in VOX cells.
6. MP: 2 clients + a late joiner.

## 8. Tests (with log lines to add)

1. Fresh-ground mortar: the log "group live N ms after the op, B bricks, F frames" should be under 100 ms. The user
   watches the swap: is it visible?
2. Crater on a rutted field: ruts kept around the crater (screenshot before / after).
3. Tunnel on fresh ground: the first dig shows in under 100 ms (SP).
4. Crater across an old tile seam position, and a crater next to an earlier one (shared bricks): no cracks, no lid.
5. Overlap strip: look at the region edge closely from low angles for z-fighting.
6. Truck driving into a voxel crater (lip stop?). Shovel at the bottom of a 3 m crater. A grenade next to a soldier
   lying in a voxel crater (cover).
7. Longest frame in the go-live frame over rutted ground (`MUD_FrameLog`).
8. MP: same craters on both clients; a late joiner sees everything at spawn, with no `waiting 10 s for op seq`.
   Seed text size per op in the log.

## 9. Reviewer points: checked and how they were handled

| Point | Checked | Decision |
|---|---|---|
| Go-live spike over many patches | true for 64 m tiles (8 x 8 patches) | bricks touch 2-4 patches; drop all-VOX bodies anyway (3.9) |
| Steep first swing lost | true (RoomFor passed, NewArea refused) | gone: no tile steepness check with bricks |
| Half voxel / half heightfield crater | true (`Submit` returned `home != null`) | op outcome rule (3.7) |
| Any player can exhaust 48 x 9 MB | true | bricks: memory by dug size, `MAX_BRICKS` |
| Seam strip / lid at tile edges | true for tiles | union of columns + pad ring (3.6) |
| Index cache thrash | patches hold no IdxSet ref (MUD_FinePatch.c) | per-patch ref (3.9) |
| `AtGround` tol "1.75 m" | wrong number: it is 2.75 m (MAX_LAYERS 10), but still < FLOOR 3 m | fix in VOX cells (5) |
| `GroundCovers` ignores voxels | true, `IsOurs` = chunk or patch only | `MUD_VoxelSubEntity` (4) |
| Ramp cuts hidden heightfield | true (`m_bRamp`) | open (5); the "top-surface proxy" from the reviewer's earlier message is not in our files |
| Undug must not trace entities | `UndugY` = skin triangles / `GetSurfaceY` | fine |
| int8 narrow-band density | not needed at brick scale | later, if memory shows it |
| Client tunnel prediction | needs rollback | deferred until after the test |
| Losing bits in `BuildChunk` | only at start-up today | add an error print if it runs after the voxel probe started |
