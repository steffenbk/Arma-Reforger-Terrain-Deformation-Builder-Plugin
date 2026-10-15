# Mud ground - master backlog (2026-10-10 17:5x, PvP only)

**SUPERSEDED 2026-10-15 by [2026-10-15-master-list.md](2026-10-15-master-list.md) for ranking; kept for its evidence notes.**

One list of everything open. Details stay in [ground-ideas](2026-10-10-ground-ideas.md) (feature items 1-16, A-D) and
[performance-overview](2026-10-10-performance-overview.md) (O/P/F/N items). This file wins on ranking.
Reviewed by a fresh read-only agent 17:4x-17:5x; its corrections are in, the big ones re-checked by hand (marked).

**Score** = 2 x Value - Cost, +1 if Opt. Value 1-5 = gain for a PvP server. Cost 1 = an hour .. 5 = days / unproven
engine support. Opt = also saves server, client or net work. Labels: CODE (read in our script), PAK (Stable and Exp
both, per reviewer), LOG (measured from a log), INFERENCE (not proven), REV (reviewer found, not re-checked by hand).

## Ranked list

| Score | Item | Group | V | C | Opt | Notes / evidence |
|---|---|---|---|---|---|---|
| 9 | Crater merge M1: rim never fills dug ground | Bug | 5 | 1 | | CODE l.2642-2648 `if (rim > h) SetH(rim)`. 2nd crater 2 m away puts +0.30 next to -1.98: 2.3 m step in 0.25 m, 85 deg raw / 73 deg collider (REV model c1_merge.py) - a trap wall, not a ridge |
| 9 | Trench DEBUG_INSTANT off | Release | 5 | 1 | | KEEP for now (user 2026-10-11: instant is handy for bug testing; later make it a test option, e.g. hold Ctrl+F to auto-finish). CODE MUD_TrenchSite.c:15 = true; any shovel holder digs a whole plan up to 60 m away instantly, every 2 s (MUD_TrenchPlanner.c:448-449, 550). Re-checked by hand |
| 9 | Crater size by charge + hit-entity check | PvP | 5 | 2 | yes | 13 ammo prefabs all dig the same 2 m pit (PAK, Stable = Exp). Also: OnExplosion ignores pHitEntity (l.2539-2545) -> a rocket on a truck / wall / tree up to 2.5 m up digs a full pit under it (REV) |
| 8 | Blasts through crater walls: in-game probe, then fix | Bug | 5 | 2 | | Material flag ruled out (IDA + PAK). Engine default "1" from IDA only |
| 8 | O1 lazy patches | Speed | 5 | 3 | yes | DONE in code 2026-10-11 (m_bLazy), not compiled/tested. Heights of an undug patch = skin (CODE InitPatch l.1204-1233; task9-sync 354/354 within 5 mm). Caveats (REV): build moves to the first press under the wheel / first message on clients; patch holds 4 gamemats vs skin 6; per-mesh normals may give a lighting seam (look) |
| 8 | Clients keep only heights for far patches, build when near | Speed | 5 | 3 | yes | DONE in code 2026-10-11 (m_fClientBuildRadius 300), not compiled/tested. NEW (REV). Every rut msg reaches every client; ApplyRect -> EnsurePatch with no distance cap (CODE l.3338, re-checked). Client log 11:39: ~45 ms/s build work per driving vehicle (LOG, REV) |
| 8 | F2 stress test (5-10 vehicles, shelling, DS + 2 peers, PvP mode) | Release | 5 | 2 | | Every number today is one vehicle |
| 7 | F4 send by distance (PlayerController Owner RPC) | Net | 5 | 4 | yes | DONE in code 2026-10-11 (m_fSendRadius 500), not compiled/tested. Real rate is ~2.5x the first guess: 13-15 msgs/s, 5.5-6.5 KB/s per client per driving vehicle (LOG, REV; stats line only prints when busy, CODE l.3634-3638 re-checked) |
| 7 | PvP scenario (Conflict or other mode) on the MudArland world | PvP | 5 | 3 | | Only the GM scenario exists. Cost INFERENCE |
| 6 | Crater lip / bowl shape <= ~45 deg | Bug | 4 | 2 | | After the collider blur still 52-56 deg (REV); bowl edge alone 58 deg (2D/R = 1.6). Needs a new shape or depth/radius ratio - do with size by charge |
| 6 | F1 debug off (m_bDebug default 1) | Release | 3 | 1 | yes | SeamCheck walks every patch every 10 s (l.3683), CheckNew traces each new patch on every machine (l.1422) (REV) |
| 6 | Probe PvP placement on holed ground | Bug | 4 | 2 | | NEW (REV). Conflict base building, mine placement, mortar deploy, radio / rally spawns on a hole cell. Only GM was checked (I2) |
| 5 | JIP ask cap per player (server side) | Net | 3 | 1 | | DONE in code 2026-10-11 (QueueJip, JIP_MAX_ASKS, reset after 600 s), not compiled yet. MUD_RpcAsk_Jip checks nothing; one ask = full resend (~4.4 MB at 2000 patches) and delays other joiners (CODE, REV) |
| 5 | Server settings + no-dig zones + per-player trench-site cap + fill policy | PvP | 4 | 3 | | MAX_SITES 20 is server-wide, no per-player cap, no expiry (CODE MUD_TrenchSite.c:12, 69 re-checked): one player can block plan mode for all. Free fill can erase enemy foxholes - design call |
| - | PINNED 2026-10-15: fps swings 64-86 while driving a loop over voxel ruts | Speed | ? | 2 | | Not rut cutting, not brick rebuilds, not wheel contact with brick colliders (LOG, docs/superpowers/results/2026-10-15-freeze-test.md: on brick 74.8 fps / 84 s vs heightfield 73.3 / 19 s). INFERENCE: camera view direction (rhythm per loop). User 2026-10-15: same build on their test server (game client, not WB): 100+ fps, NO dips, only a small expected fps loss on ruts (user: 'not bad at all') -> likely a Workbench-only effect; low priority, recheck only if dips show in the game client. Next probe: fps split by camera heading. TEMP 'RUT FPS' split still in MudSheet PERF line - remove when done |
| 5 | P1a ground changes behind the wheel (stutter) | Bug | 4 | 3 | | DONE in code 2026-10-11 (m_fWheelHold 0.15), not compiled/tested. |
| 5 | F3 ruts heal / hard patch limit | Speed | 4 | 4 | yes | Budget only warns; m_bDeformed never reset (CODE, REV) |
| 5 | Test explosions on asphalt / hard ground (item 15) | Test | 3 | 1 | | Cobblestone / stone / tiles not in hard words |
| 5 | F9 grass in craters (look) | Test | 3 | 1 | | |
| 5 | F13 publish checks (packed mud_heights.c opens, pak junk) | Release | 3 | 1 | | |
| 5 | Client ahead gate (Ahead only near the camera) | Speed | 3 | 2 | yes | Down from 7 (REV): ahead pre-pays the build a message would cause. Fold into "far patches" above |
| 5 | Measure client memory on a real game client | Test | 3 | 1 | | Skin +264..353 MB measured in Workbench only |
| 4 | Upkeep after each game update | Release | 3 | 2 | | holes write -> fit -> bake; re-check SetAllowance / IDA facts; recheck_kb |
| 4 | B depth per ground type (3) | PvP | 3 | 2 | | |
| 4 | O2 coarse plate + slab | Speed | 3 | 3 | yes | |
| 4 | Crater merge M2: hit in a crater digs deeper, capped | Bug | 3 | 2 | | |
| 3 | PINNED (user 2026-10-11): vehicle blast craters - look into later. Now no crater at a vehicle (Blast: onVehicle skip) and no cover from our ground for crews | PvP | 3 | 2 | | ground-ideas item 17 |
| 3 | Explosives with no crater: decide + second hook | PvP | 3 | 3 | | PG-7VL / PG-7VR, LAV-25 25 mm HEIT, M67 / RGD-5, mines, M112 / TSh-400g, supply-crate blasts (PAK, REV) |
| 3 | Mod clash check (one popular PvP mod) | Test | 2 | 1 | | We mod SCR_PlayerController and hook SCR_ExplosionAmmoEffect |
| 3 | Fine-patch lighting seam (look) | Test | 2 | 1 | | INFERENCE (REV): per-mesh normals |
| 3 | Slippery deep ruts (9) | PvP | 3 | 3 | | MAT_SLOTS 4 per patch |
| 3 | P1b driver predicts own ruts | Bug | 4 | 5 | | Only if P1a is not enough |
| 3 | Crater stamps (1) | Looks | 3 | 3 | | |
| 3 | F6 wet ground digs deeper | Looks | 2 | 1 | | |
| 2 | Crater merge M3 widen / M4 smooth seam | Bug | 2 | 2 | | |
| 2 | Objects float over new craters (N8, look first) | Bug | 2 | 2 | | |
| 2 | GM under-map placement + camera jump (I2/I3) | Bug | 2 | 2 | | |
| 2 | P4b 32 m skin chunks | Speed | 2 | 3 | yes | Only if covers still cost after O1 |
| 2 | Dust + dirt chunks (6), wheel-spin spray (11) | Looks | 2 | 2 | | Same effect code |
| 2 | Dug ground looks dug (5) | Looks | 3 | 4 | | Material blend test first |
| 2 | Split cheap tracks from real ruts (2) | Looks | 3 | 5 | yes | Unproven engine support |
| 2 | Preset trench layouts, GM repair brush (F11) | Feature | 2 | 2 | | |
| 2 | Other maps (Tools/mud_world_copy.py exists) | Feature | 2 | 2 | | |
| 1 | P2a crack skirt in submesh 0 | Bug | 2 | 3 | | A skirt submesh crashed WB once |
| 1 | F12 trimesh seam bumps (probe) | Bug | 1 | 1 | | |
| 1 | O3 mesh reuse, O5 direction cone | Speed | 1 | 2 | yes | |
| 1 | D crater as "type + pos" on the net | Net | 2 | 4 | yes | Only if F4 is not enough |
| 1 | Mud on vehicles | Looks | 2 | 3 | | |
| 0 | O4 4 m collider pieces, JIP skip flat runs (13) | Speed/Net | 1 | 3 | yes | |
| 0 | F7 mud drag | PvP | 2 | 4 | | MP correction risk |
| 0 | Archive MUD_GroundEntity (12 files) | Cleanup | 1 | 2 | | |
| -1 | Ragdoll limbs in walls (N9), tread prints (12), persistence (F5) | mixed | 1 | 3 | | |

## Already in the code (found by the review, re-checked by hand)
- **A "ruts dig TO a depth"**: Press caps at `depth * soft`, skips verts at/below the cap, steps 8 cm per new pass
  (CODE l.678-695). Nothing to build; B (cap per ground type) is the real next step.
- **C "side ridges"**: berm exists - m_fBermRadius 0.9, m_fBermHeight 0.08, raised beside the rut (CODE l.149-156,
  697-711). Only tuning left.
- So the open A/C question to the user is moot.

## Do-before (order)
1. One crater job: M1 (1 line) -> size by charge + hit-entity check -> lip / bowl shape -> M2. Same function.
2. Before any public test: DEBUG_INSTANT off, F1 debug off, JIP cap, trench-site cap, F13.
3. O1 lazy patches + client far-patch heights before F2 (else the stress test measures waste we already know).
4. PvP scenario on MudArland before F2 (stress test in the real mode).
5. F2 decides F3 (healing) and F4 (send by distance); D only if F4 is not enough.
6. P3 blast fix may need a collider geom slot -> O2 frees triangles (slots stay 2).

## Dropped
F8 AI (PvP only), BC2, 0.5 m verts, O4-style finer patches.
