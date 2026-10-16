# Can the 0.25 m ground grid carry the mud sheet's rut shape? (2026-10-16, offline)

Tool: `python Tools/mud_look_checks.py`. Shape = MUD_MudSheet.c Stamp with BOX + SOFT (CODE): a flat press `half`
wide (HALF_PER_R 0.38 x tyre radius, clamped 0.11-0.28), a cosine soft wall over SOFT_WALL 0.35 x half, pressed
THICK 0.15 m, then a sine ridge band BAND 0.32 x half / 0.18 wide, height h (sheet: H0 0.05 + 0.012 x speed +
0.8 x sink, max 0.28). Sampled on a 0.15 m grid (the sheet) and 0.25 m (our fine patches / rut field), triangles
interpolated, compared with the continuous shape on cross-sections; tracks at 0 / 20 / 45 deg, 7 grid offsets each.
MEASURED offline; tyre radii are ASSUMED round numbers.

| Tyre | half m | Ridge h m | Grid m | Ridge band x | Ridge peak kept mean / worst | Flat floor width drawn / true m | rms error m |
|---|---|---|---|---|---|---|---|
| UAZ r ~0.4 | 0.15 | 0.08 | 0.15 | 1.0 | 0.90 / 0.75 | 0.19 / 0.23 | 0.014 |
| UAZ r ~0.4 | 0.15 | 0.08 | 0.25 | 1.0 | 0.90 / 0.71 | 0.10 / 0.23 | 0.026 |
| UAZ r ~0.4 | 0.15 | 0.08 | 0.25 | 1.6 | 0.89 / 0.65 | 0.10 / 0.23 | 0.024 |
| UAZ r ~0.4 | 0.15 | 0.15 | 0.15 | 1.0 | 0.90 / 0.75 | 0.18 / 0.23 | 0.017 |
| UAZ r ~0.4 | 0.15 | 0.15 | 0.25 | 1.0 | 0.90 / 0.71 | 0.09 / 0.23 | 0.033 |
| UAZ r ~0.4 | 0.15 | 0.15 | 0.25 | 1.6 | 0.89 / 0.65 | 0.09 / 0.23 | 0.028 |
| Ural / M923 r ~0.6 | 0.23 | 0.08 | 0.15 | 1.0 | 0.96 / 0.91 | 0.25 / 0.35 | 0.014 |
| Ural / M923 r ~0.6 | 0.23 | 0.08 | 0.25 | 1.0 | 0.92 / 0.79 | 0.19 / 0.35 | 0.022 |
| Ural / M923 r ~0.6 | 0.23 | 0.08 | 0.25 | 1.6 | 0.98 / 0.93 | 0.19 / 0.35 | 0.021 |
| Ural / M923 r ~0.6 | 0.23 | 0.15 | 0.15 | 1.0 | 0.96 / 0.91 | 0.25 / 0.35 | 0.015 |
| Ural / M923 r ~0.6 | 0.23 | 0.15 | 0.25 | 1.0 | 0.92 / 0.79 | 0.18 / 0.35 | 0.028 |
| Ural / M923 r ~0.6 | 0.23 | 0.15 | 0.25 | 1.6 | 0.98 / 0.93 | 0.19 / 0.35 | 0.023 |

Reading:
- Ridges survive the 0.25 m grid: ~90 % of the peak on average (worst 71-79 %); a 1.6x wider band keeps 93-98 % for
  truck tyres. The sheet's 0.15 m grid itself keeps 90-96 %.
- The flat tyre floor comes out narrower on 0.25 m (0.09-0.19 m drawn vs 0.23-0.35 m true): the press reads as a
  rounded groove, not a flat box. Error roughly doubles vs the sheet (rms 0.021-0.033 vs 0.014-0.017 m).
- So the sheet's SHAPE needs no 0.15 m cells; what is lost is mostly the flat box floor of light-vehicle ruts.

## Patches touched per km of ruts (for the memory budget)
Random smooth drives, two wheel tracks 1.9 m apart (ASSUMED), +-0.5 m rut reach, 8 m patches: 211 patches for 1 km,
2063 for 10 km (~206 per km). With MEASURED 2026-10-09 patch memory (live ~340-400 KB; sleeping gives back 229-284 KB)
10 km of ruts = ~2000 patches - ESTIMATE ~0.12-0.35 GB asleep (56-171 KB each: live minus what sleep frees; the
mesh stays), ~0.7-0.8 GB if all live.
VERIFIED-CODE: rut-field patches (m_bFloor) are never freed (MUD_WorldGround.c:3817, :3842), may sleep (:3854), and
are NOT counted in m_iDeformed - so the 2000-patch budget warning (:976-981) never fires for them.
