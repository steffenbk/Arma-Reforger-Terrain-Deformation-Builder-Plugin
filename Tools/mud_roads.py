"""Road copy bake (whole-map cut plan 7A.2): road export -> Data/Mud/mud_roads.c.

Reads the World Editor export of MUD_RoadExportPlugin.c ('MUD Export Roads', $profile:mud_road_export.txt) and
writes the static road table every machine loads (same file everywhere = same roads, rule 3). Text, but named
.c: game FileIO opens only .c / .conf / .et-type files (probe 2026-10-10). The game reads it with ReadLine.

    python -I Tools/mud_roads.py bake  <mud_road_export.txt> --out <addon>/Data/Mud/mud_roads.c
    python -I Tools/mud_roads.py check <mud_road_export.txt> --hits <hits.txt>
                                       (hits: 'x,y,z|ox,oy,oz' per line = a real crosshair hit on the road whose
                                       origin is ox oy oz; every hit must be within width / 2 of that road's line)
    python -I Tools/mud_roads.py compare <baked a .c> <baked b .c>
                                       (road armor plan Task 2: build's <key>_mud_roads.c vs Data/Mud/mud_roads.c)

Export lines read (last 'done' must match the road count, else refused):
    MUD rexp road <i> origin <x y z> width <w> mat <material> type <Type | -> spline <n> parent <class> tess <m>
                  [sort <MatSortBias | -> rsort <RoadSort | ->]   (2026-11-02; older exports have none)
    MUD rexp sp <i> <x> <y> <z> [tg ...]       (the road's own spline points)
    MUD rexp tp <i> <x> <y> <z>                (the spline's tessellated points = the curve; 2026-10-18 export: none)
    MUD rexp done roads <n> ...

Bake lines written:
    MUD roads 1 <count>
    r <i> <armor 0|1> <type> <width> <n> <ox> <oy> <oz> <material or ->
    o <MatSortBias or -> <RoadSort or ->        (draw order, only when the export has it; right after its 'r' line
                                                 so the game's 'r' readers (10 fields) stay as they are)
    p <x> <y> <z>                               (n lines, world: the tessellated curve when the export has one
                                                 (>= 2 tp points; 2026-10-19 export: none - no spline entities),
                                                 else a Hermite curve through the spline points every ~STEP m
                                                 (explicit tangents, else Catmull-Rom). Roads ARE splines (user).
    MUD roads done <count> armor <a>

ARMOR (user U1 2026-10-18: dirt roads and trails are armored too) is set by material, NOT by Type: Type 255 holds
real Road_Dirt_01 / Road_Asphalt_E_03 roads as well as markings and flower beds. Armor = a material under
Structures/Infrastructure/Roads/ or a RunwayConcrete material, or no material at all (11 roads, type 2, 3-4.5 m).
"""
import argparse
import math
import sys

TOOLS_VERSION = 12   # = mud_build.TOOLS_VERSION (see there)
ROAD_DIR = '/Structures/Infrastructure/Roads/'
RUNWAY_CONCRETE = '/RunwayConcrete_'
NO_TYPE = -1    # the road's Type read empty; armor goes by material and the game skips this field, so only the slot counts


def is_armor(mat):
    return mat == '' or ROAD_DIR in mat or RUNWAY_CONCRETE in mat


def sort_field(t, key):
    """The token after `key` on a road line, '-' when absent (exports before 2026-11-02)."""
    for k in range(len(t) - 1, 12, -1):
        if t[k - 1] == key:
            return t[k]
    return '-'


def parse_export(text):
    """-> list of roads {i, origin, width, mat, type, pts}. Raises ValueError on a broken file."""
    roads = {}
    order = []
    done = None
    for n, line in enumerate(text.splitlines(), 1):
        t = line.split()
        if len(t) < 3 or t[0] != 'MUD' or t[1] != 'rexp':
            continue
        if t[2] == 'road':
            # 'mat' may be empty -> 'mat type ...' (one token less)
            if t[4] != 'origin' or t[8] != 'width' or t[10] != 'mat':
                raise ValueError('line %d: bad road line' % n)
            if t[11] == 'type':
                mat, rest = '', t[12:]
            else:
                mat, rest = t[11], t[13:]
                if t[12] != 'type':
                    raise ValueError('line %d: bad road line' % n)
            # 'type' may be empty too -> 'type spline ...' (1 of 2157 roads on Cain, 2026-11-01); new exports write '-'
            if rest[:1] == ['spline']:
                rest = ['-'] + rest
            if len(rest) < 3 or rest[1] != 'spline':
                raise ValueError('line %d: bad road line' % n)
            i = int(t[3])
            if i in roads:
                raise ValueError('line %d: road %d twice' % (n, i))
            roads[i] = {'i': i, 'origin': tuple(float(v) for v in t[5:8]), 'width': float(t[9]), 'mat': mat,
                        'type': NO_TYPE if rest[0] == '-' else int(rest[0]), 'spline': int(rest[2]),
                        'pts': [], 'tess': [], 'tg': [], 'sort': sort_field(t, 'sort'),
                        'rsort': sort_field(t, 'rsort')}
            order.append(i)
        elif t[2] == 'sp':
            i = int(t[3])
            if i not in roads:
                raise ValueError('line %d: point for unknown road %d' % (n, i))
            roads[i]['pts'].append(tuple(float(v) for v in t[4:7]))
            tg = None
            # 'MUD rexp sp i x y z tg ix iy iz ox oy oz' = 14 tokens (was '>= 15' until 2026-10-31: every saved
            # tangent was dropped -> up to 3.6 m off the AI road net on road 99, results/2026-10-31-road-look.md)
            if len(t) >= 14 and t[7] == 'tg' and t[8] != '-':
                tg = (tuple(float(v) for v in t[8:11]), tuple(float(v) for v in t[11:14]))
            roads[i]['tg'].append(tg)
        elif t[2] == 'tp':
            i = int(t[3])
            if i not in roads:
                raise ValueError('line %d: tess point for unknown road %d' % (n, i))
            roads[i]['tess'].append(tuple(float(v) for v in t[4:7]))
        elif t[2] == 'done':
            done = int(t[4])
    if done is None:
        raise ValueError('no done line - export did not finish')
    if done != len(order):
        raise ValueError('done line says %d roads, file has %d' % (done, len(order)))
    out = []
    for i in order:
        r = roads[i]
        if len(r['pts']) != r['spline']:
            raise ValueError('road %d: %d points, road line says %d' % (i, len(r['pts']), r['spline']))
        # a road on a PolylineShapeEntity exports no spline points ('spline 0'), only its tess points (Zarichne road 0,
        # 2026-11-08) - the bake uses tess anyway, so only a road with neither is broken
        if len(r['pts']) < 2 and len(r['tess']) < 2:
            raise ValueError('road %d: fewer than 2 points' % i)
        if not r['width'] > 0:
            raise ValueError('road %d: width %r' % (i, r['width']))
        for p in r['pts'] + [r['origin']]:
            if not all(math.isfinite(v) for v in p):
                raise ValueError('road %d: non-finite point' % i)
        for p in r['tess']:
            if not all(math.isfinite(v) for v in p):
                raise ValueError('road %d: non-finite tess point' % i)
        r['curve'] = len(r['tess']) >= 2
        if r['curve']:
            r['pts'] = r['tess']
        else:
            r['pts'] = hermite(r['pts'], r['tg'] or [None] * len(r['pts']))
            r['curve'] = len(r['pts']) > r['spline']
        out.append(r)
    return out


STEP = 2.0     # curve sample spacing (m)


def tangents(pts, tg):
    """Per point (in, out) tangents: explicit when exported, else Catmull-Rom (p[k+1] - p[k-1]) / 2, ends ZERO.
    INFERENCE (export 2026-10-19): explicit |tangent| ~ the segment length and in == out direction (191 / 191), so
    Hermite on t 0-1 with these as derivatives. Auto rule from the engine, 2026-10-31 (mud-gotchas.md): Catmull-Rom inside,
    zero tangent at the first / last point."""
    n = len(pts)
    out = []
    for k in range(n):
        if tg[k] is not None:
            out.append(tg[k])
            continue
        if k == 0 or k == n - 1:
            out.append(((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)))
            continue
        a = pts[k - 1]
        b = pts[k + 1]
        t = tuple((b[j] - a[j]) * 0.5 for j in range(3))
        out.append((t, t))
    return out


def hermite(pts, tg, step=STEP):
    """Cubic Hermite curve through pts, sampled every ~step m per segment (both ends kept)."""
    tt = tangents(pts, tg)
    res = [pts[0]]
    for k in range(len(pts) - 1):
        p0, p1 = pts[k], pts[k + 1]
        m0, m1 = tt[k][1], tt[k + 1][0]
        n = max(1, int(math.ceil(math.dist(p0, p1) / step)))
        for s in range(1, n + 1):
            u = s / n
            h00 = 2 * u ** 3 - 3 * u ** 2 + 1
            h10 = u ** 3 - 2 * u ** 2 + u
            h01 = -2 * u ** 3 + 3 * u ** 2
            h11 = u ** 3 - u ** 2
            res.append(tuple(h00 * p0[j] + h10 * m0[j] + h01 * p1[j] + h11 * m1[j] for j in range(3)))
    return res


MIN_REACH = 0.75    # = MUD_RoadArmor.MIN_REACH
RUNS_PER_LINE = 32  # 'c' line length stays short (ReadLine limit unknown)


def g(v):
    """v as the game reads it back from a '%g' bake line"""
    return float('%g' % v)


def armor_cells(roads, grid):
    """LOAD BAKE (2026-11-03, research/2026-11-03-belleau-load-time.md fix 2): the set of armored skin cells
    (cz * ncx + cx) exactly as MUD_RoadArmor.MarkLine finds them from the baked lines (values as printed, so the
    game's text and this agree; the game's float maths may still flip a cell right on the reach edge)."""
    x0, z0, ncx, ncz, cell = grid
    out = set()
    for r in roads:
        if not is_armor(r['mat']):
            continue
        reach = max(g(r['width']) * 0.5, cell * MIN_REACH)
        pts = [(g(p[0]), g(p[2])) for p in r['pts']]
        for (ax, az), (bx, bz) in zip(pts, pts[1:]):
            cx0 = max(0, math.floor((min(ax, bx) - reach - x0) / cell))
            cx1 = min(ncx - 1, math.floor((max(ax, bx) + reach - x0) / cell))
            cz0 = max(0, math.floor((min(az, bz) - reach - z0) / cell))
            cz1 = min(ncz - 1, math.floor((max(az, bz) + reach - z0) / cell))
            dx, dz = bx - ax, bz - az
            len2 = dx * dx + dz * dz
            for cz in range(cz0, cz1 + 1):
                vz = z0 + (cz + 0.5) * cell - az
                for cx in range(cx0, cx1 + 1):
                    vx = x0 + (cx + 0.5) * cell - ax
                    t = max(0.0, min(1.0, (vx * dx + vz * dz) / len2)) if len2 > 0.000001 else 0.0
                    ex, ez = vx - dx * t, vz - dz * t
                    if ex * ex + ez * ez <= reach * reach:
                        out.add(cz * ncx + cx)
    return out


def cell_lines(cells, grid):
    """Armored cells -> 'MUD roads grid', 'c <cz> <x0> <x1> ...' (inclusive runs, at most RUNS_PER_LINE a line) and
    'MUD roads cells <n> runs <r>' lines (MUD_RoadArmor reads them instead of running MarkLine)."""
    x0, z0, ncx, ncz, cell = grid
    rows = {}
    for i in sorted(cells):
        rows.setdefault(i // ncx, []).append(i % ncx)
    lines = ['MUD roads grid %g %g %d %d %g' % (x0, z0, ncx, ncz, cell)]
    runs = 0
    for cz in sorted(rows):
        xs = rows[cz]
        row = []
        a = prev = xs[0]
        for x in xs[1:] + [None]:
            if x is not None and x == prev + 1:
                prev = x
                continue
            row.append((a, prev))
            if x is not None:
                a = prev = x
        runs += len(row)
        for k in range(0, len(row), RUNS_PER_LINE):
            lines.append('c %d %s' % (cz, ' '.join('%d %d' % ab for ab in row[k:k + RUNS_PER_LINE])))
    lines.append('MUD roads cells %d runs %d' % (len(cells), runs))
    return lines


def count_cells(text):
    """Armored cell count from a baked file's 'MUD roads cells' line (0 = none baked)"""
    for line in text.splitlines():
        if line.startswith('MUD roads cells '):
            return int(line.split()[3])
    return 0


def bake_text(roads, grid=None):
    """grid (x0, z0, ncx, ncz, cell) of the skin = the scan mask's: the armored cells go in right after the header"""
    lines = ['MUD roads 1 %d' % len(roads)]
    if grid is not None:
        lines += cell_lines(armor_cells(roads, grid), grid)
    armor = 0
    for r in roads:
        a = 1 if is_armor(r['mat']) else 0
        armor += a
        o = r['origin']
        lines.append('r %d %d %d %g %d %g %g %g %s' % (r['i'], a, r['type'], r['width'], len(r['pts']),
                                                      o[0], o[1], o[2], r['mat'] or '-'))
        if r.get('sort', '-') != '-' or r.get('rsort', '-') != '-':
            lines.append('o %s %s' % (r.get('sort', '-'), r.get('rsort', '-')))
        lines += ['p %g %g %g' % p for p in r['pts']]
    lines.append('MUD roads done %d armor %d curve %d' % (len(roads), armor, sum(1 for r in roads if r['curve'])))
    return '\n'.join(lines) + '\n', armor


def seg_dist(p, a, b):
    """XZ distance from p to segment a-b (tuples x, z)."""
    dx, dz = b[0] - a[0], b[1] - a[1]
    ll = dx * dx + dz * dz
    u = 0.0 if ll == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dz) / ll))
    return math.hypot(p[0] - a[0] - u * dx, p[1] - a[1] - u * dz)


def line_dist(p, pts):
    xz = [(q[0], q[2]) for q in pts]
    return min(seg_dist(p, a, b) for a, b in zip(xz, xz[1:]))


def check_hits(roads, hits_text, match=0.1):
    """-> rows (hit x, z, road i, width, distance, inside). Road found by origin within `match` m."""
    rows = []
    for line in hits_text.splitlines():
        if not line.strip():
            continue
        h, o = line.strip().split('|')
        h = [float(v) for v in h.split(',')]
        o = [float(v) for v in o.split(',')]
        hit = [r for r in roads if math.dist(r['origin'], o) <= match]
        if len(hit) != 1:
            raise ValueError('hit %s: %d roads with origin %s' % (h, len(hit), o))
        r = hit[0]
        d = line_dist((h[0], h[2]), r['pts'])
        rows.append((h[0], h[2], r['i'], r['width'], d, d <= r['width'] / 2))
    return rows


def parse_baked(text):
    """Baked file -> list of (armor, width, origin, pts)"""
    roads = []
    for line in text.splitlines():
        t = line.split()
        if t and t[0] == 'r':
            roads.append((int(t[2]), float(t[4]), tuple(float(v) for v in t[6:9]), []))
        elif t and t[0] == 'p':
            roads[-1][3].append(tuple(float(v) for v in t[1:4]))
    return roads


def compare(a_text, b_text, tol=0.1):
    """Two baked files (road armor plan Task 2 check) -> report lines, ok. Roads are matched by origin (<= tol m);
    per matched road: same armor flag + width, every point of each within tol m of the other's line (XZ)."""
    a, b = parse_baked(a_text), parse_baked(b_text)
    rep = ['roads %d vs %d, armor %d vs %d' % (len(a), len(b), sum(r[0] for r in a), sum(r[0] for r in b))]
    ok = len(a) == len(b) and sum(r[0] for r in a) == sum(r[0] for r in b)
    left = list(b)
    worst = 0.0
    for r in a:
        # several roads of one generator share an origin (even a first point, Arland 1181.8 2807.42): the nearest
        # first + last point decides
        hit = [q for q in left if math.dist(q[2], r[2]) <= tol]
        if not hit:
            rep.append('road at %s: no match in the other file' % (r[2],))
            ok = False
            continue
        q = min(hit, key=lambda h: math.dist(h[3][0], r[3][0]) + math.dist(h[3][-1], r[3][-1]) + abs(len(h[3]) - len(r[3])))
        left.remove(q)
        if q[0] != r[0] or abs(q[1] - r[1]) > 1e-3:
            rep.append('road at %s: armor %d/%d width %g/%g' % (r[2], r[0], q[0], r[1], q[1]))
            ok = False
        d = max(max(line_dist((p[0], p[2]), q[3]) for p in r[3]), max(line_dist((p[0], p[2]), r[3]) for p in q[3]))
        worst = max(worst, d)
        if d > tol:
            rep.append('road at %s: points up to %.3f m off' % (r[2], d))
            ok = False
    rep.append('worst point distance %.4f m (limit %g), unmatched in second file %d' % (worst, tol, len(left)))
    return rep, ok and not left


def main(argv):
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='cmd', required=True)
    m = sub.add_parser('compare')
    m.add_argument('baked_a')
    m.add_argument('baked_b')
    b = sub.add_parser('bake')
    b.add_argument('export')
    b.add_argument('--out', required=True)
    c = sub.add_parser('check')
    c.add_argument('export')
    c.add_argument('--hits', required=True)
    a = ap.parse_args(argv)
    if a.cmd == 'compare':
        with open(a.baked_a, encoding='utf-8') as f, open(a.baked_b, encoding='utf-8') as g:
            rep, ok = compare(f.read(), g.read())
        print('\n'.join(rep))
        print('SAME' if ok else 'DIFFERENT')
        return 0 if ok else 1
    with open(a.export, encoding='utf-8') as f:
        roads = parse_export(f.read())
    if a.cmd == 'bake':
        text, armor = bake_text(roads)
        with open(a.out, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
        pts = sum(len(r['pts']) for r in roads)
        print('baked %d roads (%d armor), %d points, %d bytes -> %s' % (len(roads), armor, pts, len(text.encode()), a.out))
    else:
        with open(a.hits, encoding='utf-8') as f:
            rows = check_hits(roads, f.read())
        print('| Hit x, z | Road # | Width m | Distance to its line m | Inside half width |')
        print('|---|---|---|---|---|')
        for x, z, i, w, d, ok in rows:
            print('| %g, %g | %d | %g | %.1f | %s |' % (x, z, i, w, d, 'yes' if ok else 'NO'))
        bad = sum(1 for row in rows if not row[5])
        print('%d of %d inside' % (len(rows) - bad, len(rows)))
        return 1 if bad else 0
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
