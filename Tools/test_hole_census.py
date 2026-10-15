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


def test_measured_layout_arland_exp_192624():
    """Measured 2026-10-08 22:49 (Exp 192624, Arland, census-override.log): MATCH global.
    HOLE block coords are terrain-global: holes written into Terrain_1/16/18 all landed at their
    written cells near the map corner, not inside their own tile."""
    assert hc.expected_cells([(18, 100, 100, 100, 100)], 'any', 'global') == {(100, 100)}
    got = hc.check({(x, z) for x in range(100, 103) for z in range(100, 103)}, set(), [(18, 100, 100, 102, 102)])
    assert got['matches'] == ['global']
