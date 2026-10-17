"""Offline proof for the fixes in docs/superpowers/plans/2026-10-16-ground-improvement-plan.md (2026-10-16).
Same field, mesher and metrics as Tools/gap_checks.py / Tools/crater_slope_voxel.py; each fix is the proposed
Enforce change written in Python.

    python Tools/fix_checks.py crater | trench | tunnel | ruts | all

Results: docs/superpowers/results/2026-10-16-fix-checks.md
"""
import math
import os
import random
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dc_vertex_proto as P
import gap_checks as GC

MAXS = 0.9            # m_fCraterMaxSlope (42 deg)
RIM_SHARE = 0.17      # m_fCraterRimShare
RIM_W_SHARE = 0.4     # m_fCraterRimWidthShare
EDGE_FADE = 1.0       # m_fEdgeFade (heightfield road-edge fade): at least this
FADE_PER_M = 1.7      # fade length per m of crater depth (smoothstep slope 1.5 x D / fade <= 0.9)
STACK_DEPTH = 2.5     # m_fCraterStackDepth
STACK_FACTOR = 1.67   # m_fCraterStackFactor
ROOF_PROBE = 3.0      # under-roof test: solid within this far above the hit (tunnel clearance 2.15-2.4 m)
FLOOR = 3.0


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


class CraterV2(P.Crater):
    """Fix C1-C4: cosine bowl (heightfield profile) as a vertical-distance field, road-edge fade (lock), stack cap
    (absolute bottom never under undug - min(2.5, 1.67 x depth)), sphere when the hit is under a roof."""

    def __init__(self, cx, cz, depth, wy=None, road_x=None, G=None):
        super().__init__(cx, cz, depth)
        if wy is not None:
            self.wy = wy
        D = depth
        self.H = D * RIM_SHARE
        self.Rc = max(0.75, (D + self.H) * math.pi / (2 * MAXS))
        self.W = max(0.5, D * RIM_W_SHARE, self.H * math.pi / (2 * MAXS))
        self.outer = self.Rc + self.W
        self.rimH = self.H
        self.crest = self.Rc
        self.road_x = road_x
        self.cap = min(STACK_DEPTH, STACK_FACTOR * D)
        self.sphere = False
        if G is not None:
            gi, gk = int(round(cx / P.CELL)), int(round(cz / P.CELL))
            for gj in range(math.ceil((self.wy + 0.5) / P.CELL), math.floor((self.wy + ROOF_PROBE) / P.CELL) + 1):
                if P.J0 <= gj < P.J0 + P.NY and G[gi, gj - P.J0, gk] > 0:
                    self.sphere = True
                    break

    def lock(self, x):
        if self.road_x is None:
            return 1.0
        if x <= self.road_x + 1e-6:
            return 0.0
        return smoothstep((x - self.road_x) / max(EDGE_FADE, FADE_PER_M * self.depth))

    def value(self, x, y, z, old, undug):
        if self.sphere:
            s = math.sqrt((x - self.cx) ** 2 + (y - self.wy) ** 2 + (z - self.cz) ** 2) - self.depth
            return max(min(old, s), (undug - FLOOR) - y)
        rr = math.hypot(x - self.cx, z - self.cz)
        D, H, Rc, W = self.depth, self.H, self.Rc, self.W
        lk = self.lock(x)
        if rr <= Rc:
            bump = H
        elif rr < Rc + W:
            bump = H * (1 + math.cos(math.pi * (rr - Rc) / W)) / 2
        else:
            bump = 0.0
        d = old + bump * lk
        if rr < Rc:
            yb = H - (D + H) * (1 + math.cos(math.pi * rr / Rc)) / 2
            target = self.wy + yb * lk
            target = max(target, undug - self.cap)
            d = min(d, target - y)
        return max(d, (undug - FLOOR) - y)

    def apply(self, G):
        gi0 = math.floor((self.cx - self.outer) / P.CELL)
        gi1 = math.ceil((self.cx + self.outer) / P.CELL)
        gk0 = math.floor((self.cz - self.outer) / P.CELL)
        gk1 = math.ceil((self.cz + self.outer) / P.CELL)
        gj0 = math.floor((self.wy - self.depth - 1) / P.CELL)
        gj1 = math.ceil((self.wy + FLOOR + self.rimH + 1.5) / P.CELL)
        for gk in range(max(gk0, 0), min(gk1, P.NZ - 1) + 1):
            for gi in range(max(gi0, 0), min(gi1, P.NX - 1) + 1):
                x, z = gi * P.CELL, gk * P.CELL
                if self.road_x is not None and x <= self.road_x + 1e-6:
                    continue
                undug = P.top_of(x, z)
                for gj in range(max(gj0, P.J0), min(gj1, P.J0 + P.NY - 1) + 1):
                    y = gj * P.CELL
                    G[gi, gj - P.J0, gk] = P.nudge(self.value(x, y, z, G[gi, gj - P.J0, gk], undug))


def deepest(G):
    best = 0.0
    for gk in range(P.NZ):
        for gi in range(P.NX):
            c = GC.crossings_down(G, gi, gk)
            if c:
                best = max(best, P.top_of(gi * P.CELL, gk * P.CELL) - c[0][0])
    return best


def crater():
    # walk-out and footprint: reuse crater_slope_voxel's measure on the new class
    import crater_slope_voxel as CS

    class V2(CraterV2):
        def __init__(self, cx, cz, depth):
            super().__init__(cx, cz, depth)

    print("### Walk-out (same metric as results/2026-10-16-voxel-crater-slope.md)")
    print("| Charge | Depth m | Outer r m | Steepest tri deg | Area > 50 deg % | Steep climb, easiest way m |")
    print("|---|---|---|---|---|---|")
    for label, D in (("81 mm", 1.43), ("Hydra", 2.04), ("TM-62M", 2.81)):
        m = CS.measure(V2(CS.CX, CS.CZ, D))
        print(f"| {label} | {D} | {m['outer']:.2f} | {m['max']:.0f} | {m['p50']:.1f} | {m['run_min']:.2f} |", flush=True)

    print()
    print("### Road edge (centre 1 / 2 m from the edge; drop over the first 0.5 m and steepest 0.5 m step within 2 m)")
    print("| Crater | Centre from edge m | Today: drop in first 0.5 m | Fix: drop in first 0.5 m | Fix: steepest 0.5 m step within 2 m deg |")
    print("|---|---|---|---|---|")
    road_x = 8.0
    for label, D in (("81 mm", 1.43), ("Hydra", 2.04)):
        for off in (1.0, 2.0):
            res = []
            for cls in ("today", "fix"):
                G = GC.grid(20, 16, -6, 4)
                z = 8.1
                if cls == "today":
                    GC.CraterHard(road_x + off, z, D, road_x).apply(G)
                else:
                    CraterV2(road_x + off, z, D, road_x=road_x).apply(G)
                gk = int(round(z / P.CELL))
                ys = []
                for gi in range(int(round(road_x / P.CELL)), int(round((road_x + 2.5) / P.CELL)) + 1):
                    c = GC.crossings_down(G, gi, gk)
                    ys.append(c[0][0] - P.top_of(gi * P.CELL, z))
                steep = max(math.degrees(math.atan(abs(ys[i] - ys[i - 1]) / P.CELL)) for i in range(1, len(ys)))
                res.append((-ys[1], steep))
            print(f"| {label} {D} m | {off} | {res[0][0]:.2f} | {res[1][0]:.2f} | {res[1][1]:.0f} |", flush=True)

    print()
    print("### Repeated hits (scattered +-1 m)")
    print("| Charge | Hit | Today deepest m | Fix deepest m |")
    print("|---|---|---|---|")
    for label, D in (("81 mm", 1.43), ("O-832DU", 0.86)):
        random.seed(3)
        pts = [(11.1 + random.uniform(-1, 1), 10.9 + random.uniform(-1, 1)) for _ in range(6)]
        Gt = GC.grid(22, 22, -6, 4)
        Gf = GC.grid(22, 22, -6, 4)
        for hit, (x, z) in enumerate(pts, 1):
            for G, cls in ((Gt, "t"), (Gf, "f")):
                P.NX = P.NZ = int(round(22 / P.CELL)) + 1
                c = GC.crossings_down(G, int(round(x / P.CELL)), int(round(z / P.CELL)))
                wy = c[0][0] if c else P.top_of(x, z)
                if cls == "t":
                    GC.CraterAt(x, z, D, wy).apply(G)
                else:
                    CraterV2(x, z, D, wy=wy, G=G).apply(G)
            print(f"| {label} {D} m | {hit} | {deepest(Gt):.2f} | {deepest(Gf):.2f} |", flush=True)

    print()
    print("### Blast inside a tunnel (new sky-open columns)")
    print("| Charge | Depth m | Today: new open columns / radius m | Fix (sphere under a roof): new open columns / radius m |")
    print("|---|---|---|---|")
    for label, D in (("RPG HEAT", 0.60), ("0.76 kg charge", 1.27)):
        out = []
        for cls in ("today", "fix"):
            G, _ = GC.dig_tunnel()
            z, x = 4.1, 12.0
            fy = GC.floor_under(G, x, z, P.top_of(x, z) - 1.0)

            def open_cols():
                s = set()
                for gk in range(P.NZ):
                    for gi in range(P.NX):
                        c = GC.crossings_down(G, gi, gk)
                        ce = [y for y, k in c if k == "ceil"]
                        tp = [y for y, k in c if k == "top"]
                        if not ce and tp and tp[0] < P.top_of(gi * P.CELL, gk * P.CELL) - 1.5:
                            s.add((gi, gk))
                return s
            before = open_cols()
            if cls == "today":
                GC.CraterAt(x, z, D, fy + 0.3).apply(G)
            else:
                CraterV2(x, z, D, wy=fy + 0.3, G=G).apply(G)
            new = open_cols() - before
            r = max((math.hypot(gi * P.CELL - x, gk * P.CELL - z) for gi, gk in new), default=0.0)
            out.append((len(new), r))
        print(f"| {label} | {D} | {out[0][0]} / {out[0][1]:.2f} | {out[1][0]} / {out[1][1]:.2f} |", flush=True)


# ------------------------------------------------------------------ trench width fix

def trench():
    """Fix T1: CutTrench treats a column within PAD of the floor rectangle as floor (want = full depth)."""
    print("| Floor width planned m | Pad m | Angle | Offset | Drawn floor m (range) | Wall p50 / max deg |")
    print("|---|---|---|---|---|---|")
    for w in (0.5, 1.0, 1.5):
        for pad in (0.0, 0.25):
            for ang, off in ((0, 0.0), (0, 0.25), (30, 0.0)):
                G = GC.grid(14, 14, -4, 3)
                cx, cz = 7.05 + off, 7.05 + off
                GC.TrenchRot(cx, cz, 8.0, w + 2 * pad, ang, 6).apply(G)
                widths, slopes = section(G, cx, cz, ang)
                print(f"| {w} | {pad} | {ang} | {off} | {np.mean(widths):.2f} ({min(widths):.2f}-{max(widths):.2f}) | "
                      f"{np.percentile(slopes, 50):.0f} / {max(slopes):.0f} |", flush=True)


def section(G, cx, cz, ang):
    tris, _ = P.polygonize(G, P.Vertexer(G, "mean", []))
    B = {}
    for t in tris:
        xs = [p[0] for p in t]
        zs = [p[2] for p in t]
        for i in range(int(min(xs) // 0.5), int(max(xs) // 0.5) + 1):
            for k in range(int(min(zs) // 0.5), int(max(zs) // 0.5) + 1):
                B.setdefault((i, k), []).append(t)
    a = math.radians(ang)
    widths, slopes = [], []
    for st in (-2.0, -0.7, 0.6, 1.9):
        px, pz = cx + st * math.cos(a), cz + st * math.sin(a)
        prof = []
        for v in np.arange(-3, 3.001, 0.02):
            x, z = px - v * math.sin(a), pz + v * math.cos(a)
            best = None
            for t in B.get((int(x // 0.5), int(z // 0.5)), ()):
                y = P.ray_y(t, x, z)
                if y is not None and (best is None or y > best):
                    best = y
            if best is not None:
                prof.append((v, best - P.top_of(x, z)))
        bot = min(y for _, y in prof)
        widths.append(sum(0.02 for _, y in prof if y < bot + 0.05))
        for i in range(1, len(prof)):
            dv = prof[i][0] - prof[i - 1][0]
            dy = abs(prof[i][1] - prof[i - 1][1])
            if -1.35 < prof[i][1] < -0.15:
                slopes.append(math.degrees(math.atan(dy / dv)))
    return widths, slopes


# ------------------------------------------------------------------ tunnel FLOOR fix

def tunnel():
    """Fix U1: FLOOR for tunnel ops 6 m (craters / trenches keep 3). The player digs down, then level at a chosen depth."""
    print("| Tunnel FLOOR m | Player levels off at feet under undug m | Roof (sample field) min-max m | Clearance m |")
    print("|---|---|---|---|")
    for floor, level_at in ((3.0, 3.0), (6.0, 3.0), (6.0, 4.0), (6.0, 5.0)):
        GC.FLOOR = floor
        G = GC.grid(20, 8.5, -8, 4)
        z = 4.1
        feet = [3.0, P.top_of(3.0, z), z]
        pitch = -0.7
        while feet[0] < 16:
            mid = (feet[0] + GC.TUNNEL_AHEAD, feet[1] + GC.TUNNEL_MID + pitch * GC.TUNNEL_SLOPE, z)
            GC.carve_tunnel(G, mid)
            nx = feet[0] + 0.5
            fy = GC.floor_under(G, nx, z, feet[1] + 0.6)
            if fy is None:
                break
            if pitch < 0 and (P.top_of(nx, z) - fy >= level_at - 0.05 or fy > feet[1] - 0.05):
                pitch = 0.0
            feet = [nx, fy, z]
        gk = int(round(z / P.CELL))
        roofs, clear = [], []
        for gi in range(int(9 / P.CELL), int(15 / P.CELL) + 1):
            c = GC.crossings_down(G, gi, gk)
            tops = [y for y, k in c if k == "top"]
            ceils = [y for y, k in c if k == "ceil"]
            if ceils and len(tops) > 1:
                roofs.append(tops[0] - ceils[0])
                clear.append(ceils[0] - tops[1])
        print(f"| {floor} | {level_at} | {min(roofs):.2f}-{max(roofs):.2f} | {np.mean(clear):.2f} |", flush=True)
    GC.FLOOR = 3.0


# ------------------------------------------------------------------ rut radius by depth

def ruts():
    """Fix R1: RUT_R grows with the cap so the wall stays <= ~45 deg: r = max(0.5, cap / 0.58)."""
    print("| Vehicle | Cap m | RUT_R today -> fix m | Wall p90 deg today -> fix | Steepest today -> fix |")
    print("|---|---|---|---|---|")
    for tonnes, label in ((2.5, "UAZ ~2.5 t"), (6, "~6 t"), (11, "M923 / Ural ~11 t")):
        cap = GC.rut_cap(tonnes)
        out = []
        for r in (0.5, max(0.5, cap / 0.58)):
            GC.RUT_R = r
            p90, worst = [], []
            for ang in (0, 15, 30, 45):
                s = GC.tri_slopes(GC.rut_grid(ang, cap, GC.PROFILES["1-k2 (code today)"]))
                p90.append(float(np.percentile(s, 90)))
                worst.append(max(s))
            out.append((r, max(p90), max(worst)))
        GC.RUT_R = 0.5
        print(f"| {label} | {cap:.3f} | {out[0][0]:.2f} -> {out[1][0]:.2f} | {out[0][1]:.1f} -> {out[1][1]:.1f} | "
              f"{out[0][2]:.1f} -> {out[1][2]:.1f} |", flush=True)


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    for name, fn in (("crater", crater), ("trench", trench), ("tunnel", tunnel), ("ruts", ruts)):
        if what in (name, "all"):
            print(f"## {name}")
            fn()
            print()
