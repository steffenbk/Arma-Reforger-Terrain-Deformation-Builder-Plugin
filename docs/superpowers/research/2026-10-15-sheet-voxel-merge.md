# Mud sheet + voxel ground: making them look like one ground (2026-10-15)

User: "it's layered like a cake" - digging under the sheet drags its mesh down and looks bad, and craters show the
height step between the two layers. Labels: CODE (read in our script today), LOG, USER, INFERENCE.

## Why it looks like two layers (code check)
1. **Hard step at every mud edge.** CODE MudSheet.BuildTile: a drawn vertex sits at ground + LIFT + mud depth
   (up to 0.15 m + ridges). A blown-off (Bare) vertex jumps to ground - SINK (0.06 m). So the mud ends with a
   ~0.2 m drop over ONE 0.15 m cell - a little wall all round every crater / dig. Only the open tile sides fade
   gently (EDGE 1 m).
2. **Mesh dragged into holes.** CODE: after a dig the sheet re-reads the ground; cells that moved > MOVED 0.08 m
   lose their mud (Bare) but keep the NEW ground height - the hole bottom. A quad with one corner on the rim and one
   in the hole is still drawn, so it stretches from the mud top down into the hole (the "drags the mesh down" look).
3. **Same texture, different size.** CODE: both use DirtCover_01.emat, but the sheet maps 0..1 per 12 m
   (MudSheet SIZE) and the voxel soil per 8 m (VoxelProbe SOIL_TILE). The grain is 1.5x bigger on the sheet and never
   lines up, so the edge reads as a material change even where the heights match.
4. **Different resolution.** CODE: voxel cells 0.5 m, sheet cells 0.15 m, sheet ground read every 2nd cell
   (0.3 m) and filled between. Round shapes (crater rim) don't match exactly - small, but it adds to 1.
5. The voxel rim of a crater is raised (rimH = 0.2 x depth); since RIM_BARE the sheet stops at the rim's outer foot.

## Ways to merge them
| | Idea | Fixes | MP | Cost |
|---|---|---|---|---|
| A | **Same texture size**: voxel SOIL_TILE 8 -> 12 (both world-aligned at 12 m, so the grain lines up on flat ground) | 3 | none (look only) | one constant; side walls also go to 12 m - check they don't look too blurry |
| B | **Taper, no step**: mud thins smoothly to 0 over ~0.6 m toward any Bare cell (as EDGE does at tile sides), so mud and ground meet at the same height | 1, 5 | client-only | small: a distance-to-bare pass per tile on rebuild |
| C | **Don't follow the hole**: a Bare cell whose ground dropped (dig / crater) keeps its OLD ground height for the mesh, and any quad with a corner more than ~0.1 m below its neighbours is not drawn - the voxel wall takes over right at the rim | 2 | client-only | small |
| D | **Cut edge as a "topsoil" lip**: instead of hiding the edge, draw a short dirt skirt from the mud top down ~0.2 m into the voxel wall - a real trench shows a topsoil band | 1 (looks intended) | client-only | medium; needs B or C first |
| E | **One surface in zones**: hide voxel top quads that the sheet covers (as we already did for grass), voxels draw only where the mud is gone | z-fighting / double surfaces | client-only | small; INFERENCE: little gain after B + C |
| F | **Mud inside the voxels** (client-only fine voxel layer for the soft mud) | everything, true one ground | client-only but heavy | big; 0.25 m cells were ~3.4x cost (papers plan E, parked) |

## Recommendation
A + B + C together - all client-only, no net change, MP-safe. They attack the three things you see: the texture
change (A), the step (B), the stretch into holes (C). Then look again; D only if the cut edge still looks plain.
F stays parked (feature freeze, cost).

## Built (2026-10-15 ~13:45, not compiled yet, checkpoint v070 = before)
- A: MUD_VoxelProbe SOIL_TILE 8 -> 12; sheet v flipped (1 - v) like the voxel UVs, so the grain lines up.
- B: MudSheet TAPER 0.6 m (chamfer distance to the nearest Bare vertex inside the tile, fades like EDGE).
- C: MudSheet HOLE_SKIP 0.2 m (quad with a Bare corner and > 0.2 m height spread is not drawn).
- Vertex heights now come from one pre-pass (vy[]) in BuildTile.
