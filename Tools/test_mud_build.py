"""Tests for Tools/mud_build.py and the mud_holes helpers it uses (reset_holes, snapshot, restore, set_layout).
Synthetic map: 2 x 1 tiles of 33 x 33 vertices (one hole block a tile), cell 2 m."""

import struct

import pytest

import mud_build as mb
import mud_holes as mh
import ttile_holes as tt

V = 33


@pytest.fixture(autouse=True)
def arland_layout_after():
    yield
    mh.set_layout(129, 16)  # other test files expect the Arland default


def hvals(t):
    return [(x * 7 + z * 131 + x * z * 3 + t * 997) % 60000 for z in range(V) for x in range(V)]


def tile(t, hole=None, paint=b'\0' * 8):
    parts = [(b'VERS', b'\x01' * 9), (b'HGHT', struct.pack('<%dH' % (V * V), *hvals(t))), (b'BERR', b'\0' * 16)]
    if hole is not None:
        parts.append((b'HOLE', tt.encode(hole)))
    parts.append((b'LRS2', paint))
    return tt.build(parts)


def holes_of(data):
    d = dict(tt.chunks(data))
    return tt.decode(d[b'HOLE']) if b'HOLE' in d else {}


def make_world(tmp_path, pristine_hole=None):
    d = tmp_path / 'Terrain' / '.Data'
    d.mkdir(parents=True)
    (d / 'Terrain_0.ttile').write_bytes(tile(0, pristine_hole))
    (d / 'Terrain_1.ttile').write_bytes(tile(1))
    return d


def mask_and_pins(tmp_path, keep_rows):
    """Mask 64 x 32 cells: keep_rows = {cz: (x0, x1)} kept runs, everything else a hole. No pins."""
    lines = ['MUD mask 1', 'MUD mask box 0 0 cells 64 32 cell 2']
    cells = 0
    for cz, (a, b) in sorted(keep_rows.items()):
        lines.append('MUD mask keep z %d x %d %d' % (cz, a, b))
        cells += b - a + 1
    lines.append('MUD mask done keep %d runs %d road 0 water 0 box 0 ms 0' % (cells, len(keep_rows)))
    m = tmp_path / 'mask.txt'
    m.write_text('\n'.join(lines) + '\n')
    p = tmp_path / 'pins.txt'
    p.write_text('MUD pins box 0 0 cells 64 32 cell 2\nMUD pins done pins 0 roots 0\n')
    return m, p


def samples_file(tmp_path, a=0.01, b=-5.0):
    """Exact samples: h = a * u + b of the stored u16, chunk (ix, 0), vertex v (as the plugin writes them)."""
    lines = []
    for ix in (0, 1):
        vals = hvals(ix)
        for v in (0, 16 * V + 16, 5 * V + 7, 31 * V + 30, 10 * V + 25, 3 * V + 29):
            lines.append('MUD bake sample %d 0 %d %d' % (ix, v, round((a * vals[v] + b) * 1000)))
    f = tmp_path / 'samples.txt'
    f.write_text('\n'.join(lines) + '\n')
    return f


def run(tmp_path, cmd, tiles, extra=()):
    rep = tmp_path / ('report_%s.txt' % cmd)
    args = [cmd, '--tiles', str(tiles), '--backup', str(tmp_path / 'backup'), '--report', str(rep)] + list(extra)
    code = mb.main(args)
    return code, rep.read_text()


def build_args(tmp_path, m, p, s):
    return ['--mask', str(m), '--pins', str(p), '--samples', str(s), '--heights', str(tmp_path / 'h' / 'x_mud_heights.c'),
            '--tiles-x', '2', '--cell', '2', '--margin', '0']


def test_set_layout_rejects_bad_vertex_count():
    with pytest.raises(ValueError):
        mh.set_layout(100, 4)


def test_set_layout_changes_owner_tile_and_vertex_local():
    mh.set_layout(33, 2)
    assert mh.owner_tile(1, 0) == 1 and mh.owner_tile(0, 1) == 2
    assert mh.vertex_local(1, 0, 33) == (0, 1)


def test_reset_holes_takes_pristine_hole_and_keeps_current_paint():
    cur = tile(0, {(0, 0): [1] * 32}, paint=b'\x05' * 8)
    out = mh.reset_holes(cur, tile(0, {(0, 0): [2] + [0] * 31}))
    assert holes_of(out) == {(0, 0): [2] + [0] * 31}
    assert dict(tt.chunks(out))[b'LRS2'] == b'\x05' * 8


def test_reset_holes_drops_hole_chunk_when_pristine_has_none():
    out = mh.reset_holes(tile(0, {(0, 0): [1] * 32}), tile(0))
    assert b'HOLE' not in dict(tt.chunks(out))
    assert out == tile(0)


def test_snapshot_keeps_the_first_copy(tmp_path):
    d = make_world(tmp_path)
    assert mh.snapshot(str(d), str(tmp_path / 'backup')) == (2, 2)
    (d / 'Terrain_0.ttile').write_bytes(tile(0, {(0, 0): [1] * 32}))
    assert mh.snapshot(str(d), str(tmp_path / 'backup')) == (0, 2)
    assert (tmp_path / 'backup' / 'Terrain_0.ttile').read_bytes() == tile(0)


def test_build_holes_bakes_and_unbuild_restores(tmp_path):
    d = make_world(tmp_path, pristine_hole={(0, 0): [1 << 31] + [0] * 31})
    m, p = mask_and_pins(tmp_path, {0: (0, 63), 5: (10, 40)})
    code, rep = run(tmp_path, 'build', d, build_args(tmp_path, m, p, samples_file(tmp_path)))
    assert code == 0, rep
    assert rep.splitlines()[-1] == 'MUD build exit 0'
    h0, h1 = holes_of((d / 'Terrain_0.ttile').read_bytes()), holes_of((d / 'Terrain_1.ttile').read_bytes())
    assert h0[(0, 0)][0] == 1 << 31            # row 0 kept, the pristine hole cell stays a hole
    assert h0[(0, 0)][5] == (1 << 10) - 1      # row 5 keeps x 10..31
    assert h1[(1, 0)][5] == ~((1 << 9) - 1) & mh.FULL     # x 32..40 kept, 41..63 holes
    hb = (tmp_path / 'h' / 'x_mud_heights.c').read_bytes()
    assert hb[:4] == b'MUDH' and struct.unpack('<iii', hb[4:16]) == (1, 2, 32)

    code, rep = run(tmp_path, 'unbuild', d, ['--heights', str(tmp_path / 'h' / 'x_mud_heights.c')])
    assert code == 0, rep
    assert (d / 'Terrain_0.ttile').read_bytes() == tile(0, {(0, 0): [1 << 31] + [0] * 31})
    assert (d / 'Terrain_1.ttile').read_bytes() == tile(1)
    assert not (tmp_path / 'h' / 'x_mud_heights.c').exists()
    assert (tmp_path / 'backup' / 'x_mud_heights.c.unbuilt').read_bytes() == hb


def test_rebuild_keeps_map_edits_and_is_idempotent(tmp_path):
    d = make_world(tmp_path)
    m, p = mask_and_pins(tmp_path, {0: (0, 63)})
    args = build_args(tmp_path, m, p, samples_file(tmp_path))
    assert run(tmp_path, 'build', d, args)[0] == 0
    first = (d / 'Terrain_1.ttile').read_bytes()
    assert run(tmp_path, 'build', d, args)[0] == 0
    assert (d / 'Terrain_1.ttile').read_bytes() == first
    # the map maker repaints tile 1 after Build: Unbuild keeps the paint, drops our holes
    parts = [(t, b'\x09' * 8 if t == b'LRS2' else v) for t, v in tt.chunks(first)]
    (d / 'Terrain_1.ttile').write_bytes(tt.build(parts))
    assert run(tmp_path, 'unbuild', d)[0] == 0
    assert (d / 'Terrain_1.ttile').read_bytes() == tile(1, paint=b'\x09' * 8)


def test_failed_fit_touches_no_tile(tmp_path):
    d = make_world(tmp_path)
    m, p = mask_and_pins(tmp_path, {0: (0, 63)})
    s = tmp_path / 'samples.txt'
    s.write_text('\n'.join('MUD bake sample %d 0 %d %d' % (i % 2, i * 31, (i * 7919) % 5000) for i in range(30)))
    code, rep = run(tmp_path, 'build', d, build_args(tmp_path, m, p, s))
    assert code == 1 and 'FIT FAIL' in rep
    assert (d / 'Terrain_0.ttile').read_bytes() == tile(0)
    assert not (tmp_path / 'backup').exists()


def test_mask_of_another_grid_is_refused(tmp_path):
    d = make_world(tmp_path)
    m = tmp_path / 'mask.txt'
    m.write_text('MUD mask box 0 0 cells 32 32 cell 2\nMUD mask done keep 0 runs 0 road 0 water 0 box 0 ms 0\n')
    p = tmp_path / 'pins.txt'
    p.write_text('MUD pins box 0 0 cells 32 32 cell 2\nMUD pins done pins 0 roots 0\n')
    code, rep = run(tmp_path, 'build', d, build_args(tmp_path, m, p, samples_file(tmp_path)))
    assert code == 1 and 'another world' in rep
    assert (d / 'Terrain_0.ttile').read_bytes() == tile(0)
    assert not (tmp_path / 'backup').exists()


# --- vanilla mode (2026-10-28): override tiles in the mod, no world copy -------------------------------------

class FakePak:
    """Stands in for pak_files.PakFiles: {pak path: bytes}."""
    files = {}

    def __init__(self, data_dir):
        self.dir = data_dir

    def find(self, pattern):
        import re
        r = re.compile(pattern, re.I)
        return sorted(p for p in self.files if r.search(p))

    def read(self, path):
        return self.files[path]


def vanilla_paks(monkeypatch, name='Terrain', wrong_first=True):
    """Two same-named pak terrains: worlds/Aaa (wrong heights, sorts first) and worlds/Map (fits the samples)."""
    files = {}
    for t in (0, 1):
        files['worlds/Map/%s/.Data/%s_%d.ttile' % (name, name, t)] = tile(t)
        if wrong_first:
            files['worlds/Aaa/%s/.Data/%s_%d.ttile' % (name, name, t)] = tile(t + 5)
    monkeypatch.setattr(FakePak, 'files', files)
    monkeypatch.setattr(mb.pf, 'PakFiles', FakePak)
    return files


def run_vanilla(tmp_path, name, extra):
    rep = tmp_path / 'report_build.txt'
    args = ['build', '--vanilla', name, '--game-data', str(tmp_path / 'game'), '--addon', str(tmp_path / 'mod'),
            '--backup', str(tmp_path / 'backup'), '--report', str(rep)] + list(extra)
    return mb.main(args), rep.read_text()


@pytest.mark.parametrize('name', ['Terrain', 'Eden'])
def test_vanilla_build_writes_override_tiles_and_unbuild_removes_them(tmp_path, monkeypatch, name):
    files = vanilla_paks(monkeypatch, name)
    m, p = mask_and_pins(tmp_path, {0: (0, 63), 5: (10, 40)})
    code, rep = run_vanilla(tmp_path, name, build_args(tmp_path, m, p, samples_file(tmp_path)))
    assert code == 0, rep
    d = tmp_path / 'mod' / 'worlds' / 'Map' / name / '.Data'
    assert 'vanilla terrain worlds/Map/%s/.Data' % name in rep and ('override tiles ' + str(d).replace('\\', '/')) in rep
    assert not (tmp_path / 'mod' / 'worlds' / 'Aaa').exists()          # the same-named terrain that does not fit
    assert holes_of((d / ('%s_0.ttile' % name)).read_bytes())[(0, 0)][5] == (1 << 10) - 1
    assert files['worlds/Map/%s/.Data/%s_0.ttile' % (name, name)] == tile(0)   # the pak copy is never written
    o = mb.read_override(str(tmp_path / 'backup'))
    assert sorted(o['created']) == ['%s_0.ttile' % name, '%s_1.ttile' % name]

    rep_u = tmp_path / 'report_unbuild.txt'
    code = mb.main(['unbuild', '--backup', str(tmp_path / 'backup'), '--report', str(rep_u),
                    '--heights', str(tmp_path / 'h' / 'x_mud_heights.c')])
    assert code == 0, rep_u.read_text()
    assert sorted(x.name for x in d.iterdir()) == []                   # the pak tiles are used again
    assert (tmp_path / 'backup' / 'override_removed' / ('%s_1.ttile' % name)).read_bytes() == tile(1)
    assert 'not equal to the pre-build copy: 0' in rep_u.read_text()


def test_vanilla_unbuild_keeps_an_override_tile_the_mod_had_before(tmp_path, monkeypatch):
    vanilla_paks(monkeypatch)
    d = tmp_path / 'mod' / 'worlds' / 'Map' / 'Terrain' / '.Data'
    d.mkdir(parents=True)
    (d / 'Terrain_1.ttile').write_bytes(tile(1, paint=b'\x07' * 8))    # the map maker's own override
    m, p = mask_and_pins(tmp_path, {0: (0, 63)})
    assert run_vanilla(tmp_path, 'Terrain', build_args(tmp_path, m, p, samples_file(tmp_path)))[0] == 0
    assert mb.read_override(str(tmp_path / 'backup'))['created'] == ['Terrain_0.ttile']
    assert mb.main(['unbuild', '--backup', str(tmp_path / 'backup'), '--report', str(tmp_path / 'r.txt')]) == 0
    assert sorted(x.name for x in d.iterdir()) == ['Terrain_1.ttile']
    assert (d / 'Terrain_1.ttile').read_bytes() == tile(1, paint=b'\x07' * 8)


def test_vanilla_without_a_fitting_terrain_writes_nothing(tmp_path, monkeypatch):
    vanilla_paks(monkeypatch)
    m, p = mask_and_pins(tmp_path, {0: (0, 63)})
    s = tmp_path / 'samples.txt'
    s.write_text('\n'.join('MUD bake sample %d 0 %d %d' % (i % 2, i * 31, (i * 7919) % 5000) for i in range(30)))
    code, rep = run_vanilla(tmp_path, 'Terrain', build_args(tmp_path, m, p, s))
    assert code == 1 and 'no pak terrain fits' in rep
    assert not (tmp_path / 'mod').exists() and not (tmp_path / 'backup').exists()
