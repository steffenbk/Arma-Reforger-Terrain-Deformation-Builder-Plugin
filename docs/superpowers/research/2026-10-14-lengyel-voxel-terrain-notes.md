# Lengyel 2010, "Voxel-Based Terrain for Real-Time Virtual Simulations" - notes

Source: C:\Users\Steffen\Downloads\Lengyel-VoxelTerrain.pdf (PhD dissertation, UC Davis, 101 pages, CC BY-ND).
This is the Transvoxel paper (C4 Engine). Read in full 2026-10-14; equations checked on the rendered
pages 30, 46, 47, 57, 60. Nothing here is proven in Enfusion - it is a paper, not a measurement.

## What the paper covers

1. **Modified Marching Cubes (ch. 3).** Signed distance per voxel (8 bit, -127..127, + = air, - = solid;
   0 counts as air). Fixes the classic MC holes by always joining the two "inside" corners on an
   ambiguous face ("preferred polarity"). Result: 18 cases instead of 15, no hole ever, no extra cost.
2. **Fast build (3.3).** Blocks of 16x16x16 cells. A cell may only create vertices on its 3 "max" edges;
   everything else is reused from earlier cells (two decks of history). Vertex t on an edge is
   always computed with the LOWER-numbered corner first - otherwise neighbouring blocks get seams.
   Degenerate (zero-area) triangles from exact-zero samples are dropped.
3. **Storage.** Values clamped to a small range so solid and air are long runs; simple RLE on
   horizontal spans gave 33:1. To mesh one block you need 1 voxel before and 2 after each side
   (the extra ring is for normals by central differences).
4. **LOD (ch. 4).** Coarser blocks are 2x bigger, same cell count (octree). Two traps:
   - *Surface shift*: a coarse cell puts flat ground up to half a cell off. Fix: on an active coarse
     edge, walk down to the finest sub-edge that holds the sign change and place the vertex there.
   - *Cracks/holes between LODs*: "transition cells" - a thin slab (9 fine samples on one face, 4
     coarse on the other, 512 cases, 73 classes). The coarse block keeps 2 vertex positions and the
     shader picks one; the shift is projected onto the tangent plane (eq. 4.3) so it doesn't dent.
5. **Texturing (ch. 5).** Cubic (triplanar) projection with sign flips so nothing is mirrored (eq. 5.1).
   Blend weights `b = max(|N_axis| - d, 0)^m`, normalised; d = 0.5, m = 4 looked best (fig. 5.3).
   Per voxel: material id + blend value. Material per cell = the voxel at the cell's min corner
   (for coarse cells, the most common material among non-empty sub-cells). Material 255 = "poison":
   no triangles in that cell - punches a clean hole for a hand-made bunker/cave entrance.
6. **Editing (appendix A).** Add brush = max(existing, brush), dig brush = min, smooth brush = 3D blur
   of the distance values. Brush shapes: sphere, cylinder, slope.
7. **Future work.** Geomorphing, sharp edges (extra vertex inside the cell, like dual contouring),
   nicer transition cells, textures that follow a road curve.

## What it means for our voxel ground (MUD_VoxelProbe.c)

We use surface nets on 0.5 m cells, 8^3 sub-blocks, sparse 4 m columns, no LOD. So:

| Paper idea | Relevance for us |
|---|---|
| MC ambiguity fix | Not needed - surface nets has no ambiguous faces. Only matters if we ever switch to MC. |
| Consistent endpoint order / shared samples at block borders | Same rule we already follow (world-aligned samples, shared column at brick seams). Confirms the design. |
| Clamped distance + RLE | Directly useful if seed / JIP text gets big (SEED_CHARS 4000, JIP replays every op): clamp far-from-surface values, run-length the columns. |
| Normals by central differences (needs 1+2 voxel ring) | We can't pass normals - the engine rebuilds them from our triangles (mud-gotchas line 241). INFERENCE, not checked: normals at brick edges only see that brick's triangles, so a faint light seam along brick borders is possible. |
| LOD + transition cells | Not needed now: our bricks are small and local. Only if voxel areas ever get big enough to need far versions. |
| Surface-shift trap | Same reason our ruts look soft: 0.5 m cells can't draw a feature narrower than about a cell. Depth is fine (sub-cell vertex placement), width is not. A finer cell for rut-only bricks is the real fix, not a smarter mesher. |
| Triplanar blend (d 0.5, m 4) | We pick ONE projection per quad (hard switch, clamped 0..1 UVs). The formula is the target if an emat can blend by normal; open question for Enfusion. |
| Material per voxel, min-corner rule | Useful if we want mud vs. rock vs. dug soil in voxel walls: store a material byte per sample, pick per cell. |
| Poison material | Matches a need we have: "no ground here" cells for a placed bunker/tunnel prefab entrance without carving a big cave. |
| Smooth brush (3D blur) | Candidate for crater rims / fill ops and for softening rut edges without extra resolution. |
