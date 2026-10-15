# Gate 1 - addon override of vanilla Arland .ttile (plan 1, task 1)

Date 2026-10-08, Arma Reforger Experimental 192624 (Workbench log: version 1.9.0.10 built 2026-09-20), world
`Arlnd.ent` (subscene of `{77C41EA429B6AC35}worlds/GameMaster/GM_Arland.ent`), census entity `MUD_HoleCensus.c`.

**Result: PASS.** An addon file at `worlds/Arland/Terrain/.Data/Terrain_<i>.ttile` overrides the pak tile; its holes
load in play, a soldier falls through, and Workbench packs the file into the addon pak byte-identical.

**Surprise: HOLE block coords are terrain-GLOBAL, not tile-local.** Blocks written into Terrain_1/16/18 all landed
at their written cells (from the terrain corner), none inside their own tile. Tile index order (x- vs z-fastest)
is therefore not needed to place holes and was not measured.

## Writes (tile-local reading would have put them elsewhere)

| File | Cells (x0 z0 x1 z1, incl.) | Block | Cells |
|---|---|---|---|
| Terrain_1.ttile | 32 64 35 65 | (1, 2) | 8 |
| Terrain_16.ttile | 70 10 71 13 | (2, 0) | 8 |
| Terrain_18.ttile | 100 100 102 102 | (3, 3) | 9 |
| Terrain_18.ttile (added 22:5x, land) | 640 1100 643 1103 | (20, 34) | 16 |

Tiles copied from the Experimental pak with `pakread.py get` (35930 / 36004 / 36934 bytes before holes).

## Census runs

| Time | Override | Census `done` line | Checker |
|---|---|---|---|
| 22:42:44 | none (vanilla) | `holes 0 runs 0 ms 701 truncated 0`, box `<0,-163,0> .. <4096,148.375,4096>`, cells 2048 x 2048 | baseline |
| 22:49:18 | first 3 writes | `holes 25 runs 9 ms 389 truncated 0` | `MATCH global` |
| 23:01:32 | all 4 writes | `holes 41 runs 13 ms 400 truncated 0` | `MATCH global` |

Checksums: baseline 0 + 25 written = 25 (22:49), 0 + 41 written = 41 (23:01), both evaluated by the checker
(`holes` == cells in runs).

Checker output 22:49 (`census-override.log`):
```
after: 2048 x 2048 cells, 25 holes, truncated False
baseline: 0 holes, truncated False
new hole cells: 25 (x 32..102 z 10..102)
  x-fastest/local: expected 25, matched 0, extra 25, missing 25
  z-fastest/local: expected 25, matched 0, extra 25, missing 25
  global: expected 25, matched 25, extra 0, missing 0
MATCH global
```
Checker output 23:01 (`census-override-land.log`): `new hole cells: 41 (x 32..643 z 10..1103)`, global 41/41, both
local hypotheses 0/41, `MATCH global`.

## Other observations

- Workbench created no `.meta` for the copied `.ttile` files; no log line mentions them (22:48 restart log).
- After rewriting a tile, closing and reopening the world was enough (no Workbench restart) - the 23:01 census saw
  the land hole written after the 22:48 restart.
- Physical check 22:58 (user): walked to world x 1280-1288, z 2200-2208 and fell through.
- Pack check 23:03: Workbench publish dialog packed to
  `%TEMP%/Arma Reforger Workbench Experimental/Publishing/6A8F47B45144E6D6/data.pak` (65 files, no upload). It holds
  `worlds/Arland/Terrain/.Data/Terrain_1/16/18.ttile`, decompressed bytes == loose files (all 3 True).
- The pak also holds `.remember/backups/*.ttile`, `NEw map/...` and `Terrain/...` test files - exclude before shipping.
- `Arlnd_Layers/default.layer` was re-saved at 22:55 without the census entity (house moved); the user re-added it
  for the 23:01 run. Cause of the drop not investigated.

Logs: `census-baseline.log`, `census-override.log`, `census-override-land.log` (this folder).
Pinned test: `Tools/test_hole_census.py::test_measured_layout_arland_exp_192624`.
