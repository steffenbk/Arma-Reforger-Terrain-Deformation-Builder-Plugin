"""Displaced ruts, follow-up 2026-10-15: test the parts displace_proto.py took for granted (offline).

displace_proto.py split EVERY triangle of the 12 m test area and blended rut edges toward the TRUE surface
(P.top_of). The game would split only near ruts and only knows the coarse mesh. This tool measures:

    disp2_all     = displace_proto 'disp2' (all split, blend to the true top) - must reproduce its numbers
    disp2_allv    all split, blend to the vertex's own coarse y (what the game knows)
    disp2_tri     split only triangles where any of the 6 split points would be pulled ("rutted" triangles)
    disp2_brick   split every triangle of a 4 m brick (x, z) that has any rutted triangle

Per method: accuracy (as displace_proto), triangles, cracks (pulled split point on an edge whose other triangle is
not split), T-junctions (a vertex lying inside another triangle's edge), and the rut-wall angle.
Also: how many floor samples the u-in-[0,1) segment test skips that a capsule (u clamped) would cut.

    python Tools/displace_partial.py [--md FILE]
"""
import argparse
import math
import time
from collections import defaultdict

import numpy as np

import dc_vertex_proto as P
import displace_proto as D

BRICK = 4.0
EPS = 1e-6


class FloorV(D.Floor):
    """As D.Floor, but a corner without a rut blends to the vertex's own y (the game has no true top)."""

    def at_v(self, x, z, y):
        fx, fz = x / self.det, z / self.det
        a, b = int(math.floor(fx)), int(math.floor(fz))
        if not (0 <= a < self.n - 1 and 0 <= b < self.n - 1):
            return math.inf
        u, v = fx - a, fz - b
        c = [self.F[a, b], self.F[a + 1, b], self.F[a, b + 1], self.F[a + 1, b + 1]]
        if not any(math.isfinite(q) for q in c):
            return math.inf
        c = [q if math.isfinite(q) else y for q in c]
        w = [(1 - u) * (1 - v), u * (1 - v), (1 - u) * v, u * v]
        return sum(q * ww for q, ww in zip(c, w))


def pulled_y(p, floor, own):
    f = floor.at_v(p[0], p[2], p[1]) if own else floor.at(p[0], p[2])
    return min(p[1], f)


def split_pts(t):
    A, B, C = (np.array(p, dtype=float) for p in t)
    return [A, B, C, (A + B) / 2, (B + C) / 2, (A + C) / 2]


def is_rutted(t, floor, own):
    return any(pulled_y(p, floor, own) < p[1] - EPS for p in split_pts(t))


def key(p):
    return (round(float(p[0]), 6), round(float(p[1]), 6), round(float(p[2]), 6))


def ekey(a, b):
    ka, kb = key(a), key(b)
    return (ka, kb) if ka <= kb else (kb, ka)


def build(coarse, floor, own, mode):
    """mode: all | tri | brick. Returns displaced triangles + split flags per coarse triangle."""
    rut = [is_rutted(t, floor, own) for t in coarse]
    if mode == "all":
        do = [True] * len(coarse)
    elif mode == "tri":
        do = rut
    else:
        bricks = set()
        for t, r in zip(coarse, rut):
            if r:
                for p in t:
                    bricks.add((math.floor(p[0] / BRICK), math.floor(p[2] / BRICK)))
        do = []
        for t in coarse:
            mid = [sum(p[q] for p in t) / 3 for q in range(3)]
            do.append((math.floor(mid[0] / BRICK), math.floor(mid[2] / BRICK)) in bricks)
    out = []
    for t, s in zip(coarse, do):
        parts = D.split(t, 2) if s else [tuple(np.array(p, dtype=float) for p in t)]
        for st in parts:
            out.append(tuple((float(p[0]), float(pulled_y(p, floor, own)), float(p[2])) for p in st))
    return out, do, rut


def seams(coarse, do, floor, own):
    """Cracks and T-junctions on coarse edges whose two triangles differ in 'split'."""
    owners = defaultdict(list)
    for n, t in enumerate(coarse):
        for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            owners[ekey(a, b)].append(n)
    tj = 0
    crack = 0
    gap = 0.0
    for e, ts in owners.items():
        if len(ts) != 2 or do[ts[0]] == do[ts[1]]:
            continue
        tj += 1
        a, b = np.array(e[0]), np.array(e[1])
        m = (a + b) / 2
        dy = m[1] - pulled_y(m, floor, own)
        # the unsplit side draws the straight edge a'-b' (its corners pulled); the split side has m pulled
        ya = pulled_y(a, floor, own)
        yb = pulled_y(b, floor, own)
        line_y = (ya + yb) / 2
        g = abs((m[1] - dy) - line_y)
        if g > 1e-4:
            crack += 1
            gap = max(gap, g)
    return tj, crack, gap


def skipped_samples(rut, det):
    """Floor samples within RUT_R of the polyline (capsule) that the u in [0, 1) test leaves uncut."""
    n = int(round(12 / det)) + 1
    miss = 0
    hit = 0
    for a in range(n):
        for b in range(n):
            x, z = a * det, b * det
            cap = False
            for (ax, az), (bx, bz) in rut.segs():
                abx, abz = bx - ax, bz - az
                l2 = abx * abx + abz * abz
                if l2 < 1e-6:
                    continue
                u = min(max(((x - ax) * abx + (z - az) * abz) / l2, 0), 1)
                if math.hypot(ax + abx * u - x, az + abz * u - z) < P.RUT_R:
                    cap = True
                    break
            if not cap:
                continue
            if rut.fall_at(x, z) < 0:
                miss += 1
            else:
                hit += 1
    return hit, miss


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--md")
    args = ap.parse_args()
    rows = []
    gaps = []
    for name, ops in D.shapes().items():
        tcloud, H, xs = P.true_cloud(ops)
        tnear = P.Near(tcloud)
        G2 = P.seed()
        for op in ops:
            if not isinstance(op, P.Rut):
                op.apply(G2)
        coarse, _ = P.polygonize(G2, P.Vertexer(G2, "mean", ops))
        ruts = [op for op in ops if isinstance(op, P.Rut)]
        fl = FloorV(ruts, 0.25)
        for method, own, mode in (("disp2_all", False, "all"), ("disp2_allv", True, "all"),
                                  ("disp2_tri", True, "tri"), ("disp2_brick", True, "brick")):
            t0 = time.perf_counter()
            tris, do, rut = build(coarse, fl, own, mode)
            ms = (time.perf_counter() - t0) * 1000
            tj, crack, gap = seams(coarse, do, fl, own)
            r = D.measure(name, method, tris, ops, tnear, tcloud, ms,
                          "split %d / %d coarse tris (rutted %d), T-junction edges %d, cracks %d (max %.4f m)"
                          % (sum(do), len(do), sum(rut), tj, crack, gap))
            r.update(split=sum(do), coarse=len(do), rutted=sum(rut), tj=tj, crack=crack, gap=gap)
            rows.append(r)
        for rt in ruts:
            hit, miss = skipped_samples(rt, 0.25)
            gaps.append((name, hit, miss))
            print("%-13s u-gap: %d of %d capsule floor samples uncut" % (name, miss, hit + miss))
    if args.md:
        L = ["| Shape | Method | Tris | Split / coarse | Slivers | mesh->true rms | max | true->mesh rms | max | Wall p90 deg | T-junction edges | Cracks |",
             "|---|---|---|---|---|---|---|---|---|---|---|---|"]
        for r in rows:
            L.append("| %s | %s | %d | %d / %d | %d | %.3f | %.3f | %.3f | %.3f | %s | %d | %d |" % (
                r["shape"], r["method"], r["tris"], r["split"], r["coarse"], r["sliver"], r["m2t_rms"], r["m2t_max"],
                r["t2m_rms"], r["t2m_max"], "-" if r["wall"] is None else "%.0f" % r["wall"], r["tj"], r["crack"]))
        L += ["", "| Shape (one rut each) | Capsule floor samples | Uncut by u in [0, 1) |", "|---|---|---|"]
        for name, hit, miss in gaps:
            L.append("| %s | %d | %d |" % (name, hit + miss, miss))
        open(args.md, "w", encoding="utf-8").write("\n".join(L) + "\n")


if __name__ == "__main__":
    main()
