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
| Test kit Stage 1 (2026-10-17, v115 = before) | MUD_TestKit.c (TEST_KIT / FAKE_RUT_OPS / JIP_TEST_CAP); Stats runs on m_bDebug OR TEST_KIT (SeamCheck + CheckNew stay m_bDebug); 'MUD patches' line ends ' \| queue N (waiting N), oldest dirty S s, ops N' (waiting = awake, not building, dirt this machine builds; age since queued or last built) | WB SP m_bDebug 1, 30 s drive (log 2026-10-16_23-19-11, 00:31): queue 0-4, oldest dirty <= 1.1 s, ops 0 -> 109 | DONE MP 2026-10-17 (m_bDebug 0: 'MUD patches' with queue part on server + client, 0 seams lines; results/2026-10-17-build-1-wb.md / -mp.md) |
| Test kit Stage 2 (2026-10-17, v116 = before) | 'MUD field hash seq N kind K: H (N samples)' per op that changes FEEL samples (FieldHashAdd in Put, reset + print in Submit after SmoothBox); 'MUD disp hash' on when TEST_KIT (DispOn) | WB SP (log 2026-10-16_23-19-11, 00:40-00:41): crater seq 1 kind 0 (2373 samples), instant trench seq 80-151 kind 2 (72 ops), drive = 78 disp hash lines kind 4; compare_op_hashes.py log vs itself: 151 ops, 0 mismatched, exit 0 | tunnel (kind 1) not tried; MP: WB host + PeerPlugin1 run 01:06 (WB log 2026-10-16_23-19-11 from line 14297 vs PeerPlugin1 logs_2026-10-17_01-06-06): trench + tunnel + drive, compare_op_hashes.py 'ops in both 97, mismatched 0, only server 0, only client 0', exit 0. Full 0-MP still to run |
| Test kit Stage 3 + settings file (2026-10-17, v117 / v118 = before) | FakeRutTick (server: N kind-4 ops via DoRutOp / Issue, 30 s after the probe is up, 20 / frame, 20 x 20 grid 3 m apart 10 m off the first player, else fake_rut_x / z); Receive held cap = jip_test_cap when > 0. Settings in $profile:MUD_TestKit.txt per machine (written with defaults when missing, read once, logged as 'MUD test kit: settings from ...') - server file: fake ruts; joining client's file: JIP cap | compiles; SP 00:56 (log 2026-10-16_23-19-11): settings file written with defaults + 'MUD test kit: settings from defaults' line | MP 2026-10-17: 500 fake ruts issued (seq 30-529), cap 200 read on the client; the rejoin after them NOT run (user: on a live MP server later) |
| Test kit Stage 4 robot (2026-10-17, v118 = before) | MUD_JoltProbe / StutterRobot / FrameLog copied back from the archive; EOnFrame `if (TEST_KIT) MUD_JoltProbe.Tick(...)`; ProbeGround() back (member, only JoltProbe calls it); robot's OnPrepareControls also needs TEST_KIT. MUD_EngineProbes Tick line NOT restored (S6) | compiles; SP 00:56: 'MUD corr' + 'corr trace' lines print; tunnel = 'MUD field hash seq 1 / 2 kind 1' (closes the Stage 2 tunnel gap) | DONE MP 2026-10-17: 8 robot runs on the client's truck, 1 snap; m_bDebug 0 done |

| Build 1 J1 (2026-10-17, v119 = before) | Client never drops for good: Receive past HeldCap() (HELD_CAP 20000 or test kit jip_test_cap) sets s_bLost; Drain asks again from s_iNextSeq once pending is empty, or clears pending + asks if it is full without the next seq; a gap > RESEND_WAIT_MS 15 s asks again (safety net); gap timer resets on every applied op. AskFrom: one ask per s, log 'asked the server for the op log from seq N (... ask K) - why'. Server: MUD_RpcAsk_VoxelJip(int fromSeq) (one per s per player, no more once-per-controller) -> ServerJipAsk -> JipSendTick sends JIP_MSGS_PER_FRAME 4 messages per frame per player, oldest first (was the whole log in one frame); a new ask replaces the player's running send; logs 'player P asked for the op log from seq S' + 'op log sent up to seq N' | Compiles; MP 2026-10-17: a first join used the ask path ('asked ... from seq 1', 'op log sent up to seq 9'), hashes 1156 ops 0 mismatched. Cap-200 rejoin NOT run (user: live MP server later) | 0-MP #2 now expects NO stall: joiner logs 'asked ... from seq 201' and reaches the server seq, hashes equal. Known: a client can keep a send running (bounded at 4 msgs / frame) |
| Build 1 T1 (2026-10-17, v120 = before) | MUD_VoxelProbe.TRENCH_PAD 0.25: CutTrench + FillTrench use max(0, OutsideDist - pad) (a column within 0.25 m of a piece = full floor depth, the wall starts 0.25 m further out); op box margins (TakesDig, Prepare) + pad. Voxel only: the heightfield fallback and WidthOf (UI 0.5 / 1.0 / 1.5 / 2.0) unchanged. Build const = same on every machine | Compiles; WB 2026-10-17 user pass (one instant trench only); MP kind 2 hashes 72 / 0 mismatched | widths 0.5-2.0 not compared; Session 1-WB: trench widths 0.5-2.0 m (expect drawn floor = plan +0.10 m mean, +-0.15 m by grid offset); fill a padded trench back flat |
| Build 1 U2 (2026-10-17, v121 = before) | TUNNEL_MID 0.95 -> 1.25 (capsule floor at the feet at pitch 0). No lift: offline Tools/u2_level_check.py shows 1.25 level in absolute height over 20 m; the '3.5 cm per m sink' was the sloped test ground (results/2026-10-17-u2-level-dig.md) | Compiles; 20 m level tunnel NOT run in WB 2026-10-17 | 1-WB: 20 m level tunnel, feet start vs end within 0.2 m; sensitive (1.30 climbs 5 cm per m offline) - the game's step-up may differ from the model |
| Build 1 crater rewrite C1 C2 C3 C4 C6 (2026-10-17, v122 = before) | MUD_VoxelProbe.Carve rewritten: C1 cosine bowl as a vertical-distance field (CraterCrest = max(0.75, D x 1.17 x pi / 1.8), CraterRimW; consts CRATER_SLOPE 0.9 / RIM 0.17 / RIM_W 0.4 = the WorldGround attribute defaults; = Tools/crater_slope_voxel.py CraterCos); C6 outline stretch by angle from the op (m_fB = stretch, m_iPiece = lobe x 1e6 + turn2 deg x 1e3 + turn3 deg; CraterShape); C3 CraterFade: smoothstep to 0 within CRATER_FADE 1 m of hard samples (HardSample: column flag or Kept/Hard from the skin); C4 RoofOver (hit >= 0.75 m under undug + solid 0.5-3 m above in the nearest column) -> CarveSphere r = depth. Server: WorldGround.VoxelCrater rolls depth jitter / shape (attributes) -> Blast(wp, depth, cap, gain, stretch, lobe, turn2, turn3) -> DoBlast applies the C2 stack cap (not under a roof), logs 'crater on dug ground'. Reach(depth, stretch) for the op box; log line shows stretch. Crater SIZE (decision 2) unchanged = today's attributes | Compiles; WB 2026-10-17: repeated shelling settles at the cap (leak found + fixed, row below), 'MUD cover' lines, user pass; MP kind 0 hashes 53 / 0 mismatched | presets side by side NOT compared; 1-WB: walk out of every size; crater by a road + a house; repeated shelling of one spot (settles ~2.4 m for 81 mm); RPG in a tunnel (no hole to the sky); look for varied outlines. 1-MP: field hashes kind 0. Known: rim still raises ground inside an older hole (as before, M1 not ported); MUD_MudSheet (parked) keeps the old crest formula |
| Build 1 U1 stage 1: deep tunnel ground ops (2026-10-17, v123 = before) | MUD_VoxelProbe: TUNNEL_FLOOR 5 + SEA_GAP 1 consts; m_fSeaY = ocean base + 1 (IsOcean, else off), set in Init, logged 'MUD voxel probe: tunnel floor 5 m, sea guard y Y (ocean B)'; DeepFloor(undug) = max(undug - TUNNEL_FLOOR, m_fSeaY) used by CarveTunnel + CarveSphere (C4: FLOOR 3 with the new clamp gave no chamber in a 5 m tunnel); clamp 'no NEW air below the floor' d = max(d, min(old, floorY - y)) in Carve, CarveSphere, CarveTunnel (ruts / CutTrench / FreeDig only lower: already safe; FillTrench only raises, keeps FLOOR 3); Carve rim not added to air under the column's top solid sample (NEW: the rim refilled 44 tunnel samples, MEASURED); Carve gj1 reach TUNNEL_FLOOR; ColSurf j0 TUNNEL_FLOOR; MUD_WorldGround.AtGround voxel tol TUNNEL_FLOOR + 0.25. Offline: Tools/u1_deep_tunnel_checks.py, results/2026-10-17-u1-deep-tunnels.md | Compiles; WB 2026-10-17 user pass (blasts over a tunnel); MP floor line equal on server + client | sea guard dig NOT run; 1-WB / 1-MP U1 items (blasts in / over a 5 m tunnel, sea guard log line on server and client equal). Open: shovel DIG / FILL inside a tunnel works on the SURFACE above (ColumnDepth + CutTrench measure from the top; true at 3 m today) - decide before stage 2 |
| Build 1 U1 stage 2a: tunnel aim + slope cap + down check + debug T (2026-10-17, v124 = before) | MUD_VoxelProbe: TUNNEL_DOWN_MAX 0.5 (pitch clamped to +-TUNNEL_DOWN_MAX / TUNNEL_SLOPE; was 0.7), TUNNEL_LAG 0.5, TUNNEL_DEBUG_T true; TunnelAim(player, look) = shared capsule middle (a look with flat part < 0.1 digs ahead along MUD_Dig.Facing, no in-place shaft), TunnelAimLocal reads the camera; TunnelDownOk(mid, feet) = mid not lower than feet + TUNNEL_MID - TUNNEL_DOWN_MAX - TUNNEL_LAG. TunnelInput returns at once when TUNNEL_DEBUG_T is false and now always sends via MUD_RequestVoxelTunnel (host / SP get the server checks too; before, the host dug directly). MUD_VoxelNet: MUD_RpcAsk_VoxelTunnel refuses (logged) when TUNNEL_DEBUG_T is false; shared MUD_TunnelAimOk (3 m + down check, refusals logged 'MUD tunnel: <what> refused ...'); 'MUD tunnel: debug T by player N' once per player. Offline: the straight-down case is now the drop 0.5 row of the slope table | Compiles; WB + MP 2026-10-17: 0 'refused' lines; 'debug T by player 1 / 2' | TUNNEL_DEBUG_T false check NOT run (user keeps T on; same-scripts setup can't show the server line) |
| Build 1 U1 stage 2b: shovel TUNNEL mode (2026-10-17, v125 = before) | MUD_Dig: MODE_TUNNEL 3 (PLAN 4, COUNT 5; R: OFF DIG FILL TUNNEL PLAN), hint 'Shovel: TUNNEL', no free-dig ghost, action name 'Dig tunnel' (also over a trench piece), each loop (prefab Duration -1.5 s) sends MUD_RequestShovelTunnel(TunnelAimLocal); SHOVEL_TUNNEL_MS 1000. MUD_VoxelNet: MUD_RpcAsk_ShovelTunnel = on foot, rate (world time), HoldsShovel(c, true), MUD_TunnelAimOk, then ServerTunnel (same op: MP / JIP / hashes unchanged). Adversarial review fixes: both tunnel RPCs check the rate first and stamp it on a refusal too (no log flood); MUD_TunnelAimOk also refuses a capsule less than TUNNEL_AHEAD - TUNNEL_LAG = 0.6 m ahead (XZ) of the feet ('not ahead ... (in place)': the client's dig-ahead alone could be skipped). UNVERIFIED: the server's placement flag for HoldsShovel(c, true) (MUD_RpcAsk_Dig uses false) | Compiles + works (user 2026-10-17); MP: 0 'no entrenching tool' lines (client shovel use not proven per op) | 1-MP HoldsShovel(c, true) on a live server |
| Build 1 U1 stage 2c: slope choice + 10 m (2026-10-17, v126 = before) | User 2026-10-17: "let the player choose how steep" + "10 m max instead of 5 m". MUD_VoxelProbe: TUNNEL_FLOOR 10; TunnelDrop(i) 0.35 / 0.5 / 0.7 / 0.85 / 1.0 (start 0.5), s_iTunnelSteep local (T and the shovel share it), TunnelAim(player, look, drop); TUNNEL_DOWN_MAX 1.0 = the server's down check (the choice is not sent); X cycles it (MUD_TunnelSteep in a new context MUD_TunnelShovelContext, active only while the shovel is in TUNNEL mode, so X is not taken at other times; chimeraInputCommon.conf), hint 'Tunnel slope'. Offline: results/2026-10-17-u1-deep-tunnels.md '10 m' (19 / 27 / 39 / 47 / 54 deg; 10 m lid 0) | Compiles; WB 2026-10-17: dug to 9.56 m (y 38.61 -> 29.05), floor line '10 m', user pass | T at spawn dug 24-31 m deep (0 carved, open); 1-WB: each slope, walk out of 0.7 / 0.85 / 1.0; dig to 10 m; does X clash with anything in TUNNEL mode |
| Build 1 crater cap leak fix (2026-10-17, v127 = before) | MUD_VoxelProbe.DoBlast: when dig < 0.05 at the cap, no op, log 'crater on dug ground N m deep: at the cap C, no dig' (the 0.15 m minimum made every shell dig on: WB 1.99 -> 2.48 m past cap 2.38) | Compiles; MP 2026-10-17: 15 shells logged 'at the cap', depth held 2.338-2.342 m | - |
| Build 1 tunnel on-ground check (2026-10-17, v130 = before) | MUD_VoxelNet.MUD_TunnelAimOk (T + shovel): refuse when !MUD_WorldGroundEntity.AtGround(feet), log 'MUD tunnel: <what> refused, player P at X not on the ground' (WB: T 2 s after start dug seq 1-2 24-31 m under the ground, 0 carved) | NOT compiled yet | WB: T right after Play -> refused line, no op; normal digging + at 10 m deep: no refused line |
| Build 2 Stage 1: rut floor in whole cm (2026-10-17, v131 = before) | MUD_VoxelProbe.CarveFloor: floorY = Math.Round(floorY * 100) / 100.0 after the deepY clamp (the only real floor write) - so the J2 snapshot can send absolute y in cm | Compile + WB ok (user 2026-10-17) | MP: part of the J2 late-join check |
| Build 2 Stage 2: fill depth reads ground only (2026-10-17, v133 = before) | MUD_VoxelProbe.ColumnDepth(col, li, lk, bool withFloor): false = density / seed only, no FloorMin; FillTrench passes false (a joiner replays fills before the rut snapshot), TrenchCols true | Compile + WB ok (user 2026-10-17 'all passed'; log 10:14-10:15: 72 trench cuts, 114 ruts, 7 fills, 2 blasts with the big preset, 0 script errors, 0 HOLE) | MP: J2 late join |
| Build 2 Stage 3: J2 snapshot JIP (2026-10-17, v135 = before) | MUD_VoxelProbe JIP_SNAPSHOT: first ask gets voxel ops + rut placeholders (rows a rut op made, option B) up to S, then the rut tiles at S (cm deltas), then live ops; held real rut ops <= S replaced by placeholders; snapshot kept for J1 re-asks; TEST_KIT SP self-test (3 'MUD snapshot self-test' lines). Details: plans/2026-10-17-build-2-plan.md Stage 3 | Compile + WB ok (user; self-test seq 218: 49920 samples 0 mismatched, 0 seq gaps; tiles 48 KB vs rut op log 117 KB; 4 of 217 rut ops made rows) | MP: late join (plan Final MP) |
| Build 2 Stage 4: C7a building lock (2026-10-17) | SKIPPED, not built: user WB look - houses by craters / tunnels / trenches look fine (prefabs sit deep in the ground). Voxel ops still have no building check (skin keep mask = roads, water, keep boxes) | user WB look 2026-10-17 | revisit only if the live test shows a gap under a house |
| Build 2 Stage 5: C7b things follow the ground (2026-10-17, v137 = before) | New MUD_GroundFollow.c from MUD_VoxelProbe.GoLive (ops 0-3, every machine): wake dynamic bodies; items / mines snap onto the new ground (armed pressure mine under a crater goes off, server); static props without RplComponent hidden when floating > 0.3 m, shown again when the ground returns; new MUD_VoxelProbe.TopAt (no rut floor). Details: plans/2026-10-17-build-2-plan.md Stage 5 | Compile + WB ok (user 2026-10-17 'it works'; log: 8 crater lines, 6 props hidden, up to 27 woken, 0-1 ms, 0 errors; point-bounds props appear in WB too; mine / item / show-back lines not in the log) | MP: plan Final MP (mine height on joiner, point-bounds props) |
| Build 3 Stage 0: SCM-lite offline proto (2026-10-17) | Tools/scm_lite_proto.py on CarveFloor's maths; results/2026-10-17-scm-lite-proto.md. Pick: R4 share 0.40 + relax 30 deg x4 + ridges B 0.3 gap 0.25 max 0.08 (heavy max wall 49.9 -> 40.3 deg); R3 not needed | offline only (MEASURED) | Stages 1-3 build it (plans/2026-10-17-build-3-plan.md) |
| Build 3 Stage 1: R4 shrinking rut steps (2026-10-17, v139 = before) | MUD_VoxelProbe.RUT_SHARE 0.4: CarveFloor cut = max(step x fall, 0.4 x (surf - capY)) - heavy cap after 4 passes (was 6), light unchanged | Compile + WB ok (user 2026-10-17; log 11:19-11:23: 422 rut disp hash lines, 0 HOLE, 0 errors; v140 works) | WB look: heavy truck pass 1 ~0.14 m, cap after ~4 passes; MP: disp hash kind 4, robot |
| Build 3 Stage 2: angle-of-repose relax (2026-10-17, v141 = before) | MUD_VoxelProbe.RelaxFloor after CarveFloor's cut: written samples + 1 ring, SORTED keys, 4 neighbours x 4 passes, 30 deg (RUT_REPOSE_TAN / RUT_REPOSE_ITERS), on rut depth under the ground without the rut (hillsides / crater walls never move); raises the low side (back to none at the ground), widens up to 2 samples; whole cm, FloorChanged + MarkBox + disp hash for every moved sample. Shared eligibility = new FloorBase (cut + relax). PrepareRut kind 4 box +2 FCELL. Log 'MUD rut floor: N samples, N relaxed, ...' | Compile + WB run 2026-10-17 (log logs_2026-10-17_09-03-29 from 11:34: 253 rut ops, 7927 cut + 3898 relaxed samples, 0 HOLE, 0 script errors, self-test seq 184 0 mismatched). User look: rut too wide for the tyres, ruts show late in chunks behind the truck. Fix (v142 before, v143 works, user 'passes'): RUT_R 0.3, relax 40 deg, RUT_BEHIND_GAP 1.5 m (log 11:43: 60 ops, 3 relaxed, 0 HOLE) | WB: heavy truck 5 passes - no sharp curb, drive out sideways, 0 HOLE; MP: disp hash kind 4, J2 late join, robot |
| Build 3 Stage 3: bulldozed ridges (2026-10-17, v144 = before) | MUD_VoxelProbe: MUD_RutTile.m_aRidge, RUT_RIDGES switch, GrowRidges after RelaxFloor in CarveFloor (add-blend, B 0.4, gap 0.2, W 0.5, max 0.08; height from the deepest cut per segment), plain rut-field ground only (no bricks / rows); RidgeGone clears on cut / relax / ClearFloor; FieldTopAt / FieldTopW; snapshot TileText 'floor;ridge' + SnapSelfTest compares ridges; DispFloorList ridges cyan. MUD_WorldGround: Heights raises by the ridge blend, 4 patch paths draw max(.., ridge), edge-locked | Compile + WB ok (log 12:20: 109 ops all ridged, 3201 ridge cells, 238 relaxed in 31 ops, self-test 0 mismatched with 1170 ridge samples, 0 HOLE, 0 errors); user look OK; v145 works | MP: floor + ridge hash on a late joiner, robot with ridges (belly on the crown), PERF patch rebuilds per op |
| Build 3 Stage 4: rut mud gamemat (2026-10-17, v146 = before) | Common/Materials/Vehicles/mud_rut.vhcsurf (soil copy, Tread 0.40 / NonTread 0.30) + Game/mud_rut.gamemat (soil child, own surface). MUD_WorldGround: m_RutMudMaterial attribute (empty = off), RUT_MUD + RUT_MAT_D 0.05; grid BuildCollider marks 2 m skin cells with a rut floor >= 0.05 m under base, SplitRutMud moves their tris out of the cached slot lists into one extra mud piece (no new index layouts); only while the patch uses < 4 surfaces ('MUD rutmat: patch N full'); debug 'MUD rutmat: patch N mud cells ...'. Band path too (prefab has m_bBandMesher 1!): tri centre in a mud skin cell -> extra mud piece (m_RutMudBand); helpers RutMudCells / RutMudFits / RutMudLog. Ops untouched (J2) | files: metas hand-written (vhcsurf {86F679913611B418}, gamemat {2BCBC124175231A2} = attribute default). WB run 14:58 (grid-only code): compiles, 0 script errors, no rutmat lines - band was on. Band run 15:04-15:05 (same log): compiles, 0 errors, 49 'MUD rutmat ... (band)' lines, 0 'full', mud cells grow 1 -> 14 of 16 per patch over repeat passes, self-test seq 180 0 mismatched. User 2026-10-17: ruts feel slippier, Diag shows mud_rut; v147 works | WB: Diag wheel contact material in a rut = mud_rut, slip felt, get in / out unchanged; MP: robot >= 6 runs (lower grip = more snaps, PROVEN 2026-10-11) |
| Build 3 Stage 6: trench spoil parapet (2026-10-17, v150 = before) | Both sides of planned trenches, none for the shovel square (user). MUD_VoxelProbe Parapet() in CutTrench + ParapetStart, Prepare / TakesDig boxes; MUD_TrenchLayout.SideDist. Rise 0.5 / top 1.0 / fall 0.5 m, height depth x 0.25 (max 0.4), placed past the plan's full-depth wall + 0.25 berm. Stages 5 + 7 DROPPED (user) | WB OK (v151 works); then RAISED_DIRT 0.1 (raised voxel faces draw dirt, crater rims too) NOT compiled | WB: mound both sides, not at the ends, grows per swing, bends, crouch cover; MP: field hash kind 2 equal + late joiner, PERF per swing |
| Tunnel darkness probe (2026-10-17, v153 = before) | Client / host: MUD_VoxelProbe.TunnelDark after CarveTunnel spawns Prefabs/Mud/MudTunnelProbe.et (own copy of vanilla ProbeEnvCave_base values + ProbeVolume child, Box Size 4 4 4, BoxLerp 1, grid 3 m -> after the first WB test (probes spawned, 9 in a 31-dig tunnel; middle dark but blotchy lit patches) Size 5 5 5, BoxLerp 0.5, grid 2 m; test 2 'much better, should be darker' -> LVDiffuseBrightness -5 -> -8, LVReflectionBrightness -8 -> -10, TopColorEV 4 -> 1 (EV: -3 = 8x darker); test 3 'better, darker with depth, pitch black at 10 m' (v154 = before) -> child prefabs MudTunnelProbe_1.._4 (diffuse -10/-12/-14/-17, reflection -12/-14/-16/-19, TopColorEV 0/-2/-4/-7) by feet depth 3/5/7/9 m (TunnelDepth, TUNNEL_DARK_D1 / BAND); NOT compiled. Risk: eye adaptation may brighten a dark view; test 4: tunnel from a trench blacked a box on the trench wall (box poked out) -> BoxRoofed: ground top (ColSurf, else WorldGround SurfaceY) at 9 points +-2.5 m must be >= mid + 1.5, box Size 5 3 5 (WB OK: trench wall fine); test 5: deep probes dense (log: level 4 every 1-2 m) yet sun shines down through the roof in spots like a lamp = direct sun, not probes -> voxel tops MAT_TOP = new Common/Materials/MUD_VoxelTop.emat (child of default_MatTerrainObj1 + CastShadow 1; vanilla default_MatTerrainObj_TerrShadows.emat only adds UseOnTerrainHeightmapShadows 1, so MatTerrainObject may not cast); NOT compiled. Fails -> hidden shadow layer under the top; test 6: still 'dome light' spots, also on the CEILING (sun can't light a down face) -> not plain sun; code: MAT_TOP only on up faces at the seeded ground (OnGround), so not grass faces either (unproven which). Added DisableInteriorProbes 0 to MUD_VoxelTop.emat; user test: spots darker? + night check (spots gone = sun); test 7: crisp polygon sun patch on the FLOOR = sun shadow hole. Over uncarved cells bricks draw nothing (EmitAt) - the surface over a tunnel is our HEIGHTFIELD (default_MatTerrainObj1, MatTerrainObject) and the terrain is hidden, so only the tunnel's own walls cast; MUD_VoxelTop CastShadow never reached that surface. Probe: CastShadow 1 in the SHARED default_MatTerrainObj1.emat (all our ground). Works -> PERF check on DS/game later (whole map casts); fails -> MatTerrainObject can't cast: need a dirt-material shadow hull; test 8: SAME -> both material probes reverted (archive voxel-top-castshadow-2026-10-17). MatTerrainObject casts no sun shadow even with CastShadow 1 (WB). Options to user: shadow hull (union of probe cells, dirt material, outward faces) after a 1-quad cast probe; user: probe then build. LID PROBE (TUNNEL_LID): per probe one up-facing MAT_DIRT 5x5 m square 0.15 m under the lowest of BoxRoofed's 9 ground tops (TunnelLid, log 'MUD tunnel lid'); NOT compiled. Spots under lids gone at high sun = script dirt meshes cast -> build the full shell (side skirts for low sun); ProbeVolume has no script methods so size is fixed in the prefab; ProbeEnvCave_base has no GUID reference in any pak text, so no inheritance) once per 3 m grid cell, only under a roof (RoofOver). TUNNEL_DARK flag. .meta hand-written | NOT compiled | WB: log 'MUD tunnel dark: probe 1', deep tunnel darker than the entry, no hard light edge between probes, entry stays lit; MP: client + late joiner get probes (op replay), DS spawns none |
| Build 3 Stage 9: vanilla wet-mud probes (2026-10-17, v148 = with probes) | MUD_WetProbe.c (new, TEST_KIT): WB host forces 'Rainy' looping after 5 s; 'MUD wet' 1 s lines = rain / wetness / water accumulation at the player; 'MUD wet wheels' = per wheel contact surface + GetWheelMud / Dust, body mud / dust (VehicleWheelEffectComponent). MUD_RadiusProbe.c back from the archive (ON true: driver seat, still 5 s -> wheel 0 radius -0.1 m for 3 s). API facts (Exp pak): weather manager comment 'wetness and water accumulation effects are WIP'; SetWheelMud / SetBodyMud exist (script can make trucks muddy) | WB 15:15 (MEASURED, log logs_2026-10-17_14-56-38): forced Rainy -> rain 0.96, wetness 1.00, water 0.72 -> 1.00; user: ruts LOOK WET, no puddles seen (not needed), no mud / dust spray seen. Truck gets muddy by itself: body + wheel mud 0.90 -> 0.99 driving our ground. Radius probe (SP host): wheel 0 radius 0.6 -> 0.5 drops the hub 0.071-0.083 m, body 0.021-0.024 m + tilt (up.x -0.009 -> -0.028), all back on restore = wheel sink WORKS locally; user: PARKED for the end stage (small feature) - probe removed again (archive copy kept). BUG FOUND: 278 'Usage of released BaseResourceObject' at MUD_WetProbe GetVehicleInfo, 45-47 of 75 wheel rows 'novhc' on our ground - INFERENCE: mud_rut.gamemat freed (nothing held it); FIX: MUD_WorldGround m_RutMudRes = Resource.Load(m_RutMudMaterial) held for the entity's life (NOT compiled) | DONE v149 (WB dry run 15:32, same log): 0 released errors, no novhc; wheel dust > 0.05 on 708/708 mud_rut, 198/218 grass, 11/11 dirt rows; user sees dust. Vanilla land gamemats have no wheel mud spray. MUD_WetProbe archived (mud_backups/archive/wet-probe-2026-10-17) |
| Build 3 Stage 8: look channel (2026-10-17) | DROPPED, not built (user: fresh-mud material, texture drag, slump and drawn groove not needed). D4 = felt rut only | - | - |

**Build 0 tests DEFERRED (user 2026-10-17: "we do this later, go next stage"):** m_bDebug 0 check, J1 late-join run,
MP robot run. UPDATE 2026-10-17: m_bDebug 0 + robot DONE in the Build 1 MP session; J1 rejoin moved to the user's live MP server. Gotcha for the J1 run: the settings file is read once per script compile (static s_bLoaded), so on a WB
host edit $profile:MUD_TestKit.txt, then reload scripts before Play. Both profile files were still all 0 at 01:06.

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
  refill, shovel at 5 m, light, dropped item) - now at 10 m (TUNNEL_FLOOR 10, user 2026-10-17). U1 stage 2 extras:
  X in TUNNEL mode cycles 5 slopes (walk out of 0.7, 0.85, 1.0 and note which work); a straight-down look digs ahead, not a shaft; shovel
  TUNNEL mode (R cycles 5 modes, 'Dig tunnel', refused with the shovel put away); T logs 'MUD tunnel: debug T by
  player N' once; no 'MUD tunnel: ... refused' lines in normal digging (T while walking / backing up too).
- Blast cover ('MUD cover' lines) in a voxel crater.
- MAX_COLUMNS set to 40: what happens after the cap? Decide the policy.
- Ruts look with band off; far ruts appear when you drive back.
- Compare with the 0-WB #7 pictures.

**Session 1-MP:**
- Field hashes after all op kinds, also on a Linux DS (the Carve maths changed).
- U1: 'MUD voxel probe: tunnel floor 5 m, sea guard y Y (ocean B)' equal on server and client; a client digs with the
  shovel TUNNEL mode (no 'server sees no entrenching tool' line) and with T; hashes after deep digging; a late joiner
  sees the same tunnel. Once: TUNNEL_DEBUG_T false on the server -> T refused and logged.
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
  RULED 2026-10-17 (user): felt rut only (Build 3 Stages 1-3); look channel / groove / drag / fresh-mud material DROPPED.
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
2. **Crater size: DECIDED (user 2026-10-17, later the same day): the OLD big preset stays** (1.5 / 0.6: bigger, good
   for the server stress test; revisit after it). Tried first: the middle preset (DepthPerKg 0.8, TntPower 0.45 = the
   MUD_WorldGround attribute defaults now, v129 = before; 81 mm 0.80 m / 3.25 m across / stack cap 1.33 m, Hydra 1.29 m,
   TM-62M 1.79 m, smallest 0.47 m). The options were:
   - today's craters: 81 mm 1.43 m deep, 6.3 m across;
   - a realistic preset: ~0.47 m deep, ~2.1 m across. It is only ~4 voxel samples wide, so little cover;
   - a middle preset: perKg 0.8, power 0.45 -> 81 mm 0.80 m deep, ~3.3 m crest. All three are attribute values, so
     they can be compared in 1-WB.
3. **Tunnels: DECIDED (user 2026-10-16).** 5 m default depth (user 2026-10-17: now 10 m, slope chosen by the player); tunnels need a shovel (new shovel mode); T stays as
   a debug key until release.
4. **Rut shape.** Recommended: the wide felt rut + a drawn client-only groove. Don't build both.
