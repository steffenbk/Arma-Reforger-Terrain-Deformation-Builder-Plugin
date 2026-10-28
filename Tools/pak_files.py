"""Read-only access to the game's .pak files (all of them: data*.pak AND worlds*.pak, where Everon / Kolguyev /
GameMaster worlds live - measured 2026-10-28 Stable). Used by Tools/mud_build.py --vanilla to read a vanilla
terrain's tiles. Format as blender-mcp-BK tools/pakread.py (FILE chunk = name tree; entry offset, packed len,
original len; zlib when they differ), copied here so the mod's tools need nothing outside the mod.

    python -I Tools/pak_files.py <game data dir> find <regex>
"""
import os
import re
import struct
import sys
import zlib


def parse_pak(path):
    """{resource path: (offset, packed len, original len)} of one pak."""
    out = {}
    size = os.path.getsize(path)
    with open(path, 'rb') as fh:
        pos, buf = 12, None
        while pos + 8 <= size:
            fh.seek(pos)
            tag = fh.read(4)
            n = struct.unpack('>I', fh.read(4))[0]
            if tag == b'FILE':
                buf = fh.read(n)
                break
            pos += 8 + n
    if buf is None:
        return out
    i = 0

    def walk(prefix):
        nonlocal i
        typ, ln = buf[i], buf[i + 1]
        name = buf[i + 2:i + 2 + ln].decode('utf-8')
        i += 2 + ln
        p = prefix + '/' + name if prefix else name
        if typ == 0:
            cnt = struct.unpack_from('<I', buf, i)[0]
            i += 4
            for _ in range(cnt):
                walk(p)
        else:
            off, cl, ol = struct.unpack_from('<3I', buf, i)
            i += 24
            out[p] = (off, cl, ol)
    walk('')
    return out


class PakFiles:
    """Every resource in the .pak files of one folder (the game's addons/data); the first pak (by name) wins."""

    def __init__(self, data_dir):
        self.dir = data_dir
        self.index = {}
        names = sorted(f for f in os.listdir(data_dir) if f.lower().endswith('.pak')) if os.path.isdir(data_dir) else []
        if not names:
            raise ValueError('no .pak files in %s (want the game\'s addons/data folder)' % data_dir)
        for f in names:
            for p, e in parse_pak(os.path.join(data_dir, f)).items():
                self.index.setdefault(p, (f,) + e)

    def find(self, pattern):
        r = re.compile(pattern, re.I)
        return sorted(p for p in self.index if r.search(p))

    def read(self, path):
        pak, off, cl, ol = self.index[path]
        with open(os.path.join(self.dir, pak), 'rb') as fh:
            fh.seek(off)
            b = fh.read(cl)
        return zlib.decompress(b) if cl != ol else b


if __name__ == '__main__':
    for p in PakFiles(sys.argv[1]).find(sys.argv[3]):
        print(p)
