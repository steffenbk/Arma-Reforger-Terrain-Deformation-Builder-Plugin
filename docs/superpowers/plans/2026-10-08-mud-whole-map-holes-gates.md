# Mud Whole-Map Holes - Gate Tests (Plan 1 of 2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove (or kill) the two premises the whole design stands on: (1) an addon can ship a changed Arland `.ttile` by path and its holes load in play; (2) a whole-Arland 2 m coarse skin is affordable (build time, memory, frame cost).

**Architecture:** Two throwaway in-engine probes plus one Python checker. Task 1 copies three Arland tiles out of the Experimental pak into the addon at the same path, writes known hole rectangles with the proven `Tools/ttile_holes.py`, and a census entity lists every hole cell of the whole terrain in the log; `Tools/hole_census.py` diffs that log against the writes under every tile-order / block-coordinate hypothesis and names the one that matches. Task 2 extends the existing `MUD_WorldSkin.c` probe to 2 m chunks with WORLD-trace heights, diagonal pick, memory and frame-time logging.

**Tech Stack:** Enforce Script (Arma Reforger Experimental 192624 Workbench), Python 3 stdlib + pytest, `blender-mcp-BK/tools/pakread.py`.

**Spec:** `docs/superpowers/specs/2026-10-08-mud-whole-map-holes-design.md` (sections 2, 4.2, 5.1, 7 steps 1-2).

**Scope:** spec build order steps 1-2 only. Steps 3-5 (hole tool, ground refactor, MP) get Plan 2, written after both gates pass, because its design depends on what these tasks measure (tile index order, local vs global block coords, 2 m vs 4 m far collider). If Task 1 fails, stop and re-plan (spec section 7.1).

Paths below: `ADDON` = `C:\Users\Steffen\Documents\My Games\ArmaReforgerWorkbench\addons\Testing plan vech msh`, `BK` = `C:\Users\Steffen\Documents\A_documents\Github\blender-mcp-BK`. Workbench log: `C:\Users\Steffen\Documents\My Games\ArmaReforgerWorkbench\logs\logs_<date>_<time>\console.log` (newest folder), script lines look like `22:07:44.899   SCRIPT       : MUD ...`.

## Global Constraints

- Target: Arma Reforger **Experimental 192624** only (Workbench "Experimental, port 5776"). Stable not supported (spec section 9).
- Game files are read from the Experimental pak (`pakread.py --install experimental`), never from the stale loose `addons/data` tree or `E:\Arma reforger data\extracted_files\`.
- Heightmap cell = **2 m**; Arland = 2049 x 2049 verts = 2048 x 2048 cells = 16 x 16 tiles of 128 cells; tile `.ttile` HGHT = 129 x 129 u16 = 33282 bytes, no header (measured 2026-10-08 from pak Terrain_0/1/16/255; an earlier "+2 bytes" note was wrong).
- Hole test in script: `GetSurfaceY(x, z) < -255` (returns -256 in a hole, proven 19:58).
- Heights in or near holes come from a `TraceFlags.WORLD` trace, never GetSurfaceY (spec 5.1).
- `.ttile` writes only with the world closed in Workbench, only through `Tools/ttile_holes.py` (round-trip assert before every write).
- No git (solo mod work, user rule). "Record" steps write into `docs/superpowers/results/`, with time and game build.
- Every in-engine check prints measurable log lines; the whole per-item table goes into the results file, with a checksum line (e.g. "holes N == cells in runs N").
- Throwaway probes (`MUD_HoleCensus.c`, `Tools/hole_census.py` + test, override tiles) are deleted when Plan 2 no longer needs them; the decision is noted in the results file.

## Review Focus

1. **HOLE block coords may be tile-local or global.** The test map is one tile, so local == global there; nothing proven yet. Expected: the checker reports exactly one match; Task 1 step 12 pins it as a test.
2. **Tile index order (`Terrain_<i>`) unknown** (x-fastest vs z-fastest). Expected: exactly one hypothesis matches; pinned by the three non-square rectangles in different tiles (Task 1 step 7 + checker tests).
3. **Vanilla Arland may already have holes** (bunkers, tunnels). Expected: the baseline census (Task 1 step 6) records them and the checker subtracts them; a baseline cell that vanishes after the override is reported as `lost` and fails the check (checker test `test_lost_baseline_cell_fails`).
4. **Workbench packer may skip `.ttile` files that have no `.meta`** (pak `.ttile`s have none: 1021 files = 765 edds + 256 ttile). Expected: the packed addon pak lists the three tiles; Task 1 step 14 checks the packed FILE table, so a loose-file-only success is not mistaken for "ships".
5. **A truncated or half-finished census** (play stopped early, run-line cap hit) would look like "no holes". Expected: the checker refuses a block with no `done` line or with `truncated 1`, and checks `holes` == cells in runs (checker tests `test_parse_raises_without_done`, `test_truncated_census_fails`).

---

### Task 1: Addon override test (spec 7.1)

**Files:**
- Create: `ADDON/Tools/hole_census.py` (checker)
- Create: `ADDON/Tools/test_hole_census.py` (pytest)
- Create: `ADDON/Scripts/Game/Mud/MUD_HoleCensus.c` (throwaway census entity)
- Create (copied from pak, then holed): `ADDON/worlds/Arland/Terrain/.Data/Terrain_1.ttile`, `Terrain_16.ttile`, `Terrain_18.ttile`
- Modify: `ADDON/Arlnd_Layers/default.layer` (swap the skin entity for the census entity)
- Create: `ADDON/docs/superpowers/results/<run-date>-gate1-override.md`

**Interfaces:**
- Consumes: `Tools/ttile_holes.py <ttile> add <cx0> <cz0> <cx1> <cz1>` (inclusive cells; block = cell // 32, as written) and `<ttile> show`; `BK/tools/pakread.py get <path> <outdir> --install experimental`.
- Produces: log lines
  `MUD census box <mins> .. <maxs> cells <nx> x <nz> cell <size>`,
  `MUD census run z <cz> x <cx0> <cx1>` (cells counted from the terrain box corner, inclusive),
  `MUD census done holes <n> runs <r> ms <t> truncated <0|1>`;
  Python `parse_census(text) -> dict(cells: set[(cx, cz)], size: (nx, nz), holes: int, truncated: bool)`,
  `expected_cells(writes, order, blocks, tiles=16, tile_cells=128) -> set[(cx, cz)]` with `writes` = list of `(tile, x0, z0, x1, z1)`, `(order, blocks)` one of `HYPOTHESES` = `("x-fastest","local")`, `("z-fastest","local")`, `("any","global")` (global block coords ignore the tile, so order cannot be told there),
  `check(observed, baseline, writes, **kw) -> dict(matches: list[str], new: set, lost: set)`.
  Plan 2 relies on the result line `MATCH x-fastest/local`, `MATCH z-fastest/local` or `MATCH global`.

- [ ] **Step 1: Write the failing checker tests**

Create `ADDON/Tools/test_hole_census.py`:

```python
import pytest

import hole_census as hc

WRITES = [(1, 32, 64, 35, 65), (16, 70, 10, 71, 13), (18, 100, 100, 102, 102)]


def census_log(cells, nx=2048, nz=2048, truncated=0, done=True, holes=None):
    """Fake console.log text in the census format, runs built from a cell set."""
    lines = ['22:00:00.000   SCRIPT       : MUD census box <0,0,0> .. <4096,300,4096> cells %d x %d cell 2' % (nx, nz)]
    runs = 0
    for cz in sorted({z for _, z in cells}):
        xs = sorted(x for x, z in cells if z == cz)
        start = prev = xs[0]
        for x in xs[1:] + [None]:
            if x is not None and x == prev + 1:
                prev = x
                continue
            lines.append('22:00:01.000   SCRIPT       : MUD census run z %d x %d %d' % (cz, start, prev))
            runs += 1
            if x is not None:
                start = prev = x
    if done:
        n = len(cells) if holes is None else holes
        lines.append('22:00:02.000   SCRIPT       : MUD census done holes %d runs %d ms 900 truncated %d' % (n, runs, truncated))
    return '\n'.join(lines) + '\n'


def test_parse_census_reads_runs_and_size():
    cells = {(5, 7), (6, 7), (7, 7), (9, 7), (0, 8)}
    got = hc.parse_census(census_log(cells))
    assert got['cells'] == cells
    assert got['size'] == (2048, 2048)
    assert got['holes'] == 5
    assert got['truncated'] is False


def test_parse_uses_last_block():
    text = census_log({(1, 1)}) + census_log({(2, 2), (3, 2)})
    assert hc.parse_census(text)['cells'] == {(2, 2), (3, 2)}


def test_parse_raises_without_done():
    with pytest.raises(ValueError, match='done'):
        hc.parse_census(census_log({(1, 1)}, done=False))


def test_parse_raises_when_count_disagrees():
    with pytest.raises(ValueError, match='checksum'):
        hc.parse_census(census_log({(1, 1), (2, 1)}, holes=3))


def test_expected_cells_x_fastest_local():
    # tile 17 with 16 tiles per side, x-fastest = tile column 1, row 1
    assert hc.expected_cells([(17, 0, 0, 1, 0)], 'x-fastest', 'local') == {(128, 128), (129, 128)}
    # tile 1, x-fastest = column 1, row 0
    assert hc.expected_cells([(1, 0, 0, 0, 0)], 'x-fastest', 'local') == {(128, 0)}


def test_expected_cells_z_fastest_local():
    # tile 1, z-fastest = column 0, row 1
    assert hc.expected_cells([(1, 0, 0, 0, 0)], 'z-fastest', 'local') == {(0, 128)}


def test_expected_cells_global_ignores_tile():
    assert hc.expected_cells([(18, 3, 4, 3, 4)], 'any', 'global') == {(3, 4)}


def test_hypotheses_are_distinct_for_the_real_writes():
    sets = [frozenset(hc.expected_cells(WRITES, o, b)) for o, b in hc.HYPOTHESES]
    assert len(set(sets)) == 3


def test_global_match_is_named_once():
    got = hc.check(hc.expected_cells(WRITES, 'any', 'global'), set(), WRITES)
    assert got['matches'] == ['global']


def test_check_names_the_single_match_and_ignores_baseline():
    baseline = {(2000, 2000), (2001, 2000)}
    observed = baseline | hc.expected_cells(WRITES, 'z-fastest', 'local')
    got = hc.check(observed, baseline, WRITES)
    assert got['matches'] == ['z-fastest/local']
    assert got['lost'] == set()


def test_lost_baseline_cell_fails(tmp_path):
    base = tmp_path / 'base.log'
    after = tmp_path / 'after.log'
    base.write_text(census_log({(2000, 2000)}))
    after.write_text(census_log(hc.expected_cells(WRITES, 'x-fastest', 'local')))
    args = [str(after), '--baseline', str(base)] + ['--write=%d:%d:%d:%d:%d' % w for w in WRITES]
    assert hc.main(args) == 1


def test_truncated_census_fails(tmp_path):
    after = tmp_path / 'after.log'
    after.write_text(census_log(hc.expected_cells(WRITES, 'x-fastest', 'local'), truncated=1))
    args = [str(after)] + ['--write=%d:%d:%d:%d:%d' % w for w in WRITES]
    assert hc.main(args) == 1


def test_main_passes_on_exact_match(tmp_path, capsys):
    after = tmp_path / 'after.log'
    after.write_text(census_log(hc.expected_cells(WRITES, 'x-fastest', 'local')))
    args = [str(after)] + ['--write=%d:%d:%d:%d:%d' % w for w in WRITES]
    assert hc.main(args) == 0
    assert 'MATCH x-fastest/local' in capsys.readouterr().out
```

- [ ] **Step 2: Run tests to verify they fail**

Run (from `ADDON`): `python -m pytest Tools/test_hole_census.py -q`
Expected: FAIL / collection error `ModuleNotFoundError: No module named 'hole_census'`.

- [ ] **Step 3: Write the checker**

Create `ADDON/Tools/hole_census.py`:

```python
"""THROWAWAY (mud plan 1, task 1): check a MUD census log against holes written into Arland tiles.

Reads the LAST census block of a Workbench console.log (MUD_HoleCensus.c), subtracts an optional
baseline census (vanilla holes), and compares the new hole cells with what we wrote, under every
tile-order / block-coordinate hypothesis. Exactly one hypothesis must match.

    python hole_census.py <console.log> [--baseline <console.log>] --write=T:x0:z0:x1:z1 [...]

T = tile index of Terrain_<T>.ttile; x0..x1, z0..z1 = the cells passed to ttile_holes.py add (inclusive).
"""
import argparse
import re
import sys

BOX = re.compile(r'MUD census box .* cells (\d+) x (\d+)')
RUN = re.compile(r'MUD census run z (\d+) x (\d+) (\d+)')
DONE = re.compile(r'MUD census done holes (\d+) runs (\d+) ms (\d+) truncated (\d)')

# global block coords ignore the tile, so tile order cannot be told apart there: one 'global' hypothesis
HYPOTHESES = [('x-fastest', 'local'), ('z-fastest', 'local'), ('any', 'global')]


def name(order, blocks):
    return 'global' if blocks == 'global' else '%s/local' % order


def parse_census(text):
    """Last census block -> dict(cells, size, holes, truncated). ValueError if incomplete or inconsistent."""
    lines = text.splitlines()
    start = None
    for i, line in enumerate(lines):
        if BOX.search(line):
            start = i
    if start is None:
        raise ValueError('no "MUD census box" line in the log')
    m = BOX.search(lines[start])
    size = (int(m[1]), int(m[2]))
    cells = set()
    done = None
    for line in lines[start + 1:]:
        r = RUN.search(line)
        if r:
            cz, a, b = int(r[1]), int(r[2]), int(r[3])
            cells.update((cx, cz) for cx in range(a, b + 1))
            continue
        d = DONE.search(line)
        if d:
            done = d
            break
    if done is None:
        raise ValueError('census block has no "done" line (play stopped early?)')
    holes, truncated = int(done[1]), done[4] == '1'
    if not truncated and holes != len(cells):
        raise ValueError('checksum: done line says %d holes, runs hold %d cells' % (holes, len(cells)))
    return {'cells': cells, 'size': size, 'holes': holes, 'truncated': truncated}


def expected_cells(writes, order, blocks, tiles=16, tile_cells=128):
    """World cells (from the terrain corner) that the writes should hole under one hypothesis."""
    out = set()
    for t, x0, z0, x1, z1 in writes:
        if order == 'x-fastest':
            tx, tz = t % tiles, t // tiles
        else:
            tx, tz = t // tiles, t % tiles
        ox, oz = (tx * tile_cells, tz * tile_cells) if blocks == 'local' else (0, 0)
        out.update((ox + x, oz + z) for z in range(z0, z1 + 1) for x in range(x0, x1 + 1))
    return out


def check(observed, baseline, writes, **kw):
    new = observed - baseline
    lost = baseline - observed
    matches = [name(o, b) for o, b in HYPOTHESES if expected_cells(writes, o, b, **kw) == new]
    return {'matches': matches, 'new': new, 'lost': lost}


def bbox(cells):
    xs = [x for x, _ in cells]
    zs = [z for _, z in cells]
    return 'x %d..%d z %d..%d' % (min(xs), max(xs), min(zs), max(zs))


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument('log')
    ap.add_argument('--baseline')
    ap.add_argument('--write', action='append', required=True)
    a = ap.parse_args(argv)

    writes = [tuple(int(v) for v in w.split(':')) for w in a.write]
    after = parse_census(open(a.log, encoding='utf-8', errors='replace').read())
    base = {'cells': set(), 'truncated': False, 'holes': 0}
    if a.baseline:
        base = parse_census(open(a.baseline, encoding='utf-8', errors='replace').read())
    print('after: %d x %d cells, %d holes, truncated %s' % (after['size'] + (after['holes'], after['truncated'])))
    print('baseline: %d holes, truncated %s' % (base['holes'], base['truncated']))
    if after['truncated'] or base['truncated']:
        print('FAIL: a census was truncated - raise m_iMaxRuns and rerun')
        return 1

    r = check(after['cells'], base['cells'], writes)
    print('new hole cells: %d%s' % (len(r['new']), ' (' + bbox(r['new']) + ')' if r['new'] else ''))
    for o, b in HYPOTHESES:
        exp = expected_cells(writes, o, b)
        print('  %s: expected %d, matched %d, extra %d, missing %d' % (
            name(o, b), len(exp), len(exp & r['new']), len(r['new'] - exp), len(exp - r['new'])))
    if r['lost']:
        print('FAIL: %d baseline hole cells are gone (%s)' % (len(r['lost']), bbox(r['lost'])))
        return 1
    if len(r['matches']) != 1:
        print('NO MATCH' if not r['matches'] else 'AMBIGUOUS: ' + ', '.join(r['matches']))
        return 1
    print('MATCH ' + r['matches'][0])
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `python -m pytest Tools/test_hole_census.py -q`
Expected: `13 passed` (verified when the plan was written, 2026-10-08).

- [ ] **Step 5: Write the census entity**

Create `ADDON/Scripts/Game/Mud/MUD_HoleCensus.c`:

```c
[EntityEditorProps(category: "Mud", description: "THROWAWAY TEST (mud plan 1, task 1): in play, lists every terrain hole cell of the whole terrain (GetSurfaceY < -255 at each cell centre) as runs in the log. Place by class anywhere, one per world.", color: "255 128 0 255")]
class MUD_HoleCensusEntityClass : GenericEntityClass
{
}

//! Throwaway census for the addon override test. Log lines, read by Tools/hole_census.py:
//!   MUD census box <mins> .. <maxs> cells <nx> x <nz> cell <size>
//!   MUD census run z <cz> x <cx0> <cx1>       (cells from the terrain box corner, inclusive)
//!   MUD census done holes <n> runs <r> ms <t> truncated <0|1>
//! 2026-10-08 19:58: GetSurfaceY returns -256 inside a terrain hole (the no-terrain default).
class MUD_HoleCensusEntity : GenericEntity
{
	[Attribute("5", UIWidgets.EditBox, "Seconds after start before the census runs")]
	protected float m_fDelay;

	[Attribute("2", UIWidgets.EditBox, "Heightmap cell size (m)")]
	protected float m_fCell;

	[Attribute("32", UIWidgets.EditBox, "Cell rows scanned per frame")]
	protected int m_iRowsPerFrame;

	[Attribute("3000", UIWidgets.EditBox, "Max run lines written to the log (more = truncated 1)")]
	protected int m_iMaxRuns;

	protected float m_fTime;
	protected int m_iState;             // 0 waiting, 1 scanning, 2 done
	protected int m_iRow;
	protected int m_iNX;
	protected int m_iNZ;
	protected float m_fX0;
	protected float m_fZ0;
	protected int m_iHoles;
	protected int m_iRuns;
	protected int m_iWorkMs;

	//------------------------------------------------------------------------------------------------
	void MUD_HoleCensusEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.FRAME);
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
		int rows = Math.ClampInt(m_iRowsPerFrame, 1, 4096);
		while (rows > 0 && m_iRow < m_iNZ)
		{
			ScanRow(m_iRow);
			m_iRow++;
			rows--;
		}
		m_iWorkMs += System.GetTickCount(t0);

		if (m_iRow < m_iNZ)
			return;

		m_iState = 2;
		int truncated = 0;
		if (m_iRuns > m_iMaxRuns)
			truncated = 1;
		Print(string.Format("MUD census done holes %1 runs %2 ms %3 truncated %4", m_iHoles, m_iRuns, m_iWorkMs, truncated));
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
			Print("MUD census: no terrain under the census entity", LogLevel.WARNING);
			return;
		}

		vector mins, maxs;
		terrain.GetTerrainBoundBox(mins, maxs);
		m_fX0 = mins[0];
		m_fZ0 = mins[2];
		m_iNX = Math.Round((maxs[0] - mins[0]) / m_fCell);
		m_iNZ = Math.Round((maxs[2] - mins[2]) / m_fCell);
		Print(string.Format("MUD census box %1 .. %2 cells %3 x %4 cell %5", mins, maxs, m_iNX, m_iNZ, m_fCell));
		m_iState = 1;
	}

	//------------------------------------------------------------------------------------------------
	protected void ScanRow(int cz)
	{
		BaseWorld world = GetWorld();
		float z = m_fZ0 + (cz + 0.5) * m_fCell;
		int start = -1;
		for (int cx = 0; cx < m_iNX; cx++)
		{
			bool hole = world.GetSurfaceY(m_fX0 + (cx + 0.5) * m_fCell, z) < -255;
			if (hole)
			{
				m_iHoles++;
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
	}

	//------------------------------------------------------------------------------------------------
	protected void EmitRun(int cz, int cx0, int cx1)
	{
		m_iRuns++;
		if (m_iRuns <= m_iMaxRuns)
			Print(string.Format("MUD census run z %1 x %2 %3", cz, cx0, cx1));
	}
}
```

- [ ] **Step 6: Compile, place, and run the baseline census on vanilla Arland (control)**

1. Reload scripts (enfusion `wb_reload`, or F7 in the Script Editor). Expected: no errors from `MUD_HoleCensus.c`.
2. In Workbench open any other world (e.g. `NEw map/unnamed.ent`) so `Arlnd.ent` is closed, then edit `ADDON/Arlnd_Layers/default.layer`: replace the block
   ```
   MUD_WorldSkinEntity {
    coords 1289.1 39 2195.223
    m_fRadius 500
   }
   ```
   with
   ```
   MUD_HoleCensusEntity {
    coords 1289.1 39 2195.223
   }
   ```
   (Task 2 puts the skin entity back.) Keep the house and M923A1 blocks unchanged.
3. Open `Arlnd.ent`, check with `wb_entity_list` that `MUD_HoleCensusEntity` is there, press Play, wait ~30 s after spawn, stop.
4. Copy the newest `console.log` to `ADDON/docs/superpowers/results/census-baseline.log`.

Expected log: `MUD census box ... cells 2048 x 2048 cell 2` and a `MUD census done holes <n> runs <r> ms <t> truncated 0` line. Record `n` (vanilla holes; 0 expected from the 4 sampled tiles, but whatever it says is the baseline). If `cells` is not `2048 x 2048`, stop: the tile math in the checker (`tiles=16, tile_cells=128`) is wrong for this terrain.

- [ ] **Step 7: Copy three Arland tiles from the Experimental pak into the addon and write holes**

Workbench: keep `Arlnd.ent` closed (open `NEw map/unnamed.ent`) - Workbench must not have the Arland terrain loaded while files change.

Run (from `BK`):
```bash
python -I tools/pakread.py get "worlds/Arland/Terrain/.Data/Terrain_1.ttile" "C:/Users/Steffen/Documents/My Games/ArmaReforgerWorkbench/addons/Testing plan vech msh" --install experimental
```
```bash
python -I tools/pakread.py get "worlds/Arland/Terrain/.Data/Terrain_16.ttile" "C:/Users/Steffen/Documents/My Games/ArmaReforgerWorkbench/addons/Testing plan vech msh" --install experimental
```
```bash
python -I tools/pakread.py get "worlds/Arland/Terrain/.Data/Terrain_18.ttile" "C:/Users/Steffen/Documents/My Games/ArmaReforgerWorkbench/addons/Testing plan vech msh" --install experimental
```
Expected: `wrote ... Terrain_1.ttile 35930`, `... Terrain_16.ttile 36004`, `... Terrain_18.ttile <size>`.

Then (from `ADDON`) write three non-square rectangles at tile-local cells, each in a non-zero block so local and global readings land in different places:
```bash
python -I Tools/ttile_holes.py "worlds/Arland/Terrain/.Data/Terrain_1.ttile" add 32 64 35 65
```
```bash
python -I Tools/ttile_holes.py "worlds/Arland/Terrain/.Data/Terrain_16.ttile" add 70 10 71 13
```
```bash
python -I Tools/ttile_holes.py "worlds/Arland/Terrain/.Data/Terrain_18.ttile" add 100 100 102 102
```
Expected: each prints `wrote ... blocks 1` (no assert). Confirm with `show`:
```bash
python -I Tools/ttile_holes.py "worlds/Arland/Terrain/.Data/Terrain_1.ttile" show
```
Expected: `(1, 2) 8 cells`. Terrain_16: `(2, 0) 8 cells`. Terrain_18: `(3, 3) 9 cells`. Sum 25 cells.

- [ ] **Step 8: Restart Workbench and check what it did with the files**

Close and restart Experimental Workbench (a loose file that overrides a pak path is only picked up on mount). Then:
- Note whether Workbench created `Terrain_1.ttile.meta` etc. next to the copies (`ls "ADDON/worlds/Arland/Terrain/.Data"`). Record yes/no - this matters for packing (Review Focus 4).
- Check the newest `console.log`/`error.log` for lines mentioning `Terrain_1`, `Terrain_16`, `Terrain_18` or `ttile`. Record them verbatim.

- [ ] **Step 9: Run the census with the override in place**

Open `Arlnd.ent`, Play, wait ~30 s after spawn, stop. Do not save the world. Copy the newest `console.log` to `ADDON/docs/superpowers/results/census-override.log`.
Expected: a complete census block with `truncated 0`, and `holes` = baseline + 25 if the override works.

- [ ] **Step 10: Run the checker**

Run (from `ADDON`):
```bash
python Tools/hole_census.py docs/superpowers/results/census-override.log --baseline docs/superpowers/results/census-baseline.log --write=1:32:64:35:65 --write=16:70:10:71:13 --write=18:100:100:102:102
```
Expected (pass): exit 0, last line `MATCH x-fastest/local`, `MATCH z-fastest/local` or `MATCH global` and `new hole cells: 25`.
Fail cases and what they mean:
- `new hole cells: 0` -> the addon override of a vanilla `.ttile` does not load. **Stop: gate 1 failed.** Go to step 15.
- `NO MATCH` with new cells != 25 -> override works but the layout reading is wrong; the per-hypothesis lines (extra / missing) show how. Keep the logs, stop, re-plan with the user.
- `lost` baseline cells -> our copy is not the tile the game had (stale or wrong install); recheck `--install experimental`.

- [ ] **Step 11: Physical check (one rectangle)**

Using the matched hypothesis, compute the world centre of the Terrain_18 rectangle: world X = terrain min X + (tile column * 128 + 101) * 2 + 1, world Z likewise with the tile row. In play, teleport/walk there (debug menu camera or place a marker in edit mode first). Expected: a 6 x 6 m hole, soldier falls in. Record the screenshot time. This guards against "GetSurfaceY says hole but the engine still collides".

- [ ] **Step 12: Pin the result as tests for Plan 2**

Add to `ADDON/Tools/test_hole_census.py` one test with the measured answer, so Plan 2's hole writer inherits it. Example if the checker printed `MATCH x-fastest/local` (replace both strings with what it actually printed):

```python
def test_measured_layout_arland_exp_192624():
    """Measured <run-date> (Exp 192624, Arland, census-override.log): MATCH x-fastest/local."""
    assert hc.expected_cells([(18, 100, 100, 100, 100)], 'x-fastest', 'local') == {(2 * 128 + 100, 1 * 128 + 100)}
```

Run: `python -m pytest Tools/test_hole_census.py -q`
Expected: `14 passed`.

- [ ] **Step 13: Pack check (does it ship, not only load loose)**

In Workbench use the project's publish/pack dialog to pack the addon to a local folder **without uploading** (do not publish - that is outward-facing and needs the user's yes). Then list the packed pak (from `BK`):
```bash
python -I -c "import sys; sys.path.insert(0, 'tools'); import pakread; [print(p) for p in pakread.parse_pak(sys.argv[1]) if p.endswith('.ttile')]" "<path to packed .pak>"
```
Expected: three lines `worlds/Arland/Terrain/.Data/Terrain_1.ttile`, `..._16.ttile`, `..._18.ttile`. If they are missing, the override works only from loose files: record it, and Plan 2 must solve packing (e.g. registering the tiles) before anything ships. If the Workbench build has no local-pack option, record that and move this check to Plan 2's dedicated-server task (a server loads the packed addon).

- [ ] **Step 14: Record**

Create `ADDON/docs/superpowers/results/<run-date>-gate1-override.md` with: game build, times of each play run, the baseline `done` line, the override `done` line, the full checker output (all hypothesis lines), checksum line `baseline N + 25 written = override M` evaluated, `.meta` created yes/no, the log lines from step 8, the physical check, the pack result. Then add one row to the spec's section 2 table ("Addon override of a vanilla `.ttile` by path: <result> | <results file>") and remove it from the "Untested" line.

- [ ] **Step 15: Clean up and gate**

- Delete the three override tiles (and any `.meta` Workbench made) from `ADDON/worlds/Arland/Terrain/.Data/`, restart Workbench, so Task 2 runs on vanilla Arland. Copies are reproducible from steps 7.
- Leave `MUD_HoleCensus.c` and `Tools/hole_census.py` in place (Plan 2's hole tool test reuses the census).
- Gate: Task 2 starts only if step 10 printed `MATCH`. Otherwise report to the user and stop.

---

### Task 2: Coarse skin cost on Arland (spec 7.2)

Measures the worst case: whole Arland, 2 m spacing (64 m chunks = 32 x 32 cells), WORLD-trace heights + one mid-cell trace per cell for the diagonal, render + collider; then collider only (what a dedicated server builds). Terrain stays on (holes, not hiding, are the design now).

**Files:**
- Modify: `ADDON/Scripts/Game/Mud/MUD_WorldSkin.c` (attributes, members, `Start`, `BuildIndices` -> `BuildUVs`, `BuildChunk`, new `HeightAt`, `EOnFrame`)
- Modify: `ADDON/Arlnd_Layers/default.layer` (census entity -> skin entity with test values)
- Create: `ADDON/docs/superpowers/results/<run-date>-gate2-skin-cost.md`

**Interfaces:**
- Consumes: Task 1 result file (the gate passed).
- Produces: log lines
  `MUD skin: terrain ... building <n>, render <0|1>, trace heights <0|1>, diagonal <0|1>, mem <kb> KB`,
  `MUD skin: baseline frame <ms> ms avg over <frames> frames`,
  `MUD skin: built <n> chunks (<verts> verts, <tris> tris) over <frames> frames, <wall> ms wall, worst frame <ms> ms | heights <ms> ms, meshes <ms> ms, colliders <ms> ms | mem +<kb> KB | diagonals A <a> B <b> tie <t> | trace misses <m>`,
  `MUD skin: after build frame <ms> ms avg over <frames> frames, mem +<kb> KB`.
  Plan 2 uses: total build ms, worst frame ms, mem delta, diagonal counts (if B or A is 0 on non-tie cells, the terrain uses a fixed diagonal and the mid-cell trace can go).

- [ ] **Step 1: Add the new attributes and members**

In `MUD_WorldSkin.c`, after the `m_bTerrainOff` attribute, add:

```c
	[Attribute("1", UIWidgets.CheckBox, "Heights from a WORLD trace (still hits the heightmap inside terrain holes) instead of GetSurfaceY (-256 in holes)", category: "Skin")]
	protected bool m_bTraceHeights;

	[Attribute("1", UIWidgets.CheckBox, "One extra WORLD trace per cell centre picks the cell's diagonal (exact heightmap triangles)", category: "Skin")]
	protected bool m_bPickDiagonal;

	[Attribute("0", UIWidgets.CheckBox, "Colliders only, no meshes (what a dedicated server builds)", category: "Skin")]
	protected bool m_bNoRender;

	[Attribute("10", UIWidgets.EditBox, "Seconds of frame time measured before the build and again after it (each after a 2 s settle)", category: "Measure")]
	protected float m_fMeasureTime;
```

Replace the members `protected bool m_bStarted;` and `protected bool m_bDone;` with:

```c
	protected int m_iPhase;             // 0 measure before, 1 build, 2 measure after, 3 done
	protected float m_fPhaseTime;
	protected float m_fFrameSum;
	protected int m_iFrameCount;
	protected int m_iWorstFrameMs;
	protected int m_iMemStartKB;
	protected int m_iDiagA;
	protected int m_iDiagB;
	protected int m_iDiagTie;
	protected int m_iTraceMiss;
	protected float m_fTraceTop;
	protected float m_fTraceBottom;
	protected ref TraceParam m_Trace;
```

- [ ] **Step 2: Replace `Start`, `BuildIndices`, add `HeightAt`**

Replace the whole `Start()` method with:

```c
	//------------------------------------------------------------------------------------------------
	//! After the "before" measurement: list the chunks to build
	protected void Start()
	{
		m_bRender = RplSession.Mode() != RplMode.Dedicated && !m_bNoRender;
		m_fStep = m_fChunkSize / CELLS;
		m_Buf = new MUD_WorldSkinBuffers();
		m_Trace = new TraceParam();
		BuildUVs();

		vector pos = GetOrigin();
		GenericWorldEntity worldEnt = GetGame().GetWorldEntity();
		if (worldEnt)
			m_Terrain = worldEnt.GetTerrain(pos[0], pos[2]);
		if (!m_Terrain)
		{
			Print("MUD skin: no terrain - nothing to do", LogLevel.WARNING);
			m_iPhase = 3;
			return;
		}

		vector mins, maxs;
		m_Terrain.GetTerrainBoundBox(mins, maxs);
		m_fTraceTop = maxs[1] + 10;
		m_fTraceBottom = mins[1] - 10;
		int nx = Math.Ceil((maxs[0] - mins[0]) / m_fChunkSize);
		int nz = Math.Ceil((maxs[2] - mins[2]) / m_fChunkSize);
		for (int cz = 0; cz < nz; cz++)
		{
			for (int cx = 0; cx < nx; cx++)
			{
				vector o = Vector(mins[0] + cx * m_fChunkSize, 0, mins[2] + cz * m_fChunkSize);
				if (m_fRadius > 0)
				{
					vector centre = o + Vector(m_fChunkSize * 0.5, 0, m_fChunkSize * 0.5);
					if (vector.DistanceXZ(centre, pos) > m_fRadius + m_fChunkSize * 0.71)
						continue;
				}
				m_aTodo.Insert(o);
			}
		}
		m_iTotal = m_aTodo.Count();
		m_iStartTick = System.GetTickCount();
		m_iMemStartKB = System.MemoryAllocationKB();
		Print(string.Format("MUD skin: terrain %1 .. %2, %3 x %4 chunks of %5 m (%6 m step), building %7, render %8, trace heights %9, diagonal %10, mem %11 KB",
			mins, maxs, nx, nz, m_fChunkSize, m_fStep, m_iTotal, m_bRender, m_bTraceHeights, m_bPickDiagonal, m_iMemStartKB));
	}
```

Note: `string.Format` takes at most 9 arguments in Enforce if the build complains about `%10`/`%11`; in that case split into two `Print` lines (the first ending at `building %7`, the second `MUD skin: render %1, trace heights %2, diagonal %3, mem %4 KB`). Check the compile output of step 4.

Replace the whole `BuildIndices()` method with (indices are now built per chunk in `BuildChunk`):

```c
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
	//! Heightmap height at a world XZ. WORLD trace hits only the heightmap, also inside holes
	//! (proven 2026-10-08 19:58); GetSurfaceY is -256 there.
	protected float HeightAt(float x, float z)
	{
		if (!m_bTraceHeights)
			return GetWorld().GetSurfaceY(x, z);

		m_Trace.Start = Vector(x, m_fTraceTop, z);
		m_Trace.End = Vector(x, m_fTraceBottom, z);
		m_Trace.Flags = TraceFlags.WORLD;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		float frac = GetWorld().TraceMove(m_Trace, null);
		if (frac >= 1)
		{
			m_iTraceMiss++;
			return GetWorld().GetSurfaceY(x, z);
		}
		return m_fTraceTop + (m_fTraceBottom - m_fTraceTop) * frac;
	}
```

- [ ] **Step 3: Replace the height and index part of `BuildChunk`, and `EOnFrame`**

In `BuildChunk`, replace the first block (from `int t0 = System.GetTickCount();` through `m_iSampleMs += System.GetTickCount(t0);`) with:

```c
		int t0 = System.GetTickCount();
		for (int vz = 0; vz < VERTS; vz++)
		{
			for (int vx = 0; vx < VERTS; vx++)
			{
				float lx = vx * m_fStep;
				float lz = vz * m_fStep;
				float h = HeightAt(o[0] + lx, o[2] + lz) + m_fOffset;
				m_Buf.m_aVerts[vz * VERTS + vx] = Vector(lx, h, lz);
			}
		}

		// per cell: pick the diagonal whose midpoint is nearest the traced cell centre
		int n;
		for (int z = 0; z < CELLS; z++)
		{
			for (int x = 0; x < CELLS; x++)
			{
				int bl = z * VERTS + x;
				int br = bl + 1;
				int tl = bl + VERTS;
				int tr = tl + 1;
				bool diagB = false;
				if (m_bPickDiagonal)
				{
					float hm = HeightAt(o[0] + (x + 0.5) * m_fStep, o[2] + (z + 0.5) * m_fStep) + m_fOffset;
					vector vbl = m_Buf.m_aVerts[bl];
					vector vbr = m_Buf.m_aVerts[br];
					vector vtl = m_Buf.m_aVerts[tl];
					vector vtr = m_Buf.m_aVerts[tr];
					float dA = Math.AbsFloat((vbl[1] + vtr[1]) * 0.5 - hm);
					float dB = Math.AbsFloat((vbr[1] + vtl[1]) * 0.5 - hm);
					if (Math.AbsFloat(dA - dB) < 0.001)
					{
						m_iDiagTie++;
					}
					else if (dB < dA)
					{
						diagB = true;
						m_iDiagB++;
					}
					else
					{
						m_iDiagA++;
					}
				}

				if (diagB)
				{
					// shared edge tl-br
					m_Buf.m_aIndices[n] = bl; n++;
					m_Buf.m_aIndices[n] = tl; n++;
					m_Buf.m_aIndices[n] = br; n++;
					m_Buf.m_aIndices[n] = tl; n++;
					m_Buf.m_aIndices[n] = tr; n++;
					m_Buf.m_aIndices[n] = br; n++;
				}
				else
				{
					// shared edge bl-tr (the old fixed layout)
					m_Buf.m_aIndices[n] = bl; n++;
					m_Buf.m_aIndices[n] = tl; n++;
					m_Buf.m_aIndices[n] = tr; n++;
					m_Buf.m_aIndices[n] = tr; n++;
					m_Buf.m_aIndices[n] = br; n++;
					m_Buf.m_aIndices[n] = bl; n++;
				}
			}
		}
		m_iSampleMs += System.GetTickCount(t0);
```

(Winding checked on paper: all four triangles have the same XZ cross-product sign as the old `bl, tl, tr`.)

Replace the whole `EOnFrame` with:

```c
	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (m_iPhase == 3)
			return;

		if (m_iPhase == 0 || m_iPhase == 2)
		{
			m_fPhaseTime += timeSlice;
			if (m_fPhaseTime < 2)
				return;     // settle: skip load / build hitch frames

			m_fFrameSum += timeSlice;
			m_iFrameCount++;
			if (m_fPhaseTime < 2 + m_fMeasureTime)
				return;

			float avgMs = m_fFrameSum * 1000 / Math.Max(m_iFrameCount, 1);
			if (m_iPhase == 0)
			{
				Print(string.Format("MUD skin: baseline frame %1 ms avg over %2 frames", avgMs, m_iFrameCount));
				m_iPhase = 1;
				Start();
				return;
			}

			Print(string.Format("MUD skin: after build frame %1 ms avg over %2 frames, mem +%3 KB", avgMs, m_iFrameCount, System.MemoryAllocationKB() - m_iMemStartKB));
			m_iPhase = 3;
			if (m_bTerrainOff)
				TerrainOff();
			return;
		}

		// phase 1: build
		m_iFrames++;
		int t0 = System.GetTickCount();
		int count = Math.ClampInt(m_iChunksPerFrame, 1, 1000);
		while (count > 0 && !m_aTodo.IsEmpty())
		{
			int last = m_aTodo.Count() - 1;
			BuildChunk(m_aTodo[last]);
			m_aTodo.Remove(last);
			count--;
		}
		int frameMs = System.GetTickCount(t0);
		if (frameMs > m_iWorstFrameMs)
			m_iWorstFrameMs = frameMs;

		if (!m_aTodo.IsEmpty())
			return;

		int verts = m_iTotal * 1089;
		Print(string.Format("MUD skin: built %1 chunks (%2 verts, %3 tris) over %4 frames, %5 ms wall, worst frame %6 ms | heights %7 ms, meshes %8 ms, colliders %9 ms",
			m_iTotal, verts, m_iTotal * 2048, m_iFrames, System.GetTickCount(m_iStartTick), m_iWorstFrameMs, m_iSampleMs, m_iMeshMs, m_iColMs));
		Print(string.Format("MUD skin: mem +%1 KB | diagonals A %2 B %3 tie %4 | trace misses %5",
			System.MemoryAllocationKB() - m_iMemStartKB, m_iDiagA, m_iDiagB, m_iDiagTie, m_iTraceMiss));

		m_iPhase = 2;
		m_fPhaseTime = 0;
		m_fFrameSum = 0;
		m_iFrameCount = 0;
	}
```

Also: remove the now-unused call `BuildIndices();` (it was inside the old `Start`, already replaced), and update the class comment line "samples GetSurfaceY per vertex" to "samples heights by WORLD trace (or GetSurfaceY) per vertex, one extra trace per cell picks the diagonal".

- [ ] **Step 4: Compile**

Reload scripts (`wb_reload` or F7). Expected: no errors or new warnings from `MUD_WorldSkin.c`. If `%10`/`%11` in `Start` errors, apply the two-line split from step 2. Fix any error before continuing (`Math.Max(int, int)` here is only used in a float division, not as an array index - see the 15:25 gotcha).

- [ ] **Step 5: Small-radius smoke run (control: old behaviour still works)**

With `Arlnd.ent` closed, edit `ADDON/Arlnd_Layers/default.layer`: replace the `MUD_HoleCensusEntity { ... }` block with
```
MUD_WorldSkinEntity {
 coords 1289.1 39 2195.223
 m_fChunkSize 64
 m_fRadius 200
 m_fOffset 0.001
 m_bTerrainOff 0
 m_fMeasureTime 5
}
```
Open `Arlnd.ent`, Play, wait until the `after build frame` line, stop.
Expected in the log: `building` about 40 chunks, `trace misses 0`, a `diagonals A .. B .. tie ..` line, and in the 3D view the skin lies on the terrain (z-fight shimmer is fine at +1 mm on unholed terrain). If `trace misses` > 0, the trace start/end from the bound box is wrong - fix before the full run.

- [ ] **Step 6: Full-map run, render + collider (client worst case)**

Change the block to `m_fRadius 0` and `m_fMeasureTime 10`, keep the rest. Play, wait for `after build frame` (expect up to ~1 min), stop. Note Task Manager's Workbench memory before Play and after the build (second memory reading beside `MemoryAllocationKB`).
Expected: `building 4096`; record every number from the four `MUD skin:` lines.

- [ ] **Step 7: Full-map run, collider only (dedicated-server case)**

Add `m_bNoRender 1` to the block. Play, wait for `after build frame`, stop. Record all numbers.

- [ ] **Step 8: Record and decide**

Create `ADDON/docs/superpowers/results/<run-date>-gate2-skin-cost.md` with the full `MUD skin:` lines of steps 5-7, Task Manager readings, and a table per run: chunks, verts, tris, wall ms, worst frame ms, heights / meshes / colliders ms, mem delta KB, baseline vs after frame ms, diagonal A / B / tie, trace misses. Checksum line: `verts = chunks x 1089` and `tris = chunks x 2048`, evaluated.

Proposed decision rule (the user confirms or changes it before Plan 2):
- whole-map 2 m collider stays if collider-only wall time <= 15 s and mem delta <= 600 MB and after-build frame time <= baseline + 1 ms; otherwise Plan 2 uses a 4 m collider beyond ~500 m of any player (spec 5.1 fallback).
- render: the 2 m mesh is limited to ~500 m around the camera in Plan 2 regardless (spec 5.1); this run only bounds the worst case.
- diagonal: if A or B is 0 among non-tie cells, the heightmap uses one fixed diagonal - Plan 2 drops the mid-cell trace and hard-codes it (halves the trace count).

Then update the spec's section 2 table with the measured skin cost and remove "full-map coarse skin cost" from the "Untested" line.

- [ ] **Step 9: Hand back**

Restore `Arlnd_Layers/default.layer` to the original skin block (shown in Task 1 step 6) or leave the test block - note which in the results file. Report both result files to the user and ask for the go-ahead to write Plan 2 (hole tool, ground refactor, MP).
