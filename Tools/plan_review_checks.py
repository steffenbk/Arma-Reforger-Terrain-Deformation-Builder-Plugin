"""Second-opinion checks from the 2026-10-16 plan review (written by the independent reviewer, kept as-is).
  python Tools/plan_review_checks.py trace    U1/U2: tunnel feet depth per 0.5 m step (does digging stop at the target?)
  python Tools/plan_review_checks.py refill   U1: does a blast inside a FLOOR-6 tunnel refill it (Carve's FLOOR clamp)?
Results: docs/superpowers/research/2026-10-16-plan-review-second-opinion.md
"""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dc_vertex_proto as P
import gap_checks as G


def trace():
    for floor, target, mid_fix in ((3.0, 3.0, False), (6.0, 3.0, False), (6.0, 4.0, False), (6.0, 3.0, True)):
        G.FLOOR = floor
        Gr = G.grid(20, 8.5, -8, 4)
        z = 4.1
        feet = [3.0, P.top_of(3.0, z), z]
        pitch = -0.7
        depths = []
        while feet[0] < 16:
            tm = 1.25 if mid_fix else G.TUNNEL_MID
            p = pitch
            if mid_fix and pitch > 0: p = 0.0   # U2 as the plan writes it: TUNNEL_MID 1.25, look level
            mid = (feet[0] + G.TUNNEL_AHEAD, feet[1] + tm + p * G.TUNNEL_SLOPE, feet[2])
            G.carve_tunnel(Gr, mid)
            nx = feet[0] + 0.5
            fy = G.floor_under(Gr, nx, z, feet[1] + 0.6)
            if fy is None: break
            if pitch < 0 and P.top_of(nx, z) - fy >= target - 0.05:
                pitch = 0.3
            feet = [nx, fy, z]
            depths.append(round(P.top_of(nx, z) - fy, 2))
        print(f"FLOOR {floor} target {target} midfix {mid_fix}: feet depth per 0.5 m step:", depths)


def refill():
    G.FLOOR = 6.0
    Gr = G.grid(20, 8.5, -9, 4)
    z = 4.1
    feet = [3.0, P.top_of(3.0, z), z]
    pitch = -0.7
    while feet[0] < 16:
        mid = (feet[0] + G.TUNNEL_AHEAD, feet[1] + G.TUNNEL_MID + pitch * G.TUNNEL_SLOPE, feet[2])
        G.carve_tunnel(Gr, mid)
        nx = feet[0] + 0.5
        fy = G.floor_under(Gr, nx, z, feet[1] + 0.6)
        if fy is None: break
        if pitch < 0 and P.top_of(nx, z) - fy >= 4.0 - 0.05: pitch = 0.3
        feet = [nx, fy, z]
    def air_below(Gr, lim):
        n = 0
        for gi in range(P.NX):
            for gk in range(P.NZ):
                for j in range(P.NY):
                    y = (P.J0 + j) * P.CELL
                    if y < P.top_of(gi * P.CELL, gk * P.CELL) - lim and Gr[gi, j, gk] < 0: n += 1
        return n
    before = air_below(Gr, 3.0)
    x = 12.0
    fy = G.floor_under(Gr, x, z, P.top_of(x, z) - 1.0)
    P.FLOOR = 3.0  # craters keep FLOOR 3 (the plan: 'craters / trenches keep 3')
    c = P.Crater(x, z, 0.60); c.wy = fy + 0.3
    # apply like gap_checks CraterAt
    G.FLOOR = 3.0
    G.CraterAt(x, z, 0.60, fy + 0.3).apply(Gr)
    after = air_below(Gr, 3.0)
    print(f"tunnel feet at x=12: {P.top_of(x, z) - fy:.2f} m under undug; air samples deeper than undug-3: before blast {before}, after an RPG-size blast {after}")


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    if what in ("trace", "all"):
        trace()
    if what in ("refill", "all"):
        refill()
