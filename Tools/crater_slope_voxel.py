"""python Tools/crater_slope_voxel.py  (results: docs/superpowers/results/2026-10-16-voxel-crater-slope.md)

Voxel crater wall slopes: today's Carve (ellipsoid + funnel + sine rim) vs a cosine bowl
(heightfield Crater profile, max slope 0.9 = 42 deg). Uses Tools/dc_vertex_proto.py (game Carve formula,
Polygonize + Quad, surface-nets mean vertex). Grid widened to 26 m so a 3 m crater fits."""
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dc_vertex_proto as P

SIZE = 26.0
P.NX = P.NZ = int(round(SIZE / P.CELL)) + 1
P.J0 = int(round(-7 / P.CELL))
P.NY = int(round(11 / P.CELL)) + 1
CX, CZ = 13.1, 12.9           # off the sample lines on purpose
MAXS = 0.9                     # m_fCraterMaxSlope
RIM_SHARE, RIM_W_SHARE = 0.17, 0.4


class CraterCos(P.Crater):
    """Heightfield-style profile as a vertical-distance field (as voxel ruts already use)."""

    def __init__(self, cx, cz, depth):
        super().__init__(cx, cz, depth)
        D = depth
        self.H = D * RIM_SHARE
        self.Rc = max(0.75, (D + self.H) * math.pi / (2 * MAXS))
        self.W = max(0.5, D * RIM_W_SHARE, self.H * math.pi / (2 * MAXS))
        self.outer = self.Rc + self.W
        self.rimH = self.H
        self.crest = self.Rc

    def value(self, x, y, z, old, undug):
        rr = math.hypot(x - self.cx, z - self.cz)
        D, H, Rc, W = self.depth, self.H, self.Rc, self.W
        if rr <= Rc:
            bump = H
        elif rr < Rc + W:
            bump = H * (1 + math.cos(math.pi * (rr - Rc) / W)) / 2
        else:
            bump = 0.0
        d = old + bump
        if rr < Rc:
            yb = H - (D + H) * (1 + math.cos(math.pi * rr / Rc)) / 2
            d = min(d, self.wy + yb - y)
        return max(d, (undug - P.FLOOR) - y)


def mesh(op):
    G = P.seed()
    op.apply(G)
    tris, _ = P.polygonize(G, P.Vertexer(G, "mean", [op]))
    return tris


def bucket(tris):
    B = {}
    for t in tris:
        xs = [p[0] for p in t]
        zs = [p[2] for p in t]
        for i in range(int(min(xs) // 0.5), int(max(xs) // 0.5) + 1):
            for k in range(int(min(zs) // 0.5), int(max(zs) // 0.5) + 1):
                B.setdefault((i, k), []).append(t)
    return B


def top_y(B, x, z):
    best = None
    for t in B.get((int(x // 0.5), int(z // 0.5)), ()):
        y = P.ray_y(t, x, z)
        if y is not None and (best is None or y > best):
            best = y
    return best


def measure(op):
    tris = mesh(op)
    B = bucket(tris)
    R = op.outer + 0.5
    # area-weighted slope inside the crater footprint
    a_all = a50 = a60 = 0.0
    smax = 0.0
    for t in tris:
        mid = [sum(p[q] for p in t) / 3 for q in range(3)]
        if math.hypot(mid[0] - CX, mid[2] - CZ) > R:
            continue
        n = P.cross(P.sub(t[1], t[0]), P.sub(t[2], t[0]))
        l = math.sqrt(P.dot(n, n))
        if l < 1e-9 or n[1] <= 0:
            continue
        ang = math.degrees(math.acos(min(1, n[1] / l)))
        area = l / 2
        a_all += area
        if ang > 50:
            a50 += area
        if ang > 60:
            a60 += area
        smax = max(smax, ang)
    # walk out: per azimuth, along the radial line, the biggest height gained in one run of >50 deg ground
    # (sampled 5 cm) and the steepest 0.25 m stretch
    worst_run = []
    steep25 = []
    for a in range(72):
        th = math.radians(a * 5)
        ys = []
        for s in np.arange(0.0, R, 0.05):
            y = top_y(B, CX + s * math.cos(th), CZ + s * math.sin(th))
            ys.append(y)
        ys = [y for y in ys if y is not None]
        run = best = 0.0
        for i in range(1, len(ys)):
            dy = ys[i] - ys[i - 1]
            if dy / 0.05 > math.tan(math.radians(50)):
                run += dy
                best = max(best, run)
            else:
                run = 0.0
        worst_run.append(best)
        st = 0.0
        for i in range(5, len(ys)):
            st = max(st, math.degrees(math.atan((ys[i] - ys[i - 5]) / 0.25)))
        steep25.append(st)
    return dict(tris=len(tris), outer=op.outer, max=smax, p50=100 * a50 / a_all, p60=100 * a60 / a_all,
                run_min=min(worst_run), run_med=float(np.median(worst_run)), run_max=max(worst_run),
                st_min=min(steep25), st_med=float(np.median(steep25)))


def main():
    rows = []
    for label, D in [("HEAT 0.51 kg", 0.60), ("O-832DU 0.40 kg", 0.86), ("81 mm 0.99 kg", 1.43),
                     ("Hydra 2.9 kg", 2.04), ("TM-62M 7.5 kg", 2.81)]:
        for kind, cls in [("voxel today", P.Crater), ("cosine bowl 42", CraterCos)]:
            m = measure(cls(CX, CZ, D))
            rows.append((label, D, kind, m))
            print(label, D, kind, {k: round(v, 2) if isinstance(v, float) else v for k, v in m.items()}, flush=True)

    print()
    print("| Charge | Depth m | Shape | Outer r m | Tris | Steepest tri deg | Area > 50 deg % | Area > 60 deg % | "
          "Steep climb > 50 deg on the EASIEST way out m | ... median way m | Steepest 0.25 m stretch, easiest way deg |")
    print("|---|---|---|---|---|---|---|---|---|---|---|")
    for label, D, kind, m in rows:
        print(f"| {label} | {D} | {kind} | {m['outer']:.2f} | {m['tris']} | {m['max']:.0f} | {m['p50']:.1f} | {m['p60']:.1f} | "
              f"{m['run_min']:.2f} | {m['run_med']:.2f} | {m['st_min']:.0f} |")


if __name__ == "__main__":
    main()
