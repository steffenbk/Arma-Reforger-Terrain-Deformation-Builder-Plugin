"""Build 3 Stage 0 (2026-10-17): SCM-lite rut shape, offline, on CarveFloor's own maths.

Copied from MUD_VoxelProbe.CarveFloor (VERIFIED-CODE 2026-10-17): floor samples every FCELL 0.25 m, profile
fall = 1 - k^2 over RUT_R, a sample is taken by a segment only when 0 <= u < 1 (never clamped), the strongest fall
per sample per op wins, floorY = surf - step x fall, not under undug - cap x fall, not under undug - FLOOR, rounded
to whole cm; only lowers. Prefab values: step 0.08, cap = min(0.08 + t x 0.025, 0.35). Ground is flat (undug 0).
One "pass" = one wheel over the track = one op holding every segment (points every RUT_STEP 0.5 m).

Variants:
  base   today
  r4     cut = max(step x fall, SHARE x (surf - capY))   (capY = -cap x fall)
  relax  r4 + angle-of-repose relax: after the cut, SORTED sample order, 4 neighbours, where the drop over the
         distance is steeper than tan(REPOSE) move half the excess from the high sample to the low one (volume kept);
         touched samples + 1 ring; never above the undug ground; cm rounding after
  ridge  relax + bulldozed ridge: a band RUT_R .. RUT_R + RIDGE_W outside the centre line rises by
         BULL x (deepest cut this op) x pi / 3 x sin(pi (d - r) / W) (volume balance for fall = 1 - k^2 with
         r = W), capped at RIDGE_MAX, max blend
  r3     base with RUT_R = max(0.5, cap / 0.58)

Wall angle = slope of each grid triangle (both diagonals, the patch picks one per cell from the skin) with any
corner deeper than 1 cm, measured over the middle 4 m of a 12 m track. Tracks at 0 / 20 / 45 deg, 7 grid offsets.

    python Tools/scm_lite_proto.py      results: docs/superpowers/results/2026-10-17-scm-lite-proto.md
"""
import math

import numpy as np

FCELL = 0.25
RUT_STEP = 0.5
FLOOR = 3.0
STEP = 0.08
N = 64                      # samples per side (16 m)
TRACK_LEN = 12.0


def carve(F, R, segs, step, cap, r, share=None, rclr=False):
    """One op. F = floor (y, 0 = undug), R = ridge (y, 0 = none). Returns (written keys, deepest cut).
    rclr (2026-10-17 RUT_R 0.3 rows): a cut sample loses its ridge (the Enforce draw is max(min(smooth, floor), ridge),
    so a ridge left on a cut sample would hide the cut)."""
    best = {}
    for (ax, az), (bx, bz) in segs:
        abx, abz = bx - ax, bz - az
        len2 = abx * abx + abz * abz
        fi0 = math.floor((min(ax, bx) - r) / FCELL)
        fi1 = math.ceil((max(ax, bx) + r) / FCELL)
        fk0 = math.floor((min(az, bz) - r) / FCELL)
        fk1 = math.ceil((max(az, bz) + r) / FCELL)
        for fk in range(max(fk0, 0), min(fk1, N - 1) + 1):
            for fi in range(max(fi0, 0), min(fi1, N - 1) + 1):
                px, pz = fi * FCELL, fk * FCELL
                u = ((px - ax) * abx + (pz - az) * abz) / len2
                if u < 0 or u >= 1:
                    continue
                qx, qz = ax + abx * u - px, az + abz * u - pz
                dist = math.sqrt(qx * qx + qz * qz)
                if dist >= r:
                    continue
                k = dist / r
                fall = 1 - k * k
                key = (fk, fi)
                if fall > best.get(key, -1):
                    best[key] = fall
    written = []
    maxcut = 0.0
    for key in sorted(best):
        cf = best[key]
        surf = F[key]
        capY = -cap * cf
        if share is None:
            floorY = surf - step * cf
        else:
            floorY = surf - max(step * cf, share * (surf - capY))
        floorY = max(floorY, capY, -FLOOR)
        floorY = round(floorY * 100) / 100.0
        if floorY >= surf:
            continue
        maxcut = max(maxcut, surf - floorY)
        F[key] = floorY
        if rclr:
            R[key] = 0.0
        written.append(key)
    return written, maxcut


NB4 = ((0, 1), (1, 0), (0, -1), (-1, 0))
NB8 = NB4 + ((1, 1), (1, -1), (-1, 1), (-1, -1))


def relax(F, keys, repose_deg, iters, diag=False):
    tanA = math.tan(math.radians(repose_deg))
    ring = set()
    for fk, fi in keys:
        for dk in (-1, 0, 1):
            for di in (-1, 0, 1):
                if 0 <= fk + dk < N and 0 <= fi + di < N:
                    ring.add((fk + dk, fi + di))
    order = sorted(ring)
    lim = tanA * FCELL
    for _ in range(iters):
        for fk, fi in order:
            for dk, di in NB8 if diag else NB4:
                nk, ni = fk + dk, fi + di
                if not (0 <= nk < N and 0 <= ni < N):
                    continue
                dh = F[fk, fi] - F[nk, ni]
                if dh > lim * (1.4142136 if dk and di else 1.0):
                    move = (dh - lim) * 0.5
                    F[fk, fi] -= move
                    F[nk, ni] += move
    for key in order:
        F[key] = min(0.0, round(F[key] * 100) / 100.0)


def ridge(R, F, segs, r, w, bull, maxcut, rmax, gap=0.0, vol_r=None, blend="add"):
    """vol_r = radius for the volume balance (None = r + gap, the first Stage 0 rows; the RUT_R 0.3 rows pass RUT_R).
    blend "add" = ridge grows each pass (first rows), "max" = max(old, this op's ridge) (the Stage 3 plan text)."""
    if vol_r is None:
        vol_r = r + gap
    r = r + gap
    h = bull * maxcut * math.pi / 3 * (vol_r / w)
    if h <= 0:
        return
    for (ax, az), (bx, bz) in segs:
        abx, abz = bx - ax, bz - az
        len2 = abx * abx + abz * abz
        fi0 = math.floor((min(ax, bx) - r - w) / FCELL)
        fi1 = math.ceil((max(ax, bx) + r + w) / FCELL)
        fk0 = math.floor((min(az, bz) - r - w) / FCELL)
        fk1 = math.ceil((max(az, bz) + r + w) / FCELL)
        for fk in range(max(fk0, 0), min(fk1, N - 1) + 1):
            for fi in range(max(fi0, 0), min(fi1, N - 1) + 1):
                px, pz = fi * FCELL, fk * FCELL
                u = ((px - ax) * abx + (pz - az) * abz) / len2
                if u < 0 or u >= 1:
                    continue
                qx, qz = ax + abx * u - px, az + abz * u - pz
                d = math.sqrt(qx * qx + qz * qz)
                if d < r or d >= r + w or F[fk, fi] < 0:
                    continue    # only on undug ground beside the rut
                add = h * math.sin(math.pi * (d - r) / w)
                if blend == "max":
                    R[fk, fi] = max(R[fk, fi], min(rmax, round(add * 100) / 100.0))
                else:
                    R[fk, fi] = min(rmax, round((R[fk, fi] + add) * 100) / 100.0)


def track(angle, phase):
    a = math.radians(angle)
    dx, dz = math.cos(a), math.sin(a)
    cx, cz = 8.0 + phase, 8.0 + phase * 0.37
    n = int(TRACK_LEN / RUT_STEP)
    pts = [(cx + dx * (i * RUT_STEP - TRACK_LEN / 2), cz + dz * (i * RUT_STEP - TRACK_LEN / 2)) for i in range(n + 1)]
    return [(pts[i], pts[i + 1]) for i in range(n)], (cx, cz, dx, dz)


def measure(F, R, geo, r):
    cx, cz, dx, dz = geo
    H = F + R
    ang = []
    for fk in range(N - 1):
        for fi in range(N - 1):
            mx, mz = (fi + 0.5) * FCELL, (fk + 0.5) * FCELL
            along = (mx - cx) * dx + (mz - cz) * dz
            if abs(along) > 2.0:
                continue
            h00, h10, h01, h11 = H[fk, fi], H[fk, fi + 1], H[fk + 1, fi], H[fk + 1, fi + 1]
            if min(F[fk, fi], F[fk, fi + 1], F[fk + 1, fi], F[fk + 1, fi + 1]) > -0.01 and max(h00, h10, h01, h11) < 0.01:
                continue
            for tri in ((h00, h10, h11, 1), (h00, h11, h01, 2), (h00, h10, h01, 3), (h10, h11, h01, 4)):
                a_, b_, c_, kind = tri
                if kind == 1:      # (0,0) (1,0) (1,1)
                    gx, gz = (b_ - a_), (c_ - b_)
                elif kind == 2:    # (0,0) (1,1) (0,1)
                    gx, gz = (b_ - c_), (c_ - a_)
                elif kind == 3:    # (0,0) (1,0) (0,1)
                    gx, gz = (b_ - a_), (c_ - a_)
                else:              # (1,0) (1,1) (0,1)
                    gx, gz = (b_ - c_), (b_ - a_)
                ang.append(math.degrees(math.atan(math.hypot(gx, gz) / FCELL)))
    # depth / ridge on the middle cross-sections (grid samples within 0.13 m of the centre line count as centre)
    depth = 0.0
    rid = 0.0
    for fk in range(N):
        for fi in range(N):
            px, pz = fi * FCELL, fk * FCELL
            along = (px - cx) * dx + (pz - cz) * dz
            if abs(along) > 2.0:
                continue
            depth = max(depth, -F[fk, fi])
            rid = max(rid, R[fk, fi])
    vol_cut = -F.sum() * FCELL * FCELL / TRACK_LEN
    vol_ridge = R.sum() * FCELL * FCELL / TRACK_LEN
    width = 0.0
    for s in np.arange(-1.5, 1.5001, 0.05):     # rut width = across the track where the floor is deeper than 1 cm
        x, z = cx - dz * s, cz + dx * s
        fi, fk = int(round(x / FCELL)), int(round(z / FCELL))
        if F[fk, fi] < -0.01:
            width += 0.05
    a = np.array(ang) if ang else np.array([0.0])
    return depth, float(np.percentile(a, 90)), float(a.max()), rid, vol_cut, vol_ridge, width


VARIANTS = {
    "base": dict(),
    "r4 0.40": dict(share=0.40),
    "r4 0.55": dict(share=0.55),
    "relax 35": dict(share=0.40, repose=35, iters=2),
    "relax 40": dict(share=0.40, repose=40, iters=2),
    "ridge B0.3": dict(share=0.40, repose=40, iters=2, bull=0.3),
    "ridge B0.5": dict(share=0.40, repose=40, iters=2, bull=0.5),
    "relax 30 x4": dict(share=0.40, repose=30, iters=4),
    "relax 35 x4 diag": dict(share=0.40, repose=35, iters=4, diag=True),
    "relax 40 x4 diag": dict(share=0.40, repose=40, iters=4, diag=True),
    "ridge B0.3 gap0.25": dict(share=0.40, repose=35, iters=4, diag=True, bull=0.3, gap=0.25),
    "ridge B0.3 gap0.25 max0.08": dict(share=0.40, repose=35, iters=4, diag=True, bull=0.3, gap=0.25, rmax=0.08),
    "pick: r4 + relax 30 x4 + ridge B0.3 gap0.25 max0.08": dict(share=0.40, repose=30, iters=4, bull=0.3, gap=0.25, rmax=0.08),
    "r3": dict(r3=True),
    "r3 + r4 0.40": dict(r3=True, share=0.40),
    "r3 + r4 + ridge B0.3 gap0.25": dict(r3=True, share=0.40, bull=0.3, gap=0.25),
}
# 2026-10-17 re-run for today's code: RUT_R 0.3, share 0.40, relax 40 deg x4, 4 neighbours (MUD_VoxelProbe
# RUT_R / RUT_SHARE / RUT_REPOSE_TAN / RUT_REPOSE_ITERS). Ridge rows: a cut / relaxed sample loses its ridge (rclr),
# volume balance on RUT_R (vol_r), "max" = max blend (plan text), "add" = grows each pass.
NOW = dict(r=0.3, share=0.40, repose=40, iters=4)
VARIANTS["r0.3 relax 40 x4 (today)"] = dict(NOW)
for _b in (0.2, 0.3, 0.4):
    for _g in (0.0, 0.1, 0.2):
        for _w in (0.3, 0.5):
            for _bl in ("max", "add"):
                VARIANTS[f"r0.3 ridge {_bl} B{_b} gap{_g} W{_w} max0.08"] = dict(
                    NOW, rclr=True, vol_r=0.3, bull=_b, gap=_g, w=_w, rmax=0.08, blend=_bl)
for _m in (0.06, 0.10, 0.12):
    for _bl in ("max", "add"):
        VARIANTS[f"r0.3 ridge {_bl} B0.3 gap0.1 W0.3 max{_m}"] = dict(
            NOW, rclr=True, vol_r=0.3, bull=0.3, gap=0.1, w=0.3, rmax=_m, blend=_bl)
RIDGE_W, RIDGE_MAX = 0.5, 0.10
ANGLES = (0, 20, 45)
PHASES = [i / 7 * FCELL for i in range(7)]
PASSES = (1, 2, 3, 4, 6, 8)


def run(cap, var):
    r = max(0.5, cap / 0.58) if var.get("r3") else var.get("r", 0.5)
    rows = {p: [] for p in PASSES}
    for ang in ANGLES:
        for ph in PHASES:
            F = np.zeros((N, N))
            R = np.zeros((N, N))
            segs, geo = track(ang, ph)
            for p in range(1, max(PASSES) + 1):
                keys, maxcut = carve(F, R, segs, STEP, cap, r, var.get("share"), var.get("rclr", False))
                if "repose" in var:
                    relax(F, keys, var["repose"], var["iters"], var.get("diag", False))
                if var.get("rclr"):
                    R[F < 0] = 0.0      # a relaxed (widened) sample loses its ridge too
                if "bull" in var:
                    ridge(R, F, segs, r, var.get("w", RIDGE_W), var["bull"], maxcut, var.get("rmax", RIDGE_MAX),
                          var.get("gap", 0.0), var.get("vol_r"), var.get("blend", "add"))
                if p in rows:
                    rows[p].append(measure(F, R, geo, r))
    return rows


def main():
    out = []
    for name_cap, cap in (("light (~2.5 t, cap 0.14)", 0.14), ("heavy (>= 10.8 t, cap 0.35)", 0.35)):
        out.append(f"\n### {name_cap}\n")
        out.append("| Variant | Pass | Depth m mean | Wall p90 deg mean | Wall max deg (worst) | Ridge m mean | Cut m3/m | Ridge m3/m | Width m mean |")
        out.append("|---|---|---|---|---|---|---|---|---|")
        for vname, var in VARIANTS.items():
            rows = run(cap, var)
            for p in PASSES:
                a = np.array(rows[p])
                out.append(f"| {vname} | {p} | {a[:,0].mean():.3f} | {a[:,1].mean():.1f} | {a[:,2].max():.1f} | "
                           f"{a[:,3].mean():.3f} | {a[:,4].mean():.3f} | {a[:,5].mean():.3f} | {a[:,6].mean():.2f} |")
            print(vname, cap, "done", flush=True)
    text = "\n".join(out)
    print(text)
    return text


if __name__ == "__main__":
    main()
