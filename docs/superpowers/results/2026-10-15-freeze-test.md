# FREEZE test - mesh rebuild cost vs the rest (2026-10-15, Exp 192624, SP Workbench)

Setup: MUD_MudSheet.FREEZE_TEST, Upload (mesh + collider rebuilds) paused every other 10 s; all script work still
runs. Log: ArmaReforgerWorkbench/logs/logs_2026-10-14_23-29-21/console.log, 'MUD mud sheet PERF' lines.
Each 5 s window fell almost wholly in one state (other state < 0.2 s).

| window | state | fps | speed max m/s | our ms/frame | drawn quads |
|---|---|---|---|---|---|
| 1 | stand, no mud | 60.6 | 0 | 0.02 | 0 |
| 2 | frozen, no mud | 62.3 | 2.6 | 0.04 | 0 |
| 3 | frozen, entering mud | 52.4 | 4.9 | 0.39 | 6400 |
| 4 | live | 39.7 | 3.6 | 2.76 | 4000 |
| 5 | live | 41.7 | 3.4 | 2.59 | 4000 |
| 6 | frozen | 49.7 | 3.2 | 0.62 | 4000 |
| 7 | frozen | 40.5 | 4.7 | 1.03 | 6400 |
| 8 | live | 45.5 | 3.6 | 2.15 | 3200 |
| 9 | live | 37.2 | 3.9 | 3.11 | 3200 |
| 10 | frozen | 44.8 | 3.6 | 0.72 | 3200 |
| 11 | frozen | 42.0 | 3.7 | 0.77 | 3600 |
| 12 | live | 42.7 | 3.4 | 2.77 | 4400 |

In mud (windows 3-12): frozen 52.4, 49.7, 40.5, 44.8, 42.0 -> mean 45.9 (sum 229.4 / 5);
live 39.7, 41.7, 45.5, 37.2, 42.7 -> mean 41.4 (sum 206.8 / 5). Frozen gain ~4.5 fps (~2.4 ms/frame).
Frozen in mud is still ~15 fps below standing (~61): rebuilds are only a small part of the loss.

Earlier run same log (before FREEZE): slow windows with mud drawn (speed 1.5-2.4 m/s, 1200-4000 quads) gave
58.9 / 65.7 / 66.2 fps -> drawing the tiles alone looks cheap (inference: camera direction not recorded).
Open: the off-mud driving numbers (52-57) were at 10-12 m/s on another route - no control on the SAME loop yet.

## Control: mud sheet OFF (MUD_MudSheet.OFF_TEST), same loop, same log, ~01:56-01:57

| window | fps | worst ms | speed max m/s |
|---|---|---|---|
| 1 | 51.3 | 79.3 | 6.8 |
| 2 | 67.4 | 20.8 | 3.7 (10 s block with 0 ground presses) |
| 3 | 50.3 | 32.3 | 6.7 |
| 4 | 49.0 | 31.4 | 8.2 |
| 5 | 40.1 | 32.4 | 8.5 |
| 6 | 51.4 | 28.7 | 9.7 |

Driving windows 1,3-6: sum 242.1 / 5 = 48.4 fps, vs 45.9 frozen / 41.4 live with the sheet on.
=> the sheet's own cost beyond rebuilds is ~2.5 fps; most of the drop (61 -> 48) is there WITHOUT the sheet.

Ground ruts (heightfield, MUD world + MUD band 10 s lines) run in BOTH setups:
| 10 s block | presses | band mesh builds x mean ms | band colliders x mean ms | mesher ms total |
|---|---|---|---|---|
| sheet on (FREEZE) | 271 | 66 x 3.62 | 33 x 4.64 | 259 |
| sheet on | 832 | 249 x 3.16 | 123 x 4.15 | 763 |
| sheet on | 750 | 288 x 3.03 | 122 x 4.04 | 774 |
| sheet on | 656 | 251 x 3.59 | 85 x 4.53 | 809 |
| sheet on | 632 | 340 x 3.51 | 108 x 4.31 | 982 |
| sheet OFF | 0 | - | - | - |
| sheet OFF | 910 | 310 x 3.22 | 153 x 4.28 | 996 |
| sheet OFF | 982 | 326 x 3.27 | 151 x 4.32 | 1031 |
Mesh + collider rebuilds of the ground band ~ 1.0-1.7 s per 10 s (10-17 % of real time) while driving; the one
block with 0 presses ran 67 fps. User saw it too ("oh its the ruts"). Inside mud zones both the ground AND the
sheet take ruts = double work.

## After: v035 voxel ruts back (RUTS true, RUT_BEHIND_GAP 3), sheet ON - 2026-10-15 08:32-08:34
Log: ArmaReforgerWorkbench/logs/logs_2026-10-15_08-27-34/console.log. SP Workbench, Ural, same start spot.
Ruts all at x 1039-1097, z 2600-2628 (262 voxel rut ops). Mud zone is at 997, 2729 (64 x 64 m: z ~2697-2761),
so the truck NEVER entered the zone: sheet drawn quads 0, 0 wheel stamps all run. Heightfield presses 0.

| window end | fps | worst ms | speed max | rut ops | new voxel columns | bricks made | cells carved |
|---|---|---|---|---|---|---|---|
| 08:32:23 | 58.4 | 78.0 | 0.0 | 0 | 0 | 0 | 0 |
| 08:32:28 | 56.2 | 32.8 | 4.0 | 3 | 16 | 16 | 3 |
| 08:32:33 | 67.7 | 43.2 | 6.8 | 19 | 36 | 40 | 43 |
| 08:32:38 | 61.8 | 29.3 | 7.9 | 19 | 36 | 55 | 49 |
| 08:32:43 | 68.2 | 25.8 | 5.9 | 13 | 12 | 18 | 13 |
| 08:32:48 | 64.8 | 44.3 | 5.8 | 20 | 1 | 3 | 3 |
| 08:32:53 | 57.8 | 25.8 | 5.3 | 12 | 1 | 1 | 3 |
| 08:32:58 | 55.4 | 31.3 | 7.4 | 19 | 0 | 0 | 0 |
| 08:33:03 | 65.3 | 26.4 | 5.3 | 12 | 0 | 0 | 0 |
| 08:33:08 | 69.9 | 36.4 | 5.9 | 19 | 0 | 0 | 0 |
| 08:33:13 | 68.1 | 25.7 | 5.3 | 12 | 0 | 0 | 0 |
| 08:33:18 | 67.5 | 23.7 | 6.9 | 17 | 0 | 0 | 0 |
| 08:33:23 | 66.1 | 22.9 | 5.8 | 15 | 0 | 0 | 0 |
| 08:33:28 | 66.2 | 23.8 | 5.8 | 12 | 0 | 0 | 0 |
| 08:33:33 | 53.5 | 31.3 | 6.3 | 20 | 4 | 8 | 10 |
| 08:33:38 | 67.0 | 26.1 | 5.7 | 14 | 1 | 2 | 2 |
| 08:33:43 | 66.9 | 23.5 | 4.8 | 8 | 0 | 0 | 1 |
| 08:33:48 | 67.0 | 20.4 | 4.7 | 11 | 0 | 0 | 0 |
| 08:33:53 | 65.2 | 22.4 | 5.8 | 17 | 0 | 0 | 0 |
Driving windows (speed > 1, 18 windows): mean 64.1 fps. Checksum: rut ops column sums to 262 = 'rut seq' lines.
vs 48.4 (heightfield ruts, sheet off) and 41.4 (heightfield ruts, sheet live) above -> voxel ruts ~ +16 fps.
Grass -> dirt (new bricks / carved cells) shows NO steady fps drop in 5 s windows: the 3 busiest windows ran
67.7 / 61.8 / 68.2; the two lowest (55.4, 53.5) had 0 and 4 new columns. Worst voxel go-live frame 7 ms.
A short hitch would not show in 5 s averages - needs a per-frame log. Engine 'Terrain Warning: Maximum size of
index buffer exceeded' fired at 08:32:28.567 (1 s after the first grass cut) and 08:32:32.247 - cause unknown.
Mud zone cost with voxel ruts: NOT measured (never drove in it).
User 2026-10-15: driving outside the zone was on purpose (control run off the mud). Off-mud reference points:
no ruts at all 67 fps (stand 58-61, the 0-press 10 s block 67.4); voxel ruts 64.1 -> voxel ruts off the mud cost ~3 fps.

## Run 2 (same log, 08:41-08:42, off the mud, quads 0): dip when driving OVER old ruts (user)
User: cutting the rut is fine; the dip starts when the truck drives onto the middle of an existing rut.
| window end | fps | worst ms | rut cuts | brick rebuilds | worst build frame ms |
|---|---|---|---|---|---|
| 08:41:20 | 59.5 | 31.3 | 55 | 57 | 7 |
| 08:41:25 | 79.9 | 27.9 | 161 | 134 | 8 |
| 08:41:30 | 77.3 | 26.4 | 89 | 130 | 9 |
| 08:41:35 | 91.9 | 16.6 | 94 | 73 | 4 |
| 08:41:40 | 88.5 | 17.4 | 77 | 37 | 5 |
| 08:41:45 | 83.3 | 20.6 | 133 | 91 | 6 |
| 08:41:50 | 86.8 | 25.2 | 161 | 123 | 7 |
| 08:41:55 | 84.4 | 19.1 | 90 | 48 | 5 |
| 08:42:00 | 72.6 | 24.0 | 217 | 116 | 6 |
| 08:42:05 | 85.2 | 31.6 | 121 | 108 | 9 |
| 08:42:10 | 86.2 | 21.5 | 139 | 119 | 8 |
| 08:42:15 | 81.8 | 21.1 | 122 | 80 | 6 |
| 08:42:20 | 84.6 | 22.4 | 106 | 127 | 8 |
| 08:42:25 | 80.5 | 20.9 | 159 | 139 | 7 |
| 08:42:30 | 72.7 | 23.0 | 117 | 119 | 9 |
| 08:42:35 | 90.2 | 20.3 | 104 | 103 | 8 |
| 08:42:40 | 86.9 | 21.6 | 197 | 208 | 8 |
| 08:42:45 | 74.1 | 22.8 | 58 | 80 | 8 |
| 08:42:50 | 83.0 | 22.9 | 181 | 165 | 7 |
(rut cuts = 'rut behind' counts summed; 'cut at the voxel gap' was 0 in every line.)
No link between rebuild work and fps: 208 rebuilds -> 86.9 fps, the 3 dips (72.6/72.7/74.1) had 116/119/80.
Dip windows have normal worst frames (22-24 ms) -> a steady per-frame cost, not a hitch.
Inference (unproven): physics contact of the wheels with the voxel brick trimesh colliders (MUD_VoxelProbe.MakeBody,
layer Terrain | Foliage, one static body per 4 m brick). Not logged yet: time with a wheel on a brick.

## RUT FPS probe (TEMP sheet PERF split), 08:47-08:49, same log, off the mud zone
'on rut' = a wheel's WheelGetContactEntity is a MUD_VoxelSubEntity (brick collider); 'ours' = fine patch / coarse chunk.
Only frames with speed > 1 m/s. 'other' was 0 s in every window. Most wheels on a brick: 6.
| window end | all fps | worst ms | on brick fps (s) | our heightfield fps (s) |
|---|---|---|---|---|
| 08:47:17 | 54.6 | 27.0 | - (0) | 50.1 (1.50) |
| 08:47:22 | 68.2 | 26.6 | - (0) | 68.2 (5.00) |
| 08:47:27 | 76.3 | 24.8 | 73.2 (3.29) | 76.7 (0.76) |
| 08:47:32 | 70.8 | 19.6 | 70.4 (3.81) | - (0) |
| 08:47:37 | 65.3 | 24.5 | 65.3 (5.00) | - (0) |
| 08:47:42 | 85.1 | 21.1 | 83.9 (4.14) | - (0) |
| 08:47:47 | 64.9 | 23.9 | 65.9 (3.87) | - (0) |
| 08:47:52 | 64.5 | 50.1 | 64.5 (5.01) | - (0) |
| 08:47:57 | 86.0 | 20.4 | 85.1 (4.09) | - (0) |
| 08:48:02 | 76.3 | 21.5 | 80.4 (3.88) | - (0) |
| 08:48:07 | 68.2 | 27.0 | 68.2 (5.01) | - (0) |
| 08:48:12 | 81.8 | 25.0 | 80.1 (2.86) | 78.8 (1.22) |
| 08:48:17 | 83.2 | 23.2 | 82.9 (4.72) | 88.3 (0.29) |
| 08:48:22 | 83.2 | 22.1 | 83.8 (4.27) | 80.2 (0.74) |
| 08:48:27 | 80.5 | 25.0 | - (0) | 80.5 (5.00) |
| 08:48:32 | 74.8 | 25.5 | 83.1 (0.16) | 74.5 (4.85) |
| 08:48:37 | 65.7 | 27.8 | 65.7 (5.00) | - (0) |
| 08:48:42 | 84.6 | 20.6 | 84.6 (5.00) | - (0) |
| 08:48:47 | 78.9 | 21.6 | 78.9 (5.01) | - (0) |
| 08:48:52 | 79.4 | 21.5 | 79.4 (5.01) | - (0) |
| 08:48:57 | 80.0 | 20.2 | 80.0 (5.01) | - (0) |
| 08:49:02 | 66.0 | 23.5 | 65.1 (3.86) | - (0) |
| 08:49:07 | 66.2 | 25.5 | 66.2 (5.00) | - (0) |
Time-weighted: on brick 74.8 fps over 84.0 s, our heightfield 73.3 fps over 19.4 s.
=> DISPROVEN: wheel contact with the voxel brick colliders does not cost fps. Fps swings 64-86 inside runs that are
100 % on bricks (e.g. 65.3 -> 85.1 -> 64.9 -> 64.5 -> 86.0), i.e. something else changes - inference: view
direction while driving the loop (not logged). Note: 'on rut' = on a brick collider, which also covers uncut
ground near ruts, not only the rut middle.
