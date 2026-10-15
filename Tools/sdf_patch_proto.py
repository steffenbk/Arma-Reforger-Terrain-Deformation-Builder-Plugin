"""Layered-SDF ground, phase 1: band surface-nets mesher for one 8 m fine patch, offline.

Written loop for loop as MUD_BandMesher in Scripts/Game/Mud/MUD_BandMesher.c (flat int / float
arrays, no numpy in the mesher), so the counts here are the counts the game will see.

Field: density(x, y, z) = Y(x, z) - y, > 0 = solid. Y = base + displayed deformation, sampled on the
patch's own 33 x 33 fine vertices (0.25 m). Cells 0.25 m in x / z, DY = 0.5 m in y on WORLD planes
y = j * DY, so two patches cut the same planes. Only the band of each column is visited.

Patch borders and kept-terrain borders: an off cell (outside the patch, kept terrain) next to an on
cell is a GHOST. Its vertex is the mean of the crossings on the faces it shares with on cells (or on
the corner vertical edge it shares diagonally), clamped onto those faces. A ghost on the patch border
reads only samples on the border line, which both patches hold as identical copies (SetH writes every
copy) - so neighbours end on the same polyline (seam gap 0) without reading each other.
Every rim edge (ghost - ghost) gets a skirt quad SKIRT m down for the skin / terrain side.

    python Tools/sdf_patch_proto.py                 # synthetic rut / crater / trench / kept / pair
    python Tools/sdf_patch_proto.py --dump LOG      # also every patch in a 'MUD dump' log
    python Tools/sdf_patch_proto.py --png DIR       # top-down renders, dual mesh vs today's grid
"""
import argparse
import math
import os
import random
import sys

FINE = 0.25
NC = 32                 # fine cells per patch side
NV = 33                 # fine verts per patch side
DY = 0.5
SKIRT = 0.3
CW = NC + 2             # cell table side: cells -1 .. 32
MAX_V = 16000
MAX_I = 48000

# ------------------------------------------------------------------------------------------------
# mesher (mirror of MUD_BandMesher.c)


class Mesh:
    def __init__(self):
        self.vx = []        # vertex positions (patch local x, world y, patch local z)
        self.vy = []
        self.vz = []
        self.vd = []        # displayed deformation interpolated at the vertex (groove reveal)
        self.ghost = []     # 1 = rim vertex (ghost cell)
        self.idx = []       # surface triangles
        self.skirt = []     # skirt triangles (indices into vx.. after the surface verts)
        self.cells = 0      # cells visited (band)
        self.active = 0     # cells with a vertex
        self.degen = 0
        self.overflow = False
        self.rim = []       # rim edges (u, w) in quad loop order


def ceil_div(y):
    return int(math.ceil(y / DY))


class BandMesher:
    """Same state as the Enforce class: flat per-cell tables, reused between builds."""

    def __init__(self):
        n = CW * CW
        self.kind = [0] * n     # 0 off, 1 on, 2 ghost
        self.fx = [0] * n       # ghost: grid x of its x face, -1 = none
        self.fz = [0] * n
        self.cx = [0] * n       # ghost: diagonal corner (grid x, z), -1 = none
        self.cz = [0] * n
        self.jlo = [0] * n
        self.jn = [0] * n       # layers with a vertex
        self.v0 = [0] * n       # first vertex index
        self.single = [-1] * n  # FAST: on cell with all 4 corners in one layer -> its vertex

    # on[c] for patch cells 0..31 x 0..31 (c = cz * 32 + cx)
    def on(self, on, x, z):
        if x < 0 or z < 0 or x >= NC or z >= NC:
            return False
        return on[z * NC + x] != 0

    def build(self, Y, D, on, skirt=SKIRT, fast=False):
        """fast=True: the FAST paths of MUD_BandMesher.c (2026-10-13 speed-up); must give the same mesh"""
        m = Mesh()
        m.fast_v = 0
        m.fast_q = 0
        for i in range(CW * CW):
            self.single[i] = -1
        # 1. cell kinds
        for z in range(-1, NC + 1):
            for x in range(-1, NC + 1):
                c = (z + 1) * CW + x + 1
                self.fx[c] = -1
                self.fz[c] = -1
                self.cx[c] = -1
                self.cz[c] = -1
                self.jn[c] = 0
                if self.on(on, x, z):
                    self.kind[c] = 1
                    continue
                k = 0
                if self.on(on, x - 1, z):
                    self.fx[c] = x
                elif self.on(on, x + 1, z):
                    self.fx[c] = x + 1
                if self.on(on, x, z - 1):
                    self.fz[c] = z
                elif self.on(on, x, z + 1):
                    self.fz[c] = z + 1
                if self.fx[c] >= 0 or self.fz[c] >= 0:
                    k = 2
                else:
                    for dz in (-1, 1):
                        for dx in (-1, 1):
                            if self.cx[c] < 0 and self.on(on, x + dx, z + dz):
                                self.cx[c] = x + max(dx, 0)
                                self.cz[c] = z + max(dz, 0)
                    if self.cx[c] >= 0:
                        k = 2
                self.kind[c] = k

        # 2. vertices, column by column, band only
        for z in range(-1, NC + 1):
            for x in range(-1, NC + 1):
                c = (z + 1) * CW + x + 1
                k = self.kind[c]
                if k == 0:
                    continue
                if fast and k == 1:
                    s00 = z * NV + x
                    js = [ceil_div(Y[s00]) - 1, ceil_div(Y[s00 + 1]) - 1, ceil_div(Y[s00 + NV]) - 1, ceil_div(Y[s00 + NV + 1]) - 1]
                    if js[0] == js[1] == js[2] == js[3]:
                        m.cells += 1
                        m.active += 1
                        self.jlo[c] = js[0]
                        self.jn[c] = 1
                        self.v0[c] = len(m.vx)
                        self.single[c] = len(m.vx)
                        m.vx.append((x + 0.5) * FINE)
                        m.vy.append((Y[s00] + Y[s00 + 1] + Y[s00 + NV] + Y[s00 + NV + 1]) * 0.25)
                        m.vz.append((z + 0.5) * FINE)
                        m.vd.append((D[s00] + D[s00 + 1] + D[s00 + NV] + D[s00 + NV + 1]) * 0.25)
                        m.ghost.append(0)
                        m.fast_v += 1
                        continue
                lo, hi = self.col_range(Y, c, x, z)
                j0 = ceil_div(lo) - 1
                j1 = ceil_div(hi) - 1
                self.jlo[c] = j0
                self.v0[c] = len(m.vx)
                for j in range(j0, j1 + 1):
                    m.cells += 1
                    if len(m.vx) >= MAX_V:
                        m.overflow = True
                        return m
                    ok = self.vertex(m, Y, D, c, x, z, j)
                    if not ok:
                        break
                    self.jn[c] += 1
                    m.active += 1

        # 3. quads: y edges at every grid point, x / z edges on every plane they cross
        for gz in range(NV):
            for gx in range(NV):
                y = Y[gz * NV + gx]
                j = ceil_div(y) - 1
                # below plane j+1 solid side is down: air +y
                ca = gz * CW + gx
                qa, qb, qc, qd = self.single[ca], self.single[ca + 1], self.single[ca + CW + 1], self.single[ca + CW]
                if fast and qa >= 0 and qb >= 0 and qc >= 0 and qd >= 0:
                    dac = m.vy[qa] - m.vy[qc]
                    dbd = m.vy[qb] - m.vy[qd]
                    if dac * dac <= dbd * dbd:
                        m.idx += [qa, qd, qc, qa, qc, qb]
                    else:
                        m.idx += [qa, qd, qb, qd, qc, qb]
                    m.fast_q += 1
                else:
                    self.quad(m, gx - 1, gz - 1, j, gx, gz - 1, j, gx, gz, j, gx - 1, gz, j, (0, 1, 0))
                if gx < NC:
                    ya = y
                    yb = Y[gz * NV + gx + 1]
                    lo = min(ya, yb)
                    hi = max(ya, yb)
                    air = (1, 0, 0) if ya > yb else (-1, 0, 0)
                    for jp in range(ceil_div(lo), ceil_div(hi)):
                        # plane jp: cells layers jp-1, jp in columns (gx, gz-1), (gx, gz)
                        self.quad(m, gx, gz - 1, jp - 1, gx, gz, jp - 1, gx, gz, jp, gx, gz - 1, jp, air)
                if gz < NC:
                    ya = y
                    yb = Y[(gz + 1) * NV + gx]
                    lo = min(ya, yb)
                    hi = max(ya, yb)
                    air = (0, 0, 1) if ya > yb else (0, 0, -1)
                    for jp in range(ceil_div(lo), ceil_div(hi)):
                        self.quad(m, gx - 1, gz, jp - 1, gx, gz, jp - 1, gx, gz, jp, gx - 1, gz, jp, air)
        if skirt > 0:
            self.skirts(m, skirt)
        return m

    def col_range(self, Y, c, x, z):
        """y range of the samples the cell's vertex reads: on = its 4 corners, ghost = its faces / corner"""
        k = self.kind[c]
        pts = []
        if k == 1:
            pts = [(x, z), (x + 1, z), (x, z + 1), (x + 1, z + 1)]
        else:
            if self.fx[c] >= 0:
                pts += [(self.fx[c], z), (self.fx[c], z + 1)]
            if self.fz[c] >= 0:
                pts += [(x, self.fz[c]), (x + 1, self.fz[c])]
            if self.cx[c] >= 0:
                pts += [(self.cx[c], self.cz[c])]
        lo = 1e9
        hi = -1e9
        for (px, pz) in pts:
            px = min(max(px, 0), NC)
            pz = min(max(pz, 0), NC)
            yy = Y[pz * NV + px]
            lo = min(lo, yy)
            hi = max(hi, yy)
        return lo, hi

    def vertex(self, m, Y, D, c, x, z, j):
        """mean of the crossings on the cell's edges (ghost: its shared faces / corner), layer j"""
        sx = sy = sz = sd = 0.0
        n = 0
        y0 = j * DY
        y1 = y0 + DY
        k = self.kind[c]

        def vert(px, pz):
            nonlocal sx, sy, sz, sd, n
            yy = Y[pz * NV + px]
            if y0 < yy <= y1:
                sx += px
                sy += yy
                sz += pz
                sd += D[pz * NV + px]
                n += 1

        def hor(ax, az, bx, bz):
            nonlocal sx, sy, sz, sd, n
            ya = Y[az * NV + ax]
            yb = Y[bz * NV + bx]
            for p in (y0, y1):
                if (ya > p) != (yb > p):
                    t = (ya - p) / (ya - yb)
                    sx += ax + (bx - ax) * t
                    sy += p
                    sz += az + (bz - az) * t
                    da = D[az * NV + ax]
                    sd += da + (D[bz * NV + bx] - da) * t
                    n += 1

        if k == 1:
            vert(x, z)
            vert(x + 1, z)
            vert(x, z + 1)
            vert(x + 1, z + 1)
            hor(x, z, x + 1, z)
            hor(x, z + 1, x + 1, z + 1)
            hor(x, z, x, z + 1)
            hor(x + 1, z, x + 1, z + 1)
        else:
            f = self.fx[c]
            g = self.fz[c]
            if f >= 0:
                vert(f, z)
                vert(f, z + 1)
                hor(f, z, f, z + 1)
            if g >= 0:
                # the corner vertical edge (f, g) lies on both faces: count it once
                if x != f:
                    vert(x, g)
                if x + 1 != f:
                    vert(x + 1, g)
                hor(x, g, x + 1, g)
            if f < 0 and g < 0:
                vert(self.cx[c], self.cz[c])
        if n == 0:
            return False    # band rows past the last crossing (cannot happen for a contiguous band)
        px = sx / n
        py = sy / n
        pz = sz / n
        if k == 2:
            if self.fx[c] >= 0:
                px = self.fx[c]
            if self.fz[c] >= 0:
                pz = self.fz[c]
            if self.fx[c] < 0 and self.fz[c] < 0:
                px = self.cx[c]
                pz = self.cz[c]
        m.vx.append(px * FINE)
        m.vy.append(py)
        m.vz.append(pz * FINE)
        m.vd.append(sd / n)
        m.ghost.append(1 if k == 2 else 0)
        return True

    def vid(self, x, z, j):
        if x < -1 or z < -1 or x > NC or z > NC:
            return -1
        c = (z + 1) * CW + x + 1
        if self.kind[c] == 0:
            return -1
        r = j - self.jlo[c]
        if r < 0 or r >= self.jn[c]:
            return -2
        return self.v0[c] + r

    def quad(self, m, x0, z0, j0, x1, z1, j1, x2, z2, j2, x3, z3, j3, air):
        # at least one on cell
        if not (self.is_on(x0, z0) or self.is_on(x1, z1) or self.is_on(x2, z2) or self.is_on(x3, z3)):
            return
        a = self.vid(x0, z0, j0)
        b = self.vid(x1, z1, j1)
        c = self.vid(x2, z2, j2)
        d = self.vid(x3, z3, j3)
        if a < 0 or b < 0 or c < 0 or d < 0:
            m.missing = getattr(m, "missing", 0) + 1
            return
        pa = (m.vx[a], m.vy[a], m.vz[a])
        pb = (m.vx[b], m.vy[b], m.vz[b])
        pc = (m.vx[c], m.vy[c], m.vz[c])
        pd = (m.vx[d], m.vy[d], m.vz[d])
        n = cross(sub(pc, pa), sub(pd, pb))
        if dot(n, air) < 0:
            b, d = d, b
            pb, pd = pd, pb
        if dist2(pa, pc) <= dist2(pb, pd):
            self.tri(m, a, b, c)
            self.tri(m, a, c, d)
        else:
            self.tri(m, a, b, d)
            self.tri(m, b, c, d)
        # rim: a loop edge between two ghost verts (never a diagonal) - each is in exactly one quad
        loop = (a, b, c, d)
        for e in range(4):
            u = loop[e]
            w = loop[(e + 1) % 4]
            if m.ghost[u] and m.ghost[w]:
                m.rim += [u, w]

    def is_on(self, x, z):
        if x < -1 or z < -1 or x > NC or z > NC:
            return False
        return self.kind[(z + 1) * CW + x + 1] == 1

    def tri(self, m, a, b, c):
        if len(m.idx) + 3 > MAX_I:
            m.overflow = True
            return
        if a == b or b == c or a == c:
            m.degen += 1
            return
        pa = (m.vx[a], m.vy[a], m.vz[a])
        n = cross(sub((m.vx[b], m.vy[b], m.vz[b]), pa), sub((m.vx[c], m.vy[c], m.vz[c]), pa))
        if dot(n, n) < 1e-12:
            m.degen += 1
            return
        m.idx += [a, b, c]

    def skirts(self, m, depth):
        """every rim edge (quad loop edge between two ghost verts): a quad hanging depth down, own verts
        (top copy + bottom copy per rim vertex, so the surface normals stay as they are), wound like the
        edge reversed so it faces out (checked: 144 / 144 out on the kept-cell crater case)"""
        nv = len(m.vx)
        top = {}

        def copy(v):
            if v in top:
                return top[v]
            i = nv + len(top) * 2
            top[v] = i
            return i

        sk = []
        for r in range(0, len(m.rim), 2):
            a = m.rim[r]
            b = m.rim[r + 1]
            if m.vy[a] == m.vy[b] and m.vx[a] == m.vx[b] and m.vz[a] == m.vz[b]:
                continue
            ta = copy(a)
            tb = copy(b)
            sk += [tb, ta, ta + 1, tb, ta + 1, tb + 1]
        m.skirt_top = top
        m.skirt = sk
        for v, i in sorted(top.items(), key=lambda kv: kv[1]):
            m.vx += [m.vx[v], m.vx[v]]
            m.vy += [m.vy[v], m.vy[v] - depth]
            m.vz += [m.vz[v], m.vz[v]]
            m.vd += [m.vd[v], m.vd[v]]
            m.ghost += [0, 0]


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def dist2(a, b):
    d = sub(a, b)
    return dot(d, d)


# ------------------------------------------------------------------------------------------------
# synthetic ground (MUD_WorldGround.c profiles, defaults of the attributes)

def skin_base(gx, gz, seed=1, slope=(0.04, 0.02)):
    """skin-like base: a plane plus a bump field sampled on 2 m cells, triangles inside each cell (knees
    every 8 fine verts, as the real skin)"""
    rnd = random.Random(seed)
    cache = {}

    def corner(cx, cz):
        k = (cx, cz)
        if k not in cache:
            r = random.Random(seed * 1000003 + cx * 7919 + cz * 104729)
            cache[k] = 40 + cx * 2 * slope[0] + cz * 2 * slope[1] + r.uniform(-0.12, 0.12)
        return cache[k]
    cx = gx // 8
    cz = gz // 8
    u = (gx - cx * 8) / 8.0
    w = (gz - cz * 8) / 8.0
    h00 = corner(cx, cz)
    h10 = corner(cx + 1, cz)
    h01 = corner(cx, cz + 1)
    h11 = corner(cx + 1, cz + 1)
    # one diagonal (bl-tr), as most skin cells
    if u >= w:
        return h00 + (h10 - h00) * u + (h11 - h10) * w
    return h00 + (h11 - h01) * u + (h01 - h00) * w


def field(nx, nz):
    return [[0.0] * nx for _ in range(nz)]


def rut_line(H, x0, z0, x1, z1, depth=0.24, passes=3, rr=0.5, br=0.9, bh=0.08, step=0.08):
    """passes of Press along a line, contact every 0.25 m"""
    nz = len(H)
    nx = len(H[0])
    L = math.hypot(x1 - x0, z1 - z0)
    for _ in range(passes):
        frm = [row[:] for row in H]
        touched = set()
        for s in range(int(L / 0.25) + 1):
            px = x0 + (x1 - x0) * s * 0.25 / L
            pz = z0 + (z1 - z0) * s * 0.25 / L
            fx = px / FINE
            fz = pz / FINE
            r = br / FINE
            for gz in range(max(0, int(fz - r)), min(nz - 1, int(fz + r) + 1) + 1):
                for gx in range(max(0, int(fx - r)), min(nx - 1, int(fx + r) + 1) + 1):
                    d = math.hypot((gx - fx) * FINE, (gz - fz) * FINE)
                    h = H[gz][gx]
                    if d < rr:
                        f0 = frm[gz][gx]
                        k = d / rr
                        fall = 1 - k * k * 0.5
                        top = f0 - f0 * 0.8 * fall if f0 > 0 else f0
                        y = max(top - step * fall, -depth)
                        if y < h:
                            H[gz][gx] = y
                    elif d < br:
                        bf = frm[gz][gx]
                        if h < -0.001 or bf < -0.001 or bf >= bh:
                            continue
                        t = 1 - (d - rr) / (br - rr)
                        y2 = min(bf + step * 0.5 * t, bh)
                        if y2 > h:
                            H[gz][gx] = y2


def crater(H, cxm, czm, depth=1.43, slope=0.9, rim_share=0.17, rim_w=0.4):
    nz = len(H)
    nx = len(H[0])
    rim = depth * rim_share
    rimw = max(2 * FINE, depth * rim_w)
    crest = max(3 * FINE, (depth + rim) * math.pi / (2 * slope))
    outer = crest + max(rimw, rim * math.pi / (2 * slope))
    if outer > 3.95:
        k = 3.95 / outer
        depth *= k
        rim *= k
        crest *= k
        outer = 3.95
    for gz in range(nz):
        for gx in range(nx):
            d = math.hypot(gx * FINE - cxm, gz * FINE - czm)
            if d >= outer:
                continue
            if d < crest:
                y = -depth + (depth + rim) * (1 - math.cos(math.pi * d / crest)) * 0.5
            else:
                y = rim * (1 + math.cos(math.pi * (d - crest) / (outer - crest))) * 0.5
            h = H[gz][gx]
            if y < 0:
                if y < h:
                    H[gz][gx] = y
            elif h >= 0 and y > h:
                H[gz][gx] = y


def trench(H, x0, z0, x1, z1, depth=1.6, wall=0.25):
    """box cut, walls one fine step wide (MUD trench walls are near vertical)"""
    nz = len(H)
    nx = len(H[0])
    for gz in range(nz):
        for gx in range(nx):
            x = gx * FINE
            z = gz * FINE
            dx = max(x0 - x, 0, x - x1)
            dz = max(z0 - z, 0, z - z1)
            d = max(dx, dz)
            if d <= 0:
                y = -depth
            elif d < wall:
                y = -depth * (1 - d / wall)
            else:
                continue
            H[gz][gx] = min(H[gz][gx], y)


def patch_of(Hw, ox, oz, kept=None, seed=1):
    """patch at fine-vert offset (ox, oz) of a world field: Y, D, on"""
    Y = [0.0] * (NV * NV)
    D = [0.0] * (NV * NV)
    on = [1] * (NC * NC)
    for z in range(NV):
        for x in range(NV):
            d = Hw[oz + z][ox + x]
            D[z * NV + x] = d
            Y[z * NV + x] = skin_base(ox + x, oz + z, seed) + d
    if kept:
        for (sx, sz) in kept:        # skin cells (0..3) kept terrain
            for z in range(sz * 8, sz * 8 + 8):
                for x in range(sx * 8, sx * 8 + 8):
                    on[z * NC + x] = 0
    return Y, D, on


# ------------------------------------------------------------------------------------------------
# today's grid mesh (BuildMesh): 33 x 33 verts, 2 tris per fine cell, kept cells collapsed

def grid_mesh(Y, on):
    tris = []
    for z in range(NC):
        for x in range(NC):
            if not on[z * NC + x]:
                continue
            bl = z * NV + x
            br = bl + 1
            tl = bl + NV
            tr = tl + 1
            tris += [bl, tl, tr, tr, br, bl]
    vx = [(v % NV) * FINE for v in range(NV * NV)]
    vz = [(v // NV) * FINE for v in range(NV * NV)]
    return vx, list(Y), vz, tris


# ------------------------------------------------------------------------------------------------
# checks

def seam_gap(ma, mb, axis, line_a, line_b):
    """rim verts of A on its border line vs rim verts of B on its border line (other patch local)"""
    def rim(m, line):
        pts = set()
        for i in range(len(m.vx) - 2 * len(getattr(m, 'skirt_top', {}))):
            if not m.ghost[i]:
                continue
            c = m.vx[i] if axis == 0 else m.vz[i]
            if abs(c - line) < 1e-9:
                o = m.vz[i] if axis == 0 else m.vx[i]
                pts.add((round(o, 9), round(m.vy[i], 9)))
        return pts
    a = rim(ma, line_a)
    b = rim(mb, line_b)
    worst = 0.0
    only = len(a ^ b)
    # height gap along the line: compare polylines at every vertex of either
    def poly(pts):
        return sorted(pts)
    pa = poly(a)
    pb = poly(b)
    return len(a), len(b), only, pa == pb


def open_edges_on_line(ma, mb, axis, off_b):
    """weld A and B (B shifted by off_b along axis) by position; count edges used once that lie ON the
    shared line (cracks). Skirt tris excluded."""
    from collections import Counter
    pos = {}
    edges = Counter()

    def key(x, y, z):
        return (round(x, 7), round(y, 7), round(z, 7))

    for m, sh in ((ma, 0.0), (mb, off_b)):
        for t in range(0, len(m.idx), 3):
            ks = []
            for e in range(3):
                v = m.idx[t + e]
                x = m.vx[v] + (sh if axis == 0 else 0)
                z = m.vz[v] + (sh if axis == 2 else 0)
                ks.append(key(x, m.vy[v], z))
            for e in range(3):
                a = ks[e]
                b = ks[(e + 1) % 3]
                edges[(min(a, b), max(a, b))] += 1
    line = off_b
    bad = 0
    for (a, b), n in edges.items():
        ca = a[0] if axis == 0 else a[2]
        cb = b[0] if axis == 0 else b[2]
        if n == 1 and abs(ca - line) < 1e-6 and abs(cb - line) < 1e-6:
            bad += 1
    return bad


def holes(m):
    from collections import Counter
    e = Counter()
    for t in range(0, len(m.idx), 3):
        for k in range(3):
            a = m.idx[t + k]
            b = m.idx[t + (k + 1) % 3]
            e[(min(a, b), max(a, b))] += 1
    open_ = sum(1 for (a, b), n in e.items() if n == 1 and not (m.ghost[a] and m.ghost[b]))
    multi = sum(1 for n in e.values() if n > 2)
    rim = sum(1 for (a, b), n in e.items() if n == 1 and m.ghost[a] and m.ghost[b])
    return f"open non-rim edges {open_}, edges on 3+ tris {multi}, rim edges {rim}"


def corner_union(bm, Hw):
    """2 x 2 patches of one field: union open edges that are not on the outer 16 m border"""
    from collections import Counter
    e = Counter()
    for oz in (0, NC):
        for ox in (0, NC):
            Y, D, on = patch_of(Hw, ox, oz)
            m = bm.build(Y, D, on)
            for t in range(0, len(m.idx), 3):
                ks = []
                for k in range(3):
                    v = m.idx[t + k]
                    ks.append((round(m.vx[v] + ox * FINE, 7), round(m.vy[v], 7), round(m.vz[v] + oz * FINE, 7)))
                for k in range(3):
                    a = ks[k]
                    b = ks[(k + 1) % 3]
                    e[(min(a, b), max(a, b))] += 1
    side = 2 * NC * FINE
    inner = 0
    for (a, b), n in e.items():
        if n != 1:
            continue
        outer = all(abs(c) < 1e-6 or abs(c - side) < 1e-6 for c in (a[0], b[0])) or             all(abs(c) < 1e-6 or abs(c - side) < 1e-6 for c in (a[2], b[2]))
        if not outer:
            inner += 1
    return inner


def depth_profile(m, x_m, z0, z1, n=200):
    """surface y along a line x = x_m (top-most triangle hit), for the rut cross-section"""
    out = []
    for i in range(n + 1):
        z = z0 + (z1 - z0) * i / n
        best = None
        for t in range(0, len(m.idx), 3):
            a, b, c = m.idx[t], m.idx[t + 1], m.idx[t + 2]
            y = bary_y(m.vx, m.vy, m.vz, a, b, c, x_m, z)
            if y is not None and (best is None or y > best):
                best = y
        out.append((z, best))
    return out


def bary_y(vx, vy, vz, a, b, c, x, z):
    x1, z1, x2, z2, x3, z3 = vx[a], vz[a], vx[b], vz[b], vx[c], vz[c]
    det = (z2 - z3) * (x1 - x3) + (x3 - x2) * (z1 - z3)
    if abs(det) < 1e-12:
        return None
    l1 = ((z2 - z3) * (x - x3) + (x3 - x2) * (z - z3)) / det
    l2 = ((z3 - z1) * (x - x3) + (x1 - x3) * (z - z3)) / det
    l3 = 1 - l1 - l2
    if l1 < -1e-9 or l2 < -1e-9 or l3 < -1e-9:
        return None
    return l1 * vy[a] + l2 * vy[b] + l3 * vy[c]


def report(name, m, gridtris):
    nsurf = len(m.vx) - 2 * len(getattr(m, 'skirt_top', {}))
    print(f"{name:<22} cells {m.cells:5d}  active {m.active:5d}  verts {nsurf:5d} (+{len(m.vx) - nsurf} skirt)"
          f"  tris {len(m.idx) // 3:5d} (+{len(m.skirt) // 3} skirt)  degen {m.degen:3d}  missing {getattr(m, 'missing', 0)}"
          f"  overflow {m.overflow}  | grid: 1089 verts, {gridtris} tris")


# ------------------------------------------------------------------------------------------------
# real patches from a play log: 'MUD dump' lines (MUD_WorldGroundEntity.DumpPatches)

def read_dump(path):
    """{key: dict(px, pz, base[1089], h[1089], show[1089], smooth[1089], cell[16])}, newest copy per key"""
    out = {}
    cur = {}
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            i = line.find('MUD dump')
            if i < 0:
                continue
            tok = line[i:].split()
            if len(tok) < 3:
                continue
            kind = tok[1]
            if kind == 'dumppatch':
                key = int(tok[2])
                cur[key] = dict(px=int(tok[3]), pz=int(tok[4]), base=[0.0] * 1089, h=[0.0] * 1089,
                                show=[0.0] * 1089, smooth=[0.0] * 1089, cell=[int(t) for t in tok[5:21]], rows=0)
            elif kind in ('dumpb', 'dumph', 'dumps', 'dumpw'):
                key = int(tok[2])
                row = int(tok[3])
                if key not in cur:
                    continue
                arr = {'dumpb': 'base', 'dumph': 'h', 'dumps': 'show', 'dumpw': 'smooth'}[kind]
                vals = [int(t) / 1000.0 for t in tok[4:4 + NV]]
                if len(vals) != NV:
                    continue
                cur[key][arr][row * NV:(row + 1) * NV] = vals
                cur[key]['rows'] += 1
            elif kind == 'dumpend':
                key = int(tok[2])
                if key in cur and cur[key]['rows'] == 4 * NV:
                    out[key] = cur.pop(key)
    return out


def dump_patch(d):
    show = d['show']
    if not any(show) and any(d['h']):
        show = d['h']       # dedicated server: no mesh, m_aShow never written
    Y = [d['base'][v] + show[v] for v in range(1089)]
    D = list(show)
    on = [0] * (NC * NC)
    for z in range(NC):
        for x in range(NC):
            cell = d['cell'][(z // 8) * 4 + x // 8]
            on[z * NC + x] = 1 if cell >= 0 and (cell & 32) == 0 else 0
    return Y, D, on


# ------------------------------------------------------------------------------------------------
# renders

def render(path, meshes, titles, x0=0, x1=8, z0=0, z1=8):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection
    fig, axs = plt.subplots(1, len(meshes), figsize=(6 * len(meshes), 6))
    if len(meshes) == 1:
        axs = [axs]
    L = (-0.5, 0.75, -0.43)
    ln = math.sqrt(dot(L, L))
    L = (L[0] / ln, L[1] / ln, L[2] / ln)
    for ax, (vx, vy, vz, idx), title in zip(axs, meshes, titles):
        polys = []
        shade = []
        for t in range(0, len(idx), 3):
            a, b, c = idx[t], idx[t + 1], idx[t + 2]
            pa = (vx[a], vy[a], vz[a])
            n = cross(sub((vx[b], vy[b], vz[b]), pa), sub((vx[c], vy[c], vz[c]), pa))
            nl = math.sqrt(dot(n, n)) or 1
            n = (n[0] / nl, n[1] / nl, n[2] / nl)
            if n[1] < 0:
                n = (-n[0], -n[1], -n[2])
            polys.append([(vx[a], vz[a]), (vx[b], vz[b]), (vx[c], vz[c])])
            shade.append(max(0.0, dot(n, L)))
        pc = PolyCollection(polys, array=shade, cmap='gray', edgecolors='face', linewidths=0.05)
        pc.set_clim(0, 1)
        ax.add_collection(pc)
        ax.set_xlim(x0, x1)
        ax.set_ylim(z0, z1)
        ax.set_aspect('equal')
        ax.set_title(title)
    fig.tight_layout()
    fig.savefig(path, dpi=110)
    plt.close(fig)


def render_profile(path, prof_a, prof_b, title):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(9, 3.5))
    ax.plot([p[0] for p in prof_a], [p[1] for p in prof_a], label="today's grid mesh", lw=1.6)
    ax.plot([p[0] for p in prof_b], [p[1] for p in prof_b], label='band mesher (dual)', lw=1.2, ls='--')
    ax.set_xlabel('z (m)')
    ax.set_ylabel('y (m)')
    ax.set_title(title)
    ax.legend()
    fig.tight_layout()
    fig.savefig(path, dpi=110)
    plt.close(fig)


# ------------------------------------------------------------------------------------------------

def mesh_stats_line(name, m):
    report(name, m, 2048)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dump', help='console.log with MUD dump lines')
    ap.add_argument('--png', help='write renders here')
    args = ap.parse_args()
    bm = BandMesher()
    results = []

    # world field 16 x 16 m = 2 x 2 patches (65 x 65 fine verts)
    W = 2 * NC + 1
    cases = []
    H = field(W, W)
    rut_line(H, 0.5, 2.6, 15.5, 3.4, passes=3)
    rut_line(H, 0.5, 4.6, 15.5, 5.4, passes=3)
    cases.append(('ruts (3 passes)', H, None))
    H2 = field(W, W)
    crater(H2, 4.1, 3.9)
    cases.append(('mortar crater', H2, None))
    H3 = field(W, W)
    trench(H3, 1.0, 3.0, 7.0, 4.25)
    cases.append(('trench 1.6 m', H3, None))
    H4 = field(W, W)
    crater(H4, 4.1, 3.9)
    rut_line(H4, 0.5, 2.6, 15.5, 3.4, passes=3)
    cases.append(('crater + kept cells', H4, [(3, 0), (3, 1), (0, 3)]))

    print('PATCH COUNTS (dual band mesher vs grid)')
    for name, Hw, kept in cases:
        Y, D, on = patch_of(Hw, 0, 0, kept)
        m = bm.build(Y, D, on)
        report(name, m, sum(on) * 2)
        results.append((name, Y, D, on, m))

    H5 = field(W, W)
    crater(H5, 8.1, 7.9)
    trench(H5, 2.0, 11.0, 14.0, 12.25)
    rut_line(H5, 0.5, 6.6, 15.5, 9.4, passes=3)
    cases.append(('corner crater+trench', H5, None))

    print('\nHOLES (edges used once that are not rim edges, or used 3+ times)')
    for name, Y, D, on, m in results:
        print(f"{name:<22} {holes(m)}")

    print('\nSEAMS (patch pairs from the same world field)')
    for name, Hw, kept in cases[:3] + cases[4:]:
        for axis, (oa, ob) in ((0, ((0, 0), (NC, 0))), (2, ((0, 0), (0, NC)))):
            Ya, Da, ona = patch_of(Hw, oa[0], oa[1])
            Yb, Db, onb = patch_of(Hw, ob[0], ob[1])
            ma = bm.build(Ya, Da, ona)
            mb = bm.build(Yb, Db, onb)
            na, nb, diff, same = seam_gap(ma, mb, axis, NC * FINE, 0.0)
            cracks = open_edges_on_line(ma, mb, axis, NC * FINE)
            print(f"{name:<18} {'x' if axis == 0 else 'z'} seam: rim verts A {na} B {nb}, not shared {diff}, identical {same},"
                  f" open edges on the line {cracks}")

    print('\n2 x 2 PATCH UNION (open edges inside the 16 m square = cracks)')
    for name, Hw, kept in cases:
        if kept:
            continue
        print(f"{name:<22} {corner_union(bm, Hw)}")

    if args.dump:
        dumps = read_dump(args.dump)
        print(f'\nREAL PATCHES from {args.dump}: {len(dumps)}')
        for key, d in sorted(dumps.items()):
            Y, D, on = dump_patch(d)
            m = bm.build(Y, D, on)
            dmin = min(d['h'])
            dmax = max(d['h'])
            report(f'patch {key} ({d["px"]},{d["pz"]}) h {dmin:+.2f}..{dmax:+.2f}', m, sum(on) * 2)
        # neighbour pairs among the dumped patches
        bykey = {(d['px'], d['pz']): d for d in dumps.values()}
        pairs = 0
        for (px, pz), d in sorted(bykey.items()):
            for axis, nb in ((0, (px + 1, pz)), (2, (px, pz + 1))):
                if nb not in bykey:
                    continue
                Ya, Da, ona = dump_patch(d)
                Yb, Db, onb = dump_patch(bykey[nb])
                ma = bm.build(Ya, Da, ona)
                mb = bm.build(Yb, Db, onb)
                na, nbv, diff, same = seam_gap(ma, mb, axis, NC * FINE, 0.0)
                cracks = open_edges_on_line(ma, mb, axis, NC * FINE)
                pairs += 1
                print(f"pair ({px},{pz})-{nb}: rim verts {na}/{nbv}, not shared {diff}, identical {same}, open edges on the line {cracks}")
        print(f'neighbour pairs checked: {pairs}')

    if args.png:
        os.makedirs(args.png, exist_ok=True)
        for name, Y, D, on, m in results:
            g = grid_mesh(Y, on)
            tag = name.split()[0]
            render(os.path.join(args.png, f'sdf_{tag}.png'),
                   [g, (m.vx, m.vy, m.vz, m.idx)],
                   [f"today's grid ({len(g[3]) // 3} tris)", f"band mesher ({len(m.idx) // 3} tris)"])
        name, Y, D, on, m = results[0]
        g = grid_mesh(Y, on)

        class G:
            pass
        gm = G()
        gm.vx, gm.vy, gm.vz, gm.idx = g
        render_profile(os.path.join(args.png, 'sdf_rut_profile.png'), depth_profile(gm, 4.0, 1.5, 6.5),
                       depth_profile(m, 4.0, 1.5, 6.5), 'rut cross-section at x = 4 m')
        print('renders in', args.png)


if __name__ == '__main__':
    main()
