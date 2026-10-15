# Rut field Stage 5 - robot test (2026-10-15)

WB (robot START / STOP lines) + PeerPlugin1, same setup and counting as 2026-10-15-disp-step5-robot.md: snaps =
'MUD corr: ... mine 1' lines in the PeerPlugin1 log, bucketed by the WB robot run times (one machine, one clock).
Which setting: 'MUD rut field: floor grid on the patch vertices 1' printed = RUT_FIELD true; session 2 has none and
'MUD rut floor: ... N carved cells' > 0 = RUT_FIELD false.

| Session | Logs (WB / PeerPlugin1) | RUT_FIELD | robot runs | snaps |
|---|---|---|---|---|
| 1 | logs_2026-10-15_21-35-43 (22:50-22:54) / 22-50-51 | true | 7 (runs 5 + 7 stopped early: 15.9 s, 9.3 s) | 7 |
| 2 | logs_2026-10-15_21-35-43 (22:55-23:00) / 22-56-04 | false | 8 | 40 |

Per run:

| Setting | Run | Time (WB) | snaps |
|---|---|---|---|
| true | 1 | 22:51:56.977-22:52:16.965 | 0 |
| true | 2 | 22:52:21.639-22:52:41.626 | 1 |
| true | 3 | 22:52:46.234-22:53:06.226 | 0 |
| true | 4 | 22:53:10.804-22:53:30.805 | 0 |
| true | 5 | 22:53:35.527-22:53:51.465 | 3 |
| true | 6 | 22:53:56.027-22:54:16.039 | 3 |
| true | 7 | 22:54:21.384-22:54:30.709 | 0 |
| false | 1 | 22:56:50.924-22:57:10.919 | 0 |
| false | 2 | 22:57:15.840-22:57:35.827 | 2 |
| false | 3 | 22:57:40.546-22:58:00.552 | 4 |
| false | 4 | 22:58:05.350-22:58:25.327 | 0 |
| false | 5 | 22:58:30.061-22:58:50.069 | 22 |
| false | 6 | 22:58:54.736-22:59:14.732 | 6 |
| false | 7 | 22:59:19.593-22:59:39.599 | 4 |
| false | 8 | 22:59:44.541-23:00:04.524 | 2 |

Checksum: true 0+1+0+0+3+3+0 = 7 = all 7 'mine 1' lines in 22-50-51; false 0+2+4+0+22+6+4+2 = 40 = all 40 in 22-56-04.
WB robot STOP lines: 'corrections 0' on all 15.

Reading: true 1.0 snaps per run vs false 5.0 (false has one 22-snap burst in run 5; without it 18 / 7 = 2.6).
Stage 5 robot gate 'corrections not worse with RUT_FIELD true': PASS on this sample.

Determinism (both machines, every rut op): 'MUD disp hash seq N kind 4' lines - session 1: 517 of 517 shared
seqs identical (WB has 1 more, seq 518, logged after PeerPlugin1 left); session 2: 645 of 645 identical.
Floor tiles at the end: 56 = 56; columns 0 = 0 (true). Heightfield HashLines (every 30 s): 0 deformed patches on
both machines in both sessions (compare_hashes.py: missing 0, extra 0, mismatched 0) - field ruts never write m_aH.

NOT done: the JIP part (both clients joined at the start, 'RplLoad 0 keys'). JIP op count not measured yet.
