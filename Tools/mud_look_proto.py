"""Mud look prototype (2026-10-17): can a client-only LOOK layer on today's rut field bring back the mud sheet's
MudRunner look? Offline, no game.

Same truck drive (two wheels 2.0 m apart, gentle arc, 8 m/s, N passes) through three models:

  SHEET  MUD_MudSheet.Stamp + Slump as coded (BOX + SOFT + GRADUAL, ridges H0 + H_SPEED x speed, front push,
         texture drag DRAG / DRAG_MAX / DRAG_LIM skipped, dirt where touched TOUCH + DIRT_NOISE), 0.15 m cells.
         Height = mud top relative to the untouched mud top. The wheel rides the hard ground (sheet collider was
         CharCollide only), so sink = 0.
  BUILT  today's felt rut field (Build 3, ebe4527), via Tools/scm_lite_proto.py on CarveFloor's maths: RUT_R 0.3,
         RUT_SHARE 0.4, relax tan 40 x 4, ridges B 0.4 / GAP 0.2 / W 0.5 / MAX 0.08 (add, cut clears ridge), step
         0.08, cap 0.35 (heavy truck). Drawn on the 0.25 m patch grid: y = floor where cut, else ridge. Soil shows
         where the floor is deeper than FIELD_SOIL_D 0.03.
  LOOK   proposed: drawn = a function of BUILT's floor + ridge only (no new data, no net, J2-safe), on a finer
         mesh near the camera (0.125 m; also run at 0.25 m). Per vertex: depth D (bilinear from the floor), track
         centre = vertices at >= 90 % of the local max depth, lat = distance to the nearest centre, Dc = depth
         there. Profile = the sheet's BOX + SOFT groove (half LOOK_HALF) to Dc, ridge band from the groove wall to
         the felt ridge's outer edge with the sheet's RIDGE_TOP shape, height min(HMAX, LOOK_RIDGE_K x Dc).
         drawn = max(look, felt) outside the tyre strip (nothing floats over drawn ground there); inside the tyre
         strip drawn = look. Dirt where touched (D > 0.01 or ridge > 0.01). Texture drag along the track tangent.

    python Tools/mud_look_proto.py   -> docs/superpowers/results/2026-10-17-mud-look-proto.md + img/*.png
"""
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import scm_lite_proto as scm  # noqa: E402

import matplotlib  # noqa: E402
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_MD = os.path.join(ROOT, "docs", "superpowers", "results", "2026-10-17-mud-look-proto.md")
OUT_IMG = os.path.join(ROOT, "docs", "superpowers", "results", "img")

AREA = 16.0
SPEED = 8.0
TRACK_GAP = 2.0          # wheel track centres (ASSUMED, Ural-size truck)
TYRE_R = 0.55            # ASSUMED wheel radius -> sheet half = 0.38 x r = 0.21
PATH_LEN = 12.0
PATH_R = 25.0            # arc radius (m)

# ---- sheet constants (MUD_MudSheet.c, VERIFIED-CODE)
S_CELL = 0.15
HALF = 0.18
BAND = 0.32
NOSE = 0.2
FRONT_LEN = 0.22
HALF_PER_R, HALF_MIN, HALF_MAX = 0.38, 0.11, 0.28
RIDGE_TOP = 0.35
W_FIRST = 0.55
SOFT_WALL = 0.35
DIRT_NOISE = 0.04
DRAG_TOUCH = 0.03
H0, H_SPEED, HMAX = 0.05, 0.012, 0.28
DRAG, DRAG_MAX, DRAG_LIM = 0.5, 0.3, 0.06
PRESS_STEP = 0.05
REPOSE, SLUMP = 1.3, 0.25
THICK = 0.15
TOUCH = 0.01

# ---- built (MUD_VoxelProbe.c ebe4527, VERIFIED-CODE) + prefab
RUT_R, RUT_SHARE, RELAX_DEG, RELAX_IT = 0.3, 0.4, 40.0, 4
RIDGE_B, RIDGE_GAP, RIDGE_W, RIDGE_MAX = 0.4, 0.2, 0.5, 0.08
STEP, CAP = 0.08, 0.35
SOIL_D = 0.03

# ---- proposed look (choices, tunable)
LOOK_HALF = 0.21          # drawn tyre half width (= sheet half for this tyre)
LOOK_RIDGE_K = 0.35       # look ridge height = K x centre depth, capped HMAX (0.35 x 0.26 = 0.09 = the sheet's 3-pass ridge)
LOOK_LIFT = 0.02          # drawn groove floor this far ABOVE the felt floor (tyre looks a little sunk, never floats)
LOOK_BAND_OUT = LOOK_HALF + BAND * LOOK_HALF / HALF   # the sheet's ridge band (0.37 m for this tyre) beside the groove


# ------------------------------------------------------------------------------------------------ path
def path_points(step):
    """Centre line: start (2.5, 5) heading 15 deg, arc PATH_R. Returns list of (x, z, dirx, dirz)."""
    pts = []
    th0 = math.radians(15)
    n = int(PATH_LEN / step)
    for i in range(n + 1):
        s = i * step
        th = th0 + s / PATH_R
        x = 2.5 + PATH_R * (math.sin(th) - math.sin(th0))
        z = 5.0 - PATH_R * (math.cos(th) - math.cos(th0))
        pts.append((x, z, math.cos(th), math.sin(th)))
    return pts


def wheel_points(step, side):
    out = []
    for x, z, dx, dz in path_points(step):
        nx, nz = -dz, dx
        out.append((x + nx * side * TRACK_GAP / 2, z + nz * side * TRACK_GAP / 2))
    return out


def mid_section():
    """Mid-track centre point + direction (for cross-sections)."""
    pts = path_points(0.05)
    x, z, dx, dz = pts[len(pts) // 2]
    return x, z, dx, dz


# ------------------------------------------------------------------------------------------------ sheet
def hash01(i, k):
    h = (i * 73856093) ^ (k * 19349663)
    h = (h ^ (h >> 13)) * 1274126177
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0


def sheet_run(passes):
    n = int(AREA / S_CELL) + 1
    D = np.full((n, n), THICK)          # mud depth over the hard ground [k, i]
    OU = np.zeros((n, n))
    OV = np.zeros((n, n))
    half0 = min(HALF_MAX, max(HALF_MIN, HALF_PER_R * TYRE_R))
    h = min(HMAX, H0 + H_SPEED * SPEED)
    stamp_len = SPEED / 60.0             # one stamp per frame at 60 fps
    for _ in range(passes):
        Dstart = D.copy()
        for side in (-1, 1):
            wp = wheel_points(stamp_len, side)
            for (ax, az), (bx, bz) in zip(wp[:-1], wp[1:]):
                stamp(D, OU, OV, Dstart, ax, az, bx, bz, half0, h, n)
        slump(D, n, 40)
    dirt = np.zeros((n, n))
    for k in range(n):
        for i in range(n):
            moved = abs(D[k, i] - THICK) > TOUCH + hash01(i, k) * DIRT_NOISE
            dragged = math.hypot(OU[k, i], OV[k, i]) > DRAG_TOUCH
            dirt[k, i] = 1.0 if (moved or dragged) else 0.0
    return dict(Y=D - THICK, dirt=dirt, ou=OU, ov=OV, cell=S_CELL, felt=None)


def stamp(D, OU, OV, Dstart, ax, az, bx, bz, half, h, n):
    wi, wk = int(round(bx / S_CELL)), int(round(bz / S_CELL))
    if 0 <= wi < n and 0 <= wk < n:
        pressed = min(1.0, max(0.0, (THICK - Dstart[wk, wi]) / THICK))
        half = half * (W_FIRST + (1 - W_FIRST) * pressed)
    band = BAND * half / HALF
    dx, dz = bx - ax, bz - az
    ln = math.hypot(dx, dz)
    dx, dz = dx / ln, dz / ln
    sx, sz = dz, -dx
    reach = half + band
    nose = ln + NOSE
    far = nose + FRONT_LEN
    ex, ez = ax + dx * far, az + dz * far
    i0 = max(1, int(math.floor((min(ax, ex) - reach) / S_CELL)))
    i1 = min(n - 2, int(math.ceil((max(ax, ex) + reach) / S_CELL)))
    k0 = max(1, int(math.floor((min(az, ez) - reach) / S_CELL)))
    k1 = min(n - 2, int(math.ceil((max(az, ez) + reach) / S_CELL)))
    drag = DRAG * ln
    for k in range(k0, k1 + 1):
        for i in range(i0, i1 + 1):
            px, pz = i * S_CELL - ax, k * S_CELL - az
            t = px * dx + pz * dz
            if t < 0 or t > far:
                continue
            latv = px * sx + pz * sz
            lat = abs(latv)
            if lat >= reach or (t > nose and lat >= half):
                continue
            d0 = Dstart[k, i]
            under = t <= nose and lat < half
            target = 0.0
            if under:
                inw = half * SOFT_WALL
                fs = half - lat
                pf = 0.5 * (1 - math.cos(math.pi * fs / inw)) if fs < inw else 1.0
                D[k, i] = min(D[k, i], max(0.0, min(d0, THICK) - PRESS_STEP * pf))
                du, dv = dx * drag, dz * drag
            else:
                if t > nose:
                    w = math.cos((t - nose) / FRONT_LEN * math.pi * 0.5)
                    hh = h * 0.8
                    pdx, pdz = dx, dz
                else:
                    x = (lat - half) / band
                    w = 1.0
                    if x > RIDGE_TOP:
                        w = 0.5 * (1 + math.cos(math.pi * (x - RIDGE_TOP) / (1 - RIDGE_TOP)))
                    else:
                        w = math.sin(0.5 * math.pi * x / RIDGE_TOP)
                    hh = h
                    sgn = 1 if latv >= 0 else -1
                    pdx, pdz = sx * sgn, sz * sgn
                target = THICK + hh * w
                target = min(target, max(d0, THICK) + hh * w * PRESS_STEP / THICK)
                du, dv = pdx * drag * 0.5 * w, pdz * drag * 0.5 * w
            if target > D[k, i]:
                D[k, i] = target
            ou, ov = OU[k, i] + du, OV[k, i] + dv
            ol = math.hypot(ou, ov)
            if ol > DRAG_MAX:
                ou, ov = ou * DRAG_MAX / ol, ov * DRAG_MAX / ol
            OU[k, i], OV[k, i] = ou, ov


def slump(D, n, iters):
    step = REPOSE * S_CELL
    for _ in range(iters):
        T = np.zeros_like(D)
        moved = False
        for k in range(1, n - 1):
            for i in range(1, n - 1):
                da = D[k, i]
                if da <= 0:
                    continue
                give = 0.0
                for nk, ni in ((k, i + 1), (k, i - 1), (k + 1, i), (k - 1, i)):
                    drop = da - D[nk, ni] - step
                    if drop <= 0:
                        continue
                    f = min(drop * SLUMP * 0.5, da * 0.25)
                    if f < 0.0002:
                        continue
                    T[nk, ni] += f
                    give += f
                    moved = True
                T[k, i] -= give
        D += T
        if not moved:
            break


# ------------------------------------------------------------------------------------------------ built
def built_run(passes):
    n = int(AREA / scm.FCELL)
    scm.N = n
    F = np.zeros((n, n))
    R = np.zeros((n, n))
    for _ in range(passes):
        for side in (-1, 1):
            wp = wheel_points(0.5, side)
            segs = list(zip(wp[:-1], wp[1:]))
            keys, maxcut = scm.carve(F, R, segs, STEP, CAP, RUT_R, share=RUT_SHARE, rclr=True)
            scm.relax(F, keys, RELAX_DEG, RELAX_IT)
            scm.ridge(R, F, segs, RUT_R, RIDGE_W, RIDGE_B, maxcut, RIDGE_MAX, gap=RIDGE_GAP, vol_r=RUT_R, blend="add")
    Y = np.where(F < 0, F, R)
    dirt = (F < -SOIL_D).astype(float)
    return dict(Y=Y, dirt=dirt, ou=np.zeros_like(Y), ov=np.zeros_like(Y), cell=scm.FCELL, felt=Y.copy(), F=F, R=R)


# ------------------------------------------------------------------------------------------------ sampling
def tri_sample(H, cell, x, z):
    """Patch-style triangle interpolation of grid H [k, i] at points x, z (arrays). tl-br split."""
    n = H.shape[0]
    gx, gz = x / cell, z / cell
    i = np.clip(np.floor(gx).astype(int), 0, n - 2)
    k = np.clip(np.floor(gz).astype(int), 0, n - 2)
    tx, tz = gx - i, gz - k
    h00, h10, h01, h11 = H[k, i], H[k, i + 1], H[k + 1, i], H[k + 1, i + 1]
    a = h00 + (h10 - h00) * tx + (h11 - h10) * tz
    b = h00 + (h11 - h01) * tx + (h01 - h00) * tz
    return np.where(tx >= tz, a, b)


# ------------------------------------------------------------------------------------------------ look
def look_run(built, cell):
    n = int(round(AREA / cell)) + 1
    xs = np.arange(n) * cell
    X, Z = np.meshgrid(xs, xs)
    F, R = built["F"], built["R"]
    Dep = np.maximum(0.0, -tri_sample(np.minimum(F, 0), scm.FCELL, X, Z))
    Rid = tri_sample(R, scm.FCELL, X, Z)
    felt = np.where(Dep > 0, -Dep, Rid)
    # track centre: >= 90 % of the local max depth (window 5 x 5 fine samples, at least 0.5 m)
    win = max(2, int(round(0.25 / cell)))
    loc = np.zeros_like(Dep)
    for dk in range(-win, win + 1):
        for di in range(-win, win + 1):
            loc = np.maximum(loc, np.roll(np.roll(Dep, dk, 0), di, 1))
    centre = (Dep > 0.02) & (Dep >= 0.9 * loc)
    cpts = np.stack([X[centre], Z[centre]], 1)
    cdep = Dep[centre]
    # tangent per centre point (PCA of centre points within 0.6 m)
    tang = np.zeros_like(cpts)
    for j, p in enumerate(cpts):
        m = np.sum((cpts - p) ** 2, 1) < 0.36
        q = cpts[m] - cpts[m].mean(0)
        if len(q) >= 3:
            _, _, vt = np.linalg.svd(q, full_matrices=False)
            t = vt[0]
        else:
            t = np.array([1.0, 0.0])
        if t[0] < 0:
            t = -t
        tang[j] = t
    P = np.stack([X.ravel(), Z.ravel()], 1)
    lat = np.full(P.shape[0], 99.0)
    near = np.zeros(P.shape[0], int)
    for s in range(0, P.shape[0], 4096):
        d2 = np.sum((P[s:s + 4096, None, :] - cpts[None, :, :]) ** 2, 2)
        j = np.argmin(d2, 1)
        near[s:s + 4096] = j
        lat[s:s + 4096] = np.sqrt(d2[np.arange(len(j)), j])
    lat = blur(lat.reshape(X.shape), max(1, int(round(0.25 / cell))))     # the centre set is ragged: smooth it
    Dc = blur(cdep[near].reshape(X.shape), max(1, int(round(0.25 / cell))))
    T = tang[near].reshape(X.shape + (2,))
    # side direction (away from the centre)
    toP = np.stack([X, Z], -1) - cpts[near].reshape(X.shape + (2,))
    sgn = np.sign(toP[..., 0] * -T[..., 1] + toP[..., 1] * T[..., 0])
    Sx, Sz = -T[..., 1] * sgn, T[..., 0] * sgn

    look = np.zeros_like(Dep)
    tyre = lat < LOOK_HALF
    inw = LOOK_HALF * SOFT_WALL
    fs = LOOK_HALF - lat
    pf = np.where(fs < inw, 0.5 * (1 - np.cos(np.pi * np.clip(fs, 0, None) / inw)), 1.0)
    groove = -(Dc - LOOK_LIFT) * pf
    band = LOOK_BAND_OUT - LOOK_HALF
    x = np.clip((lat - LOOK_HALF) / band, 0, 1)
    w = np.where(x > RIDGE_TOP, 0.5 * (1 + np.cos(np.pi * (x - RIDGE_TOP) / (1 - RIDGE_TOP))),
                 np.sin(0.5 * np.pi * x / RIDGE_TOP))
    hL = np.minimum(HMAX, LOOK_RIDGE_K * Dc)
    ridge = np.where((lat >= LOOK_HALF) & (lat < LOOK_BAND_OUT), hL * w, 0.0)
    has = Dc > 0.02
    look = np.where(tyre, groove, ridge)
    look = np.where(has, look, 0.0)
    Y = np.where(tyre & has, look, np.maximum(look, felt))
    dirt = ((Dep > 0.01) | (has & (lat < LOOK_BAND_OUT) & (ridge > 0.01))).astype(float)   # felt ridge stays grass
    ratio = np.clip(Dep / np.maximum(Dc, 1e-6), 0, 1)
    ou = np.where(tyre & has, T[..., 0] * DRAG_MAX * ratio, Sx * DRAG_MAX * 0.5 * w * has)
    ov = np.where(tyre & has, T[..., 1] * DRAG_MAX * ratio, Sz * DRAG_MAX * 0.5 * w * has)
    return dict(Y=Y, dirt=dirt, ou=ou, ov=ov, cell=cell, felt=felt)


def blur(A, r):
    """Box blur, radius r samples (edges clamped)."""
    P = np.pad(A, r, mode="edge")
    out = np.zeros_like(A)
    for dk in range(-r, r + 1):
        for di in range(-r, r + 1):
            out += P[r + dk:r + dk + A.shape[0], r + di:r + di + A.shape[1]]
    return out / (2 * r + 1) ** 2


# ------------------------------------------------------------------------------------------------ metrics
def cross(model, x0, z0, dx, dz, side):
    """Profile across one wheel track at mid-track: s in -1.3..1.3 m from that wheel's centre."""
    nx, nz = -dz, dx
    cx, cz = x0 + nx * side * TRACK_GAP / 2, z0 + nz * side * TRACK_GAP / 2
    s = np.arange(-1.3, 1.3001, 0.01)
    px, pz = cx + nx * s, cz + nz * s
    y = tri_sample(model["Y"], model["cell"], px, pz)
    f = tri_sample(model["felt"], model["cell"], px, pz) if model.get("felt") is not None else None
    return s, y, f


def metrics(model, name, x0, z0, dx, dz):
    rows = []
    for along in (-1.0, -0.5, 0.0, 0.5, 1.0):
        s, y, f = cross(model, x0 + dx * along, z0 + dz * along, dx, dz, 1)
        depth = -y.min()
        half_w = np.sum(y < -0.5 * depth) * 0.01
        flat_w = np.sum(y < -0.8 * depth) * 0.01
        ridge = y.max()
        slope = np.degrees(np.arctan(np.abs(np.diff(y)) / 0.01))
        wall = np.percentile(slope[(y[:-1] < -0.01) | (y[1:] < -0.01)], 95) if depth > 0.01 else 0.0
        over = under = tunder = 0.0
        if f is not None and name.startswith("LOOK"):
            over = float(np.max(y - f))
            outside = np.abs(s) >= LOOK_HALF
            under = float(np.max((f - y)[outside])) if np.any(outside) else 0.0
            tunder = float(np.max((f - y)[~outside])) if np.any(~outside) else 0.0
        rows.append((depth, half_w, flat_w, ridge, wall, over, under, tunder))
    r = np.array(rows).mean(0)
    s, y, _ = cross(model, x0, z0, dx, dz, 1)
    ds = float(np.sum(model["dirt"] > 0.5) * model["cell"] ** 2)
    return dict(name=name, depth=r[0], w50=r[1], w80=r[2], ridge=r[3], wall95=r[4], over=r[5], under=r[6], tunder=r[7],
                dirt_m2=ds, profile=(s, y))


def shape_distance(pa, pb):
    """rms of the two profiles each scaled to its own depth (shape, not size)."""
    ya = pa[1] / max(1e-6, -pa[1].min())
    yb = pb[1] / max(1e-6, -pb[1].min())
    return float(np.sqrt(np.mean((ya - yb) ** 2)))


# ------------------------------------------------------------------------------------------------ render
def texture(u, v):
    """Procedural ground texture value 0..1 (a few octaves of value noise + streaks), same for every model."""
    val = np.zeros_like(u)
    amp, tot = 1.0, 0.0
    for f in (2.0, 5.0, 13.0, 31.0):
        val += amp * (0.5 + 0.5 * np.sin(u * f * 1.7 + np.sin(v * f * 1.3) * 1.9) * np.cos(v * f * 2.1 + np.cos(u * f) * 1.3))
        tot += amp
        amp *= 0.55
    return val / tot


def vertex_normals(Y, cell):
    gz, gx = np.gradient(Y, cell)
    nrm = np.stack([-gx, np.ones_like(Y), -gz], -1)
    return nrm / np.linalg.norm(nrm, axis=-1, keepdims=True)


def render(model, x0, z0, dx, dz, w=7.0, h=4.0, px=0.02):
    """Top view of a w x h m window centred on mid-track, aligned with the track (track runs left -> right)."""
    us = np.arange(-w / 2, w / 2, px)
    vs = np.arange(-h / 2, h / 2, px)
    U, V = np.meshgrid(us, vs)
    nx, nz = -dz, dx
    X = x0 + dx * U + nx * V
    Z = z0 + dz * U + nz * V
    cell = model["cell"]
    N3 = vertex_normals(model["Y"], cell)
    nrm = np.stack([tri_sample(N3[..., c], cell, X, Z) for c in range(3)], -1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    dirt = tri_sample(model["dirt"], cell, X, Z) > 0.5
    ou = tri_sample(model["ou"], cell, X, Z)
    ov = tri_sample(model["ov"], cell, X, Z)
    sun = np.array([0.55, 0.62, -0.56])
    sun /= np.linalg.norm(sun)
    lam = np.clip(nrm @ sun, 0, 1) * 0.85 + 0.15
    t = texture(X + ou, Z + ov)
    grass = np.stack([0.30 + 0.12 * t, 0.42 + 0.15 * t, 0.18 + 0.06 * t], -1)
    mud = np.stack([0.33 + 0.16 * t, 0.24 + 0.11 * t, 0.15 + 0.07 * t], -1)
    col = np.where(dirt[..., None], mud, grass) * lam[..., None]
    return np.clip(col, 0, 1)


def mesh_view(ax, model, x0, z0, dx, dz, title):
    """Oblique 3D of the model's own mesh vertices in a 4 x 2.6 m window round one wheel track (shows the grid)."""
    cell = model["cell"]
    nx, nz = -dz, dx
    cx, cz = x0 + nx * TRACK_GAP / 2, z0 + nz * TRACK_GAP / 2
    n = model["Y"].shape[0]
    xs = np.arange(n) * cell
    i0, i1 = np.searchsorted(xs, cx - 2.2), np.searchsorted(xs, cx + 2.2)
    k0, k1 = np.searchsorted(xs, cz - 2.2), np.searchsorted(xs, cz + 2.2)
    Xg, Zg = np.meshgrid(xs[i0:i1], xs[k0:k1])
    Yg = model["Y"][k0:k1, i0:i1]
    N3 = vertex_normals(model["Y"], cell)[k0:k1, i0:i1]
    sun = np.array([0.55, 0.62, -0.56])
    sun /= np.linalg.norm(sun)
    lam = np.clip(N3 @ sun, 0, 1) * 0.85 + 0.15
    dirt = model["dirt"][k0:k1, i0:i1] > 0.5
    base = np.where(dirt[..., None], np.array([0.42, 0.30, 0.19]), np.array([0.36, 0.50, 0.22]))
    fc = np.clip(base * lam[..., None], 0, 1)
    fc = np.concatenate([fc, np.ones(fc.shape[:2] + (1,))], -1)
    ax.plot_surface(Xg, Zg, Yg, facecolors=fc, rstride=1, cstride=1, linewidth=0.15,
                    edgecolor=(0, 0, 0, 0.25), antialiased=False, shade=False)
    ax.set_box_aspect((float(np.ptp(Xg)), float(np.ptp(Zg)), 1.2))
    ax.set_zlim(-0.45, 0.35)
    ax.view_init(elev=28, azim=math.degrees(math.atan2(dz, dx)) + 200)
    ax.set_axis_off()
    ax.set_title(title, fontsize=9)


# ------------------------------------------------------------------------------------------------ main
def main():
    os.makedirs(OUT_IMG, exist_ok=True)
    x0, z0, dx, dz = mid_section()
    results = {}
    for passes in (1, 3):
        sheet = sheet_run(passes)
        built = built_run(passes)
        look = look_run(built, 0.125)
        look25 = look_run(built, 0.25)
        models = [("SHEET 0.15 m", sheet), ("BUILT 0.25 m", built), ("LOOK 0.125 m", look), ("LOOK 0.25 m", look25)]
        results[passes] = [(nm, m, metrics(m, nm, x0, z0, dx, dz)) for nm, m in models]

    # images (3 passes)
    ms = results[3]
    fig, axs = plt.subplots(len(ms), 1, figsize=(9, 4.3 * len(ms) / 1.15))
    for ax, (nm, m, _) in zip(axs, ms):
        ax.imshow(render(m, x0, z0, dx, dz), origin="lower", extent=(-3.5, 3.5, -2, 2))
        ax.set_title(nm + " - top view, 3 passes, low sun", fontsize=10)
        ax.set_xlabel("along the track (m)")
        ax.set_ylabel("across (m)")
    fig.tight_layout()
    fig.savefig(os.path.join(OUT_IMG, "2026-10-17-mud-look-top.png"), dpi=110)
    plt.close(fig)

    fig = plt.figure(figsize=(12, 9))
    for j, (nm, m, _) in enumerate(ms):
        ax = fig.add_subplot(2, 2, j + 1, projection="3d")
        mesh_view(ax, m, x0, z0, dx, dz, nm + " - its own mesh (one wheel track)")
    fig.tight_layout()
    fig.savefig(os.path.join(OUT_IMG, "2026-10-17-mud-look-mesh.png"), dpi=100)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(10, 4))
    for nm, m, mt in ms:
        s, y = mt["profile"]
        ax.plot(s, y, label=nm)
    s, _, f = cross(ms[2][1], x0, z0, dx, dz, 1)
    ax.plot(s, f, "k:", label="felt ground (collider) under LOOK")
    ax.axhline(0, color="grey", lw=0.5)
    ax.set_xlabel("across one wheel track (m)")
    ax.set_ylabel("height vs untouched (m)")
    ax.set_title("Cross-section at mid-track, 3 passes")
    ax.legend(fontsize=8)
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT_IMG, "2026-10-17-mud-look-cross.png"), dpi=110)
    plt.close(fig)

    lines = [table_md(results)]
    print("\n".join(lines))
    write_md(results)


def table_md(results):
    out = ["| Passes | Model | Depth m | Width at 50 % depth m | Width at 80 % depth m | Ridge m | Wall slope p95 deg | "
           "Shape vs SHEET (0 = same) | Dirt area m2 | Drawn over felt max m | Drawn under felt: outside / inside tyre strip m |",
           "|---|---|---|---|---|---|---|---|---|---|---|"]
    for passes, ms in results.items():
        sheet_prof = ms[0][2]["profile"]
        for nm, _, mt in ms:
            sd = shape_distance(sheet_prof, mt["profile"])
            ov = "%.3f" % mt["over"] if nm.startswith("LOOK") else "-"
            un = "%.3f / %.3f" % (mt["under"], mt["tunder"]) if nm.startswith("LOOK") else "-"
            out.append("| %d | %s | %.3f | %.2f | %.2f | %.3f | %.0f | %.2f | %.1f | %s | %s |" % (
                passes, nm, mt["depth"], mt["w50"], mt["w80"], mt["ridge"], mt["wall95"], sd, mt["dirt_m2"], ov, un))
    return "\n".join(out)


def write_md(results):
    tab = table_md(results)
    md = f"""# Mud look prototype - sheet vs built vs a look layer (2026-10-17, offline)

Tool: `python Tools/mud_look_proto.py` (models and every constant in its docstring). MEASURED offline on copied game
maths (sheet: MUD_MudSheet.Stamp / Slump; built: CarveFloor via Tools/scm_lite_proto.py at ebe4527 values). Truck,
tyre and speed are ASSUMED round numbers. Nothing in Scripts/ changed. Images are a stand-in renderer (procedural
texture, low sun, smooth vertex normals) - they compare SHAPE, mesh detail, dirt edge and drag, not the game's
materials.

![top views](img/2026-10-17-mud-look-top.png)
![meshes](img/2026-10-17-mud-look-mesh.png)
![cross-section](img/2026-10-17-mud-look-cross.png)

## Numbers (5 cross-sections at mid-track, one wheel track, averaged)
{tab}

Columns: depth = deepest drawn point; widths = across the track where the drawn surface is under 50 / 80 % of that
depth (80 % = how flat-bottomed the groove is); ridge = highest drawn point; wall slope = 95th percentile of the
drawn slope inside the rut; shape vs SHEET = rms of the two cross-sections each scaled to its own depth; dirt area
= mud-coloured area in the 16 x 16 m window; drawn over / under felt = how far the LOOK mesh sits above / below the
ground wheels and feet stand on (over = feet / tyre hidden in drawn mud; under = something would float over the drawn
ground - kept at 0 outside the tyre strip by design).

## Reading (3 passes, heavy truck)
- **Today's felt rut is not the sheet look, by the numbers.** BUILT is twice as wide (0.46 m at half depth vs the
  sheet's 0.22), softer walls (44 vs 56 deg), ridges stay grass and sit 0.5-1.0 m out, shape distance 0.35.
- **A look layer at 0.125 m gets close.** LOOK 0.125 m: groove 0.22 m wide at half depth (sheet 0.22), walls 57 deg
  (sheet 56), dirt ridge 0.09 m right at the groove wall (sheet 0.095), shape distance 0.19 (built 0.35). It is
  deeper than the sheet (0.20 vs 0.15) because it follows the felt depth - LOOK_RIDGE_K / a depth scale tune that.
- **On today's 0.25 m mesh it does NOT work.** LOOK 0.25 m: the 0.42 m groove falls between vertices - spiky
  mesh, depth 0.02 m, shape distance 0.73. A finer mesh near the camera is required, not optional.
- **The price: drawn mud over the felt ground.** The felt bowl is wide and the drawn groove narrow, so on the bowl
  walls the drawn ridge sits up to ~0.19 m over the ground feet and wheels stand on: a soldier there has their feet in
  drawn mud (MudRunner does the same: drawn mud over hard ground). Nothing floats outside the tyre strip (0.000);
  inside it the drawn floor sits 0.02-0.06 m over the felt floor (the tyre looks a little sunk, never floats).
- **The felt ridge stays visible** as a low grass hump 0.5-1.0 m out (it is in the collider, so it must be drawn).
  One ridge instead of two would need the felt ridge moved next to the rut (felt change, MP robot) or switched off
  while the look layer is on - a choice to make.
- Bow wave / front push: the sheet's push ahead of the tyre is transient (only while the truck is there) and is not
  derivable from the floor - not in the look layer.
- 1 pass: the look follows the felt rut's first big step (R4 share 0.4), so it starts deeper than the sheet's
  gradual 0.05 m press.

## What it would take in the game (ESTIMATE / INFERENCE - not measured)
- Client only, no op / net / snapshot change: the look is computed from the rut floor + ridge every client already
  has (J2-safe, late joiners get it for free). A DS never builds it.
- Near the camera only (e.g. rutted patches within ~20 m): their MESH at 0.125 m = 65 x 65 = 4225 vertices / 8192
  triangles per 8 m patch (4x today's 33 x 33). The collider stays 0.25 m. Mesh build ~4x today's grid mesh time
  plus the per-vertex look maths (centre search, blur) - must be measured in a PERF session, never in Workbench.
- Texture drag = per-vertex UV offsets along the track (allowed: one UV set, clamped 0..1 per patch), dirt = the
  existing soil section with a lower threshold under the look.
- Open: patch edges (the look needs the floor 0.5 m past the patch), crossing tracks (centre search picks the
  deepest), how far away the switch to the coarse mesh shows (LOD pop).
"""
    with open(OUT_MD, "w") as f:
        f.write(md)


if __name__ == "__main__":
    main()
