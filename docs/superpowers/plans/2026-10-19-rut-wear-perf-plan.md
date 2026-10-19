# Rut wear perf plan - 4 ideas (2026-10-19)

Why: the wear build (RUT_FIRST_D 0, 0.03 / pass, FIELD_SOIL_D 0.08) cut script time client 3.2-3.6 -> 2.31 ms,
server 4.4 -> 3.15 ms (DS, results/2026-10-19-mesh-vs-voxel-ruts-ds.md run 5). The gain = FEWER patch rebuilds
(client mesh 235-289 -> 99-130 / 10 s, server collider ~100-113 -> 49-76), each still 4-7 ms. Push that further.
Checkpoints: v231 = wear user-ok, v232 = before these ideas. User 2026-10-19: do all 4.
One DS session for all of them at the end (TESTING RULES); A/B each by its switch.

## Idea 4 - small collider skip re-test (DONE, KEPT user-ok DS 20:32, v233: server 3.15 -> 2.79 ms, colliders 82 -> 56 / 10 s, client same; results run 6)
MUD_WorldGround.COL_SKIP_SMALL false -> true, COL_SMALL_M 0.02 -> 0.035 (one 3 cm pass), COL_SMALL_MAX_S 1.0 kept.
A collider rebuild with no vertex moved >= 3.5 cm waits until 1 s after the last real build. Log: 'PAPER TEST 3 small
collider skips N' in the MUD world 10 s line. Watch: tyre floats 3 cm then drops (feel), corrections. Was 0-4 skips /
10 s with 15 cm first cuts (DS 16:16 2026-10-18) - with 3 cm passes expect many more.

## Idea 1 - wear counter: first passes change nothing (KEPT user-ok DS 20:44, v234, results run 7; WEAR_PASSES 1, soil 0.06; review fix: trail rides wear > 0; ClearFloor keeps wear - deterministic, not cleared)
Rule: a floor sample with no floor yet counts passes; only from pass WEAR_PASSES on does CarveFloor cut it.
1. MUD_RutTile (MUD_VoxelProbe.c:146): add `float m_aWear[SIZE_F]` (0 = never driven). RutTileMake leaves it 0.
2. CarveFloor (MUD_VoxelProbe.c:4826, loop `foreach (int ck, float cf : best)`): after `surf` is known, if
   `undug - surf < RUT_FIRST_TOL` (no cut yet) and the tile wear `< WEAR_PASSES`: wear += rw (RUT_RAMP weight, so two
   half passes = one pass; rw = 1 when RUT_RAMP off), make the tile if missing, round wear to 0.01 (J2: same float as
   the snapshot), `continue` - no floor write, no FloorChanged, no MarkBox, no relax / ridge (wrote stays empty).
   New const WEAR_PASSES = 2 (user picks; 2 = cut on the 3rd pass, as today's look). 0 = off.
   An op that only adds wear must not queue bricks / dirty patches: check Submit / QueueBox use m_vLo/m_vHi from
   Prepare (whole op box) - if so, skip QueueBox when written == 0 and no rows were made (keep m_sRows rules, J2 note
   in remember.md: rut ops DO make brick rows; a skipped op must still make its rows at its seq).
3. J2 snapshot: TileText / RunText (MUD_VoxelProbe.c:2006-2160) send floor ";" ridge. Add a 3rd list "|wear" in
   cm-of-pass (Math.Round(wear * 100)), parse it in the matching reader (~2123, RunText's tokens), compare it in the
   self-test (~2433 `tile.m_aFloor[f] != bt.m_aFloor[f] || ...`). Old texts without it = wear 0.
   Also every place that clears a floor sample (5732, 5845: crater / fill moves the ground = ClearFloor) clears wear.
4. Hash: add wear to m_iDispHash where the floor is hashed (~4962) so DISP_HASH compare catches a mismatch.
5. Trail rule (TrailIn ~1195, `return dep >= thr`): rear wheels must ride a worn-but-uncut track too, else every
   axle counts a pass. Ride if `dep >= thr || WearAt(point) >= 1` (server only, op content = J2-safe).
6. Client look ruts (MUD_LookRuts.c ~477, same rule as CarveFloor): skip the look cut where the server wear
   (+ 1 for this pass) is still < WEAR_PASSES + 1, else the client shows dirt on grass the server has not cut.
7. Log: 'MUD rut wear: N samples worn, M cut' per 10 s (behind OP_LOG off -> count only, print in the existing
   'MUD rut field' 10 s line).
Test: same line 1, 2, 3, 5 passes - passes 1-2 = no shape change at all (grass flat), cut from pass 3, dirt from ~5
(FIELD_SOIL_D 0.08 still applies on top - maybe drop it to 0.05 once wear does the waiting). Late joiner after
2 passes, then pass 3 together: both see the cut at the same time (hash lines match).

## Idea 2 - client look rebuild only when it shows (KEPT user-ok 2026-10-19, v236; fix ran DS 21:07 run 9. RAN DS 20:52 look ok, client ms flat - results run 8; FIX NOT COMPILED: a skipped patch waits for m_fMeshHoldTo = window end, one skip per window; MUD_WorldGround MESH_SKIP_SMALL, MESH_SMALL_M 0.035, MESH_SMALL_MAX_S 0.5; MeshSmall / KeepMeshY, FinePatch m_aMeshY + m_fMeshBuiltAt; log 'PERF IDEA 2 small mesh skips' in 'MUD world: budget')
Client rb mesh = 0.85 ms (biggest client part). A patch mesh rebuild whose change is all below what you can see
(every vertex moved < LOOK_MIN_M, e.g. 0.02, and no reveal t crossed 0) can wait, like COL_SKIP_SMALL for meshes.
Where: the mesh queue (MUD_WorldGround.c ~4617 LOOK_MESH_EVERY gate) and BuildMeshBand (7063). Needs a per-patch
'max shown change since last mesh' (p.Show / SetShow already track shown heights ~6620) - skip while < LOOK_MIN_M and
the last mesh < LOOK_MAX_S (1 s) old. Never on the server (no mesh there). Switch LOOK_SKIP_SMALL.
With idea 1 most grass passes make no change at all, so measure idea 1 first - idea 2 may then matter less.

## Idea 3 - cheaper rebuild per build (KEPT user-ok 2026-10-19, v236: run 9 fill check 0 differ, input -35 %, frame ms flat; BAND_FILL_CHECK now false. Was: CODED 2026-10-19, v235 = before: BandFill = MUD_VoxelProbe.FieldPatchFill + MUD_LookRuts.PatchFill fill floor / look / ridge per patch in one call each; mesh loop reads the press holder once (HideAt / Show / SetShow inlined). TEMP BAND_FILL_CHECK every 10th fill vs the old readers - 'MUD band fill check' must say 0. Mesher core (MUD_BandMesher.Build) not touched yet)
Mud sheet lesson 1 (features/ruts/README.md top): hand-inline small helpers, read per-vertex flags once into a local
int array, skip untouched cells = -20 % per build (DS 16:16). Apply to BuildMeshBand (7063) and BuildColliderBand
(7249); split logs exist ('MUD band: mesh split ms - input / mesher / reveal + uv / mesh object'; collider: input /
mesher / trimeshes). Biggest: collider trimeshes 72-329 ms / 10 s, mesher 65-244. Bench recipe: N old + N new builds
of the same patch between two GetTickCount, same output count. Mind 64 locals per method.

## DS session checklist (one launch)
1. Compile ok. 2. Wear: line 1, 2, 3, 5 passes (idea 1). 3. Feel: no float-then-drop (idea 4). 4. Late join after
2 passes. 5. ~2 min loop for numbers -> Claude splits by switch (MUD prof parts, MUD band counts, skips).
