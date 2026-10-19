# All-offroad mud sheet: fps drop and layer-by-layer pop-in (deep dive, 2026-10-18)

Read-only study. No code changed, nothing launched. Run = DS + one client, `MUD_MudZoneEntity.ALL_OFFROAD` true
(H3 test). User: ~40 fps at times, often 50+, ~80 standing; tiles visibly load in "layer for layer" while driving.

Sources (all read for this doc):
- Offroad client log `PeerPlugin1/logs/logs_2026-10-18_14-31-06/console.log`, CSV `PeerPlugin1/profile/MUD_prof_28018.csv`
  (the log names this CSV: 'MUD prof: on, per-frame CSV $profile:MUD_prof_28018.csv').
- Offroad server log `server/logs/logs_2026-10-18_14-31-04/console.log`.
- Zone-only client log `PeerPlugin1/logs/logs_2026-10-18_14-11-54/console.log`, CSV `MUD_prof_26871.csv` (named in that log).
- Code: `MUD_MudSheet.c` (Tick, Frame, BuildZones, Radius, QueueNear, DropFar, DropMesh, BuildZoneTile, GroundDown,
  HoleGround, BuildTile, BuildTileMesh, FlushOneBuild, ReGroundAt, UploadPick), `MUD_MudZone.c` (TileCovered),
  `MUD_MudSync.c` (Get, ZoneTile, DsTick, MUD_MudSyncTile), `MUD_Prof.c`, `MUD_WorldGround.c` EOnFrame.

Measuring notes:
- `MUD_Prof` uses `System.GetTickCount` (1 ms steps). Per-frame part values are whole ms. Averages over hundreds of
  frames are fine; single-frame values are +-1 ms.
- "speed" = the 'MUD mud sheet PERF' line's top speed in that 5 s (m/s), not the average speed.
- GPU ms from `GetProfileData` reads ~0.01 ms while driving here. Not used for any conclusion (also flagged as odd in
  2026-10-18-sheet-alternatives-and-perf.md sec 6).
- Per-frame costs drop or rise with fps only because the frame count changes. To compare runs I also give ms per
  second (= ms/frame x fps).

## 1. Short answer

- MEASURED: the fps drop is our script, not render. Non-MUD frame time (frame minus MUD script) is 7.6 ms in the zone
  run and 7.8 ms offroad at the same speed (table 2.1). Render 3.8 vs 4.3 ms.
- MEASURED: at matched speed (~9-10 m/s) every MUD part costs the same ms per SECOND in both runs, except
  `sh zones` (camera-radius tile builds): +154 ms per second. That is the whole MUD increase (+157 ms/s).
- MEASURED: while driving >= 9 m/s, `sh zones` is non-zero in 100 % of frames and is 3 ms in most of them. So the
  radius builder builds about ONE tile every frame, and one tile costs ~3 ms (budget `RAD_BUILD_MS` is 2 ms, the first
  tile is always built, so the budget never allows a second).
- A simple model fits the numbers: fps = (1000 - 416 ms/s of other MUD work) / (7.84 non-MUD + 2.85 tile build) =
  54.6 fps. Measured 54.8 fps (rows of table 2.1). Without the tile build: ~75 fps (zone run measured 77).
- Pop-in: the builder makes ~1 tile per frame (= fps tiles per second, ~46-55/s while driving). Driving at 11 m/s
  needs ~62 new holed tiles per second inside the 40 m radius (sec 3). So it falls behind, the "to build" list holds
  91-281 tiles, and the built edge moves inward to ~25-32 m ahead (model). Tiles are built in SQUARE rings, nearest
  ring first, each ring in a fixed north-to-south order: that is the layer-by-layer look.
- Main structural cost: an untouched 3 m tile (plain THICK mud = the same terrain material lifted 0.16 m) costs as
  much to make as a rutted one: 121 ground reads + 441 sync reads + a 441-vertex mesh + skirts, ~3 ms. All offroad
  makes this cost scale with radius x speed.

## 2. Where the frame time goes (question 1)

### 2.1 Same speed, zone run vs offroad run (MEASURED, 'MUD prof 5 s' + 'MUD prof parts' lines)

Rows used: zone run 14:13:09, :14, :19, :24, :35, :40, :45 (7 rows, top speed 8.8-9.4 m/s); offroad run 14:32:37,
:42, :47, :52, :57, 14:34:02, :08, :28, :33, :38 (10 rows, top speed 9.2-10.4 m/s). Mean of the rows.

| value | zone | offroad |
|---|---|---|
| fps | 77.27 | 54.81 |
| frame ms | 13.01 | 18.36 |
| render ms | 3.81 | 4.30 |
| GPU ms (not trusted) | 0.70 | 0.03 |
| simulation ms | 7.61 | 13.06 |
| MUD script ms | 5.40 | 10.52 |
| non-MUD ms (frame - MUD) | 7.60 | 7.84 |

| part | ms/frame zone | ms/frame offroad | ms/s zone | ms/s offroad | delta ms/s |
|---|---|---|---|---|---|
| vox | 0.15 | 0.21 | 12 | 11 | -0 |
| sheet sync | 0.77 | 1.13 | 59 | 62 | +3 |
| sheet (whole, holds the sh parts) | 2.90 | 6.88 | 223 | 375 | +152 |
| sh follow | 0.33 | 0.51 | 26 | 28 | +2 |
| sh wheels | 0.49 | 0.71 | 38 | 39 | +1 |
| sh grounds | 0.09 | 0.08 | 7 | 4 | -3 |
| sh upload | 1.27 | 1.76 | 97 | 96 | -1 |
| sh stored | 0.68 | 0.94 | 52 | 51 | -1 |
| sh zones | 0.02 | 2.85 | 2 | 155 | +154 |
| look ruts | 0.24 | 0.32 | 18 | 17 | -1 |
| ahead | 0.67 | 1.16 | 51 | 63 | +12 |
| rebuild | 0.43 | 0.59 | 34 | 32 | -2 |
| free+net | 0.00 | 0.04 | 0 | 2 | +2 |
| MUD total | 5.40 | 10.52 | 416 | 573 | +157 |

Read it this way: upload, stored, sync etc. look bigger per frame offroad only because there are fewer frames. Per
second they are the same work. The new work is `sh zones` (BuildZones -> Radius -> BuildZoneTile).

At top speed (11-12.8 m/s rows, 14:32:17-27, 14:34:13-23) MUD script is 11.6-13.5 ms of a 18.9-21.8 ms frame
(57-62 %). There the other parts also grow with speed (more stamps per op), on top of the ~3 ms tile tax.

Render: +0.5 ms offroad (4.3 vs 3.8). INFERENCE: small; the earlier render A/B (sheet-alternatives doc sec 7, 354-742
stored meshes hidden) moved render by ~0.2 ms, inside noise. More meshes is not the fps problem.

### 2.2 Per-frame CSV, offroad run (MEASURED, MUD_prof_28018.csv, rows split by the log's 5 s frame counts)

Columns: window start | frames | fps | top speed | frame ms | MUD ms | non-MUD ms | render | simulation |
% frames with sh zones > 0 | mean sh zones ms when > 0 | frames with sh zones = 1/2/3/4/>=5 ms | % frames with
sh stored > 0 | % frames with sh upload > 0 | mean upload ms when > 0 | % frames with MUD >= 10 ms.
Speed -1 = no PERF line within 1.5 s.

| win | n | fps | spd | frame | MUD | non-MUD | rend | sim | zones>0 % | zones mean | zones 1/2/3/4/5+ | stored>0 % | upload>0 % | upload mean | MUD>=10 % |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 14:31:37 | 660 | 131.8 | -1.0 | 7.6 | 1.65 | 5.9 | 2.3 | 4.3 | 64 | 2.45 | 3/224/193/0/0 | 0 | 0 | 0.0 | 0 |
| 14:31:42 | 461 | 92.0 | -1.0 | 10.9 | 1.18 | 9.7 | 3.8 | 3.1 | 35 | 2.81 | 6/33/109/11/2 | 0 | 0 | 0.0 | 0 |
| 14:31:47 | 376 | 75.1 | 0.0 | 13.3 | 1.98 | 11.3 | 5.7 | 4.5 | 54 | 2.86 | 3/45/139/12/5 | 0 | 1 | 10.2 | 1 |
| 14:31:52 | 402 | 80.4 | 2.6 | 12.4 | 3.25 | 9.2 | 5.1 | 5.3 | 90 | 2.92 | 2/67/261/21/11 | 0 | 1 | 7.2 | 1 |
| 14:31:57 | 296 | 59.2 | 6.4 | 16.9 | 7.35 | 9.6 | 5.9 | 9.7 | 81 | 3.22 | 0/0/190/50/1 | 8 | 17 | 9.8 | 27 |
| 14:32:02 | 324 | 64.6 | 8.4 | 15.5 | 8.19 | 7.3 | 4.0 | 10.5 | 94 | 3.19 | 0/1/243/60/0 | 15 | 16 | 9.4 | 26 |
| 14:32:07 | 285 | 57.0 | 8.8 | 17.5 | 10.20 | 7.3 | 3.8 | 12.8 | 100 | 3.19 | 0/0/235/47/3 | 25 | 19 | 9.0 | 41 |
| 14:32:12 | 335 | 66.9 | 10.8 | 14.9 | 8.33 | 6.6 | 3.1 | 10.7 | 77 | 3.14 | 1/0/220/37/0 | 24 | 16 | 9.7 | 30 |
| 14:32:17 | 265 | 53.0 | 11.2 | 18.9 | 11.61 | 7.3 | 3.8 | 14.0 | 100 | 3.15 | 0/1/226/36/2 | 34 | 20 | 9.6 | 48 |
| 14:32:22 | 233 | 46.5 | 11.0 | 21.5 | 12.54 | 9.0 | 5.5 | 15.3 | 100 | 3.18 | 0/0/194/36/3 | 41 | 23 | 9.5 | 58 |
| 14:32:27 | 246 | 49.2 | 11.6 | 20.3 | 12.27 | 8.1 | 4.5 | 15.0 | 100 | 3.21 | 0/0/202/37/7 | 40 | 22 | 10.0 | 59 |
| 14:32:32 | 314 | 62.8 | 10.2 | 15.9 | 8.97 | 7.0 | 3.4 | 11.4 | 58 | 3.18 | 1/0/151/26/4 | 28 | 16 | 9.4 | 36 |
| 14:32:37 | 284 | 56.5 | 10.3 | 17.7 | 10.42 | 7.3 | 3.7 | 12.9 | 97 | 3.15 | 0/2/231/42/1 | 33 | 18 | 9.5 | 40 |
| 14:32:42 | 275 | 54.9 | 9.4 | 18.2 | 10.49 | 7.7 | 4.2 | 13.0 | 100 | 3.15 | 0/1/232/41/1 | 28 | 19 | 9.2 | 48 |
| 14:32:47 | 252 | 50.4 | 9.8 | 19.9 | 11.32 | 8.5 | 5.0 | 14.0 | 100 | 3.15 | 0/3/209/40/0 | 37 | 20 | 9.0 | 53 |
| 14:32:52 | 249 | 49.7 | 10.4 | 20.1 | 11.34 | 8.8 | 5.4 | 13.7 | 100 | 3.20 | 0/1/199/46/3 | 35 | 20 | 9.2 | 52 |
| 14:32:57 | 255 | 50.7 | 10.1 | 19.7 | 11.38 | 8.4 | 4.8 | 13.9 | 100 | 3.18 | 0/0/214/36/5 | 34 | 20 | 9.5 | 48 |
| 14:33:02 | 300 | 59.9 | 11.2 | 16.7 | 8.89 | 7.8 | 3.7 | 11.7 | 46 | 3.10 | 3/1/113/21/0 | 33 | 17 | 9.4 | 35 |
| 14:33:07 | 347 | 69.3 | 11.1 | 14.4 | 6.58 | 7.9 | 3.2 | 9.4 | 66 | 3.14 | 2/0/193/31/2 | 22 | 14 | 6.9 | 28 |
| 14:33:12 | 415 | 82.9 | 0.0 | 12.1 | 1.96 | 10.1 | 4.2 | 4.1 | 36 | 3.11 | 4/0/122/25/0 | 3 | 3 | 2.0 | 0 |
| 14:33:17 | 402 | 80.2 | 5.1 | 12.5 | 3.62 | 8.8 | 3.7 | 5.7 | 28 | 3.13 | 3/0/90/21/0 | 3 | 10 | 8.4 | 10 |
| 14:33:22 | 378 | 75.5 | 5.3 | 13.2 | 4.16 | 9.1 | 4.5 | 6.6 | 39 | 3.11 | 2/0/125/20/0 | 4 | 13 | 8.4 | 12 |
| 14:33:27 | 416 | 83.0 | 5.6 | 12.1 | 3.47 | 8.6 | 3.6 | 5.8 | 25 | 3.06 | 7/0/79/20/0 | 4 | 12 | 8.8 | 11 |
| 14:33:32 | 404 | 80.6 | 5.6 | 12.4 | 5.18 | 7.2 | 3.2 | 7.6 | 45 | 3.16 | 0/0/152/30/0 | 9 | 12 | 9.3 | 18 |
| 14:33:37 | 382 | 76.1 | 2.8 | 13.1 | 3.56 | 9.6 | 4.6 | 5.8 | 20 | 3.00 | 6/0/58/12/0 | 4 | 13 | 8.7 | 12 |
| 14:33:42 | 359 | 71.7 | 5.4 | 13.9 | 6.03 | 7.9 | 4.2 | 8.4 | 72 | 3.12 | 1/0/223/33/0 | 9 | 14 | 8.9 | 22 |
| 14:33:47 | 389 | 77.6 | 1.4 | 12.9 | 2.85 | 10.0 | 4.8 | 5.1 | 32 | 3.16 | 3/0/95/26/0 | 2 | 11 | 6.6 | 5 |
| 14:33:52 | 401 | 80.2 | 2.4 | 12.5 | 1.48 | 11.0 | 5.2 | 3.4 | 26 | 2.71 | 12/10/79/4/0 | 0 | 8 | 3.0 | 0 |
| 14:33:57 | 403 | 80.6 | 6.1 | 12.4 | 4.13 | 8.3 | 3.4 | 6.3 | 24 | 3.07 | 6/0/73/19/0 | 6 | 12 | 8.7 | 15 |
| 14:34:02 | 331 | 66.0 | 9.2 | 15.1 | 8.39 | 6.8 | 3.2 | 10.9 | 88 | 3.19 | 0/0/237/54/0 | 16 | 16 | 9.4 | 31 |
| 14:34:08 | 267 | 53.3 | 9.4 | 18.8 | 11.08 | 7.7 | 4.1 | 13.7 | 100 | 3.14 | 0/1/229/35/2 | 31 | 19 | 9.6 | 52 |
| 14:34:13 | 245 | 49.0 | 11.2 | 20.4 | 12.17 | 8.3 | 4.9 | 14.3 | 100 | 3.17 | 0/3/200/40/2 | 34 | 22 | 9.4 | 52 |
| 14:34:18 | 230 | 45.9 | 12.8 | 21.8 | 13.51 | 8.3 | 4.8 | 16.0 | 100 | 3.20 | 0/1/191/30/8 | 49 | 23 | 9.4 | 65 |
| 14:34:23 | 248 | 49.6 | 12.8 | 20.2 | 12.44 | 7.7 | 4.2 | 15.3 | 100 | 3.19 | 0/1/206/34/7 | 43 | 22 | 9.4 | 59 |
| 14:34:28 | 289 | 57.7 | 9.4 | 17.3 | 9.96 | 7.4 | 3.7 | 12.6 | 62 | 3.19 | 2/0/140/38/0 | 30 | 18 | 9.2 | 38 |
| 14:34:33 | 274 | 54.6 | 9.4 | 18.3 | 9.74 | 8.6 | 4.9 | 12.2 | 51 | 3.24 | 1/0/106/31/2 | 37 | 20 | 9.3 | 37 |
| 14:34:38 | 272 | 54.3 | 10.0 | 18.4 | 11.13 | 7.3 | 3.7 | 13.6 | 100 | 3.17 | 0/0/227/44/1 | 31 | 20 | 9.1 | 50 |

Checksums: 37 windows; frames sum to 12264 = the frames in the 37 log lines (CSV has 12573 rows; the last 309 rows
come after the last 5 s line). Per row the zones buckets add up to the zones>0 count (e.g. 14:32:22: 0+0+194+36+3 =
233 = 100 % of 233).

What it shows:
- Fast driving rows: sh zones runs in 100 % of frames, mostly 3 ms. That is the per-frame tile tax.
- Standing (14:33:12, speed 0.0): MUD 1.96 ms, fps 82.9. Still 36 % frames with a build (the backlog drains).
- The upload runs in ~20 % of frames (10 Hz at ~50 fps) and costs ~9.5 ms in those frames. Those are the frame spikes
  (MUD >= 10 ms in 48-65 % of fast frames = upload frames + tile tax + op steps).

### 2.3 Per-frame CSV, zone-only run (MEASURED, MUD_prof_26871.csv), same columns

| win | n | fps | spd | frame | MUD | non-MUD | rend | sim | zones>0 % | zones mean | zones 1/2/3/4/5+ | stored>0 % | upload>0 % | upload mean | MUD>=10 % |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 14:12:24 | 650 | 129.9 | -1.0 | 7.7 | 0.05 | 7.6 | 1.8 | 2.6 | 0 | 0.00 | 0/0/0/0/0 | 0 | 0 | 0.0 | 0 |
| 14:12:29 | 719 | 143.8 | -1.0 | 7.0 | 0.07 | 6.9 | 1.1 | 1.1 | 0 | 1.00 | 3/0/0/0/0 | 0 | 0 | 0.0 | 0 |
| 14:12:34 | 720 | 144.0 | -1.0 | 6.9 | 0.07 | 6.9 | 1.1 | 1.0 | 0 | 1.00 | 2/0/0/0/0 | 0 | 0 | 0.0 | 0 |
| 14:12:39 | 536 | 107.2 | -1.0 | 9.3 | 0.16 | 9.2 | 2.4 | 1.7 | 0 | 1.00 | 2/0/0/0/0 | 0 | 0 | 0.0 | 0 |
| 14:12:44 | 517 | 103.3 | -1.0 | 9.7 | 0.24 | 9.4 | 2.7 | 1.4 | 0 | 1.00 | 1/0/0/0/0 | 0 | 0 | 0.0 | 0 |
| 14:12:49 | 396 | 79.1 | 0.0 | 12.6 | 2.32 | 10.3 | 5.3 | 4.7 | 73 | 2.79 | 0/66/218/4/1 | 0 | 0 | 0.0 | 0 |
| 14:12:54 | 452 | 90.4 | 3.2 | 11.1 | 0.38 | 10.7 | 4.0 | 2.3 | 2 | 1.00 | 7/0/0/0/0 | 0 | 0 | 0.0 | 0 |
| 14:12:59 | 416 | 83.1 | 6.2 | 12.0 | 2.46 | 9.6 | 4.3 | 4.5 | 9 | 3.25 | 3/1/19/12/1 | 1 | 9 | 7.5 | 6 |
| 14:13:04 | 400 | 79.9 | 6.9 | 12.5 | 5.47 | 7.1 | 3.4 | 7.7 | 36 | 3.01 | 3/12/108/19/0 | 11 | 12 | 8.7 | 17 |
| 14:13:09 | 345 | 69.0 | 8.8 | 14.5 | 5.62 | 8.9 | 5.1 | 7.7 | 3 | 1.27 | 10/0/0/1/0 | 21 | 15 | 9.7 | 20 |
| 14:13:14 | 353 | 70.5 | 9.4 | 14.2 | 6.26 | 7.9 | 4.1 | 8.5 | 3 | 1.00 | 10/0/0/0/0 | 25 | 16 | 9.5 | 23 |
| 14:13:19 | 377 | 75.3 | 9.2 | 13.3 | 5.62 | 7.7 | 3.8 | 8.0 | 2 | 1.00 | 8/0/0/0/0 | 27 | 14 | 8.8 | 21 |
| 14:13:24 | 391 | 78.1 | 8.8 | 12.8 | 4.87 | 7.9 | 3.9 | 7.2 | 2 | 1.00 | 9/0/0/0/0 | 21 | 13 | 9.4 | 17 |
| 14:13:30 | 381 | 76.0 | 8.4 | 13.2 | 5.05 | 8.1 | 4.3 | 7.3 | 4 | 1.00 | 15/0/0/0/0 | 19 | 14 | 8.6 | 19 |
| 14:13:35 | 403 | 80.4 | 9.3 | 12.4 | 5.31 | 7.1 | 3.3 | 7.5 | 1 | 1.00 | 5/0/0/0/0 | 23 | 14 | 8.7 | 20 |
| 14:13:40 | 425 | 84.9 | 9.3 | 11.8 | 5.36 | 6.4 | 3.0 | 7.6 | 1 | 1.00 | 6/0/0/0/0 | 22 | 12 | 9.1 | 20 |
| 14:13:45 | 414 | 82.7 | 8.9 | 12.1 | 4.80 | 7.3 | 3.5 | 6.8 | 3 | 1.00 | 14/0/0/0/0 | 20 | 13 | 8.7 | 18 |
| 14:13:50 | 400 | 79.9 | 6.5 | 12.5 | 3.73 | 8.8 | 4.5 | 6.1 | 2 | 1.00 | 8/0/0/0/0 | 14 | 13 | 8.2 | 13 |
| 14:13:55 | 383 | 76.6 | 5.7 | 13.1 | 3.93 | 9.1 | 5.0 | 6.1 | 2 | 1.00 | 9/0/0/0/0 | 11 | 13 | 8.7 | 14 |
| 14:14:00 | 479 | 95.7 | 5.5 | 10.4 | 2.67 | 7.8 | 2.9 | 4.8 | 2 | 1.00 | 9/0/0/0/0 | 6 | 10 | 8.1 | 7 |
| 14:14:05 | 432 | 86.0 | 6.4 | 11.6 | 4.00 | 7.6 | 3.6 | 6.1 | 2 | 1.00 | 8/0/0/0/0 | 11 | 12 | 7.9 | 14 |
| 14:14:10 | 349 | 69.8 | 6.0 | 14.3 | 3.06 | 11.3 | 6.4 | 5.5 | 4 | 1.00 | 13/0/0/0/0 | 7 | 3 | 3.9 | 3 |
| 14:14:15 | 397 | 79.3 | 7.7 | 12.6 | 3.89 | 8.7 | 4.3 | 6.1 | 4 | 1.00 | 16/0/0/0/0 | 4 | 10 | 7.2 | 10 |
| 14:14:20 | 420 | 83.7 | 7.7 | 11.9 | 4.26 | 7.7 | 3.3 | 6.4 | 3 | 1.00 | 12/0/0/0/0 | 11 | 12 | 8.6 | 15 |
| 14:14:25 | 426 | 85.1 | 4.8 | 11.7 | 3.55 | 8.2 | 3.2 | 5.9 | 3 | 1.00 | 13/0/0/0/0 | 10 | 12 | 9.0 | 12 |

Checksums: 25 windows; frames sum to 11181 = the frames in the 25 log lines. In the zone run the 462 zone tiles are
built once (14:12:49) and then sh zones is ~0 (the 1 ms frames are the 0.25 s QueueNear / DropFar scans).

Note: the H1 snap probe run 5 (same zone log) and earlier runs gave ~85 fps / MUD ~3.2 ms; those blocks were rut-op
blocks at lower speed. The rows above are what this comparison uses.

## 3. The layer-by-layer loading (question 2)

### 3.1 Code path (VERIFIED in MUD_MudSheet.c)

1. `Tick` -> `Frame` -> `BuildZones` -> `Radius` (RadiusOn = RADIUS_ON and not ONE_GRID).
2. `Radius`: every `RADIUS_EVERY` 0.25 s, camera position -> `DropFar(cp)` then `QueueNear(cp)`.
3. `QueueNear` clears the build list, then walks SQUARE rings from ring `ceil(RADIUS / TILE_M) + 1` = 15 down to 0. A
   tile goes on the list if its middle is within `RADIUS` 40 m, it is not live, has no mesh, and `TileInZone`
   (= `MUD_MudZoneEntity.TileCovered`: with `ALL_OFFROAD` its middle is on holed ground, cached in `s_mOffroad`).
   Inside one ring the rows go dz = -ring .. +ring. Ring 0 is inserted last.
4. Same frame and every later frame: `Radius` pops from the END of the list (nearest ring first; inside a ring the
   +z (north) row first, -z row last) and calls `BuildZoneTile` while `built == 0 || elapsed < RAD_BUILD_MS` (2 ms).
5. `BuildZoneTile`: 11 x 11 `GroundDown` reads (hole cells: `HoleGround` = `MUD_VoxelProbe.ColumnAt`,
   `MUD_WorldGroundEntity.DugUndug`, `MUD_LookRuts.SheetFloorAt`; kept terrain: a TraceMove), 21 x 21
   `MUD_MudSync.Get` (one map lookup per cell), `FillTile`, `SpawnTile` (new entity), `BuildTile` -> `BuildTileMesh`
   (441 grid verts + edge cuts + `SKIRT_ALL` skirt 0.45 m down, two materials). The mesh shows in that frame.
6. No upload slot is involved for radius tiles. `UploadPick` / `UPLOAD_SLOTS` / `WHEEL_SLOTS_MAX` only pick the 16 live
   window slots round the vehicle.
7. `DropFar`: meshes past `RADIUS + RADIUS_HYST` 46 m are only dropped while more than `MESH_CAP` 600 meshes exist,
   farthest first (`DropMesh`: hidden now, deleted 1 s later, store removed with SYNC on).

### 3.2 Why rings show

- Build rate = 1 tile per frame (MEASURED: sec 2.2, 3 ms in 100 % of fast frames; and table 3.3: approach builds ~1
  per frame). One tile costs ~2.9 ms (= 155 ms/s of sh zones / ~52-60 approach builds per s; INFERENCE from 1 ms
  ticks + counts). The 'MUD sheet:' line's "mean 1.6 ms" is `BuildTileMesh` only (timed inside `BuildTile`); ground
  reads, sync reads and the spawn are not in it.
- Need: a 40 m disk moving at v sweeps 2 x 40 x v m2 per second = 8.9 v tiles/s; 63.6 % of new tiles are holed
  (MEASURED: 'all offroad' lookups 14:32:02 -> 14:33:02 = +2337 looked up, +1487 holed). Need = 5.65 v tiles/s.
- When supply < need, the unbuilt tiles pile up at the leading edge. Because the list is nearest-first, the builder
  always fills the nearest unbuilt square ring, so the built edge advances one ring at a time, in compass order.
  That is the "layer for layer" look.
- Every new tile is visible on arrival: it is the same terrain material (`MAT_TOP` = default_MatTerrainObj1 on both the
  sheet top and the WorldGround patches) but 0.16 m higher (`THICK` 0.15 + `LIFT` 0.01), and its open sides have the
  0.45 m skirt. `BuildTileMesh` fades only toward tiles that are not zone tiles (`HasTile` = `TileInZone`), so a
  built tile next to an unbuilt one has NO fade: a hard 16 cm ledge. INFERENCE: that ledge is what the eye catches.
- The 40 m edge itself is always a ledge too (README: "the mud ends with a step at the 40 m radius"). Even with a
  perfect builder, you would see the ledge 40 m ahead.

### 3.3 Build split per 10 s (MEASURED, 'MUD sheet:' + 'MUD sheet gaps:' + 'stored tiles rebuilt' lines)

builds = every `BuildTile` (all meshes). live = 'live builds' (window slots). stored re = sum of 'MUD mud sheet:
stored tiles rebuilt N' in that 10 s (FlushOneBuild). rest = builds - live - stored re = radius (approach) builds
plus window tiles frozen back to stored (in the zone run, where nothing is approached, rest is 36-126 per 10 s).
frames = frames in the 'MUD prof 5 s' lines inside that 10 s.

Offroad run:

| 10 s end | touched | meshes | builds | mean ms | live | stored re | rest | frames | rest/frame | stored meshes | dropped | to build |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 14:31:42 | 0 | 579 | 579 | 1.62 | 0 | 0 | 579 | 1121 | 0.52 | 579 | 0 | 276 |
| 14:31:52 | 0 | 628 | 624 | 1.62 | 65 | 0 | 559 | 778 | 0.72 | 612 | 526 | 147 |
| 14:32:02 | 55 | 627 | 1295 | 1.62 | 551 | 138 | 606 | 620 | 0.98 | 611 | 545 | 16 |
| 14:32:12 | 148 | 620 | 1483 | 1.59 | 554 | 279 | 650 | 620 | 1.05 | 604 | 550 | 91 |
| 14:32:22 | 274 | 624 | 1567 | 1.62 | 571 | 361 | 635 | 498 | 1.28 | 608 | 492 | 281 |
| 14:32:32 | 399 | 619 | 1476 | 1.60 | 569 | 355 | 552 | 560 | 0.99 | 603 | 447 | 34 |
| 14:32:42 | 428 | 624 | 1528 | 1.60 | 538 | 324 | 666 | 559 | 1.19 | 608 | 542 | 146 |
| 14:32:52 | 500 | 617 | 1488 | 1.61 | 517 | 347 | 624 | 501 | 1.25 | 601 | 508 | 184 |
| 14:33:02 | 593 | 624 | 1407 | 1.61 | 553 | 341 | 513 | 555 | 0.92 | 608 | 384 | 23 |
| 14:33:12 | 602 | 619 | 760 | 1.59 | 201 | 163 | 396 | 762 | 0.52 | 603 | 374 | 0 |
| 14:33:22 | 623 | 634 | 761 | 1.63 | 424 | 52 | 285 | 780 | 0.37 | 618 | 243 | 49 |
| 14:33:32 | 631 | 630 | 901 | 1.61 | 496 | 93 | 312 | 820 | 0.38 | 614 | 283 | 0 |
| 14:33:42 | 632 | 629 | 915 | 1.63 | 482 | 86 | 347 | 382 | 0.91 | 613 | 325 | 40 |
| 14:33:52 | 632 | 627 | 453 | 1.61 | 220 | 15 | 218 | 748 | 0.29 | 611 | 218 | 0 |
| 14:34:02 | 674 | 621 | 1125 | 1.59 | 535 | 132 | 458 | 804 | 0.57 | 605 | 387 | 69 |
| 14:34:12 | 775 | 624 | 1528 | 1.61 | 570 | 318 | 640 | 598 | 1.07 | 608 | 509 | 30 |
| 14:34:23 | 952 | 621 | 1630 | 1.61 | 569 | 421 | 640 | 475 | 1.35 | 605 | 482 | 91 |
| 14:34:33 | 1052 | 627 | 1326 | 1.63 | 555 | 354 | 417 | 537 | 0.78 | 611 | 311 | 14 |

Checksums: live 7970 + stored re 3779 + rest 9097 = 20846 = builds total. All 'stored tiles rebuilt' lines of the run:
2177 lines, 4088 tiles, 6560 ms (309 of those tiles fall outside the 18 windows). Dropped total 7126.

Zone run (for comparison):

| 10 s end | touched | meshes | builds | mean ms | live | stored re | rest | frames | rest/frame | stored meshes | dropped | to build |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 14:12:30 | 0 | 0 | 0 | 0.00 | 0 | 0 | 0 | 1369 | 0.00 | 0 | 0 | 0 |
| 14:12:40 | 0 | 0 | 0 | 0.00 | 0 | 0 | 0 | 1256 | 0.00 | 0 | 0 | 0 |
| 14:12:50 | 0 | 289 | 289 | 1.67 | 0 | 0 | 289 | 913 | 0.32 | 289 | 0 | 0 |
| 14:13:00 | 12 | 322 | 212 | 1.70 | 159 | 10 | 43 | 868 | 0.05 | 306 | 0 | 0 |
| 14:13:10 | 87 | 462 | 951 | 1.60 | 521 | 206 | 224 | 745 | 0.30 | 446 | 0 | 0 |
| 14:13:20 | 173 | 462 | 1042 | 1.59 | 569 | 358 | 115 | 730 | 0.16 | 446 | 0 | 0 |
| 14:13:30 | 194 | 462 | 932 | 1.61 | 538 | 298 | 96 | 772 | 0.12 | 446 | 0 | 0 |
| 14:13:40 | 220 | 462 | 1048 | 1.55 | 573 | 349 | 126 | 828 | 0.15 | 446 | 0 | 0 |
| 14:13:50 | 220 | 462 | 843 | 1.52 | 526 | 251 | 66 | 814 | 0.08 | 446 | 0 | 0 |
| 14:14:00 | 220 | 462 | 677 | 1.50 | 510 | 131 | 36 | 862 | 0.04 | 446 | 0 | 0 |
| 14:14:10 | 222 | 462 | 430 | 1.53 | 267 | 126 | 37 | 781 | 0.05 | 462 | 0 | 0 |
| 14:14:20 | 241 | 462 | 581 | 1.61 | 417 | 110 | 54 | 817 | 0.08 | 446 | 0 | 0 |

Checksums: live 4080 + stored re 1839 + rest 1086 = 7005 = builds total. 0 dropped.

Reading: offroad while driving fast, rest ~550-670 per 10 s, dropped ~450-550 per 10 s (the mesh count sits at the
MESH_CAP 600 + 16 live, so each new tile pushes one old one out). Approach builds ~50-60 per second. Backlog ('to
build' at the sample) 91-281 at 10-12 m/s, 0-49 at <= 6 m/s.

### 3.4 Need vs supply (need = 5.65 v; supply = 1 tile per frame = fps; front = 9 fps / (2 v 0.636), max 40 m)

| speed m/s | need tiles/s | fps seen at that speed | supply tiles/s | keeps up? | built edge ahead (model) |
|---|---|---|---|---|---|
| 5 | 28 | 75-83 (14:33:17-32) | 75-83 | yes | 40 m (cap) |
| 10 | 57 | 50-57 (14:32:37-57) | 50-57 | about | ~36-40 m |
| 11 | 62 | 46-53 (14:32:17-27) | 46-53 | no | ~30-34 m |
| 12.8 | 72 | 46-50 (14:34:18-23) | 46-50 | no | ~25-27 m |
| 20 (72 km/h, not driven) | 113 | unknown | unknown | no | ~14 m at 40 fps (extrapolated) |

INFERENCE: the model assumes all builds go to the front. Square rings also build the side tiles of each ring, so the
real edge is closer than the model says. Measured backing: the 'to build' backlog in table 3.3 rises with speed.

Pre-warm beyond 40 m cannot help while supply < need: it only adds tiles to build. A smaller radius cuts need in
proportion (R 25 m -> 3.5 v tiles/s, 39 at 11 m/s, which 50 fps keeps up with), but brings the ledge closer.

## 4. Is the method sound for ALL offroad? (question 3)

Verdict: sound for a 64 m test zone (462 tiles built once, then nothing), not sound as-is for all offroad.

Main structural cost: every holed 3 m tile within 40 m needs its own fine mesh, even where nobody drove. An untouched
tile is the same terrain material lifted 0.16 m. Making it costs the same as a rutted tile (~2.9 ms: 121 ground reads,
441 sync reads, a new entity, a ~441-vertex mesh + skirt). The work scales with radius x speed, and the builder spends
it at one tile per frame, so it is a fixed ~3 ms tax on every frame while driving faster than ~6-7 m/s.

What is NOT the problem (measured): render / draw calls (+0.5 ms here, render A/B earlier ~0.2 ms), the live window
upload, the sync layer, stored rebuilds - all equal per second to the zone run at the same speed (table 2.1).

### 4.1 Options, ranked by expected gain / effort

| # | option | fixes | expected gain | effort | risk | code |
|---|---|---|---|---|---|---|
| 1 | Don't build untouched tiles: radius tiles only where the sync layer has a tile (touched) or own stamps; plain ground shows elsewhere. Needs the edge fade (EDGE 1 m) toward non-built neighbours, i.e. `HasTile` = "has a sheet tile", and the live window the same rule | fps AND pop-in (nothing to pop on untouched ground) | removes ~155 ms/s at 10 m/s -> est. +20 fps (model: ~75 fps); meshes ~620 -> only touched ones | medium-high | LOOK CHANGE (the 16 cm mud only round ruts, sloping down over 1 m) = user decision; LAZY RUT MESH `Covers` must not count untouched ground; MudAt / trench ghosts | MUD_MudSheet QueueNear, BuildZoneTile, HasTile/BuildTileMesh fade, UpdateSlots/FillSlot, Covers; client only, sync layer unchanged (MP-safe) |
| 2 | Heading-first, true-distance order in QueueNear (sort by distance, tiles ahead of the camera / vehicle velocity first, behind last) | pop-in where you look (not fps) | the ~50 builds/s go to the tiles in view first | low | low (order only) | MUD_MudSheet.QueueNear |
| 3 | Cheaper approach tile: one sync tile lookup per sheet tile (skip 441 `MUD_MudSync.Get` calls when the sync layer has no tile there - true on untouched ground); first add a prof split (ground reads vs mesh) to see the real share | fps (and more tiles/s -> less pop-in) | part of the ~1.3 ms non-mesh share per tile (INFERENCE, unmeasured split) | low | low | MUD_MudSheet.BuildZoneTile, MUD_Prof parts |
| 4 | Smaller or speed-scaled radius (e.g. 25-30 m when driving fast) | fps when supply < need | need falls in proportion to R | very low (constant) | ledge comes closer | MUD_MudSheet.RADIUS / Radius |
| 5 | Make the budget a real knob: RAD_BUILD_MS < one tile means "exactly 1 per frame". A tiles-per-second cap (or 1 tile every Nth frame) trades fps for pop-in directly | fps (trade) | e.g. half the tax = ~+8-10 fps at 10 m/s (model) | very low | more pop-in | MUD_MudSheet.Radius |
| 6 | Fade at the build front / radius edge (treat "no mesh yet" as an open side) | hides the 16 cm ledge, softer pop | look only | medium | the faded tile must rebuild when its neighbour arrives (more builds at the front) | BuildTileMesh fade rule, Radius |
| 7 | Coalesce stored-tile rebuilds (a stored tile rebuilds at most every 0.3-0.5 s) and drop the per-frame 'stored tiles rebuilt' Print | fps (small) | sh stored ~51 ms/s now; 4088 rebuilds + 2177 log lines in this run | low | ruts behind the truck update a bit later (look only) | FlushOneBuild, ReGroundAt, QueueBuild |
| 8 | Coarse LOD tiles far away (0.6 m cells past ~20 m) | fps + pop-in | ~4x fewer reads and verts per far tile | high | patch ground (0.16 m below) may poke through a coarse sheet on rough ground; LOD switch = a rebuild; BuildTileMesh is near the 80 KB VM stack limit (mud-gotchas) | BuildTileMesh, BuildZoneTile, Radius |
| 9 | Merge tiles / fewer meshes for render | - | ~0 (render A/B: ~0.2 ms) | high | - | not worth it now |
| 10 | Pre-warm ring beyond 40 m | - | negative while supply < need | low | - | only after 1 or 3 make supply > need |

Order I would take (freeze-friendly, client-only): 2 + 3 first (small, safe), measure; then the user decides on 1
(look change) or 4/5 (trade). All options above are client-only look; none touches the synced layer maths.

### 4.2 Structural risk beyond fps (not a fix for now)

The sync layer keeps every touched tile forever: `MUD_MudSyncTile` = 7 arrays x 400 x 4 bytes = 11.2 KB, and
`MUD_MudSync.c` only adds tiles (`m_mTiles.Set`) or replaces the whole map at snapshot load - no removal (grep).
'tiles touched' went 674 -> 1052 in 31 s at 9-12.8 m/s (14:34:02 -> 14:34:33) = ~12 tiles/s = ~135 KB/s. INFERENCE:
~0.5 GB per hour of one truck driving fresh offroad, on every client AND the DS (`SERVER_LAYER`), and the late-joiner
snapshot grows the same way. In the 64 m zone this was capped at 462 tiles (~5 MB). With ALL_OFFROAD it is not.

## 5. Things that look like bugs (question 4)

| # | finding | evidence | effect |
|---|---|---|---|
| B1 | `RAD_BUILD_MS` 2 is not a budget: one tile is ~2.9 ms, the loop always builds the first tile, so it is "exactly 1 tile per frame". The comment says "~0.8 ms a tile" (stale) | code `Radius` (`built == 0 \|\| elapsed < RAD_BUILD_MS`); CSV 3 ms in 100 % of fast frames | fixed ~3 ms per frame tax while a backlog exists; build rate tied to fps |
| B2 | Build order = square (Chebyshev) rings, and inside a ring a fixed +z-first order; no drive direction | `QueueNear` loops | diagonal tiles of ring r (up to r x 4.2 m away) are built before nearer tiles of ring r+1; driving south, the leading edge of each ring is built last |
| B3 | 'MUD mud sheet: stored tiles rebuilt' prints in every frame that rebuilt one | 2177 lines / ~3 min; 4088 tiles, 6560 ms | log spam + Print cost (the DS rut floor Print was 2.19 ms/op, H1 run 5) |
| B4 | At start 579 tiles were built, then 526 dropped in the next 10 s | table 3.3 rows 14:31:42-52 | INFERENCE: the camera was elsewhere before the player spawned (menu camera); ~1.7 s of tile builds wasted at start |
| B5 | MESH_CAP 600 keeps ~600 meshes even though a 40 m disk holds ~355 holed tiles (559 tiles x 0.636, INFERENCE); the rest are old meshes behind | 'stored meshes' 601-618 all run | memory only; render cost ~0 per the earlier A/B; fine for now |
| B6 | DS: the sync layer drain on a DS is not a named MUD_Prof part (`MUD_MudSheet.Tick` returns after `MUD_MudSync.DsTick` with no `MUD_Prof.Add`) | code Tick; DS 'MUD prof parts' has no 'sheet sync'; DS 'MUD mud sheet sync (server)' lines: 44-502 ms per 5 s, e.g. 14:34:18 = 502 ms = ~3.3 ms/frame at 30 fps | the DS does client-only-useful work (kept only for late-joiner snapshots, Stage 5) that now scales with all offroad; hidden in the DS total (DS MUD mean 3.78 ms over 41 rows, DS holds 30 fps) |
| B7 | Sync tiles never freed | sec 4.2 | unbounded memory + snapshot size with ALL_OFFROAD |
| B8 | 'MUD sheet:' mean build ms (1.6) only times `BuildTileMesh`, not the ground / sync reads / spawn of `BuildZoneTile` | `BuildTile` timing | the log under-reports the real per-tile approach cost (~2.9 ms) by ~45 % |

No sign of the same radius tile being built over and over: approach builds (~50-60/s) match drops (~45-55/s) at the
mesh cap, i.e. new tiles in, old tiles out. Stored rebuilds of trail tiles (rut ops, look ruts, sync restores) are
~24/s while driving; whether one tile is rebuilt several times in a row is NOT measured (no per-tile log).

## 6. Not verified / open

- The split of the ~2.9 ms per approach tile into ground reads, sync reads, spawn and mesh (needs a prof part).
- That the visible "layers" are the 16 cm ledge + skirt (inference from code + the user's words; a screenshot of the
  build front would settle it).
- The equilibrium edge distances in 3.4 are a model, not measured on screen.
- Speeds are top speed per 5 s.
- All numbers are from one DS + client run; the zone comparison is a different route and speed profile, matched by
  picking rows of similar top speed.
