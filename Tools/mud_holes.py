"""Whole-map terrain holes (mud plan 2): hole mask -> HOLE chunks in every .ttile of a map.

Reads the keep mask written in play by MUD_HoleMask.c (roads, water, keep boxes), grows the kept cells by a
margin, holes every other 2 m cell and writes each 32 x 32-cell block into its owner tile
(x-fastest: tile = (bz // 4) * 16 + bx // 4, Arland Exp 192624). Every written tile starts from the VANILLA
pak tile (vanilla holes OR'ed in), so a re-run is idempotent; an existing addon tile is backed up once first.
Close the world in Workbench before writing.

    python -I Tools/mud_holes.py plan   <mask.txt> [--margin 1] [--pins <pins.txt>]
    python -I Tools/mud_holes.py write  <mask.txt> --out <addon>/worlds/MudArland/Terrain/.Data
                                        (OUR world copy from mud_world_copy.py - never worlds/Arland/...:
                                        tiles there hole every Arland mission, 2026-10-10)
                                        --backup <dir> --bk <blender-mcp-BK> [--margin 1] [--pins <pins.txt>]
                                        [--install experimental]
    python -I Tools/mud_holes.py verify <census.txt> <mask.txt> [--margin 1] [--pins <pins.txt>]
    python -I Tools/mud_holes.py fit    <console.log with MUD bake sample lines> --data <DATA> --out <fit.json>
    python -I Tools/mud_holes.py bake   --data <DATA> --fit <fit.json> --out <addon>/Data/Mud/mud_heights.c
                                        (binary, named .c: game FileIO opens only .c/.conf/.et-type files)

Mask lines (file or console.log, last block wins):
    MUD mask box <x0> <z0> cells <nx> <nz> cell <size>
    MUD mask keep z <cz> x <cx0> <cx1>          (inclusive, cells from the terrain box corner)
    MUD mask done keep <n> runs <r> road <a> water <b> box <c> ms <t>

Pin lines (MUD_RelYPins.c, file or console.log, last block wins): the cell under every root entity with
EntityFlags.RELATIVE_Y stays real terrain, NOT grown by the margin. The engine adds the terrain height at the
origin XZ to such an entity's y at load and skips it when that one cell is a hole, leaving the entity at its
raw offset (IDA Exp 192624: sub_140A1CC20 -> sub_140A016D0 -> sub_140A30820; 57409 buried, 2026-10-09).
    MUD pins box <x0> <z0> cells <nx> <nz> cell <size>
    MUD pin z <cz> x <cx>
    MUD pins done pins <n> roots <r>
"""
import argparse
import json
import os
import re
import shutil
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I drops the script dir from sys.path
import ttile_holes as tt  # noqa: E402

BLOCK = 32
FULL = 0xFFFFFFFF
TILE_PATH = 'worlds/Arland/Terrain/.Data/Terrain_%d.ttile'

BOX = re.compile(r'MUD mask box \S+ \S+ cells (\d+) (\d+) cell (\S+)')
KEEP = re.compile(r'MUD mask keep z (\d+) x (\d+) (\d+)')
DONE = re.compile(r'MUD mask done keep (\d+) runs (\d+)')
PBOX = re.compile(r'MUD pins box \S+ \S+ cells (\d+) (\d+) cell (\S+)')
PIN = re.compile(r'MUD pin z (\d+) x (\d+)')
PDONE = re.compile(r'MUD pins done pins (\d+) roots (\d+)')
CBOX = re.compile(r'MUD census box .* cells (\d+) x (\d+)')
CRUN = re.compile(r'MUD census run z (\d+) x (\d+) (\d+)')
CDONE = re.compile(r'MUD census done holes (\d+) runs (\d+) ms \d+ truncated (\d)')


def is_vanilla_out(out):
    """True when `out` is the vanilla world's own tile folder (the pak path TILE_PATH comes from)."""
    tail = os.path.dirname(TILE_PATH).lower()
    return os.path.normpath(out).replace('\\', '/').rstrip('/').lower().endswith(tail)


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


def parse_pins(text):
    """Last pin block -> dict(nx, nz, cell, cells=set of cell indices z * nx + x). ValueError if incomplete."""
    lines, start = _last_block(text, PBOX)
    if start is None:
        raise ValueError('no "MUD pins box" line')
    m = PBOX.search(lines[start])
    nx, nz, cell = int(m[1]), int(m[2]), float(m[3])
    cells = set()
    done = None
    for line in lines[start + 1:]:
        r = PIN.search(line)
        if r:
            cz, cx = int(r[1]), int(r[2])
            if cz >= nz or cx >= nx:
                raise ValueError('pin z %d x %d outside the %d x %d grid' % (cz, cx, nx, nz))
            i = cz * nx + cx
            if i in cells:
                raise ValueError('pin z %d x %d listed twice' % (cz, cx))
            cells.add(i)
            continue
        d = PDONE.search(line)
        if d:
            done = int(d[1])
            break
    if done is None:
        raise ValueError('pin block has no "done" line (play stopped early?)')
    if done != len(cells):
        raise ValueError('checksum: done line says %d pins, file holds %d' % (done, len(cells)))
    return {'nx': nx, 'nz': nz, 'cell': cell, 'cells': cells}


def apply_pins(keep, nx, nz, pins):
    """Keep every pinned cell (after the margin). Returns how many of them were holes before."""
    if pins is None:
        return 0
    if (pins['nx'], pins['nz']) != (nx, nz):
        raise ValueError('size: pins %d x %d, mask %d x %d' % (pins['nx'], pins['nz'], nx, nz))
    new = 0
    for i in pins['cells']:
        if not keep[i]:
            keep[i] = 1
            new += 1
    return new


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


def full_blocks(keep, nx, nz):
    """[(bx, bz)] of every block whose 32 x 32 cells are all holes (nothing kept)."""
    out = []
    for bz in range(nz // BLOCK):
        for bx in range(nx // BLOCK):
            if not any(any(keep[(bz * BLOCK + r) * nx + bx * BLOCK:(bz * BLOCK + r) * nx + bx * BLOCK + BLOCK])
                       for r in range(BLOCK)):
                out.append((bx, bz))
    return out


def keep_one_per_full_block(keep, nx, nz, skip=()):
    """Keep the corner cell (bx*32, bz*32) of every all-hole block not in skip. Returns how many were kept.
    A block with no real terrain cell shows no grass clutter on the skin (bald 64 m square, user 2026-10-10)."""
    n = 0
    for bx, bz in full_blocks(keep, nx, nz):
        if (bx, bz) in skip:
            continue
        keep[bz * BLOCK * nx + bx * BLOCK] = 1
        n += 1
    return n


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


TILE_VERTS = 129
# (swap x/z, flip x, flip z, signed 16-bit): how a tile's HGHT is laid out, picked by fit()
ORIENTS = [(s, fx, fz, sg) for sg in (0, 1) for s in (0, 1) for fx in (0, 1) for fz in (0, 1)]
SAMPLE = re.compile(r'MUD bake sample (\d+) (\d+) (\d+) (-?\d+)\s*$')
TILE_FILE = re.compile(r'Terrain_(\d+)\.ttile$')


def hght_index(lx, lz, o):
    """Index in a tile's 129 x 129 HGHT of tile-local vertex (lx, lz) under orientation o."""
    x, z = (lz, lx) if o[0] else (lx, lz)
    if o[1]:
        x = TILE_VERTS - 1 - x
    if o[2]:
        z = TILE_VERTS - 1 - z
    return z * TILE_VERTS + x


def hght_values(tile_bytes, signed):
    d = dict(tt.chunks(tile_bytes)).get(b'HGHT', b'')
    if len(d) != 2 * TILE_VERTS * TILE_VERTS:
        raise ValueError('HGHT is %d bytes, expected %d' % (len(d), 2 * TILE_VERTS * TILE_VERTS))
    return struct.unpack('<%d%s' % (TILE_VERTS * TILE_VERTS, 'h' if signed else 'H'), d)


def vertex_local(bx, bz, v):
    """Tile-local vertex of skin chunk / hole block (bx, bz), chunk vertex v (33 x 33, x fastest)."""
    return (bx % 4) * BLOCK + v % (BLOCK + 1), (bz % 4) * BLOCK + v // (BLOCK + 1)


def parse_samples(text):
    """'MUD bake sample <ix> <iz> <v> <mm>' lines (MUD_CoarseSkin.c TEMP) -> [(ix, iz, v, mm)]."""
    out = []
    for line in text.splitlines():
        m = SAMPLE.search(line)
        if m:
            out.append(tuple(int(g) for g in m.groups()))
    return out


def linfit(us, hs):
    """Least squares h = a * u + b -> (a, b, max residual), None if u is constant."""
    n = len(us)
    mu = sum(us) / n
    mh_ = sum(hs) / n
    sxx = sum((u - mu) ** 2 for u in us)
    if sxx == 0:
        return None
    a = sum((u - mu) * (h - mh_) for u, h in zip(us, hs)) / sxx
    b = mh_ - a * mu
    return a, b, max(abs(a * u + b - h) for u, h in zip(us, hs))


def fit(samples, load_tile):
    """Best orientation + scale over all ORIENTS: {'orient', 'a', 'b', 'resid', 'samples'}."""
    cache = {}
    best = None
    for o in ORIENTS:
        us, hs = [], []
        for ix, iz, v, mm in samples:
            t = owner_tile(ix, iz)
            if (t, o[3]) not in cache:
                cache[(t, o[3])] = hght_values(load_tile(t), o[3])
            lx, lz = vertex_local(ix, iz, v)
            us.append(cache[(t, o[3])][hght_index(lx, lz, o)])
            hs.append(mm / 1000.0)
        r = linfit(us, hs)
        if r and (best is None or r[2] < best['resid']):
            best = {'orient': list(o), 'a': r[0], 'b': r[1], 'resid': r[2], 'samples': len(samples)}
    return best


def bake(data_dir, fitd, out_path):
    """Heights of every hole block in data_dir's tiles -> MUDH file. ValueError on a block in the wrong tile."""
    o, a, b = fitd['orient'], fitd['a'], fitd['b']
    blocks = []
    tiles = 0
    for name in sorted(os.listdir(data_dir)):
        m = TILE_FILE.search(name)
        if not m:
            continue
        t = int(m.group(1))
        with open(os.path.join(data_dir, name), 'rb') as f:
            data = f.read()
        parts = dict(tt.chunks(data))
        if b'HOLE' not in parts:
            continue
        tiles += 1
        vals = hght_values(data, o[3])
        for (bx, bz), rows in tt.decode(parts[b'HOLE']).items():
            if not any(rows):
                continue
            if owner_tile(bx, bz) != t:
                raise ValueError('block %d %d in Terrain_%d, owner is %d' % (bx, bz, t, owner_tile(bx, bz)))
            hs = [a * vals[hght_index(*vertex_local(bx, bz, v), o)] + b for v in range((BLOCK + 1) ** 2)]
            blocks.append((bz, bx, hs))
    blocks.sort(key=lambda e: (e[0], e[1]))
    with open(out_path, 'wb') as f:
        f.write(b'MUDH' + struct.pack('<iii', 1, len(blocks), BLOCK))
        for bz, bx, hs in blocks:
            f.write(struct.pack('<ii', bx, bz) + struct.pack('<%df' % len(hs), *hs))
    return {'blocks': len(blocks), 'tiles': tiles, 'bytes': 16 + len(blocks) * (8 + 4 * (BLOCK + 1) ** 2)}


def merge_tile(data, blocks, allow_full=(), refuse_full=False):
    """Vanilla .ttile bytes + hole blocks -> new bytes (holes OR'ed). ValueError if the layout is unknown or a
    block ends up all holes (no grass clutter on it, see keep_one_per_full_block) and is not in allow_full, when
    refuse_full (the CLI write sets it)."""
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
    full = [k for k, rows in have.items() if all(r == FULL for r in rows) and k not in allow_full]
    if refuse_full and full:
        raise ValueError('block(s) %s would be full holes (bald grass) - keep a cell' % full[:5])
    payload = tt.encode(have)
    if b'HOLE' in tags:
        parts = [(t, payload if t == b'HOLE' else d) for t, d in parts]
    else:  # chunk order of saved tiles: VERS HGHT BERR HOLE ... -> after BERR
        parts.insert(tags.index(b'BERR') + 1, (b'HOLE', payload))
    out = tt.build(parts)
    if tt.decode(dict(tt.chunks(out))[b'HOLE']) != {k: v for k, v in have.items() if any(v)}:
        raise ValueError('written HOLE chunk does not read back')
    return out


def write_tiles(blocks, out_dir, backup_dir, load_vanilla, tile_blocks=4, tiles_x=16, allow_full=(), refuse_full=False):
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
        data = merge_tile(load_vanilla(t), mine, allow_full, refuse_full)
        with open(path, 'wb') as f:
            f.write(data)
        report.append((t, len(mine), sum(bin(r).count('1') for rows in mine.values() for r in rows)))
    return report


def stats(mask, margin, pins=None, control=(), keep_full=False):
    keep = dilate(mask['keep'], mask['nx'], mask['nz'], margin)
    pins_new = apply_pins(keep, mask['nx'], mask['nz'], pins)
    full_kept = keep_one_per_full_block(keep, mask['nx'], mask['nz'], control) if keep_full else 0
    blocks = hole_blocks(keep, mask['nx'], mask['nz'])
    k = sum(keep)
    return {'keep': k, 'holes': mask['nx'] * mask['nz'] - k, 'blocks': len(blocks),
            'tiles': len({owner_tile(bx, bz) for bx, bz in blocks}), 'keep_grid': keep, 'block_map': blocks,
            'pins_new': pins_new, 'full_kept': full_kept}


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


def verify(census_text, mask_text, margin, pins_text=None, control=(), keep_full=False):
    mask = parse_mask(mask_text)
    nx, nz, hole = parse_census_grid(census_text)
    if (nx, nz) != (mask['nx'], mask['nz']):
        raise ValueError('size: census %d x %d, mask %d x %d' % (nx, nz, mask['nx'], mask['nz']))
    keep = dilate(mask['keep'], nx, nz, margin)
    apply_pins(keep, nx, nz, parse_pins(pins_text) if pins_text is not None else None)
    if keep_full:
        keep_one_per_full_block(keep, nx, nz, control)
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
    ap.add_argument('cmd', choices=['plan', 'write', 'verify', 'fit', 'bake'])
    ap.add_argument('files', nargs='*')
    ap.add_argument('--margin', type=int, default=1)
    ap.add_argument('--out')
    ap.add_argument('--backup')
    ap.add_argument('--bk')
    ap.add_argument('--install', default='experimental')
    ap.add_argument('--pins')
    ap.add_argument('--data')
    ap.add_argument('--fit')
    ap.add_argument('--no-pins', action='store_true', help='write without pins (buries RELATIVE_Y entities on holes)')
    ap.add_argument('--control', action='append', default=[], metavar='BX,BZ',
                    help='TEST ONLY: leave this all-hole block full (stays bald) as a control')
    a = ap.parse_args(argv)
    control = {tuple(int(v) for v in c.split(',')) for c in a.control}
    read = lambda p: open(p, encoding='utf-8', errors='replace').read()  # noqa: E731

    def load_tile(t):
        with open(os.path.join(a.data, 'Terrain_%d.ttile' % t), 'rb') as f:
            return f.read()

    if a.cmd == 'fit':
        if not (a.files and a.data):
            ap.error('fit needs <console.log> and --data')
        samples = parse_samples(read(a.files[0]))
        if not samples:
            print('no MUD bake sample lines')
            return 1
        r = fit(samples, load_tile)
        ok = r['resid'] < 0.002
        print('fit samples %d orient %s a %.9f b %.6f max residual %.6f m %s'
              % (r['samples'], r['orient'], r['a'], r['b'], r['resid'], 'FIT OK' if ok else 'FIT FAIL'))
        if a.out and ok:  # a failed fit must never reach bake
            with open(a.out, 'w') as f:
                json.dump(r, f)
        return 0 if ok else 1

    if a.cmd == 'bake':
        if not (a.data and a.fit and a.out):
            ap.error('bake needs --data, --fit and --out')
        with open(a.fit) as f:
            r = bake(a.data, json.load(f), a.out)
        print('baked %(blocks)d blocks from %(tiles)d tiles, %(bytes)d bytes' % r)
        return 0

    if not a.files:
        ap.error('%s needs its input file(s)' % a.cmd)

    if a.cmd == 'verify':
        r = verify(read(a.files[0]), read(a.files[1]), a.margin, read(a.pins) if a.pins else None, control, keep_full=True)
        print('expected holes %(expected)d observed %(observed)d missing %(missing)d extra %(extra)d' % r)
        print('VERIFY OK' if r['missing'] == 0 and r['extra'] == 0 else 'VERIFY FAIL')
        return 0 if r['missing'] == 0 and r['extra'] == 0 else 1

    mask = parse_mask(read(a.files[0]))
    pins = parse_pins(read(a.pins)) if a.pins else None
    s = stats(mask, a.margin, pins, control, keep_full=True)
    n = mask['nx'] * mask['nz']
    if pins:
        print('pins %d, of those holes before %d' % (len(pins['cells']), s['pins_new']))
    print('all-hole blocks given one kept corner cell %d, left full as control %d %s'
          % (s['full_kept'], len(control), sorted(control)))
    print('grid %d x %d = %d cells, margin %d: keep %d + holes %d = %d (%s), %d blocks in %d tiles'
          % (mask['nx'], mask['nz'], n, a.margin, s['keep'], s['holes'], s['keep'] + s['holes'],
             'ok' if s['keep'] + s['holes'] == n else 'MISMATCH', s['blocks'], s['tiles']))
    if a.cmd == 'plan':
        return 0

    if not (a.out and a.backup and a.bk):
        ap.error('write needs --out, --backup and --bk')
    if is_vanilla_out(a.out):
        ap.error('--out is the VANILLA tile folder: tiles there hole every Arland mission (2026-10-10). '
                 'Write into our world copy (Tools/mud_world_copy.py), e.g. <addon>/worlds/MudArland/Terrain/.Data')
    if not a.pins and not a.no_pins:
        # without pins every RELATIVE_Y root on a hole loads at its raw offset (57409 buried, 2026-10-09)
        ap.error('write needs --pins <pins.txt> (MUD_RelYPins.c) or --no-pins on purpose')
    sys.path.insert(0, os.path.join(a.bk, 'tools'))
    import pakread
    pak = pakread.Pak(a.install)
    report = write_tiles(s['block_map'], a.out, a.backup, lambda t: pak.read(TILE_PATH % t), allow_full=control,
                         refuse_full=True)
    for t, nb, cells in report:
        print('Terrain_%d.ttile blocks %d hole cells %d' % (t, nb, cells))
    total = sum(c for _, _, c in report)
    print('wrote %d tiles, hole cells %d (%s)' % (len(report), total, 'ok' if total == s['holes'] else 'MISMATCH'))
    return 0 if total == s['holes'] else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
