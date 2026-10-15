# Plan 2 Task 2 - Arland keep mask, holes written, census verified

Date: 2026-10-08 23:44 - 2026-10-09 00:00. Build: Arma Reforger Experimental 192624 (Workbench port 5776).
Log folder: `logs/logs_2026-10-08_23-08-28`.

## Mask scan (MUD_HoleMask.c)

Mask entity on a tarmac road at x 1420.572 y 38.572 z 2185.829 (placed by the user).

First run 23:44 (plan code) FAILED: `MUD mask control x 1420.57 z 2185.83 road 0 water 0`, plus 1524 NULL pointer
errors (FileHandle stored without `ref`). Probe 23:48 at the same point:

| trace | flags | mask | frac | hit y | TraceEnt |
|---|---|---|---|---|---|
| ents-proj | ENTS | Projectile | 1 | - | none |
| ents-proj-roadonly | ENTS | Projectile | 1 | - | none |
| ents-all | ENTS | -1 | 1 | - | none |
| ents-all-roadonly | ENTS | -1 | 1 | - | none |
| ents-terrainlayer | ENTS | Terrain | 1 | - | none |
| world-proj | WORLD | Projectile | 0.4303 | 38.5722 | RoadEntity |
| ents+world-all | ENTS+WORLD | -1 | 0.4303 | 38.5722 | RoadEntity |

Ground there 38.5723. AABB query of 6 m around the point: 2 Tree, 2 SCR_DestructibleEntity, the mask entity, 0 RoadEntity.
=> Roads are WORLD geometry. Road test changed to WORLD trace + RoadOnly filter + `TraceEnt.Type() == RoadEntity`.

Water probe (TryGetWaterSurfaceSimple / full, type 1 = ocean):

| x | z | ground Y | simple | full | surface Y |
|---|---|---|---|---|---|
| 1420.57 | 2185.83 | 38.57 | 0 | 0 | - |
| 1420.57 | 50 | -116.08 | 1 | 1 | -0.19 |
| 1420.57 | 300 | -105.52 | 1 | 1 | -0.06 |
| 1420.57 | 500 | -68.98 | 1 | 1 | -0.21 |
| 2000 | 300 | -66.34 | 1 | 1 | -0.15 |
| 2000 | 2000 | 29.81 | 0 | 0 | - |
| 1 | 1 | -144.53 | 1 | 1 | -0.15 |

Second run 23:49:58 - 23:50:15:
```
MUD mask control x 1420.57 z 2185.83 road 1 water 0
MUD mask box 0 0 cells 2048 2048 cell 2
MUD mask done keep 2724878 runs 19252 road 115440 water 2609474 box 0 ms 9996
```
Gate PASS (control road 1, road > 0, water > 0). Mask: `2026-10-08-mask.txt`, picture `2026-10-08-mask.png`
(user: looks right).

`mud_holes.py plan --margin 1`:
`grid 2048 x 2048 = 4194304 cells, margin 1: keep 2797926 + holes 1396378 = 4194304 (ok), 1697 blocks in 132 tiles`

## Tile write

First write refused by the round-trip guard (nothing written): 28 vanilla tiles have an odd-length chunk (GCTD 43 /
289 bytes) with NO IFF pad byte; `ttile_holes.py` padded. Fixed (+ test), all 256 Exp pak tiles round-trip.
Second write, per-tile table (tile, blocks, hole cells):

```
Terrain_39.ttile blocks 1 hole cells 98
Terrain_40.ttile blocks 6 hole cells 2835
Terrain_41.ttile blocks 7 hole cells 3798
Terrain_42.ttile blocks 1 hole cells 187
Terrain_53.ttile blocks 6 hole cells 4108
Terrain_54.ttile blocks 15 hole cells 11445
Terrain_55.ttile blocks 16 hole cells 14465
Terrain_56.ttile blocks 16 hole cells 15006
Terrain_57.ttile blocks 16 hole cells 15723
Terrain_58.ttile blocks 15 hole cells 12546
Terrain_59.ttile blocks 4 hole cells 1699
Terrain_68.ttile blocks 8 hole cells 1679
Terrain_69.ttile blocks 15 hole cells 11569
Terrain_70.ttile blocks 16 hole cells 14868
Terrain_71.ttile blocks 16 hole cells 14842
Terrain_72.ttile blocks 16 hole cells 14415
Terrain_73.ttile blocks 16 hole cells 16097
Terrain_74.ttile blocks 16 hole cells 15892
Terrain_75.ttile blocks 16 hole cells 13401
Terrain_76.ttile blocks 10 hole cells 6481
Terrain_77.ttile blocks 2 hole cells 928
Terrain_83.ttile blocks 15 hole cells 8730
Terrain_86.ttile blocks 6 hole cells 3236
Terrain_87.ttile blocks 16 hole cells 13603
Terrain_88.ttile blocks 16 hole cells 16301
Terrain_89.ttile blocks 16 hole cells 15365
Terrain_90.ttile blocks 16 hole cells 15295
Terrain_91.ttile blocks 16 hole cells 15868
Terrain_92.ttile blocks 16 hole cells 15479
Terrain_93.ttile blocks 16 hole cells 12642
Terrain_94.ttile blocks 1 hole cells 2
Terrain_99.ttile blocks 3 hole cells 1590
Terrain_100.ttile blocks 4 hole cells 886
Terrain_101.ttile blocks 3 hole cells 747
Terrain_102.ttile blocks 1 hole cells 187
Terrain_103.ttile blocks 11 hole cells 6664
Terrain_104.ttile blocks 16 hole cells 15317
Terrain_105.ttile blocks 16 hole cells 14407
Terrain_106.ttile blocks 16 hole cells 14007
Terrain_107.ttile blocks 16 hole cells 13261
Terrain_108.ttile blocks 16 hole cells 15770
Terrain_109.ttile blocks 16 hole cells 13941
Terrain_110.ttile blocks 2 hole cells 91
Terrain_115.ttile blocks 13 hole cells 8688
Terrain_116.ttile blocks 16 hole cells 15147
Terrain_117.ttile blocks 16 hole cells 13912
Terrain_118.ttile blocks 14 hole cells 10729
Terrain_119.ttile blocks 16 hole cells 14950
Terrain_120.ttile blocks 16 hole cells 15297
Terrain_121.ttile blocks 16 hole cells 14604
Terrain_122.ttile blocks 16 hole cells 13982
Terrain_123.ttile blocks 16 hole cells 14773
Terrain_124.ttile blocks 16 hole cells 14715
Terrain_125.ttile blocks 14 hole cells 9572
Terrain_129.ttile blocks 7 hole cells 1931
Terrain_130.ttile blocks 7 hole cells 3861
Terrain_131.ttile blocks 16 hole cells 14498
Terrain_132.ttile blocks 16 hole cells 12939
Terrain_133.ttile blocks 16 hole cells 10863
Terrain_134.ttile blocks 16 hole cells 14317
Terrain_135.ttile blocks 16 hole cells 15082
Terrain_136.ttile blocks 16 hole cells 15013
Terrain_137.ttile blocks 16 hole cells 15822
Terrain_138.ttile blocks 16 hole cells 14732
Terrain_139.ttile blocks 16 hole cells 14266
Terrain_140.ttile blocks 16 hole cells 14731
Terrain_141.ttile blocks 9 hole cells 6400
Terrain_145.ttile blocks 16 hole cells 13976
Terrain_146.ttile blocks 16 hole cells 12932
Terrain_147.ttile blocks 16 hole cells 13682
Terrain_148.ttile blocks 16 hole cells 10046
Terrain_149.ttile blocks 16 hole cells 13544
Terrain_150.ttile blocks 16 hole cells 15982
Terrain_151.ttile blocks 16 hole cells 15668
Terrain_152.ttile blocks 16 hole cells 15217
Terrain_153.ttile blocks 16 hole cells 14878
Terrain_154.ttile blocks 16 hole cells 14786
Terrain_155.ttile blocks 16 hole cells 14099
Terrain_156.ttile blocks 16 hole cells 15200
Terrain_157.ttile blocks 15 hole cells 13183
Terrain_158.ttile blocks 3 hole cells 1576
Terrain_161.ttile blocks 11 hole cells 6960
Terrain_162.ttile blocks 16 hole cells 11178
Terrain_163.ttile blocks 16 hole cells 8308
Terrain_164.ttile blocks 16 hole cells 9659
Terrain_165.ttile blocks 16 hole cells 11145
Terrain_166.ttile blocks 16 hole cells 13266
Terrain_167.ttile blocks 16 hole cells 14189
Terrain_168.ttile blocks 16 hole cells 15097
Terrain_169.ttile blocks 16 hole cells 15175
Terrain_170.ttile blocks 16 hole cells 14349
Terrain_171.ttile blocks 16 hole cells 14051
Terrain_172.ttile blocks 16 hole cells 12861
Terrain_173.ttile blocks 16 hole cells 14088
Terrain_174.ttile blocks 14 hole cells 10945
Terrain_177.ttile blocks 3 hole cells 1733
Terrain_178.ttile blocks 13 hole cells 10678
Terrain_179.ttile blocks 16 hole cells 13754
Terrain_180.ttile blocks 16 hole cells 13010
Terrain_181.ttile blocks 16 hole cells 11080
Terrain_182.ttile blocks 16 hole cells 9815
Terrain_183.ttile blocks 16 hole cells 14172
Terrain_184.ttile blocks 16 hole cells 14073
Terrain_185.ttile blocks 16 hole cells 14216
Terrain_186.ttile blocks 16 hole cells 15256
Terrain_187.ttile blocks 16 hole cells 14912
Terrain_188.ttile blocks 14 hole cells 9870
Terrain_189.ttile blocks 4 hole cells 2108
Terrain_190.ttile blocks 2 hole cells 362
Terrain_195.ttile blocks 5 hole cells 2664
Terrain_196.ttile blocks 14 hole cells 12414
Terrain_197.ttile blocks 16 hole cells 15011
Terrain_198.ttile blocks 16 hole cells 13443
Terrain_199.ttile blocks 16 hole cells 15232
Terrain_200.ttile blocks 16 hole cells 14502
Terrain_201.ttile blocks 16 hole cells 12049
Terrain_202.ttile blocks 16 hole cells 12921
Terrain_203.ttile blocks 16 hole cells 14986
Terrain_204.ttile blocks 8 hole cells 2259
Terrain_205.ttile blocks 2 hole cells 110
Terrain_210.ttile blocks 5 hole cells 138
Terrain_211.ttile blocks 11 hole cells 7267
Terrain_212.ttile blocks 15 hole cells 11724
Terrain_213.ttile blocks 16 hole cells 14481
Terrain_214.ttile blocks 16 hole cells 11668
Terrain_215.ttile blocks 16 hole cells 13157
Terrain_216.ttile blocks 13 hole cells 8573
Terrain_218.ttile blocks 2 hole cells 399
Terrain_219.ttile blocks 4 hole cells 2981
Terrain_220.ttile blocks 4 hole cells 1943
Terrain_227.ttile blocks 4 hole cells 749
Terrain_229.ttile blocks 3 hole cells 333
wrote 132 tiles, hole cells 1396378 (ok)
```
No backups made (no addon tiles existed before).

## Census and verify

```
MUD census box <0,-163,0> .. <4096,148.375,4096> cells 2048 x 2048 cell 2
MUD census done holes 1396378 runs 16839 ms 767 truncated 0
expected holes 1396378 observed 1396378 missing 0 extra 0
VERIFY OK
```
Census file: `2026-10-08-census.txt`.

Checksum: keep 2797926 + holes 1396378 = 4194304; sum of per-tile hole cells = 1396378 = census holes 1396378.

## Walk check (user, 2026-10-08 23:59)

Tarmac road: no fall. Dirt path: no fall. Holes visible everywhere off roads/paths/water (screenshot). Some buildings,
trees and other objects fall through (no skin yet - Task 3, Review Focus 4).
