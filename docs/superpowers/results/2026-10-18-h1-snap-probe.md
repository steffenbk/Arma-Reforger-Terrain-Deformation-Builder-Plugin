# H1 snap-distance probe + server cost split (DS 2026-10-18 13:14)

Build: v196 + snap probe (MUD_JoltProbe.NoteFloor / RutNear) + MUD_Prof w*/rb* parts. One DS run, user driving
circles in the zone ~151 s driving. Logs: PeerPlugin1/logs/logs_2026-10-18_13-14-03, server/logs/logs_2026-10-18_13-14-01,
CSV PeerPlugin1/profile/MUD_prof_29256.csv. User: "feels a little better"; sees gaps under the map while loading and the
ground "loads then sinks down".

## Snaps (client, 'MUD corr: ... mine 1' + 'MUD corr rut near:')

| time | jump m | dir m/s | dt s | dirty wheels | nearest fresh floor change m | its age s |
|---|---|---|---|---|---|---|
| 13:15:01.612 | 0.27 | fwd 6.1 | 0.045 | 4 | 2.66 | 0.034 |
| 13:15:06.416 | 0.31 | fwd 6.5 | 0.048 | 5 | 2.74 | 0.036 |
| 13:15:08.837 | 0.43 | fwd 8.3 | 0.051 | 4 | 3.25 | 0.041 |
| 13:15:32.117 | 0.26 | rev 5.4 | 0.048 | 1 | 3.84 | 0.046 |
| 13:15:32.915 | 0.26 | rev 5.6 | 0.047 | 4 | 2.64 | 0.034 |
| 13:15:40.119 | 0.26 | rev 5.5 | 0.048 | 6 | 2.30 | 0.033 |
| 13:15:41.754 | 0.29 | rev 5.6 | 0.015 | 6 | 2.81 | 0.062 |
| 13:16:01.113 | 0.24 | rev 5.5 | 0.044 | 6 | 2.32 | 0.032 |
| 13:16:56.819 | 0.11 | fwd 1.4 | 0.021 | 0 | 2.34 | 0.542 |
| 13:17:26.860 | 0.29 | rev 5.5 | 0.052 | 6 | 2.81 | 0.049 |

Checksum: 10 rows = 10 'mine 1' lines = 10 'rut near' lines. Wheels within 0.5 m of a fresh change: 0 in all 10.
Dirty-patch time: 29.1 s of 151.3 s driving (19 %).

- 9 of 10 snaps came 0.03-0.06 s (1-2 frames) after a rut op changed the floor on this machine; ops come every
  ~0.75 s, so by chance ~8 % would. None had a wheel near the change -> NOT the wheel touching new ground.
- Most snap frames have dt 0.044-0.052 s (normal ~0.010).

## Client hitch = rut op apply ('vox' part, MUD_VoxelProbe.Tick)

CSV, 18082 frames: 125 frames over 33 ms; 104 of them come right after a frame with vox >= 10 ms (cost shows in the
next frame's dt). Frames with vox >= 10 ms: 143, next-frame dt median 35.7 ms. vox spike size (every 10th, sorted):
10 12 14 15 16 17 18 18 19 19 20 22 29 31 40 ms.

Control (5 s blocks whose worst vox >= 10 ms):

| run | under-sheet feel rut | blocks | vox worst >= 10 ms | vox max ms |
|---|---|---|---|---|
| 12:24 | off (RUT_SHEET_ONLY_TEST true) | 25 | 3 | 75 |
| 12:54 | on (H1) | 25 | 21 | 35 |
| 13:14 | on (H1 + probe) | 40 | 29 | 109 |

PROVEN (this run): under-sheet floor cuts make 10-40 ms client hitches on op apply. Snap <-> hitch link = strong
inference (timing), not proven. Which part of the op apply (carve, MUD_MudSheet.ReGround traces, FloorDirty) = not
measured yet.

## Server cost split (43 x 5 s blocks, MUD avg 2.74 ms/frame at 30 fps)

| part | avg ms/frame | worst ms |
|---|---|---|
| rebuild (all = rb collider) | 0.73 | 20 |
| wheels | 0.66 | 33 |
|  w flush (rut ops made + carved) | 0.45 | 33 |
|  w other | 0.20 | 4 |
|  w track / w ahead | 0.00 | 2 / 1 |
| ahead | 0.16 | 11 |
| vox | 0.10 | 50 |
| rb smooth / index / mesh / nav | 0.00 | 1 |

Parts sum ~1.66 of 2.74: the rest is outside the parts (test kit JoltProbe runs before them) - inference.
Server rut physics = BuildCollider 0.73 + op make/carve 0.45 ms/frame.

## Run 2: op-apply split (DS 2026-10-18 13:26, 'MUD op slow' = ops >= 8 ms)

Logs: PeerPlugin1/logs/logs_2026-10-18_13-26-25, server/logs/logs_2026-10-18_13-26-23. Server made 260 rut ops
('MUD rut field' lines); 194 of them were slow on the server, 183 slow ops on the client. All slow ops kind 4 (rut).

| part (ms per slow op) | client sum | client med | client max | server sum | server med | server max |
|---|---|---|---|---|---|---|
| all | 2987 | 14 | 37 | 2936 | 14 | 34 |
| carve (CarveFloor) | 1501 | 8 | 16 | 2291 | 12 | 24 |
| reground (sheet) | 956 | 5 | 12 | 2 | 0 | 1 |
| dirty (FloorDirty) | 483 | 0 | 16 | 380 | 0 | 13 |
| post | 6 | 0 | 1 | 204 | 1 | 15 |
| flush / pre / smooth / sync | 2 / 38 / 0 / 1 | 0 | 1 | 15 / 37 / 4 / 3 | 0 | 13 |

Checksum client: 1501 + 956 + 483 + 6 + 2 + 38 + 0 + 1 = 2987. Server: 2291 + 2 + 380 + 204 + 15 + 37 + 4 + 3 = 2936.
Server floor box median 374 samples (~19 x 20 at 0.25 m). CarveFloor = segment loop (maps) + FloorBase per sample +
RelaxFloor + GrowRidges; its own split not measured yet.

Lazy rut mesh (client 10 s lines): patch meshes hidden under the sheet grow 0 -> 33, 'shown again' 0 all run -> in the
zone the sheet is the ONLY drawn ground; any spot it does not draw for a moment is see-through (user: gaps on the rut
sides / in the rut while driving, gone after it 'loads'). Cause of the sheet's missing spots = not proven.

## Run 3: crude carve A/B + lazy mesh A/B + ease drop (DS 2026-10-18 13:38)

Logs: PeerPlugin1/logs/logs_2026-10-18_13-38-33, server/logs/logs_2026-10-18_13-38-31. 230 rut ops ('MUD op time',
both sides 230). Crude = under-sheet op with no RelaxFloor / GrowRidges, even blocks of 40 seqs.

| side | mode | ops | op mean ms | op med | ops >= 10 ms | carve mean | relax mean | ridge mean | write mean | reground mean |
|---|---|---|---|---|---|---|---|---|---|---|
| client | crude | 107 | 9.76 | 8 | 39 | 2.42 | 0 | 0 | 2.08 | 5.34 |
| client | full | 123 | 15.65 | 16 | 107 | 8.65 | 5.09 | 1.48 | 1.80 | 5.57 |
| server | crude | 107 | 7.73 | 6 | 19 | 5.03 | 0 | 0 | 2.36 | 0 |
| server | full | 123 | 14.43 | 14 | 105 | 12.41 | 6.05 | 1.83 | 2.19 | 0 |

Checksum: 107 + 123 = 230 per side. Server carve has ~2.3 ms not inside seg + write + relax + ridge (crude 5.03 vs 2.75,
full 12.41 vs 10.35); the client does not (2.42 vs 2.36) - unexplained.

Snaps (client 'mine 1'): 8 in all, 8 in full-cut time (93.6 s), 0 in crude time (85.3 s). By chance ~0.6 %
((93.6/178.9)^8). One run.

Lazy A/B (60 s phases from 13:38:58.281, client 'MUD prof 5 s' blocks): ON 24 blocks fps 87.6, MUD 3.14 ms; OFF 16
blocks fps 79.2, MUD 4.51 ms. Snaps ON 3 / OFF 5. USER: see-through gaps ONLY in lazy ON (sheet only) phases -> the
sheet leaves short-lived holes; with patch meshes shown they are covered. Hole cause = not proven. Candidates read from
code (ONE_GRID false): (a) quads with an untraced corner (m_aGs 0) are not drawn - new window cells wait for Grounds
(NEW_TRACES per frame); (b) neighbour live slots rebuild at different uploads (UPLOAD_SLOTS 3 + wheel slots) -> after a
floor cut a rebuilt tile's edge sits lower than its old neighbour's = a crack along the tile edge until both rebuilt.

## Run 4: crude always + lazy ON + sheet tile skirts + gap probe + re-ground exact (DS 2026-10-18 13:52)

Logs: PeerPlugin1/logs/logs_2026-10-18_13-52-21, server/logs/logs_2026-10-18_13-52-20. Compiled clean (0 script
errors both sides). USER: no see-through gaps any more.

Gap probe ('MUD sheet gaps', client 10 s lines, 21 lines): untraced quads 0 in every line -> candidate (a) ruled out;
border gap between neighbour slots 0.14-0.29 m while driving, up to 1.79 m in 3 lines at 13:55:37-57, most live
builds over 2 cm -> the holes were tile-border cracks (candidate b), now covered by the skirts (ONE_SKIRT 0.45 m).
The 1.2-1.8 m border gaps are bigger than the skirt; user saw no gap - cause of those not checked.

Rut ops (kind 4, 'MUD op time'), run 3 = crude ops only, run 4 = all ops (all crude now):

| side | run | ops | op mean ms | op med | ops >= 10 ms | carve mean | dirty mean | reground mean | reground max |
|---|---|---|---|---|---|---|---|---|---|
| client | 3 (13:38) | 107 | 9.76 | 8 | 39 | 2.42 | 1.80 | 5.34 | 11 |
| client | 4 (13:52) | 116 | 6.48 | 4 | 25 | 1.76 | 2.52 | 1.99 | 4 |
| server | 3 (13:38) | 107 | 7.73 | 6 | 19 | 5.03 | 1.50 | 0 | 0 |
| server | 4 (13:52) | 118 | 7.21 | 5.5 | 24 | 4.10 | 1.88 | 0 | 0 |

Client 116 vs server 118 ops: the client log may start / end at a different op (not checked).

Snaps (real 'MUD corr: ... mine 1', not 'visual only'): run 3 = 8 lines, run 4 = 4 lines = 2 events (pairs 20 ms
apart, 13:53:19 and 13:54:26). Visual-only lines 590 -> 244.

Client fps (5 s 'MUD prof' blocks with a rut op in them; run 3 lazy ON phases only): run 3 20 blocks fps 85.1, MUD
script 3.65 ms; run 4 22 blocks fps 84.8, MUD 3.21 ms. fps unchanged; the frame is not script-bound here.

## Run 5: FLOOR_SPREAD + H2 sheet look ruts (DS 2026-10-18 14:11)

Logs: PeerPlugin1/logs/logs_2026-10-18_14-11-54, server/logs/logs_2026-10-18_14-11-53. 0 script errors both. USER: "looks
better", functional - sheet work parked here, loop back later.

| side | rut ops | op mean ms (run 4) | ops with dirty >= 5 ms (run 4) | 'MUD rut floor' print ms mean |
|---|---|---|---|---|
| client | 141 | 5.74 (6.48) | 0 (25) | 0.06 |
| server | 143 | 6.83 (7.21) | 0 (24) | 2.19 |

- FLOOR SPREAD: dirty spikes 25 -> 0 (client), 24 -> 0 (server); 'floor spread' lines 2-13 patches / 10 s, 0 waiting.
- PROVEN: the server's ~2 ms carve outside the parts = the OP_LOG 'MUD rut floor' Print (2.19 ms/op on the DS,
  0.06 on the client).
- H2: drawn drop when the real cut lands ('MUD H1 drop', client, 105 ops): median 0.042 m, max 0.102 (H1 run: median
  0.10). Sheet look re-reads ~1900-3000 cells in 70-100 calls per 10 s.
- Snaps: client 2 'mine 1' lines.
