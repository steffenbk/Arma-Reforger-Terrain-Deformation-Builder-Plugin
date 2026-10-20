# Mud lab stage 1 - first DS run (2026-10-20)

One DS session (Workbench peer tool), the user driving the Ural on grass, levers off in every row. Client log
PeerPlugin1/logs/logs_2026-10-20_21-14-45/console.log, server log server/logs/logs_2026-10-20_21-14-44 (no lab lines,
0 script errors - nothing runs on the DS, as meant). Workbench check before it: WB logs_2026-10-20_21-00-32 (one
compile error, COVER_R, fixed in 69060aa; no runtime script errors).

User verdict: "it actually didn't kill my performance". Gripe: "still quite obvious where the mesh is being raised" -
a striped wall along straight tile borders with a square corner beside the Ural's rut (screenshot in the chat).

Not a full DS table: one run per preset (the prompt wants >= 6 per row) and no "off" row in this run. Treat the
numbers as a first look, not a result.

## Every 10 s lab line, sheet local (client)

"live built" = live slots with a mesh (built - stored). Presets: 1 = BAND_IN 0.4 / FADE 0.8, 2 = 0.4 / 1.2,
3 = 0.8 / 0.8, 4 = 0.8 / 1.2. Columns wheels..band = ms per frame.

| time | surface | fps avg | fps min | MUD ms | mode ms | wheels | upload | stored | slump | band | live slots | live built | born | stored tiles | compact | op stamps | skipped own |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 21:15:47.671 | sheet local/1 | 101.5 | 24.9 | 2.88 | 1.90 | 0.28 | 1.14 | 0.00 | 0.10 | 0.46 | 11 | 8 | 69 | 58 | 0 | 1 | 220 |
| 21:15:57.707 | sheet local/1 | 83.2 | 24.4 | 4.57 | 3.08 | 0.55 | 1.66 | 0.03 | 0.16 | 0.72 | 13 | 9 | 165 | 149 | 3 | 0 | 463 |
| 21:16:07.730 | sheet local/1 | 74.1 | 18.7 | 4.53 | 3.04 | 0.46 | 1.69 | 0.03 | 0.19 | 0.71 | 14 | 9 | 234 | 220 | 0 | 0 | 373 |
| 21:16:17.742 | sheet local/1 | 80.3 | 31.7 | 2.60 | 1.75 | 0.14 | 1.26 | 0.00 | 0.13 | 0.49 | 15 | 14 | 247 | 195 | 37 | 0 | 72 |
| 21:16:27.749 | sheet local/1 | 74.0 | 11.8 | 3.89 | 2.56 | 0.15 | 1.54 | 0.09 | 0.16 | 0.63 | 13 | 9 | 260 | 244 | 3 | 0 | 82 |
| 21:16:37.736 | sheet local/1 | 79.6 | 12.0 | 2.73 | 1.74 | 0.10 | 1.22 | 0.04 | 0.11 | 0.39 | 13 | 9 | 271 | 258 | 0 | 0 | 39 |
| 21:16:53.962 | sheet local/2 | 92.4 | 22.4 | 2.29 | 1.49 | 0.18 | 0.92 | 0.00 | 0.07 | 0.47 | 15 | 11 | 51 | 36 | 0 | 0 | 145 |
| 21:17:03.971 | sheet local/2 | 82.6 | 21.1 | 4.23 | 2.88 | 0.35 | 1.81 | 0.01 | 0.14 | 0.90 | 15 | 13 | 132 | 116 | 0 | 0 | 304 |
| 21:17:13.974 | sheet local/2 | 86.2 | 24.3 | 3.12 | 1.91 | 0.15 | 1.36 | 0.02 | 0.09 | 0.63 | 12 | 11 | 149 | 137 | 0 | 0 | 102 |
| 21:17:23.985 | sheet local/2 | 85.4 | 20.2 | 3.80 | 2.67 | 0.29 | 1.67 | 0.03 | 0.13 | 0.84 | 15 | 14 | 188 | 173 | 0 | 0 | 251 |
| 21:17:35.236 | sheet local/3 | 81.0 | 20.0 | 4.08 | 2.91 | 0.35 | 1.74 | 0.02 | 0.14 | 0.88 | 15 | 14 | 93 | 76 | 0 | 2 | 251 |
| 21:17:45.241 | sheet local/3 | 76.8 | 19.7 | 5.26 | 3.74 | 0.54 | 2.04 | 0.04 | 0.17 | 1.15 | 12 | 12 | 212 | 200 | 0 | 0 | 386 |
| 21:17:55.243 | sheet local/3 | 76.1 | 23.7 | 3.60 | 2.35 | 0.19 | 1.64 | 0.02 | 0.11 | 0.78 | 16 | 14 | 236 | 220 | 0 | 0 | 102 |
| 21:18:05.248 | sheet local/3 | 80.2 | 16.5 | 3.95 | 2.55 | 0.28 | 1.47 | 0.08 | 0.17 | 0.65 | 16 | 16 | 252 | 236 | 0 | 0 | 165 |
| 21:18:15.424 | sheet local/4 | 72.8 | 20.3 | 5.75 | 4.21 | 0.41 | 2.62 | 0.06 | 0.16 | 1.64 | 16 | 15 | 102 | 86 | 0 | 3 | 321 |
| 21:18:25.426 | sheet local/4 | 66.5 | 20.2 | 7.18 | 5.00 | 0.58 | 2.86 | 0.10 | 0.21 | 1.81 | 16 | 11 | 223 | 207 | 0 | 0 | 397 |
| 21:19:53.590 | sheet local/1 | 82.0 | 29.0 | 4.71 | 2.76 | 0.33 | 1.59 | 0.01 | 0.14 | 0.61 | 14 | 10 | 75 | 61 | 0 | 6 | 241 |

Check: 17 sheet local rows (6 + 4 + 4 + 2 + 1) = the 17 'surface sheet local' lines in the log.

Other surfaces in the same run (one line each): D2 pressed lift/1 fps 72.6 MUD 3.41 ms; D2/3 fps 75.3 MUD 3.13 ms;
D1 overlay/1 fps 38.9 MUD 17.48 ms; D1 overlay/2 fps 64.3 MUD 8.75 ms.

## Findings

- MEASURED: sheet local costs 2.3-5.3 ms MUD script a frame at presets 1-3, 5.8-7.2 ms at preset 4. Upload (mesh
  builds) is the biggest part (0.9-2.9 ms), then band (0.4-1.8 ms).
- MEASURED: far data barely ran (compact 0-37 tiles, never the 80 m drop / return path) - part D is still untested.
- MEASURED: own rut ops are skipped as meant (39-463 skipped own per 10 s, 0-6 stamped).
- MEASURED: every row has 1-5 live window slots that are born but have no mesh (live vs live built), also in rows
  where 'born slots waited for traces' is 0. Sheet local skips quads whose 4 corner bands are all 0
  (MUD_MudSheet.BuildTileMesh), so a born slot outside the band draws nothing - that alone is not a bug.
- OPEN (cause NOT proven): the striped wall = the ONE_SKIRT curtain (0.45 m, SKIRT_ALL) of a raised tile with nothing
  drawn, or a lower tile, beside it. Candidates: the tile beside is not born / born with no mesh / its border verts
  untraced / its band is stale (0 there). Sheet local has no EDGE fade (HasTile returns true) - the design relies on
  the band reaching 0 inside born tiles.
- Added (not compiled, not run yet): EDGE AUDIT meter (EDGE_AUDIT, EdgeRecord / EdgeDrawn / EdgeAudit in
  MUD_MudSheet.c) - each build keeps its border verts; the 10 s 'MUD sheet builds:' line ends with
  'edge audit: raised border verts n: ok, step > 3 cm, open (beside not born / no mesh / vert untraced / band 0)'.
  Checkpoint v254 = before it.
- User report (not measured): a crater blown on the mud ruts shows a small delay before the mud takes the crater.
