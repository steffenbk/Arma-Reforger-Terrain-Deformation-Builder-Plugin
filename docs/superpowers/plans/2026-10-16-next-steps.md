# The plan now: ruts, craters, trenches, tunnels (2026-10-16, revised after the second opinion)

One page that merges today's work into BUILDS, each tested in a few COMBINED sessions. Details and evidence stay in:
- plans/2026-10-16-ground-improvement-plan.md (items J1, R0-R5, V1, C1-C7, T1-T3, U1-U3, J2, S1 - problem / change /
  why each)
- research/2026-10-16-plan-review-second-opinion.md (independent review of this page + answers; what changed and why)
- research/2026-10-16-soft-mud-alternatives.md (mud look options A-K, the MudRunner look)
- research/2026-10-16-fact-check-ruts-craters-trenches-tunnels.md (verdict per claim)
- results: 2026-10-16-gap-checks.md, -improve-checks.md, -fix-checks.md (its tunnel table is superseded),
  -voxel-crater-slope.md, -mud-look-checks.md
Nothing in Scripts/ was changed by this plan. CODE BASE: the user's branch displaced-ruts-partial-split (da37c47,
review fixes stages 1-6 + patch memory, results/2026-10-15-review-baseline.md) is merged into this branch
2026-10-16. Line numbers in the older 2026-10-16 docs are from f7715c3; on da37c47: FLOOR :307, Carve clamp :2724,
CarveTunnel :3247 (clamp :3283), ColSurf j0 :5048, Receive cap :982, AskJip :1036, TunnelInput :680
(MUD_VoxelProbe.c); AtGround :4498-4522, Stats :7699, PatchCounts :7653 (MUD_WorldGround.c). Save a checkpoint
before each build (CLAUDE.md).

**Already built on da37c47 (so the plan below shrinks):**
| Plan item | Built as | Tested | Still open |
|---|---|---|---|
| R1 + far meshes | FLOOR_FREE (frees floor-only patches out of range) + FieldDue (rebuilds them near camera / player / vehicle) + FloorDirty touches existing patches only (no JIP patch storm) | SP, user "worked fine"; freed patches came back on return | MP + late join; memory still rose +170 MB in 3.5 min (run too short) -> PERF |
| Fewer rut rebuilds (part of S1) | FLOOR_SETTLE: floor-only dirt waits until quiet 0.4 s / max 1.5 s | SP: rebuild ms per rut op ~1/3 of before | MP (robot) |
| Patch memory | MUD_PatchPress: press arrays only on first write (~39 KB per patch saved) | SP | - |
| Part of the 10 s line | 'MUD patches' (live / asleep / floor-only / deformed / heights-only / building / lazy / total, mem KB, fps, vehicles moving, settle + free counters) | yes | gated by m_bDebug (Stats) - see the test kit |
| Rut floor lock same on every machine | F4: m_aEdgeLock (no buildings) in patches, LockAt and bricks | compiled + SP; brick side not exercised | look beside buildings / roads |
| Hole check covers field ruts | F3: plain + seam points | 0 holes in 1577 plain checks | seam path never run (cross a crater with a rut); 25 'HOLE (plain) ... hit none' at two spots (1039.5 2533.5; 1051-1053 2536) UNRESOLVED |
| Release switches | OP_LOG, ALLOW_INSTANT_DIG (server-checked) | built | flip before release (master list Lane 2) |

## The other plans and prompts - how they fit (2026-10-16)
| Doc (made in the other session) | Status | Where it goes in this plan |
|---|---|---|
| prompts/2026-10-15-review-fixes.md (F1-F6) | Stages 1-6 built (da37c47); **Stage 7 MP gate not run** | Session 0-MP #3 + #4 IS that gate |
| prompts/2026-10-15-patch-memory.md | Stages 0-2 built (= review Stages 0, 3, 4); **Stage 3 MP gate not run** | Session 0-MP #4 + the 0-PERF memory curve |
| plans/2026-10-16-rut-cells-plan.md (user picked option A) | not started | Stage 0 (grid mesher A/B) = 0-WB #5 + the 0-PERF band on / off; Stage 1 (offline proto) can run any time; Stages 2-6 = Build 1 **if** 0-PERF still shows rut rebuild cost too high after FLOOR_SETTLE (3-4 trucks); the rut layer is also where Build 3's look goes (0.125 m tiles keep the flat tyre floor that 0.25 m loses - results/2026-10-16-mud-look-checks.md) |
| research/2026-10-16-soil-rut-literature.md (SCM-lite) | design only | Build 3 felt rut shape: load-based depth + "each pass cuts a share of what is left" (= R4) + bulldozed ridges + angle-of-repose relax (caps rut walls - may replace R3's wider RUT_R). Offline proto first (its section 5) |
| research/2026-10-15-ida-rut-questions.md (answers 5b-13) | facts | 0-WB #9 (grass standing in ruts); Build 3: a "rut / mud" game material on the rut layer = slip in ruts on every machine |
| research/2026-10-16-custom-shaders.md | facts: no custom shaders | nothing changes: the mud look options already assumed none. MaterialBlendUseTerrainDistance test unresolved - stays parked |
| research/2026-10-15-rut-options-round2.md, -rut-cells-deep-dive.md | background for rut cells | via the rut cells plan |

## How to run it (step by step)
1. Paste prompts/2026-10-16-build-0-test-kit.md (everything below its line) into a local Claude session in the mod
   folder. Its Stage 0 checks the folder first: git state, it fetches only the plan docs + tools from this branch
   (never Scripts), reads the plan and checks every code anchor against your local scripts. Then it stops for your OK.
2. Stages 1-4 build the kit. It stops after each stage so you can compile.
3. Run the three sessions below (0-WB, 0-MP, 0-PERF), one launch each, using their checklists. Keep the logs.
4. Bring the logs (and pictures for 0-WB #7) back here. Hash check: `python Tools/compare_op_hashes.py server.log
   client.log`. We read the results and decide what Build 1 needs (V1? R2? J1 confirmed).
5. Decide crater size (decision 2) before Build 1. A Build 1 prompt gets written from the 0-session results.

## Testing rules (user 2026-10-16, also in CLAUDE.md)
1. **Combined sessions, not single launches.** Every build is tested in at most three sessions, each a checklist of
   many items, one launch per environment.
2. **No performance numbers from Workbench.** Workbench = function and look only. Reasons: WB-only fps swings were
   seen 2026-10-15, and a WB host stalls when its window is not in front (PROVEN 2026-10-11).
   Performance means fps, ms, memory, queue lengths and join time. Measure it with the uploaded mod on a dedicated
   server and / or the standalone game client:
   - The DS runs `-maxFPS 60`, on its own machine if possible.
   - Use release prefab values (m_bDebug 0).
3. **A/B switches must be the same on every machine.**
   - A switch that changes an op's result (crater shape, trench width, tunnel floor / level) goes INTO the op as a
     version field. The server writes it.
   - A switch that changes colliders or the look (band mesher, rut look) is replicated from the server.
   - Never a per-machine file or const: it forks the field or gives false snaps.
   - Today's switches are `static const` (MUD_VoxelProbe.c:204-338), so changing one needs a rebuild.
4. **Pass / fail numbers are set before the session.**
   - Robot snaps: ≤ 1.5x the baseline, ≥ 6 runs each.
   - Queue: the queue at 20 min ≤ the queue at 2 min, and the oldest dirty age < 10 s.
   - Hashes: 0 mismatching samples.
   - Robot snaps in MP sessions run on WB peer clients, so they count as zero-vs-many only, not performance.

The three session types used below:
| Session | Where | Checks | Never |
|---|---|---|---|
| **WB** | Workbench play, SP, one launch | does it work, does it look right, logs say the right thing | any ms / fps / memory number |
| **MP** | local dedicated server + peer clients (CLAUDE.md test setup, robot from the test kit) | field hashes equal on every machine, late join, robot snap COUNTS (>= 6 runs per setting) | fps / ms / join-time judgements |
| **PERF** | the mod uploaded; dedicated server (`-maxFPS 60`, ideally not the playing PC) + the standalone game client; 3-4 trucks driven by AI waypoints or friends (decide before the session) | fps, frame times, memory over time, rebuild queue, op count, join time | Workbench numbers |

## Build 0 - test kit + baseline (no feature change)
**Test kit:** one file, behind a TEST_KIT switch that is off for release. It stays through builds 1-3 and is archived
at release (CLAUDE.md archive rule). It holds:
- **The 10 s line:** 'MUD patches' exists (table above). Add only: rebuild queue length, oldest dirty age, server
  op count (m_aOps). It runs inside Stats, which needs m_bDebug - and PERF runs with m_bDebug 0 - so give Stats its
  own switch (TEST_KIT), split from the per-patch CheckNew trace that m_bDebug also runs.
- **A voxel FIELD hash per op.** Index + cm-quantised value with a mixing function; log the count of mismatching
  samples. Craters, trenches and tunnels are never compared between machines today.
- **A fake-rut-op switch:** issue N fake ops.
- **A test-only low J1 cap** (e.g. 200 held ops), so the late-join bug shows with ~500 ops instead of 21000.
- **Band mesher A/B:** no switch. In WB edit m_bBandMesher in the prefab (one machine). In PERF, use two uploads
  only if needed.
- **The robot code back** from the archive.

**Session 0-WB** (one launch, ~45 min checklist):
| # | Do | Pass / what it decides |
|---|---|---|
| 1 | Fire with several calibres: grazing fire over a trench lip; down through a tunnel roof from the surface; at tunnel walls from inside; into a voxel crater rim; at flat voxel ground | rounds stop -> no V1; through -> V1 (note: V1's slab covers the top only, not tunnel walls / roofs) |
| 2 | Walk out of an 81 mm crater, a Hydra crater, a 1.5 m trench (no jump) | decides T3 (trench exit); a fact for the crater-size decision |
| 3 | House next to a crater; tunnel under a house; objects (crates, fences) over a new hole | baseline for C7 (today's behaviour, pictures) |
| 4 | Fresh vanilla truck: `MUD wheels: <id> registered` | smoke check; repeat after every game update (1.9 warned of wheeled-sim changes) |
| 5 | Band mesher on / off (prefab m_bBandMesher, relaunch): ruts and road edges | looks the same -> R2 (off) in build 1 |
| 6 | Optional, no code: rain + water up / wet drive - are our patches wet, puddles in ruts, mud spray, muddy truck? | notes for build 2 only (mud options E, F) |
| 9 | Look along a rut at low angle on grass (IDA answer 2-bis: grass is planted at heightmap height) | grass stands in ruts -> rut cells' ClutterOccluder prefab (Build 3) or AffectFoliage |
| 7 | Baseline pictures: RPG into a tunnel, crater next to a road, a 1.0 m trench, a level-dug tunnel 20 m long | before / after pictures for build 1 |
| 8 | Open items from da37c47: drive a rut across a crater (seam hole check); revisit 1039.5 2533.5 and 1051-1053 2536 with the hole check on; ruts beside a building and across patch -> brick by a road (F4 edge lock) | 0 HOLE lines; no step; explains or fixes the 'hit none' spots |

**Session 0-MP** (local DS + 2 peers):
| # | Do | Pass |
|---|---|---|
| 1 | Craters, a trench, a tunnel, then compare field hashes server vs clients | 0 mismatching samples |
| 2 | Low J1 cap (200) + ~500 fake rut ops, then a late join | expect the stall ('waiting 10 s for op seq') -> the J1 test case for build 1 |
| 3 | Robot >= 6 runs | snap count baseline (FLOOR_SETTLE + FLOOR_FREE on: their first MP check) |
| 4 | Late join after a rutted drive (FLOOR_FREE: patches made only near the joiner) | joins, 'MUD disp hash' equal, rut patches appear near the joiner only |

**Session 0-PERF** (uploaded mod, DS + standalone client, 3-4 trucks off-road, 20 min):
- Measure fps, worst frames, memory growth, rebuild queue length / age and op count, on the DS and the client.
- Band mesher on vs off: only if 0-WB #5 looked the same, as a second upload.
- Join time with a big op log (21000 fake ops issued BEFORE the joiner and the peers connect, so connected peers
  do not hit the cap).
- Pass / decides: if the queue drains (rule 4), the rebuild gate can wait. If band off is cheaper, keep it off.

## Build 1 - MP / scale blockers + crater rewrite + small trench / tunnel fixes (switches per rule 3)
- **J1:** a joiner never loses an op.
- **R2:** band mesher off (if 0-WB #5 passed).
- **R1 + far meshes: BUILT on da37c47** (FLOOR_FREE + FieldDue). Build 1 only fixes what 0-MP / 0-PERF find.
- **Rut cells (plans/2026-10-16-rut-cells-plan.md Stages 2-6), only if** 0-PERF shows rut rebuilds still too
  costly with 3-4 trucks after FLOOR_SETTLE. Behind RUT_CELLS (false = today). Its Stage 1 offline proto can be
  done before, without the game.
- **V1** only if 0-WB #1 failed.
- **R0** only if a game update breaks registration. It is a medium refactor: ~16 typed uses of
  VehicleWheeledSimulation.
- **Crater rewrite:**
  - cosine bowl;
  - road fade max(1, 1.7 x depth);
  - stack cap;
  - sphere under a roof;
  - server jitter in the op;
  - SIZE per decision 2.
- **T1:** wider trench floors. Change the WidthOf table (same as a 0.25 m pad); the drawn floor still varies ±0.15 m
  with grid position. FillTrench must match.
- **U2:** level digging. It still sinks ~3.5 cm per m (MEASURED), so add a small lift or hold the start depth.
- **U1 = the deep-tunnel package** (research/2026-10-16-deep-tunnels.md; user 2026-10-16: 3 m is not far enough):
  - TUNNEL_FLOOR 5 m (user 2026-10-16; 10 m later, only after 5 m passes), the same on every machine;
  - the clamp becomes "no NEW air below the floor" (`d = max(d, min(old, floorY - y))`). Otherwise a crater inside a
    deep tunnel refills it (MEASURED: 81 of 218 air samples);
  - a sea-level guard;
  - AtGround / ColSurf read the new floor;
  - a dig slope cap so players can walk out.
- **Shovel tunnels** (DECIDED user 2026-10-16): a new shovel mode TUNNEL (R cycles it, shovel animation, server
  checks the shovel in hand + 1 swing / s). T stays as a DEBUG key (TUNNEL_DEBUG_T true, no tool, as today) until
  release. Design: research/2026-10-16-deep-tunnels.md section 4b.

**Session 1-WB:**
- Walk out of every crater size, and compare the crater presets side by side (attributes only).
- Crater next to a road and a house; repeated shelling of one spot.
- RPG in a tunnel, including a 4 m deep tunnel: it must not refill.
- Trench widths 0.5-2.0 m.
- A 20 m level tunnel: feet depth start vs end within 0.2 m.
- A 4 m tunnel: the roof is about feet depth - 2.5 m.
- Deep tunnels: the checklist in research/2026-10-16-deep-tunnels.md section 5 (walk out of 5 m, sea level, no
  refill, shovel at 5 m, light, dropped item).
- Blast cover ('MUD cover' lines) in a voxel crater.
- MAX_COLUMNS set to 40: what happens after the cap? Decide the policy.
- Ruts look with band off; far ruts appear when you drive back.
- Compare with the 0-WB #7 pictures.

**Session 1-MP:**
- Field hashes after all op kinds, also on a Linux DS (the Carve maths changed).
- Late join with the low cap + 500 fake ops + craters: it joins and hashes are equal.
- Robot >= 6 runs vs the 0-MP baseline (rule 4).

**Session 1-PERF:** the same 20 min drive as 0-PERF + a shelling minute, compared with build 0:
- fps and memory;
- queue and op count;
- join time with the big log;
- hitches when driving back to far ruts (R1 rebuilds one patch per frame).

## Build 2 (proposed: was build 3) - long matches and shipped-feature fixes
Before new look work: these fix shipped features, and the J2 format must be fixed before build 3 adds ridge data to
the rut op.
- **J2 rut snapshot JIP.** Prerequisites:
  - the floor is stored in cm on every machine;
  - FillTrench does not read the rut floor (it does today, via ColumnDepth -> FloorMin);
  - the snapshot is loaded after the voxel ops.
  - Reason: J1 alone still replays ~165 s for 21000 ops (ESTIMATE).
- **S1:** a server rebuild gate, if 1-PERF still shows the queue growing.
- **C7:** buildings and objects over new holes.

**Session 2-WB:** repeat the 0-WB #3 checks (house / tunnel under house / objects) vs the baseline pictures.

**Session 2-PERF (on a hosted DS):** 1-2 hours with several players and vehicles, late joins every 20 min. Measure join
time, memory, op count and fps over time.

**Session 2-MP:** compare joiner hashes with the server after the long session.

## Build 3 (proposed: was build 2) - the MudRunner look + trench / tunnel extras
- **Felt rut shape = SCM-lite** (research/2026-10-16-soil-rut-literature.md section 4, offline proto first):
  load-based depth, each pass cuts a share of what is left, bulldozed ridges, angle-of-repose relax for the walls.
  Fallback if the relax is not enough: R3's wider rut (radius = max(0.5, cap / 0.58)).
- **Drawn on the rut layer** if rut cells were built (0.125 m tiles keep the tyre's flat floor); a "rut / mud" game
  material on that layer for slip; ClutterOccluder on its prefab if grass stands in ruts (0-WB #9).
- **Tyre groove + ridges + bow wave:** a client-only look channel a few cm deep, with a cap. This is the planner's and
  reviewer's pick for decision 4. A felt tyre-width rut is tried only if the look channel fails.
- **Look only:** texture drag, a wet-mud material on fresh ruts, slump near the camera.
- **Whatever 0-WB #6 showed is free:** puddles, mud spray, muddy trucks.
- **Wheel sink:** restore the radius probe only now.
- **Clods and decals,** if wanted.
- **T2** spoil parapet, **T3** trench exit (if 0-WB #2 says it is needed), **U3** tunnel roof guard + collapse rules.
  ROOF_MIN uses feet depth - 2.5 m.

**Session 3-WB:** look checklist:
- ruts in grass / dirt / wet weather;
- texture drag, slump;
- the groove and ridges from the driver seat and from outside;
- parapet, trench exit, tunnel roof rules.

**Session 3-MP:**
- hashes;
- robot >= 6 runs, which must stay ≤ 1.5x the build 2 count;
- late join.

**Session 3-PERF:** a 20 min drive in wet weather with 3-4 trucks, all look features on vs off.

## Decisions needed before Build 1 (with the review's recommendation)
1. **Feature freeze / order.** Recommended (planner + reviewer): builds 0-1 now, then J2 + C7 (new build 2), then the
   mud look later (new build 3).
2. **Crater size:**
   - today's craters: 81 mm 1.43 m deep, 6.3 m across;
   - a realistic preset: ~0.47 m deep, ~2.1 m across. It is only ~4 voxel samples wide, so little cover;
   - a middle preset: perKg 0.8, power 0.45 -> 81 mm 0.80 m deep, ~3.3 m crest. All three are attribute values, so
     they can be compared in 1-WB.
3. **Tunnels: DECIDED (user 2026-10-16).** 5 m default depth; tunnels need a shovel (new shovel mode); T stays as
   a debug key until release.
4. **Rut shape.** Recommended: the wide felt rut + a drawn client-only groove. Don't build both.
