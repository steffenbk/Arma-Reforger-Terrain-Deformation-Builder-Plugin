# Second opinion on the plan (2026-10-16)

A fresh reviewer (no shared context) read plans/2026-10-16-next-steps.md, the improvement plan, the research and
results docs and the code, re-ran four offline tools, and did its own web search. Then the original planner checked
each finding. This page = the review, the planner's answer, and what changed.
Labels: VERIFIED-CODE, MEASURED, SOURCE, OPINION. Line numbers = git (f7715c3 code).
Checks: `python Tools/plan_review_checks.py trace | refill` (the reviewer's scripts).

## Verdict
The plan is mostly sound. Every load-bearing code claim the reviewer checked holds. The four offline tools it re-ran
(improve_checks tunnel / stack / road, gap_checks rutr) reproduce their tables exactly (MEASURED).
Five real problems:
1. Build order: new look work (build 2) comes before fixes to shipped features (J2, C7).
2. U1 as written regresses tunnels, and its roof numbers are mislabelled.
3. "Every behaviour behind a switch" breaks MP unless the server decides the switch.
4. Build 0 has checks that are certain already or decide nothing, and it lost checks the deep dive had.
5. Several sessions have no pass / fail numbers.

## Confirmed by the reviewer (independent)
- J1: the client drops ops at 20000 held (MUD_VoxelProbe.c:973-974). Drain applies in seq order (:993-1006). AskJip runs once (:1027-1037). The server sends every batch in one loop (MUD_VoxelNet.c). VERIFIED-CODE.
- R1: FloorDirty -> EnsureRange -> EnsurePatch has no camera-distance check (MUD_WorldGround.c:3891-3897,
  :3148-3175, :3528-3548). VERIFIED-CODE.
- Rut patches are never freed (:3812-3840) and are not counted in the budget (:973-982). VERIFIED-CODE.
- The band mesher is on in the prefab (MudWorldGround.et:9). The grid path also draws the rut field. VERIFIED-CODE.
- Voxel bricks are Terrain|Foliage only, with no slab and no plate (MakeBody :5370-5403). VERIFIED-CODE.
- MUD_Wheels.c:24 needs VehicleWheeledSimulation. The API snippet says VehicleWheeledSimulation_SA_B inherits
  VehicleBaseSimulation, so it is not a subclass. SOURCE (snippet; host blocked):
  community.bistudio.com ... interfaceVehicleWheeledSimulation__SA__B.html
- The crater voxel branch passes only FullDepth (:5369-5381). Carve is an ellipsoid with crest 2.2 x depth. VERIFIED-CODE.
- T has no tool check on the server: it checks only on foot, 3 m range and rate (MUD_VoxelNet.c). VERIFIED-CODE.
- J2 prerequisite (b): FillTrench reads ColumnDepth -> FloorMin (:3396, :3420-3446). VERIFIED-CODE.
- Op replay plus quantised state is how Teardown MP does it: deterministic fixed-point destruction commands. SOURCE
  (snippet): blog.voxagon.se/2026/03/13/teardown-multiplayer.html

## Findings and the planner's answer
| # | Reviewer finding | Planner check | Result |
|---|---|---|---|
| D1 | The U1 roof numbers are labelled by the "dig to" target. Feet actually overshoot: target 3 -> 3.6-4.2 m, target 4 -> 4.3-4.9 m. Roof ~ feet depth - 2.47 m. | Re-ran: same numbers (MEASURED). **Also new:** even with U2 (TUNNEL_MID 1.25, look level) the feet still sink 3.2 -> 3.53 m over ~9 m (~3.5 cm per m). The reviewer said "stays level"; it is close, not level. | **Agree + add.** Relabel by feet depth. U2 needs a small extra lift or snap-to-start-depth; check in 1-WB over a 20 m tunnel. |
| D2 | U1 regression: Carve forces solid below undug - FLOOR (`if (floorY - y > d) d = floorY - y`). So a crater (FLOOR 3) inside a FLOOR-6 tunnel REFILLS it. | Code at MUD_VoxelProbe.c:2714 and :3272 confirmed (VERIFIED-CODE). Re-ran: 218 air samples below undug-3 before an RPG-size blast, 137 after; 81 refilled (MEASURED). | **Agree. A real bug in the plan.** U1 must change the clamp to "no NEW air below the floor": `d = max(d, min(old, floorY - y))`. Same rule for C4's sphere and CarveTunnel. |
| D3 | T1 "drawn = plan within +0.10 m" is the mean only. Per grid offset a 1.0 m plan with the pad draws 0.92-1.22 m. A 0.25 m pad is the same as planning 0.5 m wider. | improve-checks table: pad 0.25 / plan 1.0 equals pad 0 / plan 1.5 exactly (MEASURED, existing). | **Agree.** T1 = widen the WidthOf table (or the pad, same thing). State a ±0.15 m spread. FillTrench must match. |
| D4 | R0 is not "a helper + two casts". About 12 world-ground functions take a typed VehicleWheeledSimulation. | 13 references in MUD_WorldGround.c, 2 in MudSheet, 1 in GroundEntity (VERIFIED-CODE). | **Agree.** R0 is a medium refactor if SA_B is a sibling class. |
| D5 | 0-WB #3 passes by construction: every robot run already registers trucks. The risk is a game update; the 1.9 notes warn about wheeled-sim changes. SOURCE: reforger.armaplatform.com/news/modding-update-sept-24-2026 | Agree. | Make it a smoke check after every game update, on the DS's game version. |
| D6 | Build 3 (J2, C7) fixes shipped features. Build 2 is new look work under the freeze. Run 3 before 2. Also J1 alone still leaves ~165 s of replay for 21000 ops (839 ops / 6.6 s measured; ESTIMATE). | Agree. Build 2 also puts ridge data into the rut op, which the J2 snapshot then has to carry. So fix the J2 format first. | **Proposed** order 0 -> 1 -> 3 -> 2. User decision 1. |
| D7 | Switches are `static const` (VoxelProbe.c:204-338). A runtime switch that changes op results (crater shape, T1, U1/U2) or colliders (band mesher), set per machine, forks the field or gives false snaps. | Agree. A per-machine profile file for the band mesher can differ between DS and client. | Shape switches go in the op (a version field the server writes). Collider / look switches are replicated from the server. Testing rule 3 is rewritten. |
| D8 | 0-MP #2 (21000 fake ops) tests a bug the code already proves. It would also flood the connected peers into the same 20000 cap. Join time is performance. | Agree. | Fix J1 directly. Test it with a test-only low cap (200) and ~500 fake ops. The big-log join time moves to PERF. |
| D9 | 0-WB #2 walk-out decides nothing (the crater rewrite is unconditional). #5-#7 are build-2 scope; #7 restores archived code (creep). | **Part disagree.** #2 still decides T3 (trench exit) and gives the crater-size decision a fact. #5 / #6 need no code (weather only), so they are free information in the same launch. | Keep #2 (relabelled), keep #5 / #6 as optional notes, move #7 to build 2. |
| D10 | "~3 trucks saturate a DS" came from a 30 fps DS. Bohemia's hosting page recommends `-maxFPS` 60-120. SOURCE (snippet): community.bistudio.com/wiki/Arma_Reforger:Server_Hosting | Agree. | The PERF DS runs `-maxFPS 60`, on its own machine if possible. |
| D11 | 0-WB #1 says "trench parapet", but no parapet exists (CutTrench only lowers). V1's slab covers only the top, not tunnel walls or roofs. | Agree (T2 adds the parapet later). | #1 = grazing fire over a trench lip, shots through a tunnel roof from the surface, shots at tunnel walls from inside. Several calibres. |
| D12 | "Test kit archived after build 0" contradicts sessions 1-MP to 3-MP, which need it. | Agree. | Keep it in one file behind a TEST_KIT switch (off for release). Archive it at release. |
| D13 | Part of the 10 s line exists: Stats under m_bDebug (:7454-7473). m_bDebug also runs CheckNew on every new patch (:3468), and the prefab ships `m_bDebug 1`. | Prefab line 8 `m_bDebug 1` (VERIFIED-CODE). | Split the stats line from CheckNew. PERF uses release prefab values (m_bDebug 0). |

## Missing items the reviewer found (all accepted into the plan)
- **C7 checks dropped.** House by a crater, tunnel under a house, objects over holes. Back into 0-WB, as a baseline.
- **MAX_COLUMNS fallback (deep dive 3.9).** No test and no decision: ~195-240 mortar craters (ESTIMATE), then
  heightfield craters. Add a MAX_COLUMNS 40 test to 1-WB and decide the policy.
- **Linux DS.** Build 1 changes the Carve maths, so run the 1-MP hashes on a Linux DS too (improvement plan section 5).
- **Hash design.** A sum of rounded values is order-blind and can cancel. Use index + cm-quantised value with a mixing
  function, and log the count of mismatching samples, not just equal / not equal.
- **Who drives 3-4 trucks in PERF?** The robot drives one. Use AI drivers (waypoints) or friends; write it down before
  the session.
- **Pass / fail numbers.**
  - Robot snaps: build N ≤ 1.5x the baseline, ≥ 6 runs each.
  - Queue drains: queue at 20 min ≤ queue at 2 min, and oldest dirty age under 10 s.
- **Robot snaps on WB peer clients.** These are counts only (zero vs many), not performance. Repeat in PERF when it
  matters.
- **T with FLOOR 6 digs 6 m shafts.** Looking down digs in place. There is no tool check, and the rate is 250 ms. Each
  click moves ~4 m³, against the shovel's 0.25 m³/s. Pit traps and fall damage follow.
- **Blast cover in voxel craters** ('MUD cover' lines) is in no session. Add it to 1-WB.
- **R1 side effects.** Far patches rebuild one per frame (9-12 ms each, measured earlier) when you drive back: watch
  the hitches in 1-PERF. Remote vehicles beyond the near range sit on unrutted ground on that client (probably minor;
  OPINION).
- **The server op log grows all match.** Put the op count + memory in the 10 s line from build 0.
- **Checkpoint before each build** (CLAUDE.md rule).

## The four decisions: reviewer's and planner's view
| Decision | Reviewer | Planner |
|---|---|---|
| 1. Freeze scope | Builds 0-1 now, then J2 + C7 (+ S1 if needed), then build 2 later. | Agree. |
| 2. Crater size | A middle preset by attributes, e.g. perKg 0.8, power 0.45: 81 mm 0.80 m deep / ~3.3 m crest; Hydra 1.29 / ~5.3; TM-62M 1.79 / ~7.3 (from the game's FullDepth formula). The realistic preset is only ~4 voxel samples wide and gives little prone cover. Real craters depend a lot on soil (SOURCE: armamentresearch.com/an-introduction-to-crater-analysis; secondary: a 152 mm crater ~5 m across, 1.8 m deep, tankarchives.com/2018/12/bombs-vs-artillery.html). | Agree a middle preset is a good third option. 81 mm checked: 0.8 x 0.99^0.45 = 0.80 m (VERIFIED-CODE formula). Compare all three in 1-WB, since they are attribute values only. |
| 3. T needs a shovel | Yes, and slow it down (4 m³ per click). A workshop trench mod gates digging on the server by cooldown, slope, distance and a per-player limit (SOURCE: reforger.armaplatform.com/workshop/69FA76B7D8B6F785). | Agree. Especially with U1. |
| 4. Rut shape | Don't build both. Keep the wide felt rut (R3). Draw the tyre groove + ridges as a client-only look channel a few cm deep. Try a felt tyre-width rut only if that fails. | Agree. This was option "wide soft + drawn groove" in the soft-mud doc. It halves the robot runs and keeps the proven MP result (7 snaps / 7 runs). |

## Not checked
- community.bistudio.com and apps.dtic.mil were blocked (403); blog.voxagon.se did not resolve. Search snippets only.
- No source was found for Enfusion RPC size limits, or for how SnowRunner, BF6 or Hell Let Loose network deformation.
- Not known yet: whether SA_B has wheel-contact methods (needs Workbench).
- Not re-run: crater_slope_voxel.py, the J2 size calculation.
