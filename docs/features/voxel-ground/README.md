# Voxel ground (bricks)

**Status: SHIPPED.** Short page - add to it when we work on this feature.

## Files (Scripts/Game/Mud)
MUD_VoxelProbe.c, MUD_VoxelNet.c, MUD_BandMesher.c

## Docs
- [2026-10-13-voxel-bricks-design.md](../../superpowers/specs/2026-10-13-voxel-bricks-design.md)
- [2026-10-14-voxel-papers-plan.md](../../superpowers/plans/2026-10-14-voxel-papers-plan.md)
- [2026-10-14-ida-voxel-feasibility.md](../../superpowers/research/2026-10-14-ida-voxel-feasibility.md)

## Open
- Options after the freeze (displaced ruts, terrain-look material probes, DC for craters): docs/superpowers/research/2026-10-15-voxel-options-deep-dive.md
- Papers plan Step D (JIP lazy queues) left. Go-live frame for a big crater ~50-100 ms (LOG 2026-10-15).

## Details (moved from CLAUDE.md 2026-10-15, kept word for word)
- Voxel ground (TEMP probe, proven SP 2026-10-12): surface nets, 0.5 m cells, sub-blocks 8^3 (~1 ms rebuild), our heightfield sunk 4 m under the voxel area with a 0.75 m ring (none toward live neighbour tiles), DirtCover_01 walls. Crater test 2026-10-13: 64 m tiles on a world grid 63.5 m apart (shared cell column, world-aligned samples so seams match), seeded over frames. Script-mesh UVs are CLAMPED to 0..1 (KB runtime-procedural-mesh.md): give 0..1 per tile, pick one tile per quad, tiling in the emat. Details mud-gotchas.
- Voxel BRICKS (2026-10-13, spec docs/superpowers/specs/2026-10-13-voxel-bricks-design.md, replaces 64 m tiles): MUD_VoxelProbe.c, sparse 4 m columns (2x2 skin cells) converted per op + 1 ring, seeded from the server's dug-undug (cm, op text field 10), kept for the match; ops are all-voxel or all-heightfield (REFUSED -9 falls through). Go-live per op batch sets skin CELL_VOX + patch VOX (mesh + collider out) ONLY on carved cells: cells where an op moved the drawn surface (sign change or a near-surface sample > CARVE_TOL 5 cm); only those draw voxels, the rest keep the heightfield and its ruts. Ruts can't be pressed in VOX cells. Since 2026-10-13 20:42 SURFACE_OPS = false: only tunnels convert ground; craters / trenches / shovel go voxel only where a tunnel already converted it, else heightfield (user: merging both at crater size kept hiding ruts). 2026-10-13 22:35 ALL-VOXEL TEST (v031 = before): SURFACE_OPS = true + RUTS = true - every op goes voxel and wheels cut voxel ruts (op 4, MUD_VoxelProbe.RutWheel / RutFlush, one op per vehicle per 250 ms); the heightfield takes no rut while the probe is up. BEST so far (v035, user 2026-10-13 23:28 "by far the smoothest, no lag", 0 corrections / 90 s): RUT_BEHIND (cut once the rearmost wheel is 3 m past, so the ground under the tyres never changes), RUT_AHEAD off; swapped-out brick entities are buried (hidden, layer 0) and deleted 1 s later - deleting at once crashed a client in VehicleDustSystem.
- Voxel papers plan (2026-10-14, docs/superpowers/plans/2026-10-14-voxel-papers-plan.md). Step A done (v040): no light seams at 4 m brick lines (user), no doubled seam quads, JIP = 96 % ruts (kind 4 + 6 plan text), seeds 0 in all-voxel, join 839 ops in 6.6 s (docs/superpowers/results/2026-10-14-jip-size.md); no triplanar emat shipped (docs/superpowers/results/2026-10-14-material-search.md). Engine limits for meshes (own normals / UV2 / colours impossible, trimesh keeps degenerate tris): docs/superpowers/research/2026-10-14-ida-voxel-feasibility.md. Later steps (all 2026-10-14): B1 dual contouring NO-GO for ruts (docs/superpowers/results/2026-10-14-dc-proto.md); E 0.25 m cells PARKED (~3.4x cost; Tools/voxel_size.py coarse|fine flips it, probe stays coarse); band scan KEPT (-27 % poly, exact; docs/superpowers/results/2026-10-14-cell-size.md); C smoothing PARKED (no visible gain, SMOOTH_OPS false); G engine probes done (docs/superpowers/results/2026-10-14-engine-probes.md: same emat on 2 sections crashes, collider patching = rebuild cost, no visible seam to fix). Only Step D (JIP lazy queues) left.
