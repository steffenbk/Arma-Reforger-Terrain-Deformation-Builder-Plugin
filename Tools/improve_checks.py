"""Offline checks of the PROPOSED fixes in docs/superpowers/plans/2026-10-16-ground-improvement-plan.md (2026-10-16).
Same game formulas as Tools/gap_checks.py (copied from MUD_VoxelProbe.c), each fix applied the way the plan describes.

    python Tools/improve_checks.py road | trench | tunnel | blast | stack | jip | all

Results: docs/superpowers/results/2026-10-16-improve-checks.md
"""
import math
import os
import random
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dc_vertex_proto as P
import gap_checks as G


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


# ------------------------------------------------------------------ fix C3: crater fades to 0 at kept / hard ground

class CraterFade(G.CraterHard):
    """Carve result blended with the old value by smoothstep(distance to the hard line / FADE) - the voxel version of
    the heightfield's EdgeLock (smoothstep over m_fEdgeFade 1 m). Distance from the skin hole map = same everywhere."""
    FADE = 1.0

    def apply(self, Gr):
        gi0 = math.floor((self.cx - self.outer) / P.CELL)
        gi1 = math.ceil((self.cx + self.outer) / P.CELL)
        gk0 = math.floor((self.cz - self.outer) / P.CELL)
        gk1 = math.ceil((self.cz + self.outer) / P.CELL)
        gj0 = math.floor((self.wy - self.depth - 1) / P.CELL)
        gj1 = math.ceil((self.wy + G.FLOOR + self.rimH + 1.5) / P.CELL)
        for gk in range(max(gk0, 0), min(gk1, P.NZ - 1) + 1):
            for gi in range(max(gi0, 0), min(gi1, P.NX - 1) + 1):
                x, z = gi * P.CELL, gk * P.CELL
                if x <= self.road_x + 1e-6:
                    continue
                f = smoothstep((x - self.road_x) / self.FADE)
                undug = P.top_of(x, z)
                for gj in range(max(gj0, P.J0), min(gj1, P.J0 + P.NY - 1) + 1):
                    y = gj * P.CELL
                    old = Gr[gi, gj - P.J0, gk]
                    new = self.value(x, y, z, old, undug)
                    Gr[gi, gj - P.J0, gk] = P.nudge(old + (new - old) * f)


def surface_profile(Gr, z, x0, x1):
    gk = int(round(z / P.CELL))
    out = []
    for gi in range(int(round(x0 / P.CELL)), int(round(x1 / P.CELL)) + 1):
        c = G.crossings_down(Gr, gi, gk)
        if c:
            out.append((gi * P.CELL, c[0][0] - P.top_of(gi * P.CELL, z)))
    return out


def road():
    print("| Crater | Centre from road m | Today: drop at 0.5 m / steepest 0.5 m step deg (first 2 m) | Fade 1 m: drop at 0.5 m / steepest deg | Fade 1 m: depth at the crater centre m |")
    print("|---|---|---|---|---|")
    road_x = 8.0
    for label, depth in (("O-832DU", 0.86), ("81 mm", 1.43), ("Hydra", 2.04)):
        for off in (1.0, 2.0, 3.0):
            res = []
            for cls in (G.CraterHard, CraterFade):
                Gr = G.grid(20, 16, -6, 4)
                z = 8.1
                cls(road_x + off, z, depth, road_x).apply(Gr)
                prof = surface_profile(Gr, z, road_x, road_x + 2.5)
                drop = -prof[1][1] + prof[0][1]
                steep = max(math.degrees(math.atan(abs(prof[i][1] - prof[i - 1][1]) / P.CELL)) for i in range(1, 5))
                cen = [d for x, d in prof if abs(x - (road_x + off)) < 0.26]
                res.append((drop, steep, -min(cen) if cen else float('nan')))
            print(f"| {label} {depth} m | {off} | {res[0][0]:.2f} / {res[0][1]:.0f} | {res[1][0]:.2f} / {res[1][1]:.0f} | {res[1][2]:.2f} |", flush=True)


# ------------------------------------------------------------------ fix T1: pad the trench cut so the DRAWN floor = plan

def trench():
    print("| Pad each side m | Planned floor m | Drawn floor mean m (min-max over 2 offsets x 3 angles) | Mean error m |")
    print("|---|---|---|---|")
    for pad in (0.0, 0.125, 0.25):
        for w in (0.5, 1.0, 1.5, 2.0):
            ws = []
            for ang in (0, 30, 45):
                for off in (0.0, 0.25):
                    Gr = G.grid(14, 14, -4, 3)
                    cx, cz = 7.05 + off, 7.05 + off
                    G.TrenchRot(cx, cz, 8.0, w + 2 * pad, ang, 6).apply(Gr)
                    ws.append(drawn_floor(Gr, cx, cz, ang))
            print(f"| {pad} | {w} | {np.mean(ws):.2f} ({min(ws):.2f}-{max(ws):.2f}) | {np.mean(ws) - w:+.2f} |", flush=True)


def drawn_floor(Gr, cx, cz, ang):
    tris, _ = P.polygonize(Gr, P.Vertexer(Gr, "mean", []))
    B = {}
    for t in tris:
        xs = [p[0] for p in t]
        zs = [p[2] for p in t]
        for i in range(int(min(xs) // 0.5), int(max(xs) // 0.5) + 1):
            for k in range(int(min(zs) // 0.5), int(max(zs) // 0.5) + 1):
                B.setdefault((i, k), []).append(t)
    a = math.radians(ang)
    widths = []
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
                prof.append(best - P.top_of(x, z))
        bot = min(prof)
        widths.append(sum(0.02 for y in prof if y < bot + 0.05))
    return float(np.mean(widths))


# ------------------------------------------------------------------ fix U1: tunnels may go deeper (FLOOR per op kind)

def tunnel():
    print("| Tunnel FLOOR m | Player digs down to m | Roof (sample field) m min-max | Roof drawn on the axis m min-max | Clearance m |")
    print("|---|---|---|---|---|")
    for floor, target in ((3.0, 3.0), (6.0, 3.0), (6.0, 4.0), (6.0, 5.0)):
        G.FLOOR = floor
        Gr = G.grid(20, 8.5, -8, 4)
        z = 4.1
        feet = [3.0, P.top_of(3.0, z), z]
        pitch = -0.7
        while feet[0] < 16:
            mid = (feet[0] + G.TUNNEL_AHEAD, feet[1] + G.TUNNEL_MID + pitch * G.TUNNEL_SLOPE, feet[2])
            G.carve_tunnel(Gr, mid)
            nx = feet[0] + 0.5
            fy = G.floor_under(Gr, nx, z, feet[1] + 0.6)
            if fy is None:
                break
            if pitch < 0 and P.top_of(nx, z) - fy >= target - 0.05:
                pitch = 0.3     # level: the capsule floor is 0.3 m under the feet at pitch 0 (0.95 - 0.45 - 0.8)
            feet = [nx, fy, z]
        gk = int(round(z / P.CELL))
        roofs, clears = [], []
        for gi in range(int(9 / P.CELL), int(16 / P.CELL) + 1):
            c = G.crossings_down(Gr, gi, gk)
            tops = [y for y, k in c if k == "top"]
            ceils = [y for y, k in c if k == "ceil"]
            if ceils and len(tops) > 1:
                roofs.append(tops[0] - ceils[0])
                clears.append(ceils[0] - tops[1])
        tris, _ = P.polygonize(Gr, P.Vertexer(Gr, "mean", []))
        drawn = []
        for x in np.arange(9, 16, 0.25):
            ys = sorted((P.ray_y(t, x, z) for t in tris if P.ray_y(t, x, z) is not None), reverse=True)
            ys = [y for i, y in enumerate(ys) if i == 0 or ys[i - 1] - y > 1e-4]
            if len(ys) >= 3:
                drawn.append(ys[0] - ys[1])
        print(f"| {floor} | {target} | {min(roofs):.2f}-{max(roofs):.2f} | {min(drawn):.2f}-{max(drawn):.2f} | {np.mean(clears):.2f} |", flush=True)
    G.FLOOR = 3.0


# ------------------------------------------------------------------ fix C5: a blast with solid ground over it is a sphere

class Sphere:
    def __init__(self, cx, cy, cz, r):
        self.c = (cx, cy, cz)
        self.r = r

    def apply(self, Gr):
        r = self.r
        cx, cy, cz = self.c
        for gk in range(max(math.floor((cz - r) / P.CELL), 0), min(math.ceil((cz + r) / P.CELL), P.NZ - 1) + 1):
            for gi in range(max(math.floor((cx - r) / P.CELL), 0), min(math.ceil((cx + r) / P.CELL), P.NX - 1) + 1):
                x, z = gi * P.CELL, gk * P.CELL
                floor_y = P.top_of(x, z) - G.FLOOR
                for gj in range(max(math.floor((cy - r) / P.CELL), P.J0), min(math.ceil((cy + r) / P.CELL), P.J0 + P.NY - 1) + 1):
                    y = gj * P.CELL
                    s = math.sqrt((x - cx) ** 2 + (y - cy) ** 2 + (z - cz) ** 2) - r
                    old = Gr[gi, gj - P.J0, gk]
                    d = max(min(old, s), floor_y - y)
                    Gr[gi, gj - P.J0, gk] = P.nudge(d)


def solid_above(Gr, x, y, z, reach=3.0):
    gi, gk = int(round(x / P.CELL)), int(round(z / P.CELL))
    for gj in range(math.ceil(y / P.CELL), math.floor((y + reach) / P.CELL) + 1):
        if P.J0 <= gj < P.J0 + P.NY and Gr[gi, gj - P.J0, gk] > 0:
            return True
    return False


def blast():
    print("| Charge | Depth m | Roof over the hit found | Shape | New columns open to the sky | Chamber: max radius dug m |")
    print("|---|---|---|---|---|---|")
    for label, depth in (("RPG HEAT", 0.60), ("0.76 kg charge", 1.27), ("81 mm-size", 1.43)):
        Gr, _ = G.dig_tunnel()
        z, x = 4.1, 12.0
        fy = G.floor_under(Gr, x, z, P.top_of(x, z) - 1.0)
        wy = fy + 0.3

        def open_cols():
            out = set()
            for gk in range(P.NZ):
                for gi in range(P.NX):
                    c = G.crossings_down(Gr, gi, gk)
                    if not [y for y, k in c if k == "ceil"] and c and c[0][0] < P.top_of(gi * P.CELL, gk * P.CELL) - 1.5:
                        out.add((gi, gk))
            return out
        before = open_cols()
        roof = solid_above(Gr, x, wy, z)
        Sphere(x, wy, z, depth).apply(Gr)
        new = open_cols() - before
        print(f"| {label} | {depth} | {roof} | sphere r = depth | {len(new)} | {depth:.2f} |", flush=True)


# ------------------------------------------------------------------ fix C2: the heightfield's stack cap on the voxel path

def stack():
    """Server, before Issue: cur = how deep the hit spot already is (ColumnDepth); target = max(full, min(cap,
    cur + GAIN x full)) as MUD_WorldGround Crater (m_fCraterStackDepth 2.5, factor 1.67, deepen gain 0.5); the op
    then carves (target - cur) under the hit, at least 0.15 (DoBlast clamp)."""
    random.seed(3)
    print("| Charge | Hit | Deepest m today (gap-checks) | Deepest m with the stack cap | Cap m |")
    print("|---|---|---|---|---|")
    today = {("81 mm", 1): 1.44, ("81 mm", 2): 2.88, ("81 mm", 3): 3.0, ("O-832DU", 1): 0.85, ("O-832DU", 2): 1.56,
             ("O-832DU", 3): 2.21, ("O-832DU", 4): 3.0}
    for label, full in (("81 mm", 1.43), ("O-832DU", 0.86)):
        cap = min(2.5, 1.67 * full)
        Gr = G.grid(22, 22, -6, 4)
        cx, cz = 11.1, 10.9
        for hit in range(1, 7):
            x = cx + random.uniform(-1, 1)
            z = cz + random.uniform(-1, 1)
            c = G.crossings_down(Gr, int(round(x / P.CELL)), int(round(z / P.CELL)))
            wy = c[0][0] if c else P.top_of(x, z)
            cur = max(0.0, P.top_of(x, z) - wy)
            target = max(full, min(cap, cur + 0.5 * full))
            d = max(0.15, target - cur)
            G.CraterAt(x, z, d, wy).apply(Gr)
            deepest = 0.0
            for gk in range(P.NZ):
                for gi in range(P.NX):
                    cc = G.crossings_down(Gr, gi, gk)
                    if cc:
                        deepest = max(deepest, P.top_of(gi * P.CELL, gk * P.CELL) - cc[0][0])
            t = today.get((label, hit), 3.0)
            print(f"| {label} {full} m | {hit} | {t:.2f} | {deepest:.2f} | {cap:.2f} |", flush=True)


# ------------------------------------------------------------------ fix J1: JIP as a rut-floor snapshot, not the op log

ALPHA = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"   # MUD_VoxelProbe.ALPHA, ALPHA_MID 30


def path(length, rng, start, heading):
    pts = [start]
    x, z = start
    h = heading
    for _ in range(int(length)):
        h += rng.uniform(-0.06, 0.06)
        x += math.cos(h)
        z += math.sin(h)
        pts.append((x, z))
    return pts


def jip():
    """Snapshot = per touched 4 m rut tile (16 x 16 floor samples, as MUD_RutTile): 'bi,bk:' + the samples in order,
    NONE runs as '~n.' (n >= 4, else one char each), a rut sample as its depth under undug in mm, written as the delta
    from the previous rut sample (one ALPHA char for -30..31 mm, else '!d.') - the MakeSeed scheme in mm.
    Tracks: two wheel tracks 1.9 m apart (ASSUMED track width), RUT_R 0.5, profile 1 - k^2, depth cap 0.35 m.
    Op log for the same drive: 2.9 rut ops / s per truck (MEASURED freeze-test) x 339 chars per op (MEASURED
    jip-size), at an ASSUMED mean speed of 6 m/s."""
    rng = random.Random(7)
    print("| Track km | Touched 4 m tiles | Snapshot chars (mm deltas) | Snapshot chars (cm, 1 char per sample) | Op log chars (one pass) | Op log / snapshot (mm / cm) | Op log chars if the same tracks are driven 3 times (snapshot unchanged) |")
    print("|---|---|---|---|---|---|---|")
    for km in (1, 5, 10):
        samples = {}
        n_paths = max(1, km)
        for p in range(n_paths):
            pts = path(1000 * km / n_paths, rng, (rng.uniform(0, 3000), rng.uniform(0, 3000)), rng.uniform(0, 6.28))
            for (ax, az), (bx, bz) in zip(pts[:-1], pts[1:]):
                dx, dz = bx - ax, bz - az
                nx, nz = -dz, dx
                for side in (-0.95, 0.95):
                    sax, saz, sbx, sbz = ax + nx * side, az + nz * side, bx + nx * side, bz + nz * side
                    for fi in range(int((min(sax, sbx) - 0.5) / 0.25), int((max(sax, sbx) + 0.5) / 0.25) + 2):
                        for fk in range(int((min(saz, sbz) - 0.5) / 0.25), int((max(saz, sbz) + 0.5) / 0.25) + 2):
                            px, pz = fi * 0.25, fk * 0.25
                            l2 = (sbx - sax) ** 2 + (sbz - saz) ** 2
                            u = max(0.0, min(1.0, ((px - sax) * (sbx - sax) + (pz - saz) * (sbz - saz)) / l2))
                            d = math.hypot(sax + (sbx - sax) * u - px, saz + (sbz - saz) * u - pz)
                            if d < 0.5:
                                mm = int(round(0.35 * (1 - (d / 0.5) ** 2) * 1000))
                                if mm > samples.get((fi, fk), 0):
                                    samples[(fi, fk)] = mm
        tiles = {}
        for (fi, fk), mm in samples.items():
            tiles.setdefault((fi // 16, fk // 16), {})[(fi % 16, fk % 16)] = mm
        chars_cm = 0
        for (bi, bk), t in tiles.items():
            s = f"{bi},{bk}:"
            run = 0
            for lk in range(16):
                for li in range(16):
                    v = t.get((li, lk))
                    if v is None:
                        run += 1
                        continue
                    if run:
                        s += f"~{run}." if run >= 4 else "." * run
                        run = 0
                    cm = int(round(v / 10))
                    s += ALPHA[cm] if cm < 62 else f"!{cm}."
            chars_cm += len(s) + 1
        chars = 0
        for (bi, bk), t in tiles.items():
            s = f"{bi},{bk}:"
            run, prev = 0, 0
            for lk in range(16):
                for li in range(16):
                    v = t.get((li, lk))
                    if v is None:
                        run += 1
                        continue
                    if run:
                        s += f"~{run}." if run >= 4 else "." * run
                        run = 0
                    dlt = v - prev
                    prev = v
                    s += ALPHA[dlt + 30] if -30 <= dlt <= 31 else f"!{dlt}."
            chars += len(s) + 1
        ops = 1000 * km / 6.0 * 2.9
        print(f"| {km} | {len(tiles)} | {chars} | {chars_cm} | {int(ops * 339)} | {ops * 339 / chars:.1f}x / {ops * 339 / chars_cm:.1f}x | {int(3 * ops * 339)} |", flush=True)


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    for name, fn in (("road", road), ("trench", trench), ("tunnel", tunnel), ("blast", blast), ("stack", stack), ("jip", jip)):
        if what in (name, "all"):
            fn()
            print()
