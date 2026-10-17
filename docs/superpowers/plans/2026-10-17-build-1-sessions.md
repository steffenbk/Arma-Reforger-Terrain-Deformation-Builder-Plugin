# Build 1 test sessions (2026-10-17)

Three combined sessions (testing rule 1): WB, MP, PERF. Each merges the Build 0 session it replaces (0-WB / 0-MP /
0-PERF, never run) with the Build 1 list and the deferred Build 0 items. Source lists: plans/2026-10-16-next-steps.md
(Sessions 0-* / 1-*, "Already built" Still open column), research/2026-10-16-deep-tunnels.md section 5 (at 10 m).
Handed out one at a time. After each, Claude reads the logs and writes results/2026-10-17-build-1-<session>.md.

Before any session: stage 2c compiles (step 1 of prompts/2026-10-17-build-1-sessions.md).

Not in these lists, on purpose:
- 0-WB #7 "before" pictures: Build 1 is already in, so there is no "before" any more. The pictures taken in WB #22
  are the baseline for Builds 2 / 3.
- Shovel DIG / FILL inside a tunnel: works on the surface above, stays as is (user 2026-10-17).

## Session WB (Workbench SP, one launch, function + look only, no fps / ms / memory)
Setup: prefab m_bDebug 1. Diag menu GameCode > Mud > Displaced ruts: hole check on. Keys: F shovel, R mode, X tunnel
slope, T debug tunnel, Ctrl+F trench.

| # | Do | Pass / what it decides |
|---|---|---|
| 1 | Spawn a fresh vanilla truck | log 'MUD wheels: <id> registered' |
| 2 | Drive a rut across a crater; drive at 1039.5 2533.5 and 1051-1053 2536; ruts beside a building; ruts from a patch onto a brick by a road | 0 'MUD disp HOLE' lines, no step at the edges |
| 3 | Drive away far, then back over old ruts | far ruts come back |
| 4 | Look along a rut on grass at a low angle | note: grass stands in ruts or not (decides the ClutterOccluder, Build 3) |
| 5 | Shell one spot (81 mm) many times; a crater next to a road; one next to a house | settles about 2.4 m deep; road edge fades; varied outlines; note the house |
| 6 | Crater presets side by side (today / middle perKg 0.8 power 0.45 / realistic), attributes only, re-Play between | walk out of each with no jump: note which (decision 2) |
| 7 | Walk out of an 81 mm crater, a Hydra crater, a 1.5 m trench (no jump) | note each (T3, decision 2) |
| 8 | Get hit by a blast while in a voxel crater | 'MUD cover' lines appear |
| 9 | Fire several calibres: grazing over a trench lip; down through a tunnel roof from above; at tunnel walls from inside; into a crater rim; at flat voxel ground | rounds stop = no V1; through = V1 |
| 10 | Trenches 0.5 / 1.0 / 1.5 / 2.0 m; fill one back | floor about plan +0.10 m wide; the filled one is flat again |
| 11 | House next to a crater; tunnel under a house; crates / fences over a new hole | pictures (C7 baseline) |
| 12 | Shovel: R cycles OFF DIG FILL TUNNEL PLAN | hint 'Shovel: TUNNEL', action 'Dig tunnel', animation plays, each swing digs |
| 13 | In TUNNEL mode, put the shovel away mid-dig | digging stops |
| 14 | T with nothing in hand | digs; log 'MUD tunnel: debug T by player N' once |
| 15 | X in TUNNEL mode; X in other modes and with no shovel | TUNNEL: hint 'Tunnel slope', 5 steps; elsewhere X does what it always did; no clash with any other action |
| 16 | Each of the 5 slopes: dig down a few m, walk back out with no jump | note which work; 0.7 / 0.85 / 1.0 are the ones to check |
| 17 | Look straight down and dig | digs ahead, not a shaft |
| 18 | Dig down to 10 m | log 'MUD voxel probe: tunnel floor 10 m, sea guard y Y (ocean B)' at start; digging stops at about 10 m |
| 19 | A 20 m level tunnel (shallow), then one at about 9.5 m deep | feet depth start vs end within 0.2 m (U2) |
| 20 | 4 m tunnel: look at the roof. RPG fired inside a 4 m and the 10 m tunnel. Mortar + RPG on the surface above the 10 m tunnel | roof about feet - 2.5 m; inside: a chamber, no refill, no hole to the sky; above: ceiling not lowered, no break-in |
| 21 | Dig toward the coast below sea level; drop an item at 10 m; look at the light at 3 / 5 / 10 m | the guard stops it (note the water); item stays; light note only |
| 22 | Pictures: RPG into a tunnel, crater next to a road, a 1.0 m trench, the 20 m level tunnel | baseline for Builds 2 / 3 |
| 23 | Band mesher off (prefab m_bBandMesher), re-Play, look at ruts and road edges | same look -> R2 (off) |
| 24 | Optional: rain / wet drive | notes for Build 2 only |
| 25 | LAST, needs a rebuild: Claude sets MAX_COLUMNS 40 (checkpoint first), you reload, make craters until past the cap | note what happens (decides the MAX_COLUMNS policy); Claude puts 6000 back |

Claude checks the log for the whole session: no 'MUD tunnel: ... refused' line during normal digging (T while
walking / backing up too), no script errors.

## Session MP (DS + PeerPlugin1, CLAUDE.md setup; hashes, late join, robot counts - no fps / ms)
Setup: prefab m_bDebug 0 (release value). Test kit settings ($profile:MUD_TestKit.txt, read once per compile):
server file fake_rut_ops 500; peer file jip_test_cap 200. Edit them before the launch.

| # | Do | Pass |
|---|---|---|
| 1 | Start DS + peer | server and client both log 'MUD voxel probe: tunnel floor 10 m, sea guard y Y (ocean B)', the same line; 'MUD test kit: settings from' shows the values above |
| 2 | Wait 20 s | m_bDebug 0: 'MUD patches' lines still print (with queue / oldest dirty / ops), no 'MUD world seams' line |
| 3 | From the client: craters, a trench (dug + instant), shovel TUNNEL mode down to about 10 m, a T tunnel, a rutted drive | no 'shovel swing refused, server sees no entrenching tool' line (HoldsShovel(c, true) UNVERIFIED until this); server logs 'debug T by player N' |
| 3b | From the client: shell one spot 15+ times (cap fix, v127 = before) | server logs 'crater on dug ground ... at the cap C, no dig'; depth stops near the cap; hashes still equal (#6) |
| 4 | Wait for the 500 fake ruts (30 s after start) | server ops count up by 500 |
| 5 | Client quits and rejoins (late join, cap 200) | joiner logs 'asked the server for the op log from seq ...', server 'player P asked ...' + 'op log sent up to seq N'; joiner reaches the server's seq, no repeating 'waiting 10 s'; joiner sees the same tunnel; rut patches only near the joiner |
| 6 | Claude: `python Tools/compare_op_hashes.py server.log client.log` | 0 mismatched, 0 only-server, 0 only-client |
| 7 | Fresh truck, robot (driver seat, stopped, hold Space 3 s) >= 6 runs | snap counts per run (zero-vs-many only, rule 4) |
| 8 | Optional: Linux DS, repeat 3 + 6 | 0 mismatched |
| 9 | LAST, needs a rebuild: TUNNEL_DEBUG_T false, press T on the client | T digs nothing, no 'debug T by' line. Known limit: with the same scripts on every machine the client never sends, so the server's 'debug T refused' line can't show here |

User 2026-10-17: the leftover MP items (#5 J1 rejoin) and PERF run once at the END, on a live MP server, after
all builds / stages are done.

## Session PERF (uploaded mod; DS -maxFPS 60, best on another PC; standalone client; never Workbench)
Setup: m_bDebug 0, TEST_KIT on (for the 10 s line). Decide before: 3-4 trucks by AI waypoints or by friends.
Pass numbers (rule 4, set now): queue at 20 min <= queue at 2 min; oldest dirty < 10 s.

| # | Do | Measure |
|---|---|---|
| 1 | 20 min off-road drive with 3-4 trucks + 1 minute of shelling | fps, worst frames, memory over time, queue + oldest dirty, op count (DS and client) |
| 2 | Drive back to far ruts | hitches (R1 rebuilds one patch per frame) |
| 3 | Server file fake_rut_ops 21000, peers join only after they are issued | join time |
| 4 | 100 m tunnel at 10 m vs 3 m deep | brick count, memory |
| 5 | Only if WB #23 looked the same: second upload with band mesher off, repeat 1 | cheaper -> keep it off (R2) |
