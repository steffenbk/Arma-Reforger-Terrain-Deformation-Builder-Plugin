# Mud two designs - DS client run (2026-10-20 00:24)

Prompt: prompts/2026-10-19-local-mud-two-designs.md. Plans: plans/2026-10-19-mud-overlay-v3.md (design 1),
plans/2026-10-19-pressed-lift-ruts.md (design 2). One DS run, user drove a truck on the client and cycled numpad 3:
off -> D1 p1 -> D1 p2 -> D2 p1 -> p2 -> p3 -> p4 (stayed on p4).

Logs: My Games/PeerPlugin1/logs/logs_2026-10-20_00-24-35/console.log (client; all lines below). Server log
server/logs/logs_2026-10-20_00-24-33 not read (both designs are client-only look).

Build: lag fixes compiled (ground reads only near stamps + ground cache, op poll 0.1 s, fallback 3 tiles/frame, replay
budget: quiet 1 s, every 2 s, 2 ms/frame), DbgUI mode label, reset to off on a new play.

## User verdict (eyes)

- D1: "horrible fps". Look blocky / patchy (screenshot earlier session).
- D2: "now worked but didn't look so good and was very slow to get any rut".
- Both look worse than the old MUD_MudSheet: "the original sheet looked way better with how it deformed and flowy it was".

## Client MUD prof, every 5 s window (whole table)

| time | state | fps | frame avg ms | MUD script ms/frame | pct | max ms |
|---|---|---|---|---|---|---|
| 00:25:08 | off (loading) | 133.9 | 7.47 | 0.07 | 0.9 | 6 |
| 00:25:13 | off | 84.0 | 11.91 | 0.30 | 2.5 | 1 |
| 00:25:18 | off | 84.2 | 11.88 | 0.36 | 3.0 | 1 |
| 00:25:23 | D1 p1 (on 00:25:21) | 94.8 | 10.55 | 1.42 | 13.4 | 8 |
| 00:25:28 | D1 p1 | 96.4 | 10.37 | 3.83 | 36.9 | 38 |
| 00:25:33 | D1 p1 | 71.8 | 13.93 | 6.26 | 44.9 | 82 |
| 00:25:38 | D1 p1 | 67.8 | 14.75 | 5.61 | 38.0 | 58 |
| 00:25:43 | D1 p1 | 69.5 | 14.38 | 6.62 | 46.0 | 89 |
| 00:25:48 | D1 p2 (on 00:25:46) | 79.1 | 12.64 | 3.85 | 30.4 | 39 |
| 00:25:53 | D1 p2 | 81.8 | 12.23 | 1.96 | 16.0 | 27 |
| 00:25:58 | D2 p1 (on 00:25:56) | 79.0 | 12.66 | 3.80 | 30.0 | 41 |
| 00:26:03 | D2 p2 (00:26:02) | 86.4 | 11.57 | 2.16 | 18.7 | 36 |
| 00:26:08 | D2 p3 (00:26:04) -> p4 (00:26:07) | 82.7 | 12.09 | 2.97 | 24.5 | 35 |
| 00:26:13 | D2 p4 | 90.5 | 11.05 | 1.95 | 17.6 | 9 |
| 00:26:18 | D2 p4 | 101.4 | 9.87 | 1.66 | 16.8 | 10 |
| 00:26:23 | D2 p4 | 95.3 | 10.50 | 1.21 | 11.6 | 53 |
| 00:26:28 | D2 p4 | 98.5 | 10.16 | 1.31 | 12.9 | 9 |
| 00:26:33 | D2 p4 | 102.3 | 9.77 | 1.16 | 11.9 | 10 |
| 00:26:38 | D2 p4 | 98.8 | 10.12 | 1.50 | 14.8 | 11 |
| 00:26:43 | D2 p4 | 98.8 | 10.12 | 1.28 | 12.7 | 12 |
| 00:26:48 | D2 p4 | 93.2 | 10.73 | 1.63 | 15.2 | 12 |
| 00:26:53 | D2 p4 | 99.7 | 10.03 | 1.42 | 14.2 | 11 |
| 00:26:58 | D2 p4 | 91.2 | 10.97 | 1.82 | 16.6 | 10 |
| 00:27:03 | D2 p4 | 92.7 | 10.79 | 2.56 | 23.7 | 13 |

Note: the first D2 windows (00:25:58, 00:26:03) still hold the D1 p2 tail and the view rebuild on each preset change.
The "off" windows had the truck standing (no ruts being made), so they are not a like-for-like baseline for driving.
Single run, not 6 runs - treat as a first look, not a final number.

## D1 overlay, 10 s lines (whole table)

| time | tiles (built / kept) | stamps live / re-centred / fallback | builds | stamps + re-centre ms | fallback ms | poll + keep ms | builds ms (ground reads / replays) |
|---|---|---|---|---|---|---|---|
| 00:25:31 | 58 (58 / 0) | 1527 / 104 / 0 | 624 | 1112 | 172 | 264 | 1193 (622 / 232) |
| 00:25:41 | 66 (65 / 1) | 2457 / 835 / 0 | 482 | 1036 | 105 | 651 | 1363 (605 / 469) |

Checksum 00:25:41: 1036 + 105 + 651 + 1363 = 3155 ms per 10 s (~700 frames at ~70 fps) = ~4.5 ms/frame;
the prof line says 5.6-6.6 ms/frame, the rest is the voxel ruts / patches running at the same time.
Biggest parts: builds (ground reads ~0.6 s / 10 s) and stamps + re-centre (~1.1 s / 10 s).

## D2 pressed lift, 10 s lines (whole table)

| time | preset | rutted verts | pressed out | half (wear) | drawn below heightmap | wear marks | lifted | soil shown |
|---|---|---|---|---|---|---|---|---|
| 00:26:06 | 3 | 4044 | 1514 | 22081 | 3058 | 1327 | 279061 | 7374 |
| 00:26:16 | 4 | 11335 | 1109 | 1673 | 2316 | 845 | 203863 | 11135 |
| 00:26:26 | 4 | 8195 | 2485 | 0 | 1569 | 563 | 111481 | 8416 |
| 00:26:36 | 4 | 12880 | 1771 | 0 | 2977 | 682 | 107288 | 10971 |
| 00:26:46 | 4 | 15918 | 4439 | 0 | 6318 | 142 | 95244 | 14344 |
| 00:26:56 | 4 | 21824 | 7598 | 0 | 9586 | 343 | 117804 | 19552 |
| 00:27:06 | 4 | 15565 | 18621 | 0 | 6407 | 1884 | 157052 | 26591 |

Counters are per built vertex (a vertex counts again each time its patch rebuilds). Preset 4 has WEAR_PRESS 0, so
"half (wear)" 0 is expected there.

## Why both look worse than the old sheet (code reading, INFERENCE until tested)

- Depth source. D1 draws its groove on the voxel rut floor; D2 presses only where that floor is cut or worn. Since
  RUT WEAR (v234) pass 1 on grass only counts wear and later passes cut 3 cm each, so for the first passes there is
  almost nothing to show. The old MudSheet pressed its own 15 cm mud layer from pass 1 (BOX + SOFT + GRADUAL stamps),
  so deep grooves showed at once.
- Flow. The old sheet had slump (SLUMP 0.25, 10 Hz, MudSync SLUMP_PER_OP 3), ridges / bow wave from the pushed-out
  volume, SMOOTH_EDGE, SKIRT_ALL and texture drag. D1 has none of slump / ridge flow / smooth edge / skirt, and only
  draws stamped quads on its 0.15 m grid, so it reads as blocky patches. D2 has no ridges or flow at all; it only
  lowers a lifted layer on the 0.25 m patch grid.
- Cost. D1 still re-stamps live per wheel per frame and reads ground for every rebuild; the old sheet paid a similar
  cost (that is why it was parked).
