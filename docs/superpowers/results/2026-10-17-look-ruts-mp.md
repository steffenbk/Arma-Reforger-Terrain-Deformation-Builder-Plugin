# Client look ruts - first MP run (2026-10-17, Loafhost DS + game client, Exp 1.8.0.13 client)

Logs: server archive-2026-10-17T215948+0000/console.log (UTC 21:39-21:59, 'look ruts 1', 1303 ops, no MUD script errors);
client ArmaReforger/logs/logs_2026-10-17_23-07-36/console.log (local = UTC+2). look_ruts 1 the whole time: NO A/B run.

## Result
- Works: look ruts ran on the client, 'no contact' 0-1 (only the player's own truck drove; other drivers not tested).
- Cost: look meshes up to 205 per 10 s, 3.3-4.1 ms each, up to 774 ms per 10 s (~7 % of frame time) while driving.
  Client fps driving 73-95 vs standing 84-116 - noisy, no A/B, so not attributed.
- Expired: 15-30 % of written samples ran to TTL 8 s (mesh pops back up). INFERENCE (not measured): the server's repose
  relax lifts the rut bottom, so its floor stays > CATCH shallower than the look cut. Fix built: REPLACE 3 s (drop +
  rebuild once a shallower server floor is there), counted as 'replaced' in the 10 s line.
- Client crash on leaving the server 23:56:56: native access violation (write at 0x10) after 'Game destroyed'.
  UNRESOLVED; earlier sessions with the same 'teardown left N geoms' warning did not crash; this session had 4 joins.
  Look ruts now also Reset() in the world ground destructor.

## Per-10 s client lines (whole table)
```
23:41:53.128 vehicle-frames 336 (no contact 0), cuts 33, samples written 507, caught up 278, expired 0, dirty marks 59, tiles 7, 25 ms | look meshes 38 (124 ms)
23:42:03.127 vehicle-frames 749 (no contact 0), cuts 78, samples written 1580, caught up 1242, expired 124, dirty marks 154, tiles 12, 39 ms | look meshes 106 (333 ms)
23:42:13.130 vehicle-frames 860 (no contact 0), cuts 81, samples written 1766, caught up 1130, expired 186, dirty marks 225, tiles 27, 44 ms | look meshes 159 (502 ms)
23:42:23.127 vehicle-frames 953 (no contact 0), cuts 95, samples written 2590, caught up 1699, expired 573, dirty marks 308, tiles 31, 57 ms | look meshes 187 (637 ms)
23:42:33.131 vehicle-frames 879 (no contact 0), cuts 94, samples written 2166, caught up 1483, expired 595, dirty marks 277, tiles 32, 63 ms | look meshes 205 (734 ms)
23:42:43.135 vehicle-frames 899 (no contact 0), cuts 95, samples written 2146, caught up 1318, expired 539, dirty marks 294, tiles 31, 42 ms | look meshes 188 (716 ms)
23:42:53.150 vehicle-frames 202 (no contact 0), cuts 20, samples written 404, caught up 344, expired 418, dirty marks 158, tiles 4, 12 ms | look meshes 88 (322 ms)
23:43:03.156 vehicle-frames 656 (no contact 0), cuts 61, samples written 864, caught up 450, expired 41, dirty marks 108, tiles 17, 35 ms | look meshes 75 (244 ms)
23:43:13.269 vehicle-frames 691 (no contact 0), cuts 75, samples written 874, caught up 315, expired 120, dirty marks 197, tiles 12, 37 ms | look meshes 156 (596 ms)
23:43:23.282 vehicle-frames 687 (no contact 0), cuts 78, samples written 756, caught up 321, expired 174, dirty marks 201, tiles 13, 42 ms | look meshes 162 (637 ms)
23:43:33.286 vehicle-frames 647 (no contact 0), cuts 79, samples written 1140, caught up 427, expired 245, dirty marks 234, tiles 11, 36 ms | look meshes 184 (683 ms)
23:43:43.282 vehicle-frames 847 (no contact 0), cuts 87, samples written 648, caught up 318, expired 324, dirty marks 231, tiles 10, 36 ms | look meshes 188 (774 ms)
23:43:53.290 vehicle-frames 180 (no contact 0), cuts 17, samples written 44, caught up 11, expired 160, dirty marks 120, tiles 2, 17 ms | look meshes 95 (376 ms)
23:44:13.305 vehicle-frames 527 (no contact 0), cuts 23, samples written 0, caught up 0, expired 0, dirty marks 0, tiles 0, 14 ms | look meshes 0 (0 ms)
23:44:23.311 vehicle-frames 938 (no contact 0), cuts 92, samples written 100, caught up 33, expired 0, dirty marks 64, tiles 2, 31 ms | look meshes 64 (261 ms)
23:44:33.319 vehicle-frames 1089 (no contact 0), cuts 92, samples written 176, caught up 77, expired 15, dirty marks 115, tiles 5, 31 ms | look meshes 104 (493 ms)
23:44:43.325 vehicle-frames 275 (no contact 0), cuts 19, samples written 20, caught up 12, expired 40, dirty marks 88, tiles 1, 15 ms | look meshes 74 (306 ms)
23:45:43.366 vehicle-frames 225 (no contact 1), cuts 3, samples written 57, caught up 13, expired 0, dirty marks 2, tiles 4, 10 ms | look meshes 2 (9 ms)
23:45:53.363 vehicle-frames 476 (no contact 0), cuts 35, samples written 366, caught up 78, expired 44, dirty marks 58, tiles 5, 21 ms | look meshes 38 (142 ms)
23:46:03.369 vehicle-frames 127 (no contact 0), cuts 10, samples written 62, caught up 22, expired 185, dirty marks 54, tiles 0, 19 ms | look meshes 38 (150 ms)
23:46:13.381 vehicle-frames 1 (no contact 0), cuts 1, samples written 7, caught up 0, expired 0, dirty marks 1, tiles 1, 4 ms | look meshes 1 (5 ms)
23:46:23.385 vehicle-frames 830 (no contact 0), cuts 78, samples written 762, caught up 229, expired 3, dirty marks 76, tiles 10, 40 ms | look meshes 59 (205 ms)
23:46:33.384 vehicle-frames 657 (no contact 0), cuts 43, samples written 0, caught up 0, expired 303, dirty marks 56, tiles 0, 36 ms | look meshes 34 (129 ms)
23:46:43.384 vehicle-frames 2 (no contact 0), cuts 1, samples written 0, caught up 0, expired 0, dirty marks 0, tiles 0, 9 ms | look meshes 0 (0 ms)
```

## Client fps / moving per 10 s
```
23:41:23.021 113.3 moving 0 of 2
23:41:33.019  90.5 moving 0 of 2
23:41:43.025 113.9 moving 0 of 2
23:41:53.034  85.8 moving 1 of 2
23:42:03.034  84.0 moving 0 of 2
23:42:13.046  93.7 moving 1 of 2
23:42:23.049  95.2 moving 1 of 2
23:42:33.059  88.0 moving 1 of 2
23:42:43.061  89.9 moving 1 of 2
23:42:53.078  87.4 moving 0 of 3
23:43:03.082  89.3 moving 0 of 3
23:43:13.193  76.9 moving 1 of 3
23:43:23.210  72.9 moving 1 of 3
23:43:33.215  72.4 moving 0 of 3
23:43:43.223  84.9 moving 0 of 3
23:43:53.221  90.4 moving 0 of 3
23:44:03.234  90.7 moving 0 of 3
23:44:13.242  88.4 moving 0 of 3
23:44:23.249  93.6 moving 0 of 3
23:44:33.254 108.7 moving 0 of 3
23:44:43.260 116.5 moving 0 of 3
23:44:53.265 100.3 moving 0 of 3
23:45:03.270 113.1 moving 0 of 3
23:45:13.269 113.5 moving 0 of 3
23:45:23.288  84.1 moving 0 of 3
23:45:33.282  90.6 moving 0 of 3
23:45:43.292  82.3 moving 0 of 3
23:45:53.290  89.4 moving 0 of 3
23:46:03.297 100.3 moving 0 of 3
23:46:13.303  96.5 moving 0 of 3
23:46:23.308 100.5 moving 0 of 3
23:46:33.307  99.9 moving 0 of 3
23:46:43.314 116.4 moving 0 of 3
23:46:53.310 171.6 moving 0 of 3
23:47:03.319 150.7 moving 0 of 3
23:47:13.320 114.1 moving 0 of 3
23:47:23.334 110.5 moving 0 of 3
23:48:30.967  58.2 moving 0 of 3
23:48:40.970  91.1 moving 0 of 3
23:48:50.982  90.1 moving 0 of 3
23:49:00.984  98.1 moving 0 of 3
23:49:10.990  77.4 moving 0 of 3
23:49:20.989  86.7 moving 0 of 3
23:49:31.001  88.7 moving 0 of 3
23:49:41.003  89.4 moving 0 of 3
23:49:51.007  69.6 moving 0 of 3
23:50:01.016  76.1 moving 0 of 3
23:50:11.018  83.3 moving 0 of 3
23:50:21.019  80.9 moving 0 of 3
23:50:31.023  98.0 moving 0 of 3
23:50:41.025  94.3 moving 0 of 3
23:50:51.034  88.7 moving 0 of 3
23:51:01.039  84.1 moving 0 of 3
23:51:11.043  83.4 moving 0 of 3
23:51:21.049  84.9 moving 0 of 3
23:51:31.053  87.1 moving 0 of 3
23:51:41.059  88.4 moving 0 of 3
23:51:51.072  92.2 moving 0 of 3
23:52:01.072  86.6 moving 0 of 3
23:52:11.077  93.7 moving 0 of 3
23:52:21.077 123.8 moving 0 of 3
23:52:31.076 129.0 moving 0 of 3
23:52:41.078 137.2 moving 0 of 3
23:52:51.084 137.8 moving 0 of 3
23:53:01.090 117.8 moving 0 of 3
23:53:11.093 108.5 moving 0 of 3
23:53:21.096  95.6 moving 0 of 3
23:53:31.100  99.4 moving 0 of 3
23:53:41.104  95.7 moving 0 of 3
23:53:51.104 100.4 moving 0 of 3
23:54:01.105 105.6 moving 0 of 3
23:54:11.112 101.5 moving 0 of 3
23:54:21.125 105.6 moving 0 of 3
23:54:31.137 107.1 moving 0 of 3
23:54:41.140 105.1 moving 0 of 3
23:54:51.140  90.2 moving 0 of 3
23:55:01.147  92.4 moving 0 of 3
23:55:11.154  86.0 moving 0 of 3
23:55:21.154  91.9 moving 0 of 0
23:55:31.159  92.5 moving 0 of 3
23:55:41.159  89.7 moving 0 of 0
23:55:51.168  94.2 moving 0 of 0
23:56:01.171  90.4 moving 0 of 3
23:56:11.182  85.5 moving 0 of 3
23:56:21.193  88.1 moving 0 of 3
23:56:31.194  88.0 moving 0 of 3
23:56:41.204  87.9 moving 0 of 3
23:56:51.212  87.3 moving 0 of 3
```
