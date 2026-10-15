# Mud Whole-Map Holes - Hole Tool + Coarse Skin (Plan 2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Arland holed everywhere except roads and water, and still playable: an in-play scan writes a keep mask, a Python tool writes the HOLE chunks into all 256 tiles, and a coarse skin entity fills every hole cell with the exact 2 m heightmap triangles (collider + terrain-look mesh).

**Architecture:** Task 1 builds the offline Python tool `Tools/mud_holes.py` (mask parse, keep-margin dilation, block packing, owner tile, vanilla-based tile write, census verify) with pytest. Task 2 adds the in-play scan entity `MUD_HoleMask.c` (roads by RoadEntity-only trace, water by `TryGetWaterSurfaceSimple`, keep boxes), runs it on vanilla Arland, writes the tiles, and proves with the census that every planned hole and only those are in the game. Task 3 adds `MUD_CoarseSkin.c` (production successor of the `MUD_WorldSkin.c` probe): only hole cells get triangles, per-cell gamemat, build at init (default) or time-budgeted over frames, and a self-check that traces the skin against the heightmap.

**Tech Stack:** Enforce Script (Arma Reforger Experimental 192624 Workbench), Python 3 stdlib + pytest, `blender-mcp-BK/tools/pakread.py`.

**Spec:** `docs/superpowers/specs/2026-10-08-mud-whole-map-holes-design.md` (sections 4, 5.1, 7 step 3). Gate results this plan stands on: `docs/superpowers/results/2026-10-08-gate1-override.md`, `docs/superpowers/results/2026-10-08-gate2-skin-cost.md`.

**Scope:** spec build order step 3 (hole tool) plus the coarse-skin half of step 4, because a holed Arland is unplayable without the skin, so neither is testable in play alone. The rest of step 4 (fine patches, coarse swap, Edge Mode, depth boxes, floor, hard surfaces, bullet slab) and step 5 (MP) are Plan 3.

**Deviations from the spec (decided here, each costs one task if wrong):**
- Spec 4.1 asks for a Workbench plugin that reads road/water generators. This plan scans in play instead: the runtime sees the final generated roads and water (no generator parsing). If the road trace finds no roads (Task 2 gate), stop and re-plan to the plugin route (`SCR_RoadGeneratorEntity` pattern: `SCR_ShapeNextPointHelper.CreateFromShape`, `RoadWidth`).
- Spec 5.1 render "2 m near, 8 m beyond 500 m, skirts": this plan renders 2 m everywhere, no skirts. Measured: full-map 2 m render + collider costs +0.9 ms frame, +587 MB (gate 2). LOD cracks at hole edges are measured first (Task 3); skirts / far mesh only if they are seen.
- Gate 2 decision rule pointed at a 4 m far collider; user decision 2026-10-08: **2 m collider over the whole map**, and tune the build per frame instead (Task 3: time budget in ms per frame, or build at init).
- Spec 4.1 road margin "1 m": here a cell is kept when any of its 4 corners or its centre samples road, then the kept set grows by 1 cell (2 m) - at least the spec margin. `--margin` changes it.
- Tiles are always rebuilt from the vanilla pak tile (vanilla holes OR'ed in), never merged into an older addon tile: a re-run with a changed mask is exact and idempotent.

Paths below: `ADDON` = `C:\Users\Steffen\Documents\My Games\ArmaReforgerWorkbench\addons\Testing plan vech msh`, `BK` = `C:\Users\Steffen\Documents\A_documents\Github\blender-mcp-BK`, `DATA` = `ADDON\worlds\Arland\Terrain\.Data`, `TILEBAK` = `C:\Users\Steffen\Documents\My Games\ArmaReforgerWorkbench\mud_backups\tiles` (outside the addon, so it is never packed). Workbench log folder: newest `C:\Users\Steffen\Documents\My Games\ArmaReforgerWorkbench\logs\logs_<date>_<time>\` (`console.log`; `$logs:` files land in the same folder).

## Global Constraints

- Target: Arma Reforger **Experimental 192624** only (Workbench "Experimental, port 5776"). Stable not supported (spec section 9).
- Game files are read from the Experimental pak (`pakread.py`, install `experimental`), never from the stale loose `addons/data` tree or `E:\Arma reforger data\extracted_files\`.
- Heightmap cell = **2 m**; Arland = 2048 x 2048 cells = 16 x 16 tiles of 128 cells = 64 x 64 blocks of 32 cells; terrain box `<0,-163,0> .. <4096,148.375,4096>` (census 2026-10-08).
- HOLE chunk (spec 4.2): `u32 count`, `u16 128`, per block `u16 bx, u16 bz` + 32 rows `u32`; bit c of row r = cell (bx*32+c, bz*32+r). Block coords are **terrain-global** (gate 1). Owner tile of a block = `(bz // 4) * 16 + bx // 4` (x-fastest, measured from shared HGHT edges).
- Hole test in script: `GetSurfaceY(x, z) < -255`. Heights in or near holes: `TraceFlags.WORLD` trace only (spec 5.1).
- Samples stay 1 cm inside the far terrain edges (WORLD trace exactly on x = 4096 missed: 4223 misses, gate 2).
- `.ttile` writes only with the world closed in Workbench, only through `Tools/mud_holes.py` (round-trip check before and read-back after every write).
- `.ttile` HGHT = 129 x 129 u16 = 33282 bytes, no header.
- Skin: 64 m chunks, height +1 mm, layers `EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage`, terrain-look material `{D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat`, colliders on every machine, meshes not on a dedicated server.
- New `.c` files are compiled by the user (open in Script Editor, Ctrl+S / F7); `wb_reload` does not compile them. `wb_play` needs the user to focus the game window.
- `string.Format` takes at most 9 arguments.
- No git (solo mod work, user rule). Results go to `docs/superpowers/results/` with time and game build; whole per-item tables plus a checksum line.
- Before shipping a pack: exclude `.remember/backups/*.ttile`, `NEw map/...`, `Terrain/...` test files (gate 1 pack check).

## Review Focus

1. **Roads may have no collider a trace can hit.** The editor code that tells roads apart by `TraceEnt.Type() == RoadEntity` proves the type, not that every Arland road collides. Expected: the scan's control sample (entity placed on a road) logs `road 1`, the done line has `road > 0`, and a walk on a dirt path after the write shows no hole under it. Pinned in Task 2 steps 6-7 (gate: stop and re-plan on `road 0`).
2. **Scanning a map that already has our holes** (re-run after Task 2 wrote tiles) would read every hole as "under the -256 floor" and keep the seabed logic broken. Expected: the scan refuses (`FAILED ... WITHOUT our holes`, no done line) and `mud_holes.py` refuses a mask without a done line. Pinned: `test_parse_mask_raises_without_done` (Task 1) + Task 2 step 6 control note.
3. **A census of the fully holed map overflows the log** (hundreds of thousands of run lines, old cap 3000). Expected: the census writes runs to a file (`m_sFile`), never truncates there, and `mud_holes.py verify` refuses a truncated census. Pinned: Task 2 step 2 (census file output) + `test_verify_refuses_truncated_census`.
4. **Vehicles and soldiers placed in the world fall through holes before the skin exists.** Expected: with build at init (default) the M923A1 in the layer stays put; with the spread build it may drop, and the result file says so. Pinned: Task 3 step 6 (M923 height before / after 10 s).
5. **Far-edge samples miss the WORLD trace and fall back to GetSurfaceY = -256 inside a hole**, giving a 260 m spike on the map border. Expected: clamped samples, `trace misses 0` in the skin log, check `worst` < 5 cm. Pinned: Task 3 step 5 Expected lines.

---

### Task 1: Python hole tool `mud_holes.py`

**Files:**
- Create: `ADDON/Tools/mud_holes.py`
- Create: `ADDON/Tools/test_mud_holes.py`
- Uses unchanged: `ADDON/Tools/ttile_holes.py` (`chunks`, `decode`, `encode`, `build`)

**Interfaces:**
- Consumes: `ttile_holes.chunks(bytes) -> [(tag, data)]`, `decode(data) -> {(bx, bz): [32 x u32]}`, `encode(blocks) -> bytes` (drops all-zero blocks), `build(parts) -> bytes`; `BK/tools/pakread.py`: `pakread.Pak('experimental').read('worlds/Arland/Terrain/.Data/Terrain_<i>.ttile') -> bytes` (checked 2026-10-08: Terrain_18 = 36934 bytes, `FORM`/`TERR`).
- Produces (Task 2 relies on these):
  - mask file format (written by `MUD_HoleMask.c`): `MUD mask box <x0> <z0> cells <nx> <nz> cell <size>`, `MUD mask keep z <cz> x <cx0> <cx1>`, `MUD mask done keep <n> runs <r> road <a> water <b> box <c> ms <t>`; last block wins; a log-line prefix before `MUD mask` is allowed.
  - census format read by `verify`: `MUD census box ... cells <nx> x <nz> ...`, `MUD census run z <cz> x <a> <b>`, `MUD census done holes <n> runs <r> ms <t> truncated <0|1>`.
  - `parse_mask(text) -> {'nx', 'nz', 'cell', 'keep': bytearray}`; `dilate(keep, nx, nz, margin) -> bytearray`; `hole_blocks(keep, nx, nz) -> {(bx, bz): rows}`; `owner_tile(bx, bz, tile_blocks=4, tiles_x=16) -> int`; `merge_tile(data, blocks) -> bytes`; `write_tiles(blocks, out_dir, backup_dir, load_vanilla) -> [(tile, nblocks, cells)]`; `stats(mask, margin) -> dict`; `verify(census_text, mask_text, margin) -> {'expected', 'observed', 'missing', 'extra'}`.
  - CLI: `python -I Tools/mud_holes.py plan|write|verify ...`, prints `grid ... keep K + holes H = N (ok)`, `wrote T tiles, hole cells H (ok)`, `VERIFY OK` / `VERIFY FAIL` (exit 0 only on ok).

- [x] **Step 1: Write the failing tests**

Create `ADDON/Tools/test_mud_holes.py`:

```python
import struct

import pytest

import mud_holes as mh
import ttile_holes as tt


def mask_text(nx, nz, runs, keep=None, nruns=None, done=True, prefix=''):
    lines = [prefix + 'MUD mask 1', prefix + 'MUD mask box 0 0 cells %d %d cell 2' % (nx, nz)]
    lines += [prefix + 'MUD mask keep z %d x %d %d' % r for r in runs]
    if keep is None:
        keep = sum(b - a + 1 for _, a, b in runs)
    if nruns is None:
        nruns = len(runs)
    if done:
        lines.append(prefix + 'MUD mask done keep %d runs %d road 0 water 0 box 0 ms 1' % (keep, nruns))
    return '\n'.join(lines) + '\n'


def fake_ttile(hole=None):
    """FORM/TERR with the chunk order seen in shipped tiles; HGHT is 129 x 129 u16, no header."""
    parts = [(b'VERS', b'\x01' * 9), (b'HGHT', b'\0' * 33282), (b'BERR', b'\0' * 576)]
    if hole is not None:
        parts.append((b'HOLE', tt.encode(hole)))
    parts += [(b'LRS2', b'abc'), (b'TMAT', b'\0' * 4)]
    return tt.build(parts)


def holes_of(data):
    parts = dict(tt.chunks(data))
    return tt.decode(parts[b'HOLE']) if b'HOLE' in parts else {}


# ---------------------------------------------------------------- parse_mask

def test_parse_mask_reads_runs():
    m = mh.parse_mask(mask_text(64, 32, [(0, 0, 3), (5, 10, 10)]))
    assert (m['nx'], m['nz']) == (64, 32)
    assert sum(m['keep']) == 5
    assert m['keep'][0 * 64 + 3] == 1 and m['keep'][0 * 64 + 4] == 0
    assert m['keep'][5 * 64 + 10] == 1


def test_parse_mask_uses_last_block_and_log_prefix():
    text = mask_text(64, 32, [(0, 0, 63)]) + mask_text(64, 32, [(1, 2, 2)], prefix='22:07:44.899   SCRIPT       : ')
    m = mh.parse_mask(text)
    assert sum(m['keep']) == 1 and m['keep'][1 * 64 + 2] == 1


def test_parse_mask_raises_without_done():
    with pytest.raises(ValueError, match='done'):
        mh.parse_mask(mask_text(64, 32, [(0, 0, 3)], done=False))


def test_parse_mask_checksum_mismatch_raises():
    with pytest.raises(ValueError, match='checksum'):
        mh.parse_mask(mask_text(64, 32, [(0, 0, 3)], keep=5))


def test_parse_mask_overlapping_runs_raise():
    with pytest.raises(ValueError, match='overlap'):
        mh.parse_mask(mask_text(64, 32, [(0, 0, 3), (0, 2, 5)]))


def test_parse_mask_run_outside_grid_raises():
    with pytest.raises(ValueError, match='outside'):
        mh.parse_mask(mask_text(64, 32, [(0, 60, 64)]))


def test_parse_mask_grid_not_multiple_of_32_raises():
    with pytest.raises(ValueError, match='32'):
        mh.parse_mask(mask_text(40, 32, []))


# ---------------------------------------------------------------- dilate

def test_dilate_grows_square_and_clips_at_edges():
    nx, nz = 8, 8
    keep = bytearray(nx * nz)
    keep[0] = 1                 # corner cell
    keep[4 * nx + 4] = 1        # middle cell
    out = mh.dilate(keep, nx, nz, 1)
    assert sum(out) == 4 + 9    # corner clipped to 2 x 2, middle 3 x 3
    assert out[3 * nx + 3] == 1 and out[5 * nx + 5] == 1 and out[2 * nx + 2] == 0
    assert out[nx - 1] == 0     # no wrap from row 0 into the row end


def test_dilate_zero_margin_is_a_copy():
    keep = bytearray([0, 1, 0, 0] * 4)
    out = mh.dilate(keep, 4, 4, 0)
    assert out == keep and out is not keep


# ---------------------------------------------------------------- blocks and tiles

def test_hole_blocks_inverts_keep_and_skips_full_keep_blocks():
    nx, nz = 64, 32
    keep = bytearray(nx * nz)
    for i in range(32, 64):          # block (1, 0) fully kept
        for z in range(32):
            keep[z * nx + i] = 1
    keep[3 * nx + 5] = 1             # one kept cell in block (0, 0)
    blocks = mh.hole_blocks(keep, nx, nz)
    assert list(blocks) == [(0, 0)]
    rows = blocks[(0, 0)]
    assert rows[3] == 0xFFFFFFFF & ~(1 << 5)
    assert all(r == 0xFFFFFFFF for i, r in enumerate(rows) if i != 3)


def test_owner_tile_is_x_fastest():
    assert mh.owner_tile(0, 0) == 0
    assert mh.owner_tile(3, 3) == 0
    assert mh.owner_tile(4, 0) == 1
    assert mh.owner_tile(0, 4) == 16
    assert mh.owner_tile(63, 63) == 255


def test_merge_tile_inserts_hole_after_berr_and_round_trips():
    out = mh.merge_tile(fake_ttile(), {(2, 1): [1] * 32})
    tags = [t for t, _ in tt.chunks(out)]
    assert tags == [b'VERS', b'HGHT', b'BERR', b'HOLE', b'LRS2', b'TMAT']
    assert holes_of(out) == {(2, 1): [1] * 32}


def test_merge_tile_ors_with_vanilla_holes():
    out = mh.merge_tile(fake_ttile({(2, 1): [2] * 32}), {(2, 1): [1] * 32, (3, 1): [4] * 32})
    assert holes_of(out) == {(2, 1): [3] * 32, (3, 1): [4] * 32}


def test_merge_tile_refuses_unknown_layout():
    with pytest.raises(ValueError, match='round trip'):
        mh.merge_tile(fake_ttile() + b'junk', {(0, 0): [1] * 32})


# ---------------------------------------------------------------- write_tiles

def test_write_tiles_starts_from_vanilla_backs_up_once_and_rewrites_stale(tmp_path):
    out, bak = tmp_path / 'Data', tmp_path / 'bak'
    out.mkdir()
    (out / 'Terrain_1.ttile').write_bytes(b'old addon tile 1')
    (out / 'Terrain_5.ttile').write_bytes(b'stale addon tile 5')   # no blocks for 5 in this run
    loaded = []

    def vanilla(t):
        loaded.append(t)
        return fake_ttile()

    blocks = {(0, 0): [1] * 32, (4, 0): [0xFFFFFFFF] * 32}       # tiles 0 and 1
    report = mh.write_tiles(blocks, out, bak, vanilla)
    assert sorted(loaded) == [0, 1, 5]
    assert holes_of((out / 'Terrain_0.ttile').read_bytes()) == {(0, 0): [1] * 32}
    assert holes_of((out / 'Terrain_1.ttile').read_bytes()) == {(4, 0): [0xFFFFFFFF] * 32}
    assert holes_of((out / 'Terrain_5.ttile').read_bytes()) == {}
    assert (bak / 'Terrain_1.ttile').read_bytes() == b'old addon tile 1'
    assert (bak / 'Terrain_5.ttile').read_bytes() == b'stale addon tile 5'
    assert report == [(0, 1, 32), (1, 1, 1024), (5, 0, 0)]

    # second run: the first backup is never overwritten
    mh.write_tiles(blocks, out, bak, vanilla)
    assert (bak / 'Terrain_1.ttile').read_bytes() == b'old addon tile 1'


# ---------------------------------------------------------------- stats and verify

def test_stats_checksum():
    m = mh.parse_mask(mask_text(64, 32, [(0, 0, 3)]))
    s = mh.stats(m, margin=1)
    assert s['keep'] + s['holes'] == 64 * 32
    assert s['keep'] == 4 * 2 + 1 * 2   # run 0..3 on row 0 grows to x 0..4, z 0..1
    assert s['blocks'] == 2


def census_text(nx, nz, runs, holes=None, truncated=0):
    lines = ['MUD census box <0,0,0> .. <1,1,1> cells %d x %d cell 2' % (nx, nz)]
    lines += ['MUD census run z %d x %d %d' % r for r in runs]
    if holes is None:
        holes = sum(b - a + 1 for _, a, b in runs)
    lines.append('MUD census done holes %d runs %d ms 1 truncated %d' % (holes, len(runs), truncated))
    return '\n'.join(lines) + '\n'


def test_verify_passes_when_census_equals_mask():
    mask = mask_text(64, 32, [(z, 0, 31) for z in range(32)])   # left block kept, right block holed
    census = census_text(64, 32, [(z, 32, 63) for z in range(32)])
    r = mh.verify(census, mask, margin=0)
    assert (r['expected'], r['observed'], r['missing'], r['extra']) == (1024, 1024, 0, 0)


def test_verify_counts_missing_and_extra():
    mask = mask_text(64, 32, [(z, 0, 31) for z in range(32)])
    census = census_text(64, 32, [(0, 31, 62)] + [(z, 32, 63) for z in range(1, 32)])
    r = mh.verify(census, mask, margin=0)
    assert (r['missing'], r['extra']) == (1, 1)


def test_verify_refuses_truncated_census():
    mask = mask_text(64, 32, [])
    with pytest.raises(ValueError, match='truncated'):
        mh.verify(census_text(64, 32, [], holes=5, truncated=1), mask, margin=0)


def test_verify_refuses_size_mismatch():
    with pytest.raises(ValueError, match='size'):
        mh.verify(census_text(32, 32, []), mask_text(64, 32, []), margin=0)
```

- [x] **Step 2: Run the tests to verify they fail**

Run: `cd "ADDON/Tools" && python -m pytest -q test_mud_holes.py`
Expected: collection error `ModuleNotFoundError: No module named 'mud_holes'`.

- [x] **Step 3: Write the implementation**

Create `ADDON/Tools/mud_holes.py`:

```python
"""Whole-map terrain holes (mud plan 2): hole mask -> HOLE chunks in every .ttile of a map.

Reads the keep mask written in play by MUD_HoleMask.c (roads, water, keep boxes), grows the kept cells by a
margin, holes every other 2 m cell and writes each 32 x 32-cell block into its owner tile
(x-fastest: tile = (bz // 4) * 16 + bx // 4, Arland Exp 192624). Every written tile starts from the VANILLA
pak tile (vanilla holes OR'ed in), so a re-run is idempotent; an existing addon tile is backed up once first.
Close the world in Workbench before writing.

    python -I Tools/mud_holes.py plan   <mask.txt> [--margin 1]
    python -I Tools/mud_holes.py write  <mask.txt> --out <addon>/worlds/Arland/Terrain/.Data
                                        --backup <dir> --bk <blender-mcp-BK> [--margin 1] [--install experimental]
    python -I Tools/mud_holes.py verify <census.txt> <mask.txt> [--margin 1]

Mask lines (file or console.log, last block wins):
    MUD mask box <x0> <z0> cells <nx> <nz> cell <size>
    MUD mask keep z <cz> x <cx0> <cx1>          (inclusive, cells from the terrain box corner)
    MUD mask done keep <n> runs <r> road <a> water <b> box <c> ms <t>
"""
import argparse
import os
import re
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I drops the script dir from sys.path
import ttile_holes as tt  # noqa: E402

BLOCK = 32
FULL = 0xFFFFFFFF
TILE_PATH = 'worlds/Arland/Terrain/.Data/Terrain_%d.ttile'

BOX = re.compile(r'MUD mask box \S+ \S+ cells (\d+) (\d+) cell (\S+)')
KEEP = re.compile(r'MUD mask keep z (\d+) x (\d+) (\d+)')
DONE = re.compile(r'MUD mask done keep (\d+) runs (\d+)')
CBOX = re.compile(r'MUD census box .* cells (\d+) x (\d+)')
CRUN = re.compile(r'MUD census run z (\d+) x (\d+) (\d+)')
CDONE = re.compile(r'MUD census done holes (\d+) runs (\d+) ms \d+ truncated (\d)')


def _last_block(text, box_re):
    lines = text.splitlines()
    start = None
    for i, line in enumerate(lines):
        if box_re.search(line):
            start = i
    return lines, start


def parse_mask(text):
    """Last mask block -> dict(nx, nz, cell, keep=bytearray nx*nz of 0/1). ValueError if incomplete."""
    lines, start = _last_block(text, BOX)
    if start is None:
        raise ValueError('no "MUD mask box" line')
    m = BOX.search(lines[start])
    nx, nz, cell = int(m[1]), int(m[2]), float(m[3])
    if nx % BLOCK or nz % BLOCK:
        raise ValueError('grid %d x %d is not a multiple of 32 cells' % (nx, nz))
    keep = bytearray(nx * nz)
    runs = cells = 0
    done = None
    for line in lines[start + 1:]:
        r = KEEP.search(line)
        if r:
            cz, a, b = int(r[1]), int(r[2]), int(r[3])
            if cz >= nz or a > b or b >= nx:
                raise ValueError('run z %d x %d %d outside the %d x %d grid' % (cz, a, b, nx, nz))
            row = cz * nx
            if any(keep[row + a:row + b + 1]):
                raise ValueError('run z %d x %d %d overlaps an earlier run' % (cz, a, b))
            keep[row + a:row + b + 1] = b'\1' * (b - a + 1)
            runs += 1
            cells += b - a + 1
            continue
        d = DONE.search(line)
        if d:
            done = (int(d[1]), int(d[2]))
            break
    if done is None:
        raise ValueError('mask block has no "done" line (play stopped early?)')
    if done != (cells, runs):
        raise ValueError('checksum: done line says keep %d runs %d, file holds %d cells in %d runs'
                         % (done[0], done[1], cells, runs))
    return {'nx': nx, 'nz': nz, 'cell': cell, 'keep': keep}


def dilate(keep, nx, nz, margin):
    """Grow kept cells by `margin` cells in x and z (square), clipped at the grid edge. Returns a new array."""
    if margin <= 0:
        return bytearray(keep)
    rowmask = (1 << (8 * nx)) - 1
    rows = []
    for z in range(nz):  # one byte per cell: shifting the row int by 8 bits moves it one cell
        v = int.from_bytes(keep[z * nx:(z + 1) * nx], 'big')
        g = v
        for k in range(1, margin + 1):
            g |= ((v << (8 * k)) & rowmask) | (v >> (8 * k))
        rows.append(g)
    out = bytearray(nx * nz)
    for z in range(nz):
        g = 0
        for k in range(max(0, z - margin), min(nz, z + margin + 1)):
            g |= rows[k]
        out[z * nx:(z + 1) * nx] = g.to_bytes(nx, 'big')
    return out


def hole_blocks(keep, nx, nz):
    """{(bx, bz): 32 row bitmasks} for every block holding at least one hole (= not kept) cell."""
    allkeep = b'\1' * BLOCK
    allhole = b'\0' * BLOCK
    out = {}
    for bz in range(nz // BLOCK):
        for bx in range(nx // BLOCK):
            rows = []
            for r in range(BLOCK):
                at = (bz * BLOCK + r) * nx + bx * BLOCK
                seg = keep[at:at + BLOCK]
                if seg == allhole:
                    rows.append(FULL)
                elif seg == allkeep:
                    rows.append(0)
                else:
                    rows.append(sum(1 << c for c in range(BLOCK) if not seg[c]))
            if any(rows):
                out[(bx, bz)] = rows
    return out


def owner_tile(bx, bz, tile_blocks=4, tiles_x=16):
    """Tile index of the .ttile that owns block (bx, bz): x-fastest (measured, Arland Exp 192624)."""
    return (bz // tile_blocks) * tiles_x + bx // tile_blocks


def merge_tile(data, blocks):
    """Vanilla .ttile bytes + hole blocks -> new bytes (holes OR'ed). ValueError if the layout is unknown."""
    if data[:4] != b'FORM' or data[8:12] != b'TERR':
        raise ValueError('not a FORM/TERR file')
    parts = tt.chunks(data)
    if tt.build(parts) != data:
        raise ValueError('round trip failed - unknown layout, not writing')
    tags = [t for t, _ in parts]
    have = tt.decode(dict(parts)[b'HOLE']) if b'HOLE' in tags else {}
    for k, rows in blocks.items():
        old = have.setdefault(k, [0] * BLOCK)
        have[k] = [a | b for a, b in zip(old, rows)]
    payload = tt.encode(have)
    if b'HOLE' in tags:
        parts = [(t, payload if t == b'HOLE' else d) for t, d in parts]
    else:  # chunk order of saved tiles: VERS HGHT BERR HOLE ... -> after BERR
        parts.insert(tags.index(b'BERR') + 1, (b'HOLE', payload))
    out = tt.build(parts)
    if tt.decode(dict(tt.chunks(out))[b'HOLE']) != {k: v for k, v in have.items() if any(v)}:
        raise ValueError('written HOLE chunk does not read back')
    return out


def write_tiles(blocks, out_dir, backup_dir, load_vanilla, tile_blocks=4, tiles_x=16):
    """Write every tile that gets blocks or already exists in out_dir. Returns [(tile, blocks, cells)]."""
    out_dir, backup_dir = str(out_dir), str(backup_dir)
    per_tile = {}
    for k, rows in blocks.items():
        per_tile.setdefault(owner_tile(k[0], k[1], tile_blocks, tiles_x), {})[k] = rows
    existing = set()
    if os.path.isdir(out_dir):
        for name in os.listdir(out_dir):
            m = re.fullmatch(r'Terrain_(\d+)\.ttile', name)
            if m:
                existing.add(int(m[1]))
    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(backup_dir, exist_ok=True)
    report = []
    for t in sorted(set(per_tile) | existing):
        path = os.path.join(out_dir, 'Terrain_%d.ttile' % t)
        bak = os.path.join(backup_dir, 'Terrain_%d.ttile' % t)
        if os.path.exists(path) and not os.path.exists(bak):
            shutil.copyfile(path, bak)  # first backup only: it is the pre-tool state
        mine = per_tile.get(t, {})
        data = merge_tile(load_vanilla(t), mine)
        with open(path, 'wb') as f:
            f.write(data)
        report.append((t, len(mine), sum(bin(r).count('1') for rows in mine.values() for r in rows)))
    return report


def stats(mask, margin):
    keep = dilate(mask['keep'], mask['nx'], mask['nz'], margin)
    blocks = hole_blocks(keep, mask['nx'], mask['nz'])
    k = sum(keep)
    return {'keep': k, 'holes': mask['nx'] * mask['nz'] - k, 'blocks': len(blocks),
            'tiles': len({owner_tile(bx, bz) for bx, bz in blocks}), 'keep_grid': keep, 'block_map': blocks}


def parse_census_grid(text):
    """Last census block -> (nx, nz, bytearray of 1 = hole). ValueError if incomplete or truncated."""
    lines, start = _last_block(text, CBOX)
    if start is None:
        raise ValueError('no "MUD census box" line')
    m = CBOX.search(lines[start])
    nx, nz = int(m[1]), int(m[2])
    hole = bytearray(nx * nz)
    cells = 0
    done = None
    for line in lines[start + 1:]:
        r = CRUN.search(line)
        if r:
            cz, a, b = int(r[1]), int(r[2]), int(r[3])
            hole[cz * nx + a:cz * nx + b + 1] = b'\1' * (b - a + 1)
            cells += b - a + 1
            continue
        d = CDONE.search(line)
        if d:
            done = d
            break
    if done is None:
        raise ValueError('census block has no "done" line')
    if done[3] == '1':
        raise ValueError('census truncated - raise m_iMaxRuns or write to file')
    if int(done[1]) != cells:
        raise ValueError('checksum: census says %s holes, runs hold %d' % (done[1], cells))
    return nx, nz, hole


def verify(census_text, mask_text, margin):
    mask = parse_mask(mask_text)
    nx, nz, hole = parse_census_grid(census_text)
    if (nx, nz) != (mask['nx'], mask['nz']):
        raise ValueError('size: census %d x %d, mask %d x %d' % (nx, nz, mask['nx'], mask['nz']))
    keep = dilate(mask['keep'], nx, nz, margin)
    missing = extra = expected = 0
    for i in range(nx * nz):
        want = not keep[i]
        expected += want
        if want and not hole[i]:
            missing += 1
        elif hole[i] and not want:
            extra += 1
    return {'expected': expected, 'observed': sum(hole), 'missing': missing, 'extra': extra}


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['plan', 'write', 'verify'])
    ap.add_argument('files', nargs='+')
    ap.add_argument('--margin', type=int, default=1)
    ap.add_argument('--out')
    ap.add_argument('--backup')
    ap.add_argument('--bk')
    ap.add_argument('--install', default='experimental')
    a = ap.parse_args(argv)
    read = lambda p: open(p, encoding='utf-8', errors='replace').read()  # noqa: E731

    if a.cmd == 'verify':
        r = verify(read(a.files[0]), read(a.files[1]), a.margin)
        print('expected holes %(expected)d observed %(observed)d missing %(missing)d extra %(extra)d' % r)
        print('VERIFY OK' if r['missing'] == 0 and r['extra'] == 0 else 'VERIFY FAIL')
        return 0 if r['missing'] == 0 and r['extra'] == 0 else 1

    mask = parse_mask(read(a.files[0]))
    s = stats(mask, a.margin)
    n = mask['nx'] * mask['nz']
    print('grid %d x %d = %d cells, margin %d: keep %d + holes %d = %d (%s), %d blocks in %d tiles'
          % (mask['nx'], mask['nz'], n, a.margin, s['keep'], s['holes'], s['keep'] + s['holes'],
             'ok' if s['keep'] + s['holes'] == n else 'MISMATCH', s['blocks'], s['tiles']))
    if a.cmd == 'plan':
        return 0

    if not (a.out and a.backup and a.bk):
        ap.error('write needs --out, --backup and --bk')
    sys.path.insert(0, os.path.join(a.bk, 'tools'))
    import pakread
    pak = pakread.Pak(a.install)
    report = write_tiles(s['block_map'], a.out, a.backup, lambda t: pak.read(TILE_PATH % t))
    for t, nb, cells in report:
        print('Terrain_%d.ttile blocks %d hole cells %d' % (t, nb, cells))
    total = sum(c for _, _, c in report)
    print('wrote %d tiles, hole cells %d (%s)' % (len(report), total, 'ok' if total == s['holes'] else 'MISMATCH'))
    return 0 if total == s['holes'] else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
```

- [x] **Step 4: Run the tests to verify they pass**

Run: `cd "ADDON/Tools" && python -m pytest -q test_mud_holes.py test_hole_census.py`
Expected: `34 passed` (20 new + 14 existing census tests).

- [x] **Step 5: Full-size and pak smoke test**

Run (from `ADDON`):
```bash
python -I - <<'EOF'
import random, sys, time
sys.path.insert(0, 'Tools'); import mud_holes as mh
random.seed(1); lines = ['MUD mask box 0 0 cells 2048 2048 cell 2']; k = r = 0
for z in range(2048):
    x = 0
    while True:
        x += random.randint(5, 60); w = random.randint(1, 4)
        if x + w >= 2048: break
        lines.append('MUD mask keep z %d x %d %d' % (z, x, x + w - 1)); k += w; r += 1; x += w
lines.append('MUD mask done keep %d runs %d road 0 water 0 box 0 ms 1' % (k, r))
t = time.time(); s = mh.stats(mh.parse_mask('\n'.join(lines)), 1)
print('runs', r, 'secs %.1f' % (time.time() - t), 'keep', s['keep'], 'holes', s['holes'], 'blocks', s['blocks'], 'tiles', s['tiles'])
sys.path.insert(0, r'C:\Users\Steffen\Documents\A_documents\Github\blender-mcp-BK\tools'); import pakread
d = pakread.Pak('experimental').read(mh.TILE_PATH % 18)
out = mh.merge_tile(d, {b: v for b, v in s['block_map'].items() if mh.owner_tile(*b) == 18})
print(len(d), len(out))
EOF
```
Expected (measured while writing this plan, 2026-10-08): `runs 119325 secs 0.3 keep 1413502 holes 2780802 blocks 4096 tiles 256` (time may differ), then `36934 39060` (39060 = 36934 + 8 chunk header + 6 + 16 blocks x 132).

---

### Task 2: Keep-mask scan, write Arland holes, census check

**Files:**
- Create: `ADDON/Scripts/Game/Mud/MUD_HoleMask.c`
- Modify: `ADDON/Scripts/Game/Mud/MUD_HoleCensus.c` (file output, no truncation there)
- Modify: `ADDON/Arlnd_Layers/default.layer` (skin probe out, mask entity in; later census in)
- Create (by the tool): `DATA/Terrain_0.ttile` .. `Terrain_255.ttile` (only tiles with holes)
- Create: `ADDON/docs/superpowers/results/<run-date>-plan2-task2-holes.md`, copies of the mask and census files next to it

**Interfaces:**
- Consumes: Task 1 mask format and CLI; `MUD_CutBoxEntity.GetBox(out vector mins, out vector maxs)` (local space); `ChimeraWorldUtils.TryGetWaterSurfaceSimple(BaseWorld world, vector inPoint)` (pak `scripts/Game/generated/ChimeraWorldUtils.c`); `FileIO.OpenFile(string, FileMode.WRITE) -> FileHandle`, `FileHandle.WriteLine(string)`, `.Close()` (pattern `SCR_AutotestPrinter.c`); `TraceFlags.ENTS`, `world.TraceMove(TraceParam, filter)` with `bool filter(notnull IEntity e)`; `RoadEntity` type (`SCR_PreviewEntityEditorUIComponent.c`: `trace.TraceEnt.Type() == RoadEntity`).
- Produces: `$logs:mud_hole_mask.txt` (Task 1 format, plus `MUD mask 1` and `MUD mask control x <x> z <z> road <0|1> water <0|1>`); `$logs:mud_census.txt` (census format, all runs); holed Arland tiles in `DATA`. Task 3 relies on: the holed map, and hole cells = `GetSurfaceY < -255`.

- [x] **Step 1: Create the scan entity**

Create `ADDON/Scripts/Game/Mud/MUD_HoleMask.c`:

```c
[EntityEditorProps(category: "Mud", description: "Mud plan 2: in play, writes the keep mask of the whole terrain (cells that stay real terrain: roads, water, MUD_CutBoxEntity boxes) to $logs:<file> for Tools/mud_holes.py. Place it ON A ROAD (control sample). Run only on the map without our holes.", color: "0 255 128 255")]
class MUD_HoleMaskEntityClass : GenericEntityClass
{
}

//! Keep mask scan (mud plan 2, task 2). Samples every heightmap vertex and every cell centre:
//! road = an ENTS trace that accepts only RoadEntity hits something (vanilla
//! SCR_PreviewEntityEditorUIComponent tells roads apart by TraceEnt.Type() == RoadEntity);
//! water = the point m_fWaterLift above the terrain is under water (TryGetWaterSurfaceSimple).
//! A cell is kept when any of its 4 corners or its centre is road or water, or its centre is inside
//! a MUD_CutBoxEntity footprint. Lines in $logs:<m_sFile> (= the session's log folder):
//!   MUD mask 1
//!   MUD mask control x <x> z <z> road <0|1> water <0|1>
//!   MUD mask box <x0> <z0> cells <nx> <nz> cell <size>
//!   MUD mask keep z <cz> x <cx0> <cx1>        (inclusive, cells from the terrain box corner)
//!   MUD mask done keep <n> runs <r> road <a> water <b> box <c> ms <t>
//! No done line = scan failed or stopped early; Tools/mud_holes.py refuses such a file.
class MUD_HoleMaskEntity : GenericEntity
{
	static const int ROAD = 1;
	static const int WATER = 2;
	static const int BOX = 4;

	[Attribute("5", UIWidgets.EditBox, "Seconds after start before the scan runs")]
	protected float m_fDelay;

	[Attribute("2", UIWidgets.EditBox, "Heightmap cell size (m)")]
	protected float m_fCell;

	[Attribute("30", UIWidgets.EditBox, "Scan work per frame (ms), at least one cell row")]
	protected int m_iBudgetMs;

	[Attribute("0.05", UIWidgets.EditBox, "Water test point height above the terrain (m)")]
	protected float m_fWaterLift;

	[Attribute("mud_hole_mask.txt", UIWidgets.EditBox, "Output file in the session's log folder ($logs:)")]
	protected string m_sFile;

	protected float m_fTime;
	protected int m_iState;             // 0 waiting, 1 scanning, 2 done or failed
	protected int m_iRow;
	protected int m_iNX;
	protected int m_iNZ;
	protected float m_fX0;
	protected float m_fZ0;
	protected float m_fXMax;
	protected float m_fZMax;
	protected float m_fTop;
	protected float m_fBottom;
	protected ref array<int> m_aPrev = {};     // flags of vertex row m_iRow
	protected ref array<int> m_aNext = {};     // flags of vertex row m_iRow + 1
	protected ref array<MUD_CutBoxEntity> m_aBoxes = {};
	protected ref TraceParam m_Trace;
	protected FileHandle m_File;
	protected int m_iKeep;
	protected int m_iRuns;
	protected int m_iRoad;
	protected int m_iWater;
	protected int m_iBox;
	protected int m_iWorkMs;

	//------------------------------------------------------------------------------------------------
	void MUD_HoleMaskEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	void ~MUD_HoleMaskEntity()
	{
		if (m_File)
			m_File.Close();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		m_fTime += timeSlice;
		if (m_iState == 0)
		{
			if (m_fTime >= m_fDelay)
				Begin();
			return;
		}

		if (m_iState != 1)
			return;

		int t0 = System.GetTickCount();
		while (m_iRow < m_iNZ)
		{
			if (!ScanRow(m_iRow))
			{
				Fail(string.Format("terrain hole near cell row %1 - scan the map WITHOUT our holes (vanilla tiles)", m_iRow));
				return;
			}
			m_iRow++;
			if (m_iRow % 256 == 0)
				Print(string.Format("MUD mask: row %1 / %2, keep %3", m_iRow, m_iNZ, m_iKeep));
			if (System.GetTickCount(t0) >= m_iBudgetMs)
				break;
		}
		m_iWorkMs += System.GetTickCount(t0);

		if (m_iRow < m_iNZ)
			return;

		m_iState = 2;
		string done = string.Format("MUD mask done keep %1 runs %2 road %3 water %4 box %5 ms %6", m_iKeep, m_iRuns, m_iRoad, m_iWater, m_iBox, m_iWorkMs);
		m_File.WriteLine(done);
		m_File.Close();
		m_File = null;
		Print(done);
		if (m_iRoad == 0)
			Print("MUD mask: NO road cells - the RoadEntity trace found nothing (plan 2 task 2 gate fails)", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected void Begin()
	{
		m_iState = 2;
		vector pos = GetOrigin();
		GenericTerrainEntity terrain;
		GenericWorldEntity worldEnt = GetGame().GetWorldEntity();
		if (worldEnt)
			terrain = worldEnt.GetTerrain(pos[0], pos[2]);
		if (!terrain)
		{
			Print("MUD mask: no terrain under the mask entity", LogLevel.WARNING);
			return;
		}

		vector mins, maxs;
		terrain.GetTerrainBoundBox(mins, maxs);
		m_fX0 = mins[0];
		m_fZ0 = mins[2];
		// samples stay 1 cm inside the far edges (a WORLD trace exactly on x = 4096 missed, 2026-10-08)
		m_fXMax = maxs[0] - 0.01;
		m_fZMax = maxs[2] - 0.01;
		m_fTop = maxs[1] + 50;
		m_fBottom = mins[1] - 10;
		m_iNX = Math.Round((maxs[0] - mins[0]) / m_fCell);
		m_iNZ = Math.Round((maxs[2] - mins[2]) / m_fCell);
		m_Trace = new TraceParam();

		// keep boxes: the callback keeps only our boxes (a whole-map query passes every tree through it)
		GetWorld().QueryEntitiesByAABB(mins - Vector(0, 50, 0), maxs + Vector(0, 50, 0), AddBox);

		m_File = FileIO.OpenFile("$logs:" + m_sFile, FileMode.WRITE);
		if (!m_File)
		{
			Print("MUD mask: cannot open $logs:" + m_sFile, LogLevel.ERROR);
			return;
		}

		// control: this entity is placed on a road, so road must be 1 here
		int ctl = Sample(pos[0], pos[2]);
		int ctlRoad = 0;
		int ctlWater = 0;
		if (ctl > 0 && (ctl & ROAD))
			ctlRoad = 1;
		if (ctl > 0 && (ctl & WATER))
			ctlWater = 1;
		string control = string.Format("MUD mask control x %1 z %2 road %3 water %4", pos[0], pos[2], ctlRoad, ctlWater);
		string box = string.Format("MUD mask box %1 %2 cells %3 %4 cell %5", m_fX0, m_fZ0, m_iNX, m_iNZ, m_fCell);
		m_File.WriteLine("MUD mask 1");
		m_File.WriteLine(control);
		m_File.WriteLine(box);
		Print(control);
		Print(box);
		Print(string.Format("MUD mask: %1 keep boxes, writing $logs:%2", m_aBoxes.Count(), m_sFile));

		m_aPrev.Clear();
		m_aNext.Clear();
		for (int i = 0; i <= m_iNX; i++)
		{
			m_aPrev.Insert(0);
			m_aNext.Insert(0);
		}
		if (!SampleVertexRow(0, m_aPrev))
		{
			Fail("terrain hole in vertex row 0 - scan the map WITHOUT our holes (vanilla tiles)");
			return;
		}
		m_iState = 1;
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddBox(IEntity e)
	{
		MUD_CutBoxEntity b = MUD_CutBoxEntity.Cast(e);
		if (b)
			m_aBoxes.Insert(b);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool RoadOnly(notnull IEntity e)
	{
		return e.Type() == RoadEntity;
	}

	//------------------------------------------------------------------------------------------------
	//! ROAD | WATER flags at a world XZ; -1 if the terrain already has a hole there
	protected int Sample(float x, float z)
	{
		x = Math.Min(x, m_fXMax);
		z = Math.Min(z, m_fZMax);
		BaseWorld world = GetWorld();
		float h = world.GetSurfaceY(x, z);
		if (h < -255)
			return -1;

		int f = 0;
		if (ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, Vector(x, h + m_fWaterLift, z)))
			f |= WATER;

		m_Trace.Start = Vector(x, m_fTop, z);
		m_Trace.End = Vector(x, m_fBottom, z);
		m_Trace.Flags = TraceFlags.ENTS;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		if (world.TraceMove(m_Trace, RoadOnly) < 1)
			f |= ROAD;
		return f;
	}

	//------------------------------------------------------------------------------------------------
	protected bool SampleVertexRow(int vz, array<int> flags)
	{
		float z = m_fZ0 + vz * m_fCell;
		for (int vx = 0; vx <= m_iNX; vx++)
		{
			int f = Sample(m_fX0 + vx * m_fCell, z);
			if (f < 0)
				return false;
			flags[vx] = f;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool InBox(float x, float z)
	{
		foreach (MUD_CutBoxEntity b : m_aBoxes)
		{
			if (!b)
				continue;
			vector mins, maxs;
			b.GetBox(mins, maxs);
			vector l = b.CoordToLocal(Vector(x, b.GetOrigin()[1], z));
			if (l[0] >= mins[0] && l[0] <= maxs[0] && l[2] >= mins[2] && l[2] <= maxs[2])
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! One cell row: needs vertex rows cz (m_aPrev) and cz + 1 (sampled here into m_aNext)
	protected bool ScanRow(int cz)
	{
		if (!SampleVertexRow(cz + 1, m_aNext))
			return false;

		float zc = m_fZ0 + (cz + 0.5) * m_fCell;
		int start = -1;
		for (int cx = 0; cx < m_iNX; cx++)
		{
			float xc = m_fX0 + (cx + 0.5) * m_fCell;
			int c = Sample(xc, zc);
			if (c < 0)
				return false;

			int f = c | m_aPrev[cx] | m_aPrev[cx + 1] | m_aNext[cx] | m_aNext[cx + 1];
			if (!m_aBoxes.IsEmpty() && InBox(xc, zc))
				f |= BOX;

			if (f != 0)
			{
				m_iKeep++;
				if (f & ROAD)
					m_iRoad++;
				if (f & WATER)
					m_iWater++;
				if (f & BOX)
					m_iBox++;
				if (start < 0)
					start = cx;
				continue;
			}

			if (start >= 0)
			{
				EmitRun(cz, start, cx - 1);
				start = -1;
			}
		}

		if (start >= 0)
			EmitRun(cz, start, m_iNX - 1);

		// this row's top vertex row is the next row's bottom one
		m_aPrev.Copy(m_aNext);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void EmitRun(int cz, int cx0, int cx1)
	{
		m_iRuns++;
		m_File.WriteLine(string.Format("MUD mask keep z %1 x %2 %3", cz, cx0, cx1));
	}

	//------------------------------------------------------------------------------------------------
	protected void Fail(string why)
	{
		m_iState = 2;
		Print("MUD mask: FAILED, " + why, LogLevel.ERROR);
		if (m_File)
		{
			m_File.Close();     // no done line: Tools/mud_holes.py refuses the file
			m_File = null;
		}
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(float timeSlice)
	{
		Shape.CreateSphere(ARGB(255, 0, 255, 128), ShapeFlags.WIREFRAME | ShapeFlags.ONCE, GetOrigin(), 2);
		super._WB_AfterWorldUpdate(timeSlice);
	}
#endif
}
```

- [x] **Step 2: Census file output**

In `ADDON/Scripts/Game/Mud/MUD_HoleCensus.c`:

a) After the `m_iMaxRuns` attribute add:
```c
	[Attribute("", UIWidgets.EditBox, "If set: all lines go to this file in the session's log folder ($logs:), never truncated")]
	protected string m_sFile;
```
and after `protected int m_iWorkMs;` add:
```c
	protected FileHandle m_File;
```

b) In `Begin()`, replace
```c
		Print(string.Format("MUD census box %1 .. %2 cells %3 x %4 cell %5", mins, maxs, m_iNX, m_iNZ, m_fCell));
		m_iState = 1;
```
with
```c
		string box = string.Format("MUD census box %1 .. %2 cells %3 x %4 cell %5", mins, maxs, m_iNX, m_iNZ, m_fCell);
		Print(box);
		if (!m_sFile.IsEmpty())
		{
			m_File = FileIO.OpenFile("$logs:" + m_sFile, FileMode.WRITE);
			if (!m_File)
			{
				Print("MUD census: cannot open $logs:" + m_sFile, LogLevel.ERROR);
				return;
			}
			m_File.WriteLine(box);
		}
		m_iState = 1;
```

c) In `EOnFrame`, replace
```c
		m_iState = 2;
		int truncated = 0;
		if (m_iRuns > m_iMaxRuns)
			truncated = 1;
		Print(string.Format("MUD census done holes %1 runs %2 ms %3 truncated %4", m_iHoles, m_iRuns, m_iWorkMs, truncated));
```
with
```c
		m_iState = 2;
		int truncated = 0;
		if (!m_File && m_iRuns > m_iMaxRuns)
			truncated = 1;
		string done = string.Format("MUD census done holes %1 runs %2 ms %3 truncated %4", m_iHoles, m_iRuns, m_iWorkMs, truncated);
		Print(done);
		if (m_File)
		{
			m_File.WriteLine(done);
			m_File.Close();
			m_File = null;
		}
```

d) Replace the body of `EmitRun` with
```c
		m_iRuns++;
		string run = string.Format("MUD census run z %1 x %2 %3", cz, cx0, cx1);
		if (m_File)
			m_File.WriteLine(run);
		else if (m_iRuns <= m_iMaxRuns)
			Print(run);
```

e) Add a destructor after the constructor:
```c
	//------------------------------------------------------------------------------------------------
	void ~MUD_HoleCensusEntity()
	{
		if (m_File)
			m_File.Close();
	}
```

- [x] **Step 3: Compile**

Ask the user to open `MUD_HoleMask.c` and `MUD_HoleCensus.c` in the Script Editor and press Ctrl+S, then F7.
Expected: no compile errors in the Workbench log. If a line errors, fix that line only and ledger a ruling.

- [x] **Step 4: Check Arland is vanilla and put the mask entity in the layer**

Run: `ls "ADDON/worlds/Arland/Terrain/.Data" 2>&1`
Expected: `No such file or directory` or an empty folder (the gate 1 override tiles were deleted). If tiles exist: stop; they would make the scan fail (Review Focus 2).

With the world closed in Workbench, in `ADDON/Arlnd_Layers/default.layer` replace the whole first block `MUD_WorldSkinEntity { ... }` with:
```
MUD_HoleMaskEntity {
 coords 1289.1 39 2195.223
}
```
Then ask the user to open `Arlnd.ent` and drag the green mask sphere onto the middle of a tarmac road (any), save, and tell the position. Record it.
Expected: the layer file holds `MUD_HoleMaskEntity` with the user's road coords; the house and the M923A1 blocks are unchanged.

- [x] **Step 5: Run the scan**

`wb_play`, ask the user to focus the game window and wait for the `MUD mask done` line (work time unmeasured - estimate 20-60 s at 30 ms per frame; progress every 256 rows), then `wb_stop`.
Expected in `console.log`: `MUD mask control x .. z .. road 1 water 0`, `MUD mask box 0 0 cells 2048 2048 cell 2`, 8 progress lines, `MUD mask done keep <n> runs <r> road <a> water <b> box 0 ms <t>` with `a > 0` and `b > 0`, and no `FAILED`.

- [x] **Step 6: Gate - roads and water found**

Copy `$logs:mud_hole_mask.txt` (newest log folder) to `ADDON/docs/superpowers/results/<run-date>-mask.txt`, then run:
`python -I Tools/mud_holes.py plan docs/superpowers/results/<run-date>-mask.txt --margin 1`
Expected: `grid 2048 x 2048 = 4194304 cells, margin 1: keep K + holes H = 4194304 (ok), B blocks in T tiles`.
Gate: control `road 1`, done `road > 0`, `water > 0`. If `road 0` or control `road 0`: STOP - roads have no traceable collider; re-plan the road part to the edit-time plugin (spec 4.1). If `water 0`: STOP and re-plan water (try `TryGetWaterSurface` with the out params).

- [x] **Step 7: Picture check of the mask**

Run (from `ADDON`):
```bash
python -I - docs/superpowers/results/<run-date>-mask.txt <<'EOF'
import sys, zlib, struct
sys.path.insert(0, 'Tools'); import mud_holes as mh
m = mh.parse_mask(open(sys.argv[1]).read()); nx, nz = m['nx'], m['nz']
keep = mh.dilate(m['keep'], nx, nz, 1)
rows = b''.join(b'\0' + bytes(255 if keep[(nz - 1 - z) * nx + x] else 0 for x in range(nx)) for z in range(nz))
png = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
out = sys.argv[1].replace('.txt', '.png')
open(out, 'wb').write(b'\x89PNG\r\n\x1a\n' + png(b'IHDR', struct.pack('>IIBBBBB', nx, nz, 8, 0, 0, 0, 0)) + png(b'IDAT', zlib.compress(rows)) + png(b'IEND', b''))
print(out)
EOF
```
Open the PNG (white = kept, north up). Expected: the Arland road network and the sea / lakes / rivers in white, fields and forests black. Show it to the user. A road the user knows is missing = Review Focus 1, record which.

- [x] **Step 8: Write the holes**

Ask the user to close `Arlnd.ent` in Workbench (a tile write under an open world is lost). Then run:
```bash
python -I Tools/mud_holes.py write docs/superpowers/results/<run-date>-mask.txt --margin 1 --out "worlds/Arland/Terrain/.Data" --backup "C:/Users/Steffen/Documents/My Games/ArmaReforgerWorkbench/mud_backups/tiles" --bk "C:/Users/Steffen/Documents/A_documents/Github/blender-mcp-BK"
```
Expected: one `Terrain_<i>.ttile blocks <b> hole cells <c>` line per tile, then `wrote T tiles, hole cells H (ok)` with `T` and `H` equal to step 6. Paste all lines into the results file (the whole per-tile table).

- [x] **Step 9: Census of the holed map**

In `default.layer` replace the `MUD_HoleMaskEntity { ... }` block with:
```
MUD_HoleCensusEntity {
 coords 1289.1 39 2195.223
 m_sFile "mud_census.txt"
}
```
Ask the user to open `Arlnd.ent` (no restart needed, gate 1), `wb_play`, focus the game window, wait for `MUD census done`, `wb_stop`.
Expected: `MUD census done holes H runs <r> ms <t> truncated 0` with `H` = step 8.

- [x] **Step 10: Verify every hole**

Copy `$logs:mud_census.txt` to `docs/superpowers/results/<run-date>-census.txt`, then run:
`python -I Tools/mud_holes.py verify docs/superpowers/results/<run-date>-census.txt docs/superpowers/results/<run-date>-mask.txt --margin 1`
Expected: `expected holes H observed H missing 0 extra 0`, `VERIFY OK`.
If `missing > 0` with whole tiles missing: the owner tile formula is wrong for those blocks - record the cells, rule, and fix `owner_tile` with a pinned test.

- [x] **Step 11: Physical check (no skin yet) and record**

`wb_play` the census layer again. Ask the user to walk off a road onto a field (falls through = holes work) and to walk along a dirt path and a tarmac road (does not fall = roads kept). Then `wb_stop`.
Write `docs/superpowers/results/<run-date>-plan2-task2-holes.md`: date, build, mask entity coords, the `control`/`box`/`done` lines, step 6 plan line, the PNG, step 8 per-tile table with its total, the census done line, the verify lines, the user's walk results.
Expected: checksum line in the file: `keep K + holes H = 4194304`, `sum of per-tile hole cells = H = census holes`.

---

### Task 3: Coarse skin fills the holes

**Files:**
- Create: `ADDON/Scripts/Game/Mud/MUD_CoarseSkin.c`
- Modify: `ADDON/Scripts/Game/Mud/MUD_BuildingDestructionFix.c:10` (skip the new chunk class)
- Modify: `ADDON/Arlnd_Layers/default.layer` (census out, skin in)
- Create: `ADDON/docs/superpowers/results/<run-date>-plan2-task3-skin.md`

**Interfaces:**
- Consumes: Task 2's holed tiles (hole cell = `GetSurfaceY < -255`), census hole count `H`; patterns from `MUD_WorldSkin.c` (WORLD-trace heights, diagonal pick, MeshObject build) and `MUD_GroundEntity.c` (`MakeStatic` 1-6 geoms, per-material split, layers).
- Produces: `MUD_CoarseSkinEntity` (one per world) and `MUD_CoarseChunkEntity` (one per 64 m chunk with holes). Log lines:
  `MUD coarse: built <c> chunks with holes + <e> without = <total> | hole cells <h>, tris <2h> | via <init|first frame|frames>`,
  `MUD coarse: work <ms> ms (heights .., meshes .., colliders ..), wall <ms> ms, <f> frames, worst frame <ms> ms`,
  `MUD coarse: trace misses <m>, cells joined to slot 0 (> 6 surfaces) <j>, mem +<kb> KB`,
  `MUD coarse check: <n> hole cells, skin hit <a>, within 5 cm <b>, worst <w> m`.
  Plan 3 relies on `MUD_CoarseChunkEntity` and the per-chunk cell slot layout (`-1` = not a hole).

- [x] **Step 1: Create the skin entity**

Create `ADDON/Scripts/Game/Mud/MUD_CoarseSkin.c`:

```c
[EntityEditorProps(category: "Mud", description: "Mud plan 2: fills every terrain hole cell of the whole map with the exact 2 m heightmap triangles (+1 mm). Collider on every machine, terrain-look mesh on clients. Place by class anywhere, one per world.", color: "0 200 255 255")]
class MUD_CoarseSkinEntityClass : GenericEntityClass
{
}

//! One 64 m skin chunk. Own class so MUD_BuildingDestructionFix keeps it out of a ruin's sink-and-delete query.
class MUD_CoarseChunkEntityClass : GenericEntityClass
{
}

class MUD_CoarseChunkEntity : GenericEntity
{
}

//! Scratch buffers for one chunk (static arrays live in a helper class: script class members max 256 KB)
class MUD_CoarseSkinBuffers
{
	vector m_aVerts[1089];
	float m_aUVs[2178];
	int m_aIndices[6144];       // mesh: hole cells real, the rest zero-area (all 6 on one vertex)
	int m_aSplitIdx[6144];      // collider: hole triangles of one material
	int m_aCellSlot[1024];      // material slot of each cell, -1 = not a hole
}

//! Coarse skin (mud plan 2, task 3; spec 5.1). Tools/mud_holes.py holes the map everywhere except
//! roads and water; this entity puts the ground back in every hole cell, so the map plays as before.
//! Plan 3's fine deformable patches will replace skin cells where the ground moves.
//! Per 64 m chunk (32 x 32 cells at 2 m): hole cell = GetSurfaceY < -255 at its centre (-256 in a
//! hole, proven 2026-10-08); heights from a WORLD trace (still hits the heightmap inside holes);
//! one extra trace per hole cell picks its diagonal and its game material. A chunk without holes
//! costs one GetSurfaceY per cell and nothing else.
//! Same skin on every machine from the same terrain: no replication. Dedicated server: colliders only.
class MUD_CoarseSkinEntity : GenericEntity
{
	static const int CELLS = 32;
	static const int VERTS = 33;
	static const int MAT_SLOTS = 6;     // MakeStatic takes 1-6 geoms

	[Attribute("2", UIWidgets.EditBox, "Heightmap cell size (m). A chunk is 32 x 32 cells.", category: "Skin")]
	protected float m_fCell;

	[Attribute("{D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat", UIWidgets.ResourcePickerThumbnail, "Mesh material (terrain look)", "emat", category: "Skin")]
	protected ResourceName m_Material;

	[Attribute("{C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat", UIWidgets.ResourcePickerThumbnail, "Collider game material where the trace finds none", "gamemat", category: "Skin")]
	protected ResourceName m_GameMaterial;

	[Attribute("0.001", UIWidgets.EditBox, "Height above the heightmap (m). Grass clutter shows on our ground at +1 mm (2026-10-08).", category: "Skin")]
	protected float m_fOffset;

	[Attribute("1", UIWidgets.CheckBox, "Build the whole skin in EOnInit, before the first physics step: one freeze at load, nothing placed falls through. Off = spread over frames (Budget).", category: "Build")]
	protected bool m_bBuildAtInit;

	[Attribute("8", UIWidgets.EditBox, "Spread build: work per frame (ms), at least one chunk per frame", category: "Build")]
	protected int m_iBudgetMs;

	[Attribute("0", UIWidgets.CheckBox, "Colliders only, no meshes (what a dedicated server builds)", category: "Build")]
	protected bool m_bNoRender;

	[Attribute("2000", UIWidgets.EditBox, "After the build: hole cells checked by a trace against the skin (0 = no check)", category: "Check")]
	protected int m_iCheckCells;

	protected ref MUD_CoarseSkinBuffers m_Buf;
	protected ref array<vector> m_aTodo = {};       // chunk origins (world XZ) not built yet
	protected ref array<IEntity> m_aChunks = {};
	protected ref array<ref Resource> m_aMeshRes = {};
	protected ref array<ResourceName> m_aChunkMats = {};
	protected ref TraceParam m_Trace;
	protected bool m_bRender;
	protected bool m_bStarted;
	protected bool m_bDone;
	protected bool m_bLastHit;
	protected float m_fChunk;
	protected float m_fX0;
	protected float m_fZ0;
	protected float m_fXMax;
	protected float m_fZMax;
	protected float m_fTop;
	protected float m_fBottom;
	protected int m_iNCX;               // cells across the terrain
	protected int m_iNCZ;
	protected int m_iTotal;
	protected int m_iEmpty;
	protected int m_iHoleCells;
	protected int m_iMiss;
	protected int m_iJoined;
	protected int m_iFrames;
	protected int m_iWorstMs;
	protected int m_iWorkMs;
	protected int m_iHeightMs;
	protected int m_iMeshMs;
	protected int m_iColMs;
	protected int m_iStartTick;
	protected int m_iMemStartKB;

	//------------------------------------------------------------------------------------------------
	void MUD_CoarseSkinEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	void ~MUD_CoarseSkinEntity()
	{
		foreach (IEntity e : m_aChunks)
		{
			if (e)
				delete e;
		}
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame().InPlayMode())
			return;     // never in the World Editor's edit mode
		if (!Start())
			return;     // no terrain yet: EOnFrame retries
		if (m_bBuildAtInit)
			BuildAll("init");
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (m_bDone)
			return;

		if (!m_bStarted)
		{
			if (!GetGame().InPlayMode() || !Start())
				return;
			if (m_bBuildAtInit)
			{
				BuildAll("first frame");
				return;
			}
		}

		m_iFrames++;
		int t0 = System.GetTickCount();
		while (!m_aTodo.IsEmpty())
		{
			BuildNext();
			if (System.GetTickCount(t0) >= m_iBudgetMs)
				break;
		}
		int ms = System.GetTickCount(t0);
		m_iWorkMs += ms;
		if (ms > m_iWorstMs)
			m_iWorstMs = ms;

		if (m_aTodo.IsEmpty())
			Finish("frames");
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildAll(string how)
	{
		int t0 = System.GetTickCount();
		while (!m_aTodo.IsEmpty())
		{
			BuildNext();
		}
		m_iWorkMs = System.GetTickCount(t0);
		m_iWorstMs = m_iWorkMs;
		m_iFrames = 1;
		Finish(how);
	}

	//------------------------------------------------------------------------------------------------
	//! List the chunks. False when there is no terrain yet.
	protected bool Start()
	{
		vector pos = GetOrigin();
		GenericTerrainEntity terrain;
		GenericWorldEntity worldEnt = GetGame().GetWorldEntity();
		if (worldEnt)
			terrain = worldEnt.GetTerrain(pos[0], pos[2]);
		if (!terrain)
			return false;

		m_bStarted = true;
		m_bRender = RplSession.Mode() != RplMode.Dedicated && !m_bNoRender;
		m_fChunk = CELLS * m_fCell;
		m_Buf = new MUD_CoarseSkinBuffers();
		m_Trace = new TraceParam();
		BuildUVs();

		vector mins, maxs;
		terrain.GetTerrainBoundBox(mins, maxs);
		m_fX0 = mins[0];
		m_fZ0 = mins[2];
		// samples stay 1 cm inside the far edges (a WORLD trace exactly on x = 4096 missed, 2026-10-08)
		m_fXMax = maxs[0] - 0.01;
		m_fZMax = maxs[2] - 0.01;
		m_fTop = maxs[1] + 10;
		m_fBottom = mins[1] - 10;
		m_iNCX = Math.Round((maxs[0] - mins[0]) / m_fCell);
		m_iNCZ = Math.Round((maxs[2] - mins[2]) / m_fCell);
		int nx = Math.Ceil(m_iNCX / 32.0);
		int nz = Math.Ceil(m_iNCZ / 32.0);
		for (int cz = 0; cz < nz; cz++)
		{
			for (int cx = 0; cx < nx; cx++)
			{
				m_aTodo.Insert(Vector(m_fX0 + cx * m_fChunk, 0, m_fZ0 + cz * m_fChunk));
			}
		}
		m_iTotal = m_aTodo.Count();
		m_iStartTick = System.GetTickCount();
		m_iMemStartKB = System.MemoryAllocationKB();
		Print(string.Format("MUD coarse: terrain %1 .. %2, %3 x %4 chunks of %5 m, render %6, build at init %7, budget %8 ms",
			mins, maxs, nx, nz, m_fChunk, m_bRender, m_bBuildAtInit, m_iBudgetMs));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildUVs()
	{
		for (int vz = 0; vz < VERTS; vz++)
		{
			for (int vx = 0; vx < VERTS; vx++)
			{
				int v = vz * VERTS + vx;
				m_Buf.m_aUVs[v * 2] = vx / 32.0;
				m_Buf.m_aUVs[v * 2 + 1] = 1 - vz / 32.0;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Heightmap height at a world XZ by WORLD trace (also inside holes). m_Trace keeps the hit
	//! (SurfaceProps) and m_bLastHit says whether there was one.
	protected float HeightAt(float x, float z)
	{
		x = Math.Min(x, m_fXMax);
		z = Math.Min(z, m_fZMax);
		m_Trace.Start = Vector(x, m_fTop, z);
		m_Trace.End = Vector(x, m_fBottom, z);
		m_Trace.Flags = TraceFlags.WORLD;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		float frac = GetWorld().TraceMove(m_Trace, null);
		m_bLastHit = frac < 1;
		if (!m_bLastHit)
		{
			m_iMiss++;
			return GetWorld().GetSurfaceY(x, z);
		}
		return m_fTop + (m_fBottom - m_fTop) * frac;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildNext()
	{
		int last = m_aTodo.Count() - 1;
		BuildChunk(m_aTodo[last]);
		m_aTodo.Remove(last);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildChunk(vector o)
	{
		BaseWorld world = GetWorld();
		int t0 = System.GetTickCount();

		int holes = 0;
		for (int hz = 0; hz < CELLS; hz++)
		{
			for (int hx = 0; hx < CELLS; hx++)
			{
				int hc = hz * CELLS + hx;
				m_Buf.m_aCellSlot[hc] = -1;
				if (world.GetSurfaceY(o[0] + (hx + 0.5) * m_fCell, o[2] + (hz + 0.5) * m_fCell) < -255)
				{
					m_Buf.m_aCellSlot[hc] = 0;
					holes++;
				}
			}
		}
		if (holes == 0)
		{
			m_iEmpty++;
			m_iHeightMs += System.GetTickCount(t0);
			return;
		}

		for (int vz = 0; vz < VERTS; vz++)
		{
			for (int vx = 0; vx < VERTS; vx++)
			{
				float lx = vx * m_fCell;
				float lz = vz * m_fCell;
				float h = HeightAt(o[0] + lx, o[2] + lz) + m_fOffset;
				m_Buf.m_aVerts[vz * VERTS + vx] = Vector(lx, h, lz);
			}
		}

		// per hole cell: diagonal whose midpoint is nearest the traced centre, and the centre's gamemat
		m_aChunkMats.Clear();
		int n;
		for (int z = 0; z < CELLS; z++)
		{
			for (int x = 0; x < CELLS; x++)
			{
				int c = z * CELLS + x;
				int bl = z * VERTS + x;
				int br = bl + 1;
				int tl = bl + VERTS;
				int tr = tl + 1;
				if (m_Buf.m_aCellSlot[c] < 0)
				{
					// not a hole: real terrain shows here, zero-area cell keeps the index count fixed
					for (int j = 0; j < 6; j++)
					{
						m_Buf.m_aIndices[n] = bl;
						n++;
					}
					continue;
				}

				float hm = HeightAt(o[0] + (x + 0.5) * m_fCell, o[2] + (z + 0.5) * m_fCell) + m_fOffset;
				ResourceName mat;
				if (m_bLastHit)
				{
					GameMaterial gm = m_Trace.SurfaceProps;
					if (gm)
						mat = gm.GetResourceName();
				}
				if (mat.IsEmpty())
					mat = m_GameMaterial;
				int slot = m_aChunkMats.Find(mat);
				if (slot < 0)
				{
					if (m_aChunkMats.Count() < MAT_SLOTS)
					{
						slot = m_aChunkMats.Count();
						m_aChunkMats.Insert(mat);
					}
					else
					{
						slot = 0;   // more than 6 surfaces in one chunk: the rest join slot 0
						m_iJoined++;
					}
				}
				m_Buf.m_aCellSlot[c] = slot;

				vector vbl = m_Buf.m_aVerts[bl];
				vector vbr = m_Buf.m_aVerts[br];
				vector vtl = m_Buf.m_aVerts[tl];
				vector vtr = m_Buf.m_aVerts[tr];
				float dA = Math.AbsFloat((vbl[1] + vtr[1]) * 0.5 - hm);
				float dB = Math.AbsFloat((vbr[1] + vtl[1]) * 0.5 - hm);
				if (dB < dA - 0.001)
				{
					// shared edge tl-br (99.93 % of non-flat Arland cells, 2026-10-08)
					m_Buf.m_aIndices[n] = bl; n++;
					m_Buf.m_aIndices[n] = tl; n++;
					m_Buf.m_aIndices[n] = br; n++;
					m_Buf.m_aIndices[n] = tl; n++;
					m_Buf.m_aIndices[n] = tr; n++;
					m_Buf.m_aIndices[n] = br; n++;
				}
				else
				{
					// shared edge bl-tr (also flat cells: both diagonals are the same there)
					m_Buf.m_aIndices[n] = bl; n++;
					m_Buf.m_aIndices[n] = tl; n++;
					m_Buf.m_aIndices[n] = tr; n++;
					m_Buf.m_aIndices[n] = tr; n++;
					m_Buf.m_aIndices[n] = br; n++;
					m_Buf.m_aIndices[n] = bl; n++;
				}
			}
		}
		m_iHoleCells += holes;
		m_iHeightMs += System.GetTickCount(t0);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = o;
		IEntity ent = GetGame().SpawnEntity(MUD_CoarseChunkEntity, world, params);
		if (!ent)
		{
			Print("MUD coarse: could not spawn chunk entity", LogLevel.ERROR);
			return;
		}
		m_aChunks.Insert(ent);

		if (m_bRender)
		{
			int t1 = System.GetTickCount();
			int numVerts[] = {1089};
			int numIndices[] = {6144};
			string materials[] = {m_Material};
			Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
			if (res && res.GetResource())
			{
				MeshObject mesh = res.GetResource().ToMeshObject();
				// indices first: UpdateVerts rebuilds normals + tangents from the triangle list
				mesh.UpdateIndices(0, m_Buf.m_aIndices);
				mesh.UpdateVerts(0, m_Buf.m_aVerts, m_Buf.m_aUVs);
				ent.SetObject(mesh, "");
				m_aMeshRes.Insert(res);
			}
			else
			{
				Print(string.Format("MUD coarse: MeshObject.Create failed - material '%1' did not load?", m_Material), LogLevel.ERROR);
				m_bRender = false;
			}
			m_iMeshMs += System.GetTickCount(t1);
		}

		int t2 = System.GetTickCount();
		array<PhysicsGeom> geoms = {};
		array<ResourceName> mats = {};
		array<int> layers = {};
		// same layers as the ground patches: Terrain preset + Foliage (vehicle get in/out sweep skips it)
		int layer = EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage;
		for (int k = 0; k < m_aChunkMats.Count(); k++)
		{
			int sn = 0;
			for (int sc = 0; sc < CELLS * CELLS; sc++)
			{
				if (m_Buf.m_aCellSlot[sc] != k)
					continue;
				for (int sj = 0; sj < 6; sj++)
				{
					m_Buf.m_aSplitIdx[sn] = m_Buf.m_aIndices[sc * 6 + sj];
					sn++;
				}
			}
			if (sn == 0)
				continue;
			PhysicsGeom geom = PhysicsGeom.CreateTriMesh(m_Buf.m_aVerts, m_Buf.m_aSplitIdx, 1089, sn);
			if (!geom)
			{
				Print("MUD coarse: CreateTriMesh failed", LogLevel.ERROR);
				continue;
			}
			geoms.Insert(geom);
			mats.Insert(m_aChunkMats[k]);
			layers.Insert(layer);
		}
		if (!geoms.IsEmpty() && !MakeStatic(ent, geoms, mats, layers))
			Print("MUD coarse: CreateStaticEx failed", LogLevel.ERROR);
		m_iColMs += System.GetTickCount(t2);
	}

	//------------------------------------------------------------------------------------------------
	//! Static body from 1-6 geoms. CreateStaticEx takes a fixed-size array and no shipped script
	//! passes a variable count, so one literal array per count (copy of MUD_GroundEntity.MakeStatic).
	protected Physics MakeStatic(IEntity ent, array<PhysicsGeom> g, array<ResourceName> m, array<int> l)
	{
		switch (g.Count())
		{
			case 1:
			{
				autoptr PhysicsGeomDef d1[] = {PhysicsGeomDef("", g[0], m[0], l[0])};
				return Physics.CreateStaticEx(ent, d1);
			}
			case 2:
			{
				autoptr PhysicsGeomDef d2[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1])};
				return Physics.CreateStaticEx(ent, d2);
			}
			case 3:
			{
				autoptr PhysicsGeomDef d3[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2])};
				return Physics.CreateStaticEx(ent, d3);
			}
			case 4:
			{
				autoptr PhysicsGeomDef d4[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2]), PhysicsGeomDef("", g[3], m[3], l[3])};
				return Physics.CreateStaticEx(ent, d4);
			}
			case 5:
			{
				autoptr PhysicsGeomDef d5[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2]), PhysicsGeomDef("", g[3], m[3], l[3]), PhysicsGeomDef("", g[4], m[4], l[4])};
				return Physics.CreateStaticEx(ent, d5);
			}
			case 6:
			{
				autoptr PhysicsGeomDef d6[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2]), PhysicsGeomDef("", g[3], m[3], l[3]), PhysicsGeomDef("", g[4], m[4], l[4]), PhysicsGeomDef("", g[5], m[5], l[5])};
				return Physics.CreateStaticEx(ent, d6);
			}
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish(string how)
	{
		m_bDone = true;
		int built = m_iTotal - m_iEmpty;
		Print(string.Format("MUD coarse: built %1 chunks with holes + %2 without = %3 | hole cells %4, tris %5 | via %6",
			built, m_iEmpty, m_iTotal, m_iHoleCells, m_iHoleCells * 2, how));
		Print(string.Format("MUD coarse: work %1 ms (heights %2, meshes %3, colliders %4), wall %5 ms, %6 frames, worst frame %7 ms",
			m_iWorkMs, m_iHeightMs, m_iMeshMs, m_iColMs, System.GetTickCount(m_iStartTick), m_iFrames, m_iWorstMs));
		Print(string.Format("MUD coarse: trace misses %1, cells joined to slot 0 (> 6 surfaces) %2, mem +%3 KB",
			m_iMiss, m_iJoined, System.MemoryAllocationKB() - m_iMemStartKB));
		if (m_iCheckCells > 0)
			Check();
	}

	//------------------------------------------------------------------------------------------------
	protected bool SkinOnly(notnull IEntity e)
	{
		return MUD_CoarseChunkEntity.Cast(e) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Trace the skin at hole cell centres spread over the map (odd stride over a power-of-two
	//! cell count visits every cell once) and compare with the heightmap height there.
	protected void Check()
	{
		BaseWorld world = GetWorld();
		int total = m_iNCX * m_iNCZ;
		int tested;
		int hit;
		int within;
		float worst;
		TraceParam p = new TraceParam();
		int idx = 0;
		for (int k = 0; k < total && tested < m_iCheckCells; k++)
		{
			idx = (idx + 104729) % total;   // stays below 2^31 (k * 104729 would not)
			float x = m_fX0 + (idx % m_iNCX + 0.5) * m_fCell;
			float z = m_fZ0 + (idx / m_iNCX + 0.5) * m_fCell;
			if (world.GetSurfaceY(x, z) > -255)
				continue;   // not a hole: real terrain
			tested++;

			float hW = HeightAt(x, z) + m_fOffset;
			p.Start = Vector(x, m_fTop, z);
			p.End = Vector(x, m_fBottom, z);
			p.Flags = TraceFlags.ENTS;
			p.LayerMask = EPhysicsLayerPresets.Projectile;
			float frac = world.TraceMove(p, SkinOnly);
			if (frac >= 1)
				continue;
			hit++;
			float err = Math.AbsFloat(m_fTop + (m_fBottom - m_fTop) * frac - hW);
			if (err < 0.05)
				within++;
			if (err > worst)
				worst = err;
		}
		Print(string.Format("MUD coarse check: %1 hole cells, skin hit %2, within 5 cm %3, worst %4 m", tested, hit, within, worst));
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(float timeSlice)
	{
		Shape.CreateSphere(ARGB(255, 0, 200, 255), ShapeFlags.WIREFRAME | ShapeFlags.ONCE, GetOrigin(), 2);
		super._WB_AfterWorldUpdate(timeSlice);
	}
#endif
}
```

- [x] **Step 2: Keep skin chunks out of ruin clean-up**

In `ADDON/Scripts/Game/Mud/MUD_BuildingDestructionFix.c` line 10, replace
```c
		if (MUD_GroundEntity.IsMudPatch(entity) || MUD_WorldSkinChunkEntity.Cast(entity))
```
with
```c
		if (MUD_GroundEntity.IsMudPatch(entity) || MUD_WorldSkinChunkEntity.Cast(entity) || MUD_CoarseChunkEntity.Cast(entity))
```

- [x] **Step 3: Compile**

Ask the user to open `MUD_CoarseSkin.c` and `MUD_BuildingDestructionFix.c` in the Script Editor, Ctrl+S, F7.
Expected: no compile errors.

- [x] **Step 4: Skin in the layer**

World closed. In `default.layer` replace the `MUD_HoleCensusEntity { ... }` block with:
```
MUD_CoarseSkinEntity {
 coords 1289.1 39 2195.223
}
```
Record the M923A1 block's `coords` line from the same file (its height before play).

- [x] **Step 5: Build at init (default)**

User opens `Arlnd.ent`; `wb_play`; focus; wait about 30 s; `wb_stop`.
Expected in `console.log`:
- `MUD coarse: built <c> chunks with holes + <e> without = 4096 | hole cells H, tris 2H | via init` (or `via first frame`, then ledger a ruling: no terrain at EOnInit), with `H` = Task 2 census holes.
- `trace misses 0`, `cells joined ... <j>` (record j).
- `MUD coarse check: 2000 hole cells, skin hit 2000, within 5 cm 2000, worst < 0.05 m`.
If `skin hit 0`: the Projectile mask does not see the skin layer; rule on the mask (try `EPhysicsLayerPresets.Character`) and rerun. If `worst` > 0.05 with hit = 2000: diagonal pick wrong in some cells - record, do not tune.

- [x] **Step 6: Play check at init build**

`wb_play` again. Ask the user to:
1. Watch the M923A1 for 10 s at start - it must not drop (Review Focus 4).
2. Walk across a field, a forest edge and a field-road border; drive the M923A1 off-road 200 m.
3. Look for cracks or flicker where holed ground meets a road or the shore (terrain LOD at distance: fly the camera up 300 m and look down).
4. Look at grass clutter on the skin.
`wb_stop`. Record each answer.
Expected: no falls, vehicle drives, clutter present. Cracks: record where and at what distance - they decide skirts in Plan 3, no fix here.

- [x] **Step 7: Spread build, tune the budget**

In `default.layer` set the skin block to:
```
MUD_CoarseSkinEntity {
 coords 1289.1 39 2195.223
 m_bBuildAtInit 0
 m_iBudgetMs 8
}
```
Play once each with `m_iBudgetMs` 4, 8 and 16 (edit the layer with the world closed between runs). Each run: wait for `via frames`, record the `work`, `wall`, `frames`, `worst frame` and `mem` numbers and whether the M923A1 dropped.
Expected: three rows; `worst frame` close to the budget plus one chunk (~3 ms per chunk measured at gate 2); `wall` falls as the budget rises.

- [x] **Step 8: Record and pick**

Write `docs/superpowers/results/<run-date>-plan2-task3-skin.md`: date, build, the init-build log lines, the check line, the play-check answers, the budget table (4 / 8 / 16 ms and init) with columns work / wall / frames / worst frame / mem / M923 dropped. Checksum line: `hole cells H (skin) = H (census)`, `chunks with holes + without = 4096`.
Ask the user to pick init build or a budget as the default; set the attribute default in `MUD_CoarseSkin.c` to the pick and leave the layer block at defaults (`coords` only).
Expected: the results file holds every row; the user's pick is written into the attribute default and the ledger.

- [x] **Step 9: Probe clean-up decision**

`MUD_WorldSkin.c` (gate 2 probe) is replaced by `MUD_CoarseSkin.c`; `MUD_HoleCensus.c` + `Tools/hole_census.py` + test stay (census is how every later hole write is verified). Ask the user before deleting `MUD_WorldSkin.c` (it still has the terrain-off experiment). Record the answer in the results file.
Expected: decision recorded; nothing deleted without a yes.
