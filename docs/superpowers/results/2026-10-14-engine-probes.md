# Step G engine probes (2026-10-14, Exp 192624, WB SP, Arland)

Probe: Scripts/Game/Mud/MUD_EngineProbes.c (TEMP). Log: ArmaReforgerWorkbench/logs/logs_2026-10-14_19-39-10.

## G2 - same material on two sections: CRASH (proven)

| step | material 0 | material 1 | result |
|---|---|---|---|
| control (20:18:33) | default_MatTerrainObj1.emat | DirtCover_01.emat | Create, both section updates, SetObject, second update ok; "survived 5 s" at 20:18:38 |
| same (20:18:40.067) | DirtCover_01.emat | DirtCover_01.emat | Create, UpdateIndices/UpdateVerts on both sections and SetObject all returned ("SetObject ok" 20:18:40.067) |
| | | | 20:18:40.070 ENGINE (F): Crashed - access violation, illegal read at 0xffffffffffffffff (crash.log) |

Script calls do not fail; the crash comes 3 ms later, after SetObject (inference: on the first draw/render
update of the mesh). Matches the IDA finding (Create de-dups materials by name) and the 2026-10-10 skirt crash.
Rule: every section of a MeshObject needs a different .emat.

## G1 - brick-edge light fix

Built (run 2, 20:20:00): OFF / ON / WHOLE domes 8 x 4 m, 1.2 m high, split on the ridge (ON = 48 tiny
triangles). User, side view through binoculars, daytime sun, 2026-10-14: "they kinda look the same" - no crease
visible even on OFF. Result: no visible light seam to fix with this material (default_MatTerrainObj1 takes its
colour from the terrain under it - user confirmed - which may hide shading). Our voxel tops use this same
material, so the result holds for tops; dirt walls (DirtCover_01) were not tested. Agrees with A1 (no seams at brick lines).
G1 not taken into the mesher.

## G3 - collider patch vs rebuild: WORKS, NO SPEED GAIN

Run 2 (logs_2026-10-14_20-19-11, 20:20:20), 4 x 4 m flat trimesh grids, K = 200 per method, each new piece 1 cm
lower than the last (so a downward trace hits the newest piece only if the old ones are really off).

| grid | tris | fill only (K) | full rebuild (K) | per rebuild | patch (K) | per patch | patch first 50 / last 50 | AddGeom failed |
|---|---|---|---|---|---|---|---|---|
| 8 | 128 | 2 ms | 6 ms | 0.03 ms | 6 ms | 0.03 ms | 2 / 2 ms | 0 |
| 32 | 2048 | 37 ms | 93 ms | 0.465 ms | 98 ms | 0.49 ms | 25 / 24 ms | 0 |

Traces (world y, want in brackets): first body -12.0904 (-12.0904); after rebuilds -14.0804 (-14.0804); after
patches -14.0904 (-14.0904) on both grids. So SetGeomInteractionLayer(old, 0) really takes old pieces out of
traces, AddGeom returns rising indices (last 200), and 200 dead pieces on one body do not slow the next patch.
Both times include the grid fill (fill only: 0.01 / 0.185 ms each); net 0.02 vs 0.02 ms (grid 8) and 0.28 vs
0.305 ms (grid 32). Patching costs the same as a full rebuild: the cost is CreateTriMesh, not CreateStaticEx.
No reason to switch the bricks to patching; dead pieces would also pile up memory.
