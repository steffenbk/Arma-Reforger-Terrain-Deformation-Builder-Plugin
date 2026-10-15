# Gate 2 - whole-Arland coarse skin cost (plan 1, task 2)

Date 2026-10-08, Arma Reforger Experimental 192624 (1.9.0.10), Workbench play, world `Arlnd.ent`, probe
`MUD_WorldSkin.c` (64 m chunks = 32 x 32 cells at 2 m, WORLD-trace heights, one extra trace per cell for the
diagonal, terrain left on, skin +1 mm, 8 chunks per frame). Logs: `skin-smoke.log`, `skin-full-render.log`,
`skin-full-collider.log`.

## Runs

| Run | Time | Chunks | Verts | Tris | Frames | Wall ms | Worst frame ms | Heights ms | Meshes ms | Colliders ms | Mem after build KB | Mem +12 s KB | Frame ms before -> after | Diag A / B / tie | Trace misses |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Smoke, r 200 m, render | 23:11 | 47 | 51183 | 96256 | 6 | 227 | 23 | 97 | 11 | 17 | +6710 | +7415 | 16.28 -> 14.46 | 0 / 18484 / 29644 | 0 |
| Full, render + collider | 23:13 | 4096 | 4460544 | 8388608 | 512 | 22292 | 29 | 8270 | 1128 | 1576 | +586979 | +603133 | 16.99 -> 17.88 | 1861 / 2492933 / 1699510 | 4223 |
| Full, collider only | 23:14 | 4096 | 4460544 | 8388608 | 512 | 20325 | 25 | 8231 | 0 | 1957 | +392969 | +404527 | 15.91 -> 16.54 | 1861 / 2492933 / 1699510 | 4223 |

Checksums (evaluated): 4096 x 1089 = 4460544 verts, 4096 x 2048 = 8388608 tris, 47 x 1089 = 51183, 47 x 2048 = 96256.
Diagonal cells 1861 + 2492933 + 1699510 = 4194304 = 4096 x 1024 (every cell counted once).
Script work per full run = heights + meshes + colliders = 10974 ms (render) / 10188 ms (collider only); the rest of
the wall time is ordinary frames (512 frames at 8 chunks per frame).
Task Manager readings: not taken.

## Findings

- Heights dominate (8.2 s of ~10-11 s): 4096 x 2113 = 8.65 M WORLD traces, ~0.95 us each.
- Memory: collider ~393 MB, meshes ~194 MB more (587 - 393).
- Frame cost after build: +0.9 ms (render), +0.6 ms (collider only) vs the 10 s before.
- Diagonal: 99.93 % of non-flat cells use B (shared edge tl-br = (x, z+1)-(x+1, z)); 1861 cells (0.07 %) picked A.
  Unresolved whether those are real or the 1 mm tie threshold - not investigated.
- Trace misses 4223 ~= 2 x 64 x 33 = 4224 vertices on the far map edges x = 4096 / z = 4096 (inference: a trace
  exactly on the terrain's far edge misses; GetSurfaceY fallback was used there). Not proven.
- `MemoryAllocationKB` at the start of the collider run was 2457034 KB vs 2068447 KB before the render run: ~389 MB
  was not given back after the previous play stopped (unresolved: leak in the probe, or Workbench caching).

## Decision rule (proposed in the plan, user to confirm)

| Rule | Limit | Measured (collider only) | Pass |
|---|---|---|---|
| Wall time | <= 15 s | 20.3 s (work 10.2 s) | no |
| Memory | <= 600 MB | 393 MB | yes |
| Frame time after | <= baseline + 1 ms | +0.63 ms | yes |

Per the rule as written: Plan 2 uses a 4 m collider beyond ~500 m of any player. Notes for the call: wall time is
set by chunks-per-frame, not by work (10.2 s); a fixed B diagonal would drop the mid-cell trace (2113 -> 1089 traces
per chunk, heights ~8.2 s -> ~4.2 s, estimate) and bring work to ~6 s.
Diagonal rule from the plan ("drop the mid trace if A or B is 0") is NOT met: A = 1861.
