# The plan now: ruts, craters, trenches, tunnels (2026-10-16)

One page that merges today's work into BUILDS, each tested in a few COMBINED sessions. Details and evidence stay in:
- plans/2026-10-16-ground-improvement-plan.md (items J1, R0-R5, V1, C1-C7, T1-T3, U1-U3, J2, S1 - problem / change /
  why each)
- research/2026-10-16-soft-mud-alternatives.md (mud look options A-K, the MudRunner look)
- research/2026-10-16-fact-check-ruts-craters-trenches-tunnels.md (verdict per claim)
- results: 2026-10-16-gap-checks.md, -improve-checks.md, -fix-checks.md (its tunnel table is superseded),
  -voxel-crater-slope.md, -mud-look-checks.md
Nothing in Scripts/ was changed. The user's local scripts are ahead of git: line numbers in the docs are from git
(f7715c3 code) - check them against the local version before building.

## Testing rules (user 2026-10-16, also in CLAUDE.md)
1. **Combined sessions, not single launches.** Every build is tested in at most three sessions, each a checklist of
   many items, one launch per environment.
2. **No performance numbers from Workbench.** Workbench = function and look only (it does not run like the game:
   WB-only fps swings were already seen 2026-10-15, and a WB host stalls when its window is not in front, PROVEN
   2026-10-11). Performance (fps, ms, memory, queue lengths, join time) = the uploaded mod on a dedicated server and /
   or the standalone game client.
3. Every new behaviour sits behind a switch, so one build can A/B inside one session.

The three session types used below:
| Session | Where | Checks | Never |
|---|---|---|---|
| **WB** | Workbench play, SP, one launch | does it work, does it look right, logs say the right thing | any ms / fps / memory number |
| **MP** | local dedicated server + peer clients (robot code restored from the archive, CLAUDE.md test setup) | same result on every machine (hashes), late join, robot snap COUNTS (>= 6 runs per setting) | fps / ms judgements |
| **PERF** | the mod uploaded; dedicated server (ideally not on the playing PC) + the standalone game client | fps, frame times, memory over time, rebuild queue, join time, many vehicles | Workbench numbers |

## Build 0 - test kit + baseline (no feature change)
Code (small, behind switches, archived after, CLAUDE.md rule): a 10 s log line (rebuild queue length, oldest dirty age,
builds/s, rut-field patch count, memory); a voxel FIELD hash per op (craters / trenches / tunnels are never compared
between machines today); a switch issuing N fake rut ops (JIP size test); a RUNTIME switch for the band mesher (so the
PERF session can compare without a second upload - e.g. a value read once from a profile file); the robot code back.

**Session 0-WB** (one launch, ~45 min checklist):
| # | Do | Pass / what it decides |
|---|---|---|
| 1 | Shoot into a voxel crater rim, a trench parapet, flat voxel ground | rounds stop -> no V1 |
| 2 | Walk out of an 81 mm crater, a Hydra crater, a 1.5 m trench (no jump) | decides crater-rewrite urgency, trench exits |
| 3 | Fresh vanilla truck: `MUD wheels: <id> registered` | missing -> R0 in build 1 |
| 4 | Band mesher on / off (runtime switch): ruts and road edges | looks the same -> R2 (off) in build 1 |
| 5 | Rain + water accumulation up: our patches / bricks wet, puddles in ruts? | mud option E free? |
| 6 | Wet weather drive: mud spray instead of dust? truck gets muddy? | mud option F free? |
| 7 | Archived radius probe: tyre sinks, body drops? | mud option H |
| 8 | Baseline pictures: RPG into a tunnel, crater next to a road and a house, a 1.0 m trench, a level-dug tunnel | before / after pictures for build 1 |

**Session 0-MP** (local DS + 2 peers):
| # | Do | Pass |
|---|---|---|
| 1 | Craters, a trench, a tunnel, then compare field hashes server vs clients | all equal |
| 2 | 21000 fake rut ops, then a late join | expect the stall ('waiting 10 s for op seq') -> J1 confirmed |
| 3 | Robot >= 6 runs | snap count baseline for build 1 |

**Session 0-PERF** (uploaded mod, DS + standalone client, 3-4 trucks off-road, 20 min):
fps, worst frames, memory growth, rebuild queue length / age (DS and client), band mesher on vs off (runtime switch).
Pass / decides: queue drains -> the rebuild gate can wait; band off cheaper -> keep off.

## Build 1 - MP / scale blockers + crater rewrite + small trench / tunnel fixes (all behind switches)
J1 (joiner never loses an op), R2 (band mesher off, if 0-WB #4 passed), R1 + far meshes (rut patches only near players,
data kept everywhere), R0 / V1 only if 0-WB #3 / #1 failed; crater rewrite (cosine bowl + road fade max(1, 1.7 x depth)
+ stack cap + sphere under a roof + server jitter in the op; crater SIZE per your decision); T1 trench pad 0.25 m;
U2 level digging; U1 tunnel FLOOR 6 m.

**Session 1-WB:** walk out of every crater size; crater next to a road and a house; repeated shelling of one spot; RPG
in a tunnel; trench widths 0.5-2.0 m (floor drawn as planned); dig a tunnel looking ahead (stays level) and one 4 m
deep (thick roof); ruts look with band off; far ruts appear when you drive back to them. Compare with 0-WB #8 pictures.
**Session 1-MP:** field hashes after all op kinds; late join after 21000 fake ops + craters (joins, hashes equal); robot
>= 6 runs vs the 0-MP baseline.
**Session 1-PERF:** same 20 min drive as 0-PERF + a shelling minute: fps, memory, queue, join time vs build 0.

## Build 2 - the MudRunner look on the rut field + trench / tunnel extras
Rut radius by depth; ridges + bow wave + gradual press in the rut op (felt); texture drag, wet-mud material on fresh
ruts, slump near the camera (client-only look); whatever 0-WB #5-7 proved free (puddles, mud spray / muddy trucks,
wheel sink); clods and decals if wanted; T2 spoil parapet, T3 trench exit, U3 tunnel roof guard + collapse rules.
Both rut shapes behind a switch (tyre-width vs wide soft + drawn groove) so one session compares them.

**Session 2-WB:** look checklist: ruts in grass / dirt / wet weather, both rut shapes, texture drag, slump, ridges from
the driver seat and from outside, parapet, trench exit, tunnel roof rules.
**Session 2-MP:** hashes (ridges are replicated), robot >= 6 runs for each rut shape, late join.
**Session 2-PERF:** 20 min drive in wet weather with 3-4 trucks, all look features on vs off.

## Build 3 - long matches
J2 rut snapshot JIP (prerequisites: floor stored in cm on every machine, FillTrench not reading the rut floor, snapshot
loaded after the voxel ops), S1 server rebuild gate if 1-PERF still shows the queue growing, C7 buildings + objects
over new holes.
**Session 3-PERF (on a hosted DS):** a 1-2 hour session with several players and vehicles, late joins every 20 min:
join time, memory, fps over time. **Session 3-MP:** joiner hashes vs server after the long session.

## Decisions needed before Build 1
1. Feature freeze: builds 0-1 fix shipped features; build 2 is new look work - go now or later?
2. Crater size: today's game craters (81 mm: 1.43 m deep, 6.3 m across) or a realistic preset (~0.47 m, ~2.1 m).
3. Tunnels: should T need a shovel like F?
4. Rut shape for build 2: tyre-width (sheet look, wheel-feel risk) vs wide soft + drawn groove - or both behind a switch
   (the plan above assumes both).
