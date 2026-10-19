# Look rut delay split (WB 2026-10-18 21:28-21:30, one steady offroad drive)

Log logs_2026-10-18_20-10-50/console.log, play run after the 21:26:49 compile. Workbench = where the wait goes, not perf.
cut = wheel at a segment's end -> its look cut (waits for the REARMOST wheel GAP 0.4 m past); drawn = oldest segment's
wheel -> patch mesh drawn; q = first look mark -> mesh built from the rebuild queue; w = patch-frames a look patch waited.

| time | cut avg | cut max | segs | drawn avg | drawn max | m behind | mark ready | mark bld | asleep | MISSING | w bld | w asleep | w sheet | w every | w lost | w budget | w index | q avg | q max | q n | other avg | other n | build ms |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 21:28:40.583 | 569 | 2092 | 670 | 1443 | 2304 | 6.94 | 65 | 9 | 0 | 0 | 424 | 0 | 0 | 304 | 65 | 31 | 0 | 127 | 309 | 64 | 245 | 7 | 4 |
| 21:28:50.600 | 500 | 1139 | 1108 | 1212 | 1477 | 7.12 | 154 | 15 | 0 | 0 | 1268 | 0 | 0 | 334 | 400 | 43 | 0 | 115 | 369 | 153 | 253 | 11 | 4 |
| 21:29:00.600 | 510 | 1139 | 1127 | 1231 | 1412 | 7.15 | 134 | 15 | 0 | 0 | 3711 | 0 | 0 | 486 | 191 | 47 | 0 | 101 | 314 | 135 | 183 | 11 | 5 |
| 21:29:10.621 | 515 | 1143 | 1126 | 1230 | 1480 | 7.14 | 162 | 10 | 0 | 0 | 5884 | 0 | 0 | 434 | 391 | 45 | 0 | 98 | 366 | 158 | 5725 | 10 | 5 |
| 21:29:20.633 | 504 | 1131 | 1093 | 1220 | 1453 | 7.10 | 151 | 2 | 0 | 0 | 4754 | 0 | 0 | 435 | 197 | 17 | 0 | 96 | 375 | 149 | 35109 | 2 | 5 |
| 21:29:30.657 | 492 | 1150 | 1126 | 1183 | 1426 | 6.99 | 142 | 10 | 0 | 0 | 3781 | 0 | 0 | 331 | 281 | 46 | 0 | 110 | 386 | 148 | 10270 | 11 | 5 |
| 21:29:40.671 | 488 | 1041 | 1133 | 1163 | 1398 | 7.08 | 142 | 16 | 0 | 0 | 6018 | 0 | 0 | 396 | 279 | 51 | 0 | 102 | 378 | 142 | 148 | 11 | 5 |
| 21:29:50.676 | 507 | 1139 | 1103 | 1217 | 1439 | 6.98 | 153 | 12 | 0 | 0 | 7345 | 0 | 0 | 472 | 246 | 40 | 0 | 98 | 386 | 153 | 159 | 12 | 5 |
| 21:30:00.684 | 464 | 1032 | 1161 | 1122 | 1363 | 6.88 | 174 | 17 | 0 | 0 | 6758 | 0 | 0 | 359 | 464 | 42 | 0 | 116 | 420 | 175 | 149 | 16 | 5 |
| 21:30:10.697 | 490 | 1094 | 1097 | 1184 | 1375 | 6.85 | 160 | 0 | 0 | 0 | 6020 | 0 | 0 | 446 | 303 | 45 | 0 | 114 | 359 | 159 | 60654 | 2 | 5 |
| 21:30:20.710 | 497 | 1151 | 1105 | 1203 | 1462 | 7.02 | 138 | 3 | 0 | 0 | 5593 | 0 | 0 | 454 | 208 | 36 | 0 | 105 | 370 | 139 | 272 | 3 | 5 |

rows 11; median: cut avg 500 ms, drawn avg 1212 ms, queue mark -> built avg 105 ms (all rows [127, 115, 101, 98, 96, 110, 102, 98, 116, 114, 105]); MISSING sum 0

## Reading
- The mesh side is small: first look mark -> mesh built ~0.1 s (median 105 ms), build ~5 ms, MISSING 0 (a fine patch
  is always there), budget / index waits tiny.
- The wait is the CUT RULE: a segment is cut only once the REARMOST wheel is GAP 0.4 m past it, checked every EVERY 0.1 s.
  At ~6 m/s (7 m in 1.2 s) a front-wheel segment waits about wheelbase + 0.4 m of driving; 'drawn' (oldest segment)
  ~1.2 s = ~7 m behind the FRONT wheel. INFERENCE (not measured per wheel): behind the rear wheel the rut shows after
  ~0.4 m + ~0.15 s of driving = ~1.3 m.
- 'other' builds (a patch's own build steps): 2-16 per 10 s, usually ~150-250 ms, rare outliers 6-60 s (patches far
  behind, not the wheel's).
