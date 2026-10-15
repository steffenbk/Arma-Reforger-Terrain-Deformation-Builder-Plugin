# Displaced ruts - partial split check (2026-10-15, offline)

Tool: Tools/displace_partial.py (uses displace_proto.py / dc_vertex_proto.py). Follow-up to
docs/superpowers/research/2026-10-15-voxel-options-deep-dive.md section 3, which split EVERY triangle and blended rut
edges toward the true surface. Labels as there: MEASURED = this run.

- disp2_all reproduces displace_proto 'disp2' exactly (control).
- disp2_allv: rut edges blend to the vertex's own coarse y (all the game knows) instead of the true top. Same on the
  rut shapes; BETTER on rut_x_crater (true->mesh max 0.337 -> 0.270): the true-top blend was wrong inside craters.
- disp2_tri: split only "rutted" triangles (any of the 6 split points pulled). Geometry IDENTICAL to all-split
  (max height difference 0.000000 m on a 5 cm ray grid, scratch check); its worse rms is a sampling artefact
  (fewer mesh samples on flat ground). 0 cracks: a pulled edge midpoint is a split point of BOTH triangles, so both
  are rutted - no ring needed. But 50-95 T-junction edges per test area (render sparkle / Bullet internal-edge bumps).
- disp2_brick: split every triangle of a 4 m brick with any rutted triangle. 0 cracks, 0 T-junctions INSIDE a brick
  (the counted ones are on brick lines, where bricks are separate meshes / bodies anyway). ~3000-3600 tris for the
  12 m area vs 4448 all-split.
- Segment test u in [0, 1): only 2 floor samples (0.25 m) left uncut inside the arc rut, 0 on straight ones (the
  rest are the open rut ends). Minor; clamp u when the floor op is written.

| Shape | Method | Tris | Split / coarse | Slivers | mesh->true rms | max | true->mesh rms | max | Wall p90 deg | T-junction edges | Cracks |
|---|---|---|---|---|---|---|---|---|---|---|---|
| rut_straight | disp2_all | 4448 | 1112 / 1112 | 0 | 0.015 | 0.089 | 0.028 | 0.132 | 45 | 0 | 0 |
| rut_straight | disp2_allv | 4448 | 1112 / 1112 | 0 | 0.015 | 0.089 | 0.028 | 0.132 | 45 | 0 | 0 |
| rut_straight | disp2_tri | 2003 | 297 / 1112 | 0 | 0.019 | 0.089 | 0.037 | 0.132 | 45 | 95 | 0 |
| rut_straight | disp2_brick | 3347 | 745 / 1112 | 0 | 0.016 | 0.089 | 0.032 | 0.132 | 45 | 43 | 0 |
| rut_diag | disp2_all | 4448 | 1112 / 1112 | 0 | 0.014 | 0.101 | 0.025 | 0.144 | 45 | 0 | 0 |
| rut_diag | disp2_allv | 4448 | 1112 / 1112 | 0 | 0.014 | 0.101 | 0.025 | 0.144 | 45 | 0 | 0 |
| rut_diag | disp2_tri | 1607 | 165 / 1112 | 0 | 0.019 | 0.101 | 0.036 | 0.144 | 45 | 69 | 0 |
| rut_diag | disp2_brick | 3056 | 648 / 1112 | 0 | 0.015 | 0.101 | 0.031 | 0.144 | 45 | 85 | 0 |
| rut_arc | disp2_all | 4448 | 1112 / 1112 | 0 | 0.012 | 0.153 | 0.037 | 0.318 | 49 | 0 | 0 |
| rut_arc | disp2_allv | 4448 | 1112 / 1112 | 0 | 0.012 | 0.153 | 0.037 | 0.318 | 49 | 0 | 0 |
| rut_arc | disp2_tri | 1526 | 138 / 1112 | 0 | 0.016 | 0.153 | 0.045 | 0.318 | 50 | 50 | 0 |
| rut_arc | disp2_brick | 3659 | 849 / 1112 | 0 | 0.013 | 0.153 | 0.038 | 0.318 | 49 | 58 | 0 |
| rut_x_crater | disp2_all | 5824 | 1456 / 1456 | 0 | 0.044 | 0.218 | 0.064 | 0.337 | 49 | 0 | 0 |
| rut_x_crater | disp2_allv | 5824 | 1456 / 1456 | 1 | 0.043 | 0.216 | 0.059 | 0.270 | 54 | 0 | 0 |
| rut_x_crater | disp2_tri | 2158 | 234 / 1456 | 1 | 0.042 | 0.207 | 0.064 | 0.270 | 58 | 82 | 0 |
| rut_x_crater | disp2_brick | 4426 | 990 / 1456 | 1 | 0.043 | 0.216 | 0.061 | 0.270 | 54 | 40 | 0 |

| Shape (one rut each) | Capsule floor samples | Uncut by u in [0, 1) |
|---|---|---|
| rut_straight | 196 | 8 |
| rut_straight | 196 | 8 |
| rut_diag | 207 | 14 |
| rut_arc | 201 | 14 |
| rut_x_crater | 196 | 8 |
| rut_x_crater | 196 | 8 |
