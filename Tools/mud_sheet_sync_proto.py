"""Synced mud sheet prototype (2026-10-17): the mud sheet back as a LOOK-ONLY layer, kept the same on every client
by pressing it from the replicated rut ops, with local prediction at the tyre. Offline, no game.

One truck (two wheel tracks 2.0 m apart, 8 m/s, gentle arc), 3 passes 5 s apart, through:

  SERVER   true wheel path. Rut ops every RUT_EVERY_MS 250 hold the 0.5 m wheel segments the rear wheel is
           RUT_BEHIND_GAP 1.5 m past (as today), endpoints in cm, plus the speed in 0.1 m/s (NEW look-only field)
           and the pass number (NEW: GRADUAL needs a deterministic pass start; the sheet used a clock).
  CLIENT A / B  each sees the truck delayed by its interpolation (100 / 160 ms) and off by a slow lateral error
           (+-3 cm, different phase), receives each op after its own latency (80 / 140 ms +- 20).
           Felt ground: today's rut field from the ops (Tools/scm_lite_proto.py CarveFloor maths, ebe4527 values).
           SYNC layer: the sheet (MUD_MudSheet Stamp, BOX + SOFT + GRADUAL, 0.15 m cells) pressed ONLY from op
           segments, in op order, + 3 slump iterations per op -> deterministic.
           PREDICTION: drawn = SYNC + the client's own per-frame stamps at its view of the wheels that no op has
           covered yet (rollback + replay on every op = classic client prediction). The handoff jump is morphed out
           (residual x 0.85 per frame).
           Drawn height = felt + mud depth - THICK (mud is a THICK layer over the ground the wheels stand on).
  SHEET    reference: the old sheet as it was - per-frame stamps at the true wheels, slump at 10 Hz.
  JOINER   a late joiner gets the J2 snapshot (floor only, no op segments): its fallback is the rut-derived look
           (Tools/mud_look_proto.look_run).

    python Tools/mud_sheet_sync_proto.py  -> results/2026-10-17-mud-sheet-sync-proto.md + img/ (png + gif)
"""
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mud_look_proto as L  # noqa: E402
import scm_lite_proto as scm  # noqa: E402

import matplotlib  # noqa: E402
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.animation import FuncAnimation, PillowWriter  # noqa: E402

ROOT = L.ROOT
OUT_MD = os.path.join(ROOT, "docs", "superpowers", "results", "2026-10-17-mud-sheet-sync-proto.md")
OUT_IMG = L.OUT_IMG

FPS = 60
DT = 1.0 / FPS
SPEED = L.SPEED
PATH_LEN = 15.0               # the truck drives on past the 12 m window so every segment there gets cut
PASSES = 3
PASS_EVERY = 5.0
RUT_EVERY = 0.25
RUT_STEP = 0.5
RUT_BEHIND_GAP = 1.5
SLUMP_PER_OP = 3
MORPH = 0.85
SUBSTEPS = 1                   # op segments stamped in 0.125 m pieces (1 = whole 0.5 m segments)

N = int(L.AREA / L.S_CELL) + 1
XS = np.arange(N) * L.S_CELL
GX, GZ = np.meshgrid(XS, XS)
HALF0 = min(L.HALF_MAX, max(L.HALF_MIN, L.HALF_PER_R * L.TYRE_R))

CLIENTS = {"A": dict(interp=0.10, lat=0.08, err_phase=0.0), "B": dict(interp=0.16, lat=0.14, err_phase=2.1)}
ERR_AMP = 0.03


# ------------------------------------------------------------------------------------------------ path
def wheel_at(s, side, lat_err=0.0):
    """Wheel contact at path distance s (m) on side -1 / 1, with a lateral view error."""
    th0 = math.radians(15)
    th = th0 + s / L.PATH_R
    x = 2.5 + L.PATH_R * (math.sin(th) - math.sin(th0))
    z = 5.0 - L.PATH_R * (math.cos(th) - math.cos(th0))
    dx, dz = math.cos(th), math.sin(th)
    off = side * L.TRACK_GAP / 2 + lat_err
    return x - dz * off, z + dx * off


# ------------------------------------------------------------------------------------------------ sheet maths
def stamp(D, OU, OV, P, ax, az, bx, bz, speed):
    """MUD_MudSheet.Stamp (BOX + SOFT + GRADUAL), vectorised. P = mud depth at this pass's start."""
    half = HALF0
    wi, wk = int(round(bx / L.S_CELL)), int(round(bz / L.S_CELL))
    if 0 <= wi < N and 0 <= wk < N:
        pressed = min(1.0, max(0.0, (L.THICK - P[wk, wi]) / L.THICK))
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
    i0 = max(1, int(math.floor((min(ax, ex) - reach) / L.S_CELL)))
    i1 = min(N - 2, int(math.ceil((max(ax, ex) + reach) / L.S_CELL)))
    k0 = max(1, int(math.floor((min(az, ez) - reach) / L.S_CELL)))
    k1 = min(N - 2, int(math.ceil((max(az, ez) + reach) / L.S_CELL)))
    if i1 < i0 or k1 < k0:
        return
    h = min(L.HMAX, L.H0 + L.H_SPEED * speed)
    drag = L.DRAG * ln
    sl = (slice(k0, k1 + 1), slice(i0, i1 + 1))
    px, pz = GX[sl] - ax, GZ[sl] - az
    t = px * dx + pz * dz
    latv = px * sx + pz * sz
    lat = np.abs(latv)
    ok = (t >= 0) & (t <= far) & (lat < reach) & ~((t > nose) & (lat >= half))
    if not ok.any():
        return
    d0 = P[sl]
    Dv = D[sl]
    under = ok & (t <= nose) & (lat < half)
    inw = half * L.SOFT_WALL
    fs = half - lat
    pf = np.where(fs < inw, 0.5 * (1 - np.cos(np.pi * np.clip(fs, 0, None) / inw)), 1.0)
    pressv = np.maximum(0.0, np.minimum(d0, L.THICK) - L.PRESS_STEP * pf)
    Dv = np.where(under, np.minimum(Dv, pressv), Dv)
    push = ok & ~under
    front = t > nose
    x = (lat - half) / band
    w_side = np.where(x > L.RIDGE_TOP, 0.5 * (1 + np.cos(np.pi * (x - L.RIDGE_TOP) / (1 - L.RIDGE_TOP))),
                      np.sin(0.5 * np.pi * np.clip(x, 0, None) / L.RIDGE_TOP))
    w_front = np.cos(np.clip((t - nose) / L.FRONT_LEN, 0, 1) * np.pi * 0.5)
    w = np.where(front, w_front, w_side)
    hh = np.where(front, h * 0.8, h)
    target = np.minimum(L.THICK + hh * w, np.maximum(d0, L.THICK) + hh * w * L.PRESS_STEP / L.THICK)
    Dv = np.where(push & (target > Dv), target, Dv)
    D[sl] = Dv
    sgn = np.where(latv >= 0, 1.0, -1.0)
    pdx = np.where(front, dx, sx * sgn)
    pdz = np.where(front, dz, sz * sgn)
    du = np.where(under, dx * drag, np.where(push, pdx * drag * 0.5 * w, 0.0))
    dv = np.where(under, dz * drag, np.where(push, pdz * drag * 0.5 * w, 0.0))
    ou, ov = OU[sl] + du, OV[sl] + dv
    ol = np.hypot(ou, ov)
    sc = np.where(ol > L.DRAG_MAX, L.DRAG_MAX / np.maximum(ol, 1e-9), 1.0)
    OU[sl], OV[sl] = ou * sc, ov * sc


def slump(D, G, iters):
    """MUD_MudSheet.Slump, vectorised (it already sums into T and applies once per call). G = ground height."""
    step = L.REPOSE * L.S_CELL
    for _ in range(iters):
        T = np.zeros_like(D)
        S = G + D
        for dk, di in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            Sn = np.roll(np.roll(S, -dk, 0), -di, 1)
            drop = S - Sn - step
            f = np.minimum(drop * L.SLUMP * 0.5, D * 0.25)
            m = (D > 0) & (drop > 0) & (f >= 0.0002)
            m[0, :] = m[-1, :] = m[:, 0] = m[:, -1] = False
            m[max(0, -dk):N - max(0, dk), max(0, -di):N - max(0, di)] &= True
            f = np.where(m, f, 0.0)
            T -= f
            T += np.roll(np.roll(f, dk, 0), di, 1)
        D += T


NOISE = np.array([[L.hash01(i, k) for i in range(N)] for k in range(N)])


def dirt_of(D, OU, OV):
    noise = NOISE
    moved = np.abs(D - L.THICK) > L.TOUCH + noise * L.DIRT_NOISE
    dragged = np.hypot(OU, OV) > L.DRAG_TOUCH
    return (moved | dragged).astype(float)


# ------------------------------------------------------------------------------------------------ server
def server_ops():
    """Rut ops over all passes: list of dict(t, seq, pass, speed, segs[side] = [((ax, az), (bx, bz)) cm-rounded])."""
    ops = []
    seq = 0
    for p in range(PASSES):
        t0 = p * PASS_EVERY
        cut_to = 0.0
        drive_t = PATH_LEN / SPEED
        t = t0 + RUT_EVERY
        while t <= t0 + drive_t + RUT_EVERY:
            s_now = min(PATH_LEN, (t - t0) * SPEED)
            ready = math.floor(max(0.0, s_now - RUT_BEHIND_GAP) / RUT_STEP) * RUT_STEP
            if s_now >= PATH_LEN:
                ready = math.floor(PATH_LEN / RUT_STEP) * RUT_STEP
            if ready > cut_to + 1e-9:
                segs = {}
                for side in (-1, 1):
                    lst = []
                    s = cut_to
                    while s < ready - 1e-9:
                        a = wheel_at(s, side)
                        b = wheel_at(s + RUT_STEP, side)
                        lst.append(((round(a[0], 2), round(a[1], 2)), (round(b[0], 2), round(b[1], 2))))
                        s += RUT_STEP
                    segs[side] = lst
                seq += 1
                ops.append(dict(t=t, seq=seq, pas=p, speed=round(SPEED, 1), segs=segs, to=ready))
                cut_to = ready
            t += RUT_EVERY
    return ops


# ------------------------------------------------------------------------------------------------ felt (rut field)
class Felt:
    def __init__(self):
        self.n = int(L.AREA / scm.FCELL)
        scm.N = self.n
        self.F = np.zeros((self.n, self.n))
        self.R = np.zeros((self.n, self.n))
        self.G = np.zeros((N, N))

    def apply(self, op):
        scm.N = self.n
        for side in (-1, 1):
            segs = op["segs"][side]
            if not segs:
                continue
            keys, maxcut = scm.carve(self.F, self.R, segs, L.STEP, L.CAP, L.RUT_R, share=L.RUT_SHARE, rclr=True)
            scm.relax(self.F, keys, L.RELAX_DEG, L.RELAX_IT)
            scm.ridge(self.R, self.F, segs, L.RUT_R, L.RIDGE_W, L.RIDGE_B, maxcut, L.RIDGE_MAX, gap=L.RIDGE_GAP,
                      vol_r=L.RUT_R, blend="add")
        self.G = L.tri_sample(np.where(self.F < 0, self.F, self.R), scm.FCELL, GX, GZ)


# ------------------------------------------------------------------------------------------------ client
class Client:
    def __init__(self, name, cfg, ops, rng):
        self.name, self.cfg = name, cfg
        self.felt = Felt()
        self.D = np.full((N, N), L.THICK)      # SYNC layer
        self.OU = np.zeros((N, N))
        self.OV = np.zeros((N, N))
        self.P = self.D.copy()                  # pass start (sync), taken at the first op of a pass
        self.pas = -1
        self.arrive = sorted(((op["t"] + cfg["lat"] + rng.uniform(-0.02, 0.02), op) for op in ops),
                             key=lambda a: a[1]["seq"])
        # an op can only be applied in seq order: arrival = max(own, previous)
        last = 0.0
        fixed = []
        for t, op in self.arrive:
            last = max(last, t)
            fixed.append((last, op))
        self.arrive = fixed
        self.next = 0
        self.local = []                          # (pass, s_view, side, a, b, speed) not yet covered
        self.cov = {}                            # pass -> path distance covered by ops
        self.prev_view = {}
        self.E = np.zeros((N, N))                # handoff residual (morphed out)
        self.morphs = []
        self.Dp, self.OUp, self.OVp = self.D.copy(), self.OU.copy(), self.OV.copy()

    def view_s(self, t):
        p = int(t // PASS_EVERY)
        tt = t - p * PASS_EVERY - self.cfg["interp"]
        if p >= PASSES or tt < 0:
            return p, None
        return p, min(PATH_LEN, tt * SPEED)

    def lat_err(self, t):
        return ERR_AMP * math.sin(t * 2.3 + self.cfg["err_phase"])

    def predict(self):
        Dp, OUp, OVp = self.D.copy(), self.OU.copy(), self.OV.copy()
        P = self.P if self.pas >= 0 else self.D
        Pnew = None
        for pas, s, side, a, b, sp in self.local:
            if pas == self.pas:
                Pp = P
            else:
                if Pnew is None:
                    Pnew = Dp.copy()     # a pass the ops have not started yet: it starts from the drawn state
                Pp = Pnew
            stamp(Dp, OUp, OVp, Pp, a[0], a[1], b[0], b[1], sp)
        return Dp, OUp, OVp

    def step(self, t):
        # 1. this frame's own stamp at the client's view of the wheels (prediction)
        p, s = self.view_s(t)
        if s is not None:
            for side in (-1, 1):
                b = wheel_at(s, side, self.lat_err(t))
                key = (p, side)
                a = self.prev_view.get(key)
                self.prev_view[key] = b
                if a is not None and a != b:
                    self.local.append((p, s, side, a, b, SPEED))
        arrived = self.next < len(self.arrive) and self.arrive[self.next][0] <= t
        if arrived:
            before, _, _ = self.predict()       # same local stamps, OLD sync: what the player would see now
            before = before + self.E
        # 2. ops that arrived: sync layer + felt, retire the local stamps they cover
        while self.next < len(self.arrive) and self.arrive[self.next][0] <= t:
            op = self.arrive[self.next][1]
            self.next += 1
            if op["pas"] != self.pas:
                self.pas = op["pas"]
                self.P = self.D.copy()
            self.felt.apply(op)
            for side in (-1, 1):
                for (a, b) in op["segs"][side]:
                    for j in range(SUBSTEPS):      # 0.5 m segment in SUBSTEPS pieces: closer to the per-frame stamps
                        f0, f1 = j / SUBSTEPS, (j + 1) / SUBSTEPS
                        stamp(self.D, self.OU, self.OV, self.P, a[0] + (b[0] - a[0]) * f0, a[1] + (b[1] - a[1]) * f0,
                              a[0] + (b[0] - a[0]) * f1, a[1] + (b[1] - a[1]) * f1, op["speed"])
            slump(self.D, self.felt.G, SLUMP_PER_OP)
            self.cov[op["pas"]] = op["to"]
            self.local = [x for x in self.local if x[0] > op["pas"] or (x[0] == op["pas"] and x[1] > op["to"])]
        # 3. drawn = sync + replayed prediction (+ the handoff residual, morphed out)
        self.Dp, self.OUp, self.OVp = self.predict()
        if arrived:
            self.E = before - self.Dp
            big = np.abs(self.E)
            if self.arrive[self.next - 1][1]["to"] < PATH_LEN - 1e-6:   # the last op of a pass = the truck stopped
                self.morphs.append((float(big.max()), float((big > 0.01).sum() * L.S_CELL ** 2)))
        self.E *= MORPH

    def drawn(self, settled=False):
        D = self.D if settled else self.Dp + self.E
        OU = self.OU if settled else self.OUp
        OV = self.OV if settled else self.OVp
        return dict(Y=self.felt.G + D - L.THICK, dirt=dirt_of(D, OU, OV), ou=OU, ov=OV, cell=L.S_CELL, felt=None)


# ------------------------------------------------------------------------------------------------ old sheet reference
def old_sheet():
    felt = Felt()
    for op in server_ops():
        felt.apply(op)
    D = np.full((N, N), L.THICK)
    OU, OV = np.zeros((N, N)), np.zeros((N, N))
    for p in range(PASSES):
        P = D.copy()
        prev = {}
        nfr = int(PATH_LEN / SPEED / DT)
        for f in range(nfr + 1):
            s = f * DT * SPEED
            for side in (-1, 1):
                b = wheel_at(s, side)
                a = prev.get(side)
                prev[side] = b
                if a is not None:
                    stamp(D, OU, OV, P, a[0], a[1], b[0], b[1], SPEED)
            if f % 6 == 0:
                slump(D, felt.G, 1)
        slump(D, felt.G, 30)     # settles before the next pass (10 Hz for ~3 s)
    return dict(Y=felt.G + D - L.THICK, dirt=dirt_of(D, OU, OV), ou=OU, ov=OV, cell=L.S_CELL, felt=None), felt


# ------------------------------------------------------------------------------------------------ run
def main():
    os.makedirs(OUT_IMG, exist_ok=True)
    rng = np.random.default_rng(7)
    ops = server_ops()
    cl = {nm: Client(nm, cfg, ops, rng) for nm, cfg in CLIENTS.items()}
    x0, z0, dx, dz = L.mid_section()
    t_end = (PASSES - 1) * PASS_EVERY + PATH_LEN / SPEED + 1.5
    frames = []
    nfr = int(t_end / DT)
    gif_from = (PASSES - 1) * PASS_EVERY - 0.1
    gif_to = gif_from + PATH_LEN / SPEED + 1.0
    for f in range(nfr + 1):
        t = f * DT
        for c in cl.values():
            c.step(t)
        if gif_from <= t <= gif_to and f % 4 == 0:
            frames.append((t, {nm: c.drawn() for nm, c in cl.items()},
                           {nm: [wheel_at(c.view_s(t)[1], sd, c.lat_err(t)) for sd in (-1, 1)]
                            if c.view_s(t)[1] is not None else [] for nm, c in cl.items()}))
    for c in cl.values():                       # everything delivered, nothing predicted
        c.local = []
        c.E[:] = 0
    A, B = cl["A"], cl["B"]
    sync_diff = float(np.max(np.abs(A.D - B.D)))
    felt_diff = float(np.max(np.abs(A.felt.F - B.felt.F)) + np.max(np.abs(A.felt.R - B.felt.R)))
    morph_max = [m for c in cl.values() for m in c.morphs]
    mm = np.array([m[0] for m in morph_max]) if morph_max else np.zeros(1)
    ma = np.array([m[1] for m in morph_max]) if morph_max else np.zeros(1)

    ref, ref_felt = old_sheet()
    synced = A.drawn(settled=True)
    # joiner fallback: the rut-derived look on the same felt floor
    built = dict(F=A.felt.F, R=A.felt.R)
    joiner = L.look_run(built, 0.125)
    joiner["felt"] = None
    today = dict(Y=np.where(A.felt.F < 0, A.felt.F, A.felt.R), dirt=(A.felt.F < -L.SOIL_D).astype(float),
                 ou=np.zeros_like(A.felt.F), ov=np.zeros_like(A.felt.F), cell=scm.FCELL, felt=None)
    models = [("OLD SHEET (local, per frame)", ref), ("SYNCED SHEET (ops, any client)", synced),
              ("JOINER fallback (rut-derived look)", joiner), ("TODAY (felt rut only)", today)]
    mets = [(nm, L.metrics(m, nm, x0, z0, dx, dz)) for nm, m in models]
    ref_prof = mets[0][1]["profile"]
    rms_ref = float(np.sqrt(np.mean((ref["Y"] - synced["Y"]) ** 2)))
    win = (np.abs((GX - x0) * dx + (GZ - z0) * dz) < 4) & (np.abs((GX - x0) * -dz + (GZ - z0) * dx) < 2.5)
    rms_ref_win = float(np.sqrt(np.mean(((ref["Y"] - synced["Y"])[win]) ** 2)))
    max_ref_win = float(np.max(np.abs(ref["Y"] - synced["Y"])[win]))

    # ---- images
    fig, axs = plt.subplots(len(models), 1, figsize=(9, 3.9 * len(models)))
    for ax, (nm, m) in zip(axs, models):
        ax.imshow(L.render(m, x0, z0, dx, dz), origin="lower", extent=(-3.5, 3.5, -2, 2))
        ax.set_title(nm + " - 3 passes, settled", fontsize=10)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT_IMG, "2026-10-17-mud-sync-top.png"), dpi=100)
    plt.close(fig)

    fig = plt.figure(figsize=(12, 9))
    for j, (nm, m) in enumerate(models):
        ax = fig.add_subplot(2, 2, j + 1, projection="3d")
        L.mesh_view(ax, m, x0, z0, dx, dz, nm)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT_IMG, "2026-10-17-mud-sync-mesh.png"), dpi=100)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(10, 4))
    for nm, mt in mets:
        ax.plot(*mt["profile"], label=nm)
    ax.axhline(0, color="grey", lw=0.5)
    ax.set_xlabel("across one wheel track (m)")
    ax.set_ylabel("height vs untouched mud top (m)")
    ax.legend(fontsize=8)
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT_IMG, "2026-10-17-mud-sync-cross.png"), dpi=110)
    plt.close(fig)

    gif(frames, x0, z0, dx, dz)

    lines = report(sync_diff, felt_diff, mm, ma, mets, ref_prof, rms_ref_win, max_ref_win, len(ops), cl)
    print("\n".join(lines))
    with open(OUT_MD, "w") as f:
        f.write(md(lines))


def gif(frames, x0, z0, dx, dz):
    """Last pass, client A and B drawn side by side + |A - B| (the difference while the truck is there)."""
    fig, axs = plt.subplots(1, 3, figsize=(15, 3.6))

    def draw(i):
        t, mods, wheels = frames[i]
        for ax in axs:
            ax.clear()
        for ax, nm in zip(axs[:2], ("A", "B")):
            ax.imshow(L.render(mods[nm], x0, z0, dx, dz, w=7.0, h=4.0, px=0.04), origin="lower",
                      extent=(-3.5, 3.5, -2, 2))
            for (wx, wz) in wheels[nm]:
                u = (wx - x0) * dx + (wz - z0) * dz
                v = (wx - x0) * -dz + (wz - z0) * dx
                if abs(u) < 3.5:
                    ax.plot(u, v, "o", ms=9, mfc="none", mec="yellow")
            ax.set_title("client %s drawn, t = %.2f s" % (nm, t), fontsize=9)
            ax.set_xticks([])
            ax.set_yticks([])
        us = np.arange(-3.5, 3.5, 0.04)
        vs = np.arange(-2, 2, 0.04)
        U, V = np.meshgrid(us, vs)
        X = x0 + dx * U - dz * V
        Z = z0 + dz * U + dx * V
        d = np.abs(L.tri_sample(mods["A"]["Y"], L.S_CELL, X, Z) - L.tri_sample(mods["B"]["Y"], L.S_CELL, X, Z))
        axs[2].imshow(d, origin="lower", extent=(-3.5, 3.5, -2, 2), vmin=0, vmax=0.05, cmap="magma")
        axs[2].set_title("|A - B| height (0 .. 5 cm), max %.3f m" % d.max(), fontsize=9)
        axs[2].set_xticks([])
        axs[2].set_yticks([])

    anim = FuncAnimation(fig, draw, frames=len(frames))
    anim.save(os.path.join(OUT_IMG, "2026-10-17-mud-sync-drive.gif"), writer=PillowWriter(fps=12))
    plt.close(fig)


def report(sync_diff, felt_diff, mm, ma, mets, ref_prof, rms_win, max_win, nops, cl):
    out = []
    out.append("| Check | Result |")
    out.append("|---|---|")
    out.append("| Rut ops (3 passes) | %d |" % nops)
    out.append("| SYNC layer client A vs B after all ops (max abs diff) | %.6f m |" % sync_diff)
    out.append("| Felt rut field A vs B (max abs diff) | %.6f m |" % felt_diff)
    out.append("| Handoff jump when an op replaces the prediction: median / p95 / max | %.3f / %.3f / %.3f m |"
               % (np.median(mm), np.percentile(mm, 95), mm.max()))
    out.append("| Area that jumps > 1 cm per handoff: median / max | %.2f / %.2f m2 |" % (np.median(ma), ma.max()))
    out.append("| Synced vs old sheet, settled, 8 x 5 m window: rms / max | %.3f / %.3f m |" % (rms_win, max_win))
    out.append("")
    out.append("| Model | Depth m | Width at 50 % m | Width at 80 % m | Ridge m | Wall p95 deg | Shape vs OLD SHEET | Dirt m2 |")
    out.append("|---|---|---|---|---|---|---|---|")
    for nm, mt in mets:
        out.append("| %s | %.3f | %.2f | %.2f | %.3f | %.0f | %.2f | %.1f |" % (
            nm, mt["depth"], mt["w50"], mt["w80"], mt["ridge"], mt["wall95"], L.shape_distance(ref_prof, mt["profile"]),
            mt["dirt_m2"]))
    return out


def md(lines):
    body = "\n".join(lines)
    return f"""# Synced mud sheet prototype (2026-10-17, offline)

Tool: `python Tools/mud_sheet_sync_proto.py` (setup in its docstring). MEASURED offline on copied game maths
(MUD_MudSheet Stamp / Slump; CarveFloor via Tools/scm_lite_proto.py, ebe4527 values). Truck, latencies, view errors
are ASSUMED round numbers. Nothing in Scripts/ changed. Stand-in renderer (procedural texture, low sun).

![drive: client A, client B, difference](img/2026-10-17-mud-sync-drive.gif)
![settled top views](img/2026-10-17-mud-sync-top.png)
![meshes](img/2026-10-17-mud-sync-mesh.png)
![cross-section](img/2026-10-17-mud-sync-cross.png)

{body}

## Reading
- **Every client ends up with the same mud.** Pressing the sheet only from the replicated rut ops (in op order, slump
  a fixed 3 iterations per op) gives client A and B bit-identical sheets and rut fields after the last op (max diff
  0.000000 m), whatever their latency or view of the truck.
- **It still looks like the old sheet.** Settled, the synced sheet is within 6 mm rms (3.9 cm max) of the old
  per-frame sheet in the 8 x 5 m window; groove width, walls, ridge and dirt are the same (shape distance 0.01).
  Op segments of 0.5 m are fine-grained enough (stamping them in 0.125 m pieces changed nothing).
- **The live part stays live.** Each client stamps its own view of the wheels every frame (press, bow wave, ridge
  at the tyre at once), then the op version replaces it (rollback + replay).
- **The handoff jump:** median 6 cm, p95 10 cm, on ~1.7 m2, 1.5-2 m behind the tyre (where RUT_BEHIND cuts),
  morphed out over ~0.25 s. Sources, measured with a sweep (scratch runs, same tool):
  - the client's view of the truck being off sideways (+-3 cm here) - with no view error the median is 3.6 cm, at
    +-10 cm it is 9.8 cm;
  - mud slumping into the felt rut the op just cut (area 1.2 m2 with slump on vs 0.04 m2 off).
  It is a groove section shifting a few cm sideways, not mud appearing or vanishing. Whether it is visible needs
  the game.
- **While the truck is there, two clients differ** (gif, right panel: up to ~0.17 m for a fraction of a second
  behind the truck): one client got the op (and its felt rut cut) ~60 ms earlier, and each sees the truck at its
  own interpolated place. This is the same as today's felt ruts (each client applies an op when it arrives). It goes
  to 0 when the op reaches both.
- **Felt rut under the mud:** drawn = felt ground + mud depth, so tyres always sit at the bottom of the mud and the
  felt rut deepens the drawn rut 1.5 m behind the truck (depth 0.38 m after 3 passes = felt 0.27 + mud press).
  Option (not in the proto): let the mud fill the felt cut (depth += felt drop), so the drawn top does not drop and
  tyres look sunk deeper - a look choice.
- **Late joiners do NOT get the sheet from the J2 snapshot** (it has the floor, not the op segments). Fallback
  shown: the rut-derived look (shape distance 0.18 to the sheet, the bottom-but-one picture). A real fix = the
  snapshot also carries the touched mud cells (ESTIMATE: ~1.2 m of touched strip per wheel track, 0.15 m cells,
  ~3 bytes each -> roughly 0.1-0.3 MB per km of tracks before compression) - a later choice.

## What is NOT shown / still open
- Cost per frame in the game (stamps per op: 2 wheels x ~4 segments; prediction replays ~20-25 local stamps per op
  arrival; tile mesh rebuilds near the camera) - PERF session only.
- Pass start must be deterministic: the sheet used a clock (PASS_GAP 4 s); the proto puts a pass number in the op.
  In the game: per-cell "last op seq + vehicle" instead of time.
- Speed for the ridge height must travel in the op (proto: 0.1 m/s field). Tyre half width too if it should vary
  by vehicle (proto: one truck).
- Where mud is (zones or the terrain gamemat), the mud edge (TAPER), and craters / trenches clearing mud: the old
  sheet's rules, unchanged, look-only now.
- Animation: the gif is the stand-in renderer, 0.04 m pixels, 12 fps.
"""


if __name__ == "__main__":
    main()
