# Voxel OVERLAY mode: review brief (2026-10-13)

For a second AI to check. **The code is not compiled and not tested in game yet.** The user reloads the scripts in Workbench
themselves. Game: Arma Reforger Experimental 192624, Enforce Script. Mod root: `addons/Testing plan vech msh`. Scripts are in
`Scripts/Game/Mud`. Line numbers were taken from the files right after the edit; they may drift.

## 1. Background: what existed before

- **Ground system.** The terrain has cut-out "hole" cells (2 m). Our own ground replaces it there:
  - a coarse skin of 2 m cells in 64 m chunks (`MUD_CoarseSkin.c`);
  - 8 m fine patches with 0.25 m vertices (`MUD_FinePatch.m_aH`, built in `MUD_WorldGround.c`).

  Ruts, craters and trenches change `m_aH` on the server. Changed rectangles go to the clients as int16 mm values.
- **Voxel probe (`MUD_VoxelProbe.c`, TEMP).**
  - **Tiles.** Each tile is 64 m, placed on a world grid with a pitch of 63.5 m. Neighbouring tiles share one cell column.
  - **Cells and meshing.** Cells are 0.5 m. The mesh is built with surface nets, in sub-blocks of 8^3 cells. Each sub-block gets a mesh and a static trimesh collider.
  - **Op log.** Kinds: 0 crater, 1 tunnel capsule, 2 trench cut, 3 fill one layer. Text form: `seq|kind|x|y|z|a|b|piece|plan`.
  - **Server side.** The server `Issue`s each op: it encodes it, decodes it back (so the server rounds the same way the clients do), applies it, and broadcasts the text (`MUD_WorldGroundEntity.RpcDo_VoxelOp`).
  - **Client side.** Clients hold ops by seq and apply them in order (`Receive`/`Drain`). On join they ask once for the whole log (`MUD_VoxelNet.c`).
  - **A new tile:** seeds ground heights, then density (a few rows per frame), replays every logged op that reaches it, builds hidden, then goes live.
- **Before today, two modes:**
  - `GROUND = true` (the test until now). Fine patches are off everywhere. Tiles stream in within 60 m of the camera or players and are dropped past 100 m. The user disliked this: destruction "pops in" when you come near.
  - `GROUND = false`. The old single-player test: it sinks the heightfield 4 m under a tile. That sink goes through `MarkDirty` and so is networked, which is why that mode is not usable in MP.

## 2. Goal of this change (user request)

Destruction should load while a player joins, as the old heightfield system did. The design that was picked:

- Voxels are an **overlay**.
- Tiles exist only where an op happened, and they are kept for the whole match.
- Every machine makes the same tiles from the same ops.
- A joining client builds all tiles before it spawns.
- Under a live tile, the skin cells and fine-patch cells it carries are hidden (both mesh and collider) on each machine locally. No heights change, so nothing is networked.

## 3. What was changed

### `MUD_VoxelProbe.c`

- **Mode switches.**
  - `GROUND = false`, `OVERLAY = true`, `MAX_OVERLAY_AREAS = 48`, `JOIN_MS = 25` (lines 173-177).
  - `OverlayMode()` (408), `Networked()` = ground or overlay (415), `MaxAreas()` (422).
- **Gating.** `Blast`, `TunnelInput`, `ServerTunnel` and `Tick` used to check `GroundMode()` for MP. They now check `Networked()`.
  - In overlay, `Tick` creates the instance on every machine once the skin and the WorldGround are ready (372). Clients drain ops only after that point.
- **Tile creation.** `Submit` (951) already made the home tile and the 3x3 neighbours the op box touches when not in ground mode.
  - The cap is now `MaxAreas()`.
  - A tile that `NewArea` refuses (too steep or no ground) goes into `m_sNoTile` (977, 993). The cap being full is a separate case and is not recorded there.
- **Determinism.** `GroundAt` (1067) returns `MUD_WorldGroundEntity.UndugY` in overlay (1073). That height is undug: skin triangles, or the heightmap on kept terrain. It contains no ruts and does not depend on older tiles. The steepness check uses `GROUND_SPREAD` (24 m). Kept and hard columns (`Kept`, `Hard`, from skin hole cells) are computed in overlay too.
- **`Issue` (809).** If `Submit` did not add the op to `m_aOps` (821), the seq is rolled back and nothing is broadcast. Before this, a sent op that was missing from the JIP log left a late joiner waiting for that seq forever.
- **`Step`.**
  - `cover = ground || overlay` (2008). With it, there is no heightfield warm or sink, and `GoLive` runs when a tile has nothing queued.
  - `Stream()` runs only in ground mode, so overlay never drops tiles.
- **Join boost.** `Joining()` (485) is true for an MP client in overlay with no locally controlled entity. While it is true, `JoinBudgets()` (2172) sets the seed, build and urgent budgets to `JOIN_MS`. It logs `joining ...` and `joined after N ms ...`.
- **`UpdateCover`.** In overlay it calls `skin.SetVoxCells` (2485) instead of `SetCoveredCells`, then `MUD_WorldGroundEntity.VoxCellsChanged(cx0, cx1, cz0, cz1)`.
- **New tile meshes** spawn hidden in overlay too (2848) and show in `GoLive`.
- **Trench and shovel.**
  - `TakesDig(lay)` (432) is true always in ground mode. In overlay it is true when `RoomFor` (458) says every tile square the layout box reaches exists, or the missing ones fit under the cap and none of them is in `m_sNoTile`.
  - `TrenchCols`: in overlay, a column that no tile holds counts as depth 0 (undug). A column in a still-seeding tile makes the result "not ready" (1671 onward).

### `MUD_CoarseSkin.c`

- `CELL_VOX = 128` (74). `BuildGeometry` (682 onward): a VOX cell gets no mesh and no collider slot, whatever COVER or SLEEP say.
- `IsVoxCell` (970), `SetVoxCells` (980): set or clear the bit and rebuild the touched chunks once.

### `MUD_WorldGround.c`

- **Patch cell bit.** `VOX = 32` (96) in `MUD_FinePatch.m_aCell`. Bits 0-3 are the slot and bit 4 is TLBR.
  - `InitPatch` sets it from the skin (2952).
  - `BreakHard` keeps it (3951).
  - `FillIndices` treats VOX like a kept cell: slot -1, mesh collapsed (5375).
  - `CheckNew` skips VOX cells (5771).
  - `FindIdx` keys on the full `m_aCell`, so VOX layouts get their own cached index sets.
- **`NoShownCell` (3152).** When every cell of a patch is kept or VOX, a collider with no body is correct. `BuildStep` no longer counts that as a build failure (3270).
- **`VoxCellsChanged` / `DoVoxCells` (3167/3175)** run on every machine when a tile goes live. For each patch already made in that range, the VOX bits are recomputed. Then:
  - a live, awake patch gets `BuildCollider` right away, in the same frame the tile shows;
  - a patch at `BUILD_COVER` is sent back to `BUILD_COLLIDER` (3217);
  - the mesh is marked dirty and goes through the rebuild queue.
- **`UndugY` (3235)** is a static wrapper over `Heights(..., undug)`.
- **Hooks.** `CarveTrench` / `FreeDig` / `FreeFill` route to the voxel versions when `MUD_VoxelProbe.TakesDig(lay)` is true (4216, 4335, 4366).
- **`GroundRescue`.** It used to teleport the local player up when their feet were 0.7 m under our heightfield. It now skips positions in VOX cells (4747). Without the skip, walking down a voxel crater would have tripped it.
- **Unchanged:**
  - `HasHole` (3063) still returns false only in ground mode, so fine patches are back on.
  - Crater hooks: `InArea` goes to `Blast` (5093). The normal path calls `Blast` at 5151; if that returns false, the heightfield crater is dug.

## 4. Known gaps (accepted for this test)

1. Ruts don't show inside a tile, because the patch there is hidden. The server still presses ruts into the hidden heightfield and sends them out.
2. Heightfield ruts, craters and trenches made where a tile is later created get covered by the tile, which seeds from undug heights.
3. **Overlap at the tile edge.** Where a skin cell is only partly inside a tile, both the skin/patch and the voxel surface show. The voxel surface sits 2 cm above the undug ground. This is the same as in ground mode.
4. **Cost and memory.**
   - The `GoLive` frame rebuilds the colliders of all live patches in the tile at once, which may cause a spike.
   - Each tile is about 9 MB (5-20 MB). 48 tiles is roughly 450 MB on every machine.
   - Any player can create tiles with the tunnel key or the shovel. Tunnel requests are rate-checked; nothing else limits this.
5. **Build window.** While a tile is still building, its hidden voxel colliders already exist (+2 cm above undug). Skin and patch colliders still carry the ground, so a crater can't be entered until the tile is live.
6. **Join length.** A client with many tiles needs about 48 x ~400 ms of work at 25 ms per frame. That is several hundred frames of join.
7. **Lost skin bits.** If a skin chunk is rebuilt from scratch (`BuildChunk`), it loses the COVER and VOX bits. This only happens at start-up today.

## 5. What to check (questions for the reviewer)

1. **Determinism.** Can the server and a client ever make different tiles, or refuse different ones? Look at `Submit`, `NewArea`, `GroundAt`, the cap, op order, and the timing of when `s_Inst` exists on a client.
2. **Seq rollback.** Is the rollback in `Issue` safe? Can a client's `Submit` log an op the server did not log, or the other way round?
3. **Collider holes or ceilings.** In `DoVoxCells`, check patches in each state: building, asleep, waking, `BUILD_INDEX` / `BUILD_COLLIDER` / `BUILD_COVER`. Also check the wheel-hold path in `BuildCollider`.
4. **Skin flags.** Check `BuildGeometry` for every mix of COVER, SLEEP and VOX.
5. **Index cache.** `IdxSet` cache size is `IDX_SETS = 16`. VOX layouts add variety. Is there a thrash risk?
6. **Enforce pitfalls.** Loop-body locals must be initialised. Check float-to-int `Math.Floor`, static methods calling protected statics, and the missing ternary.
7. **Trench routing.** Check the edge cases of `TakesDig` and `RoomFor`. Could a dig be lost or applied twice? For example, the first swing at a steep tile returns -1 before the tile is marked as refused.
8. **Other heightfield users.** Do other systems read our heightfield in tile areas and misbehave? Candidates:
   - the crater air checks;
   - `MUD_Dig.AtGround`, `MUD_TrenchGhost` and `MUD_TrenchSite` via `SurfaceY`;
   - `MUD_Belly`;
   - ahead ruts;
   - `GroundCovers`.

## 6. Test plan (not run yet)

1. Reload the scripts. Check the log for `MUD voxel probe: OVERLAY on`.
2. Single player: fire a mortar, wait for `live ... ms after it was made` and `patches took the voxel cells`. Check that there is no double surface or hole, that walking in does not teleport you out, and that a vehicle can drive in.
3. Shovel (F) on fresh ground. The first swing makes a tile, and the hole shows about 1-2 s later.
4. MP with a server and one client: make 3-4 craters. Both machines should show the same result.
5. Join a second client late. The log should show `joining` and then `joined after N ms`. All craters should be there at spawn. There should be no `waiting 10 s for op seq` warning.

## 7. Related docs

- Proven engine facts: `docs/superpowers/research/mud-gotchas.md`. These include the IDA physics-layer findings from 2026-10-13: the layer matrix is symmetric, and there is no vehicle-only layer that soldiers don't also hit.
- External design review this follows: `Downloads/Terrain deformation system design review(1).md`.
