"""U2 level digging (2026-10-17): which TUNNEL_MID keeps a tunnel dug looking straight ahead level?
  python Tools/u2_level_check.py            full table (per step) -> stdout; copy into results/2026-10-17-u2-level-dig.md
Walks down at pitch -0.7 to the target depth, then digs 20 m at pitch 0 (look level) and records the feet per 0.5 m
step: depth under the undug ground AND absolute height. The test ground (dc_vertex_proto.top_of) rises 0.04 m per m
along x, so a tunnel that is truly level shows its depth under the surface grow ~4 cm per m.
Model: gap_checks.carve_tunnel / floor_under (MUD_VoxelProbe.CarveTunnel, CELL 0.5) with G.FLOOR 6.
"""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dc_vertex_proto as P
import gap_checks as G

WALK = 20.0


def run(tunnel_mid, target, z=4.1, x0=3.0):
    G.FLOOR = 6.0
    Gr = G.grid(x0 + WALK + 8, 8.5, -9, 4)
    feet = [x0, P.top_of(x0, z), z]
    pitch = -0.7
    level_x = None
    level_y = None
    rows = []
    while True:
        mid = (feet[0] + G.TUNNEL_AHEAD, feet[1] + tunnel_mid + pitch * G.TUNNEL_SLOPE, feet[2])
        G.carve_tunnel(Gr, mid)
        nx = feet[0] + 0.5
        fy = G.floor_under(Gr, nx, z, feet[1] + 0.6)
        if fy is None:
            rows.append((nx, None, None))
            break
        depth = P.top_of(nx, z) - fy
        if pitch < 0 and depth >= target - 0.05:
            pitch = 0.0
            level_x, level_y = nx, fy
        feet = [nx, fy, z]
        if level_x is not None:
            rows.append((nx - level_x, depth, fy - level_y))
            if nx - level_x >= WALK:
                break
        elif nx > x0 + 15:
            break
    return rows


def main():
    print("| TUNNEL_MID | Target m | Level m walked | Feet height change m (abs) | per m | Depth under surface start -> end m |")
    print("|---|---|---|---|---|---|")
    detail = []
    for tm in (0.95, 1.25, 1.30, 1.35, 1.40, 1.45, 1.50):
        for target in (2.0, 3.0):
            rows = run(tm, target)
            ok = [r for r in rows if r[1] is not None]
            if len(ok) < 2:
                print(f"| {tm:.2f} | {target} | - | lost the floor | - | - |")
                continue
            walked = ok[-1][0]
            dy = ok[-1][2]
            print(f"| {tm:.2f} | {target} | {walked:.1f} | {dy:+.2f} | {dy / max(walked, 0.5):+.3f} | {ok[0][1]:.2f} -> {ok[-1][1]:.2f} |")
            detail.append((tm, target, rows))
    print()
    print("Per step (walked m: feet abs change m / depth under surface m):")
    for tm, target, rows in detail:
        s = ", ".join(f"{r[0]:.1f}:{r[2]:+.2f}/{r[1]:.2f}" for r in rows if r[1] is not None)
        print(f"- MID {tm:.2f} target {target}: {s}")


if __name__ == "__main__":
    main()
