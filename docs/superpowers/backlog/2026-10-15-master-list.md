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
  v083). Notes: docs/features/mud-sheet/README.md. Feature index: docs/features/README.md.
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
| Ground debug off | NOT done: `Prefabs/Mud/MudWorldGround.et` has `m_bDebug 1` (CODE; script default is 0) |
| Trench DEBUG_INSTANT | false in MUD_TrenchSite.c:19 (CODE) - user wants it kept as a test option |
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

## Release checklist (before upload for players)
- MUD_VoxelProbe.TUNNEL_DEBUG_T false (T = debug tunnel key, no tool; user 2026-10-16: on for testing).

## Parked ideas (not now - add here, don't start)
- Skip a patch COLLIDER rebuild when the ground moved less than a tolerance since the LAST BUILT collider (mesh
  still rebuilds). Idea from Box2D 'contact recycling' (box2d.org/posts/2026/10/contact-recycling, user 2026-10-16):
  compare against the cached build, never update the cache on a skip (else drift). Must be the same rule on every
  machine. Low value today: rut passes move ~0.08 m (> any useful tolerance). Only if 0-PERF shows collider rebuilds
  as the cost.
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
- Voxel house / wall panels (code stays, see Lane 2 decision).
- Papers plan: 0.25 m cells (E, parked), smoothing (C, parked), dual contouring (B1, NO-GO for ruts).
- AI pathing over holes (navmesh rebuild is a dead end - CLAUDE.md).
- Vehicle blast craters, explosives with no crater, mud on vehicles, dust / spray, crater stamps, other maps,
  persistence, heal ruts (F3) - all from the old list.
- Archive MUD_GroundEntity (old test-map system) and MUD_TerrainOffProbe (old map layer still uses it).
