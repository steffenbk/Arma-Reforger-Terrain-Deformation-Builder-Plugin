# Step C smoothing pass - SP test (2026-10-14, Exp 192624)

WB console logs_2026-10-14_19-39-10. 0.5 m cells, band scan on, SMOOTH_W 0.5, 1 pass, SMOOTH_KINDS 19 (crater,
tunnel, rut feel). Run A = SMOOTH_OPS true (19:52:55-19:55:50), run B = false (19:56:38-19:59:29). The two runs did
different things (A: 4 blasts + 41 tunnel digs, B: 47 blasts), so whole-run build numbers are not a clean A/B;
the per-op smoothing cost is.

## Per op kind (sums of the op log lines)

| run | kind | ops | samples smoothed | smooth ms (max per op) | op ms total | new cells carved | bricks queued |
|---|---|---|---|---|---|---|---|
| A on | blast | 4 | 4484 | 23 (7) | 102 | 110 | 186 |
| A on | rut | 312 | 36877 | 171 (2) | 236 | 228 | 1556 |
| A on | rut look | 334 | 0 | 0 (0) | 137 | 0 | 3340 |
| A on | tunnel dig | 41 | 7171 | 27 (1) | 32 | 0 | 410 |
| B off | blast | 47 | 0 | 0 (0) | 376 | 394 | 2463 |
| B off | rut | 336 | 0 | 0 (0) | 61 | 253 | 1559 |
| B off | rut look | 367 | 0 | 0 (0) | 179 | 0 | 4209 |

Checks: A's smoothed samples 4484 + 36877 + 0 + 7171 = 48532; B all 0 (switch really off).

## Whole run (sums of 'live' lines)

| run | span s | builds | verts per build | build ms/s | build ms per build | latency mean / max ms | worst frame max / mean ms |
|---|---|---|---|---|---|---|---|
| A on | 167.2 | 5598 | 46.7 | 33.3 | 0.99 | 46 / 136 | 11 / 9.1 |
| B off | 166.6 | 8476 | 38.8 | 38.9 | 0.77 | 50 / 124 | 12 / 9.7 |

## Findings

- Cost: a rut op goes from 0.18 to 0.76 ms (smoothing 0.55 ms each, ~118 samples). A blast: ~1100 samples, up to
  7 ms in one op. A tunnel dig: ~175 samples, < 1 ms.
- No frame got worse: worst frame 11 vs 12 ms, latency 46 vs 50 ms mean.
- Look (user, 2026-10-14): no real difference on vs off. Step C PARKED: SMOOTH_OPS = false, code kept behind the
  switch (SmoothBox).
