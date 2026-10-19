# Mud ground - master list (2026-10-15, replaces 2026-10-10-master-list.md for ranking)

Decision (user 2026-10-15): **feature freeze**. Finish what exists - ground + voxel + mud sheet solid in MP - before
anything new. New ideas go to "Parked ideas" at the bottom, not into the work lanes. Test code archived to
`mud_backups/archive/test-code-2026-10-15/` (README there; checkpoint v066 = before).

Old list (`old/2026-10-10-master-list.md`, with old/ ground-ideas + performance-overview) stays for its evidence notes; many of its crater / heightfield items were
written before the voxel bricks (all ops are voxel since v031/v035) and need a re-check before work starts.
Labels: CODE (read in our script today), LOG (measured), USER (user saw it), INFERENCE (not proven).

## What exists today (shipped state)
- Whole-map heightfield ground (MUD_WorldGround + MUD_FinePatch), MP + JIP.
- Voxel bricks (MUD_VoxelProbe / MUD_VoxelNet): all ops voxel (SURFACE_OPS true), voxel ruts v035 (RUTS true,
  RUT_BEHIND 3 m) - CODE. User: "by far the smoothest" (v035), fine on their test server, 100+ fps (USER 2026-10-15).
- Mud sheet (MUD_MudSheet, client-only look) inside MUD_MudZoneEntity boxes - PARKED 2026-10-15 (ENABLED false,
  v083). UN-PARKED 2026-10-18 as the LOOK-ONLY synced sheet (user go, freeze exception): Stage 1 passed WB
  2026-10-18 (v177 = before, v178 = works); Stage 2 (sync layer MUD_MudSync from rut ops + own-stamp
  prediction) built + op split over frames, works in WB (v179 = before, v181 = works); Stage 3 BUILT, NOT COMPILED
  (v182 = before): camera radius 40 m, sparse tiles, no pre-build, LAZY RUT MESH (real rut patch mesh hidden under
  the sheet, rebuilt lazily; colliders immediate); Stage 5 late joiners BUILT, NOT COMPILED (v185 = before; server keeps the layer, J2 snapshot carries it); Stages 3 + 4 WORK WB (v184): op clearing -
  craters / digs clear the sync layer from the ops (same holes on every client, digs past 40 m kept); Stages 2-5 = op sync + prediction, memory, op clearing, late joiners. Notes: docs/features/mud-sheet/README.md. Feature index: docs/features/README.md.
- Trenches / shovel / tunnels (Ctrl+F, F, T), camera ease, AI navmesh refresh (off).

## Lane 1 - make it solid in MP (do in this order)
| # | Item | Why / evidence |
|---|---|---|
| 1 | Recompile + SP smoke test after the archive | 2026-10-15 cleanup removed probe calls (CODE). Must compile clean |
| 2 | Mud-zone run with voxel ruts (SP): sheet cost + does the sheet clear mud along voxel ruts? | Wipe risk NOT real: rut ops (kind 4) never call GroundChanged (CODE 2026-10-15). Check instead: does the sheet float over a deep voxel rut (slow re-trace)? |
| 3 | MP run: DS or WB host + 1 peer, drive through a mud zone, dig, crater | Sheet is client-only: check each client draws its own marks, no errors, no snaps. Last MP numbers are from v035 (no sheet) |
| 4 | JIP: join mid-match after ruts + craters + trench (+ rut field Stage 5 JIP: joiner disp hash / columns / op count, docs/features/ruts/README.md) | Step A numbers (docs/superpowers/results/2026-10-14-jip-size.md) are pre-sheet; papers plan Step D only if joins hitch |
| 5 | Sheet loose ends: collider-only rebuild for held tiles (CollPending double rebuild), UV stretch (DRAG_MAX 0.6 vs 0.15 cells, UV clamp at 12 m edges, steep sides) | remember.md LATER list |
| 6 | F2 stress test: 5-10 vehicles, shelling, DS + 2 peers | Every number today is one vehicle |

## Lane 2 - before any public test (release switches)
| Item | State today |
|---|---|
| Ground debug off | flip before release: `Prefabs/Mud/MudWorldGround.et` has `m_bDebug 1` (CODE; script default is 0). It also gates the 10 s 'MUD patches' / 'MUD world' lines (Stats) - since 2026-10-17 MUD_TestKit.TEST_KIT turns Stats on too (not SeamCheck / CheckNew) |
| Trench DEBUG_INSTANT | false in MUD_TrenchSite.c:19 (CODE) - user wants it kept as a test option |
| Client instant dig (review F6) | flip before release: `MUD_TrenchSiteComponent.ALLOW_INSTANT_DIG` true (testing) -> false; server then refuses Ctrl+F instant dig and logs it (2026-10-16) |
| Debug tunnel key T (built 2026-10-17, U1 stage 2a) | flip before release: `MUD_VoxelProbe.TUNNEL_DEBUG_T` true -> false (T digs with no tool; tunnels then need the shovel mode). User 2026-10-16: keep T on for testing |
| Build 0 test kit | flip before release: `MUD_TestKit.TEST_KIT` true -> false (Scripts/Game/Mud/MUD_TestKit.c; 10 s Stats lines without m_bDebug, op hashes, robot) (2026-10-17) |
| Per-op log lines (review F5) | flip before release: `MUD_VoxelProbe.OP_LOG` true -> false ('MUD voxel probe' op line, 'MUD rut field', 'MUD rut floor'; ~4/s per vehicle) (2026-10-16) |
| JIP ask cap | in code: JIP_MAX_ASKS 10 (MUD_WorldGround.c) - check it still applies to voxel JIP |
| Per-player trench-site cap | DROPPED 2026-10-15 (user: no per-player limit). Only MAX_SITES 20 server-wide |
| Voxel house | DONE 2026-10-15: MUD_VoxelHouse ENABLED true for release - user tested it earlier, works |
| Mud sheet debug switches | FREEZE_TEST / OFF_TEST / DEBUG_SHOW / DEBUG_TELEPORT all false (CODE); PERF log line still prints |
| F13 publish checks | packed mud_heights.c opens, no pak junk |
| PvP scenario on MudArland | only the GM scenario exists |

## Lane 3 - known bugs (open)
- Client GPU hang (DEVICE_HUNG) seen twice - no repro yet.
- GM-placed vehicle hangs in air.
- MUD_Surface.Y has the same holed-heightmap flaw (GM camera etc.) - [SUGGESTION] from remember.md.
- Old trench test 'unpack should refuse' FAILED (stale case '1,9' after MAX_LAYERS went to 10) - case now '1,11' (user OK 2026-10-15), recheck after reload.
- PINNED: fps swings 64-86 over voxel ruts in Workbench only (game client 100+ fps, USER) - recheck only if seen in game.
- Crater items from the old list (M1 rim, size by charge, hit-entity check, lip slope): re-check against the voxel
  crater code before working on them.

## Parked ideas (not now - add here, don't start)
- MUD SHEET / MUD ZONES / ALL-OFFROAD SHEET (parked by user 2026-10-18, ENABLED false, v206 = last state): open = driven / undriven height step + tile pop (survived LIFT_BY_DRAWN), sync tiles never freed. Lessons: docs/features/mud-sheet/README.md top.
- PREMADE RUT PIECES (user 2026-10-18): one rut mesh made once, placed (pooled entities) at each wheel stamp instead of
  rebuilding tiles. Limit (code read): a sunken piece is hidden under the lifted A2 patch / sheet, so it cannot replace
  the tile rebuild; fits as a look-only bridge for the newest 1-2 m behind each wheel until the tile catches up.
  Unverified: several entities sharing one MeshObject.
- Hybrid sheet feel LOOSE ENDS (parked by user 2026-10-18 14:15, 'functional, loop back later'; v200; results/2026-10-18-h1-snap-probe.md
  runs 1-5): ease look never rated; H2 guess may run deeper than the server (front + rear wheel cut twice) -> rise
  after 3 s; tyres may look to float over the guess (SHEET_GAP 0.1); real-cut drop still median 4 cm; H4 drag. The
  server OP_LOG Print = 2.2 ms per rut op (Lane 2 OP_LOG switch covers it). H3 = sheet on all offroad (own plan).
- Mud look layer (client-only 0.125 m look mesh near the camera; user go 2026-10-17, STOPPED + removed 2026-10-18:
  ~30 ms per build, fps tanked, holes near the ruts unexplained). Findings + retry conditions:
  docs/features/mud-look/README.md; code: mud_backups/archive/mud-look-layer-2026-10-18.
- Tunnel darkness (user 2026-10-17 'put a pin in it'): off via MUD_VoxelProbe TUNNEL_DARK / DARK_MAT false. Resume
  notes: docs/features/trenches-tunnels/README.md (ambient-on-walls cause proven, graded IBL materials untested).
- Crater size + look fine-tune (user 2026-10-17: "we will fine tune this at a later point"): old big preset stays for
  the server stress test; options tried: middle 0.8 / 0.45, mix 1.1 / 0.5 (MUD_WorldGround DepthPerKg / TntPower).
- Server-owner crater preset option (user 2026-10-17): a server setting (config / mission header / attribute) to pick
  the explosion deformation preset - big (today), mix, middle, realistic. Safe for MP only if the size is rolled on
  the server and carried in the op (voxel craters: yes, VoxelCrater -> op depth). CHECK FIRST: any crater path a
  client computes from its own attributes (heightfield crater when Blast returns false) would fork - it must read the
  server's choice (replicated) instead.
- Skip a patch COLLIDER rebuild when the ground moved less than a tolerance since the LAST BUILT collider (mesh
  still rebuilds). Idea from Box2D 'contact recycling' (box2d.org/posts/2026/10/contact-recycling, user 2026-10-16):
  compare against the cached build, never update the cache on a skip (else drift). Must be the same rule on every
  machine. Low value today: rut passes move ~0.08 m (> any useful tolerance). Only if 0-PERF shows collider rebuilds
  as the cost. Note: FLOOR_SETTLE (da37c47) already does the coalescing half of this (one build per quiet window).
- Keep the terrain + grass over DEEP tunnels (no hole, no surface bricks) - UNVERIFIED (character under an un-holed
  heightfield); research/2026-10-16-deep-tunnels.md section 6.
- **THE PLAN NOW (2026-10-16, revised after an independent review): docs/superpowers/plans/2026-10-16-next-steps.md**
  - build 0 test kit + baseline, build 1 MP / scale blockers + crater rewrite + trench / tunnel fixes, build 2 long
  matches (J2, C7), build 3 mud look. Needs user decisions (section end). Review + answers:
  research/2026-10-16-plan-review-second-opinion.md.
- FULL OPTIONS REVIEW 2026-10-16 (ruts / craters / trenches / tunnels / more, every untested item with an ID):
  docs/superpowers/research/2026-10-16-ground-options-full-review.md. Its section 6A lists 6 cheap RISK checks on
  shipped code (bullets through voxel walls, voxel ops ignore the building lock, nothing wakes objects over new
  holes, 1.9 vehicle sim class, crater lip slope; voxel house item withdrawn) - candidates for Lane 1/3.
  DEEP DIVE 2026-10-16: docs/superpowers/research/2026-10-16-architecture-gaps-deep-dive.md - design shape holds, but
  rut-field cost with many vehicles (no client distance gate, band mesher 3x, ~3 trucks saturate a DS), JIP op-log
  growth and FLOOR 3 m (tunnel roofs <= ~0.8 m) are design-level. Section 4 = one test session, section 5 = fix order.
  FACT CHECK 2026-10-16: docs/superpowers/research/2026-10-16-fact-check-ruts-craters-trenches-tunnels.md (+ results/
  2026-10-16-gap-checks.md, Tools/gap_checks.py). New: JIP joiner stalls for good past 20000 held ops.
  IMPROVEMENT PLAN 2026-10-16: docs/superpowers/plans/2026-10-16-ground-improvement-plan.md - Phase 0 game checks,
  then MP fixes (J1 JIP never drops ops, R1 rut-field camera gate, R2 band mesher off, V1 bullet slab), shape fixes
  (crater bowl / stack / road fade / blast under roof, trench pad + parapet, tunnel floor + level dig, rut radius),
  scale (rut snapshot JIP). Fixes measured offline: results/2026-10-16-improve-checks.md.
- Rut field cost (LOG 2026-10-15 23:16: ~20 ms per rut op, mesher run twice per patch = 53%): shared build tried
  and reverted (6% hit, mud-gotchas). Next idea: schedule a patch's collider right after its mesh, then share.
- DISPLACED RUTS (2026-10-15, offline MEASURED): ruts as a 2D rut-floor grid (0.25 m) that pulls the split 0.5 m voxel
  mesh down - as sharp as 0.25 m voxels without the 3D cost, one mesh = look = feel. Plus cheap probes MatTerrainObject /
  GlobalLayer on voxel tops. docs/superpowers/research/2026-10-15-voxel-options-deep-dive.md
- FIRST IN LINE: solid ruts in mud zones - voxel rut profile with raised side ridges + zone-tuned step/cap
  (ideas A+B, MP-safe, no new op). docs/superpowers/research/2026-10-15-raised-ruts-ideas.md
- MudRunner-style mud feel: sinking that gets deeper and more slippery with each pass, belly drag, topsoil break.
  Research: docs/superpowers/research/2026-10-15-sinking-deep-dive.md, 2026-10-14-mudrunner-ida.md,
  Tools/mudrunner_mcp (telemetry MCP, finished and proven).
- Mud texture in ruts via MatTerrainObject MaterialBlendUseTerrainDistance (copy of our skin emat + mud BCR/NMO
  maps; distance to the untouched heightmap = rut depth). No shipped emat uses it. Quick test allowed as a
  freeze exception (user 2026-10-16), test stopped unresolved (1 white rim, 1 no change), emat restored. Custom shaders are
  impossible (2026-10-16). docs/superpowers/research/2026-10-16-custom-shaders.md
- Voxel house / wall panels (code stays, see Lane 2 decision).
- Papers plan: 0.25 m cells (E, parked), smoothing (C, parked), dual contouring (B1, NO-GO for ruts).
- AI pathing over holes (navmesh rebuild is a dead end - CLAUDE.md).
- Vehicle blast craters, explosives with no crater, mud on vehicles, dust / spray, crater stamps, other maps,
  persistence, heal ruts (F3) - all from the old list.
- Archive MUD_GroundEntity (old test-map system) and MUD_TerrainOffProbe (old map layer still uses it).
- Craters on roads / asphalt / concrete (user 2026-10-17). Blocked: roads are kept terrain (no skin holes), script
  cannot hole terrain at runtime. Options: A fake (scorch decal + debris, no dig); B re-run mud_holes.py with roads
  holed - open: RoadEntity mesh floats over the hole, navmesh rebuilds empty on holes (AI drives roads), ruts would
  start on roads unless a 'crater only' kept flag. Concrete pads that are prefabs would float over a crater.
- Crater decal + blast smoke (user 2026-10-17, "deal with this later"). Decal probe (MUD_VoxelProbe CRATER_DECAL,
  now false) MEASURED WB 16:10: 8 craters, the ENTS|WORLD trace hit the hidden GenericTerrainEntity at the old ground
  y every time and CreateDecal on it returned null. Next try: trace ENTS only / our brick entities, then CreateDecal.
  Smoke spawns at the old surface above the crater: a 0.3 s crater delay was tried and looked bad (removed). Idea: a
  second short dust puff at the crater floor.
- Tunnels always lit however deep (user 2026-10-17). Vanilla has Prefabs/Structures/Core/ProbeEnvCave_base.et
  (GameEnvironmentProbeEntity, InteriorProbe 1, LVReflectionBrightness -8, LVDiffuseBrightness -5) + ProbeVolume
  (VolumeEntity) for interiors. Idea: client spawns a cave probe + volume per tunnel. Unproven: runtime spawn, volume
  size from script. IN WORK 2026-10-17: probe built (MudTunnelProbe.et + TunnelDark, next-steps table); volume size
  cannot be set from script (ProbeVolume has no methods) - fixed 4 m box in the prefab. Waiting for the WB look test.
