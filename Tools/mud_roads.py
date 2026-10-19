"""Road copy bake (whole-map cut plan 7A.2): road export -> Data/Mud/mud_roads.c.

Reads the World Editor export of MUD_RoadExportPlugin.c ('MUD Export Roads', $profile:mud_road_export.txt) and
writes the static road table every machine loads (same file everywhere = same roads, rule 3). Text, but named
.c: game FileIO opens only .c / .conf / .et-type files (probe 2026-10-10). The game reads it with ReadLine.

    python -I Tools/mud_roads.py bake  <mud_road_export.txt> --out <addon>/Data/Mud/mud_roads.c
    python -I Tools/mud_roads.py check <mud_road_export.txt> --hits <hits.txt>
                                       (hits: 'x,y,z|ox,oy,oz' per line = a real crosshair hit on the road whose
                                       origin is ox oy oz; every hit must be within width / 2 of that road's line)

Export lines read (last 'done' must match the road count, else refused):
    MUD rexp road <i> origin <x y z> width <w> mat <material> type <Type> spline <n> parent <class> tess <m>
    MUD rexp sp <i> <x> <y> <z> [tg ...]       (the road's own spline points)
    MUD rexp tp <i> <x> <y> <z>                (the spline's tessellated points = the curve; 2026-10-18 export: none)
    MUD rexp done roads <n> ...

Bake lines written:
    MUD roads 1 <count>
    r <i> <armor 0|1> <type> <width> <n> <ox> <oy> <oz> <material or ->
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

ROAD_DIR = '/Structures/Infrastructure/Roads/'
RUNWAY_CONCRETE = '/RunwayConcrete_'


def is_armor(mat):
    return mat == '' or ROAD_DIR in mat or RUNWAY_CONCRETE in mat


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
            i = int(t[3])
            if i in roads:
                raise ValueError('line %d: road %d twice' % (n, i))
            roads[i] = {'i': i, 'origin': tuple(float(v) for v in t[5:8]), 'width': float(t[9]), 'mat': mat,
                        'type': int(rest[0]), 'spline': int(rest[2]), 'pts': [], 'tess': [], 'tg': []}
            order.append(i)
        elif t[2] == 'sp':
            i = int(t[3])
            if i not in roads:
                raise ValueError('line %d: point for unknown road %d' % (n, i))
            roads[i]['pts'].append(tuple(float(v) for v in t[4:7]))
            tg = None
            if len(t) >= 15 and t[7] == 'tg':
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
        if len(r['pts']) < 2:
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
    """Per point (in, out) tangents: explicit when exported, else Catmull-Rom (p[k+1] - p[k-1]) / 2, ends one-sided.
    INFERENCE (export 2026-10-19): explicit |tangent| ~ the segment length and in == out direction (191 / 191), so
    Hermite on t 0-1 with these as derivatives; the engine's auto-tangent rule is unknown - Catmull-Rom is a guess."""
    n = len(pts)
    out = []
    for k in range(n):
        if tg[k] is not None:
            out.append(tg[k])
            continue
        a = pts[max(k - 1, 0)]
        b = pts[min(k + 1, n - 1)]
        f = 0.5 if 0 < k < n - 1 else 1.0
        t = tuple((b[j] - a[j]) * f for j in range(3))
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


def bake_text(roads):
    lines = ['MUD roads 1 %d' % len(roads)]
    armor = 0
    for r in roads:
        a = 1 if is_armor(r['mat']) else 0
        armor += a
        o = r['origin']
        lines.append('r %d %d %d %g %d %g %g %g %s' % (r['i'], a, r['type'], r['width'], len(r['pts']),
                                                      o[0], o[1], o[2], r['mat'] or '-'))
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


def main(argv):
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='cmd', required=True)
    b = sub.add_parser('bake')
    b.add_argument('export')
    b.add_argument('--out', required=True)
    c = sub.add_parser('check')
    c.add_argument('export')
    c.add_argument('--hits', required=True)
    a = ap.parse_args(argv)
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
