# Ground improvement plan: ruts, craters, trenches, tunnels (2026-10-16)

User: "How do we improve, change or make it just better? Don't implement, just make the plan and why, and fact check,
do research online."
Status: PLAN ONLY - nothing built. Feature freeze (master list 2026-10-15) still on: most items below FIX shipped
features (they protect what exists); the user decides which go in now.

Built on (read these for the evidence):
- research/2026-10-16-plan-review-second-opinion.md (independent review 2026-10-16: corrections to U1, U2, T1, R0 marked CORRECTION below)
- research/2026-10-16-fact-check-ruts-craters-trenches-tunnels.md (verdict per claim)
- results/2026-10-16-gap-checks.md (problems, measured offline) - Tools/gap_checks.py
- results/2026-10-16-improve-checks.md (the fixes below, measured offline) - Tools/improve_checks.py
- results/2026-10-16-voxel-crater-slope.md (crater profiles) - Tools/crater_slope_voxel.py
Labels: VERIFIED-CODE, MEASURED (offline, game formulas), ESTIMATE, UNVERIFIED (needs game), READ (web search text;
the network policy blocked the source pages).

## 0. Rules every change keeps (from the project's proven history)
1. One drawn + felt surface per spot (every two-ground attempt failed: 10-13, 10-13/14, 10-15 - architecture review).
2. Ground never changes under or just ahead of a moving tyre (RUT_BEHIND, PROVEN v035 0 corrections / 90 s).
3. Every op is deterministic: values that differ per machine (random, camera, live entities, timing) are decided by
   the SERVER and carried in the op text; clients only replay. JIP replays the same text.
4. Measure before and after: offline first (Tools/), then the combined test sessions in plans/2026-10-16-next-steps.md
   (Workbench = function / look only; performance only on a dedicated server or the standalone game - user 2026-10-16).
   The per-item 'Verify' lines below are folded into those sessions.
5. Checkpoint before each item (mud_backups/checkpoint.ps1); one switch per item so it can be turned off.

## 1. Phase 0 - five game checks before any code (one session; they decide what Phase 1 needs)
| # | Check | Why | Pass / what it decides |
|---|---|---|---|
| 0.1 | Rifle fire at a target prone in a voxel mortar crater (shooter 50 m out, aiming low over the rim); at a standing target in a 1.5 m voxel trench (aim at the parapet); straight down into flat voxel ground | VERIFIED-CODE: no FireGeo bullet slab under voxel cells (VOX cells get slot -1, MUD_WorldGround.c:5652); the heightfield needed one ("bullet got stopped", results 2026-10-09 task 6) | rounds stop -> skip V1; rounds pass -> V1 first |
| 0.2 | Walk (no jump) out of an 81 mm and a Hydra voxel crater; out of a 1.5 m trench; prone in a crater wall | MEASURED: 14-18 % of an 81 mm+ crater over soldier MaxSlope 50 deg; trench walls 49-57 deg; GroundRescue is off on voxel cells (VERIFIED-CODE :5011) | decides how urgent C1 and T4 are |
| 0.3 | Spawn a fresh vanilla truck; look for `MUD wheels: <id> registered` | MUD_Wheels.c:24 finds vehicles only as VehicleWheeledSimulation; Bohemia moved vanilla vehicles to VehicleWheeledSimulation_SA (READ) | line present -> fine; missing -> R0 before anything else |
| 0.4 | Local DS + 2 peers, 3-4 trucks off-road 5 min, a temp log line every 10 s: rebuild queue length (m_aDirty.Count), oldest dirty age, builds/s | ESTIMATE: ~3 trucks fill a 30 fps DS's 1 collider / frame | queue drains -> R1/R2 can wait; grows -> R1/R2 now |
| 0.5 | A temp switch that issues 21000 fake rut ops on a local DS, then a late join | VERIFIED-CODE: client drops ops past 20000 held (MUD_VoxelProbe.c:974), no recovery | expect 'waiting 10 s for op seq' -> confirms J1 |

## 2. Phase 1 - MP blockers and safety (small, mostly local code)

### J1 - a joiner must never lose an op
- Problem: Receive drops ops once 20000 are held; ops apply strictly in order; JIP is asked once and answered once per
  controller -> one lost op freezes that player's ground for the match (VERIFIED-CODE, fact-check F1). The server
  sends the whole log in one frame (VERIFIED-CODE MUD_VoxelNet.c).
- Change: (a) Receive never drops: past a cap it stops ACCEPTING and remembers "resend from seq N"; once drained it asks
  the server for seq >= N (new RPC with a from-seq; the server answers from m_aOps). (b) The server sends JIP batches
  spread over frames (N per frame, as the patch JIP's "8 full copies per frame" attribute does), oldest first.
- Why: it is a correctness bug that grows with match length; nothing else in this plan helps a joiner it hits.
- Verify: check 0.5 again -> joiner reaches the server's seq and the same `MUD disp hash` / column count.
- Cost: small. MP: no format change.

### R1 - the rut field builds only near the camera on clients
- Problem: FloorDirty -> EnsureRange(.., wheel) -> EnsurePatch builds / wakes the patch synchronously (one per frame,
  ~9-12 ms, MEASURED 2026-10-09) and marks existing patches for mesh + collider rebuilds, with NO distance check
  (VERIFIED-CODE MUD_WorldGround.c:3891-3920, :3144-3175). The old height path skipped far patches (FarFromCamera ->
  HeightsOnly, :6999).
- Change: on a proxy, FloorDirty for a patch farther than m_fClientBuildRadius (300 m) calls HeightsOnly (setup only,
  held in m_sFar; FarDue builds it when the camera comes near - existing code :7049-7100); Rebuild skips mesh/collider
  rebuilds of far live patches on proxies (they rebuild when near). The floor data is already in the field, so nothing
  is lost and nothing is networked. The server keeps building (it needs colliders where vehicles drive).
- Why: per-client cost then scales with what that player can see, not with every vehicle on the map.
- Verify: 2 clients, a truck rutting 1 km from client B -> B's 10 s line shows no floor builds; B drives there -> ruts
  appear (FarDue). Hash per tile equal.
- Cost: small. MP: local only.

### R2 - band mesher off for the heightfield (prefab value)
- Problem: the prefab runs the band mesher (m_bBandMesher 1). MEASURED 2026-10-13: band mesh 2.9-4.7 ms vs grid 0.9-1.1,
  band collider 4.2-5.5 vs grid 1.4-1.6 per patch. It was built for heightfield trench / crater walls, which are voxel
  since v031/v035; no doc says why it stayed on (VERIFIED by grep).
- Change: m_bBandMesher 0 in Prefabs/Mud/MudWorldGround.et. The grid path draws the rut field with the same soil rule
  (VERIFIED-CODE BuildMesh :5832-5845, BuildCollider :5955-5962).
- Why: ~3x cheaper rut rebuilds on every machine, server included, with no code.
- Verify: same drive band on / off - rut look and road-edge look (user), 'MUD band' / mesh ms lines, robot snaps.
- Risk: the band drew kept-cell edges with a skirt; check road edges for gaps before keeping it.

### V1 - bullet slab and safety plate under voxel cells (only if check 0.1 fails)
- Problem: VERIFIED-CODE: carved (VOX) cells have no patch collider, so no FireGeo slab and no safety plate; bricks are
  Terrain | Foliage only. A crack in a brick body = an endless fall (holed terrain) and GroundRescue is off there.
- Change: in MakeBody, (a) a 3rd geom: the brick's TOP-section triangles moved down by min(1.5 m, solid thickness under
  each vertex - read from the column's samples) and reversed, FireGeo only - so no slab hangs inside a tunnel;
  (b) a plate: 2 triangles per converted column at undug - FLOOR - 0.5, Terrain layer.
- Why: restores the two protections the heightfield proved it needed.
- READ: Reforger penetration compares the round's penetration with the game material resistance x the collider
  THICKNESS (Bohemia wiki snippet); how thickness is found on a one-sided trimesh is undocumented - so test, don't assume.
- Cost: +1 CreateTriMesh per brick build (~0.06-0.3 ms at our sizes, PROVEN mud-gotchas). Deterministic (built from
  the field).

### R0 - only if check 0.3 fails: find SA vehicles too
- Change: MUD_Wheels.Register and the two casts also accept the SA class behind one helper. The script API lists
  VehicleWheeledSimulation_SA_B (READ); check in Workbench that it has WheelCount / WheelHasContact /
  WheelGetContactPosition before relying on it. Verify: the log line, a rut, the robot.
- CORRECTION (second opinion 2026-10-16): not "one helper + two casts". VehicleWheeledSimulation is a typed parameter
  in 13 places in MUD_WorldGround.c (+2 MudSheet, +1 GroundEntity), and the API snippet says SA_B inherits
  VehicleBaseSimulation (a sibling class). If needed, R0 is a medium refactor. Today trucks register (every robot run),
  so this is a smoke check after game updates.

## 3. Phase 2 - shape fixes (each is one function on the op path; deterministic)

### C1 - crater profile: the heightfield's cosine bowl in Carve
- Problem: MEASURED: today's ellipsoid crater turns vertical at the crest: 81 mm 14 % of the area > 50 deg, steepest 67
  deg, even the easiest way out climbs 0.21 m steeper than 50 deg; Hydra / TM-62M worse (72-74 deg).
- Change: Carve's bowl below the hit = the heightfield profile (crest Rc = (D + H) x pi / (2 x 0.9), cosine to the
  rim, rim fall over W) written as a vertical-distance field, as Tools/crater_slope_voxel.py CraterCos does.
- Why: MEASURED: every triangle <= 44 deg; crater radius -25 % (81 mm outer 4.58 -> 3.49 m) -> ~31 -> ~25 columns per
  crater, so MAX_COLUMNS 6000 lasts ~240 instead of ~195 mortar craters (ESTIMATE, column count averaged over 4000
  positions). No FUNNEL needed (a vertical-distance cut cannot undercut).
- Verify: SP walk-out (0.2 again), look; MP: crater ops hash (add one, see section 5).

### C2 - repeated hits: the stack cap on the voxel path
- Problem: MEASURED: each hit is a fresh full-depth Carve on the already dug surface: 81 mm hits 2 / 3 reach 2.88 / 3.0
  m (heightfield cap 2.39 m); 82 mm reaches 3.0 m by hit 4.
- Change: the server, before Issue, reads how deep the hit spot already is (ColumnDepth) and sends depth =
  max(0.15, target - current), target = max(full, min(m_fCraterStackDepth, current + m_fCraterDeepenGain x full)),
  as Crater() does for the heightfield. Carried in the op's depth field (no format change).
- Why: MEASURED: 81 mm series settles at 2.37-2.39 m, 82 mm at 1.38-1.42 m.
- Note: ColumnDepth reads the rut floor (FloorMin) - fine on the server (it decides; clients get the number).

### C3 - craters fade out at roads / kept ground
- Problem: MEASURED: hard columns are never dug, so a crater 1-3 m from a road leaves a 0.8-2.0 m drop within 0.5 m
  (58-76 deg). The heightfield faded depth to 0 over 1 m (EdgeLock smoothstep, VERIFIED-CODE).
- Change: Carve blends the new value with the old by smoothstep(distance to the nearest hard column / FADE). Distance
  from the skin hole map -> same on every machine. CutTrench has the same hard skip (VERIFIED-CODE :3315) - a trench
  dug up to a road likely ends in the same cliff (not measured).
- Why: MEASURED with FADE 1 m: the road-edge drop halves (81 mm at 1 m: 1.40 -> 0.69 m); with C1 the bowl wall itself
  stays <= ~44 deg. FADE 1.5-2 m for the biggest charges is the tuning knob.

### C4 - a blast under a roof (in a tunnel) carves a chamber, not a hole to the sky
- Problem: MEASURED: Carve's funnel above the hit is air up to wp + FLOOR + rim + 1.5 m: an RPG in a tunnel opens a
  ~4 m wide hole to the sky, an 81 mm-size charge ~8 m. Crater() only refuses blasts ABOVE the undug ground
  (VERIFIED-CODE :5374-5378).
- Change: in Carve, if the field has a solid sample within ~3 m straight above the hit, carve a sphere r = depth
  (FLOOR kept) instead of bowl + funnel. The test reads the field -> identical on every machine.
- Why: MEASURED: 0 columns opened to the sky for all three charges.

### C5 - crater size: a decision, not a bug (READ + calculation)
- Fact: real craters are much smaller. CRREL SR 95-2 (winter): 81 mm point-detonating ~2.3 m across, 0.16 m deep;
  CRREL 2007: 120 mm 2.69 m across, 0.72 m deep. Ours: 81 mm 1.43 m deep, 6.3 m across the crest.
- Crater scaling literature (READ): linear crater size ~ charge^(1/3) (cube root), ~0.3 power in desert alluvium. Our
  FullDepth uses kg^0.6 up to the 1.35 m knee (tuned on purpose: "40 mm 0.3 m, mortar 1.5 m", attribute comment).
- Option: a "realistic" preset by prefab attributes only (m_fCraterTntPower 0.333, m_fCraterDepthPerKg ~0.47): 81 mm
  0.47 m deep / 2.1 m across, Hydra 0.67 / 2.9, TM-62M 0.92 / 4.0 (calculation, results/2026-10-16-improve-checks.md;
  one measured point, order of magnitude only). Smaller craters = less cover and far fewer columns.
- The user picks: today's big game craters, the realistic preset, or in between.

### C6 - crater variety back (jitter + lobed outline)
- Problem: VERIFIED-CODE: the voxel path gets only the depth; the heightfield's depth jitter (+-15 %) and lobed
  outline (+-15 %) are skipped -> every crater of one charge is identical.
- Change: the SERVER rolls jitter / lobes / turn and writes them into the crater op's free fields (b, plan text -
  VERIFIED-CODE: kind 0 uses only the depth, Encode/Decode :2081-2125). Carve scales the bowl radius by angle.
- Why: variety at no net cost; deterministic because the random numbers travel in the op.

### T1 - trench floors as wide as planned
- Problem: MEASURED: drawn floors are 0.1-0.47 m narrower than planned and vary with grid position (1.0 m plan: 0.46-0.72
  m) - widths were picked for the old 0.25 m heightfield grid (MUD_TrenchLayout.c WidthOf comment); our prone
  collider is 0.6 m (Character_Base.et override).
- Change: CutTrench / FillTrench treat a column as inside when it is within half a sample (0.25 m) of the piece
  (OutsideDist - 0.25). Same layout math, same on every machine.
- Why: MEASURED: with a 0.25 m pad the drawn floor = plan within +0.10 m for all four widths.
- CORRECTION (second opinion 2026-10-16): that is the MEAN. Per grid offset the spread stays ±0.15 m (1.0 m plan +
  pad: 0.92-1.22 m). A 0.25 m pad gives exactly the same numbers as planning 0.5 m wider (pad 0.25 / 1.0 m = pad 0 /
  1.5 m), so changing the WidthOf table is the simpler form.

### T2 - spoil parapet (trenches give the dug earth back as cover)
- Problem: VERIFIED-CODE: CutTrench only lowers; no parapet. Doctrine (READ): fighting positions use the spoil for
  flank / rear parapets (front when natural cover is lacking); parapet <= ~50 cm high, >= ~90 cm wide (older FM);
  current ATP 3-21.8: parapet walls >= 39 in (~1 m) thick; positions armpit deep.
- Change: the trench plan gets a parapet side (planner key, stored in the plan text); each swing also raises a band
  outside the sloped wall on that side (a FillTrench-style "only raises" pass) to <= 0.4-0.5 m, ~1 m wide on top.
  The parapet is deterministic (plan text + layer count).
- Why: real cover without digging deeper. INFERENCE: a 1.5 m trench + 0.4 m parapet should hide a standing soldier's
  head - CHECK in game (no published Reforger character dimensions were found).
- Volume note: a 1.5 m deep, 1.0 m floor, 60 deg trench removes ~2.8 m3 per metre (geometry); the parapet uses only
  part of it - no volume bookkeeping needed (MudRunner / Valheim don't keep volume either, research 2026-10-14).

### T3 - a way out of trenches
- Problem: walls 49-57 deg drawn (MEASURED) vs MaxSlope 50 (IDA); Reforger climb / vault limits unpublished (READ: none).
- Change (after check 0.2): an optional ramp at a trench end (one piece whose run is depth / tan 35 deg) chosen in the
  planner; or nothing if soldiers can climb out in game.

### U1 - tunnels may go deeper (FLOOR per op kind, depth boxes read)
- Problem: VERIFIED-CODE: FLOOR 3 m for everything; MEASURED: a full-height tunnel under it has a 0.45-0.87 m roof
  (1-2 solid samples). Real references (READ): Cu Chi levels ~4 / ~6 / 7-10 m deep; WWI dugouts 1.8-9 m, an
  "ordinary" one ~2 m of earth on top.
- Change: CarveTunnel uses TUNNEL_FLOOR 6 m (craters / trenches keep 3); a column's floor also reads
  MUD_DepthBoxEntity.DepthAt (placed entities: same everywhere). Rows below are made lazily already (Put).
- Why: MEASURED with FLOOR 6: a tunnel dug to 4 m gets a 2.1-2.4 m roof, to 5 m 2.8-3.1 m.
  CORRECTION (second opinion 2026-10-16, MEASURED: Tools/plan_review_checks.py trace): those labels are the "dig to"
  target. The feet overshoot it: target 3 -> feet 3.6-4.2 m, target 4 -> 4.3-4.9 m. Read the roof as feet depth
  - ~2.5 m.
- MUST come with: Carve / CarveTunnel / the C4 sphere clamp `if (floorY - y > d) d = floorY - y` (MUD_VoxelProbe.c
  :2714, :3272) forces SOLID below undug - FLOOR. So a crater (FLOOR 3) inside a FLOOR-6 tunnel refills it (MEASURED:
  81 of 218 air samples, plan_review_checks.py refill). Change it to "no NEW air below the floor":
  `d = max(d, min(old, floorY - y))`.
- MUST come with: T needs a shovel + a slower rate (decision 3). Today the server checks only on foot, range and a
  250 ms rate (VERIFIED-CODE MUD_VoxelNet.c), so with FLOOR 6 one player digs a 6 m shaft in seconds.
- Cost: more brick rows under tunnels only (memory per tunnel up; MAX_COLUMNS unchanged).

### U2 - "look straight ahead" digs level
- Problem: MEASURED: at pitch 0 the capsule bottom is 0.3 m under the feet (0.95 - 0.45 - 0.8, VERIFIED-CODE :313-316),
  so a tunnel dug looking ahead sinks ~0.17 m per 0.5 m walked (~18 deg) until it reaches FLOOR. Every tunnel ends up
  at the floor, which is why every tunnel has the thin roof.
- Change: TUNNEL_MID 0.95 -> 1.25 (capsule bottom at the feet at pitch 0); look down to go down, up to go up.
- Why: tunnels stay at the depth the player chose; with U1 that is the roof the player chose.
- CORRECTION (2026-10-16, MEASURED: plan_review_checks.py trace): with TUNNEL_MID 1.25 the feet still sink ~3.5 cm
  per m (3.2 -> 3.53 m over ~9 m). Add a small lift or hold the start depth; check over 20 m.

### U3 - roof guard + collapse rules (after U1 + U2)
- From research/2026-10-15-voxel-stability-and-optimisation.md: server dig-time guard (refuse / open a capsule that
  would leave < ROOF_MIN of roof), then R1 thin roof and R2 thin bridge rules inside the op apply (deterministic).
  R3 floating chunks = a flood fill from anchored samples, the standard approach (READ: GameDev floating-chunk thread;
  26-connectivity choice matters).
- Why after U1: with FLOOR 3 a 1.0 m ROOF_MIN would collapse nearly every tunnel (MEASURED roofs 0.45-0.87 m).

### R3 - heavy-truck rut walls under 45 deg
- Problem: MEASURED: at cap 0.35 m and RUT_R 0.5 the rut walls are p90 46 deg, max 48.5 deg; the profile shape barely
  matters (1-k2 / (1-k2)^2 / cosine all 45.6-48.5). The wheel is one ray with the raw triangle normal (IDA 2026-10-15).
- Change: RUT_R 0.6 (or radius = max(0.5, cap / 0.58) so light vehicles keep narrow ruts).
- Why: MEASURED RUT_R 0.6: p90 40.7, max 44.3 deg.
- Verify: robot snaps >= 6 runs (feel), user look (wider ruts).

### R4 - first pass does most of the rut (feel / look)
- READ: soil-bin data: the first pass is ~55 % of the final rut depth, increments shrink per pass (track wheel, 2 kN -
  only the trend transfers); Vennik et al. 2019 (7 t military truck): deeper ruts on grassland than arable soils.
  Ours: equal 0.08 m steps (first pass = 23 % of a 0.35 m cap).
- Change: step = max(0.08, 0.45 x (cap - current depth)) per pass (shrinking steps), still cut 3 m behind (rule 2).
- Verify: user feel in SP, robot snaps in MP (the heightfield rut step attribute says 0.12 m was 'rough + worse MP
  stutter', user 2026-10-10, MUD_WorldGround.c m_fRutStep - RUT_BEHIND did not exist then).

### R5 - pressed grass along tracks (client-only look)
- IDA 2026-10-15 item 3: AffectFoliage* = one 20-byte obstacle per call, max 1000 per frame, flattened grass stays
  down ~10 s after the last submit (Render diag defaults FlattenLying 10 / FlattenRecovery 5, INFERENCE seconds).
- Change: re-submit the recent rut segments near the camera every ~5 s. No net, no geometry.

## 4. Phase 3 - scale (long matches, many vehicles)

### J2 - JIP as a rut-floor snapshot (needs J1)
- Problem: JIP = replay every op; ruts are 96 % of it (MEASURED jip-size).
- Change: the server sends (1) all voxel ops (kinds 0-3) in order, (2) then every rut tile (MUD_RutTile, 16 x 16) as
  depth under undug in cm, one char per sample, NONE runs RLE'd, with the seq it is valid at, (3) then live ops after
  that seq.
- Why: MEASURED: ~3x smaller than the rut op log for one pass, ~9x when the same tracks are driven 3 times (the
  snapshot is bounded by AREA, the op log by TIME); joiners skip replaying thousands of CarveFloor ops.
- Prerequisites found by the fact check (VERIFIED-CODE):
  a. Store the floor in cm when it is written, on every machine - else a cm snapshot differs from the server's float
     floor by up to 5 mm (the server's move check allows ~speed/300 m, PROVEN mud-gotchas).
  b. FillTrench branches on ColumnDepth, which reads the rut floor (FloorMin) - replaying fill ops without the ruts of
     that time could fork the field. Make the fill's depth test read the density only.
  c. Craters / trenches / fill clear the floor where they move the ground (Put -> ClearFloor) - so load the snapshot
     AFTER replaying the voxel ops, never before.
- Verify: per-tile floor hash server vs joiner (DISP_HASH already hashes the floor in mm); join time.

### S1 - many vehicles: measure, then gate the server
- After R1 + R2: repeat check 0.4. If the DS queue still grows: rebuild colliders only within N m of any vehicle or
  player (the DS has no camera; the wake lead already rebuilds on approach).

### C7 - ground ops respect buildings, and things on the ground follow it
- Building lock (VERIFIED-CODE: voxel ops skip only m_aHard): the SERVER lists the building boxes an op touches in the
  op text; every machine treats samples inside them as hard. Not a per-machine entity query - buildings can be
  destroyed or GM-placed at different times on a joiner.
- After a go-live: server wakes dynamic bodies over the changed box (SetActive, as MUD_WakePlaced does for GM spawns);
  static world props whose base is now > 0.3 m above the surface are hidden locally on every machine (same world
  file = same result). Mines: design call.

## 5. Cross-cutting: make MP checkable for craters / trenches / tunnels
- VERIFIED-CODE: only the rut floor is hashed (DISP_HASH); the voxel density of craters / trenches / tunnels / fill is
  never compared between machines. Add a per-op field hash (sum of Round(d x 1000) over the samples Put changed), log
  it like 'MUD disp hash', compare with Tools/compare_hashes.py. Needed to sign off C1-C4, T1-T2, U1-U3, J2.
- READ: hosted Reforger servers often run the native Linux dedicated server; all our MP proofs were Windows-only.
  Run one session on a Linux DS with the hashes on.

## 6. Order and dependencies
| Step | Items | Needs | Size |
|---|---|---|---|
| 1 | Phase 0 checks 0.1-0.5 | - | 1 session |
| 2 | J1, R2 (prefab), R1, R0 if needed | 0.3, 0.4, 0.5 | small |
| 3 | V1 if 0.1 failed; field hash (section 5) | 0.1 | small |
| 4 | C1 + C3 + C4 (one Carve rewrite), C2 (server), C6 | hash | medium |
| 5 | T1, then T2 / T3 | 0.2 | small / medium |
| 6 | U2, U1, then U3 | - | small / medium |
| 7 | R3, R4, R5 | robot | small each |
| 8 | J2, S1, C7 | J1 | medium |
| - | C5 crater size | user decision | attributes only |

## 7. Not doing (measured dead ends - unchanged)
0.25 m voxels everywhere (3.4-5.6x cost), dual contouring for ruts (sample spacing limit), smoothing pass (no visible
gain), any second ground mesh (sheet / look-feel split), GPU deformation (no shaders for mods), track decals on our
meshes (TrackDecalFactory is terrain-only, IDA 2026-10-15).

## Sources (READ via search results; the pages themselves were blocked by the network policy)
- CRREL SR 95-2, Winter Tests of Artillery Firing into Eagle River Flats - https://www.enviro.wiki/images/d/dd/Collins-1995-Winter_tests_of_artillery_firing_into_Eagle_River_Flats.pdf
- 120-mm mortar crater geometries, Eagle River Flats 2007 - https://www.researchgate.net/publication/235178638
- Crater scaling laws for desert alluvium - https://www.osti.gov/biblio/4158442 ; Craters produced by underground explosions - https://www.sciencedirect.com/science/article/abs/pii/S0045794909001710
- KNDS 120 mm HE mortar (2.5 kg Comp B) - https://knds.com/en/products/ammunition/120mm-he-mortar ; M933/M934 - https://www.globalsecurity.org/military/systems/munitions/m933.htm
- ATP 3-21.8 (2024) fighting positions - https://infantrydrills.com/manuals/fm-atp-3-21-8-infantry-rifle-platoon-squad-2024/defense/fighting-position-construction/
- Spoil and parapets - https://lonesentry.com/articles/fieldworks/index.html ; FM 7-10 - https://ibiblio.org/hyperwar/USA/ref/FM/FM7-10/FM7-10-I.html ; https://thetacticalhermit.substack.com/p/small-unit-tactics-building-a-fighting
- Cu Chi tunnels (UNESCO) - https://whc.unesco.org/en/tentativelists/7038/ ; WWI dugouts - https://warhistory.org/zh/article/wwi-technology-logistics-and-tactics-an-overview-i
- Rut depth: Vennik et al. 2019 - https://sciencedirect.com/science/article/pii/S0167198718308778 ; multiple passes - https://pmc.ncbi.nlm.nih.gov/articles/PMC12311209/
- Chrono SCM terrain sync (modified nodes only) - https://api.projectchrono.org/terrain_synchronization.html
- Floating voxel chunks / flood fill - https://www.gamedev.net/forums/topic/588921-problem-with-floating-chunks-in-voxel-terrain/
- Reforger penetration model - https://community.bistudio.com/wiki/Arma_Reforger:Weapon_Creation/Prefab_Configuration
- Reforger SA vehicles - https://reforger.armaplatform.com/news/modding-update-july-14 ; Linux server - https://community.bistudio.com/wiki/Arma_Reforger:Server_Hosting
