# Option A deep dive - rut cells (2026-10-15 late)

Follows research/2026-10-15-rut-options-round2.md option A ("rut cells: small 2D rut tiles that replace the ground
in their cell"). User: "do more research into option A". Labels: CODE (master 6bb6738 / branch), MEASURED (file
given), PROVEN, INFERENCE (not tested). Nothing built.

## 1. Short answer
- **Step 0 is almost free and may give most of the gain: switch the patches back to the GRID mesher.**
  MEASURED results/2026-10-13-sdf-phase2.md: per 8 m patch, grid path mesh 0.86-1.10 ms / collider 1.37-1.58 ms;
  band mesher 2.9-4.75 / 4.2-5.5 ms (today's rut-field log: 6-7 ms each). The band mesher ("Layered-SDF test",
  2026-10-13) was built for deep HEIGHTFIELD craters / trenches with steep walls. Since v031 / v035 every op is voxel
  and the heightfield never deforms (m_aH stays 0 - the rut field hash shows 0 deformed patches), so its job is gone.
  The grid path already draws the rut floor (rut field Stage 3: "grid fallback mesher draws the floor too"), already
  skips VOX cells (BuildMesh `cell & VOX`), already has the groove reveal. Switch = `m_bBandMesher 0` in
  Prefabs/Mud/MudWorldGround.et. Expected (INFERENCE from the measured per-build times): ~4-5x cheaper rebuilds,
  i.e. ~120-140 ms/s -> ~25-35 ms/s for one vehicle - near v035's ~20 ms/s.
- **Rut cells (A) on top** cut the rebuild unit from an 8 m patch to a 2 m cell on REPEAT passes (another ~3-5x,
  INFERENCE) and allow 0.125 m only where ruts are. The first pass over fresh ground still costs a patch rebuild
  (the cell flip). Worth it only if Step 0 is not enough, or for the 0.125 m look.
- **A2 (rut tiles independent of patches)** also fixes the memory / "players on foot make no patches" problem, but
  needs the coarse skin to skip rut cells too - the biggest change.

## 2. How the existing cell kinds work (CODE - what A copies)
- Skin: MUD_CoarseSkin.m_aCell per 2 m cell: slot | CELL_TLBR | CELL_COVER | CELL_SLEEP | CELL_VOX (bit 7). The
  skin does not draw VOX cells.
- Patch: MUD_FinePatch.m_aCell[16] (4 x 4 skin cells per 8 m patch): slot | TLBR 16 | VOX 32 | LOOK 64.
  - VOX: patch mesh AND collider skip the cell (BuildMesh / BuildCollider `cell & VOX`, band FillBandOn slot -1);
    the voxel bricks draw it.
  - LOOK (old RUT_LOOK, off): patch MESH skips the cell, patch COLLIDER keeps it - a 3rd kind already exists.
- A flip (DoVoxCells / DoLookCells, MUD_WorldGround.c): every made patch over the changed cells takes the new bits
  and rebuilds collider + mesh IN THAT FRAME (so the old surface never lies over the new one). Cost = one full
  patch rebuild per touched patch.
- Voxel / patch border: not vertex-shared - a brick quad over a cell line draws when any corner is in a carved cell,
  LIFT 0.02 m above the heightfield (CODE Quad comment); skirts on the patch side (band m_fBandSkirt).

## 3. Design - A1: rut cells inside patches
- New patch cell bit RUT (128). A cell becomes RUT when the rut floor reaches it - the floor tile already says where
  (MUD_RutTile per 4 m, FieldInBox). Plus one ring of cells, so the floor is NONE on every border between a RUT cell
  and a normal cell (the offline crack rule: then both sides are the plain base ground at the border).
  NOTE: with r = RUT_R 0.5 m and 2 m cells, a track hugging a cell line makes both cells RUT; that ring is the main
  extra cost (INFERENCE: ~2.5 cells across one vehicle's two tracks).
- Patch skips RUT cells in mesh + collider (as VOX). One extra entity PER PATCH ("rut layer") draws all its RUT
  cells: one MeshObject (top = MAT_TOP, groove soil = 2nd section as today) + one trimesh body. Per patch, not per
  cell: bounds the entity / body count (12,500 cells for 10 km of tracks would be too many single bodies).
- Rut layer build: for each RUT cell a 9 x 9 (0.25 m) or 17 x 17 (0.125 m) grid: y = base + min(0, floor - base)
  with the patch's base heights (m_aBase, BaseAt - the SAME skin triangles the patch uses, +1 mm), edge lock as the
  patch. Groove reveal / soil rule as the patch (FIELD_SOIL_D). Cell borders at 0.25 m are the patch's own border
  vertices -> exact match; a 0.125 m tile puts extra border verts on the straight edge (floor NONE there) - T-junction
  on a line, no crack. RUT next to VOX: same as patch next to VOX (LIFT overlap from the brick side; the bricks read
  the same floor).
- Rebuild on a rut op: only the rut layer of the touched patches (FloorDirty marks rut layers, not patches). The
  patch itself rebuilds only when a cell FLIPS to RUT (or back).
- Flip early, not late: a cell can flip to RUT before any floor exists (the tile then equals the patch surface) - no
  visible or physical change. So flips can be batched when the patch is built / woken for a vehicle (Ahead) along the
  vehicle's heading, instead of one patch rebuild per new cell during the cut. INFERENCE: removes most first-pass
  patch rebuilds; needs a rule for which cells to pre-flip (e.g. the 3 x 3 cells round each wheel's projected path).
- MP: base (skin triangles) + floor (op stream) + kept-edge lock are the same on every machine -> deterministic. The
  building part of the lock is the review's F4 (decide it there).
- Get in / out: new entity class -> must join HideFromTraces (MUD_GroundEntity.SetTraceHidden), the v093 lesson.
  Also MUD_Surface.IsOurs / GroundCovers / BuildingDestructionFix lists (as MUD_VoxelSubEntity, CODE header comment).

## 4. Design - A2: rut tiles independent of patches (only if A1 is not enough)
- Skin gets CELL_RUT too: skin and patch both skip RUT cells; rut tiles live on their own (by camera distance on
  clients, by vehicle distance on the server), rebuilt from the floor tiles when needed.
- Fixes: ruts visible to players on foot without patches, and the floor-patch memory climb (patches go back to
  being freed). Costs: a skin chunk rebuild on every flip (CoarseSkin SetVoxCells path - cost NOT measured), a tile
  lifecycle (LOD / free / rebuild), far ruts drawn by nothing (skin skips the cell) unless a far tile stays -> needs a
  far-LOD rule. INFERENCE: big; do A1 + the patch-memory prompt first.

## 5. Cost estimate (INFERENCE from measured parts)
| Path | Rebuild unit | Per rebuild | Per rut op (1 vehicle) | Notes |
|---|---|---|---|---|
| Today (band) | 8 m patch | 6-7 ms mesh + 6-7 ms collider (ruts README) | ~2 patches | ~120-140 ms/s |
| Step 0 (grid) | 8 m patch | ~0.9-1.1 + 1.4-1.6 ms (MEASURED grid) | ~2 patches | ~25-35 ms/s |
| A1 repeat pass | patch rut layer | grid / 13 per cell (9x9 vs 33x33 verts) + fixed MeshObject / body cost | 1-3 layers | INFERENCE ~5-15 ms/s |
| A1 first pass | + patch flip | as Step 0 per flip (0 if pre-flipped) | | |
For scale: v035 voxel ruts ~20 ms/s (freeze-test.md), v097 displace 6-15 ms/s on top of brick builds.

## 6. What to test, in order
1. **Step 0 A/B (no code):** `m_bBandMesher 0` vs 1, same rutted drive, `MUD band:` / `grid path` lines + the cost
   split, fps. Check for losses: patch / skin seam cracks (DispHoles once it covers patches - review F3), groove
   look, VOX borders, kept-terrain edges, get in / out. If clean -> keep it off, and the band mesher can be archived.
2. Stress run (3-5 vehicles) with Step 0 - the review's Stage 0. If ms/s is fine, A is a look upgrade only
   (0.125 m), not a cost fix: park it.
3. Only then A1: prototype offline first (Tools/: patch + rut-layer meshes from the same base + floor, crack check
   on RUT / normal borders at 0.25 and 0.125 m, tri counts), then in game behind a switch.

## 7. Open questions
- Was the band mesher kept for anything else than heightfield craters (skirts at skin seams, kept-terrain cells,
  partial cells)? Check results/2026-10-13-sdf-phase1.md / phase2 and the band code comments before switching.
- Rut cell ring: 1 ring of 2 m cells per side is generous for a 0.5 m rut radius - a rule based on the floor
  support (+1 sample) would flip fewer cells but needs the border-NONE guarantee.
- How many patches does one rut op really touch (the ±1 vertex box crosses patch borders)? The Stage 0 split logs
  answer it.
