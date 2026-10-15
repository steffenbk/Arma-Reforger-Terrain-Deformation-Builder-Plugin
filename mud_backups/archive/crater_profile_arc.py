"""Crater shape model for MUD_WorldGround.c Crater (2026-10-10 crater job).

Mirrors the Enforce math line for line: cosine bowl to a rim crest, cosine fall to the
outer edge, M1 rim rule, M2 deepen rule, floor clamp, then the collider blur (binomial
1 4 6 4 1 per axis = m_iColliderSmooth 2). Measures the steepest triangle of the fine
grid (0.25 m, each cell split on one diagonal) raw (hard ground keeps raw heights) and
blurred (soft ground collider).

    python Tools/crater_profile.py          # table of all cases
    python Tools/crater_profile.py --check  # exit 1 if a gated case is over 45 deg
"""
import math
import sys

import numpy as np

STEP = 0.25            # m_fFine on Arland (skin cell 2 m / FINE 8)
MAX_SLOPE = 0.75       # m_fCraterMaxSlope default (0.9 before the crawl job)
GAIN = 0.5             # m_fCraterDeepenGain default

DEPTH_PER_KG = 1.5     # m_fCraterDepthPerKg
TNT_POWER = 0.6        # m_fCraterTntPower
MAX_DEPTH = 1.35       # m_fCraterMaxDepth (knee: cube root past it)
HUGE_DEPTH = 3.0       # m_fCraterHugeDepth
BIG_TNT = 2.0          # m_fCraterBigTnt (>= this: no 2 x 2 patch fit)
RIM_SHARE = 0.17       # m_fCraterRimShare
RIM_WIDTH_SHARE = 0.8  # m_fCraterRimWidthShare (0.4 before the crawl job)
STACK_FACTOR = 1.67    # m_fCraterStackFactor
STACK_DEPTH = 2.5      # m_fCraterStackDepth
HEAT_FACTOR = 0.6      # m_fCraterHeatFactor (warhead fires a penetrator = HEAT/HEDP)
DEPTH_JITTER = 0.15    # m_fCraterDepthJitter (depth x 1 +- this)
SHAPE_JITTER = 0.15    # m_fCraterShapeJitter (radius x 1 .. 1 + this, lobed by angle)
MAX_OUTER = 3.95       # CRATER_MAX_OUTER: a fresh crater stays within 2 x 2 patches of 8 m
HUGE_OUTER = 7.95      # CRATER_HUGE_OUTER: a big charge's crater within 3 x 3 patches
# arc bowl (2026-10-10 crawl job): concave floor arc, straight wall at MAX_SLOPE, convex arc to the crest
FLOOR_R = 3.5          # m_fCraterFloorRadius (0 = old cosine bowl)
CREST_R = 2.0          # m_fCraterCrestRadius
ARC_REF = 1.2          # m_fCraterArcRef: bowls shallower than this (depth + rim) shrink both radii
PROFILE = "arc"        # "arc" or "cos" (--cos)

# TNT kg = ChargeWeight g x TntEquivalent / 1000, read from the Stable pak 2026-10-10
# (warhead prefabs; O832DU sets no TntEquivalent, engine default unknown -> 1 assumed here)
# (kg, heat): heat = warhead has a penetrator submunition (pak scan 2026-10-10)
FAMILIES = {
    "small": (0.077, False),   # 40 mm VOG-25 (M406 0.043)
    "m433": (0.072, True),     # 40 mm M433 HEDP 45 g x 1.6
    "mid": (0.672, False),     # S-5MO frag
    "heat": (0.512, True),     # PG-7VM / RPG-75 320 g x 1.6 (M72A3 0.467, PG-22 0.612, S-5KO 0.528)
    "big": (0.990, False),     # 81 mm M821 680 g x 1.456
    "hydra": (2.902, False),   # Hydra M229 2177 g x 1.333
    "m247": (1.213, True),     # Hydra M247 HEDP
    "o832": (0.400, False),    # 82 mm O-832DU 400 g x (default)
    "tm62": (7.5, False),      # TM-62M AT mine 7500 g (no TntEquivalent: 1 assumed)
    "rack": (38.0, False),     # Explosion_AmmoRack_Large 38000 g (vehicle blast)
}


def full_depth(tnt, heat):
    """FullDepth in the script: power curve to the knee, cube root past it, huge cap."""
    knee = (MAX_DEPTH / DEPTH_PER_KG) ** (1 / TNT_POWER)
    kg = max(tnt, 0)
    d = DEPTH_PER_KG * kg ** TNT_POWER if kg <= knee else MAX_DEPTH * (kg / knee) ** (1 / 3)
    if heat:
        d *= HEAT_FACTOR
    return min(d, HUGE_DEPTH)


def shape(fam, hc, floor_c, scale=1.0, jit=0.0):
    """(D, Rc, H, Ro, stretch) of one blast, as Crater in the script. hc = height (rel. to the
    undeformed surface) at the centre vertex before the blast, floor_c = floor there (<0).
    jit = the depth random in -1..1."""
    tnt, heat = fam
    full = full_depth(tnt, heat)
    D = full * scale * (1 + DEPTH_JITTER * jit)
    H = D * RIM_SHARE
    W = max(2 * STEP, D * RIM_WIDTH_SHARE)
    # M2: a hit in dug ground digs below what is there, capped
    if hc < 0:
        D = max(D, min(min(STACK_DEPTH, full * STACK_FACTOR), -hc + GAIN * D))
    if PROFILE == "arc":
        Rc, arc = arc_params(D + H)
    else:
        Rc = max(3 * STEP, (D + H) * math.pi / (2 * MAX_SLOPE))
        arc = None
    W = max(W, H * math.pi / (2 * MAX_SLOPE))
    # script order: crest from the unclamped depth, then fit into the floor (only flattens)
    Ro = Rc + W
    big = tnt >= BIG_TNT
    lim = HUGE_OUTER if big else MAX_OUTER
    if hc >= 0 and Ro > lim:     # fresh crater: scale the whole bowl into 2 x 2 (big: 3 x 3) patches
        k = lim / Ro
        D, H, Rc, Ro = D * k, H * k, Rc * k, lim
        if arc:
            arc = tuple(a * k for a in arc[:4]) + (arc[4],)
    D = min(D, -floor_c)
    stretch = min(SHAPE_JITTER, lim / Ro - 1)
    stretch = max(0.0, min(stretch, 1.0))
    return D, Rc, H, Ro, stretch, arc


def arc_params(rise):
    """Crest radius + (rb, rt, wall, natural crest, theta) of the arc bowl for a rise of depth + rim."""
    kr = min(1.0, rise / max(ARC_REF, 0.1))
    rb = FLOOR_R * kr
    rt = CREST_R * kr
    th = math.atan(MAX_SLOPE)
    sr = max(rb + rt, 0.01)
    wall = 0.0
    if rise < sr * (1 - math.cos(th)):
        th = math.acos(1 - rise / sr)
    else:
        wall = (rise - sr * (1 - math.cos(th))) / math.tan(th)
    nat = sr * math.sin(th) + wall
    return max(3 * STEP, nat), (rb, rt, wall, nat, th)


def arc_rise(d, arc):
    """Rise above the bottom at distance d along the natural (unstretched) arc bowl."""
    rb, rt, wall, nat, th = arc
    xb = rb * math.sin(th)
    if d <= xb:
        return rb - math.sqrt(max(rb * rb - d * d, 0.0))
    if d <= xb + wall:
        return rb * (1 - math.cos(th)) + (d - xb) * math.tan(th)
    u = max(nat - d, 0.0)
    total = (rb + rt) * (1 - math.cos(th)) + wall * math.tan(th)
    return total - (rt - math.sqrt(max(rt * rt - u * u, 0.0)))


def profile(d, D, Rc, H, Ro, arc=None):
    if d < Rc and arc:
        rb, rt, wall, nat, th = arc
        total = (rb + rt) * (1 - math.cos(th)) + wall * math.tan(th)
        return -D + (D + H) * arc_rise(d * nat / Rc, arc) / max(total, 1e-6)
    if d < Rc:
        return -D + (D + H) * (1 - math.cos(math.pi * d / Rc)) / 2
    if d < Ro:
        return H * (1 + math.cos(math.pi * (d - Rc) / (Ro - Rc))) / 2
    return 0.0


def blast(h, floor, cx, cz, fam, m1="strict", scale=1.0, rnd=None):
    """Apply one blast at world (cx, cz) to height grid h (in place). Grid vertex i -> i*STEP.
    rnd = (jit -1..1, w 0..1, p2, p3 radians) or None for a plain round crater."""
    n = h.shape[0]
    ic = int(round(cx / STEP))
    jc = int(round(cz / STEP))
    jit, w, p2, p3 = rnd if rnd else (0.0, 0.0, 0.0, 0.0)
    D, Rc, H, Ro, stretch, arc = shape(fam, h[jc, ic], floor[jc, ic], scale, jit)
    if not rnd:
        stretch = 0.0
    for j in range(n):
        for i in range(n):
            dx = i * STEP - cx
            dz = j * STEP - cz
            d = math.hypot(dx, dz)
            if d >= Ro * (1 + stretch):
                continue
            if stretch > 0:
                a = math.atan2(dz, dx)
                f = 1 + stretch * (w * (0.5 + 0.5 * math.cos(2 * (a - p2)))
                                   + (1 - w) * (0.5 + 0.5 * math.cos(3 * (a - p3))))
                d /= f
                if d >= Ro:
                    continue
            y = max(profile(d, D, Rc, H, Ro, arc), floor[j, i])
            cur = h[j, i]
            if y < 0:
                if y < cur:
                    h[j, i] = y
            elif y > cur:
                if m1 == "strict":
                    if cur >= 0:            # M1: the rim never fills dug ground
                        h[j, i] = y
                elif m1 == "old":
                    h[j, i] = y
    return D, Rc, H, Ro * (1 + stretch)


def blast_legacy(h, cx, cz, R=2.5, D=2.0, H=0.3):
    """The shape before this job (Crater as of Task 10), as a control."""
    n = h.shape[0]
    for j in range(n):
        for i in range(n):
            d = math.hypot(i * STEP - cx, j * STEP - cz)
            if d < R:
                k = d / R
                y = -D * (1 - k * k)
                if y < h[j, i]:
                    h[j, i] = y
            elif d < 1.5 * R:
                t = 1 - (d - R) / (0.5 * R)
                if H * t * t > h[j, i]:
                    h[j, i] = H * t * t


def blur(h):
    w = np.array([1, 4, 6, 4, 1], float) / 16
    p = np.pad(h, 2, mode="edge")
    rows = sum(w[k] * p[:, k:k + h.shape[1]] for k in range(5))
    return sum(w[k] * rows[k:k + h.shape[0], :] for k in range(5))


def steepest(h):
    """Max triangle slope (deg). Cells split bl-tr; both triangles' face gradients."""
    a = h[:-1, :-1]
    b = h[:-1, 1:]
    c = h[1:, :-1]
    d = h[1:, 1:]
    # triangle (a, b, d): dx = b-a, dz = d-b ; triangle (a, c, d): dx = d-c, dz = c-a
    g1 = np.hypot(b - a, d - b) / STEP
    g2 = np.hypot(d - c, c - a) / STEP
    return math.degrees(math.atan(max(g1.max(), g2.max())))


def tightest_bend(h):
    """Smallest concave bend radius (m) on the grid rows and columns: where a prone box's ends dig in."""
    best = 1e9
    for g in (h, h.T):
        d1 = (g[:, 2:] - g[:, :-2]) / (2 * STEP)
        d2 = (g[:, 2:] - 2 * g[:, 1:-1] + g[:, :-2]) / (STEP * STEP)
        k = d2 / (1 + d1 * d1) ** 1.5
        kmax = k.max()
        if kmax > 1e-6:
            best = min(best, 1 / kmax)
    return best


def case(name, blasts, m1="strict", ruts=False, size=20.0, floor=-4.0):
    n = int(size / STEP) + 1
    h = np.zeros((n, n))
    floor = np.full((n, n), floor)
    if ruts:
        h[:, int(n * 0.45):int(n * 0.45) + 3] = -0.2
    shapes = []
    for (x, z, fam, *rest) in blasts:
        scale = rest[0] if rest else 1.0
        rnd = rest[1] if len(rest) > 1 else None
        if fam == "legacy":
            blast_legacy(h, x, z)
            shapes.append((2.0, 2.5, 0.3, 3.75))
            continue
        shapes.append(blast(h, floor, x, z, FAMILIES[fam], m1, scale, rnd))
    return dict(name=name, m1=m1, raw=steepest(h), blur=steepest(blur(h)), bend=tightest_bend(blur(h)), low=h.min(),
                high=h.max(), shapes=shapes)


def cases():
    c = 10.0
    o = 0.11     # off-grid centre, as real hits are
    out = [case("control: old single", [(c + o, c + 0.07, "legacy")], m1="old"),
           case("control: old two 2 m apart", [(c - 1 + o, c, "legacy"), (c + 1 + o, c, "legacy")], m1="old")]
    for fam in FAMILIES:
        out.append(case("single %s (%.3f kg%s)" % (fam, FAMILIES[fam][0], " HEAT" if FAMILIES[fam][1] else ""),
                        [(c + o, c + 0.07, fam)]))
    # variation: worst-case randoms (deep, pure ellipse / pure 3-lobe, mixed), 3 phases each
    import random
    rng = random.Random(7)
    worst = {}
    for fam in FAMILIES:
        for k in range(12):
            rnd = (rng.choice([1.0, rng.uniform(-1, 1)]), rng.choice([0.0, 1.0, rng.random()]),
                   rng.uniform(0, 6.283), rng.uniform(0, 6.283))
            r = case("varied %s" % fam, [(c + o, c + 0.07, fam, 1.0, rnd)])
            if fam not in worst or r["blur"] > worst[fam]["blur"]:
                worst[fam] = r
    out.extend(worst.values())
    out.append(case("varied two big 2 m apart", [(c - 1 + o, c, "big", 1.0, (1.0, 0.0, 0.5, 1.0)),
                                                (c + 1 + o, c, "big", 1.0, (1.0, 1.0, 2.0, 0.3))]))
    out.append(case("varied big twice same spot (M2)", [(c + o, c, "big", 1.0, (1.0, 0.0, 0.0, 0.0)),
                                                       (c + o + 0.1, c, "big", 1.0, (1.0, 1.0, 1.0, 0.0))]))
    out.append(case("hydra twice same spot (M2)", [(c + o, c, "hydra"), (c + o + 0.1, c, "hydra")]))
    out.append(case("two big 2 m apart", [(c - 1 + o, c, "big"), (c + 1 + o, c, "big")]))
    out.append(case("two big 2 m apart", [(c - 1 + o, c, "big"), (c + 1 + o, c, "big")], m1="old"))
    out.append(case("two big 4 m apart", [(c - 2 + o, c, "big"), (c + 2 + o, c, "big")]))
    out.append(case("two big 6 m apart", [(c - 3 + o, c, "big"), (c + 3 + o, c, "big")]))
    out.append(case("big twice same spot (M2)", [(c + o, c, "big"), (c + o + 0.1, c, "big")]))
    out.append(case("big x3 same spot (M2 cap)", [(c + o, c, "big")] * 3))
    out.append(case("small inside big", [(c + o, c, "big"), (c + o + 1, c, "small")]))
    out.append(case("mid on big rim", [(c + o, c, "big"), (c + o + 3.1, c, "mid")]))
    out.append(case("big beside ruts", [(c + o, c, "big")], ruts=True))
    out.append(case("big half height (air 0.75)", [(c + o, c, "big", 0.5)]))
    out.append(case("big, floor 0.5 m (depth box)", [(c + o, c, "big")], floor=-0.5))
    return out


GATED_DEG = 45.0


def main(argv):
    global PROFILE
    if "--cos" in argv:
        PROFILE = "cos"
    rows = cases()
    print("profile: %s" % PROFILE)
    print("| case | M1 | raw deg | collider deg | collider bend m | lowest m | highest m | shapes (D, Rc, H, Ro) |")
    print("|---|---|---|---|---|---|---|---|")
    bad = []
    for r in rows:
        sh = "; ".join("%.2f %.2f %.2f %.2f" % s[:4] for s in r["shapes"])
        print("| %s | %s | %.1f | %.1f | %.2f | %.2f | %.2f | %s |" % (
            r["name"], r["m1"], r["raw"], r["blur"], r["bend"], r["low"], r["high"], sh))
        if r["m1"] != "old" and r["blur"] > GATED_DEG:
            bad.append(r["name"])
    if bad:
        print("OVER %.0f deg after the blur: %s" % (GATED_DEG, ", ".join(bad)))
    if "--check" in argv:
        return 1 if bad else 0
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
