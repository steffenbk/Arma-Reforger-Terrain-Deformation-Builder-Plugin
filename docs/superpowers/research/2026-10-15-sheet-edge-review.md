# Sheet edge review (2026-10-15)

Read-only review of `Scripts/Game/Mud/MUD_MudSheet.c` (BuildTile, ApplyGroundOp, TrenchGround, DigNow) against screenshots 14 (tunnel mouth, before the hole curtains) and 15 (sheet trench, after).
PROVEN = read in the code (or the log). INFERENCE = how I match it to the picture.

Log check (WB `logs_2026-10-15_11-54-08/console.log`, last line 15:46): 154 `MUD one grid: op kind 2` lines (trench digs taken by the sheet), 0 kind 3, 697 kind 4, 7 kind 0, 145 `dig now:` (voxel go-lives). The last trench ops step about 0.33 m in x and 0.9 m in z, so the trench runs at an angle to the 0.15 m grid. PROVEN.

## 1. Screenshot 15 (sheet trench, no tunnel)

What makes the look, most to least visible:

- **a. Brown curtains with vertical stripes come from ONE_SKIRT (tile border) and have zero texture height.** PROVEN: the tile skirt (lines ~3544-3557) copies each top UV to the bottom vertex, so the texture has no height on the curtain. Every pixel column repeats one texel, which gives vertical stripes. Skirts are always in the dirt section (`tix`), so they are always brown. A skirt shows wherever this tile's border sits higher than the ground next to it.
- **b. The two tiles at a border disagree on height near a dig.** PROVEN: the TAPER distance `bd` is seeded only from Bare vertices *inside this tile* (lines 3130-3177; the comment at line 123 already says so). Take a dug vertex 1 cell inside tile A. A tapers the shared border vertex to +0.04 m. Tile B has no Bare vertex in range, so it draws the same vertex at full LIFT + THICK = +0.16 m. That leaves a step of up to about 0.12-0.16 m along a straight 3 m line, and B's skirt fills it with a striped curtain. INFERENCE: these are the thin straight striped bands in 15 (top centre) and the brown band in 14.
- **c. Sheer one-cell walls between trench pieces.** PROVEN: TrenchGround (line 1365) does `if (owner >= 0) continue;`. A vertex that belongs to another layout piece gets no slope from this piece's dig. So where two pieces are dug to different depths, the drop happens across one 0.15 m cell, with no SLOPE_RUN. Even inside a piece, the slope run is depth x 0.58 = 0.145 m per 0.25 m layer, which is less than one cell. So every wall is one cell wide. The UV is planar world XZ (line 3281), so 0.15 m of texture gets stretched over 0.25-2.5 m of wall, and the wall goes streaky. The face is near vertical, so it is lit dark. The trench is at an angle to the grid (log above), so the top of the wall follows the vertex grid in 0.15 m steps: that is the staircase. INFERENCE: these are the tall jagged dark walls.
- **d. Squarish patches on the floor.** INFERENCE, three candidates in this order:
  1. Pieces dug to different layer depths (one swing = one piece, 0.25 m) make rectangular steps (see c).
  2. Normals come from one section of one tile only and are not area-weighted (mud-gotchas, IDA 2026-10-14). So the shading breaks at every 3 m tile border and at every dirt/grass section border. SMOOTH_EDGE sliver triangles count fully in the normal of a floor vertex.
  3. Old texture drag stays on dug vertices: ApplyGroundOp sets D = 0 and Bare = 1 but never clears `m_aOu` / `m_aOv` (lines 1519-1523, 1560-1564; PROVEN). Next to the 0..1 UV clamp, that can smear a block.
- **e. Not the cause here:**
  - The mud-thickness step inside one tile is already tapered. A vertex next to a Bare one sits at +0.04 m (fadeB = 1 x 0.15 / 0.6 = 0.25). PROVEN.
  - The new HOLE CURTAINS and the dirt-rim q check only fire on BARE_VOX vertices. Only DigNow sets BARE_VOX (voxel go-live). Sheet trenches never reach the voxels (`TakeOp` returns true, VoxelProbe line 2087), so these two blocks do nothing on a sheet trench. PROVEN.
  - SMOOTH_EDGE only cuts the grass/dirt colour line. It moves no geometry; its cut vertex sits 1/3 down the wall, which brings a little grass onto the wall top. PROVEN.

So "worse on trenches" is because trenches are now sheet ground (kind 2 via TakeOp). Their walls are one-cell sheer quads with planar UV, plus the border skirts (a + b).

## 2. Screenshot 14 (tunnel mouth, see-under / layer look)

- PROVEN: DigNow marks a vertex BARE_VOX only where the voxel surface moved more than MOVED = 0.08 m (lines 1662, 1707). That vertex keeps its old `g`. BuildTile skips every quad touching such a vertex (line 3253). The drawn edge stops at the first vertex that moved less than 0.08 m, and is drawn at g + 0.04 m (in-tile taper) or g + 0.16 m (when the hole is in the neighbour tile: TAPER is per tile).
- INFERENCE: the voxel surface between that edge and the hole (0.5 m surface nets) drops faster than the 0.15 m sheet samples. So the sheet's last row hangs 0.05-0.25 m over the voxel lip as a thin grass slab: the layer look. You see under it because nothing closes it.
- PROVEN: the brown striped band in 14 is the tile skirt (it was already there before the change). The skirt only skips a border edge whose own vertex is BARE_VOX (line 3541). If the hole quad across the border is skipped one vertex in, this tile's border vertices are not BARE_VOX. The skirt is then drawn, hangs 0.45 m into the tunnel mouth, and has degenerate UV (stripes).
- The new hole curtains add the same kind of 0.45 m wall on every drawn side facing a hole quad. Their UV shifts only v by ONE_SKIRT / SIZE (line 3483-3491). For sides 2/3 (edges running along z), both top vertices share the same u, so all 4 UVs lie on one line: zero texture width = stripes again. PROVEN. They also stand vertical where the voxel lip is a gentle slope, so they can poke out as a wall that is not in the voxels (INFERENCE).

## 3. Fixes, smallest first

All of these add at most a few scalars or one 441 float array (1.7 KB). BuildTile is at about 69 KB of the 80 KB stack, so there is room for one more 441 float array, not for a 1024 vector one.

1. **Skirt and curtain UV (low risk, about 6 lines).** Give the bottom vertices the top UV shifted by `ONE_SKIRT / SIZE` toward the tile's inside, along the axis across the edge: sides 0/1 (edges along x) shift v, sides 2/3 (edges along z) shift u. Toward the inside means it never hits the 0..1 clamp (a tile spans only 0.25 of the range). Do the same in the hole-curtain block, by `hs`. This alone turns the stripes into plain continued dirt texture.
2. **Revert the two new HOLE blocks (low risk).** Put the hole-curtain block (3425-3503) and the q-loop BARE_VOX neighbour check (3230-3242) behind `static const bool HOLE_CURTAIN = false;`. For the tunnel lip instead:
   - (a) Use a separate `VOX_MOVED = 0.02` in DigNow lines 1662 / 1707 in place of MOVED. The sheet then opens wider than the voxel lip, so the voxel top shows and no slab hangs over.
   - (b) In the vy loop, ONE_GRID: if any 8-neighbour vertex is BARE_VOX, use `vy = g - ONE_DIP`, so the sheet edge tucks under the voxel top instead of floating. Risk: (a) makes tunnel holes in the sheet about 1 cell larger. The voxel bricks should cover it (4 m columns + ring), but that is INFERENCE; check that no sky shows at the mouth.
3. **Make the taper the same across tile borders (medium risk).** In BuildTile, after seeding `bd`, also seed from Bare vertices of the 4 neighbour tiles within `TAPER / CELL` = 4 cells of the border (live window `m_F` or the stored `m_mTrail` data). Each border vertex then gets the same height on both tiles, and skirts no longer show at dig edges. A simpler data-side version: in ApplyGroundOp, after marking dug vertices, run a 2nd pass over the op box grown by TAPER and set `D = min(D, THICK * dist / TAPER)` by world distance. That is the same on both tiles by construction. Side effect: a ~0.6 m dirt rim round each dig (D off THICK makes those quads dirt), which matches what the user asked for at holes.
4. **Wider, sloped trench walls (medium risk).** In TrenchGround:
   - (a) Use `run = Math.Max(depth * slope, 2 * CELL)` with `w = depth * (1 - od / run)`.
   - (b) Drop the `owner >= 0` skip, or slope into other pieces only when they are shallower: `ng = min(g, ...)` already keeps the deeper one.

   Walls then span at least 2 cells, so they look less stair-stepped and less stretched. Risk: SheetDepth feeds the server's TrenchCols / trench plan, so the dug shape changes how deep neighbour columns read. Test that the plan does not over-dig. The collider follows `g`, so it stays matched.
5. **Clear the texture drag on dug vertices (low risk, INFERENCE for the floor patches).** Where `dug` / `dug2` set D = 0, also set `m_aOu = m_aOv = 0`. Do the same in DigNow when a vertex moves more than MOVED.

Not recommended:
- Dropping THICK from the drawn height in ONE_GRID (grass at g, ruts below it) would remove every step. But it changes the rut look and puts tyres 0.15 m above the pressed mud. High risk.
- A triplanar or own-normal fix for wall stretch is not available (no triplanar emat shipped; script meshes take no own normals).

Suggested order: 1 + 2 together (one test, quick to judge), then 3, then 4 if the trench walls still read as stairs.
