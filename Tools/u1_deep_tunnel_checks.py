"""U1 deep tunnels (2026-10-17): offline checks before the Enforce change.
  python Tools/u1_deep_tunnel_checks.py clamp   floor clamp old vs "no NEW air below the floor" (refill + chamber)
  python Tools/u1_deep_tunnel_checks.py lid     crater at the bottom of a 5 m open shaft: Carve's gj1 (FLOOR vs TUNNEL_FLOOR)
  python Tools/u1_deep_tunnel_checks.py slope   dig-down slope per max drop per click (the P2 slope cap)
  python Tools/u1_deep_tunnel_checks.py all
  python Tools/u1_deep_tunnel_checks.py deep10  TUNNEL_FLOOR 10 (user 2026-10-17): lid at a 10 m shaft + steep slope choices
Results: docs/superpowers/results/2026-10-17-u1-deep-tunnels.md (full tables copied from stdout).
Model ports (CELL 0.5, ground dc_vertex_proto.top_of), from MUD_VoxelProbe.c on 2026-10-17 (after C1-C4, U2):
- tunnel(): CarveTunnel (capsule r 0.8, half 0.45), TUNNEL_MID 1.25, TUNNEL_AHEAD 1.1.
- bowl(): Carve's C1 cosine bowl + rim (CRATER_SLOPE 0.9, RIM 0.17, RIM_W 0.4), no stretch, no fade, no hard columns.
- sphere(): CarveSphere (C4, r = depth).
- clamp "old": d = max(d, floorY - y)  (today: forces solid below the floor, refills older air)
  clamp "new": d = max(d, min(old, floorY - y))  (no NEW air below the floor; old air stays)
Feet follow the dug floor one 0.5 m step per click (gap_checks.floor_under, step-up 0.6 m), as u2_level_check.py.
"""
import math
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dc_vertex_proto as P
import gap_checks as G

FLOOR = 3.0
TUNNEL_FLOOR = 5.0
TUNNEL_R, TUNNEL_HALF, TUNNEL_AHEAD, TUNNEL_MID = 0.8, 0.45, 1.1, 1.25
CRATER_SLOPE, RIM_SHARE, RIM_W_SHARE = 0.9, 0.17, 0.4
Z = 4.1


def clamp(d, old, floor_y, y, mode):
    if mode == "old":
        return max(d, floor_y - y)
    return max(d, min(old, floor_y - y))


def tunnel(Gr, mid, floor, mode):
    r, h = TUNNEL_R, TUNNEL_HALF
    gi0, gi1 = math.floor((mid[0] - r) / P.CELL), math.ceil((mid[0] + r) / P.CELL)
    gk0, gk1 = math.floor((mid[2] - r) / P.CELL), math.ceil((mid[2] + r) / P.CELL)
    gj0, gj1 = math.floor((mid[1] - h - r) / P.CELL), math.ceil((mid[1] + h + r) / P.CELL)
    for gk in range(max(gk0, 0), min(gk1, P.NZ - 1) + 1):
        for gi in range(max(gi0, 0), min(gi1, P.NX - 1) + 1):
            x, z = gi * P.CELL, gk * P.CELL
            floor_y = P.top_of(x, z) - floor
            for gj in range(max(gj0, P.J0), min(gj1, P.J0 + P.NY - 1) + 1):
                y = gj * P.CELL
                dy = y - mid[1]
                dy = dy - max(-h, min(h, dy))
                s = math.sqrt((x - mid[0]) ** 2 + dy * dy + (z - mid[2]) ** 2) - r
                old = Gr[gi, gj - P.J0, gk]
                Gr[gi, gj - P.J0, gk] = P.nudge(clamp(min(old, s), old, floor_y, y, mode))


def sphere(Gr, wp, r, floor, mode):
    for gk in range(max(math.floor((wp[2] - r) / P.CELL), 0), min(math.ceil((wp[2] + r) / P.CELL), P.NZ - 1) + 1):
        for gi in range(max(math.floor((wp[0] - r) / P.CELL), 0), min(math.ceil((wp[0] + r) / P.CELL), P.NX - 1) + 1):
            x, z = gi * P.CELL, gk * P.CELL
            floor_y = P.top_of(x, z) - floor
            for gj in range(max(math.floor((wp[1] - r) / P.CELL), P.J0), min(math.ceil((wp[1] + r) / P.CELL), P.J0 + P.NY - 1) + 1):
                y = gj * P.CELL
                s = math.sqrt((x - wp[0]) ** 2 + (y - wp[1]) ** 2 + (z - wp[2]) ** 2) - r
                old = Gr[gi, gj - P.J0, gk]
                Gr[gi, gj - P.J0, gk] = P.nudge(clamp(min(old, s), old, floor_y, y, mode))


def bowl(Gr, wp, depth, floor, mode, gj1_floor, rim_top_only=False):
    rimH = depth * RIM_SHARE
    crest = max(0.75, depth * (1 + RIM_SHARE) * math.pi / (2 * CRATER_SLOPE))
    rimW = max(max(0.5, depth * RIM_W_SHARE), depth * RIM_SHARE * math.pi / (2 * CRATER_SLOPE))
    outer = crest + rimW
    gj0 = math.floor((wp[1] - depth - 1) / P.CELL)
    gj1 = math.ceil((wp[1] + gj1_floor + rimH + 1.5) / P.CELL)
    for gk in range(max(math.floor((wp[2] - outer) / P.CELL), 0), min(math.ceil((wp[2] + outer) / P.CELL), P.NZ - 1) + 1):
        for gi in range(max(math.floor((wp[0] - outer) / P.CELL), 0), min(math.ceil((wp[0] + outer) / P.CELL), P.NX - 1) + 1):
            x, z = gi * P.CELL, gk * P.CELL
            rr = math.hypot(x - wp[0], z - wp[2])
            if rr >= outer:
                continue
            bump = rimH
            if rr > crest:
                bump = rimH * (1 + math.cos(math.pi * (rr - crest) / rimW)) * 0.5
            in_bowl = rr < crest
            bowl_y = wp[1] + rimH - (depth + rimH) * (1 + math.cos(math.pi * rr / crest)) * 0.5
            floor_y = P.top_of(x, z) - floor
            lo, hi = max(gj0, P.J0), min(gj1, P.J0 + P.NY - 1)
            top_j = lo - 1      # rim_top_only: the highest solid sample in the range (the column's top)
            for gj in range(hi, lo - 1, -1):
                if Gr[gi, gj - P.J0, gk] > 0:
                    top_j = gj
                    break
            for gj in range(lo, hi + 1):
                y = gj * P.CELL
                old = Gr[gi, gj - P.J0, gk]
                b = bump
                if rim_top_only and gj < top_j and old < 0:
                    b = 0.0     # air under the column's top = a buried hole (tunnel): the rim does not fill it
                d = old + b
                if in_bowl and bowl_y - y < d:
                    d = bowl_y - y
                Gr[gi, gj - P.J0, gk] = P.nudge(clamp(d, old, floor_y, y, mode))


def roof_over(Gr, wp):
    """RoofOver: undug - hit >= 0.75 and solid 0.5 .. 3 m above in the nearest column"""
    gi, gk = int(round(wp[0] / P.CELL)), int(round(wp[2] / P.CELL))
    if P.top_of(gi * P.CELL, gk * P.CELL) - wp[1] < 0.75:
        return False
    for gj in range(math.ceil((wp[1] + 0.5) / P.CELL), math.floor((wp[1] + 3) / P.CELL) + 1):
        if P.J0 <= gj < P.J0 + P.NY and Gr[gi, gj - P.J0, gk] > 0:
            return True
    return False


def dig(Gr, floor, mode, target, drop_max, x0=3.0, level_m=8.0, in_place=False):
    """Walk +x digging: drop_max per click down until feet reach target under the undug ground, then level.
    in_place: capsule straight under the camera (flat look = 0, today's straight-down case). Returns feet rows."""
    feet = [x0, P.top_of(x0, Z), Z]
    down = True
    rows = [(feet[0], feet[1])]
    level_from = None
    for _ in range(400):
        p = -drop_max if down else 0.0
        ahead = 0.0 if (in_place and down) else TUNNEL_AHEAD
        tunnel(Gr, (feet[0] + ahead, feet[1] + TUNNEL_MID + p, feet[2]), floor, mode)
        nx = feet[0] + (0.0 if (in_place and down) else 0.5)
        fy = G.floor_under(Gr, nx, Z, feet[1] + 0.6)
        if fy is None:
            break
        if in_place and down and fy > feet[1] - 0.01:
            break     # shaft stopped sinking (floor)
        feet = [nx, fy, Z]
        rows.append((nx, fy))
        if down and P.top_of(nx, Z) - fy >= target - 0.05:
            down = False
            level_from = nx
        if level_from is not None and nx - level_from >= level_m:
            break
        if not in_place and nx > x0 + 30:
            break
    return rows


def air_set(Gr, below):
    out = set()
    for gi in range(P.NX):
        for gk in range(P.NZ):
            t = P.top_of(gi * P.CELL, gk * P.CELL)
            for j in range(P.NY):
                y = (P.J0 + j) * P.CELL
                if y < t - below and Gr[gi, j, gk] < 0:
                    out.add((gi, j, gk))
    return out


def fresh(y0=-9, size_x=26):
    return G.grid(size_x, 8.5, y0, 4)


def clamp_check():
    print("### Clamp: blasts in / over a tunnel dug to feet ~4.5 m (TUNNEL_FLOOR 5)")
    print("Air = samples with value < 0 deeper than undug - 3 m (where FLOOR 3 clamps). Refilled = air before -> solid after.")
    print("New air = solid before -> air after (the chamber). Hit 'in tunnel' = tunnel floor + 0.3 m at x 12; 'surface' =")
    print("the undug ground over the tunnel at x 12. 'roof' = RoofOver at the hit (C4 sphere when true).")
    print("'rim top only' (bowl): the rim bump is not added to air under the column's top solid sample (a buried hole).")
    print()
    print("| Blast | Depth m | Hit | Roof | Op (floor) | Clamp | Air below undug-3 before | after | Refilled (all, incl. the rim over air above ground) | Refilled > 1 m under undug (tunnel) | Refilled below undug-3 | Refilled depth range m | New air |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    x = 12.0
    for label, depth in (("RPG HEAT", 0.60), ("81 mm", 1.43)):
        for hit in ("in tunnel", "surface"):
            for op_floor_name, op_floor, rim_top in (("FLOOR 3", FLOOR, False), ("TUNNEL_FLOOR 5", TUNNEL_FLOOR, False), ("FLOOR 3, rim top only", FLOOR, True)):
                for mode in ("old", "new"):
                    Gr = fresh()
                    dig(Gr, TUNNEL_FLOOR, "new", 4.5, 0.5)
                    if hit == "in tunnel":
                        fy = G.floor_under(Gr, x, Z, P.top_of(x, Z) - 1.0)
                        wp = (x, fy + 0.3, Z)
                    else:
                        wp = (x, P.top_of(x, Z), Z)
                    roof = roof_over(Gr, wp)
                    before = air_set(Gr, 3.0)
                    all_before = air_set(Gr, -10)
                    if roof:
                        sphere(Gr, wp, depth, op_floor, mode)
                        kind = "sphere"
                    else:
                        bowl(Gr, wp, depth, op_floor, mode, op_floor, rim_top)
                        kind = "bowl"
                    after = air_set(Gr, 3.0)
                    all_after = air_set(Gr, -10)
                    lost = all_before - all_after
                    refilled = len(lost)
                    deep = len(before - after)
                    ds = [P.top_of(gi * P.CELL, gk * P.CELL) - (P.J0 + j) * P.CELL for gi, j, gk in lost]
                    rng = f"{min(ds):.2f} .. {max(ds):.2f}" if ds else "-"
                    new_air = len(all_after - all_before)
                    print(f"| {label} | {depth} | {hit} | {roof} | {kind} ({op_floor_name}) | {mode} | {len(before)} | {len(after)} | {refilled} | {sum(1 for v in ds if v > 1.0)} | {deep} | {rng} | {new_air} |")
    print()


def lid_check(y0=-9):
    print(f"### Lid: a crater at the bottom of an open shaft dug straight down to the floor (in place, TUNNEL_FLOOR {TUNNEL_FLOOR:g})")
    print("Lid columns = columns with air that has solid above it (a 'ceil' crossing) that had none before the blast.")
    print()
    print("| Blast | Depth m | Shaft bottom under undug m | Carve gj1 reach | Clamp floor | Lid columns | New air samples |")
    print("|---|---|---|---|---|---|---|")
    x = 10.0
    for label, depth in (("RPG HEAT", 0.60), ("81 mm", 1.43)):
        for reach_name, reach in (("FLOOR 3 (today)", FLOOR), (f"TUNNEL_FLOOR {TUNNEL_FLOOR:g}", TUNNEL_FLOOR)):
            Gr = fresh(y0)
            rows = dig(Gr, TUNNEL_FLOOR, "new", 99, 0.5, x0=x - TUNNEL_AHEAD, in_place=True)
            bottom = rows[-1][1]
            wp = (x - TUNNEL_AHEAD, bottom, Z)

            def lids():
                out = set()
                for gk in range(P.NZ):
                    for gi in range(P.NX):
                        if any(k == "ceil" for _, k in G.crossings_down(Gr, gi, gk)):
                            out.add((gi, gk))
                return out
            before = lids()
            air0 = air_set(Gr, -10)
            roof = roof_over(Gr, wp)
            bowl(Gr, wp, depth, FLOOR, "new", reach)
            new = lids() - before
            print(f"| {label} | {depth} | {P.top_of(wp[0], Z) - bottom:.2f} (roof {roof}) | {reach_name} | FLOOR 3, new clamp | {len(new)} | {len(air_set(Gr, -10) - air0)} |")
    print()


def slope_check(drops=(0.7, 0.6, 0.5, 0.45, 0.4, 0.35, 0.3), target=4.5, y0=-9, size_x=26):
    print(f"### Slope: dig down at the max drop per click, to feet {target} m (TUNNEL_FLOOR {TUNNEL_FLOOR:g}), then 8 m level")
    print("Per 0.5 m step: feet drop. Angle = atan(drop / 0.5). Walk-out = the same steps climbed back. Target <= 35 deg.")
    print()
    print("| Max drop per click m (= pitch clamp x TUNNEL_SLOPE) | Ramp steps | Ramp length m | Mean deg | Max step deg | Max step drop m | Reached depth m |")
    print("|---|---|---|---|---|---|---|")
    detail = []
    for drop in drops:
        Gr = fresh(y0, size_x)
        rows = dig(Gr, TUNNEL_FLOOR, "new", target, drop)
        steps = []
        for (xa, ya), (xb, yb) in zip(rows, rows[1:]):
            depth_b = P.top_of(xb, Z) - yb
            steps.append((xb, ya - yb, depth_b))
        ramp = []
        for s in steps:
            ramp.append(s)
            if s[2] >= target - 0.05:
                break
        drops = [s[1] for s in ramp]
        length = ramp[-1][0] - rows[0][0]
        total = rows[0][1] - (rows[0][1] - sum(drops))
        mean = math.degrees(math.atan(sum(drops) / max(length, 0.01)))
        mx = max(drops)
        print(f"| {drop} | {len(ramp)} | {length:.1f} | {mean:.1f} | {math.degrees(math.atan(mx / 0.5)):.1f} | {mx:.2f} | {ramp[-1][2]:.2f} |")
        detail.append((drop, ramp))
    Gr = fresh(y0, size_x)
    rows = dig(Gr, TUNNEL_FLOOR, "new", 99, 0.7, in_place=True)
    print(f"| straight down (flat look 0, today) | {len(rows) - 1} | 0.0 | 90 | 90 | - | {P.top_of(rows[-1][0], Z) - rows[-1][1]:.2f} |")
    print()
    print("Per step (x m: drop m / depth m):")
    for drop, ramp in detail:
        print(f"- drop {drop}: " + ", ".join(f"{s[0]:.1f}:{s[1]:.2f}/{s[2]:.2f}" for s in ramp))
    print()


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    if what in ("clamp", "all"):
        clamp_check()
    if what in ("lid", "all"):
        lid_check()
    if what in ("slope", "all"):
        slope_check()
    if what == "deep10":
        TUNNEL_FLOOR = 10.0
        lid_check(-14)
        slope_check((1.0, 0.85, 0.7, 0.6, 0.5, 0.35), 9.5, -14, 40)
