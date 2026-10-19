# Mesh (heightfield) ruts vs voxel ruts - DS logs (2026-10-19)

MESH = RUTS false (v228 = before), DS + PeerPlugin1 client logs_2026-10-19_19-28-56, one ~60 s drive.
VOXEL = control, DS + client logs_2026-10-19_00-14-4x (build of ~00:14, before v227 hub change), one ~80 s drive.
Different drives / routes, one run each: NOT the >= 6 run A/B the testing rules ask for.

## Driving-window averages (5 s 'MUD prof' windows)

| run | side | windows | fps | MUD script ms/frame | mean worst frame ms |
|---|---|---|---|---|---|
| mesh | client | 10 (19:29:45-19:30:30) | 88.7 | 3.35 (sum 33.523) | 35.9 |
| voxel | client | 17 (00:16:42-00:18:02) | 85.1 | 2.53 (sum 42.980) | 24.0 |
| mesh | server | 10 (19:29:46-19:30:31) | 30.0 (DS cap) | 5.61 (sum 56.140) | 52.2 |
| voxel | server | 17 (00:16:38-00:17:58) | 30.0 (DS cap) | 3.27 (sum 55.634) | 46.6 |

Client corrections (MUD corr 10 s, slow/mid/fast): mesh 6 in ~58 s driving; voxel 0 in ~105 s driving.
Mesh cost is the patch rebuild (client rb mesh ~1.3-1.9 + rb collider ~1.0-1.3 ms/frame; server rb collider 2.3-3.2 ms).

## Full per-window tables
```
== PeerPlugin1/logs/logs_2026-10-19_19-28-56/console.log
  corr 19:29:35 drive 0.0 s corr 0 0 0
  corr 19:29:45 drive 8.8 s corr 0 0 0
  corr 19:29:55 drive 10.0 s corr 0 0 2
  corr 19:30:05 drive 10.0 s corr 0 0 3
  corr 19:30:15 drive 8.9 s corr 0 0 1
  corr 19:30:25 drive 10.0 s corr 0 1 0
  19:29:30 fps  132.5 frame  7.55 max  100.0 | MUD 0.060 max 6
  19:29:35 fps   93.3 frame 10.72 max   90.5 | MUD 0.212 max 1
  19:29:40 fps   98.3 frame 10.17 max   25.9 | MUD 0.266 max 3
  19:29:45 fps   79.2 frame 12.63 max   46.3 | MUD 3.068 max 9
  19:29:50 fps   86.4 frame 11.58 max   47.8 | MUD 3.294 max 9
  19:29:55 fps   76.9 frame 13.00 max   34.8 | MUD 4.491 max 10
  19:30:00 fps   89.6 frame 11.16 max   29.2 | MUD 3.880 max 11
  19:30:05 fps   96.1 frame 10.41 max   43.1 | MUD 3.578 max 10
  19:30:10 fps  102.5 frame  9.76 max   30.4 | MUD 2.996 max 10
  19:30:15 fps   98.2 frame 10.19 max   26.5 | MUD 2.012 max 8
  19:30:20 fps   96.2 frame 10.39 max   28.5 | MUD 3.226 max 10
  19:30:25 fps   79.9 frame 12.52 max   33.2 | MUD 3.800 max 10
  19:30:30 fps   81.9 frame 12.21 max   39.0 | MUD 3.178 max 29
== server/logs/logs_2026-10-19_19-28-56/console.log
  corr 19:29:21 drive 2.9 s corr 1 0 0
  corr 19:29:31 drive 0.0 s corr 0 0 0
  corr 19:29:41 drive 4.8 s corr 0 0 0
  corr 19:29:51 drive 10.0 s corr 0 0 1
  corr 19:30:01 drive 10.0 s corr 0 0 0
  corr 19:30:11 drive 9.0 s corr 0 0 0
  corr 19:30:21 drive 10.0 s corr 0 0 0
  corr 19:30:31 drive 10.0 s corr 0 1 1
  19:29:16 fps   29.9 frame 33.50 max  100.0 | MUD 0.907 max 5
  19:29:21 fps   30.0 frame 33.38 max   41.4 | MUD 0.364 max 2
  19:29:26 fps   30.0 frame 33.28 max   49.0 | MUD 0.272 max 3
  19:29:31 fps   30.0 frame 33.34 max   58.8 | MUD 0.367 max 4
  19:29:36 fps   30.0 frame 33.34 max   48.4 | MUD 0.220 max 1
  19:29:41 fps   30.0 frame 33.37 max   54.7 | MUD 1.580 max 11
  19:29:46 fps   30.0 frame 33.35 max   46.2 | MUD 4.933 max 13
  19:29:51 fps   30.0 frame 33.34 max   63.0 | MUD 6.180 max 13
  19:29:56 fps   30.0 frame 33.28 max   43.9 | MUD 6.715 max 15
  19:30:01 fps   30.0 frame 33.38 max   48.8 | MUD 6.220 max 12
  19:30:06 fps   30.0 frame 33.33 max   41.3 | MUD 6.490 max 11
  19:30:11 fps   30.1 frame 33.28 max   50.3 | MUD 3.927 max 11
  19:30:16 fps   29.9 frame 33.39 max   43.4 | MUD 4.387 max 10
  19:30:21 fps   30.0 frame 33.34 max   58.4 | MUD 6.367 max 11
  19:30:26 fps   30.0 frame 33.31 max   73.2 | MUD 5.662 max 25
  19:30:31 fps   30.0 frame 33.33 max   53.2 | MUD 5.258 max 11
== PeerPlugin1/logs/logs_2026-10-19_00-14-46/console.log
  corr 00:15:22 drive 0.0 s corr 0 0 0
  corr 00:15:32 drive 6.2 s corr 0 0 0
  corr 00:15:42 drive 6.1 s corr 0 0 0
  corr 00:15:52 drive 0.0 s corr 0 0 0
  corr 00:16:02 drive 0.0 s corr 0 0 0
  corr 00:16:12 drive 0.0 s corr 0 0 0
  corr 00:16:22 drive 6.5 s corr 0 0 0
  corr 00:16:32 drive 0.0 s corr 0 0 0
  corr 00:16:42 drive 8.8 s corr 0 0 0
  corr 00:16:52 drive 10.0 s corr 0 0 0
  corr 00:17:02 drive 10.0 s corr 0 0 0
  corr 00:17:12 drive 10.0 s corr 0 0 0
  corr 00:17:22 drive 10.0 s corr 0 0 0
  corr 00:17:32 drive 9.6 s corr 0 0 0
  corr 00:17:42 drive 10.0 s corr 0 0 0
  corr 00:17:52 drive 10.0 s corr 0 0 0
  corr 00:18:02 drive 8.1 s corr 0 0 0
  00:15:17 fps  132.8 frame  7.53 max  100.0 | MUD 0.051 max 1
  00:15:22 fps  117.6 frame  8.50 max  100.0 | MUD 0.182 max 1
  00:15:27 fps  104.0 frame  9.61 max  100.0 | MUD 0.278 max 1
  00:15:32 fps  100.8 frame  9.92 max   19.7 | MUD 1.208 max 11
  00:15:37 fps  103.5 frame  9.66 max   20.8 | MUD 1.761 max 10
  00:15:42 fps   89.5 frame 11.18 max   35.8 | MUD 1.013 max 9
  00:15:47 fps  104.7 frame  9.55 max   37.3 | MUD 0.588 max 5
  00:15:52 fps  113.2 frame  8.83 max   16.9 | MUD 0.295 max 1
  00:15:57 fps  112.8 frame  8.86 max   18.2 | MUD 0.223 max 1
  00:16:02 fps  112.9 frame  8.86 max   18.2 | MUD 0.251 max 1
  00:16:07 fps  113.5 frame  8.81 max   18.0 | MUD 0.245 max 2
  00:16:12 fps  112.6 frame  8.88 max   18.8 | MUD 0.241 max 1
  00:16:17 fps  119.5 frame  8.37 max   17.1 | MUD 1.097 max 9
  00:16:22 fps   82.7 frame 12.09 max   47.9 | MUD 1.232 max 11
  00:16:27 fps   99.5 frame 10.05 max   31.1 | MUD 0.661 max 7
  00:16:32 fps  105.0 frame  9.52 max   19.4 | MUD 0.350 max 6
  00:16:37 fps   92.9 frame 10.76 max   19.5 | MUD 1.598 max 11
  00:16:42 fps   79.7 frame 12.54 max   27.7 | MUD 2.980 max 11
  00:16:47 fps   78.2 frame 12.79 max   20.8 | MUD 3.281 max 11
  00:16:52 fps   83.6 frame 11.97 max   34.1 | MUD 2.611 max 11
  00:16:57 fps   83.9 frame 11.91 max   21.2 | MUD 2.326 max 13
  00:17:02 fps   77.9 frame 12.83 max   41.6 | MUD 2.533 max 11
  00:17:07 fps   73.9 frame 13.54 max   29.9 | MUD 2.462 max 11
  00:17:12 fps   81.5 frame 12.27 max   23.9 | MUD 2.022 max 10
  00:17:17 fps   82.8 frame 12.08 max   19.4 | MUD 2.510 max 12
  00:17:22 fps   78.1 frame 12.81 max   21.8 | MUD 2.724 max 13
  00:17:27 fps  106.8 frame  9.37 max   25.5 | MUD 1.639 max 11
  00:17:32 fps   86.6 frame 11.55 max   21.6 | MUD 2.118 max 13
  00:17:37 fps   83.5 frame 11.97 max   22.5 | MUD 2.134 max 11
  00:17:42 fps   95.4 frame 10.48 max   18.5 | MUD 2.573 max 11
  00:17:47 fps   83.6 frame 11.97 max   19.8 | MUD 2.714 max 12
  00:17:52 fps   80.0 frame 12.50 max   20.8 | MUD 3.500 max 13
  00:17:57 fps   90.5 frame 11.05 max   20.4 | MUD 2.989 max 12
  00:18:02 fps  100.3 frame  9.97 max   18.3 | MUD 1.865 max 11
== server/logs/logs_2026-10-19_00-14-44/console.log
  corr 00:15:08 drive 2.9 s corr 0 0 0
  corr 00:15:18 drive 0.0 s corr 0 0 0
  corr 00:15:28 drive 1.8 s corr 0 0 0
  corr 00:15:38 drive 10.0 s corr 0 0 0
  corr 00:15:48 drive 0.5 s corr 0 0 0
  corr 00:15:58 drive 0.0 s corr 0 0 0
  corr 00:16:08 drive 0.0 s corr 0 0 0
  corr 00:16:18 drive 5.1 s corr 0 0 0
  corr 00:16:28 drive 1.5 s corr 0 0 0
  corr 00:16:38 drive 4.5 s corr 0 0 0
  corr 00:16:48 drive 10.0 s corr 0 0 0
  corr 00:16:58 drive 10.0 s corr 0 0 0
  corr 00:17:08 drive 10.0 s corr 0 0 0
  corr 00:17:18 drive 10.0 s corr 0 0 0
  corr 00:17:28 drive 9.9 s corr 0 0 2
  corr 00:17:38 drive 9.8 s corr 0 0 0
  corr 00:17:48 drive 10.0 s corr 0 0 0
  corr 00:17:58 drive 10.0 s corr 0 0 1
  00:15:03 fps   29.8 frame 33.52 max  100.0 | MUD 0.840 max 5
  00:15:08 fps   30.0 frame 33.37 max   40.1 | MUD 0.291 max 3
  00:15:13 fps   30.0 frame 33.29 max   42.1 | MUD 0.298 max 3
  00:15:18 fps   30.0 frame 33.33 max   41.6 | MUD 0.318 max 3
  00:15:23 fps   30.0 frame 33.33 max   56.4 | MUD 0.205 max 2
  00:15:28 fps   30.0 frame 33.35 max   49.4 | MUD 0.293 max 4
  00:15:33 fps   30.0 frame 33.38 max   43.1 | MUD 2.033 max 10
  00:15:38 fps   30.0 frame 33.34 max   51.3 | MUD 2.728 max 10
  00:15:43 fps   30.1 frame 33.28 max   36.2 | MUD 0.318 max 2
  00:15:48 fps   30.0 frame 33.33 max   43.7 | MUD 0.305 max 3
  00:15:53 fps   30.0 frame 33.34 max   39.9 | MUD 0.180 max 1
  00:15:58 fps   30.0 frame 33.33 max   40.2 | MUD 0.232 max 3
  00:16:03 fps   30.0 frame 33.34 max   38.4 | MUD 0.273 max 1
  00:16:08 fps   30.0 frame 33.34 max   40.5 | MUD 0.367 max 3
  00:16:13 fps   30.0 frame 33.34 max   36.0 | MUD 0.227 max 1
  00:16:18 fps   30.0 frame 33.33 max   43.4 | MUD 2.099 max 10
  00:16:23 fps   30.0 frame 33.33 max   45.0 | MUD 0.894 max 11
  00:16:28 fps   30.0 frame 33.34 max   49.8 | MUD 0.179 max 2
  00:16:33 fps   30.0 frame 33.33 max   37.7 | MUD 0.238 max 1
  00:16:38 fps   30.0 frame 33.35 max   45.7 | MUD 3.213 max 12
  00:16:43 fps   30.0 frame 33.34 max   45.5 | MUD 3.853 max 13
  00:16:48 fps   30.0 frame 33.32 max   46.5 | MUD 3.901 max 14
  00:16:53 fps   30.0 frame 33.32 max   45.5 | MUD 3.020 max 12
  00:16:58 fps   30.0 frame 33.38 max   45.0 | MUD 3.427 max 12
  00:17:03 fps   30.0 frame 33.34 max   43.4 | MUD 2.727 max 9
  00:17:08 fps   30.0 frame 33.30 max   52.6 | MUD 2.450 max 9
  00:17:13 fps   30.0 frame 33.33 max   45.0 | MUD 2.901 max 11
  00:17:18 fps   29.9 frame 33.40 max   48.1 | MUD 4.100 max 13
  00:17:23 fps   30.1 frame 33.25 max   45.8 | MUD 3.503 max 14
  00:17:28 fps   30.0 frame 33.34 max   49.1 | MUD 1.993 max 11
  00:17:33 fps   30.0 frame 33.38 max   43.6 | MUD 2.560 max 11
  00:17:38 fps   30.1 frame 33.28 max   43.9 | MUD 3.132 max 12
  00:17:43 fps   30.0 frame 33.34 max   43.8 | MUD 3.493 max 12
  00:17:48 fps   30.0 frame 33.34 max   55.2 | MUD 3.219 max 12
  00:17:53 fps   30.0 frame 33.38 max   46.0 | MUD 4.427 max 16
  00:17:58 fps   30.0 frame 33.36 max   46.6 | MUD 3.715 max 13
  00:18:03 fps   30.1 frame 33.27 max   45.6 | MUD 1.311 max 11
```

## Run 4: SAME session, old-rut box (MUD_OldRutZone x 1048-1118 z 2480-2550) vs outside, logs_2026-10-19_20-10-4x

Windows tagged by the server's 'MUD wheel: touches' lines (they print only on the heightfield press path = inside the box):
OLD = 3+ lines in the window, VOX = 0 lines + driving. Client windows tagged by the server times (OLD 20:11:55-20:12:35 + 20:13:21-20:13:41).

| side | box | windows | fps | MUD script ms/frame | mean worst frame ms |
|---|---|---|---|---|---|
| server | OLD (mesh) | 12 | 30.0 | 6.31 (sum 75.712) | 46.1 |
| server | VOX | 10 | 30.0 | 4.48 (sum 44.779) | 46.7 |
| client | OLD (mesh) | 9 | 88.9 | 3.93 (sum 35.403) | 26.7 |
| client | VOX | 8 | 90.9 | 3.59 (sum 28.731) | 19.4 |

Server parts: OLD rb collider 3.26 ms/frame, wheels 1.73; VOX rb collider 2.56, op carve 0.75. Most driving was 'fast'.
Corrections (client): spread over both, edge blocks mixed - not split.

```
== server/logs/logs_2026-10-19_20-10-41/console.log inside-box wheel lines 62
  20:10:59 mix  wheel-lines 2 fps   29.9 worst  100.0 MUD 1.340
  20:11:04 idle wheel-lines 0 fps   30.0 worst   38.6 MUD 0.360
  20:11:09 idle wheel-lines 0 fps   30.0 worst   42.6 MUD 0.367
  20:11:14 idle wheel-lines 0 fps   30.0 worst   36.4 MUD 0.307
  20:11:19 idle wheel-lines 0 fps   30.0 worst   36.0 MUD 0.245
  20:11:24 idle wheel-lines 0 fps   30.0 worst   38.8 MUD 0.340
  20:11:29 idle wheel-lines 0 fps   30.0 worst   36.0 MUD 0.351
  20:11:34 idle wheel-lines 0 fps   30.0 worst   37.7 MUD 0.467
  20:11:39 idle wheel-lines 0 fps   30.0 worst   42.0 MUD 0.377
  20:11:44 idle wheel-lines 0 fps   30.0 worst   43.3 MUD 0.427
  20:11:49 idle wheel-lines 0 fps   30.0 worst   50.3 MUD 0.411
  20:11:54 mix  wheel-lines 2 fps   30.0 worst   48.3 MUD 1.640
  20:12:00 OLD  wheel-lines 5 fps   30.0 worst   41.6 MUD 5.556
  20:12:05 OLD  wheel-lines 5 fps   30.0 worst   43.1 MUD 6.503
  20:12:10 OLD  wheel-lines 5 fps   30.0 worst   43.1 MUD 6.490
  20:12:15 OLD  wheel-lines 5 fps   30.0 worst   51.2 MUD 6.046
  20:12:20 OLD  wheel-lines 5 fps   30.0 worst   41.1 MUD 6.146
  20:12:25 OLD  wheel-lines 5 fps   30.0 worst   44.4 MUD 6.200
  20:12:30 OLD  wheel-lines 5 fps   30.0 worst   42.1 MUD 6.233
  20:12:35 OLD  wheel-lines 3 fps   30.0 worst   44.0 MUD 6.453
  20:12:40 VOX  wheel-lines 0 fps   30.0 worst   46.2 MUD 4.848
  20:12:45 VOX  wheel-lines 0 fps   30.0 worst   47.1 MUD 4.662
  20:12:50 VOX  wheel-lines 0 fps   30.0 worst   47.4 MUD 4.913
  20:12:55 VOX  wheel-lines 0 fps   30.0 worst   49.0 MUD 4.868
  20:13:00 VOX  wheel-lines 0 fps   30.0 worst   42.6 MUD 3.907
  20:13:05 VOX  wheel-lines 0 fps   30.0 worst   49.2 MUD 4.530
  20:13:10 VOX  wheel-lines 0 fps   30.0 worst   45.1 MUD 4.324
  20:13:15 VOX  wheel-lines 0 fps   30.0 worst   49.4 MUD 4.682
  20:13:20 VOX  wheel-lines 0 fps   29.9 worst   48.4 MUD 4.773
  20:13:25 OLD  wheel-lines 4 fps   30.0 worst   53.3 MUD 6.127
  20:13:30 OLD  wheel-lines 5 fps   30.1 worst   56.6 MUD 6.172
  20:13:35 OLD  wheel-lines 5 fps   30.0 worst   50.1 MUD 6.487
  20:13:40 OLD  wheel-lines 5 fps   30.0 worst   42.0 MUD 7.298
  20:13:45 mix  wheel-lines 1 fps   30.1 worst   55.0 MUD 5.483
  20:13:50 VOX  wheel-lines 0 fps   30.0 worst   42.6 MUD 3.272
  AVG OLD n 12 fps 30.0 MUD 6.31 (sum 75.712) worst 46.1
  AVG VOX n 10 fps 30.0 MUD 4.48 (sum 44.779) worst 46.7
   20:11:04 driving 5.11467 s | ours slow 2.94296 s / 0 corr, mid 1.69699 s / 0, fast 0.474721 s / 0
   20:11:14 driving 0 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 0 s / 0
   20:11:24 driving 0 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 0 s / 0
   20:11:34 driving 0 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 0 s / 0
   20:11:44 driving 0 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 0 s / 0
   20:11:54 driving 5.20371 s | ours slow 4.27025 s / 0 corr, mid 0.933461 s / 0, fast 0 s / 0
   20:12:04 driving 10.0353 s | ours slow 0 s / 0 corr, mid 2.2971 s / 0, fast 7.73822 s / 0
   20:12:14 driving 10.029 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 10.029 s / 0
   20:12:24 driving 10.0041 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 10.0041 s / 0
   20:12:34 driving 10.0347 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 10.0347 s / 0
   20:12:44 driving 10.0038 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 10.0038 s / 0
   20:12:55 driving 10.0218 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 10.0218 s / 0
   20:13:05 driving 10.0102 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 10.0102 s / 0
   20:13:15 driving 10.0219 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 10.0219 s / 0
   20:13:25 driving 10.0015 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 10.0015 s / 0
   20:13:35 driving 10.0042 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 8.40185 s / 0
   20:13:45 driving 10.0367 s | ours slow 0 s / 0 corr, mid 0 s / 0, fast 10.0367 s / 0
```

## Run 5: RUT WEAR (RUT_FIRST_D 0, 0.03 / pass, FIELD_SOIL_D + RUT_MAT_D 0.08), logs_2026-10-19_20-19-4x (~105 s driving)

| run | side | windows | fps | MUD script ms/frame | mean worst frame ms |
|---|---|---|---|---|---|
| wear, all driving | client | 22 (20:20:32-20:22:17) | 99.4 | 2.31 (sum 50.908) | 21.5 |
| wear, first 40 s | client | 8 | 103.5 | 1.66 (sum 13.309) | 18.5 |
| wear, later | client | 14 | 97.1 | 2.69 (sum 37.599) | 23.2 |
| wear, all driving | server | 22 (20:20:33-20:22:18) | 30.0 | 3.15 (sum 69.387) | 47.9 |
| wear, first 40 s | server | 8 | 30.0 | 2.49 (sum 19.938) | 47.9 |
| wear, later | server | 14 | 30.0 | 3.53 (sum 49.449) | 47.9 |

vs voxel before wear: run 3 client 3.23 / server 4.40; run 4 VOX client 3.59 / server 4.48 ms. Corrections: client 1, server 3 in ~105 s.
One run, route not controlled; early/late split = inference (fresh grass first, repeat passes later).

### Why run 5 is cheaper (MUD band 10 s lines + 'MUD prof parts' averages over the driving windows)

| side | part (ms/frame) | run 3 (19:36) | run 5 wear (20:20) |
|---|---|---|---|
| client | rebuild | 2.01 | 1.33 |
| client | rb mesh | 1.33 | 0.85 |
| client | rb collider | 0.67 | 0.46 |
| client | ahead | 0.78 | 0.56 |
| server | rb collider | 2.25 | 1.76 |
| server | op carve | 0.56 | 0.36 |
| server | wheels | 1.17 | 0.89 |
| server | ahead | 0.73 | 0.20 |

Band builds per 10 s while driving: client mesh 60 / 289 / 249 / 235 (run 3) vs 47 / 130 / 115 / 99 (run 5); server collider
111 / 108 / 113 / 97 vs 76 / 66 / 55 / 49. Mean ms per build about the same (4-7 ms) -> the gain = FEWER patch rebuilds, not cheaper ones.
Why fewer: unproven. Inference: a 3 cm pass dirties fewer patches than a 15 cm first cut (narrower relax / ridges, less degen),
and rear wheels now count a 2 cm rut as a rut and ride instead of cutting.

## Run 6 - perf idea 4: small collider skip ON (COL_SKIP_SMALL true, COL_SMALL_M 0.035) - DS 20:32 vs run 5 (DS 20:19, skip off)
Same wear build otherwise. Window = 22 x 5 s prof lines while driving (110 s). One run each - snaps need >= 6 runs to judge.

| | run 5 skip off (20:20:30-20:22:20) | run 6 skip on (20:33:35-20:35:25) |
|---|---|---|
| server MUD script avg | 3.15 ms | 2.79 ms |
| server colliders / 10 s (avg of 11) | 82 | 56 |
| server small collider skips / 10 s | - | 12, 67, 60, 50, 68, 53, 71, 65, 53, 59, 68 |
| client MUD script avg | 2.31 ms | 2.37 ms |
| client colliders / 10 s | 82 | 58 |
| client meshes / 10 s | 179 | 205 |
| client snaps ('mine 1') / driving s | 1 / 104.7 s | 3 / 107.6 s (body jumps 0.126, 0.125, 0.108 m, all at ~7.5 m/s, steer 1) |

Server colliders per 10 s run 6: 13, 91, 82, 88, 90, 83, 56, 30, 26, 26, 32 (first 6 heavy, last 5 light - driving changed mid-run).
Read: skip cuts server collider builds ~30 % and server script ~11 %. Client unchanged (client cost = look meshes, idea 2).
Snaps 1 -> 3 is within the noted run-to-run spread; not proven either way.

## Run 7 - perf idea 1: WEAR COUNT (WEAR_PASSES 1, FIELD_SOIL_D / RUT_MAT_D 0.06) + idea 4 skip - DS 20:44
Window 20:44:45-20:46:40 (23 x 5 s prof lines, 107 s driving). One client, no late joiner, test kit self-test not in the log.

| | run 6 skip on (20:33) | run 7 wear + skip (20:44) |
|---|---|---|
| server MUD script avg | 2.79 ms | 2.22 ms |
| server colliders / 10 s | 56 | 30 |
| client MUD script avg | 2.37 ms | 1.52 ms |
| client meshes / 10 s | 205 | 86 |
| client colliders / 10 s | 58 | 34 |
| client snaps / driving s | 3 / 107.6 s | 0 / 107.2 s |

'MUD rut wear 10 s' per bin, server (worn / cut): 28/0, 910/397, 191/963, 12/1078, 441/795, 3471/92, 1705/838, 1729/273,
2634/33, 2813/133, 2961/125. Sums: server worn 16895 cut 4727; client worn 16918 cut 4769 (bins are time-cut, not
op-cut; the client's last bin likely holds ops after the server's last print - inference, no hash line to prove it).
Server colliders per 10 s: 0, 0, 22, 24, 23, 29, 28, 44, 37, 34, 38, 53. Client meshes: 8, 73, 98, 83, 108, 70, 181, 113, 62, 68, 86.
Server ms rises late (2.3-3.5) with 'ahead' 0.7-1.7 ms/frame in prof parts = new patches on fresh ground, not ruts.
Driving differs per run (run 7 ~78 % of rut samples only worn = mostly fresh ground) - not a same-path A/B.

## Run 8 - perf idea 2: small MESH skip (MESH_SMALL_M 0.035, 0.5 s) + wear + collider skip - DS 20:52
Driving only ~66 s at the end (client 'driving' sum 65.96 s); averages over 5 s lines with script > 0.6 ms (client n=14,
server n=16) and 10 s lines with > 10 builds (n=7). User: look the same, fine.

| | run 7 (20:44) | run 8 (20:52) |
|---|---|---|
| server MUD script avg | 2.22 ms | 1.97 ms |
| client MUD script avg | 1.52 ms | 1.49 ms |
| client meshes / 10 s | 86 | 49 (driving bins: 37, 61, 66, 68, 78) |
| client small mesh skips / 10 s | - | 17, 26, 0, 0, 0, 60, 121, 117, 151, 105 |
| client snaps / driving s | 0 / 107 s | 1 / 66 s |
| server worn / cut samples | 16895 / 4727 | 6637 / 3631 |

Client band mesh split (10 s): input 72-85 ms for 61-78 builds = ~1.2 ms input per build. A skip returns after that
input pass (and is not in the split), and a skipped patch retries every LOOK_MESH_EVERY 0.25 s - so ~2 skips per build
each pay ~1.2 ms. Inference: that eats most of the saving; client ms did not move (1.52 -> 1.49, different driving).

## Run 9 - idea 2 skip fix (m_fMeshHoldTo) + idea 3 BandFill - DS 21:07
Driving ~84 s (client sum 83.93 s). Fill check (TEMP BAND_FILL_CHECK, every 10th fill vs the old per-vertex readers):
server 9 bins 0 vertices differ (55 checked fills), client 9 bins 0 differ (132 checked fills) - same output.
Note: the run 8 table's skip row is the MESH skip count (right); 'small collider skips' per run are listed below.

Whole-log sums from 'MUD band: ... split' (input ms / total ms) and 'MUD band: last 10 s' (builds):

| | run 8 (20:52) | run 9 (21:07) |
|---|---|---|
| client mesh: input / total / builds | 380 / 1465 / 344 | 339 / 1576 / 418 |
| client mesh input per build | 1.10 ms | 0.81 ms (incl. the check) |
| client mesh total per build | 4.26 ms | 3.77 ms |
| client collider: input / total / builds | 164 / 967 / 190 | 181 / 1106 / 232 |
| client collider input per build | 0.86 ms | 0.78 ms |
| server collider: input / total / builds | 215 / 1308 / 209 | 199 / 1349 / 220 |
| server collider input per build | 1.03 ms | 0.90 ms |
| fill check ms (client / server, all bins) | - | 97 / 38 |
| client small mesh skips / 10 s | 17, 26, 0, 0, 0, 60, 121, 117, 151, 105 | 5, 25, 23, 16, 26, 37, 44, 41, 31 |
| server small collider skips / 10 s | 0, 0, 39, 6, 0, 0, 13, 57, 64, 86, 72, 57 | 0, 0, 41, 65, 44, 41, 39, 55, 55, 60 |
| client MUD script avg (5 s lines > 0.6 ms) | 1.49 ms (n=14) | 1.49 ms (n=17) |
| server MUD script avg | 1.97 ms (n=16) | 2.03 ms (n=19) |
| client snaps / driving s | 1 / 66 s | 0 / 84 s |

Without the check (client 97 ms over 650 fills): input ~(339 + 181 - 97) / 650 = 0.65 ms per build vs run 8
(380 + 164) / 534 = 1.02 ms = about -35 % input. The input is only ~25 % of a build; mesher core + trimeshes are the rest,
so whole builds got ~10 % cheaper on the client mesh and ~2 % on colliders. Frame ms did not move (driving differs per
run). Mesh skips dropped to one per window as meant.

## Run 10 - PERF B7 (AHEAD_HIDE_SKIP) + ahead split - DS 21:31
Compiled ok. Client log PeerPlugin1 logs_2026-10-19_21-31-00. 'a hide' never printed (= 0: skip works). Presses 0 on
both sides (12 client / 14 server lines). Snaps 0.

| client | run 9 (21:07) | run 10 (21:31) |
|---|---|---|
| MUD script avg, driving lines (> 0.6 ms) | 1.49 ms (n=17) | 1.22 ms (n=24) |
| ahead avg, same lines | 0.68 ms | 0.26 ms |
| rebuild avg, same lines | 0.40 ms | 0.55 ms |
| look ruts / vox avg | 0.08 / 0.08 | 0.08 / 0.07 |
| frames (all prof lines) | 8928 | 16706 |
| ahead ms, whole run (avg x frames) | 5051 | 2961 |
| new patches made ('made N ahead' sum) | 171 | 87 |
| band mesh builds / colliders (sum) | 418 / 232 | 845 / 382 |
| mesh ms per build | 3.77 | 3.72 |

Run 10 ahead split (avg x frames per 5 s line, ms): a build 328, a view 1683, a spread 384, a scan 551; sum 2946 vs
ahead 2961 (rounding). a build = 3.8 ms per new patch.
Inference (routes differ): run 9 ahead without patch builds ~ 5051 - 171 x 3.8 = ~4400 ms / 8928 frames = ~0.49
ms/frame; run 10 = (1683 + 384 + 551) / 16706 = 0.16 ms/frame -> B7 saves ~0.33 ms/frame client (audit guessed 0.3-0.4).
Rebuild went up because this run made twice the mesh builds (more repeat passes); per build cost is the same.
Left in ahead: a view 0.10 ms/frame (AheadView without the hide: Contacts, BrakeCap, AheadPoint per wheel).

## Run 11 - REBUILD 5 (all 5 options on) - DS 22:15, sheet OFF, robot 120 s loop
Logs: PeerPlugin1 logs_2026-10-19_22-15-58, server logs_2026-10-19_22-15-57. Robot run 1 22:17:23-22:19:23, 119 s on
ours. Compiled ok. Checks: partial 'verts bad' 0 in every line (162 checked); bench 'differ' 0 (client 63 + server 16
benches). Snaps: 1 (22:19:17, body jump 0.26 m).

Client 'MUD rebuild 5' per 10 s line (12 lines, all on):
| line | held pf | held builds | same skips | partial n | part verts | part ms | full n | full ms | checked/bad | col builds | merged/build | col tris/build | bench n | old ms | new ms | differ | bench ms |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 226 | 14 | 0 | 31 | 135 | 14 | 19 | 7 | 3/0 | 25 | 124 | 1463 | 2 | 10 | 8 | 0 | 21 |
| 2 | 279 | 22 | 6 | 51 | 138 | 25 | 48 | 30 | 5/0 | 41 | 104 | 1601 | 4 | 23 | 20 | 0 | 45 |
| 3 | 371 | 22 | 34 | 133 | 280 | 76 | 63 | 37 | 13/0 | 54 | 86 | 1723 | 7 | 40 | 36 | 0 | 82 |
| 4 | 453 | 21 | 55 | 162 | 282 | 93 | 63 | 48 | 16/0 | 42 | 77 | 1785 | 6 | 35 | 31 | 0 | 72 |
| 5 | 374 | 20 | 48 | 156 | 314 | 87 | 68 | 43 | 15/0 | 34 | 78 | 1778 | 6 | 33 | 32 | 0 | 70 |
| 6 | 283 | 16 | 49 | 155 | 344 | 99 | 61 | 41 | 15/0 | 34 | 74 | 1789 | 5 | 25 | 25 | 0 | 51 |
| 7 | 349 | 22 | 53 | 156 | 285 | 83 | 58 | 34 | 15/0 | 33 | 71 | 1836 | 6 | 34 | 32 | 0 | 68 |
| 8 | 324 | 17 | 63 | 169 | 282 | 84 | 62 | 46 | 16/0 | 33 | 73 | 1834 | 6 | 32 | 28 | 0 | 69 |
| 9 | 398 | 23 | 63 | 173 | 273 | 97 | 64 | 49 | 17/0 | 36 | 78 | 1815 | 6 | 49 | 43 | 0 | 98 |
| 10 | 339 | 16 | 60 | 174 | 291 | 96 | 65 | 50 | 17/0 | 37 | 74 | 1819 | 6 | 37 | 32 | 0 | 74 |
| 11 | 329 | 16 | 58 | 154 | 282 | 78 | 61 | 38 | 15/0 | 31 | 75 | 1776 | 6 | 33 | 30 | 0 | 67 |
| 12 | 310 | 16 | 61 | 151 | 293 | 94 | 58 | 42 | 15/0 | 36 | 78 | 1827 | 5 | 46 | 46 | 0 | 99 |
Checksum: checked 3+5+13+16+15+15+15+16+17+17+15+15 = 162.
Bench sums client: old 397 ms, new 363 ms (-9 %). Server (12 lines): col builds 416, merged ~78 / build, bench old
117 new 96 ms; no mesh (DS).

Per-build cost, whole log (sum of 10 s lines):
| client | run 10 (21:31) | run 11 (22:15) |
|---|---|---|
| band mesh builds / colliders | 845 / 382 | 1241 / 454 |
| mesh input ms (run 11: + skipped passes) | 545 | 1450 |
| mesh core ms / build | 2.00 | 2.02 |
| mesh reveal+uv / object ms per build | 0.62 / 0.44 | 0.76 / 0.53 |
| collider core ms / build | 1.85 | 2.18 |
| collider trimesh ms / build | 2.18 | 2.05 |
| hot driving 10 s lines: mesh builds / mean ms | 102-112 / 3.80-3.90 | 107-115 / 3.94-4.13 |
| small mesh skips / 10 s (driving) | 23-62 | 55-63 |
Run 11 prof parts (24 lines, avg ms/frame): rebuild 0.86 (rb mesh 0.58, rb collider 0.30), ahead 0.27 (a view 0.19),
vox 0.10, look ruts 0.09. MUD script driving lines 1.72 ms (n=23) - a 2 min nonstop circle, not run 10's 34 s.
Reading (inference, routes / run lengths differ): no measurable win. Hot-driving build rate and ms per build are the
same as run 10's hot lines (rebuild ~0.8-1.0 ms/frame both). Same skips (~60 / 10 s) and partial passes still pay
the fixed input cost (~0.55 ms partial vs ~0.7 full per pass: BandFill, kept copy, skin); the pace only delays
builds; merge cuts 25 % of collider tris but trimesh ms ~ flat; the new mesher core is ~9 % faster in the bench, not
visible per build.
