"""Offline fact checks for docs/superpowers/research/2026-10-16-architecture-gaps-deep-dive.md (2026-10-16).
Game formulas copied from Scripts/Game/Mud/MUD_VoxelProbe.c (CarveFloor, CarveTunnel, Carve, hard columns) and
MUD_WorldGround.c (rut cap); meshing = Tools/dc_vertex_proto.py (Polygonize + Quad, surface-nets mean = CellPos).

    python Tools/gap_checks.py ruts | tunnel | blast | road | all

Results: docs/superpowers/results/2026-10-16-gap-checks.md
"""
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dc_vertex_proto as P

FCELL = 0.25
RUT_R = 0.5
RUT_ON = 0.6
FLOOR = 3.0
TUNNEL_R, TUNNEL_HALF, TUNNEL_AHEAD, TUNNEL_MID, TUNNEL_SLOPE = 0.8, 0.45, 1.1, 0.95, 1.0
KEPT_SINK = 0.3


# ------------------------------------------------------------------ 1. rut floor slopes (CarveFloor on 0.25 m)

def rut_cap(tonnes):
    """MUD_WorldGround.c:1191 depth = min(base 0.08 + t x 0.025, max 0.35)"""
    return min(0.08 + tonnes * 0.025, 0.35)


PROFILES = {
    "1-k2 (code today)": lambda k: 1 - k * k,
    "(1-k2)^2 (plan Step 4)": lambda k: (1 - k * k) ** 2,
    "cosine": lambda k: 0.5 * (1 + math.cos(math.pi * k)),
}


def rut_grid(angle_deg, cap, prof, passes=8, step=0.08, size=8.0, length_share=1.6):
    """length_share 1.6: the track runs past the grid (walls only); 0.7: it starts and ends inside (rut ends)"""
    n = int(size / FCELL) + 1
    undug = np.zeros((n, n))
    floor = np.full((n, n), np.inf)
    a = math.radians(angle_deg)
    cx = cz = size / 2
    L = size * length_share
    ax, az = cx - math.cos(a) * L / 2, cz - math.sin(a) * L / 2
    pts = [(ax + math.cos(a) * s, az + math.sin(a) * s) for s in np.arange(0, L + 1e-6, 0.5)]
    segs = list(zip(pts[:-1], pts[1:]))
    for _ in range(passes):
        best = {}
        for (sax, saz), (sbx, sbz) in segs:
            abx, abz = sbx - sax, sbz - saz
            l2 = abx * abx + abz * abz
            for fk in range(n):
                for fi in range(n):
                    px, pz = fi * FCELL, fk * FCELL
                    u = ((px - sax) * abx + (pz - saz) * abz) / l2
                    if u < 0 or u >= 1:
                        continue
                    d = math.hypot(sax + abx * u - px, saz + abz * u - pz)
                    if d >= RUT_R:
                        continue
                    f = prof(d / RUT_R)
                    if f > best.get((fi, fk), -1):
                        best[(fi, fk)] = f
        for (fi, fk), f in best.items():
            surf = min(undug[fk, fi], floor[fk, fi])
            fy = surf - step * f
            fy = max(fy, undug[fk, fi] - cap * f, undug[fk, fi] - FLOOR)
            if fy >= surf:
                continue
            floor[fk, fi] = fy
    h = np.minimum(undug, floor)
    return h


def tri_slopes(h):
    """Every grid cell split on its tl-br diagonal (and, as a check, bl-tr); slope deg of each triangle that dips."""
    out = []
    n = h.shape[0]
    for k in range(n - 1):
        for i in range(n - 1):
            p00 = (0, h[k, i], 0)
            p10 = (FCELL, h[k, i + 1], 0)
            p01 = (0, h[k + 1, i], FCELL)
            p11 = (FCELL, h[k + 1, i + 1], FCELL)
            for t in ((p00, p10, p11), (p00, p11, p01), (p00, p10, p01), (p10, p11, p01)):
                if min(q[1] for q in t) > -0.005:
                    continue
                nrm = P.cross(P.sub(t[1], t[0]), P.sub(t[2], t[0]))
                l = math.sqrt(P.dot(nrm, nrm))
                out.append(math.degrees(math.acos(min(1, abs(nrm[1]) / l))))
    return out


def ruts():
    rows = []
    for tonnes, label in ((2.5, "UAZ ~2.5 t"), (11, "M923 / Ural ~11 t")):
        cap = rut_cap(tonnes)
        for pname, prof in PROFILES.items():
            worst = []
            p90 = []
            depth = 0
            for ang in (0, 15, 30, 45):
                h = rut_grid(ang, cap, prof)
                s = tri_slopes(h)
                worst.append(max(s))
                p90.append(float(np.percentile(s, 90)))
                depth = max(depth, -h.min())
            rows.append((label, round(cap, 3), pname, round(depth, 3), round(max(p90), 1), round(max(worst), 1)))
            print(rows[-1], flush=True)
    ends = []
    for ang in (0, 15, 30, 45):
        ends.append(max(tri_slopes(rut_grid(ang, rut_cap(11), PROFILES["1-k2 (code today)"], length_share=0.7))))
    print()
    print(f"With the rut's start / end inside the grid (vehicle starts or stops): steepest triangle {max(ends):.1f} deg")
    print()
    print("| Vehicle | Cap m | Profile | Deepest m | Rut wall slope p90 deg (worst of 0/15/30/45 deg tracks) | Steepest triangle deg |")
    print("|---|---|---|---|---|---|")
    for r in rows:
        print("| " + " | ".join(str(x) for x in r) + " |")


# ------------------------------------------------------------------ 3D grid helpers

def grid(size_x, size_z, y0, y1):
    P.NX = int(round(size_x / P.CELL)) + 1
    P.NZ = int(round(size_z / P.CELL)) + 1
    P.J0 = int(round(y0 / P.CELL))
    P.NY = int(round((y1 - y0) / P.CELL)) + 1
    return P.seed()


def col_vals(G, gi, gk):
    return [(((P.J0 + j) * P.CELL), G[gi, j, gk]) for j in range(P.NY)]


def crossings_down(G, gi, gk):
    """Sign changes in one sample column, top to bottom: list of (y, 'top' solid below air | 'ceil' air below solid)."""
    v = col_vals(G, gi, gk)
    out = []
    for j in range(P.NY - 1, 0, -1):
        (ya, da), (yb, db) = v[j], v[j - 1]       # a above, b below
        if (da > 0) == (db > 0):
            continue
        y = yb + db / (db - da) * (ya - yb)
        out.append((y, "top" if db > 0 else "ceil"))
    return out


def carve_tunnel(G, mid, hard=lambda x, z: False):
    """MUD_VoxelProbe.CarveTunnel (r TUNNEL_R, half TUNNEL_HALF, FLOOR under the undug ground)."""
    r, h = TUNNEL_R, TUNNEL_HALF
    gi0, gi1 = math.floor((mid[0] - r) / P.CELL), math.ceil((mid[0] + r) / P.CELL)
    gk0, gk1 = math.floor((mid[2] - r) / P.CELL), math.ceil((mid[2] + r) / P.CELL)
    gj0, gj1 = math.floor((mid[1] - h - r) / P.CELL), math.ceil((mid[1] + h + r) / P.CELL)
    for gk in range(max(gk0, 0), min(gk1, P.NZ - 1) + 1):
        for gi in range(max(gi0, 0), min(gi1, P.NX - 1) + 1):
            x, z = gi * P.CELL, gk * P.CELL
            if hard(x, z):
                continue
            floor_y = P.top_of(x, z) - FLOOR
            for gj in range(max(gj0, P.J0), min(gj1, P.J0 + P.NY - 1) + 1):
                y = gj * P.CELL
                dy = y - mid[1]
                dy = dy - max(-h, min(h, dy))
                s = math.sqrt((x - mid[0]) ** 2 + dy * dy + (z - mid[2]) ** 2) - r
                old = G[gi, gj - P.J0, gk]
                d = min(old, s)
                d = max(d, floor_y - y)
                G[gi, gj - P.J0, gk] = P.nudge(d)


def floor_under(G, x, z, from_y):
    """Highest 'top' crossing at or below from_y in the nearest sample column (where the player would stand)."""
    gi, gk = int(round(x / P.CELL)), int(round(z / P.CELL))
    for y, kind in crossings_down(G, gi, gk):
        if kind == "top" and y <= from_y + 1e-6:
            return y
    return None


def dig_tunnel(x_start=3.0, x_end=16.0, z=4.1):
    """A player on foot holding T, walking +x: looks down (pitch -0.7) until the floor stops sinking, then level.
    One dig per 0.5 m walked (TUNNEL_EVERY_MS 300 at ~1.7 m/s). Feet follow the dug floor."""
    G = grid(20, 8.5, -6, 4)
    feet = [x_start, P.top_of(x_start, z), z]
    pitch = -0.7
    log = []
    while feet[0] < x_end:
        mid = (feet[0] + TUNNEL_AHEAD, feet[1] + TUNNEL_MID + pitch * TUNNEL_SLOPE, feet[2])
        carve_tunnel(G, mid)
        nx = feet[0] + 0.5
        fy = floor_under(G, nx, z, feet[1] + 0.6)
        if fy is None:
            break
        if pitch < 0 and fy > feet[1] - 0.05 and feet[1] < P.top_of(nx, z) - 1.5:
            pitch = 0.0      # floor stopped sinking (FLOOR reached): dig level from here
        feet = [nx, fy, z]
        log.append((round(nx, 2), round(fy - P.top_of(nx, z), 2), pitch))
    return G, log


def tunnel():
    G, log = dig_tunnel()
    z = 4.1
    gk = int(round(z / P.CELL))
    print("walk (x, feet under undug, pitch):", log)
    rows = []
    for gi in range(int(4 / P.CELL), int(16 / P.CELL) + 1):
        x = gi * P.CELL
        c = crossings_down(G, gi, gk)
        tops = [y for y, k in c if k == "top"]
        ceils = [y for y, k in c if k == "ceil"]
        if not ceils:
            continue
        surf, ceil = tops[0], ceils[0]
        flo = tops[1] if len(tops) > 1 else None
        solid = sum(1 for (y, d) in col_vals(G, gi, gk) if ceil < y < surf and d > 0)
        rows.append((x, round(P.top_of(x, z) - surf, 2), round(surf - ceil, 2), round(ceil - flo, 2) if flo else None,
                     round(P.top_of(x, z) - flo, 2) if flo else None, solid))
    # mesh check: rays down through the drawn mesh along the axis
    tris, _ = P.polygonize(G, P.Vertexer(G, "mean", []))
    mesh_roof = []
    for x in np.arange(4, 16, 0.25):
        ys = sorted((P.ray_y(t, x, z) for t in tris if P.ray_y(t, x, z) is not None), reverse=True)
        ys = [y for i, y in enumerate(ys) if i == 0 or ys[i - 1] - y > 1e-4]
        if len(ys) >= 3:
            mesh_roof.append(ys[0] - ys[1])
        elif len(ys) == 1:
            mesh_roof.append(0.0)      # only one surface: open to the sky (or no tunnel here)
    print()
    print("| x m | surface under undug m | roof thickness m (samples) | clearance m | floor under undug m | solid samples in roof |")
    print("|---|---|---|---|---|---|")
    for r in rows:
        print(f"| {r[0]} | {r[1]} | {r[2]} | {r[3]} | {r[4]} | {r[5]} |")
    full = [r for r in rows if r[3] and r[3] >= 1.8]
    print()
    print(f"columns with a tunnel: {len(rows)}; with >= 1.8 m clearance: {len(full)}; "
          f"roof over those: min {min(r[2] for r in full) if full else '-'} max {max(r[2] for r in full) if full else '-'} m")
    if mesh_roof:
        print(f"mesh roof along the axis (0.25 m steps): min {min(mesh_roof):.2f} max {max(mesh_roof):.2f} m, "
              f"spots with one surface only: {sum(1 for m in mesh_roof if m == 0.0)} of {len(mesh_roof)}")
    return G


# ------------------------------------------------------------------ 3. a blast inside the tunnel

class CraterAt(P.Crater):
    def __init__(self, cx, cz, depth, wy):
        super().__init__(cx, cz, depth)
        self.wy = wy


def blast():
    print("| Charge (FullDepth) | Depth m | Blast at | NEW sample columns open to the sky (0.5 m grid) | Widest open radius m |")
    print("|---|---|---|---|---|")
    for label, depth in (("RPG HEAT 0.51 kg x 0.6", 0.60), ("a 0.76 kg charge (example)", 1.27), ("81 mm 0.99 kg", 1.43)):
        G, _ = dig_tunnel()
        z = 4.1
        x = 12.0
        fy = floor_under(G, x, z, P.top_of(x, z) - 1.0)

        def open_cols():
            out = set()
            for gk in range(P.NZ):
                for gi in range(P.NX):
                    c = crossings_down(G, gi, gk)
                    ceils = [y for y, k in c if k == "ceil"]
                    tops = [y for y, k in c if k == "top"]
                    if not ceils and tops and tops[0] < P.top_of(gi * P.CELL, gk * P.CELL) - 1.5:
                        out.add((gi, gk))
            return out
        before = open_cols()      # the entrance ramp is open by design
        CraterAt(x, z, depth, fy + 0.3).apply(G)
        opened = [math.hypot(gi * P.CELL - x, gk * P.CELL - z) for (gi, gk) in open_cols() - before]
        print(f"| {label} | {depth} | tunnel floor + 0.3 m at x {x} | {len(opened)} | {max(opened) if opened else 0:.2f} |",
              flush=True)


# ------------------------------------------------------------------ 4. a crater next to a road (hard columns)

class CraterHard(P.Crater):
    """Carve skips hard columns (MUD_VoxelProbe.c:2684); kept columns are seeded KEPT_SINK under the terrain."""

    def __init__(self, cx, cz, depth, road_x):
        super().__init__(cx, cz, depth)
        self.road_x = road_x

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
                if x <= self.road_x + 1e-6:      # kept (x < road) or hard (on the line) - never dug
                    continue
                undug = P.top_of(x, z)
                for gj in range(max(gj0, P.J0), min(gj1, P.J0 + P.NY - 1) + 1):
                    y = gj * P.CELL
                    G[gi, gj - P.J0, gk] = P.nudge(self.value(x, y, z, G[gi, gj - P.J0, gk], undug))


def road():
    print("| Crater | Centre from road edge m | Drop from the road edge to the first dug sample (0.5 m in) m | Slope over that 0.5 m deg (sample surface) |")
    print("|---|---|---|---|")
    road_x = 8.0
    for label, depth in (("O-832DU", 0.86), ("81 mm", 1.43), ("Hydra", 2.04)):
        for off in (1.0, 2.0, 3.0):
            G = grid(20, 16, -6, 4)
            z = 8.1
            CraterHard(road_x + off, z, depth, road_x).apply(G)
            gk = int(round(z / P.CELL))
            gi = int(round((road_x + 0.5) / P.CELL))
            c = crossings_down(G, gi, gk)
            y_in = c[0][0] if c else None
            drop = P.top_of(road_x, z) - y_in
            ang = math.degrees(math.atan(drop / 0.5))
            print(f"| {label} {depth} m | {off} | {drop:.2f} | {ang:.0f} |", flush=True)


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    if what in ("ruts", "all"):
        ruts()
    if what in ("tunnel", "all"):
        tunnel()
    if what in ("blast", "all"):
        blast()
    if what in ("road", "all"):
        road()


def rut_radius():
    """Rut wall slope vs rut radius at the heaviest cap (code profile)."""
    global RUT_R
    keep = RUT_R
    print("| RUT_R m | Cap m | Rut wall slope p90 deg (worst of 0/15/30/45) | Steepest triangle deg |")
    print("|---|---|---|---|")
    for r in (0.5, 0.6, 0.75, 1.0):
        RUT_R = r
        p90, worst = [], []
        for ang in (0, 15, 30, 45):
            s = tri_slopes(rut_grid(ang, 0.35, PROFILES["1-k2 (code today)"]))
            p90.append(float(np.percentile(s, 90)))
            worst.append(max(s))
        print(f"| {r} | 0.35 | {max(p90):.1f} | {max(worst):.1f} |", flush=True)
    RUT_R = keep


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "rutr":
    rut_radius()


# ------------------------------------------------------------------ 5. trench cross-sections on 0.5 m samples

class TrenchRot(P.Trench):
    """CutTrench for one straight piece of floor width w along angle a (OutsideDist = distance to the rectangle)."""

    def __init__(self, cx, cz, length, width, angle_deg, layers):
        self.cx, self.cz, self.L, self.W = cx, cz, length, width
        self.a = math.radians(angle_deg)
        self.depth = layers * P.LAYER
        self.run = self.depth * P.SLOPE_RUN

    def want(self, x, z):
        dx, dz = x - self.cx, z - self.cz
        u = dx * math.cos(self.a) + dz * math.sin(self.a)
        v = -dx * math.sin(self.a) + dz * math.cos(self.a)
        ou = max(abs(u) - self.L / 2, 0)
        ov = max(abs(v) - self.W / 2, 0)
        od = math.hypot(ou, ov)
        if od == 0:
            return self.depth
        if od >= self.run:
            return -1
        return self.depth - od / P.SLOPE_RUN


def trench():
    print("| Floor width planned m | Angle deg | Centre offset m | Drawn floor width (within 5 cm of the bottom) m | Bottom drawn under undug m (planned 1.5) | Wall slope p50 / max deg (planned 60) |")
    print("|---|---|---|---|---|---|")
    for w in (0.5, 1.0, 1.5, 2.0):
        for ang, off in ((0, 0.0), (0, 0.25), (30, 0.0)):
            G = grid(14, 14, -4, 3)
            cx, cz = 7.05 + off, 7.05 + off
            TrenchRot(cx, cz, 8.0, w, ang, 6).apply(G)
            tris, _ = P.polygonize(G, P.Vertexer(G, "mean", []))
            B = {}
            for t in tris:
                xs = [p[0] for p in t]
                zs = [p[2] for p in t]
                for i in range(int(min(xs) // 0.5), int(max(xs) // 0.5) + 1):
                    for k in range(int(min(zs) // 0.5), int(max(zs) // 0.5) + 1):
                        B.setdefault((i, k), []).append(t)
            a = math.radians(ang)
            widths, bottoms, slopes = [], [], []
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
                bottoms.append(-bot)
                widths.append(sum(0.02 for _, y in prof if y < bot + 0.05))
                for i in range(1, len(prof)):
                    dv = prof[i][0] - prof[i - 1][0]
                    dy = abs(prof[i][1] - prof[i - 1][1])
                    if -1.35 < prof[i][1] < -0.15:
                        slopes.append(math.degrees(math.atan(dy / dv)))
            print(f"| {w} | {ang} | {off} | {np.mean(widths):.2f} ({min(widths):.2f}-{max(widths):.2f}) | "
                  f"{np.mean(bottoms):.2f} | {np.percentile(slopes, 50):.0f} / {max(slopes):.0f} |", flush=True)


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "trench":
    trench()


# ------------------------------------------------------------------ 6. repeated hits on one spot (voxel path)

def stack():
    """Crater() hands the voxel path only FullDepth (no stack cap): each new hit is a fresh Carve centred on the
    current surface. Heightfield rule for comparison: depth <= min(m_fCraterStackDepth 2.5, 1.67 x full)."""
    import random
    random.seed(3)
    print("| Charge | Hit | Deepest point under undug m (voxel today) | Heightfield cap m |")
    print("|---|---|---|---|")
    for label, depth in (("81 mm", 1.43), ("O-832DU", 0.86)):
        G = grid(22, 22, -6, 4)
        cx, cz = 11.1, 10.9
        for hit in range(1, 7):
            x = cx + random.uniform(-1, 1)
            z = cz + random.uniform(-1, 1)
            c = crossings_down(G, int(round(x / P.CELL)), int(round(z / P.CELL)))
            wy = c[0][0] if c else P.top_of(x, z)
            CraterAt(x, z, depth, wy).apply(G)
            deepest = 0.0
            for gk in range(P.NZ):
                for gi in range(P.NX):
                    cc = crossings_down(G, gi, gk)
                    if cc:
                        deepest = max(deepest, P.top_of(gi * P.CELL, gk * P.CELL) - cc[0][0])
            print(f"| {label} {depth} m | {hit} | {deepest:.2f} | {min(2.5, 1.67 * depth):.2f} |", flush=True)


if __name__ == "__main__" and len(sys.argv) > 1 and sys.argv[1] == "stack":
    stack()
