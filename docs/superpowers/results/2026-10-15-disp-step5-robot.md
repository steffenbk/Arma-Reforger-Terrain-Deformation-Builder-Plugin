# Displaced ruts Step 5 - robot test (2026-10-15)

WB host + PeerPlugin1 client, robot on the host's truck (reverse circle, 20 s), snaps counted on the client
('MUD corr: ... mine 1'). Which setting: 'MUD rut floor' lines exist only with DISPLACE_RUTS true.

| Session | Logs (WB / client) | DISPLACE_RUTS | rut floor lines | robot runs | snaps |
|---|---|---|---|---|---|
| 1 | logs_2026-10-15_21-30-16 / 21-31-34 | true | 159 | 2 (21:33:03, 21:33:28) | 6 |
| 2 | logs_2026-10-15_21-35-43 / 21-36-03 | false | 0 | 2 (21:36:45, 21:37:09) | 1 |

Host side: 'corrections 0' on all 4 robot STOP lines.

Every snap:

| Setting | Time | body jump m | visual jump m | speed |
|---|---|---|---|---|
| true | 21:33:06.440 | 0.072 | 0.064 | rev 4.82 m/s |
| true | 21:33:08.010 | 0.092 | 0.078 | rev 5.55 m/s |
| true | 21:33:08.155 | 0.098 | 0.096 | rev 5.55 m/s |
| true | 21:33:08.226 | 0.081 | 0.079 | rev 5.55 m/s |
| true | 21:33:08.323 | 0.116 | 0.114 | rev 5.55 m/s |
| true | 21:33:08.381 | 0.094 | 0.094 | rev 5.50 m/s |
| false | 21:37:21.458 | 0.072 | 0.069 | rev 5.54 m/s |

Checksum: 6 + 1 = 7 rows.

Reading: true's 6 snaps are one burst in run 1 (2 s, 3-5 s into the run); run 2 had 0. False had 1 in run 2.
Only 2 runs per setting (plan asks >= 6; CLAUDE.md: the rate varies a lot between 3-run blocks), so NOT a
pass / fail - true looks worse on this sample, unproven. JIP (part 2) and F2 stress (part 3) skipped by the user.
