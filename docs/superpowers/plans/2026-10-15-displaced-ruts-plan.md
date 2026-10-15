# Displaced ruts - build plan (2026-10-15)

PARKED until the feature freeze ends (master list Lane 1 done, or the user lifts it). Nothing here is built.
Idea + first numbers: docs/superpowers/research/2026-10-15-voxel-options-deep-dive.md section 3.
Partial-split check: docs/superpowers/results/2026-10-15-displace-partial.md (Tools/displace_partial.py).
Labels: CODE (read in our script), MEASURED (offline tool), INFERENCE (not proven), USER.

## Goal
Ruts as sharp as 0.25 m voxels (walls ~45 deg vs 28-35 today, rms error about half) without the 0.25 m 3D cost.
Ruts go into a 2D "rut floor" (0.25 m, world y) instead of the voxel samples. A brick a rut touches is built as
today, then every triangle is split 2 x 2 and each vertex is pulled down: y = min(y, floor(x, z)).
One mesh, one collider (BuildSub builds the body from the same buffers - CODE), same op stream (MP / JIP format
unchanged). Switch: `MUD_VoxelProbe.DISPLACE_RUTS` (false = v035 voxel ruts).

## Settled offline (MEASURED, don't redo)
- 2 x 2 split + 0.25 m floor is the right pair (4 x 4 split on the same data gains nothing).
- Split rule: split EVERY triangle of a brick once any triangle in it would be pulled (any of its 6 split points).
  0 cracks, 0 T-junctions inside a brick. No "+1 ring" needed. (Splitting only rutted triangles: same geometry,
  but 50-95 T-junction edges per 12 m - render sparkle / Bullet internal-edge bumps.)
- Floor corners without a rut blend to the vertex's OWN coarse y (the game has no true top). Same quality on plain
  ruts, better across craters (true->mesh max 0.337 -> 0.270 m).
- Cost: ~4x triangles in rutted bricks only (~500 per brick, MAX_V 8000 / MAX_I 24000 - CODE).

## Gate 0 - before any code (one short session)
1. Checkpoint (`mud_backups/checkpoint.ps1 save "before displaced ruts"`).
2. Material check: is a deep v035 rut clearly visible on MAT_TOP (default_MatTerrainObj1.emat) vs a plain
   material? mud-gotchas 2026-10-08: ruts on MatTerrainObject looked "subtle" (NormalBlend suspected, never A/B'd).
   If the material hides the shape, sharper geometry buys little -> fix / pick the material first.
   GlobalLayer probe (deep dive option 4): the 3 shipped emats read GlobalLayerUVSrc "UV set 2"
   (results/2026-10-14-material-search.md) - our mesh has one UV set. Use / copy an emat that reads UV set 1 or
   world position (INFERENCE).
   PASS = user sees the rut shape on MAT_TOP.

## Step 1 - data (no mesh change yet)
- MUD_VoxelColumn: `float m_aFloor[256]` (16 x 16 owned floor samples at 0.25 m; the +x / +z edge row reads the
  neighbour column, like the sample gather). Value FLOOR_NONE (e.g. 1e6) = no rut. ~1 KB per column, only
  meaningful in rutted columns (INFERENCE: fine within MAX_COLUMNS 6000).
- New `CarveFloor(op)` for kind 4 when DISPLACE_RUTS: same profile as CarveRut (fall = 1 - k^2 / 2, step
  m_fDepth, cap m_fB under m_aUndug, never under undug - FLOOR, RUT_ON check) but per floor sample:
  - u clamped to 0..1 (capsule) - removes the small bend gaps (MEASURED: 2 samples on the test arc).
  - surface for the per-pass step = min(field surface (SurfaceY), current floor) - else ruts never deepen.
  - writes floor = min(floor, new) - no Put, no 3D samples.
  - marks carved: MarkCarved(gi, gk) where the new floor is more than CARVE_TOL under the surface. Without this a
    rut on fresh ground never draws (cells draw only if IsHoleCell AND Carved - CODE BuildSub / Quad).
  - queues the bricks whose top crosses the changed floor samples (MarkBox equivalent) - AND the neighbour brick
    across a 4 m line when a changed sample is within ~0.75 m of that line. Else the neighbour stays unsplit and
    stale -> crack on the line (see "Brick-border rule").
    Why 0.75 m, not 0.25 (CODE CellPos / PolyPass): the shared edges between two bricks' quads are NOT on the line.
    A brick's quads use cells -1 .. SUB-1, so its mesh reaches 0.5 m into the -x / -z neighbour; the border
    vertices sit anywhere in that 0.5 m strip, and Floor there reads samples up to 0.25 m further = 0.75 m.
    Also the brick ABOVE / BELOW (4 m y lines, same column): ground near y = 4k is common, so a rut there changes
    triangles of two stacked bricks. Queue every brick whose y range is within ~0.5 m of the floor / surface.
- Ops that move the ground clear the floor ONLY at samples they changed (where Put moved the surface), not in
  the whole op box (a crater box is a square well past its rim):
  - crater (0) / trench cut (2): floor = FLOOR_NONE where the new surface is ABOVE the floor (fresh rim covers the
    rut); where they dig deeper the min keeps them anyway.
  - fill (3): floor = FLOOR_NONE in changed samples (a fill fills the rut).
  - tunnel (1): leave the floor (guard below keeps ceilings).
- Determinism: same op order on every machine -> same floor. No new net text. JIP replays ops as today.
- Check: log line `MUD rut floor: N samples, M carved cells, ms` per op. Floor values sane in a debug print.

## Step 2 - mesher
- After Polygonize, if the brick is "rutted": split pass over the top + dirt buffers. Rutted = the SAME test as the
  offline disp2_brick rule: any coarse triangle with any of its 6 split points (3 corners + 3 edge midpoints)
  pulled by Floor(). Not "has a floor sample under its surface" - see the brick-border rule.
- Brick-border rule (INFERENCE from the offline crack rule, not measured across bricks):
  - The offline "0 cracks" holds because a pulled edge midpoint is a split point of BOTH triangles on that edge.
    Across a 4 m line the two triangles are in different bricks, so it only holds if both bricks run the same
    split-point test on the same floor values (Floor at the line uses the shared edge-row samples - Step 1 reads
    the neighbour column for the +x / +z row). A per-brick test like "own column has a floor sample" can say
    "not rutted" for brick B while brick A pulls a midpoint on the shared edge -> crack.
  - Both bricks must also be rebuilt after the op (Step 1 neighbour queue). A stale unsplit neighbour = crack.
  - Overflow fallback (below) on one side of a line breaks this: brick B pulls coarse vertices only, brick A pulls
    a midpoint B does not have. Then A must not pull midpoints on edges that lie on the line to an overflowed
    brick (keep them on the straight edge = T-junction, no crack). Needs a per-brick "split" flag A can read.
    Order problem: A may build before B finds out it overflows (same batch or earlier). So B's overflow sets a
    persistent "no split" flag on B AND queues its rutted neighbours; they rebuild next batch reading the flag (one
    batch of crack at most). Overflow needs > ~2000 coarse tris in one brick (MEASURED busiest ~185), so this is a
    rare path - log it, don't optimise it.
  - Lines between a split and an unsplit brick still get T-junctions (offline: 40-85 per 12 m). Bricks are separate
    meshes / bodies, so no crack; render sparkle there is a Step 4 look check.
  - Check (Step 4): drive a rut ALONG and ACROSS a 4 m line and end one 0.1 m short of a line; look for gaps from
    below / at a low angle. Log `rut border pulls` = midpoints pulled on brick-line edges per batch.
  - Edge-midpoint cache (key = the two vertex ids, sorted) so neighbour triangles share midpoints - CellV only
    caches per cell; without it shading goes faceted (engine normals = equal-weight average, PROVEN-IDA).
  - UV of a midpoint = average of its ends (same tile per quad, KindAB).
  - Pull: y = min(y, Floor(x, z)), Floor = bilinear over the 4 floor samples, missing corners = the vertex's y.
  - Guard: pull only vertices less than ~1 m above the floor (tunnel ceilings / overhang lips stay).
  - Fold filter: after the pull, run each split triangle through Tri() (drops zero-area only - CODE) AND drop a
    non-top triangle whose normal flipped vs before the pull. CreateTriMesh keeps folded / zero-area tris (IDA
    sub_140A52060, no checks). Top triangles can't fold (the pull only moves y, so the facing of an upward
    triangle is fixed by its x/z shape; MEASURED 0 folds). Walls can: MEASURED 1 fold in 145 pulled wall
    triangles (rut_x_crater). Count drops in the batch log.
    Note: a dropped triangle is a hole in BOTH the look and the collider (same buffers). A folded triangle overlaps
    its neighbours, so the hole is usually covered, but not proven. If drops show in game: clamp the pull for that
    triangle's vertices (pull less until it no longer flips) instead of dropping it.
  - Decide grass vs soil on rut walls ON PURPOSE: today `top` is per coarse quad (n.y > 0.7). Keep it per coarse
    quad (rut walls stay grass) for step 2; a per-triangle pick is a later look test.
- Budget (MEASURED 2026-10-15 on the prototype shapes, busiest brick): crater_big 185 tris / 117 verts -> split
  740 tris (2220 indices) / 418 verts with shared midpoints (1110 without sharing). New verts = edges (~1.5 per
  triangle), not 3 per triangle. MAX_I (24000) binds before MAX_V: a brick over ~2000 coarse tris overflows when
  split (surface-nets max for 8^3 = 3072 tris -> 36864 indices). Prototype has no tunnels and no UV-parity copies
  (CellV) - check the real busiest brick in game (log coarse tris per rutted brick).
- Overflow: check BEFORE splitting (coarse tris x 4 x 3 > MAX_I, or verts + edges > MAX_V). Then don't split:
  pull the coarse vertices only (a soft rut, still a rut). Don't fall back to "no rut" - with floor-only ruts the
  field holds no rut there. Log it.
- Body: unchanged path (same buffers), so wheels feel exactly what is drawn.
- Check: "slow brick" log lines; new counter `split bricks / verts` in the batch line.

## Step 3 - the other readers of the ground
All read the voxel field only today (CODE); with ruts out of the field they must take min(field, floor):
- `SurfaceY` (CarveFloor's own per-pass surface) - step 1.
- `SurfaceAtI` / `ColSurf` (DIG NOW, MUD_MudSheet - parked, still keep it right).
- `ColumnDepth` (FillTrench wall check, TrenchCols depth).
- `DrawsAt` reads Carved - fine once step 1 marks carved.

## Step 4 - SP test (DISPLACE_RUTS true, checkpoint first)
Pass:
- Look: rut walls visibly sharper than v035 on the same drive (user, side by side with the switch).
- No cracks / sparkle at brick lines, no dark faceting in ruts.
- Cost: rut ms/s on the client vs v035's ~20 ms/s (freeze-test.md), no 'slow brick' spam, fps over ruts.
- Feel: no new suspension jitter (2026-10-08 lesson: jagged ruts = jitter). If twitchy: rut edge profile to 0 at
  the rim (today fall = 0.5 at k = 1 = a half-depth step that a 0.25 m floor now shows).

## Step 5 - MP (the real gate)
- Robot test as CLAUDE.md (DS or WB host + peer), >= 6 runs per setting, DISPLACE_RUTS false vs true.
  PASS = corrections not worse than v035 (0 / 90 s).
- JIP: join after ruts + crater + trench + fill; joined client's ruts match the host's (screenshot same spot).
- Then F2 stress (5-10 vehicles) as master list Lane 1 item 6.

## Rollback
DISPLACE_RUTS false = v035 exactly (CarveRut path untouched). Checkpoint from Gate 0.

## Later (only if step 5 passes)
- Raised side ridges (master list FIRST IN LINE) as a second floor value with max blend (push UP the coarse top).
- Wheel sink (sinking-deep-dive.md): tie sink depth to the rut step about to be cut, so a wheel in its own rut
  doesn't sink twice. Sink is the "now" feedback RUT_BEHIND leaves out (rut shows 3 m later).
- Same 2D floor for trenches / shovel where no tunnel is near (architecture review option c) - decide after.

## Open questions for the user
- Start before the freeze ends, or keep parked?
- Rut walls: grass (per quad, default) or soil (per triangle)?
