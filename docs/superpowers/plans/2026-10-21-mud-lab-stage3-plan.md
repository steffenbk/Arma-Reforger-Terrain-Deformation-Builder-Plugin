# Mud lab stage 3 - plan (2026-10-21, cloud session, code read only)

For: `prompts/2026-10-21-local-mud-lab-stage3.md` (the three jobs) - to be carried out by the LOCAL session
(prompt: `prompts/2026-10-21-local-mud-lab-stage3-run.md`). This cloud session could not read the game logs
(My Games/... is on the user's PC), could not compile and could not launch, so everything below is from the code and
the numbers already in the repo. Labels: PROVEN-CODE (file:line on branch claude/upbeat-lovelace-q0w022 at b68d7b0),
MEASURED (a repo doc says so), INFERENCE (not proven yet - the meters in Part 0 are there to prove or kill it).

A draft of every meter and switch below exists as an **uncompiled patch**:
`plans/2026-10-21-mud-lab-stage3-draft.patch` (git apply, against b68d7b0; ~700 lines in MUD_MudSheet.c,
MUD_WorldGround.c, MUD_LookRuts.c). Use it as a starting point or ignore it - it was never compiled or run. Its line
names match the names in this plan.

## 0. Measure first (Part 0) - what the local session must read and add

### 0.1 What the logs already have
| job | log line (client) | what it gives |
|---|---|---|
| A | `MUD sheet builds:` 10 s (MUD_MudSheet.LocalLog 3629) | builds by reason, `edge match: sides copied / moved + co-built / faded`, `drape cut at cap`, `edge audit` (a 10 s SNAPSHOT, corners skipped: EdgeAudit 3306, j from 1) |
| B | `MUD band:` 10 s (MUD_WorldGround BandStats) | band mesh builds, mean / worst ms, `mesh split ms - input, mesher, reveal + uv, mesh object` |
| B | `MUD look ruts: 10 s` (MUD_LookRuts.Tick 280) | look cuts, `look meshes N (ms)` |
| B / C | `MUD rebuild why 10 s`, `MUD rebuild 5` | why patch meshes are rebuilt, same / small skips |
| C | `MUD prof 5 s` / `MUD prof parts` / `MUD prof worst` (MUD_Prof) | per part avg / worst: VOX, SHEET (+ SH_*), REBUILD (+ RB_MESH / RB_COLLIDER), LOOK, AHEAD, OP_* |
| C | `MUD world: voxel cells live ... colliders N ms, meshes N ms, meshes later N` | the crater go-live patch cost (DoVoxCells 3690) |
| C | `MUD sheet prebuild:` | dig ahead ms (worst frame), close ms |
| C | `MUD crater seq S:` (MUD_CraterTrace) | recv -> go-live, go-live frame swap / vox / sheet ms |

### 0.2 Meters that are missing (add, TEST_KIT only, print only)
1. **A - TURN METER** (MUD_MudSheet): heading of the followed vehicle (`veh.GetYawPitchRoll()[0]` in Frame where
   `NearVehicle()` is read, ~1099), smoothed deg/s; bucket straight (< 8 deg/s) / turning / stopped (< 1 m/s).
   After EVERY Upload (end of Upload 8967), check each drawn live slot's edge record against the drawn neighbour's
   (EdgeNb 3185) for j = 0..TQ (corners included): count raised border verts `step > 3 cm`, `open`, `corner off`, per
   bucket. Also per bucket: drape cut at cap (10452), co-build sides copied over a > 3 cm step (see A11), born slots
   waiting for traces (UploadSlot `m_iBornWait++`). Append to the `MUD sheet builds:` line. This is the "at the moment
   it shows" proof the stage 3 prompt asks for: EdgeAudit is a 10 s snapshot and cannot split straight / turning.
2. **B - under-sheet look cost** (MUD_WorldGround BuildMeshBand 7747, sheet local only): per band mesh build, are
   all revealed (dirt) verts (t > 0 at 7876) under the sheet's full cover (see B9 query)? Count builds: none revealed /
   revealed all covered; time the groove section upload (UpdateIndices / UpdateVerts section 1 at 7928); and the B10
   "would hold" test (below) with the switch off. New 10 s line `MUD world B under sheet 10 s: ...`.
3. **B - look ruts the sheet draws over** (MUD_LookRuts ~255, before `t.m_aPendA.Insert`): count segments whose two
   (lead-shifted) ends lie where sheet local stamps the wheel itself (inside the live window, on `TileCovered` ground =
   MudSheet.ZoneAtWorld in local). Append to `MUD look ruts: 10 s`.
4. **D - crater notes** (MudSheet PreNote 2721): notes, notes while joining (`SCR_PlayerController.
   GetLocalControlledEntity() == null`), most notes in one sheet frame, entries dropped at the 32 cap. Append to the
   prebuild line and the 10 s builds line.

### 0.3 The measure doc (`results/2026-10-21-mud-lab-stage3-measure.md`)
Numbers already in the repo (copy them in as the "before" rows):
- Crater go-live frame on the sheet ruts, DS (stage 2 report end): set 12 69-91 ms; set 14 ~35-50; set 15 ~12-25
  (swap / voxel / sheet 1/6/9, 1/5/18, 1/7/5; off ruts 0/2/0). Set 15 split: colliders 5/4/4 ms in the go-live frame,
  meshes later 4/2/1 one per frame (12/6/2 ms total, worst 4). Ahead dig DS 12-21 ms, close 5-18 ms.
- Patch go-live table (crater, patches, colliders now, meshes now, ms) - stage 2 report, WB 23:46-23:57 + DS 23:49-23:50.
- Client rut cost without the sheet (results/2026-10-19-mesh-vs-voxel-ruts-ds.md): rebuild 1.33-2.01 ms/frame
  (rb mesh 0.85-1.33, rb collider 0.46-0.67), ahead 0.56-0.78; 86-205 patch meshes / 10 s; 4-7 ms per build.
  "client cost = look meshes".
- Sheet local cost (results/2026-10-20-mud-lab-stage1-ds.md): 2.3-5.3 ms MUD script/frame at presets 1-3; upload
  0.9-2.9, band 0.4-1.8 ms; ~600-950 tile builds / 10 s, 19-40 % would skip.
Tables to fill from the new run: A (per bucket per 10 s), B (under-sheet builds / ms, groove ms, look segments,
B10 would-hold), C (MUD prof parts for: driving on ruts, crater on ruts, crater off ruts, tunnel dig, trench; >= 6 runs
per row + an off row; top 5 costs), D (join with 30+ craters).

## 1. Part A - edges on turns

### 1.1 Candidates found in the code (ranked by how well they fit "mostly while turning")
| # | cause | why it fits turning | PROVEN-CODE | meter that proves it |
|---|---|---|---|---|
| A-1 | **Drape cut at cap leaves the old vertical skirt** on the last sides (E / W: sides run S N W E) - a straight wall with UVs copied from the top vert = **stripes**. Room: tix 3360 = EDGE_ICAP 2880 + 480 skirt; the drape needs +480 more indices (+160 verts). Any tile with > ~400 quads of dirt indices (+ SMOOTH_EDGE cut fans) runs out. | turning: wheels off-track, the swept dirt area per tile is wider; a diagonal dirt edge crosses more quads = more cuts. The user says 'drape cut at cap' is high. | BuildTileMesh drape 10445-10480 (`nv + 2 > EDGE_VCAP \|\| ni + 6 > 3360` -> break); smooth-edge cut reserve counts only the 320 skirt verts (10222), not the drape's 160 | drape cut per bucket |
| A-2 | **A co-build copies EVERY side unconditionally** (`bool mCopy = m_bCoBuilding`, 9865): a tile co-built for one neighbour also copies its other neighbours' borders, however far off (> 3 cm), ramped over 4 rows = a visible crease / wedge that stays until that other neighbour rebuilds (a stored tile may never). | turning makes more borders differ at once (the band crosses tiles diagonally) | 9865; CoBuild 3574 sets m_bCoBuilding for the stored path, UploadSlot for live pass 1 | co-copy over 3 cm per bucket |
| A-3 | **Corners are never matched**: EDGE_MATCH moves border verts j = 1..TQ-1 only; the corner vert keeps its own height -> a spike / slit at a tile corner when a side was copied or faded; the diagonal tile is never looked at. | a diagonal track crosses tile corners far more often than a straight one | 9858-9900 (mj from 1), EdgeAudit skips corners too (j from 1) | corner off per bucket |
| A-4 | **Undrawn neighbour after a window shift**: a slot entering the window from a data-only / compact store goes through FillSlot (no ground: m_aGs 0) and waits for traces (NEW_TRACES 150 / frame, ~121 per tile); meanwhile A9 pulls the drawn neighbours' borders down to band-0 height (a notch). A diagonal shift brings in 7 slots at once. | turning shifts the window on both axes | Follow 1433-1475 (`st && st.m_Ent` else FillSlot); UploadSlot `m_iBornWait` | born waits per bucket, open per bucket |
| A-5 | co-builds asked outside Upload (m_aCoNext) are cleared by every window shift (Follow 1424) | shifts are more frequent while turning at speed | 1424 | (open count after shifts) |
| A-6 | EDGE_CO_STEP 3 cm copy: copies a neighbour within 3 cm, ramped - at most a 3 cm crease, not a wedge | weak | 9870-9880 | step count stays ~0 |
Not a turning cause: the 4-row ramp itself (only moves <= 3 cm unless A-2), stored vs live records (one map, keyed
by world tile, both kinds write it at build time).

### 1.2 Fixes (each its own static switch, default off; new fix sets after 15)
- **A10 EDGE_DRAPE_FIT** (for A-1): when the drape hits the cap, the skirt quads it did not reach get the A4 apron
  shape in place (bottom verts out by the drop and down to G - BAND_UNDER - 0.02, UVs from their own position): no new
  verts / indices, no stack growth, no stripes. Do it in a sibling block AFTER the drape block (BuildTileMesh is near
  64 locals / 80 KB stack - do not grow tv / tix): note the first left skirt vert + end in two members at the cut,
  then loop. Alternative (bigger, stack risk): reserve 160 more verts in the cut test and grow tix by 480.
- **A11 EDGE_CO_SIDE** (for A-2): a co-build copies unconditionally only the side(s) facing the tile(s) that asked
  (`CoBuild(tx, tz, os)`, a side mask per live slot for pass 1, a mask member for the stored path). Its other sides keep
  the 3 cm rule; a side that would move there is sent to a LATER build (live: m_aCoNext + mask -> CoPick next upload;
  stored: QueueBuild as a plain build), never built now (no chains).
- **A12 EDGE_CORNER** (for A-3): a copied side also takes the neighbour's two drawn corner verts (W / E after S / N,
  last wins). Fade sides leave corners alone.
- Proposal only if A-4 shows in the meter: keep the drawn neighbours raised (drape) instead of fading while an entering
  slot is only waiting for traces, or restore a data-only store's traced ground on entry (needs a ground-staleness check).
Sets: 16 = 15 + A10; 17 = 16 + A11 + A12. (FixApply 3691: change the B5-B7 tests from `s == 13 || ...` to `s >= 13`
etc. so later sets keep them.)

## 2. Part B - ruts under the sheet = physics only

### 2.1 Look costs of the voxel ruts where the sheet draws (sheet local)
In sheet local the drawn sheet is `G - BAND_UNDER + (BAND_UNDER + LIFT + D) x band` (9843), G = traced ground (the
collider). So where band = 1 the sheet is >= LIFT (1 cm) over the collider ground and hides the patch mesh. Visible
patch ground under the sheet only: band < 1 (the band edge, ~BAND_IN + BAND_FADE out), bare cells / taper (craters,
tunnel mouths), undrawn tiles (not traced yet, data-only, past MESH_R 70 m), the A9 fade / ramp at a side with no
drawn neighbour, far LOD tiles.
| look cost | where | skips under sheet local today? | PROVEN-CODE |
|---|---|---|---|
| patch band mesh rebuild on every rut op (input pass, mesher, reveal + UV, MeshObject) - client ~0.85-1.33 ms/frame driving, 4-7 ms per build (MEASURED 10-19) | WorldGround Rebuild -> BuildMeshBand | **no**: LAZY_RUT_MESH needs SYNC (`LazyOn` 5002 = `... && SYNC`), sheet local has SYNC false | 5002, SheetCovered 4830 |
| groove (dirt) section: a 2nd copy of the whole surface 3 cm under it (nv verts, ni indices, its own normals) + the reveal sink | every band mesh with m_RutMaterial set (prefab default DirtCover_01) - even patches with NOTHING revealed | no | 7876, 7928, Create 7898 |
| client look ruts (cut ahead of each wheel, LEAD) -> patch mesh rebuilds | MUD_LookRuts.Tick | **no**: SHEET_SKIP / SHEET_LOOK use MudZone.SheetCovers, which is false in sheet local (MUD_MudZone.c:62) | LookRuts 236 |
| rut patch collider + mesh at a crater go-live | DoVoxCells 3690 | mesh moved later (B7), collider stays (physics) | 3690-3790 |
| rut game material (RUT_MAT_D / FIELD_SOIL_D) on colliders | BuildCollider | physics: KEEP | |
| voxel brick meshes in rut columns | MUD_VoxelProbe | mostly NOT under the sheet (craters / tunnels make bare holes - seen) | |
| patch lift / LIFT_BY_DRAWN / SheetLiftDirty | LiftSetup 6935 | already off in local (PatchLiftOn needs SYNC) | 6935 |

### 2.2 Cheapest physics-only form (switches, one per piece, sheet local only, colliders untouched)
- **B8 LOOK_UNDER_SKIP** (MUD_LookRuts): no look cut for a segment whose two lead-shifted ends lie where sheet local
  stamps the wheel itself (inside the live window +- 2 cells, on TileCovered ground). The band (>= BAND_IN round the
  tyre) hides the look rut there, and the sheet never reads the look floor in local. Remote trucks far from the
  window keep their look ruts (the sheet only gets them when the op comes). Needs `MUD_MudSheet.LocalStampsAt(x, z)`.
- **B9 GROOVE_UNDER_OFF** (BuildMeshBand): build the patch mesh without the groove section when every revealed vert
  is under the sheet's FULL cover; heights and sink unchanged (so nothing pokes through). Needs
  `MUD_MudSheet.FullCoverAt(x, z)`: nearest sheet vert traced, not bare, band >= 0.999; its tile drawn (EdgeDrawn),
  full detail, no bare in taper reach (NearBareTile); within 4 rows of a side (the A9 ramp) the neighbour drawn and
  raised at band 1 at that border vert (from its edge record) - cache per tile per frame. Cover can be LOST later (tile
  dropped / hidden, crater, sheet local off): keep a list patch -> revealed grid verts, re-check one listed patch per
  frame, rebuild (m_fMeshBuiltAt = -100: no same / small skip) when a vert is uncovered; switch / local off -> rebuild
  all listed at once. Worst case of a miss: a rut in grass texture for a fraction of a second.
- **B10 MESH_UNDER_HOLD** (queued BuildMeshBand only): skip a patch mesh rebuild when (kept input = shown mesh, same
  skin classes, nothing hidden owed) every vertex that changed is under full cover AND its shown height less its
  groove sink is <= new ground + LIFT - 2 cm (the sheet never draws lower than ground + LIFT). Keep the kept input and
  the changed box (no KeepMeshY / MeshDone) so the next real build reads it all; add the changed verts to the B9 watch
  list. This is the big one if the meter says so: client rut meshes are most of the client rut cost. The 9 cm reveal
  sink (RevealGap 0.03 x 3) gives slack for ~2 more 3 cm cuts on worn ruts; fresh cuts on unrevealed ground fail the
  test (rebuilt as today).
- Not proposed: hiding whole patches (LAZY_RUT_MESH style) - a 8 m patch is almost never fully under a ~5 m band.
  Coarser meshes under the sheet - band mesher already merges; B10 removes the rebuilds instead.
Sets: 18 = 17 + D1 (below); 19 = 18 + B8; 20 = 19 + B9; 21 = 20 + B10.
Watch: sheet edges (band < 1), holes (bare + taper), undrawn tiles, the 70 m mesh radius, sheet local off.

## 3. Part C - the whole system (research only; user picks before any build)
Write `research/2026-10-21-voxel-sheet-optimise.md` from the Part 0 numbers. Candidates found while reading
(est. = INFERENCE until the meters say):
| # | idea | where the ms is | est. saved | effort | risk |
|---|---|---|---|---|---|
| C1 | **Put the stage 2 cost fixes (set 5: BAND_INCR, BAND_MICRO, BUILD_SKIP_SAME, UV64_FAST, BAND_STORED_HOLD) into the current path** - sets 9-15 have none of them | band 0.4-1.8 ms/frame -> ~0.05-0.25 (research 10-20 sec 3); skip-same 19-40 % of builds | ~0.5-2 ms/frame driving | a fix set, no code | low (bench showed incr = full, diff 0) |
| C2 | B10 hold + B8 (Part B) | client rut meshes 0.85-1.33 ms/frame | up to ~1 ms/frame driving | built in B | medium (look) |
| C3 | **spread the B5 ahead dig** (PreStep 2873 runs QueueChange + DigNow for every noted crater in one frame): a ms budget per frame over the recv -> go-live window (50-120 ms = 3-7 frames) | 12-21 ms DS in one frame | ~8-15 ms off the worst frame | medium | low |
| C4 | **pre-build the rut patch colliders' trimeshes before the go-live** (geoms from the carved data while the bricks build; CreateStatic / swap at the go-live, same shape, same frame of change) | DoVoxCells colliders 4-28 ms in the go-live frame | most of it | high | medium (physics timing must stay the go-live frame) |
| C5 | crater bricks show a bit late: client URGENT_MS for a batch with a crater, close the batch after a crater (research 10-20 O4) | recv -> go-live 50-120 ms | 1-2 frames earlier | low | low (look timing only) |
| C6 | groove section off where NOTHING is revealed (outside the sheet too; identical look) | the section-1 upload of every band mesh without ruts | B meter 'none revealed' x groove ms | low | needs the user's OK (outside "where the sheet draws") |
| C7 | sheet ground from data instead of traces: G = TopAt + FieldFloorAt (PreSurf, proven by B5/B6) instead of physics traces (NEW_TRACES 150 / frame) - both systems compute the same ground | SH_GROUNDS | trace ms + no 'born waits' (also fixes A-4) | medium | medium (rocks / props traced today) |
| C8 | EDGE_AUDIT off once A is accepted (EdgeRecord on every build) | ~3-5 % of each sheet build | small | trivial | none |
| C9 | tunnels / trenches / shovel: measure first (prof table rows); no sheet there in most cases | VOX, OP_* | ? | ? | ? |
Rank them by MEASURED ms saved per effort after the session; stop and let the user pick.

## 4. Part D - MP checks for the crater fixes (sets 13-15)

### 4.1 JIP with many craters - what the code says
- A joiner's replay goes Drain (VoxelProbe 1694, JOIN_MS a frame while `Joining()`) -> Submit -> SubmitBody ->
  `MUD_MudSheet.TakeOp` (4086) -> PreNote, but ONLY if `CRATER_PREBUILD && ENABLED && s_Inst.m_bLocal`.
- On a fresh client process all of these are off (sheet local and the fix sets are numpad switches; a new play resets
  them via MUD_MudLab.Tick 'new play'). So a normal join never reaches PreNote. It does only if the user turns on sheet
  local + set 13+ while the replay is still draining (839 ops took 6.6 s, results/2026-10-14-jip-size.md), or in WB
  when the statics survive into a new play for the frames before MudLab resets them (MudSheet.Tick 941 makes the new
  instance with `m_bLocal = !SYNC` from the old statics).
- If it is reached: PreStep digs EVERY un-dug note in ONE frame (QueueChange per crater + one DigNow) -> one long frame
  that grows with the craters drained that frame. The 32 cap (PreNote) calls PreForget, which drops all notes: those
  craters just take the normal go-live dig (dug-ahead ones are dug again - same result, double cost). **No missing
  holes from PreForget** (PreTaken false -> normal GroundChanged). The held tiles / window are not cleared by PreForget;
  a go-live later than PRE_MAX_MS 1 s closes the window early ("no go-live in time") - held meshes show before the
  voxels: a look glitch, not a hole loss. Also: PreClose at a go-live calls PreForget, dropping notes of the NEXT batch
  (they go the normal path).
- **D1 PRE_LIVE_ONLY** (switch, client only, no op / J2 change): PreNote ignores craters while joining (no local
  controlled entity) and past PRE_FRAME_MAX (3) notes in one sheet frame -> those take the normal go-live dig. Side
  effect: craters while the player is dead / in the respawn menu also take the normal path. Set 18.
- Test: join with 30+ craters on the sheet ruts twice - (a) turn sheet local + set 15 on AFTER the join finished
  (expected: PART D notes 0 while joining); (b) turn them on as fast as possible during the join (set 15, then set 18).
  Read: PART D notes (joining N, most in one frame N), dropped at the 32 cap, prebuild 'dig ahead worst frame', holes
  present (user look).

### 4.2 Two players, same crater
- Physics: colliders come from the same op data on every machine; B5-B7 only move LOOK timing (sheet tiles held /
  shown at the go-live; B7 moves patch MESHES a frame or more later, the collider is still built in the go-live frame,
  DoVoxCells 3740-3750). So the ground under the wheels cannot differ by these sets (PROVEN-CODE). The new switches
  (A10-A12, B8-B10, D1) are look / timing only too; B10 never touches colliders.
- Test (two clients side by side, both sheet local + set 15, later the final set): same hole, same rut look, no truck
  floating / sinking on either; 'MUD corr ... mine 1' snaps per client. Compare both clients' crater lines.

## 5. One combined test session (after the build)
WB (look / function only):
1. Compile clean. Sheet local (numpad 3), numpad * to set 15, then 16, 17 ... 21: each name on screen.
2. Drive straight 20 s, circle 20 s (tight), stop 12 s, per set 15 / 16 / 17 - striped wedges by eye? TURN METER lines.
3. Set 20 / 21: drive over old ruts; look at the band edge, a crater on the ruts, a tunnel mouth, drive 80 m away and
   back (mesh radius) - any grass-coloured rut or rut poking through the mud?
4. Craters on the ruts with set 18 (live notes still pre-build).
DS (ms only, >= 6 runs per row, an off row): rows off / set 15 / 17 / 19 / 20 / 21 for: driving on ruts, crater on the
ruts, crater off the ruts, tunnel dig, trench. Plus Part D join and the two-client check.
