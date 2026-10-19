"""Offline experiments on the synced mud sheet maths (2026-10-19, research/2026-10-19-mud-sheet-script-cost-research.md
sec 8, results/2026-10-19-sheet-sync-maths-offline.md). Reuses Tools/mud_sheet_sync_proto.py (server ops, felt rut
field) and Tools/mud_look_proto.py (constants, metrics). Variants: sync cell size, slump region / passes, drag limit.
Prints a markdown table: look metrics vs the reference (game today) + work counts (cells touched per variant).

    python Tools/mud_sheet_cost_exp.py [passes5 | straight]
"""
import math
import os
import sys
from unittest import mock
try:
    import matplotlib  # noqa: F401  (the protos import it at module level; not needed here)
except ImportError:
    for m in ("matplotlib", "matplotlib.pyplot", "matplotlib.animation"):
        sys.modules[m] = mock.MagicMock()
TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import numpy as np
import mud_sheet_sync_proto as P
import mud_look_proto as L
import scm_lite_proto as scm

THICK = L.THICK
FINE = L.S_CELL
AREA = L.AREA


class Grid:
    def __init__(self, cell):
        self.cell = cell
        self.n = int(AREA / cell + 1e-9) + 1
        xs = np.arange(self.n) * cell
        self.GX, self.GZ = np.meshgrid(xs, xs)


def stamp(g, D, OU, OV, Pst, ax, az, bx, bz, speed, cnt, drag_mode, changed):
    """MUD_MudSheet.Stamp (BOX + SOFT + GRADUAL) at grid g. drag_mode: 'none' | 'lim' | 'pf' | 'pf_lim'."""
    c = g.cell
    n = g.n
    half = P.HALF0
    wi, wk = int(round(bx / c)), int(round(bz / c))
    if 0 <= wi < n and 0 <= wk < n:
        pressed = min(1.0, max(0.0, (THICK - Pst[wk, wi]) / THICK))
        half = half * (L.W_FIRST + (1 - L.W_FIRST) * pressed)
    band = L.BAND * half / L.HALF
    dx, dz = bx - ax, bz - az
    ln = math.hypot(dx, dz)
    if ln < 1e-6:
        return
    dx, dz = dx / ln, dz / ln
    sx, sz = dz, -dx
    reach = half + band
    nose = ln + L.NOSE
    far = nose + L.FRONT_LEN
    ex, ez = ax + dx * far, az + dz * far
    i0 = max(1, int(math.floor((min(ax, ex) - reach) / c)))
    i1 = min(n - 2, int(math.ceil((max(ax, ex) + reach) / c)))
    k0 = max(1, int(math.floor((min(az, ez) - reach) / c)))
    k1 = min(n - 2, int(math.ceil((max(az, ez) + reach) / c)))
    if i1 < i0 or k1 < k0:
        return
    h = min(L.HMAX, L.H0 + L.H_SPEED * speed)
    drag = L.DRAG * ln
    sl = (slice(k0, k1 + 1), slice(i0, i1 + 1))
    px, pz = g.GX[sl] - ax, g.GZ[sl] - az
    t = px * dx + pz * dz
    latv = px * sx + pz * sz
    lat = np.abs(latv)
    ok = (t >= 0) & (t <= far) & (lat < reach) & ~((t > nose) & (lat >= half))
    cnt["box"] += ok.size
    cnt["hit"] += int(ok.sum())
    if not ok.any():
        return
    d0 = Pst[sl]
    Dv = D[sl].copy()
    under = ok & (t <= nose) & (lat < half)
    inw = half * L.SOFT_WALL
    fs = half - lat
    pf = np.where(fs < inw, 0.5 * (1 - np.cos(np.pi * np.clip(fs, 0, None) / inw)), 1.0)
    pressv = np.maximum(0.0, np.minimum(d0, THICK) - L.PRESS_STEP * pf)
    Dv = np.where(under, np.minimum(Dv, pressv), Dv)
    push = ok & ~under
    front = t > nose
    x = (lat - half) / band
    w_side = np.where(x > L.RIDGE_TOP, 0.5 * (1 + np.cos(np.pi * (x - L.RIDGE_TOP) / (1 - L.RIDGE_TOP))),
                      np.sin(0.5 * np.pi * np.clip(x, 0, None) / L.RIDGE_TOP))
    w_front = np.cos(np.clip((t - nose) / L.FRONT_LEN, 0, 1) * np.pi * 0.5)
    w = np.where(front, w_front, w_side)
    hh = np.where(front, h * 0.8, h)
    target = np.minimum(THICK + hh * w, np.maximum(d0, THICK) + hh * w * L.PRESS_STEP / THICK)
    Dv = np.where(push & (target > Dv), target, Dv)
    D[sl] = Dv
    changed[sl] |= ok
    sgn = np.where(latv >= 0, 1.0, -1.0)
    pdx = np.where(front, dx, sx * sgn)
    pdz = np.where(front, dz, sz * sgn)
    pscale = pf if drag_mode in ("pf", "pf_lim") else 1.0
    du = np.where(under, dx * drag * pscale, np.where(push, pdx * drag * 0.5 * w, 0.0))
    dv = np.where(under, dz * drag * pscale, np.where(push, pdz * drag * 0.5 * w, 0.0))
    ou, ov = OU[sl] + du, OV[sl] + dv
    ol = np.hypot(ou, ov)
    sc = np.where(ol > L.DRAG_MAX, L.DRAG_MAX / np.maximum(ol, 1e-9), 1.0)
    OU[sl], OV[sl] = ou * sc, ov * sc
    if drag_mode in ("lim", "pf_lim"):
        limit_drag(OU, OV, i0, i1, k0, k1, cnt)
    elif drag_mode == "strip":
        m = np.zeros(OU.shape, bool)
        m[sl] = ok
        limit_drag_mask(OU, OV, dilate(m, 1), cnt)


def limit_drag(OU, OV, i0, i1, k0, k1, cnt):
    """MUD_MudSync.LimitDrag: 2 sweeps over the stamp box, each cell clamped to within DRAG_LIM of its 4 neighbours."""
    lim = L.DRAG_LIM
    n = OU.shape[0]
    for sweep in range(2):
        ks = range(k0, k1 + 1) if sweep == 0 else range(k1, k0 - 1, -1)
        for k in ks:
            iis = range(i0, i1 + 1) if sweep == 0 else range(i1, i0 - 1, -1)
            for i in iis:
                cnt["drag"] += 1
                ou, ov = OU[k, i], OV[k, i]
                for ni, nk in ((i - 1, k), (i + 1, k), (i, k - 1), (i, k + 1)):
                    if 0 <= ni < n and 0 <= nk < n:
                        au, av = OU[nk, ni], OV[nk, ni]
                        ou = min(max(ou, au - lim), au + lim)
                        ov = min(max(ov, av - lim), av + lim)
                OU[k, i], OV[k, i] = ou, ov


def limit_drag_mask(OU, OV, mask, cnt):
    lim = L.DRAG_LIM
    n = OU.shape[0]
    ks, iis = np.nonzero(mask)
    order = list(zip(ks, iis))
    for sweep in range(2):
        seq = order if sweep == 0 else order[::-1]
        for k, i in seq:
            cnt["drag"] += 1
            ou, ov = OU[k, i], OV[k, i]
            for ni, nk in ((i - 1, k), (i + 1, k), (i, k - 1), (i, k + 1)):
                if 0 <= ni < n and 0 <= nk < n:
                    au, av = OU[nk, ni], OV[nk, ni]
                    ou = min(max(ou, au - lim), au + lim)
                    ov = min(max(ov, av - lim), av + lim)
            OU[k, i], OV[k, i] = ou, ov


def slump_region(g, D, G, iters, mask, slump_k, cnt):
    """MUD_MudSync slump (all flows from the old depths, then added) over cells in mask (bool grid)."""
    step = L.REPOSE * g.cell
    for _ in range(iters):
        T = np.zeros_like(D)
        S = G + D
        for dk, di in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            Sn = np.roll(np.roll(S, -dk, 0), -di, 1)
            Mn = np.roll(np.roll(mask, -dk, 0), -di, 1)
            drop = S - Sn - step
            f = np.minimum(drop * slump_k * 0.5, D * 0.25)
            m = mask & Mn & (D > 0) & (drop > 0) & (f >= 0.0002)
            m[0, :] = m[-1, :] = m[:, 0] = m[:, -1] = False
            f = np.where(m, f, 0.0)
            T -= f
            T += np.roll(np.roll(f, dk, 0), di, 1)
        D += T
        cnt["slump"] += int(mask.sum())


def dilate(m, r):
    out = m.copy()
    for _ in range(r):
        o = out.copy()
        o[1:, :] |= out[:-1, :]
        o[:-1, :] |= out[1:, :]
        o[:, 1:] |= out[:, :-1]
        o[:, :-1] |= out[:, 1:]
        out = o
    return out


def bbox_mask(changed, pad):
    m = np.zeros_like(changed)
    ks, iis = np.nonzero(changed)
    if len(ks) == 0:
        return m
    m[max(0, ks.min() - pad):ks.max() + pad + 1, max(0, iis.min() - pad):iis.max() + pad + 1] = True
    return m


def run(cell=FINE, slump_mode="box", iters=3, slump_k=L.SLUMP, drag_mode="lim", seg_sub=1, near_r=None):
    g = Grid(cell)
    felt = P.Felt()
    D = np.full((g.n, g.n), THICK)
    OU = np.zeros_like(D)
    OV = np.zeros_like(D)
    Pst = D.copy()
    pas = -1
    cnt = dict(box=0, hit=0, drag=0, slump=0, ops=0, stamps=0)
    for op in P.server_ops():
        if op["pas"] != pas:
            pas = op["pas"]
            Pst = D.copy()
        felt.apply(op)
        Gc = L.tri_sample(np.where(felt.F < 0, felt.F, felt.R), scm.FCELL, g.GX, g.GZ)
        changed = np.zeros(D.shape, bool)
        for side in (-1, 1):
            for (a, b) in op["segs"][side]:
                for j in range(seg_sub):
                    f0, f1 = j / seg_sub, (j + 1) / seg_sub
                    stamp(g, D, OU, OV, Pst, a[0] + (b[0] - a[0]) * f0, a[1] + (b[1] - a[1]) * f0,
                          a[0] + (b[0] - a[0]) * f1, a[1] + (b[1] - a[1]) * f1, op["speed"], cnt, drag_mode, changed)
                    cnt["stamps"] += 1
        cnt["ops"] += 1
        if drag_mode == "perop":
            limit_drag_mask(OU, OV, dilate(changed, 1), cnt)
        if slump_mode == "box":
            mask = bbox_mask(changed, 1)
        elif slump_mode == "near":
            mask = dilate(changed, iters if near_r is None else near_r)
        else:
            mask = np.ones(D.shape, bool)
        if iters > 0:
            slump_region(g, D, Gc, iters, mask, slump_k, cnt)
    # drawn on the fine grid
    fine = Grid(FINE)
    Gf = L.tri_sample(np.where(felt.F < 0, felt.F, felt.R), scm.FCELL, fine.GX, fine.GZ)
    if cell != FINE:
        Df = bilinear(D, cell, fine.GX, fine.GZ)
        OUf = bilinear(OU, cell, fine.GX, fine.GZ)
        OVf = bilinear(OV, cell, fine.GX, fine.GZ)
    else:
        Df, OUf, OVf = D, OU, OV
    model = dict(Y=Gf + Df - THICK, dirt=P.dirt_of(Df, OUf, OVf), ou=OUf, ov=OVf, cell=FINE, felt=None)
    return model, cnt, (D, OU, OV)


def bilinear(H, cell, x, z):
    n = H.shape[0]
    gx, gz = x / cell, z / cell
    i = np.clip(np.floor(gx).astype(int), 0, n - 2)
    k = np.clip(np.floor(gz).astype(int), 0, n - 2)
    tx, tz = gx - i, gz - k
    return (H[k, i] * (1 - tx) * (1 - tz) + H[k, i + 1] * tx * (1 - tz) + H[k + 1, i] * (1 - tx) * tz
            + H[k + 1, i + 1] * tx * tz)


def stretch(OU, OV, cell, touched):
    """Texture stretch between neighbours: |delta drag| / cell, over touched cells (p50 / p99 / max)."""
    d = []
    for A in (OU, OV):
        d.append(np.abs(np.diff(A, axis=1))[touched[:, 1:] | touched[:, :-1]])
        d.append(np.abs(np.diff(A, axis=0))[touched[1:, :] | touched[:-1, :]])
    v = np.concatenate(d) / cell
    return np.percentile(v, 50), np.percentile(v, 99), v.max()


def main():
    x0, z0, dx, dz = L.mid_section()
    fine = Grid(FINE)
    win = (np.abs((fine.GX - x0) * dx + (fine.GZ - z0) * dz) < 4) & (np.abs((fine.GX - x0) * -dz + (fine.GZ - z0) * dx) < 2.5)
    ref, rc, (rD, rOU, rOV) = run()
    rm = L.metrics(ref, "ref", x0, z0, dx, dz)
    touched = (np.abs(rD - THICK) > 1e-6) | (np.hypot(rOU, rOV) > 1e-6)
    variants = [
        ("REF 0.15 m, box slump x3, LimitDrag (game today)", {}),
        ("drag: LimitDrag over stamped cells +1 only (per stamp)", dict(drag_mode="strip")),
        ("drag: LimitDrag once per op over its stamped cells +1", dict(drag_mode="perop")),
        ("slump: 1 pass, factor 0.5", dict(iters=1, slump_k=0.5)),
        ("slump: 1 pass, factor 0.7", dict(iters=1, slump_k=0.7)),
        ("slump: 1 pass f0.6, cells within 1 of stamps", dict(iters=1, slump_k=0.6, slump_mode="near", near_r=1)),
        ("combo: per-op drag + 1 pass f0.6 near 1", dict(drag_mode="perop", iters=1, slump_k=0.6, slump_mode="near", near_r=1)),
    ]
    variants_more = [
        ("REF 0.15 m, box slump x3, LimitDrag (game today)", {}),
        ("drag: no LimitDrag", dict(drag_mode="none")),
        ("drag: press drag x soft-wall pf, no LimitDrag", dict(drag_mode="pf")),
        ("slump: only cells within 3 of this op's stamps", dict(slump_mode="near")),
        ("slump: 1 pass (same factor)", dict(iters=1)),
        ("slump: 1 pass, factor 0.6", dict(iters=1, slump_k=0.6)),
        ("slump: off", dict(iters=0)),
        ("sync cell 0.20 m -> drawn 0.15", dict(cell=0.20)),
        ("sync cell 0.25 m -> drawn 0.15", dict(cell=0.25)),
        ("sync cell 0.30 m -> drawn 0.15", dict(cell=0.30)),
        ("sync cell 0.30 m + no LimitDrag + near slump", dict(cell=0.30, drag_mode="pf", slump_mode="near")),
    ]
    variants = variants + variants_more[1:]
    print("| variant | rms vs ref (window) m | max m | depth m | ridge m | w50 m | wall95 deg | shape dist | dirt m2 | stretch p50/p99/max | stamp box cells | hit | drag cells | slump cell-passes |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for name, kw in variants:
        if not kw:
            mod, cnt, (D, OU, OV) = ref, rc, (rD, rOU, rOV)
        else:
            mod, cnt, (D, OU, OV) = run(**kw)
        mt = L.metrics(mod, name, x0, z0, dx, dz)
        e = (mod["Y"] - ref["Y"])[win]
        cell = kw.get("cell", FINE)
        if cell == FINE:
            st = stretch(OU, OV, FINE, touched)
        else:
            st = stretch(mod["ou"], mod["ov"], FINE, touched)
        print("| %s | %.4f | %.3f | %.3f | %.3f | %.2f | %.0f | %.3f | %.1f | %.2f / %.2f / %.2f | %d | %d | %d | %d |" % (
            name, float(np.sqrt(np.mean(e ** 2))), float(np.abs(e).max()), mt["depth"], mt["ridge"], mt["w50"],
            mt["wall95"], L.shape_distance(rm["profile"], mt["profile"]), mt["dirt_m2"], st[0], st[1], st[2],
            cnt["box"], cnt["hit"], cnt["drag"], cnt["slump"]))
    print("ops", rc["ops"], "stamps", rc["stamps"])


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "passes5":
        P.PASSES = 5
    elif len(sys.argv) > 1 and sys.argv[1] == "straight":
        L.PATH_R = 1000.0
    main()
