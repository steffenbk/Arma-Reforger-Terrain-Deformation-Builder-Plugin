# Voxel papers, batch 2 - notes (read in full 2026-10-14)

Companion to 2026-10-14-lengyel-voxel-terrain-notes.md. Papers only - nothing here is proven in Enfusion.

## 1. van Ruschen et al. 2018, "Volumetric Terrain Rendering with WebGL" (EI 2018, 14 p.)
Open-source JS engine (github vanruesc/rabbit-hole): Dual Contouring (DC) + QEF solver, octree, CSG edits, phone timings.

- **Hermite data**: per grid point a material byte (air / solid); per edge with a sign change the crossing t
  (0..1) and the surface normal there. Crossing found by bisection on the SDF (max 8 steps, 1e-6 interval,
  1e-2 bias). Normal by finite difference or the analytic SDF gradient. Only ONE crossing per edge - detail
  smaller than a cell is lost.
- **DC**: one vertex per cell = the point closest (least squares, QEF) to all the planes (crossing + normal) of
  that cell's active edges. Cells with only 1-2 planes are underdetermined; if the QEF answer lands OUTSIDE the
  cell they fall back to the "mass point" = average of the crossings (= plain surface nets). Vertex normal =
  average of the crossing normals. Cells around each active edge (4) are joined into a quad.
- DC weaknesses named: meshes are "rarely intersection-free" and can be non-manifold (fixes: Manifold DC, Dual
  MC, Cubical Marching Squares - all costlier).
- **CSG on SDFs**: union = min(f,g), difference = max(f,-g), intersection = max(f,g), negation = -f. Edits only
  touch grid points inside the shape's AABB (intersection is the exception - it touches everything).
  Merging stored Hermite data needs care: union keeps the crossing nearer the air point, difference the one
  nearer the solid point, and flips the normals of edges taken from the cutter - else volume grows/shrinks.
- **Lazy edits**: every world chunk keeps its OWN queue of pending CSG ops and only runs them when the chunk is
  near the viewer.
- **Storage**: RLE on the material array, 64^3 cell: 274625 bytes -> 9021 runs (6:1, 83.6 % saved). Edge data
  only 2.65 % of the possible edges are used (21528 of 811200).
- **Seams between chunks**: either overlap each chunk by one voxel in + directions (cheap, but the overlapping
  voxels must NOT be joined to each other or polygons are doubled), or build a separate seam mesh (needed for LOD).
  They shipped without a full gap fix.
- **Phone timings** (Nexus 5X, JS): res 32: CSG 32 ms, octree build 144 ms, DC 29 ms (5184 verts / 10400 tris);
  res 64: CSG 175 ms, octree 652 ms, DC 135 ms. Octree (SVO) building was the slowest part.

## 2. Sanford 2015, "Dynamic Voxel Based Terrain Generation" (student report, 9 p.)
Octree of chunks, Marching Cubes in a GPU geometry shader so edits never stall the CPU.
- Add only changes voxels outside the surface, remove only inside (= max / min).
- Triplanar: colour = nz^2 * tex(x,y) + nx^2 * tex(y,z) + ny^2 * tex(x,z) (squared normal weights, simpler than
  Lengyel's).
- Limits stated: FLAT shading because each cell's triangles don't know their neighbours, so no smooth normals;
  no shadow maps (mesh rebuilt every frame).

## 3. Fang, Wang, Wang 2025, "Aokana: GPU-Driven Voxel Rendering for Open World Games" (ACM PACMCGIT, 17 p.)
Blocky (Minecraft-style) voxels drawn by ray marching compressed trees (SVDAG) in Unity compute shaders.
- World cut into chunks of 256^3; many SHALLOW trees beat one deep tree (fewer pointer jumps, better cache).
- LOD: 8 voxels -> 1 when at least 2 are solid, average colour; chunk LOD picked by distance; only ~5 % of the
  scene in VRAM; 64K-res scenes (~10 billion voxels) in ~6 ms on an RTX 3060 Ti.
- No runtime edits; for editing they suggest a dense array near the player (meshed) and the compressed tree
  further out, rebuilt in idle time.

## What it means for MUD_VoxelProbe (surface nets, 0.5 m cells, sample-based distances)

| Idea | For us |
|---|---|
| DC vertex (QEF) with surface-nets fallback | The concrete upgrade path for sharper rut walls / crater lips: keep today's average as the fallback when the QEF point leaves the cell. Our ops are analytic shapes (capsule, crater, rut) so exact normals are available, or use differences between samples. Limit: still ONE vertex per cell, so a rut wall AND a berm top in the same 0.5 m cell can't both be sharp. Risk: self-intersecting triangles in the collider. |
| min / max CSG on samples | We store distance samples, not Hermite edges, so plain min/max already gives the right merge - their edge-merge rules are not needed. |
| Lazy per-chunk op queues | Most useful MP idea here: a joining client could apply only ops near itself first and queue the rest per brick, instead of replaying every op at once within JOIN_MS. Not designed yet. |
| RLE | Second paper showing big wins (6:1 here, 33:1 in Lengyel) - supports the seed/JIP compression idea. |
| Overlap-one-voxel seams, don't join the overlap | Same idea as our shared sample column at brick seams; the "don't double the overlap polygons" trap is worth checking in our brick builder. |
| Flat shading from missing neighbours (Sanford) | Same mechanism as the brick-edge normal guess in the Lengyel notes, in its extreme form. |
| Aokana | Not usable: needs custom render passes / compute shaders, blocky voxels, no edits. Only takeaway: many small flat chunks over one deep tree - which our brick map already is. |
