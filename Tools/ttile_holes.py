"""THROWAWAY TEST (2026-10-08): add holes to an Enfusion .ttile (format measured 21:08, checked in play 21:10).

HOLE chunk: u32 LE block count, u16 LE bytes per block (128), then per block u16 bx, u16 bz,
32 rows x u32 LE; bit c of row r = cell (bx*32+c, bz*32+r) is a hole. Cell = 2 m, cell x = world X / 2.
Chunk sizes in the FORM are big-endian.

    python -I ttile_holes.py <ttile> add <cx0> <cz0> <cx1> <cz1>   (cells, inclusive)
    python -I ttile_holes.py <ttile> show
"""
import struct
import sys

ROWBYTES = 128


def chunks(b):
    i = 12
    out = []
    while i + 8 <= len(b):
        tag = b[i:i + 4]
        n = struct.unpack('>I', b[i + 4:i + 8])[0]
        out.append((tag, b[i + 8:i + 8 + n]))
        i += 8 + n  # no IFF pad byte: Enfusion writes odd chunks unpadded (Arland Terrain_39 GCTD 43)
    return out


def decode(d):
    cnt, sz = struct.unpack('<IH', d[:6])
    assert sz == ROWBYTES, sz
    blocks = {}
    p = 6
    for _ in range(cnt):
        bx, bz = struct.unpack('<HH', d[p:p + 4])
        blocks[(bx, bz)] = list(struct.unpack('<32I', d[p + 4:p + 4 + sz]))
        p += 4 + sz
    assert p == len(d), (p, len(d))
    return blocks


def encode(blocks):
    keys = [k for k in blocks if any(blocks[k])]
    out = struct.pack('<IH', len(keys), ROWBYTES)
    for k in keys:  # keep file order for existing blocks (dict keeps insertion order)
        out += struct.pack('<HH', *k) + struct.pack('<32I', *blocks[k])
    return out


def build(parts):
    body = b''
    for tag, data in parts:
        body += tag + struct.pack('>I', len(data)) + data  # unpadded, see chunks()
    return b'FORM' + struct.pack('>I', len(body) + 4) + b'TERR' + body


def main(argv):
    path = argv[0]
    b = open(path, 'rb').read()
    assert b[:4] == b'FORM' and b[8:12] == b'TERR'
    parts = chunks(b)
    assert build(parts) == b, 'round trip failed - unknown layout, not writing'
    tags = [t for t, _ in parts]
    blocks = decode(dict(parts)[b'HOLE']) if b'HOLE' in tags else {}

    if argv[1] == 'show':
        for k, rows in blocks.items():
            print(k, sum(bin(r).count('1') for r in rows), 'cells')
        return

    cx0, cz0, cx1, cz1 = map(int, argv[2:6])
    for cz in range(cz0, cz1 + 1):
        for cx in range(cx0, cx1 + 1):
            k = (cx // 32, cz // 32)
            blocks.setdefault(k, [0] * 32)[cz % 32] |= 1 << (cx % 32)
    data = encode(blocks)
    if b'HOLE' in tags:
        parts = [(t, data if t == b'HOLE' else d) for t, d in parts]
    else:  # chunk order seen in the saved file: VERS HGHT BERR HOLE ... -> insert after BERR
        at = tags.index(b'BERR') + 1
        parts.insert(at, (b'HOLE', data))
    open(path, 'wb').write(build(parts))
    print('wrote', path, 'blocks', len(blocks))


if __name__ == '__main__':
    main(sys.argv[1:])
