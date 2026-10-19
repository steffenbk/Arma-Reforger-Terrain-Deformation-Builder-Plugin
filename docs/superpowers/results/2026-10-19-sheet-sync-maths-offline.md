# Synced sheet maths: cheaper slump / drag / grid, offline (2026-10-19)

Tool: `python Tools/mud_sheet_cost_exp.py [passes5 | straight]` (new; reuses Tools/mud_sheet_sync_proto.py ops +
felt rut field and Tools/mud_look_proto.py constants / metrics). MEASURED offline on copied game maths; nothing in
Scripts/ changed. The reference = the game today: 0.15 m cells, Stamp per op segment, MUD_MudSync.LimitDrag after
every stamp (2 sweeps over the stamp box), slump 3 passes over the op box + 1 cell. "Work" = cells each step touches
(the proxy for Enforce ms: the in-game parts are script loops over these cells).

Scenarios: base (truck, 2 tracks, arc R 25 m, 3 passes, 8 m/s, 24 ops), passes5 (5 passes, 40 ops), straight
(R 1000 m). Look metrics: rms / max height error vs the reference in the 8 x 5 m window, groove depth, ridge, width
at 50 %, wall p95, shape distance (profile scaled to its depth; 0.01 = same shape, joiner fallback was 0.18), dirt
area, texture stretch (|drag difference| between neighbour cells / cell size; p50 / p99 / max over touched cells).

## Results (base scenario; the other two in brackets where they differ)

| variant | rms / max m | shape dist | stretch p99 | drag work | slump work |
|---|---|---|---|---|---|
| REF (game today) | 0 / 0 | 0 | 0.40 | 46870 | 40419 |
| LimitDrag over the stamp's own cells +1 (per stamp) | 0 / 0 | 0 | 0.48 (0.60 / 0.40) | -64 % | - |
| LimitDrag once per op over its cells +1 | 0 / 0 | 0 | 0.49 (0.69 / 0.40) | -77 % | - |
| no LimitDrag | 0 / 0 | 0 | **2.77** | -100 % | - |
| press drag x soft-wall factor pf, no LimitDrag | 0 / 0 | 0 | **2.76** | -100 % | - |
| slump 1 pass, factor 0.5 | 0.0017 / 0.009 (0.0035 / 0.024) | 0.002 | 0.40 | - | -67 % |
| slump 1 pass, factor 0.6 | 0.0017 / 0.021 (0.0022 / 0.032; 0.0006 / 0.002) | 0.001 | 0.40 | - | -67 % |
| slump 1 pass, factor 0.25 (as today, fewer passes) | 0.0090 / 0.046 | 0.012 | 0.40 | - | -67 % |
| slump 3 passes, cells within 3 of the op's stamps | 0.0023 / 0.029 | 0.006 | 0.40 | - | -31 % |
| slump 1 pass f 0.6, cells within 1 of the stamps | 0.0031 / 0.030 (0.0050 / 0.036) | 0.006 | 0.40 | - | -87 % (-86 / -85) |
| slump off | 0.019 / 0.089 | 0.028 | 0.40 | - | -100 % |
| sync cells 0.20 m, drawn 0.15 (bilinear) | 0.018 / 0.113 | 0.058 | 0.30 | -38 % | -41 % |
| sync cells 0.30 m, drawn 0.15 (bilinear) | 0.031 / 0.200 | 0.039 | 0.20 | -67 % | -72 % |

Groove depth: REF 0.387 m; 1-pass slumps 0.385-0.389; sync 0.30 m 0.343 (-11 %; straight -14 %), width at 50 %
0.36 vs 0.32 m.

## Reading
- **Slump: one pass at factor 0.6 (or 0.5) gives the same mud as three passes at 0.25** - rms 0.6-2.2 mm, worst
  cell 2-3 cm, shape distance 0.001-0.004 in all three scenarios - for a third of the slump work. Limiting it to
  cells within 1 of the op's stamped cells cuts the work by 85-87 % for rms 3-5 mm / worst 3-3.6 cm. It also cuts
  the ground reads (GroundY), which run on the op box's even grid today: same mask -> about -60 % reads (inference
  from the cell counts).
- **LimitDrag is needed** - without it texture stretch p99 goes from 0.40 to 2.8 (a texel stretched ~3x per cell);
  scaling the press drag by the soft-wall factor does NOT fix it (my 2026-10-19 sec 3.3 guess, refuted here). But it
  does not need the whole stamp box: over the stamp's own cells +1 it does 64 % less work, stretch p99 0.48-0.60;
  once per op 77 % less, p99 up to 0.69 with 5 passes. Heights are untouched either way (drag is UV + dirt area only:
  dirt 32.2-32.5 vs 32.7 m2).
- **A coarser synced grid is a look trade, not a free win:** 0.30 m cuts every sync step by 67-76 % but the groove
  loses 11-14 % depth, gets ~12 % wider, and single cells differ by up to 0.2 m at groove walls. Needs a look test
  before anyone builds it; not recommended first.
- Deterministic: every variant is the same code on every machine; only the hash baseline changes.

## In-game estimate (inference from these work ratios x measured ms)
Sync per 5 s, one truck, DS client after P1 (sheet-alternatives sec 7): stamps ~60, slump ~70, sheet restore ~50,
drag limit 14-72 ms. With slump 1 pass f 0.6 near 1 (-85 %) and LimitDrag per stamp strip (-64 %): slump ~10,
drag ~5-26 -> total ~125-145 instead of ~195-250 ms per 5 s = about -40 % of the sync layer, on every client and
the DS, per driving vehicle. Assumes ms scale with cells touched (the loops are per cell; ground reads per even cell).

## Not shown
- One truck model, assumed latencies; no crossing tracks, no craters / bare cells, no zone edges.
- Prediction / replay cost (client-only) is not in the tool.
