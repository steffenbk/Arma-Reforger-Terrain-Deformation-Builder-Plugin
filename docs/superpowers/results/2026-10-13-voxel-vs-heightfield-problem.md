# Voxels vs. heightfield: the problem and what we tried (2026-10-13)

Status: the problem is NOT solved. Written at the user's request after test 5 (20:48), which the user called "even worse".
Game: Arma Reforger Experimental 192624, Enforce Script. Mod root `addons/Testing plan vech msh`, scripts in
`Scripts/Game/Mud`. Logs: `My Games/ArmaReforgerWorkbench/logs/logs_2026-10-13_19-21-15/console.log` (all runs below).
Proven facts and timings are also in `docs/superpowers/research/mud-gotchas.md`.

## 1. What we want

Deformable ground over the whole of Arland, working in MP and for late joiners:
- vehicle ruts (fine, 0.25 m detail, pressed all the time);
- mortar craters;
- trenches and shovel holes;
- tunnels / overhangs (a heightfield cannot do these).

All of them must show together in the same place: ruts running into and through a crater, a tunnel dug into a crater
wall, and so on.

## 2. The two systems we have

| | Heightfield (old, proven) | Voxels (new, TEMP probe) |
|---|---|---|
| Files | `MUD_WorldGround.c`, `MUD_FinePatch.c`, `MUD_CoarseSkin.c` | `MUD_VoxelProbe.c` |
| Detail | 0.25 m vertices (fine patches), 2 m skin cells | 0.5 m samples, surface nets, 4 m bricks |
| Does | ruts, berms, craters, trenches, shovel, fill, rim plow | craters, tunnels, trenches, fill |
| Can't do | overhangs, tunnels | ruts (0.5 m is too coarse for a ~0.3 m rut) |
| MP | changed rectangles sent as int16 mm | ops sent in seq order + seed text |

## 3. The core problem

Both systems draw a ground surface. They can't both draw in the same spot (double surface, or one floats over the
other), so every 2 m skin cell must belong to ONE of them:
- a cell given to voxels (VOX) hides the heightfield there, mesh and collider. Every rut in that cell disappears, and
  ruts pressed there later go into the hidden heightfield, so they can't be seen ("I can't make any on it").
- a cell kept on the heightfield can't show anything a voxel op did there.

So the voxel area around every crater is a hole in the rut system. The handover is at 2 m cell steps, and a
crater with its raised rim is ~9 m across (crest 3.14 m + rim 1.43 m, mortar depth 1.427), so the voxel patch is
always much bigger than the dirt disc the player sees.

## 4. What we tried (all 2026-10-13, SP Arland unless noted)

| # | Time | What | Result (log / user) |
|---|---|---|---|
| 0 | 00:07-01:41 | Voxel GROUND mode: 64 m tiles, heightfield sunk 4 m under them | Craters looked "much better"; tunnels worked. But tiles stream in near the camera ("pops in"), fine patches off. |
| 1 | 19:39 | OVERLAY mode: 64 m tiles kept for the match, heightfield cells under them hidden locally | A crater on fresh ground took 1087-1299 ms to show. Ruts vanished: the tile seeded from undug heights. |
| 2 | 20:22 | BRICKS (spec `specs/2026-10-13-voxel-bricks-design.md`): only 4 m columns the op touches + 1 ring, seeded from the server's dug-undug | Fast: crater live 101-163 ms, tunnel dig 4-8 ms. Bug: a dirt sheet (the old patch surface with ruts) lay over the crater. |
| 3 | 20:30 | Fix: changed patches rebuild their mesh in the go-live frame (asleep patches never did) | Sheet gone. User: "it still removed the ruts" - the whole converted area (columns + ring, ~20 m) went VOX. |
| 4 | 20:36 | Carved cells only: only cells where an op changed samples go VOX | Log: 1 mortar = 40 cells (~160 m^2). Ruts stop several m before the crater (user screenshot). The crater funnel also changes air samples out to ~6 m. |
| 5 | 20:40 | Only cells where the drawn surface moves > 5 cm (`CARVE_TOL`) | Log: 29 cells (~116 m^2). User: "a little better but again not good". |
| 6 | 20:48 | `SURFACE_OPS = false`: craters / trenches / shovel back on the heightfield; voxels only for tunnels | Log: 11 tunnel digs, live 3-24 ms, 6 + 2 + 2 + 1 + 1 cells VOX. User: "even worse" (no details yet). |

## 5. Where the code is now

- `MUD_VoxelProbe.c`: `SURFACE_OPS = false`, `CARVE_TOL = 0.05`. Craters, trenches and shovel digs go to the heightfield,
  unless they land in columns a tunnel already converted. Tunnels convert ground to voxels.
- Known gap: a heightfield crater whose edge reaches a tunnel's VOX cells - that part is hidden.
- Pre-existing, unrelated: "MUD trench test: FAIL unpack should refuse '1,9;0,0,0;100,0,0'" fails in every run.

Checkpoints (`mud_backups/checkpoint.ps1 restore N`):

| Version | State |
|---|---|
| v010 | before any voxel work (heightfield only) |
| v019 | voxel GROUND mode, user-tested |
| v025 | before bricks (OVERLAY tiles) |
| v026 | bricks, whole converted area VOX |
| v027 | carved cells (any changed sample) |
| v028 | carved cells with `CARVE_TOL` (test 5) |
| current | `SURFACE_OPS = false` (test 6) |

## 6. Open questions

1. What exactly was worse in test 6? Did the heightfield craters look bad, did the tunnel look wrong, or both?
2. What did the heightfield crater lack that made us move craters to voxels? (Not written down anywhere we found.)
   If it was only the look, that may be cheaper to fix than merging two systems.
3. Is the rut-crater merge a must-have for tunnels too, or only for craters?

## 7. Possible directions (none tested)

- **A. Heightfield owns the surface, voxels only underneath.** Voxels never draw the top surface; the heightfield only opens
  a small hole at a tunnel mouth. Ruts stay everywhere except the mouth.
- **B. Ruts as voxel ops in VOX cells.** Needs finer voxels (0.25 m = 8x the samples and memory) to look like ruts.
- **C. Voxel crater drawn ON TOP of the heightfield without hiding it.** Only the bowl draws (the cells below the
  heightfield), the rim and the ruts stay heightfield. Needs the two surfaces to meet cleanly at the bowl edge.
- **D. User's idea from day 1: a deformed plane with no collider** for the look, the heightfield for physics.
- **E. Heightfield only** (back to v010-style craters), and drop tunnels or do them some other way.
