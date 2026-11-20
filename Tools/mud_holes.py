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
    python -I Tools/mud_holes.py area   <new mask> <old mask> --pins <new pins> --old-pins <old pins>
                                        --rect <x0,z0,x1,z1 world m> --out-mask <f> --out-pins <f>
                                        (test area, 2026-10-17: new mask + pins inside the rect, old outside;
                                        feed the two files to plan / write)
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
raw offset (Exp 192624; 57409 buried, 2026-10-09).
    MUD pins box <x0> <z0> cells <nx> <nz> cell <size>
    MUD pin z <cz> x <cx>
    MUD pins done pins <n> roots <r>
"""
import argparse
import json
import os
import json
import re
import shutil
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I drops the script dir from sys.path
import ttile_holes as tt  # noqa: E402

TOOLS_VERSION = 12   # = mud_build.TOOLS_VERSION (see there)
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


def box_origin(text, box_re):
    """(x0, z0) world metres of the last block's box line."""
    lines, start = _last_block(text, box_re)
    if start is None:
        raise ValueError('no box line')
    m = re.search(r'box (\S+) (\S+) cells', lines[start])
    return float(m[1]), float(m[2])


def rect_cells(rect, x0, z0, cell, nx, nz):
    """World rect (x0, z0, x1, z1) -> cells (cx0, cz0, cx1, cz1), end exclusive, every cell the rect touches."""
    import math
    a = max(0, int(math.floor((rect[0] - x0) / cell)))
    c = max(0, int(math.floor((rect[1] - z0) / cell)))
    b = min(nx, int(math.ceil((rect[2] - x0) / cell)))
    d = min(nz, int(math.ceil((rect[3] - z0) / cell)))
    if a >= b or c >= d:
        raise ValueError('rect %s holds no cell' % (rect,))
    return a, c, b, d


def merge_area(new_mask, old_mask, new_pins, old_pins, cells):
    """New mask + pins inside cells (cx0, cz0, cx1, cz1, end exclusive), old ones outside -> (mask, pins)."""
    nx, nz = old_mask['nx'], old_mask['nz']
    for name, d in (('new mask', new_mask), ('new pins', new_pins), ('old pins', old_pins)):
        if (d['nx'], d['nz']) != (nx, nz):
            raise ValueError('size: %s %d x %d, old mask %d x %d' % (name, d['nx'], d['nz'], nx, nz))
    cx0, cz0, cx1, cz1 = cells
    keep = bytearray(old_mask['keep'])
    for z in range(cz0, cz1):
        keep[z * nx + cx0:z * nx + cx1] = new_mask['keep'][z * nx + cx0:z * nx + cx1]

    def inside(i):
        return cz0 <= i // nx < cz1 and cx0 <= i % nx < cx1
    pins = {i for i in old_pins['cells'] if not inside(i)} | {i for i in new_pins['cells'] if inside(i)}
    return ({'nx': nx, 'nz': nz, 'cell': old_mask['cell'], 'keep': keep},
            {'nx': nx, 'nz': nz, 'cell': old_mask['cell'], 'cells': pins})


def mask_text_of(mask, x0, z0):
    """Mask dict -> MUD_HoleMask text (parse_mask reads it back)."""
    nx, nz, keep = mask['nx'], mask['nz'], mask['keep']
    out = ['MUD mask 1', 'MUD mask box %g %g cells %d %d cell %g' % (x0, z0, nx, nz, mask['cell'])]
    runs = 0
    for z in range(nz):
        x = 0
        while x < nx:
            if keep[z * nx + x]:
                a = x
                while x < nx and keep[z * nx + x]:
                    x += 1
                out.append('MUD mask keep z %d x %d %d' % (z, a, x - 1))
                runs += 1
            else:
                x += 1
    out.append('MUD mask done keep %d runs %d road 0 water 0 box 0 ms 0' % (sum(keep), runs))
    return '\n'.join(out) + '\n'


def pins_text_of(pins, x0, z0):
    """Pins dict -> MUD_RelYPins text (parse_pins reads it back)."""
    nx = pins['nx']
    out = ['MUD pins box %g %g cells %d %d cell %g' % (x0, z0, nx, pins['nz'], pins['cell'])]
    out += ['MUD pin z %d x %d' % (i // nx, i % nx) for i in sorted(pins['cells'])]
    out.append('MUD pins done pins %d roots %d' % (len(pins['cells']), len(pins['cells'])))
    return '\n'.join(out) + '\n'


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


def owner_tile(bx, bz, tile_blocks=None, tiles_x=None):
    """Tile index of the .ttile that owns block (bx, bz): x-fastest (measured, Arland Exp 192624)."""
    tile_blocks = TILE_BLOCKS if tile_blocks is None else tile_blocks
    tiles_x = TILES_X if tiles_x is None else tiles_x
    return (bz // tile_blocks) * tiles_x + bx // tile_blocks


# Tile layout. Arland (Exp 192624): 16 x 16 tiles of 129 x 129 vertices = 4 x 4 hole blocks each. Another map:
# set_layout (Tools/mud_build.py reads the vertex count from a tile's HGHT and gets tiles_x from the World Editor).
TILE_VERTS = 129
TILE_BLOCKS = 4
TILES_X = 16


def set_layout(tile_verts, tiles_x):
    """Use another map's tile layout: tile_verts per tile side (32 * n + 1), tiles_x tiles per row."""
    global TILE_VERTS, TILE_BLOCKS, TILES_X
    if tile_verts < BLOCK + 1 or (tile_verts - 1) % BLOCK:
        raise ValueError('tile has %d vertices a side, want 32 * n + 1' % tile_verts)
    TILE_VERTS, TILE_BLOCKS, TILES_X = tile_verts, (tile_verts - 1) // BLOCK, tiles_x
# (swap x/z, flip x, flip z, signed 16-bit): how a tile's HGHT is laid out, picked by fit()
ORIENTS = [(s, fx, fz, sg) for sg in (0, 1) for s in (0, 1) for fx in (0, 1) for fz in (0, 1)]
SAMPLE = re.compile(r'MUD bake sample (\d+) (\d+) (\d+) (-?\d+)\s*$')
TILE_FILE = re.compile(r'_(\d+)\.ttile$')   # <terrain name>_<n>.ttile (Arland / Cain 'Terrain', Everon 'Eden')


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
    return (bx % TILE_BLOCKS) * BLOCK + v % (BLOCK + 1), (bz % TILE_BLOCKS) * BLOCK + v // (BLOCK + 1)


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


FIT_TOL = 0.002     # m, = mud_build.FIT_MAX
CENTRE_V = (BLOCK // 2) * (BLOCK + 1) + BLOCK // 2   # the 33 x 33 chunk's centre vertex (never lowered)
CENTRE_MIN = 16     # centre samples needed for the centre-only fit


def fit(samples, load_tile):
    """Best orientation + scale over all ORIENTS: {'orient', 'a', 'b', 'resid', 'samples'}.
    When all samples do not fit, the fit is tried again on the chunk-centre samples only ('centre' True): a
    re-Build takes its samples from tiles whose hidden heights under holes were lowered (lower_tile) while the pak
    tiles it fits against were not; centre vertices are never lowered."""
    best = _fit(samples, load_tile)
    if best is not None and best['resid'] < FIT_TOL:
        return best
    centre = [s for s in samples if s[2] == CENTRE_V]
    if CENTRE_MIN <= len(centre) < len(samples):
        alt = _fit(centre, load_tile)
        if alt is not None and (best is None or alt['resid'] < best['resid']):
            alt['centre'] = True
            return alt
    return best


def _fit(samples, load_tile):
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


CELLS_MAGIC = b'MUDC'
CODE_NONE = 127
CMAT = re.compile(r'^MUD cellmat (\d+) (.+?)\s*$')
CMDONE = re.compile(r'^MUD cellmats done (\d+)\s*$')


def parse_cells(data, mats_text):
    """Build scan cell file + its gamemat list (MUD_KeepScan CELL_FILE, LOAD BAKE 2026-11-03) ->
    {'nx', 'nz', 'codes' (bytes nx * nz, cell z * nx + x: bits 0-6 gamemat index, 127 none, bit 7 tl-br), 'mats'}.
    ValueError when either is broken (scan stopped early, an index past the list, a name not ASCII)."""
    if len(data) < 20 or data[:4] != CELLS_MAGIC:
        raise ValueError('cell file: no MUDC header')
    ver, nx, nz, words = struct.unpack('<iiii', data[4:20])
    if ver != 1 or nx <= 0 or nz <= 0 or words != (nx + 3) // 4:
        raise ValueError('cell file: version %d grid %d x %d ints per row %d' % (ver, nx, nz, words))
    if len(data) != 20 + nz * words * 4:
        raise ValueError('cell file: %d bytes, want %d (scan stopped early?)' % (len(data), 20 + nz * words * 4))
    rowb = words * 4
    codes = bytearray(nx * nz)
    for z in range(nz):
        at = 20 + z * rowb
        codes[z * nx:(z + 1) * nx] = data[at:at + nx]
    mats, done = [], None
    for line in mats_text.splitlines():
        m = CMAT.match(line.strip())
        if m:
            if int(m[1]) != len(mats):
                raise ValueError('cell mats: index %s out of order' % m[1])
            name = m[2]
            name.encode('ascii')    # UnicodeEncodeError is a ValueError
            if not name.endswith('.gamemat'):
                raise ValueError('cell mats: %r is no .gamemat' % name)
            mats.append(name)
            continue
        d = CMDONE.match(line.strip())
        if d:
            done = int(d[1])
    if done is None or done != len(mats):
        raise ValueError('cell mats: done line %s, list holds %d' % (done, len(mats)))
    if len(mats) >= CODE_NONE:
        raise ValueError('cell mats: %d names, at most %d fit' % (len(mats), CODE_NONE - 1))
    used = {c & CODE_NONE for c in set(codes)} - {CODE_NONE}
    if used and max(used) >= len(mats):
        raise ValueError('cell file: gamemat index %d, the list holds %d' % (max(used), len(mats)))
    return {'nx': nx, 'nz': nz, 'codes': bytes(codes), 'mats': mats}


def mats_bytes(mats):
    """Version 2 gamemat table: count, then per name its byte length + the ASCII bytes padded to whole ints."""
    out = struct.pack('<i', len(mats))
    for name in mats:
        enc = name.encode('ascii')
        out += struct.pack('<i', len(enc)) + enc + b'\0' * (-len(enc) % 4)
    return out


def bake(data_dir, fitd, out_path, cells=None, tiles=None):
    """Heights of every hole block in data_dir's tiles -> MUDH file. ValueError on a block in the wrong tile.
    With cells (parse_cells of the same grid): version 2 = also per block its 32 hole rows and 1024 cell codes
    (LOAD BAKE: MUD_CoarseSkin.SampleChunk then needs no GetSurfaceY and no trace).
    tiles {file name: bytes}: use these bytes instead of the file (mud_build: the tiles BEFORE lower_tile, so the
    skin is baked from the real heights, never the lowered hidden ones)."""
    o, a, b = fitd['orient'], fitd['a'], fitd['b']
    tiles_in = tiles
    blocks = []
    tiles = 0
    for name in sorted(os.listdir(data_dir)):
        m = TILE_FILE.search(name)
        if not m:
            continue
        t = int(m.group(1))
        if tiles_in is not None and name in tiles_in:
            data = tiles_in[name]
        else:
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
                raise ValueError('block %d %d in %s, owner is %d' % (bx, bz, name, owner_tile(bx, bz)))
            hs = [a * vals[hght_index(*vertex_local(bx, bz, v), o)] + b for v in range((BLOCK + 1) ** 2)]
            blocks.append((bz, bx, hs, rows))
    blocks.sort(key=lambda e: (e[0], e[1]))
    if cells is None:
        with open(out_path, 'wb') as f:
            f.write(b'MUDH' + struct.pack('<iii', 1, len(blocks), BLOCK))
            for bz, bx, hs, _ in blocks:
                f.write(struct.pack('<ii', bx, bz) + struct.pack('<%df' % len(hs), *hs))
        return {'blocks': len(blocks), 'tiles': tiles, 'bytes': 16 + len(blocks) * (8 + 4 * (BLOCK + 1) ** 2)}
    nx, nz, codes = cells['nx'], cells['nz'], cells['codes']
    table = mats_bytes(cells['mats'])
    with open(out_path, 'wb') as f:
        f.write(b'MUDH' + struct.pack('<iii', 2, len(blocks), BLOCK) + table)
        for bz, bx, hs, rows in blocks:
            if (bx + 1) * BLOCK > nx or (bz + 1) * BLOCK > nz:
                raise ValueError('block %d %d outside the cell file grid %d x %d' % (bx, bz, nx, nz))
            code = b''.join(codes[(bz * BLOCK + r) * nx + bx * BLOCK:(bz * BLOCK + r) * nx + bx * BLOCK + BLOCK]
                            for r in range(BLOCK))
            f.write(struct.pack('<ii', bx, bz) + struct.pack('<%df' % len(hs), *hs)
                    + struct.pack('<%dI' % BLOCK, *rows) + code)
    size = 16 + len(table) + len(blocks) * (8 + 4 * (BLOCK + 1) ** 2 + 4 * BLOCK + BLOCK * BLOCK)
    return {'blocks': len(blocks), 'tiles': tiles, 'bytes': size, 'version': 2, 'mats': len(cells['mats'])}


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


# ---------------------------------------------------------------------------------------------------------------
# Lowered hidden heights under holes (TRENCH SKY GAP, retail look check OK 2026-11-14, docs/features/trenches-tunnels
# README 'TRENCH SEE-THROUGH'). The retail terrain occlusion raymarch reads the raw HGHT heights and ignores holes,
# so the hidden heightmap inside a holed hill hid far trench walls (sky showed). Every vertex whose 4 cells are ALL
# our holes goes down LOWER_DROP m (never under LOWER_FLOOR m = sea level + 1 m, never up). Kept: each 64 m block's
# centre vertex (MUD_AutoGround.Fits + MUD_CoarseSkin's stale check compare it with the heightmap) and every vertex
# that touches a real cell (kept ground and roads must not bend). The skin is baked from the heights BEFORE the
# lowering (bake tiles=), and the lowering is written to <backup>/LOWER_FILE so a re-Build / Unbuild can undo it.
LOWER_DROP = 1.0   # m (user 2026-11-19, was 12): 3 m+ broke the grass (GrassManager cluster boxes follow these heights); the sky slit over deeper digs is closed by MUD_VoxelProbe.SKY_BOX instead
LOWER_FLOOR = 1.0   # m
KEEP_BLOCK_TOP = True   # also keep each block's highest vertex (block_tops: clutter block box, CLUTTER FLICKER 2026-11-18)
LOWER_FILE = 'lowered.txt'


def lower_params(fitd, drop=LOWER_DROP, floor=LOWER_FLOOR):
    """What lower_tile / unlower_tile need: the fit (orient, a, b) and the drop."""
    return {'orient': [int(v) for v in fitd['orient']], 'a': float(fitd['a']), 'b': float(fitd['b']),
            'drop': float(drop), 'floor': float(floor)}


def write_lowered(backup_dir, lw):
    os.makedirs(backup_dir, exist_ok=True)
    with open(os.path.join(backup_dir, LOWER_FILE), 'w', encoding='utf-8', newline='\n') as f:
        f.write(json.dumps(lw, sort_keys=True) + '\n')


def read_lowered(backup_dir):
    """The lower_params a Build wrote into backup_dir, None when it lowered nothing (or an older Tools build)."""
    p = os.path.join(backup_dir, LOWER_FILE)
    if not os.path.exists(p):
        return None
    with open(p, encoding='utf-8') as f:
        lw = json.loads(f.read())
    for k in ('orient', 'a', 'b', 'drop', 'floor'):
        if k not in lw:
            raise ValueError('%s has no %r' % (p, k))
    return lw


def lowered_value(u, lw, lo, hi):
    """Stored height u after the lowering (u itself when it would not go down)."""
    a, b = lw['a'], lw['b']
    if a == 0:
        return u
    y = a * u + b
    ny = max(y - lw['drop'], lw['floor'])
    if ny >= y:
        return u
    nu = max(lo, min(hi, int(round((ny - b) / a))))
    return nu if a * nu + b < y else u


def hole_rows(block_map):
    """{cell row z: int with bit x set for every hole cell x} of hole blocks {(bx, bz): 32 row bitmasks}."""
    rows = {}
    for (bx, bz), rs in block_map.items():
        for r, bits in enumerate(rs):
            if bits:
                z = bz * BLOCK + r
                rows[z] = rows.get(z, 0) | (bits << (bx * BLOCK))
    return rows


def _hght(data, signed):
    parts = tt.chunks(data)
    if tt.build(parts) != data:
        raise ValueError('round trip failed - unknown layout, not writing')
    d = dict(parts).get(b'HGHT', b'')
    n = len(d) // 2
    if n < 4 or 2 * n != len(d):
        raise ValueError('HGHT is %d bytes' % len(d))
    fmt = '<%d%s' % (n, 'h' if signed else 'H')
    return parts, list(struct.unpack(fmt, d)), fmt, ((-32768, 32767) if signed else (0, 65535))


def block_tops(vals, o, a):
    """HGHT indices of each 32-cell block's highest vertex in this tile (tile-edge vertices left out: the next tile
    holds its own copy of them, so a keep there could differ). lower_tile keeps them: the engine's clutter block box
    is the block's heightmap min / max (engine, Stable), so with every high vertex lowered the box top
    sank under our skin and the block's grass was culled while on screen (CLUTTER FLICKER, user WB 2026-11-18)."""
    side = TILE_VERTS - 1
    sign = 1 if a >= 0 else -1
    tops = set()
    for bz in range(0, side, BLOCK):
        for bx in range(0, side, BLOCK):
            best, bi = None, -1
            for lz in range(max(bz, 1), min(bz + BLOCK, side - 1) + 1):
                for lx in range(max(bx, 1), min(bx + BLOCK, side - 1) + 1):
                    i = hght_index(lx, lz, o)
                    h = sign * vals[i]
                    if best is None or h > best:
                        best, bi = h, i
            if bi >= 0:
                tops.add(bi)
    return tops


def lower_tile(data, t, rows, lw):
    """(.ttile bytes with the hidden heights under holes lowered, vertices lowered). rows = hole_rows of the holes
    that may lower (ours), in the cell grid of the whole terrain; t = tile index (set_layout's layout)."""
    o = lw['orient']
    parts, vals, fmt, (lo, hi) = _hght(data, o[3])
    if len(vals) != TILE_VERTS * TILE_VERTS:
        raise ValueError('HGHT holds %d vertices, layout says %d a side' % (len(vals), TILE_VERTS))
    side = TILE_VERTS - 1
    x0, z0 = (t % TILES_X) * side, (t // TILES_X) * side
    span = (1 << TILE_VERTS) - 1
    half = BLOCK // 2
    keep = block_tops(vals, o, lw['a']) if KEEP_BLOCK_TOP else set()
    n = 0
    for lz in range(TILE_VERTS):
        gz = z0 + lz
        m = rows.get(gz - 1, 0) & rows.get(gz, 0)
        if not m:
            continue
        v = ((m & (m << 1)) >> x0) & span   # bit lx: cells gx - 1 and gx (rows gz - 1 and gz) are all holes
        while v:
            low = v & -v
            v ^= low
            lx = low.bit_length() - 1
            if (x0 + lx) % BLOCK == half and gz % BLOCK == half:
                continue
            i = hght_index(lx, lz, o)
            if i in keep:
                continue
            nu = lowered_value(vals[i], lw, lo, hi)
            if nu != vals[i]:
                vals[i] = nu
                n += 1
    if not n:
        return data, 0
    hb = struct.pack(fmt, *vals)
    return tt.build([(tag, hb if tag == b'HGHT' else p) for tag, p in parts]), n


def unlower_tile(cur, pristine, lw):
    """cur with the pristine height back at every vertex where cur holds exactly the lowered pristine height
    (lowered_value); a height the map maker edited after Build differs and stays."""
    signed = lw['orient'][3]
    parts, now, fmt, (lo, hi) = _hght(cur, signed)
    old = _hght(pristine, signed)[1]
    if len(old) != len(now) or old == now:
        return cur
    n = 0
    for i, (c, p) in enumerate(zip(now, old)):
        if c != p and c == lowered_value(p, lw, lo, hi):
            now[i] = p
            n += 1
    if not n:
        return cur
    hb = struct.pack(fmt, *now)
    return tt.build([(tag, hb if tag == b'HGHT' else p) for tag, p in parts])


def hole_cells(data):
    """Hole cells in .ttile bytes (0 without a HOLE chunk)."""
    parts = dict(tt.chunks(data))
    if b'HOLE' not in parts:
        return 0
    return sum(bin(r).count('1') for rows in tt.decode(parts[b'HOLE']).values() for r in rows)


def reset_holes(cur, pristine):
    """Current .ttile bytes with the HOLE chunk of the pristine copy (none there = no HOLE chunk). Heights, paint
    and every other chunk stay as they are now, so map edits made after Build survive Unbuild / a re-Build."""
    for name, d in (('current', cur), ('pristine', pristine)):
        if d[:4] != b'FORM' or d[8:12] != b'TERR':
            raise ValueError('%s tile is not a FORM/TERR file' % name)
    parts = tt.chunks(cur)
    if tt.build(parts) != cur:
        raise ValueError('round trip failed - unknown layout, not writing')
    old = dict(tt.chunks(pristine)).get(b'HOLE')
    tags = [t for t, _ in parts]
    if old is None:
        parts = [(t, d) for t, d in parts if t != b'HOLE']
    elif b'HOLE' in tags:
        parts = [(t, old if t == b'HOLE' else d) for t, d in parts]
    else:  # chunk order of saved tiles: VERS HGHT BERR HOLE ... -> after BERR
        parts.insert(tags.index(b'BERR') + 1, (b'HOLE', old))
    return tt.build(parts)


def tile_names(d):
    """{tile index: file name} of the <terrain name>_<n>.ttile files in folder d (one terrain per .Data folder)."""
    out = {}
    if os.path.isdir(d):
        for name in os.listdir(d):
            m = re.fullmatch(r'.+_(\d+)\.ttile', name)
            if m:
                out[int(m[1])] = name
    return out


def snapshot(tiles_dir, backup_dir):
    """Copy every tile of tiles_dir that backup_dir does not hold yet (the first copy = the pre-tool state).
    Returns (copied, total in backup)."""
    os.makedirs(backup_dir, exist_ok=True)
    have = tile_names(backup_dir)
    copied = 0
    for t, name in sorted(tile_names(tiles_dir).items()):
        if t not in have:
            shutil.copyfile(os.path.join(tiles_dir, name), os.path.join(backup_dir, name))
            copied += 1
    return copied, len(tile_names(backup_dir))


HOLED_MIN = BLOCK * BLOCK   # review 2 E4: below this many planned cells in the new tiles the check says nothing


def holed_already(tiles_dir, backup_dir, block_map):
    """Review 2 E4: the tiles snapshot() would copy now (none in backup_dir yet) vs our planned holes (stats block_map).
    Returns (cells holed in those tiles that we also plan to hole, cells we plan in those tiles). A lost profile backup
    + already-holed tiles = overlap close to planned; a map's own holes are few (Belleau Wood 56 cells, 2026-11-01).
    A block holed WHOLE is the map's own (our cut always keeps a cell, merge_tile refuse_full): left out of both counts."""
    have = tile_names(backup_dir)
    new = {t: name for t, name in tile_names(tiles_dir).items() if t not in have}
    if not new:
        return 0, 0
    holes = {}
    for t, name in sorted(new.items()):
        with open(os.path.join(tiles_dir, name), 'rb') as f:
            holes.update(tile_holes(f.read()))
    overlap = planned = 0
    for k, mine in block_map.items():
        if owner_tile(k[0], k[1]) not in new:
            continue
        rows = holes.get(k)
        if rows and all(r == FULL for r in rows):
            continue
        planned += sum(bin(r).count('1') for r in mine)
        if rows:
            overlap += sum(bin(a & b).count('1') for a, b in zip(rows, mine))
    return overlap, planned


def restore(tiles_dir, backup_dir, lw=None):
    """Put the pristine HOLE chunk (backup_dir) back into every current tile (reset_holes), and with lw (read_lowered)
    the pristine height back at every vertex lower_tile lowered (unlower_tile). Returns
    [(tile, hole cells before, after)]; a backup tile with no current tile is copied back whole (after = -1)."""
    cur = tile_names(tiles_dir)
    report = []
    for t, name in sorted(tile_names(backup_dir).items()):
        with open(os.path.join(backup_dir, name), 'rb') as f:
            pristine = f.read()
        path = os.path.join(tiles_dir, cur.get(t, name))
        if t not in cur:
            with open(path, 'wb') as f:
                f.write(pristine)
            report.append((t, 0, -1))
            continue
        with open(path, 'rb') as f:
            now = f.read()
        out = reset_holes(now, pristine)
        if lw:
            out = unlower_tile(out, pristine, lw)
        if out != now:
            with open(path, 'wb') as f:
                f.write(out)
        report.append((t, hole_cells(now), hole_cells(out)))
    return report


def write_tiles(blocks, out_dir, backup_dir, load_vanilla, tile_blocks=None, tiles_x=None, allow_full=(), refuse_full=False):
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


def stats(mask, margin, pins=None, control=(), keep_full=False, boxes=None):
    """boxes = box_grids(...) result or None (None = the result before boxes existed, byte for byte)."""
    if boxes is None:
        keep = dilate(mask['keep'], mask['nx'], mask['nz'], margin)
        pins_new = apply_pins(keep, mask['nx'], mask['nz'], pins)
        full_kept = keep_one_per_full_block(keep, mask['nx'], mask['nz'], control) if keep_full else 0
        extra = {}
    else:
        c = compose(mask, margin, pins, boxes, control, keep_full)
        keep, pins_new, full_kept = c['keep_grid'], c['pins_new'], c['full_kept']
        extra = {'box_cut': c['box_cut'], 'box_keep': c['box_keep']}
    blocks = hole_blocks(keep, mask['nx'], mask['nz'])
    k = sum(keep)
    out = {'keep': k, 'holes': mask['nx'] * mask['nz'] - k, 'blocks': len(blocks),
           'tiles': len({owner_tile(bx, bz) for bx, bz in blocks}), 'keep_grid': keep, 'block_map': blocks,
           'pins_new': pins_new, 'full_kept': full_kept}
    out.update(extra)
    return out


# ---------------------------------------------------------------- keep / cut boxes (plan 2026-11-14 cut preview)
# boxes.txt (MUD_GroundBuildPlugin.WriteBoxes, from the OPEN world), world metres, XZ only:
#   MUD boxes 1
#   MUD box <keep|cut> <cx> <cz> <rx> <rz> <fx> <fz> <hx> <hz>     c origin, r / f transform rows 0 / 2 (unnormalised:
#   MUD boxes done <n>                                              scale counts), h half size in local units
# A cell is inside when its CENTRE p solves p - c = lx * r + lz * f with |lx| <= hx and |lz| <= hz
# (= MUD_KeepScan.InBox for an upright box). Keep box = MUD_CutBoxEntity, cut box = MUD_MudBoxEntity.
BXHEAD = re.compile(r'MUD boxes 1\s*$')
BXLINE = re.compile(r'MUD box (keep|cut)((?: \S+){8})\s*$')
BXDONE = re.compile(r'MUD boxes done (\d+)\s*$')
BOX_EPS = 1e-6
# preview codes (compose codes=True); 0 = hole the scan made, not listed
P_SCAN, P_MARGIN, P_PIN, P_KEEPBOX, P_CUTBOX, P_CORNER = 1, 2, 3, 4, 5, 6
P_KEPT = (P_SCAN, P_MARGIN, P_PIN, P_KEEPBOX, P_CORNER)


def parse_boxes(text):
    """boxes.txt -> [dict(kind, c, r, f, h)]. ValueError (nothing to apply) without the head / done line, on a
    wrong count, a bad number or a flat box (r and f parallel)."""
    lines = text.splitlines()
    if not lines or not BXHEAD.search(lines[0]):
        raise ValueError('boxes file has no "MUD boxes 1" line')
    out = []
    done = None
    for line in lines[1:]:
        if not line.strip():
            continue
        m = BXLINE.search(line)
        if m:
            try:
                v = [float(t) for t in m[2].split()]
            except ValueError:
                raise ValueError('bad number in box line: %s' % line.strip())
            if any(x != x or abs(x) == float('inf') for x in v):
                raise ValueError('bad number in box line: %s' % line.strip())
            b = {'kind': m[1], 'c': (v[0], v[1]), 'r': (v[2], v[3]), 'f': (v[4], v[5]), 'h': (abs(v[6]), abs(v[7]))}
            if abs(v[2] * v[5] - v[3] * v[4]) < 1e-9:
                raise ValueError('box %d is flat from above (rows 0 and 2 parallel): %s' % (len(out) + 1, line.strip()))
            out.append(b)
            continue
        d = BXDONE.search(line)
        if d:
            done = int(d[1])
            break
        raise ValueError('unknown line in boxes file: %s' % line.strip())
    if done is None:
        raise ValueError('boxes file has no "MUD boxes done" line')
    if done != len(out):
        raise ValueError('checksum: done line says %d boxes, file holds %d' % (done, len(out)))
    return out


def _box_inside(b, px, pz):
    (cx, cz), (rx, rz), (fx, fz), (hx, hz) = b['c'], b['r'], b['f'], b['h']
    det = rx * fz - rz * fx
    dx, dz = px - cx, pz - cz
    lx = (dx * fz - dz * fx) / det
    lz = (rx * dz - rz * dx) / det
    return abs(lx) <= hx + BOX_EPS and abs(lz) <= hz + BOX_EPS


def box_cells(b, x0, z0, cell, nx, nz):
    """[(cz, a, b)] inclusive cell runs whose centre is inside box b. Per row the inside part is one x interval
    (a box is convex): solved, then its ends checked with the exact test."""
    import math
    (cx, cz), (rx, rz), (fx, fz), (hx, hz) = b['c'], b['r'], b['f'], b['h']
    # corners -> z rows the box can touch
    zs = [cz + sx * hx * rz + sz * hz * fz for sx in (-1, 1) for sz in (-1, 1)]
    xs = [cx + sx * hx * rx + sz * hz * fx for sx in (-1, 1) for sz in (-1, 1)]
    r0 = max(0, int(math.floor((min(zs) - z0) / cell - 0.5)) - 1)
    r1 = min(nz - 1, int(math.ceil((max(zs) - z0) / cell - 0.5)) + 1)
    c0 = max(0, int(math.floor((min(xs) - x0) / cell - 0.5)) - 1)
    c1 = min(nx - 1, int(math.ceil((max(xs) - x0) / cell - 0.5)) + 1)
    det = rx * fz - rz * fx
    runs = []
    for row in range(r0, r1 + 1):
        pz = z0 + (row + 0.5) * cell
        dz = pz - cz
        lo, hi = -math.inf, math.inf
        # lx = (dx * fz - dz * fx) / det, lz = (rx * dz - rz * dx) / det, dx = px - cx: both linear in px
        for k, off, lim in ((fz / det, -dz * fx / det, hx), (-rz / det, rx * dz / det, hz)):
            if abs(k) < 1e-12:
                if abs(off) > lim + BOX_EPS:
                    lo, hi = 1, 0
                continue
            a_ = (-lim - off) / k
            b_ = (lim - off) / k
            lo, hi = max(lo, min(a_, b_) + cx), min(hi, max(a_, b_) + cx)
        if lo > hi:
            continue
        a = max(c0, int(math.ceil((lo - x0) / cell - 0.5)) - 1)
        e = min(c1, int(math.floor((hi - x0) / cell - 0.5)) + 1)

        def ins(c):
            return _box_inside(b, x0 + (c + 0.5) * cell, pz)
        while a <= e and not ins(a):
            a += 1
        while e >= a and not ins(e):
            e -= 1
        if a > e:
            continue
        while a - 1 >= 0 and ins(a - 1):
            a -= 1
        while e + 1 < nx and ins(e + 1):
            e += 1
        runs.append((row, a, e))
    return runs


def box_grids(boxes, x0, z0, cell, nx, nz):
    """Parsed boxes -> {'cut': bytearray, 'keep': bytearray (0/1 per cell), 'n_cut', 'n_keep' (box counts)}."""
    g = {'cut': bytearray(nx * nz), 'keep': bytearray(nx * nz), 'n_cut': 0, 'n_keep': 0}
    for b in boxes:
        grid = g[b['kind']]
        g['n_' + b['kind']] += 1
        for row, a, e in box_cells(b, x0, z0, cell, nx, nz):
            grid[row * nx + a:row * nx + e + 1] = b'\1' * (e - a + 1)
    return g


def _pack(arrays, n):
    """0/1 byte arrays -> one byte array with array i in bit i (no carry: every byte stays < 256)."""
    v = 0
    for i, a in enumerate(arrays):
        v |= int.from_bytes(bytes(a), 'big') << i
    return v.to_bytes(n, 'big')


def _table(fn, bits):
    return bytes(fn(i) & 0xFF if i < (1 << bits) else 0 for i in range(256))


def compose(mask, margin, pins=None, boxes=None, control=(), keep_full=False, codes=False):
    """The final keep grid: dilate(mask, margin) -> clear cut-box cells -> set keep-box cells -> pins -> one corner
    cell per all-hole block. Keep box beats cut box, pins always win. Returns {'keep_grid', 'pins_new', 'full_kept',
    'box_cut' (cells kept after the margin that a cut box holed), 'box_keep' (holes a keep box kept), 'codes'
    (bytearray of P_* per cell, only with codes=True)}."""
    nx, nz = mask['nx'], mask['nz']
    n = nx * nz
    d = dilate(mask['keep'], nx, nz, margin)
    if boxes is None:
        boxes = {'cut': bytearray(n), 'keep': bytearray(n), 'n_cut': 0, 'n_keep': 0}
    if (len(boxes['cut']), len(boxes['keep'])) != (n, n):
        raise ValueError('box grids are not %d x %d' % (nx, nz))
    dsum = sum(d)
    c = bytearray(_pack((d, boxes['cut']), n).translate(_table(lambda i: (i & 1) and not (i & 2), 2)))
    csum = sum(c)
    keep = bytearray(_pack((c, boxes['keep']), n).translate(_table(lambda i: 1 if i else 0, 2)))
    ksum = sum(keep)
    pins_new = apply_pins(keep, nx, nz, pins)
    full_kept = keep_one_per_full_block(keep, nx, nz, control) if keep_full else 0
    out = {'keep_grid': keep, 'pins_new': pins_new, 'full_kept': full_kept, 'box_cut': dsum - csum,
           'box_keep': ksum - csum}
    if codes:
        pin = bytearray(n)
        for i in (pins['cells'] if pins else ()):
            pin[i] = 1

        def code(i):
            m, dd, cut, kb, p, fin = (i >> 0) & 1, (i >> 1) & 1, (i >> 2) & 1, (i >> 3) & 1, (i >> 4) & 1, (i >> 5) & 1
            cc = dd and not cut
            if not fin:
                return P_CUTBOX if dd else 0
            if cc:
                return P_SCAN if m else P_MARGIN
            if kb:
                return P_KEEPBOX
            if p:
                return P_PIN
            return P_CORNER
        out['codes'] = bytearray(_pack((mask['keep'], d, boxes['cut'], boxes['keep'], pin, keep), n)
                                 .translate(_table(code, 6)))
    return out


PREVIEW_RUN = re.compile(rb'\x01+|\x02+|\x03+|\x04+|\x05+|\x06+')


def preview_text(codes, nx, nz, x0, z0, cell):
    """Code grid -> preview.txt ('MUD preview row <cz> <a> <b> <code>' runs, holes not listed)."""
    out = ['MUD preview 1', 'MUD preview box %g %g cells %d %d cell %g' % (x0, z0, nx, nz, cell)]
    cells = runs = keep = 0
    for z in range(nz):
        row = bytes(codes[z * nx:(z + 1) * nx])
        for m in PREVIEW_RUN.finditer(row):
            a, e = m.start(), m.end() - 1
            k = row[a]
            out.append('MUD preview row %d %d %d %d' % (z, a, e, k))
            runs += 1
            cells += e - a + 1
            if k in P_KEPT:
                keep += e - a + 1
    out.append('MUD preview done %d %d keep %d holes %d' % (cells, runs, keep, nx * nz - keep))
    return '\n'.join(out) + '\n', {'cells': cells, 'runs': runs, 'keep': keep, 'holes': nx * nz - keep}


# hole black, scan white, margin light grey, pin yellow, keep box green, cut box red, grass corner cell blue
PREVIEW_COLOURS = [(0, 0, 0), (255, 255, 255), (170, 170, 170), (255, 220, 0), (0, 200, 0), (230, 0, 0), (40, 120, 255)]


def preview_png(codes, nx, nz):
    """Code grid -> PNG bytes (palette, 1 pixel per cell, north up = row nz-1 at the top). zlib + struct only."""
    import zlib

    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = bytearray()
    for z in range(nz - 1, -1, -1):
        raw += b'\0' + bytes(codes[z * nx:(z + 1) * nx])
    plte = b''.join(struct.pack('BBB', *c) for c in PREVIEW_COLOURS)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', nx, nz, 8, 3, 0, 0, 0))
            + chunk(b'PLTE', plte) + chunk(b'IDAT', zlib.compress(bytes(raw), 6)) + chunk(b'IEND', b''))


def tile_holes(data):
    """{(bx, bz): 32 row bitmasks} of the blocks with holes in .ttile bytes."""
    parts = dict(tt.chunks(data))
    return {k: v for k, v in tt.decode(parts[b'HOLE']).items() if any(v)} if b'HOLE' in parts else {}


def spare_map_holes(s, nx, map_holes):
    """A map can have holes of its own (Belleau Wood 56 cells, measured 2026-11-01). Where those + our cut would leave a
    block with no terrain cell, keep the block's first cell the map did not hole (updates s). A block the map itself
    holed whole has no such cell. Returns (cells spared, set of those whole blocks: merge_tile's allow_full)."""
    spared, whole = 0, set()
    for k, base in sorted(map_holes.items()):
        if all(r == FULL for r in base):
            whole.add(k)
            continue
        rows = s['block_map'].get(k)
        if rows is None or any((a | b) != FULL for a, b in zip(base, rows)):
            continue
        r, c = next((r, c) for r in range(BLOCK) for c in range(BLOCK) if not base[r] >> c & 1)
        rows[r] &= ~(1 << c)
        s['keep_grid'][(k[1] * BLOCK + r) * nx + k[0] * BLOCK + c] = 1
        s['keep'] += 1
        s['holes'] -= 1
        spared += 1
    return spared, whole


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
    ap.add_argument('cmd', choices=['plan', 'write', 'verify', 'fit', 'bake', 'area'])
    ap.add_argument('files', nargs='*')
    ap.add_argument('--margin', type=int, default=1)
    ap.add_argument('--out')
    ap.add_argument('--backup')
    ap.add_argument('--bk')
    ap.add_argument('--install', default='experimental')
    ap.add_argument('--pins')
    ap.add_argument('--data')
    ap.add_argument('--fit')
    ap.add_argument('--old-pins')
    ap.add_argument('--rect', help='area: x0,z0,x1,z1 world metres')
    ap.add_argument('--out-mask')
    ap.add_argument('--out-pins')
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

    if a.cmd == 'area':
        if len(a.files) != 2 or not (a.pins and a.old_pins and a.rect and a.out_mask and a.out_pins):
            ap.error('area needs <new mask> <old mask> --pins --old-pins --rect --out-mask --out-pins')
        new_text, old_text = read(a.files[0]), read(a.files[1])
        new_m, old_m = parse_mask(new_text), parse_mask(old_text)
        x0, z0 = box_origin(old_text, BOX)
        if box_origin(new_text, BOX) != (x0, z0):
            ap.error('the two masks have different box corners')
        new_p, old_p = parse_pins(read(a.pins)), parse_pins(read(a.old_pins))
        cells = rect_cells([float(v) for v in a.rect.split(',')], x0, z0, old_m['cell'], old_m['nx'], old_m['nz'])
        m, p = merge_area(new_m, old_m, new_p, old_p, cells)
        n_in = (cells[2] - cells[0]) * (cells[3] - cells[1])
        k_old = sum(sum(old_m['keep'][z * old_m['nx'] + cells[0]:z * old_m['nx'] + cells[2]]) for z in range(cells[1], cells[3]))
        k_new = sum(sum(m['keep'][z * m['nx'] + cells[0]:z * m['nx'] + cells[2]]) for z in range(cells[1], cells[3]))
        print('area cells x %d-%d z %d-%d (%d cells): mask keep old %d -> new %d; pins %d -> %d (whole map)'
              % (cells[0], cells[2] - 1, cells[1], cells[3] - 1, n_in, k_old, k_new, len(old_p['cells']), len(p['cells'])))
        with open(a.out_mask, 'w', newline='\n') as f:
            f.write(mask_text_of(m, x0, z0))
        with open(a.out_pins, 'w', newline='\n') as f:
            f.write(pins_text_of(p, x0, z0))
        return 0

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
