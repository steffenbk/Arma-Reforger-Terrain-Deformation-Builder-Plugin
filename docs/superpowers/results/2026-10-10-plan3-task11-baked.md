# Plan 3 Task 11 - baked heights (2026-10-10, Exp 192624)

Skin vertex heights now load from a baked file instead of 1089 traces per holed chunk.
The per-cell centre trace stays (diagonal + game material).

## Fit (step 6)

Samples: 2000 `MUD bake sample` lines per machine, peer play 12:11
(server `~/Documents/My Games/server/logs/logs_2026-10-10_12-11-45`, client `PeerPlugin1/logs/logs_2026-10-10_12-11-47`).

```
client: fit samples 2000 orient [0, 0, 0, 0] a 0.031249999 b -204.781210 max residual 0.000720 m FIT OK
server: fit samples 2000 orient [0, 0, 0, 0] a 0.031249991 b -204.781148 max residual 0.000782 m FIT OK
```

Arland HGHT: world height = u16 / 32 - 204.781 m, x fastest, no flips, unsigned. Residual is at the
1 mm print rounding. `Data/Mud/mud_fit.json` is the client fit.

## Bake

```
baked 1697 blocks from 132 tiles, 7405724 bytes
```

7405724 = 16 + 1697 x (8 + 4 x 1089). 1697 = chunks with holes (skin line below).

## Opened path form

Game FileIO opens addon files by EXTENSION, not folder (probes 12:27 and 12:31, server and client identical):

| file | opens |
|---|---|
| Scripts/Game/Mud/MUD_CoarseSkin.c (with and without `$Testingplanvechmsh:`) | yes |
| Data/Mud/zprobe.c | yes |
| Configs/System/chimeraInputCommon.conf | yes |
| Prefabs/Mud/MudGround.et | yes |
| Data/Mud/mud_heights.bin (3 path forms) | no |
| zprobe.bin / .json / .txt in Scripts/Game/Mud and in Data/Mud | no |
| Data/Mud/mud_fit.json | no |
| worlds/Arland/Terrain/.Data/Terrain_100.ttile | no |
| Arlnd.ent (has .meta), addon.gproj | no |

So the binary file is named `Data/Mud/mud_heights.c`, opened as `$Testingplanvechmsh:Data/Mud/mud_heights.c`
(the `m_sBaked` default). A `.c` outside Scripts is not compiled (zprobe.c held JSON, compile fine).

## MUD coarse lines

Traced (12:18, file did not open = fallback path):
```
server: work 6123 ms (heights 3675, meshes 0, colliders 1345)
client: work 6229 ms (heights 3562, meshes 426, colliders 1248)
chunks from baked heights 0, traced (no baked block) 1697
check (build): 2000 hole cells, skin hit 2000, within 5 cm 2000, worst 0.000740051 m
```

Baked (12:32, server/client logs_2026-10-10_12-32-31):
```
server: baked heights 1697 blocks from '$Testingplanvechmsh:Data/Mud/mud_heights.c' (9 ms)
server: work 4836 ms (heights 2435, meshes 0, colliders 1299), wall 4847 ms
server: chunks from baked heights 1697, traced (no baked block) 0
server: check (build) 2000 / hit 2000 / within 5 cm 2000, worst 0.000648499 m; late same
client: baked heights 1697 blocks (8 ms)
client: work 5231 ms (heights 2507, meshes 285, colliders 1303), wall 5239 ms
client: chunks from baked heights 1697, traced (no baked block) 0
client: check (build) 2000 / 2000 / 2000, worst 0.000648499 m; late same
```

Checksum: baked 1697 + traced 0 = 1697 = chunks with holes.

## Gate

| | before | after |
|---|---|---|
| heights ms, server | 3675 (traced, same session); plan 2 doc 3594 | 2435 |
| heights ms, client | 3562 (traced, same session) | 2507 |
| traced chunks | 1697 | 0 |
| check within 5 cm | 2000 / 2000 | 2000 / 2000 |

PASS. Remaining heights time is the per-cell GetSurfaceY scan (4M cells) and the centre traces.
Check is now independent of the build (MUD_Surface.HeightmapY), default `m_iCheckCells` 0.

## Review fix run (12:37, server/client logs_2026-10-10_12-37-31 / 12-37-33)

```
baked heights 1697 blocks from '$Testingplanvechmsh:Data/Mud/mud_heights.c' (8 ms), spot check 66 vertices worst 0.000236511 m
server work 4776 ms (heights 2396); client work 5096 ms (heights 2424)
chunks from baked heights 1697, traced (no baked block) 0
```
66 spots = every 26th block (1697 / 64 = 26). TASK 11 DONE.
