"""Voxel options research 2026-10-15: fine ruts as a DISPLACEMENT of the 0.5 m voxel mesh (offline).

Idea (Koca & Gudukbay 2014: coarse voxel surface + a heightmap that displaces its patches): the voxel field keeps
craters / trenches / tunnels at CELL 0.5 m, but ruts do NOT go into the voxel samples. They go into a 2D "rut floor"
grid (world y, spacing DET). The voxel mesh is built as today, each triangle is split n x n, and every vertex is
pulled DOWN to the rut floor there: y = min(y, floor(x, z)) (MudRunner's EXTRUDE = min blend). Shared triangle edges
split the same way and the floor is a function of world x, z only, so neighbour triangles / bricks cannot crack.

Methods per shape:
    vox05     today: ruts cut into the 0.5 m voxel field (CarveRut), mean vertex - the CONTROL (must match
              docs/superpowers/results/2026-10-14-dc-proto.md 'mean' rows)
    disp2     coarse mesh without ruts, split 2 x 2, floor grid 0.25 m
    disp4     split 4 x 4, floor grid 0.125 m
    disp4g25  split 4 x 4, floor grid 0.25 m (finer mesh, coarse data)

    python Tools/displace_proto.py [--png DIR] [--md FILE]
"""
import argparse
import math
import time

import numpy as np

import dc_vertex_proto as P

assert abs(P.CELL - 0.5) < 1e-9, "run with DC_CELL unset (0.5)"


def shapes():
    s = P.shapes()
    s = {k: v for k, v in s.items() if k.startswith("rut")}
    # rut driven through a fresh crater (the case the sheet failed at): rut floor above the crater floor inside
    s["rut_x_crater"] = [P.Crater(6.1, 5.9, 1.43), P.Rut(P.line(0.2, 6.1, 11.8, 6.1)), P.Rut(P.line(0.2, 4.2, 11.8, 4.2))]
    return s


class Floor:
    """Rut floor (world y) on a 2D grid of spacing det, bilinear between; +inf where no rut."""

    def __init__(self, ruts, det):
        self.det = det
        n = int(round(12 / det)) + 1
        self.n = n
        F = np.full((n, n), np.inf)
        for a in range(n):
            for b in range(n):
                x, z = a * det, b * det
                for r in ruts:
                    f = r.true_floor(x, z)
                    if f is not None:
                        F[a, b] = min(F[a, b], f)
        self.F = F
        self.cells = int(np.isfinite(F).sum())

    def at(self, x, z):
        fx, fz = x / self.det, z / self.det
        a, b = int(math.floor(fx)), int(math.floor(fz))
        if not (0 <= a < self.n - 1 and 0 <= b < self.n - 1):
            return math.inf
        u, v = fx - a, fz - b
        c = [self.F[a, b], self.F[a + 1, b], self.F[a, b + 1], self.F[a + 1, b + 1]]
        w = [(1 - u) * (1 - v), u * (1 - v), (1 - u) * v, u * v]
        # corners with no rut: undug (no pull); a corner without rut next to one with rut blends to 'no pull'
        # by treating it as +big -> min() keeps the mesh. Bilinear over finite corners only, else inf.
        if not all(math.isfinite(q) for q in c):
            if not any(math.isfinite(q) for q in c):
                return math.inf
            hi = top_guard(x, z)
            c = [q if math.isfinite(q) else hi for q in c]
        return sum(q * ww for q, ww in zip(c, w))


def top_guard(x, z):
    return P.top_of(x, z) + 0.0


def split(t, n):
    """n x n barycentric split of triangle t (same edge points on both sides of a shared edge)."""
    A, B, C = (np.array(p) for p in t)
    pts = {}
    for i in range(n + 1):
        for j in range(n + 1 - i):
            k = n - i - j
            pts[(i, j)] = (A * k + B * i + C * j) / n
    out = []
    for i in range(n):
        for j in range(n - i):
            out.append((pts[(i, j)], pts[(i + 1, j)], pts[(i, j + 1)]))
            if i + j < n - 1:
                out.append((pts[(i + 1, j)], pts[(i + 1, j + 1)], pts[(i, j + 1)]))
    return out


def displace(tris, n, floor):
    res = []
    evals = 0
    for t in tris:
        for s in split(t, n):
            q = []
            for p in s:
                evals += 1
                f = floor.at(p[0], p[2])
                q.append((float(p[0]), float(min(p[1], f)), float(p[2])))
            res.append(tuple(q))
    return res, evals


def measure(name, method, tris, ops, tnear, tcloud, ms, extra=""):
    mp = P.mesh_samples(tris)
    mnear = P.Near(mp)
    m2t, t2m = P.two_way(mp, tcloud, tnear, mnear)
    wall = P.steep_wall(tris, ops)
    q = [P.tri_quality(t) for t in tris]
    tiny = sum(1 for a, _ in q if a < P.AREA_EPS)
    sliver = sum(1 for a, m in q if a >= P.AREA_EPS and m < P.SLIVER_DEG)
    r = dict(shape=name, method=method, tris=len(tris), tiny=tiny, sliver=sliver,
             m2t_max=m2t.max(), m2t_p95=np.percentile(m2t, 95), m2t_rms=math.sqrt((m2t ** 2).mean()),
             t2m_max=t2m.max(), t2m_p95=np.percentile(t2m, 95), t2m_rms=math.sqrt((t2m ** 2).mean()),
             wall=wall, ms=ms, extra=extra)
    print("%-13s %-9s tris %5d tiny %4d sliver %4d | mesh->true max %.3f rms %.3f | true->mesh max %.3f rms %.3f | wall %s | %.0f ms %s"
          % (name, method, r["tris"], tiny, sliver, r["m2t_max"], r["m2t_rms"], r["t2m_max"], r["t2m_rms"],
             "-" if wall is None else "%.0f" % wall, ms, extra), flush=True)
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png")
    ap.add_argument("--md")
    args = ap.parse_args()
    rows = []
    profiles = {}
    cuts = dict(P.CUTS)
    cuts["rut_x_crater"] = ("x", 6.05, 1.5, 10.5)
    for name, ops in shapes().items():
        tcloud, H, xs = P.true_cloud(ops)
        tnear = P.Near(tcloud)
        _, xc, z0, z1 = cuts[name]
        # control: ruts in the voxel field
        G = P.seed()
        for op in ops:
            op.apply(G)
        vx = P.Vertexer(G, "mean", ops)
        t0 = time.perf_counter()
        tris, _ = P.polygonize(G, vx)
        rows.append(measure(name, "vox05", tris, ops, tnear, tcloud, (time.perf_counter() - t0) * 1000))
        profiles[(name, "vox05")] = P.profile(tris, ops, xc, z0, z1)
        # coarse field without ruts
        G2 = P.seed()
        for op in ops:
            if not isinstance(op, P.Rut):
                op.apply(G2)
        vx2 = P.Vertexer(G2, "mean", ops)
        t0 = time.perf_counter()
        coarse, _ = P.polygonize(G2, vx2)
        coarse_ms = (time.perf_counter() - t0) * 1000
        ruts = [op for op in ops if isinstance(op, P.Rut)]
        for method, n, det in (("disp2", 2, 0.25), ("disp4", 4, 0.125), ("disp4g25", 4, 0.25)):
            fl = Floor(ruts, det)
            t0 = time.perf_counter()
            d, evals = displace(coarse, n, fl)
            ms = coarse_ms + (time.perf_counter() - t0) * 1000
            rows.append(measure(name, method, d, ops, tnear, tcloud, ms,
                                "floor samples %d (with rut %d), vertex evals %d" % (fl.n * fl.n, fl.cells, evals)))
            profiles[(name, method)] = P.profile(d, ops, xc, z0, z1)
    if args.md:
        L = ["| Shape | Method | Tris | Tiny | Slivers | mesh->true max | p95 | rms | true->mesh max | p95 | rms | Rut wall p90 deg | Python ms | Notes |",
             "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
        for r in rows:
            L.append("| %s | %s | %d | %d | %d | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %s | %.0f | %s |" % (
                r["shape"], r["method"], r["tris"], r["tiny"], r["sliver"], r["m2t_max"], r["m2t_p95"], r["m2t_rms"],
                r["t2m_max"], r["t2m_p95"], r["t2m_rms"], "-" if r["wall"] is None else "%.0f" % r["wall"], r["ms"], r["extra"]))
        open(args.md, "w", encoding="utf-8").write("\n".join(L) + "\n")
    if args.png:
        import os
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        os.makedirs(args.png, exist_ok=True)
        for name in sorted({k[0] for k in profiles}):
            fig, ax = plt.subplots(figsize=(9, 3.6))
            zs, _, th = profiles[(name, "vox05")]
            ax.plot(zs, th, color="black", lw=2.2, label="true")
            for m, col in zip(("vox05", "disp2", "disp4", "disp4g25"), ("#d62728", "#1f77b4", "#2ca02c", "#ff7f0e")):
                ax.plot(zs, profiles[(name, m)][1], color=col, lw=1.2, label=m)
            ax.set_title("%s - cross-section at x = %.2f m" % (name, cuts[name][1]))
            ax.set_xlabel("z (m)")
            ax.set_ylabel("y (m)")
            ax.set_aspect("equal", adjustable="datalim")
            ax.legend(fontsize=8, loc="lower right")
            fig.tight_layout()
            fig.savefig(os.path.join(args.png, name + ".png"), dpi=110)
            plt.close(fig)


if __name__ == "__main__":
    main()
