"""Step B1 of docs/superpowers/plans/2026-10-14-voxel-papers-plan.md: Dual Contouring vertex vs today's
surface-nets mean (MUD_VoxelProbe.CellPos), offline.

Field and ops are copied from MUD_VoxelProbe.c (2026-10-14, v040): samples on a world grid every CELL = 0.5 m,
solid = v > 0, seed v = Nudge(top - y), ops write min() as Carve / CarveRut / CutTrench do (same row ranges).
Meshing is Polygonize + Quad (one quad per sign-changing sample edge, flipped to face the air, split on the
shorter diagonal). Only the cell vertex changes between methods:

    mean      today's CellPos: mean of the edge crossings
    dc_tri    QEF, normal = gradient of the trilinear field inside the cell (plan B1)
    dc_cd     QEF, normal = central-difference gradients at the edge's two samples, lerped
    dc_exact  QEF, normal = gradient of the TRUE (continuous) op field at the crossing. Not possible in game
              (the game only has samples) - the upper bound: if this does not help, no normal source will.

QEF: planes (n_i, p_i), solved around the mass point c with a 3x3 Jacobi eigen solve (as Enforce would),
singular values under DC_SVD_EPS x the largest dropped; answer outside the cell box + DC_CLAMP -> mass point.

Measured against the TRUE surface: the same op formulas evaluated continuously (not sampled), as a fine
heightfield (all shapes here are heightfields; vertical steps get wall points).

    python Tools/dc_vertex_proto.py [--png DIR] [--md FILE]
"""
import argparse
import math
import os
import time

import numpy as np

CELL = float(os.environ.get("DC_CELL", "0.5"))     # Step E: 0.25 = finer rut cells
FLOOR = 3.0
FUNNEL = 0.5
RUT_R = 0.5
RUT_ON = 0.6
LAYER = 0.25
SLOPE_RUN = 0.58
DC_SVD_EPS = 0.1
DC_CLAMP = 0.1          # share of CELL the answer may leave the cell box by

NX = int(round(12 / CELL)) + 1     # samples along x / z (12 m)
NZ = NX
J0 = int(round(-4 / CELL))         # sample rows gj = J0 .. J0 + NY - 1 (y = gj * CELL, -4 .. 3 m)
NY = int(round(7 / CELL)) + 1
FINE = 0.025            # true heightfield step
IN0, IN1 = 1.5, 10.5    # measured region (x and z), away from the grid border


def nudge(d):
    if -0.005 < d < 0.005:
        return 0.005 if d >= 0 else -0.005
    return d


def top_of(x, z):
    """Undug ground: gentle slope + a soft wave (heights not on the sample planes)."""
    return 0.37 + 0.04 * x + 0.02 * z + 0.06 * math.sin(0.7 * x) * math.cos(0.5 * z)


# ---------------------------------------------------------------- ops: sampled (game) and true (continuous)

class Crater:
    def __init__(self, cx, cz, depth):
        self.cx, self.cz, self.depth = cx, cz, depth
        self.wy = top_of(cx, cz)
        self.crest = max(0.75, depth * 2.2)
        self.rimH = depth * 0.2
        self.rimW = max(0.5, depth)
        self.inner = self.crest * 0.8
        self.outer = self.crest + self.rimW
        self.rad = min(self.crest, depth)

    def value(self, x, y, z, old, undug):
        """Carve's d for one sample (old = value before, continuous in x, y, z)."""
        dx = x - self.cx
        dz = z - self.cz
        rr = math.sqrt(dx * dx + dz * dz)
        bump = 0.0
        if self.inner < rr < self.outer:
            bump = self.rimH * math.sin(math.pi * (rr - self.inner) / (self.outer - self.inner))
        d = old + bump
        up = y - self.wy
        if up <= 0:
            ex, ey, ez = dx / self.crest, up / self.depth, dz / self.crest
            s = (math.sqrt(ex * ex + ey * ey + ez * ez) - 1) * self.rad
        else:
            s = (rr - self.crest - FUNNEL * up) * self.rad / self.crest
        d = min(d, s)
        d = max(d, (undug - FLOOR) - y)
        return d

    def apply(self, G):
        gi0 = math.floor((self.cx - self.outer) / CELL)
        gi1 = math.ceil((self.cx + self.outer) / CELL)
        gk0 = math.floor((self.cz - self.outer) / CELL)
        gk1 = math.ceil((self.cz + self.outer) / CELL)
        gj0 = math.floor((self.wy - self.depth - 1) / CELL)
        gj1 = math.ceil((self.wy + FLOOR + self.rimH + 1.5) / CELL)
        for gk in range(max(gk0, 0), min(gk1, NZ - 1) + 1):
            for gi in range(max(gi0, 0), min(gi1, NX - 1) + 1):
                x, z = gi * CELL, gk * CELL
                undug = top_of(x, z)
                for gj in range(max(gj0, J0), min(gj1, J0 + NY - 1) + 1):
                    y = gj * CELL
                    G[gi, gj - J0, gk] = nudge(self.value(x, y, z, G[gi, gj - J0, gk], undug))


class Rut:
    """A wheel track (polyline), cut in passes of step to cap as CarveRut (feel field)."""

    def __init__(self, pts, step=0.08, cap=0.33, passes=6):
        self.pts = pts
        self.step, self.cap, self.passes = step, cap, passes

    def segs(self):
        return list(zip(self.pts[:-1], self.pts[1:]))

    def fall_at(self, px, pz):
        """CarveRut's strongest profile for a column at px, pz (only the segment it projects into)."""
        best = -1.0
        for (ax, az), (bx, bz) in self.segs():
            abx, abz = bx - ax, bz - az
            l2 = abx * abx + abz * abz
            if l2 < 1e-6:
                continue
            u = ((px - ax) * abx + (pz - az) * abz) / l2
            if u < 0 or u >= 1:
                continue
            qx, qz = ax + abx * u - px, az + abz * u - pz
            dist = math.sqrt(qx * qx + qz * qz)
            if dist >= RUT_R:
                continue
            k = dist / RUT_R
            best = max(best, 1 - k * k * 0.5)
        return best

    def apply(self, G):
        for _ in range(self.passes):
            for gk in range(NZ):
                for gi in range(NX):
                    x, z = gi * CELL, gk * CELL
                    cf = self.fall_at(x, z)
                    if cf < 0:
                        continue
                    undug = top_of(x, z)
                    wheel = undug          # wheel rides the undug ground (first pass); RUT_ON allows the dip
                    surf = surface_y(G, gi, gk, wheel)
                    if surf is None or abs(surf - wheel) > RUT_ON:
                        continue
                    floor_y = max(surf - self.step * cf, undug - self.cap * cf, undug - FLOOR)
                    if floor_y >= surf:
                        continue
                    for gj in range(math.floor(floor_y / CELL) - 1, math.ceil(surf / CELL) + 2):
                        if not J0 <= gj < J0 + NY:
                            continue
                        cut = floor_y - gj * CELL
                        if cut < G[gi, gj - J0, gk]:
                            G[gi, gj - J0, gk] = nudge(cut)

    def true_floor(self, x, z):
        cf = self.fall_at(x, z)
        if cf < 0:
            return None
        return top_of(x, z) - self.cap * cf


class Trench:
    """One piece: x0..x1 by z0..z1 rectangle, layers x LAYER deep, sloped wall outside (CutTrench)."""

    def __init__(self, x0, x1, z0, z1, layers=4):
        self.x0, self.x1, self.z0, self.z1 = x0, x1, z0, z1
        self.depth = layers * LAYER
        self.run = self.depth * SLOPE_RUN

    def want(self, x, z):
        if self.x0 <= x <= self.x1 and self.z0 <= z <= self.z1:
            return self.depth
        dx = max(self.x0 - x, 0, x - self.x1)
        dz = max(self.z0 - z, 0, z - self.z1)
        od = math.sqrt(dx * dx + dz * dz)
        if od >= self.run:
            return -1
        return self.depth - od / SLOPE_RUN

    def apply(self, G):
        for gk in range(NZ):
            for gi in range(NX):
                x, z = gi * CELL, gk * CELL
                w = self.want(x, z)
                if w < 0:
                    continue
                g = top_of(x, z)
                bottom = max(g - w, g - FLOOR)
                for gj in range(math.floor(bottom / CELL) - 1, math.ceil((g + 2) / CELL) + 1):
                    if not J0 <= gj < J0 + NY:
                        continue
                    s = bottom - gj * CELL
                    if s < G[gi, gj - J0, gk]:
                        G[gi, gj - J0, gk] = nudge(s)

    def true_floor(self, x, z):
        w = self.want(x, z)
        if w < 0:
            return None
        return top_of(x, z) - w


def surface_y(G, gi, gk, near):
    top = math.ceil((near + RUT_ON + CELL) / CELL)
    bot = math.floor((near - RUT_ON - CELL) / CELL)
    above = G[gi, top - J0, gk]
    for gj in range(top - 1, bot - 1, -1):
        v = G[gi, gj - J0, gk]
        if v > 0 and above <= 0:
            return gj * CELL + v / (v - above) * CELL
        above = v
    return None


# ---------------------------------------------------------------- shapes

def seed():
    G = np.zeros((NX, NY, NZ))
    for i in range(NX):
        for k in range(NZ):
            t = top_of(i * CELL, k * CELL)
            for j in range(NY):
                G[i, j, k] = nudge(t - (J0 + j) * CELL)
    return G


def arc(cx, cz, r, a0, a1, n=24):
    return [(cx + r * math.cos(a0 + (a1 - a0) * t / n), cz + r * math.sin(a0 + (a1 - a0) * t / n)) for t in range(n + 1)]


def line(x0, z0, x1, z1, step=0.5):
    n = max(1, int(math.hypot(x1 - x0, z1 - z0) / step))
    return [(x0 + (x1 - x0) * t / n, z0 + (z1 - z0) * t / n) for t in range(n + 1)]


def shapes():
    a = math.radians(27)
    return {
        "crater_big": [Crater(6.1, 5.9, 1.43)],
        "crater_small": [Crater(6.1, 5.9, 0.5)],
        "rut_straight": [Rut(line(0.2, 6.1, 11.8, 6.1)), Rut(line(0.2, 4.2, 11.8, 4.2))],
        "rut_diag": [Rut(line(6 - 6 * math.cos(a), 6 - 6 * math.sin(a), 6 + 6 * math.cos(a), 6 + 6 * math.sin(a)))],
        "rut_arc": [Rut(arc(0.5, 0.5, 8.0, 0.05, 1.52))],
        "trench": [Trench(5.5, 6.5, 3.0, 9.0, layers=4)],
    }


def true_height(ops, x, z):
    """Continuous op formulas: the surface y over x, z (all shapes are heightfields)."""
    h = top_of(x, z)
    for op in ops:
        if isinstance(op, Crater):
            # Carve is a field min; find its zero along y by bisection on the continuous value
            undug = top_of(x, z)
            lo, hi = undug - FLOOR - 0.01, undug + 2.0

            def f(y):
                return op.value(x, y, z, h - y, undug)
            for _ in range(50):
                mid = (lo + hi) * 0.5
                if f(mid) > 0:
                    lo = mid
                else:
                    hi = mid
            h = (lo + hi) * 0.5
        else:
            fl = op.true_floor(x, z)
            if fl is not None:
                h = min(h, fl)
    return h


def true_field(ops, x, y, z):
    return true_height(ops, x, z) - y


# ---------------------------------------------------------------- vertex methods

EA = [0, 2, 4, 6, 0, 1, 4, 5, 0, 1, 2, 3]
EB = [1, 3, 5, 7, 2, 3, 6, 7, 4, 5, 6, 7]
OFF = [(q & 1, (q >> 1) & 1, (q >> 2) & 1) for q in range(8)]


def corners(G, i, j, k):
    return [G[i + o[0], j + o[1], k + o[2]] for o in OFF]


def crossings(G, i, j, k):
    c = corners(G, i, j, k)
    bp = (i * CELL, (J0 + j) * CELL, k * CELL)
    out = []
    for e in range(12):
        qa, qb = EA[e], EB[e]
        da, db = c[qa], c[qb]
        if (da > 0) == (db > 0):
            continue
        t = da / (da - db)
        oa, ob = OFF[qa], OFF[qb]
        p = tuple(bp[a] + (oa[a] + (ob[a] - oa[a]) * t) * CELL for a in range(3))
        out.append((p, qa, qb, t))
    return c, bp, out


def trilinear_grad(c, fx, fy, fz):
    """Gradient of the trilinear interpolant of corner values c at local (0..1) fx, fy, fz, per metre."""
    def L(a, b, t):
        return a + (b - a) * t
    gx = L(L(c[1] - c[0], c[3] - c[2], fy), L(c[5] - c[4], c[7] - c[6], fy), fz)
    gy = L(L(c[2] - c[0], c[3] - c[1], fx), L(c[6] - c[4], c[7] - c[5], fx), fz)
    gz = L(L(c[4] - c[0], c[5] - c[1], fx), L(c[6] - c[2], c[7] - c[3], fx), fy)
    return (gx / CELL, gy / CELL, gz / CELL)


def cd_grad(G, i, j, k):
    """Central differences at sample i, j, k (one-sided at the grid border)."""
    out = []
    for a, n in ((0, NX), (1, NY), (2, NZ)):
        lo = [i, j, k]
        hi = [i, j, k]
        lo[a] = max(lo[a] - 1, 0)
        hi[a] = min(hi[a] + 1, n - 1)
        out.append((G[tuple(hi)] - G[tuple(lo)]) / ((hi[a] - lo[a]) * CELL))
    return tuple(out)


def norm(v):
    l = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    if l < 1e-9:
        return None
    return (v[0] / l, v[1] / l, v[2] / l)


def jacobi3(A):
    """Symmetric 3x3 eigen decomposition, Jacobi sweeps (loop for loop what Enforce would run)."""
    a = [row[:] for row in A]
    v = [[1.0, 0, 0], [0, 1.0, 0], [0, 0, 1.0]]
    for _ in range(8):
        off = a[0][1] ** 2 + a[0][2] ** 2 + a[1][2] ** 2
        if off < 1e-18:
            break
        for p, q in ((0, 1), (0, 2), (1, 2)):
            if abs(a[p][q]) < 1e-15:
                continue
            th = (a[q][q] - a[p][p]) / (2 * a[p][q])
            t = (1 if th >= 0 else -1) / (abs(th) + math.sqrt(th * th + 1))
            cs = 1 / math.sqrt(t * t + 1)
            sn = t * cs
            for r in range(3):
                arp, arq = a[r][p], a[r][q]
                a[r][p] = cs * arp - sn * arq
                a[r][q] = sn * arp + cs * arq
            for r in range(3):
                apr, aqr = a[p][r], a[q][r]
                a[p][r] = cs * apr - sn * aqr
                a[q][r] = sn * apr + cs * aqr
            for r in range(3):
                vrp, vrq = v[r][p], v[r][q]
                v[r][p] = cs * vrp - sn * vrq
                v[r][q] = sn * vrp + cs * vrq
    return [a[0][0], a[1][1], a[2][2]], v


def qef(planes, c):
    """min sum (n.(x - p))^2 around mass point c; small singular values dropped."""
    ata = [[0.0] * 3 for _ in range(3)]
    atb = [0.0] * 3
    for n, p in planes:
        r = n[0] * (p[0] - c[0]) + n[1] * (p[1] - c[1]) + n[2] * (p[2] - c[2])
        for a in range(3):
            atb[a] += n[a] * r
            for b in range(3):
                ata[a][b] += n[a] * n[b]
    lam, V = jacobi3(ata)
    smax = math.sqrt(max(max(lam), 0))
    x = list(c)
    for e in range(3):
        if lam[e] <= 0 or math.sqrt(lam[e]) < DC_SVD_EPS * smax:
            continue
        proj = V[0][e] * atb[0] + V[1][e] * atb[1] + V[2][e] * atb[2]
        for a in range(3):
            x[a] += V[a][e] * proj / lam[e]
    return tuple(x)


class Vertexer:
    def __init__(self, G, method, ops):
        self.G, self.method, self.ops = G, method, ops
        self.cache = {}
        self.clamped = 0
        self.solved = 0

    def pos(self, i, j, k):
        key = (i, j, k)
        if key in self.cache:
            return self.cache[key]
        c, bp, xs = crossings(self.G, i, j, k)
        if not xs:
            p = tuple(bp[a] + CELL * 0.5 for a in range(3))
        else:
            m = tuple(sum(x[0][a] for x in xs) / len(xs) for a in range(3))
            p = m
            if self.method != "mean":
                planes = []
                for (pt, qa, qb, t) in xs:
                    if self.method == "dc_tri":
                        n = trilinear_grad(c, *((pt[a] - bp[a]) / CELL for a in range(3)))
                    elif self.method == "dc_cd":
                        oa, ob = OFF[qa], OFF[qb]
                        ga = cd_grad(self.G, i + oa[0], j + oa[1], k + oa[2])
                        gb = cd_grad(self.G, i + ob[0], j + ob[1], k + ob[2])
                        n = tuple(ga[a] + (gb[a] - ga[a]) * t for a in range(3))
                    else:
                        h = 1e-3
                        n = tuple((true_field(self.ops, *[pt[b] + (h if b == a else 0) for b in range(3)])
                                   - true_field(self.ops, *[pt[b] - (h if b == a else 0) for b in range(3)])) / (2 * h)
                                  for a in range(3))
                    n = norm(n)
                    if n:
                        planes.append((n, pt))
                if planes:
                    self.solved += 1
                    x = qef(planes, m)
                    lo = [bp[a] - DC_CLAMP * CELL for a in range(3)]
                    hi = [bp[a] + CELL * (1 + DC_CLAMP) for a in range(3)]
                    if all(lo[a] <= x[a] <= hi[a] for a in range(3)):
                        p = x
                    else:
                        self.clamped += 1
        self.cache[key] = p
        return p


# ---------------------------------------------------------------- polygonize (Polygonize + Quad)

def tri_quality(t):
    """(area m2, smallest corner angle deg)"""
    n = cross(sub(t[1], t[0]), sub(t[2], t[0]))
    area = 0.5 * math.sqrt(dot(n, n))
    angs = []
    for a in range(3):
        u = sub(t[(a + 1) % 3], t[a])
        w = sub(t[(a + 2) % 3], t[a])
        lu, lw = math.sqrt(dot(u, u)), math.sqrt(dot(w, w))
        if lu < 1e-9 or lw < 1e-9:
            return area, 0.0
        angs.append(math.degrees(math.acos(max(-1, min(1, dot(u, w) / (lu * lw))))))
    return area, min(angs)


AREA_EPS = 1e-4         # m2 (1 cm2): 'near zero' for the collider filter (IDA 2026-10-14: CreateTriMesh keeps them)
SLIVER_DEG = 10         # smallest corner angle under this = sliver (pulls engine normals with full weight)


def polygonize(G, vx):
    tris = []
    folded = 0
    for i in range(1, NX - 1):
        for j in range(1, NY - 1):
            for k in range(1, NZ - 1):
                v = G[i, j, k]
                solid = v > 0
                for axis, (di, dj, dk) in enumerate(((1, 0, 0), (0, 1, 0), (0, 0, 1))):
                    w = G[i + di, j + dj, k + dk]
                    if solid == (w > 0):
                        continue
                    # the 4 cells round the edge (cell index = its min sample)
                    if axis == 0:
                        cells = [(i, j - 1, k - 1), (i, j, k - 1), (i, j, k), (i, j - 1, k)]
                    elif axis == 1:
                        cells = [(i - 1, j, k - 1), (i, j, k - 1), (i, j, k), (i - 1, j, k)]
                    else:
                        cells = [(i - 1, j - 1, k), (i, j - 1, k), (i, j, k), (i - 1, j, k)]
                    if any(not (0 <= c[0] < NX - 1 and 0 <= c[1] < NY - 1 and 0 <= c[2] < NZ - 1) for c in cells):
                        continue
                    p = [vx.pos(*c) for c in cells]
                    air = [0.0, 0.0, 0.0]
                    air[axis] = 1.0 if solid else -1.0
                    n = cross(sub(p[2], p[0]), sub(p[3], p[1]))
                    if dot(n, air) < 0:
                        p = [p[0], p[3], p[2], p[1]]
                    if dist2(p[0], p[2]) <= dist2(p[1], p[3]):
                        quad = [(p[0], p[1], p[2]), (p[0], p[2], p[3])]
                    else:
                        quad = [(p[0], p[1], p[3]), (p[1], p[2], p[3])]
                    for t in quad:
                        tn = cross(sub(t[1], t[0]), sub(t[2], t[0]))
                        if dot(tn, air) < 0:
                            folded += 1
                        tris.append(t)
    return tris, folded


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def dist2(a, b):
    d = sub(a, b)
    return dot(d, d)


# ---------------------------------------------------------------- measuring

def true_cloud(ops):
    xs = np.arange(IN0, IN1 + 1e-9, FINE)
    H = np.array([[true_height(ops, x, z) for z in xs] for x in xs])
    pts = []
    for a, x in enumerate(xs):
        for b, z in enumerate(xs):
            pts.append((x, H[a, b], z))
            # vertical steps (rut edge, trench wall at a column line): wall points
            for da, db in ((1, 0), (0, 1)):
                if a + da < len(xs) and b + db < len(xs):
                    h2 = H[a + da, b + db]
                    if abs(h2 - H[a, b]) > FINE:
                        mx, mz = x + da * FINE * 0.5, z + db * FINE * 0.5
                        lo, hi = sorted((H[a, b], h2))
                        for y in np.arange(lo, hi, FINE):
                            pts.append((mx, y, mz))
    return np.array(pts), H, xs


def mesh_samples(tris, n=6):
    pts = []
    for t in tris:
        for a in range(n + 1):
            for b in range(n + 1 - a):
                c = n - a - b
                p = tuple((t[0][q] * a + t[1][q] * b + t[2][q] * c) / n for q in range(3))
                if IN0 <= p[0] <= IN1 and IN0 <= p[2] <= IN1:
                    pts.append(p)
    return np.array(pts)


class Near:
    """Nearest-point distance through a 3D bucket hash."""

    def __init__(self, pts, b=0.1):
        self.b = b
        self.pts = pts
        keys = np.floor(pts / b).astype(np.int64)
        self.map = {}
        for idx, kk in enumerate(map(tuple, keys)):
            self.map.setdefault(kk, []).append(idx)
        self.map = {k: np.array(v) for k, v in self.map.items()}

    def cube(self, kq, r):
        idx = []
        for dx in range(-r, r + 1):
            for dy in range(-r, r + 1):
                for dz in range(-r, r + 1):
                    m = self.map.get((kq[0] + dx, kq[1] + dy, kq[2] + dz))
                    if m is not None:
                        idx.append(m)
        if not idx:
            return None
        return self.pts[np.concatenate(idx)]

    def dist(self, q):
        kq = tuple(np.floor(q / self.b).astype(np.int64))
        for r in range(1, 40):
            cand = self.cube(kq, r)
            if cand is None:
                continue
            d = np.sqrt(((cand - q) ** 2).sum(axis=1)).min()
            # every point within d lies in the cube of radius ceil(d / b) + 1: look there once more
            r2 = int(math.ceil(d / self.b)) + 1
            if r2 > r:
                cand = self.cube(kq, r2)
                d = np.sqrt(((cand - q) ** 2).sum(axis=1)).min()
            return d
        return float("inf")


def two_way(mesh_pts, tcloud, tnear, mnear, step=3):
    m2t = np.array([tnear.dist(p) for p in mesh_pts[::step]])
    t2m = np.array([mnear.dist(p) for p in tcloud[::step * 4]])
    return m2t, t2m


def steep_wall(tris, ops, band=(0.15, 0.85)):
    """Rut shapes: p90 of the slope (deg) of triangles whose middle lies across a rut wall band."""
    ang = []
    for t in tris:
        mid = tuple((t[0][q] + t[1][q] + t[2][q]) / 3 for q in range(3))
        if not (IN0 <= mid[0] <= IN1 and IN0 <= mid[2] <= IN1):
            continue
        near = False
        for op in ops:
            if isinstance(op, Rut):
                for (ax, az), (bx, bz) in op.segs():
                    abx, abz = bx - ax, bz - az
                    l2 = abx * abx + abz * abz
                    if l2 < 1e-6:
                        continue
                    u = min(max(((mid[0] - ax) * abx + (mid[2] - az) * abz) / l2, 0), 1)
                    d = math.hypot(ax + abx * u - mid[0], az + abz * u - mid[2])
                    if band[0] <= d <= band[1]:
                        near = True
        if not near:
            continue
        n = cross(sub(t[1], t[0]), sub(t[2], t[0]))
        l = math.sqrt(dot(n, n))
        if l < 1e-9:
            continue
        ang.append(math.degrees(math.acos(min(1, abs(n[1]) / l))))
    if not ang:
        return None
    return float(np.percentile(ang, 90))


def profile(tris, ops, x_cut, z0, z1, n=240):
    """Mesh height along x = x_cut (highest hit), vs the true height."""
    zs = np.linspace(z0, z1, n)
    mh = []
    for z in zs:
        best = None
        for t in tris:
            y = ray_y(t, x_cut, z)
            if y is not None and (best is None or y > best):
                best = y
        mh.append(best if best is not None else np.nan)
    th = [true_height(ops, x_cut, z) for z in zs]
    return zs, np.array(mh), np.array(th)


def ray_y(t, x, z):
    (ax, ay, az), (bx, by, bz), (cx, cy, cz) = t
    den = (bz - cz) * (ax - cx) + (cx - bx) * (az - cz)
    if abs(den) < 1e-12:
        return None
    l1 = ((bz - cz) * (x - cx) + (cx - bx) * (z - cz)) / den
    l2 = ((cz - az) * (x - cx) + (ax - cx) * (z - cz)) / den
    l3 = 1 - l1 - l2
    if min(l1, l2, l3) < -1e-9:
        return None
    return l1 * ay + l2 * by + l3 * cy


# ---------------------------------------------------------------- main

METHODS = ["mean", "dc_tri", "dc_cd", "dc_exact"]
CUTS = {"crater_big": ("x", 6.1, 1.5, 10.5), "crater_small": ("x", 6.1, 2.5, 9.5),
        "rut_straight": ("x", 6.05, 3.0, 7.5), "rut_diag": ("x", 6.05, 4.0, 8.0),
        "rut_arc": ("x", 5.0, 4.0, 8.5), "trench": ("x", 6.05, 1.5, 10.5)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png")
    ap.add_argument("--md")
    ap.add_argument("--npz", help="save the cross-section profiles (Step E: compare cell sizes)")
    ap.add_argument("--methods", default=",".join(METHODS))
    args = ap.parse_args()
    methods = args.methods.split(",")
    rows = []
    profiles = {}
    only = os.environ.get("DC_SHAPES")
    for name, ops in shapes().items():
        if only and name not in only.split(","):
            continue
        G = seed()
        for op in ops:
            op.apply(G)
        tcloud, H, xs = true_cloud(ops)
        tnear = Near(tcloud)
        for method in methods:
            vx = Vertexer(G, method, ops)
            t0 = time.perf_counter()
            # time the vertex solve alone, over every cell with a crossing
            for i in range(NX - 1):
                for j in range(NY - 1):
                    for k in range(NZ - 1):
                        vx.pos(i, j, k)
            ms = (time.perf_counter() - t0) * 1000
            cells = sum(1 for (i, j, k) in vx.cache if crossings(G, i, j, k)[2])
            t1 = time.perf_counter()
            tris, folded = polygonize(G, vx)
            poly_ms = (time.perf_counter() - t1) * 1000
            mp = mesh_samples(tris)
            mnear = Near(mp)
            m2t, t2m = two_way(mp, tcloud, tnear, mnear)
            wall = steep_wall(tris, ops) if name.startswith("rut") else None
            q = [tri_quality(t) for t in tris]
            tiny = sum(1 for a, _ in q if a < AREA_EPS)
            sliver = sum(1 for a, m in q if a >= AREA_EPS and m < SLIVER_DEG)
            rows.append(dict(shape=name, method=method, tris=len(tris), folded=folded, cells=cells,
                             solved=vx.solved, clamped=vx.clamped, ms=ms,
                             m2t_max=m2t.max(), m2t_rms=math.sqrt((m2t ** 2).mean()), m2t_p95=np.percentile(m2t, 95),
                             t2m_max=t2m.max(), t2m_rms=math.sqrt((t2m ** 2).mean()), t2m_p95=np.percentile(t2m, 95),
                             wall=wall, tiny=tiny, sliver=sliver, poly_ms=poly_ms))
            ax_, xc, z0, z1 = CUTS[name]
            profiles[(name, method)] = profile(tris, ops, xc, z0, z1)
            r = rows[-1]
            print("%-13s %-8s tiny %4d sliver %4d tris %5d folded %3d cells %5d clamped %4d | mesh->true max %.3f rms %.3f | true->mesh max %.3f rms %.3f | wall %s | %.0f ms"
                  % (name, method, r["tiny"], r["sliver"], r["tris"], r["folded"], r["cells"], r["clamped"], r["m2t_max"], r["m2t_rms"],
                     r["t2m_max"], r["t2m_rms"], "-" if wall is None else "%.0f" % wall, ms), "poly %.0f ms" % poly_ms, flush=True)
    if args.npz:
        np.savez(args.npz, **{"%s|%s|%s" % (n, m, part): arr for (n, m), prof in profiles.items()
                              for part, arr in zip(("z", "mesh", "true"), prof)})
    if args.md:
        write_md(args.md, rows)
    if args.png:
        write_png(args.png, profiles)


def write_md(path, rows):
    base = {r["shape"]: r["ms"] for r in rows if r["method"] == "mean"}
    L = ["| Shape | Method | Tris | Folded tris | Tiny tris (< 1 cm2) | Slivers (< 10 deg) | Cells w/ crossing | QEF solved | Clamped to mean | mesh->true max | mesh->true p95 | mesh->true rms | true->mesh max | true->mesh p95 | true->mesh rms | Rut wall p90 slope (deg) | Vertex ms | Cost vs mean | Polygonize ms |",
         "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in rows:
        L.append("| %s | %s | %d | %d | %d | %d | %d | %d | %d | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %s | %.0f | %.1fx | %.0f |" % (
            r["shape"], r["method"], r["tris"], r["folded"], r["tiny"], r["sliver"], r["cells"], r["solved"], r["clamped"],
            r["m2t_max"], r["m2t_p95"], r["m2t_rms"], r["t2m_max"], r["t2m_p95"], r["t2m_rms"],
            "-" if r["wall"] is None else "%.0f" % r["wall"], r["ms"], r["ms"] / base.get(r["shape"], r["ms"]), r["poly_ms"]))
    open(path, "w", encoding="utf-8").write("\n".join(L) + "\n")


def write_png(d, profiles):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    os.makedirs(d, exist_ok=True)
    names = sorted({k[0] for k in profiles})
    for name in names:
        fig, ax = plt.subplots(figsize=(9, 3.6))
        zs, _, th = profiles[(name, "mean")]
        ax.plot(zs, th, color="black", lw=2.2, label="true (op formula)")
        for m, col in zip([m for m in METHODS if (name, m) in profiles], ("#d62728", "#1f77b4", "#2ca02c", "#9467bd")):
            ax.plot(zs, profiles[(name, m)][1], color=col, lw=1.2, label=m)
        _, xc, _, _ = CUTS[name]
        ax.set_title("%s - cross-section at x = %.2f m" % (name, xc))
        ax.set_xlabel("z (m)")
        ax.set_ylabel("y (m)")
        ax.set_aspect("equal", adjustable="datalim")
        ax.legend(fontsize=8, loc="lower right")
        fig.tight_layout()
        fig.savefig(os.path.join(d, name + ".png"), dpi=110)
        plt.close(fig)


if __name__ == "__main__":
    main()
