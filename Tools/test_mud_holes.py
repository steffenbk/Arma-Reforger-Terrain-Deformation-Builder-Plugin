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


def test_merge_tile_round_trips_odd_chunk_without_pad_byte():
    """Enfusion writes odd-length chunks with NO IFF pad byte (Arland Terrain_39: GCTD 43 bytes, LRS2 right after)."""
    body = b''
    for tag, data in [(b'VERS', b'\1' * 4), (b'HGHT', b'\0' * 33282), (b'BERR', b'\0' * 576),
                      (b'GCTD', b'g' * 43), (b'LRS2', b'abc'), (b'TMAT', b'\0' * 4)]:
        body += tag + struct.pack('>I', len(data)) + data
    data = b'FORM' + struct.pack('>I', len(body) + 4) + b'TERR' + body
    out = mh.merge_tile(data, {(0, 0): [1] * 32})
    parts = tt.chunks(out)
    assert [t for t, _ in parts] == [b'VERS', b'HGHT', b'BERR', b'HOLE', b'GCTD', b'LRS2', b'TMAT']
    assert dict(parts)[b'GCTD'] == b'g' * 43 and dict(parts)[b'LRS2'] == b'abc'
    assert len(out) == len(data) + 8 + 6 + 132


# ---------------------------------------------------------------- pins (RELATIVE_Y roots, 2026-10-09)
# Engine (IDA, Exp 192624 sub_140A1CC20): a root with RELATIVE_Y gets y += terrain height at its origin
# XZ, and the height lookup fails when that ONE cell is a hole -> the entity keeps its raw offset.
# So the cell under each such origin must stay real terrain; it is not grown by the margin.

def pins_text(nx, nz, cells, pins=None, roots=None, done=True, prefix=''):
    lines = [prefix + 'MUD pins box 0 0 cells %d %d cell 2' % (nx, nz)]
    lines += [prefix + 'MUD pin z %d x %d' % (z, x) for z, x in cells]
    if pins is None:
        pins = len(cells)
    if roots is None:
        roots = len(cells)
    if done:
        lines.append(prefix + 'MUD pins done pins %d roots %d' % (pins, roots))
    return '\n'.join(lines) + '\n'


def test_parse_pins_reads_cells_from_last_block_with_log_prefix():
    text = pins_text(64, 32, [(0, 0)]) + pins_text(64, 32, [(1, 2), (5, 63)], prefix='08:50:10.742   SCRIPT       : ')
    p = mh.parse_pins(text)
    assert (p['nx'], p['nz']) == (64, 32)
    assert p['cells'] == {1 * 64 + 2, 5 * 64 + 63}


def test_parse_pins_raises_without_done():
    with pytest.raises(ValueError, match='done'):
        mh.parse_pins(pins_text(64, 32, [(0, 0)], done=False))


def test_parse_pins_checksum_mismatch_raises():
    with pytest.raises(ValueError, match='checksum'):
        mh.parse_pins(pins_text(64, 32, [(0, 0)], pins=2))


def test_parse_pins_duplicate_cell_raises():
    with pytest.raises(ValueError, match='twice'):
        mh.parse_pins(pins_text(64, 32, [(0, 0), (0, 0)]))


def test_parse_pins_cell_outside_grid_raises():
    with pytest.raises(ValueError, match='outside'):
        mh.parse_pins(pins_text(64, 32, [(32, 0)]))


def test_stats_keeps_pinned_cells_without_margin():
    m = mh.parse_mask(mask_text(64, 32, []))
    p = mh.parse_pins(pins_text(64, 32, [(10, 40)]))
    s = mh.stats(m, margin=1, pins=p)
    assert s['keep'] == 1                      # the pin only, not grown to 3 x 3
    assert s['keep_grid'][10 * 64 + 40] == 1
    assert s['keep'] + s['holes'] == 64 * 32
    assert s['pins_new'] == 1                  # pins that were holes before


def test_stats_pin_on_kept_cell_changes_nothing():
    m = mh.parse_mask(mask_text(64, 32, [(0, 0, 3)]))
    p = mh.parse_pins(pins_text(64, 32, [(0, 1)]))
    assert mh.stats(m, margin=1, pins=p)['keep'] == mh.stats(m, margin=1)['keep']
    assert mh.stats(m, margin=1, pins=p)['pins_new'] == 0


def test_stats_refuses_pins_of_another_grid():
    m = mh.parse_mask(mask_text(64, 32, []))
    with pytest.raises(ValueError, match='size'):
        mh.stats(m, margin=1, pins=mh.parse_pins(pins_text(32, 32, [])))


def test_verify_with_pins_expects_pinned_cell_kept():
    mask = mask_text(64, 32, [(z, 0, 31) for z in range(32)])   # right block holed
    pins = pins_text(64, 32, [(3, 40)])
    census = census_text(64, 32, [(z, 32, 63) for z in range(32) if z != 3] + [(3, 32, 39), (3, 41, 63)])
    r = mh.verify(census, mask, margin=0, pins_text=pins)
    assert (r['expected'], r['observed'], r['missing'], r['extra']) == (1023, 1023, 0, 0)


def test_write_refuses_without_pins(tmp_path, capsys):
    mask = tmp_path / 'mask.txt'
    mask.write_text(mask_text(64, 32, []))
    with pytest.raises(SystemExit):
        mh.main(['write', str(mask), '--out', str(tmp_path / 'o'), '--backup', str(tmp_path / 'b'), '--bk', str(tmp_path)])
    assert '--pins' in capsys.readouterr().err
    assert not (tmp_path / 'o').exists()


# ---------------------------------------------------------------- full blocks (bald grass, 2026-10-10)

def test_keep_one_per_full_block_keeps_corner_of_full_blocks_only():
    # 64 x 32: block (0, 0) all holes, block (1, 0) has a kept cell already
    keep = bytearray(64 * 32)
    keep[5 * 64 + 40] = 1
    n = mh.keep_one_per_full_block(keep, 64, 32)
    assert n == 1
    assert keep[0] == 1                       # corner cell (0, 0) of the full block kept
    assert sum(keep) == 2                     # partial block untouched
    assert mh.full_blocks(keep, 64, 32) == []


def test_keep_one_per_full_block_skips_the_control_block():
    keep = bytearray(64 * 32)
    n = mh.keep_one_per_full_block(keep, 64, 32, skip={(1, 0)})
    assert n == 1
    assert keep[0] == 1 and keep[32] == 0
    assert mh.full_blocks(keep, 64, 32) == [(1, 0)]


def test_stats_leaves_no_full_block_and_verify_agrees():
    text = mask_text(64, 32, [(0, 63, 63)])   # one kept cell in block (1, 0), block (0, 0) all holes
    m = mh.parse_mask(text)
    s = mh.stats(m, margin=0, keep_full=True)
    assert s['full_kept'] == 1
    assert mh.full_blocks(s['keep_grid'], 64, 32) == []
    assert s['keep'] == 2 and s['keep'] + s['holes'] == 64 * 32
    hole_runs = [(z, 1, 31) for z in range(1)] + [(z, 0, 31) for z in range(1, 32)] + [(z, 32, 62) for z in range(1)] + [(z, 32, 63) for z in range(1, 32)]
    r = mh.verify(census_text(64, 32, hole_runs), text, 0, keep_full=True)
    assert (r['missing'], r['extra']) == (0, 0)


def test_merge_tile_refuses_a_full_block():
    with pytest.raises(ValueError, match='full'):
        mh.merge_tile(fake_ttile(), {(0, 0): [mh.FULL] * 32}, refuse_full=True)


def test_merge_tile_allows_the_control_block():
    out = mh.merge_tile(fake_ttile(), {(0, 0): [mh.FULL] * 32}, allow_full={(0, 0)}, refuse_full=True)
    assert holes_of(out) == {(0, 0): [mh.FULL] * 32}


# ---------------------------------------------------------------- fit / bake (plan 3 task 11)

def hght_tile(values, hole=None):
    """FORM/TERR like fake_ttile, HGHT = given 129*129 u16 values."""
    parts = [(b'VERS', b'\x01' * 9), (b'HGHT', struct.pack('<16641H', *values)), (b'BERR', b'\0' * 576)]
    if hole is not None:
        parts.append((b'HOLE', tt.encode(hole)))
    parts += [(b'LRS2', b'abc'), (b'TMAT', b'\0' * 4)]
    return tt.build(parts)


def pattern(i):
    x, z = i % 129, i // 129
    return (x * 131 + z * 977 + x * z) % 60000 + 1000


def test_hght_index_orients():
    assert mh.hght_index(0, 0, (0, 0, 0, 0)) == 0
    assert mh.hght_index(5, 7, (0, 0, 0, 0)) == 7 * 129 + 5
    assert mh.hght_index(5, 7, (1, 0, 0, 0)) == 5 * 129 + 7
    assert mh.hght_index(5, 7, (0, 1, 0, 0)) == 7 * 129 + 123
    assert mh.hght_index(5, 7, (0, 0, 1, 0)) == 121 * 129 + 5
    assert len(mh.ORIENTS) == 16


def test_vertex_local():
    assert mh.vertex_local(5, 6, 0) == (32, 64)
    assert mh.vertex_local(7, 0, 32 * 33 + 32) == (128, 32)


def test_parse_samples_with_prefix():
    text = "12:00:00.0 SCRIPT : MUD bake sample 3 4 100 -12345\nnoise\nMUD bake sample 0 0 0 7\n"
    assert mh.parse_samples(text) == [(3, 4, 100, -12345), (0, 0, 0, 7)]


def make_samples(o, a, b, tile_vals, n=300):
    out = []
    for k in range(n):
        ix, iz, v = k % 4, (k // 4) % 4, (k * 37) % 1089
        lx, lz = mh.vertex_local(ix, iz, v)
        out.append((ix, iz, v, round((a * tile_vals[mh.hght_index(lx, lz, o)] + b) * 1000)))
    return out


def test_fit_finds_orientation_and_scale():
    vals = [pattern(i) for i in range(16641)]
    o = (1, 0, 1, 0)
    samples = make_samples(o, 0.0047, -163.0, vals)
    r = mh.fit(samples, lambda t: hght_tile(vals))
    assert tuple(r['orient']) == o
    assert abs(r['a'] - 0.0047) < 1e-7 and abs(r['b'] + 163.0) < 1e-3
    assert r['resid'] < 0.001 and r['samples'] == 300


def test_fit_reports_bad_residual_on_noise():
    vals = [pattern(i) for i in range(16641)]
    samples = [(s[0], s[1], s[2], s[3] + (k % 7) * 500) for k, s in enumerate(make_samples((0, 0, 0, 0), 0.0047, -163.0, vals))]
    assert mh.fit(samples, lambda t: hght_tile(vals))['resid'] > 0.002


def test_bake_writes_hole_blocks_only(tmp_path):
    vals = [pattern(i) for i in range(16641)]
    rows = [0] * 32
    rows[3] = 1
    (tmp_path / 'Terrain_0.ttile').write_bytes(hght_tile(vals, {(1, 2): rows, (2, 2): [0] * 32}))
    (tmp_path / 'notes.txt').write_text('x')
    out = tmp_path / 'h.bin'
    fitd = {'orient': [0, 0, 0, 0], 'a': 0.5, 'b': -10.0}
    r = mh.bake(str(tmp_path), fitd, str(out))
    assert r == {'blocks': 1, 'tiles': 1, 'bytes': 16 + 8 + 4 * 1089}
    data = out.read_bytes()
    assert data[:4] == b'MUDH'
    assert struct.unpack('<iii', data[4:16]) == (1, 1, 32)
    assert struct.unpack('<ii', data[16:24]) == (1, 2)
    h = struct.unpack('<1089f', data[24:])
    assert h[0] == pytest.approx(0.5 * vals[mh.hght_index(32, 64, (0, 0, 0, 0))] - 10.0)
    assert h[1088] == pytest.approx(0.5 * vals[mh.hght_index(64, 96, (0, 0, 0, 0))] - 10.0)
    assert len(data) == r['bytes']


def test_bake_rejects_block_in_wrong_tile(tmp_path):
    vals = [pattern(i) for i in range(16641)]
    (tmp_path / 'Terrain_1.ttile').write_bytes(hght_tile(vals, {(0, 0): [1] * 32}))
    with pytest.raises(ValueError):
        mh.bake(str(tmp_path), {'orient': [0, 0, 0, 0], 'a': 1.0, 'b': 0.0}, str(tmp_path / 'h.bin'))


def test_main_fit_and_bake(tmp_path, capsys):
    vals = [pattern(i) for i in range(16641)]
    data = tmp_path / 'data'
    data.mkdir()
    (data / 'Terrain_0.ttile').write_bytes(hght_tile(vals, {(0, 0): [1] * 32}))
    log = tmp_path / 'console.log'
    log.write_text('\n'.join('MUD bake sample %d %d %d %d' % s for s in make_samples((0, 0, 0, 0), 0.0047, -163.0, vals)), encoding='utf-8')
    fitp = tmp_path / 'fit.json'
    assert mh.main(['fit', str(log), '--data', str(data), '--out', str(fitp)]) == 0
    assert 'FIT OK' in capsys.readouterr().out
    assert mh.main(['bake', '--data', str(data), '--fit', str(fitp), '--out', str(tmp_path / 'h.bin')]) == 0
    assert 'baked 1 blocks from 1 tiles' in capsys.readouterr().out


def test_main_fit_fail_writes_no_fit_file(tmp_path, capsys):
    vals = [pattern(i) for i in range(16641)]
    data = tmp_path / 'data'
    data.mkdir()
    (data / 'Terrain_0.ttile').write_bytes(hght_tile(vals))
    noisy = [(s[0], s[1], s[2], s[3] + (k % 7) * 500) for k, s in enumerate(make_samples((0, 0, 0, 0), 0.0047, -163.0, vals))]
    log = tmp_path / 'console.log'
    log.write_text('\n'.join('MUD bake sample %d %d %d %d' % s for s in noisy), encoding='utf-8')
    fitp = tmp_path / 'fit.json'
    assert mh.main(['fit', str(log), '--data', str(data), '--out', str(fitp)]) == 1
    assert 'FIT FAIL' in capsys.readouterr().out
    assert not fitp.exists()


@pytest.mark.parametrize('out, vanilla', [
    (r'C:\addons\X\worlds\Arland\Terrain\.Data', True),
    ('C:/addons/X/worlds/arland/Terrain/.Data/', True),
    ('C:/addons/X/worlds/MudArland/Terrain/.Data', False),
    ('C:/addons/X/worlds/Arland/Terrain/.Data2', False),
])
def test_is_vanilla_out_refuses_only_the_vanilla_tile_folder(out, vanilla):
    assert mh.is_vanilla_out(out) is vanilla
