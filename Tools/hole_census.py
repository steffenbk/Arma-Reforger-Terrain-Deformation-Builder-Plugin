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
