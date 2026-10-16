"""Can the 0.25 m heightfield patch grid carry the mud sheet's rut shape? (2026-10-16)
Shape = MUD_MudSheet.c Stamp, BOX + SOFT: flat press 'half' wide with a cosine soft wall (SOFT_WALL 0.35 x half),
ridge band = BAND x half / HALF wide beside it, sin profile, height h. Values from the code (HALF 0.18, BAND 0.32,
HALF_PER_R 0.38, THICK 0.15, HMAX 0.28). Sampled on a grid at 0.15 m (the sheet) and 0.25 m (our fine patches),
cells split into triangles (linear), compared with the continuous shape on cross-sections.

    python Tools/mud_look_checks.py      results: docs/superpowers/results/2026-10-16-mud-look-checks.md
"""
import math

import numpy as np

HALF, BAND, SOFT_WALL, THICK = 0.18, 0.32, 0.35, 0.15


def shape(lat, half, h, band_scale=1.0):
    band = BAND * half / HALF * band_scale
    if lat < half:
        f = half - lat
        inw = half * SOFT_WALL
        pf = 0.5 * (1 - math.cos(math.pi * f / inw)) if f < inw else 1.0
        return -THICK * pf
    if lat < half + band:
        return h * math.sin(math.pi * (lat - half) / band)
    return 0.0


def grid_cross(cell, angle, phase, half, h, band_scale):
    """Height along a cross-section (perpendicular to the track) of the grid-sampled, triangle-interpolated shape."""
    a = math.radians(angle)
    dx, dz = math.cos(a), math.sin(a)          # track direction
    sx, sz = -dz, dx                            # across
    cx, cz = 5.0 + phase, 5.0 + phase * 0.37     # a point on the track centre line, moved against the grid

    def H(x, z):
        lat = abs((x - cx) * sx + (z - cz) * sz)
        return shape(lat, half, h, band_scale)

    out = []
    for s in np.arange(-1.0, 1.0001, 0.01):
        x, z = cx + s * sx, cz + s * sz
        i, k = math.floor(x / cell), math.floor(z / cell)
        tx, tz = x / cell - i, z / cell - k
        h00, h10, h01, h11 = H(i * cell, k * cell), H((i + 1) * cell, k * cell), H(i * cell, (k + 1) * cell), H((i + 1) * cell, (k + 1) * cell)
        if tx >= tz:          # tl-br split
            y = h00 + (h10 - h00) * tx + (h11 - h10) * tz
        else:
            y = h00 + (h11 - h01) * tx + (h01 - h00) * tz
        out.append((s, y, shape(abs(s), half, h, band_scale)))
    return out


def stats(cell, half, h, band_scale=1.0):
    peaks, floors, rms = [], [], []
    for angle in (0, 20, 45):
        for phase in np.linspace(0, cell, 7, endpoint=False):
            prof = grid_cross(cell, angle, phase, half, h, band_scale)
            peaks.append(max(y for _, y, _ in prof) / h)
            floors.append(sum(0.01 for _, y, _ in prof if y < -0.8 * THICK))
            rms.append(math.sqrt(np.mean([(y - t) ** 2 for _, y, t in prof])))
    true_floor = sum(0.01 for s in np.arange(-1, 1.0001, 0.01) if shape(abs(s), half, h, band_scale) < -0.8 * THICK)
    return np.mean(peaks), min(peaks), np.mean(floors), true_floor, np.mean(rms)


def main():
    print("| Tyre | half m | Ridge h m | Grid m | Ridge band x | Ridge peak kept mean / worst | Flat floor width drawn / true m | rms error m |")
    print("|---|---|---|---|---|---|---|---|")
    for label, r in (("UAZ r ~0.4", 0.40), ("Ural / M923 r ~0.6", 0.60)):
        half = min(0.28, max(0.11, 0.38 * r))
        for h in (0.08, 0.15):
            for cell, bs in ((0.15, 1.0), (0.25, 1.0), (0.25, 1.6)):
                pk, pw, fw, tf, e = stats(cell, half, h, bs)
                print(f"| {label} | {half:.2f} | {h} | {cell} | {bs} | {pk:.2f} / {pw:.2f} | {fw:.2f} / {tf:.2f} | {e:.3f} |", flush=True)


if __name__ == "__main__":
    main()
