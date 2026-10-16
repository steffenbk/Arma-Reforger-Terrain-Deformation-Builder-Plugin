# Deep dive: is the ground design right, and what is untested around it (2026-10-16)

User: "Deep dive on this: the setup we have now (heightfield base, 2D rut field, small voxel blocks only where
something was dug) is the right overall design. The biggest open problems are untested gaps around it."
Follows research/2026-10-16-ground-options-full-review.md (yesterday's claim). Code read at f7715c3 (= 6bb6738 code).
Labels: CODE (file:line read today), MEASURED (number with its source; "offline" = Python with the game formulas),
PROVEN (earlier doc), INFERENCE (my reasoning, not tested), USER. Nothing in Scripts/ was changed.

## 1. Verdict after the deep dive

**The SHAPE is right. But "only untested gaps around it" was too kind: three problems sit inside the design, not
around it, and they only show with many vehicles, long matches or deep tunnels - none of which was ever tested.**

| | Claim | Holds? | Why (section) |
|---|---|---|---|
| A | 2D for single-valued ground, 3D only where dug | YES | One representation per spot, one hand-over (carved cells), no second mesh. Every measured dead end (sheet, look/feel split, 0.25 m voxels, DC ruts) was a breach of this rule (2) |
| B | Rut field = the right rut answer | YES for look/feel/MP-1-vehicle | Robot 7 snaps / 7 runs vs 40 / 8, hashes identical (PROVEN rut-field-stage5) |
| C | Its cost scales | **NO** | Every client builds every rutted patch on the whole map (no distance gate), through the band mesher (3-4x the grid cost), at most 1 collider per frame per machine. A dedicated server saturates at about 3 driving vehicles (3.1) |
| D | MP state = the op log | **NO for long matches** | JIP replays every rut op ever made. ~31 MB of op text per hour with 10 vehicles, all sent in ONE server frame (3.2) |
| E | Tunnels fit under FLOOR 3 m | **NO** | A full-height tunnel (2.5 m capsule) can never have more than ~0.8 m of roof on flat ground. Thin roofs are built in, not a digging mistake (3.5) |
| F | The rest | Gaps to test | bullets (3.3), crater shape (3.4, MEASURED offline), building lock (3.6), objects over holes (3.7), 1.9 vehicle class (3.8), column cap fallback (3.9), determinism coverage (3.10) |

Correction to yesterday: finding N5 (voxel house desyncs in MP) is WITHDRAWN - MUD_VoxelHouse.Tick / Blast return
when `RplSession.Mode() != RplMode.None` (CODE MUD_VoxelHouse.c:105, :125). It is simply off in MP.

## 2. Why the shape is right (the part that holds)

| Layer | Owns | Resolution | Why this representation | Evidence |
|---|---|---|---|---|
| Holed terrain + coarse skin | everything undug | 2 m (exact heightmap triangles) | the only way to "replace" engine terrain: no runtime terrain edit exists | PROVEN mud-gotchas ~26, ~86-89 |
| Fine patches (heightfield) | single-valued ground near action | 0.25 m | cheapest collider per detail, already whole-map sparse, server-side physics | MEASURED CreateTriMesh 2048 tris 0.28 ms |
| Rut field (2D sparse floor) | ruts on any ground | 0.25 m | a rut is single-valued and shallow; 2D is what every rut system uses; read by BOTH grounds, so a rut crossing into a crater has no hand-over | PROVEN stage 1-5 |
| Voxel bricks (4 m, 0.5 m samples) | craters, trenches, tunnels, fill | 0.5 m | the only option for overhangs; joins tunnels to craters with no seam | PROVEN 2026-10-13..15 |

The rule behind it: **one drawn + felt surface per spot, built from one data set.** Every failure in the project's
history broke that rule: heightfield-vs-voxel hand-over (10-13, "ruts vanish"), look field vs feel field (10-13/14,
41.6 vs 20.0 ms/s, 3 corrections vs 0), mud sheet (10-15, DS stutter + 21-52 ms mortar frames). The current design
has exactly one hand-over: a carved 2 m skin cell goes VOX, the voxel mesh sits LIFT 2 cm over the heightfield on
the cell line (CODE MUD_VoxelProbe.c header). Measured since: 0 holes / 0 cracks in the displaced-ruts hole check.

What would make me change my mind: if crater/trench LOOK becomes the top complaint, the next step is still not a new
representation - it is displacing crater rims / trench lips from an analytic 2D field the way ruts are displaced
(full review C4 / T5). If tunnels become central, FLOOR has to move (3.5) - also not a new representation.

## 3. The gaps, traced through the code

### 3.1 Rut field cost does not scale with vehicles  [design-level, untested with > 1 vehicle]
What the code does:
- A rut op (kind 4) on EVERY machine calls `MUD_WorldGroundEntity.FloorDirty` -> `EnsureRange(..., true)` ->
  `EnsurePatch` + `MarkFloorDirty` (CODE MUD_WorldGround.c:3891-3920). No camera-distance check. The old height
  messages had one (`FarFromCamera` -> `HeightsOnly`, CODE :6999, m_fClientBuildRadius 300 m) - the rut field path
  skipped it. So every client makes and rebuilds every rutted patch on the map, for every vehicle.
- Rebuild does at most ONE mesh and ONE collider per frame for the whole map (CODE Rebuild, `mp` / `cp` picks),
  gated by MayRun with COST_MESH 1 / COST_COLLIDER 4 ms (CODE :113-115) - estimates from the GRID era.
- The prefab runs the band mesher (`m_bBandMesher 1`, Prefabs/Mud/MudWorldGround.et). MEASURED 2026-10-13
  (results/2026-10-13-sdf-phase2.md): band mesh 2.9-4.7 ms vs grid 0.9-1.1; band collider 4.2-5.5 vs grid 1.4-1.6.
  Rut field stage 4 LOG: ~10 mesh + ~10 collider rebuilds per second for ONE vehicle at 6-7 ms each; LOG 23:16:
  ~20 ms per rut op, mesher run twice per patch (53 %).
- The band mesher was built for heightfield trench / crater walls (layered SDF, 2026-10-13). Since v031/v035 those
  are voxel; the heightfield only takes ruts now. No doc says why it stayed on. The grid path draws the rut field
  with the same soil rule (CODE BuildMesh :5832-5845 FIELD_SOIL_D, BuildCollider :5959 floor + lock) - stage 3 notes
  "grid fallback mesher draws the floor too".

What follows (INFERENCE from those numbers):
- One vehicle needs ~10 collider builds/s. A DS runs ~30 fps (PROVEN MUD_ServerLog 2026-10-11) -> at most 30
  colliders/s, fewer once the 4 ms frame budget is spent. **About 3 driving vehicles saturate a DS.** Past that the
  dirty queue grows; server colliders lag the op stream, while clients (60-100 fps) catch up faster - server and
  client wheels meet different ground under convoys = corrections. Exactly the class of stutter the project fought.
- On a client: 10 vehicles x ~10 builds/s x ~6 ms = more work than the 1-per-frame scheduler can do; ruts appear
  seconds-to-minutes late everywhere, and the MayRun gate lets 3-7 ms band jobs into frames it costed as 1-4 ms.

Tests (one session, local DS + 2 peers, robot + 2-4 GM-spawned AI or player trucks driving off-road):
1. Add one 10 s log line on each machine: `m_aDirty.Count()`, oldest dirty age, builds done / s. PASS = the queue
   drains (not growing over 60 s). The same line answers how many vehicles a DS carries.
2. A/B `m_bBandMesher` 1 vs 0 on the same drive: rut look (user), build ms, queue length, robot snaps (>= 6 runs).

Fixes, cheapest first:
- **Band mesher off** (prefab `m_bBandMesher 0`): expected ~3x less rebuild time, no code. Look check first: the band
  gave nicer kept-cell edges / skirts; the grid has the edge lock instead.
- **Distance gate for the rut field on clients**: in FloorDirty, a proxy patch farther than m_fClientBuildRadius only
  gets `m_bFloor` + a "far due" flag (the m_aFarDue machinery, CODE :592 / :7069) and builds when the camera comes
  near. The floor data is already in the field (no net needed) - the gate is purely local and safe.
- Server: build colliders only within N m of any vehicle / player (the DS has no camera); far rutted patches can stay
  on their last collider (nobody stands there) and rebuild on approach (wake lead already exists).
- Then the parked "collider right after its mesh, share the band build" idea, only if the band stays.

### 3.2 JIP = replay of every op ever made  [design-level, untested past ~840 ops]
What the code does:
- `JipBatches` walks `m_aOps` (every op of the match) and `MUD_RpcAsk_VoxelJip` sends ALL batches in one loop, in one
  server frame, reliable, once per controller (CODE MUD_VoxelProbe.c:1042-1066, MUD_VoxelNet.c).
- MEASURED 2026-10-14 (results/2026-10-14-jip-size.md): 839 ops = 284573 chars (339 per op), ruts 96 % of the text;
  joined in 6.6 s, client memory +127 MB at 1182 columns. MEASURED rut rate: 262 rut ops in ~90 s for one truck
  (freeze-test "After" table) = ~2.9 ops/s per driving vehicle.

Projection (INFERENCE, linear): 10 trucks driving half the time = ~52000 rut ops / hour = **~18 MB of op text per
hour** (~31 MB if they drive all the time); ~12000-21000 reliable RPCs of 1500 chars sent in one server frame per
joining player; replay at the measured 839 ops / 6.6 s rate = 7-12 minutes of join. Untested: real-link RPC limits
(the peer tool ran with -rpl-timeout-disable, PROVEN mud-gotchas 2026-10-10), server memory for the log.

Tests: (1) a synthetic long log - a debug switch that issues N fake rut ops along a looped path (N = 5k, 20k) on a
local DS, then join; measure ask -> live, server frame during the send, joiner errors. (2) Same on a real hosted DS
(not localhost).

Fix (design, the plan's "Later" item, now required before public matches): **compaction**. Past N ops, the server
sends a snapshot instead of the old ops: per rut tile (MUD_RutTile 16 x 16 floats) the floor as mm deltas from undug,
RLE (rut tiles are mostly NONE). Size is bounded by AREA rutted, not by time: ~6000 tiles for 10 km of tracks x
~100-200 chars = ~1 MB, once. Voxel ops (craters / trenches / tunnels) stay as ops (few, and needed in order for the
field). Send batches spread over frames (N per frame, like the patch JIP's 8 per frame). Determinism stays checkable:
the joiner's per-tile hash vs the server's (DISP_HASH already hashes the floor in mm).
Also gives healing for free later (a heal = a smaller snapshot).

### 3.3 Bullets and falls under voxel ground  [untested, PvP critical]
What the code does:
- Heightfield collider = surface trimesh(es) on Terrain | Foliage + a **bullet slab** (the surface copied 1.5 m down,
  reversed, FireGeo only) + a **safety plate** 0.5 m under the floor (CODE BuildCollider :5934-6015, band :6265+).
  The slab exists because "open script trimesh on Terrain preset lets bullets pass (rim -> next crater)" (PROVEN
  2026-10-08 09:26) and it fixed it ("bullet got stopped", PROVEN results/2026-10-09-plan3-task6-craters.md).
- A carved (VOX) cell gets slot -1 in FillIndices, and slab + plate are built only for cells with a slot (CODE
  :5652-5654, :5771-5783; band path the same via the band mesh). The coarse skin's VOX cells get no collider either.
- Voxel bricks build only the surface (Terrain | Foliage, CODE MUD_VoxelProbe.c MakeBody :5372).
=> **Under every crater, trench and tunnel there is no bullet slab and no safety plate.** The ground that needs cover
most is exactly where the proven fix is missing. GroundRescue also skips VOX cells (overlay brief). A crack in a
brick collider = an endless fall (the terrain is holed there).

Why it is not certain bullets pass (INFERENCE): a crater rim seen from outside has a real exit face (the inner wall),
so it MAY stop rounds that the open heightfield did not. But the 2026-10-08 failure was the same geometry (rim ->
next crater) on an open trimesh, so expect the same until shown otherwise.

Tests (SP, 15 min): AI or a dummy soldier (a) prone in a mortar crater, shooter 50 m out aiming low over the rim;
(b) standing in a 1.5 m trench, shooter aiming at the parapet; (c) shots straight down into flat voxel ground (does
the round vanish under the map?). Diag: Physics > Show bodies + collider type trimesh, to see which body is hit.
Also check fragments: GroundCovers already counts bricks (CODE MUD_WorldGround.c:5136), but log 'MUD cover:' lines in a voxel crater.

Fix design (only if (a)-(c) fail):
- Slab: in MakeBody, a 3rd geom = the TOP section's triangles moved down along y and reversed, FireGeo only. Move each
  vertex down by min(1.5 m, solid thickness under it) - read the column's samples (Value) so a slab never hangs inside
  a tunnel (else it would stop bullets fired IN the tunnel). Cost: +1 CreateTriMesh per brick (~0.06-0.3 ms, PROVEN
  sizes), no net, deterministic (built from the field).
- Plate: one flat 2-triangle trimesh per converted column at undug - FLOOR - 0.5, Terrain layer (catches falls);
  could ride on the lowest brick's body. ~0 cost.

### 3.4 Crater shape regressed when craters went voxel  [MEASURED offline]
What the code does: `Crater()` sends only `full * scale` to MUD_VoxelProbe.Blast (CODE MUD_WorldGround.c :5380,
:5427) before the depth jitter, oval / lobe outline, stack cap and the 42 deg slope rule (m_fCraterMaxSlope) run.
Voxel Carve = half ellipsoid with crest 2.2 x depth + funnel + sine rim (CODE MUD_VoxelProbe.c:2657).

MEASURED offline (results/2026-10-16-voxel-crater-slope.md, Tools/crater_slope_voxel.py, game formula + game mesher):
| Charge | Depth | Today: area > 50 deg | Today: steepest | Today: steep climb on the EASIEST way out | Cosine bowl 42: steepest / steep climb |
|---|---|---|---|---|---|
| 81 mm mortar | 1.43 m | 14.4 % | 67 deg | 0.21 m (median way 0.52 m) | 44 deg / 0 |
| Hydra | 2.04 m | 14.9 % | 72 deg | 0.43 m | 44 deg / 0 |
| TM-62M | 2.81 m | 17.8 % | 74 deg | 0.82 m | 43 deg / 0 |
| HEAT / 82 mm | 0.6-0.86 m | 0 % | 35-43 deg | 0 | 43-47 deg / 0 |
Soldier MaxSlope is 50 deg (IDA/pak 2026-10-10). The cosine bowl also shrinks the footprint (mortar outer radius
4.58 -> 3.49 m): ~31 -> ~25 columns per mortar crater, MAX_COLUMNS lasts ~195 -> ~240 mortar craters.

Also found: a blast UNDER a roof (an RPG or charge inside a tunnel) cuts straight up to the sky - Carve's funnel above
the hit (`up > 0`) is air for every rr < crest + 0.5 x up, up to wp + FLOOR + rim + 1.5 m (CODE :2692-2712), and
Crater()'s voxel branch only rejects air bursts ABOVE the undug ground (CODE MUD_WorldGround.c:5369-5381).

Tests: walk out of a mortar / Hydra crater (SP, 2 min each); fire an RPG into a tunnel and look up.
Fix design: Carve gets the heightfield profile as a vertical-distance field (offline-proven shape above), with
jitter / lobes chosen by the SERVER and sent in the op (kind 0 leaves m_fB and the plan field free; e.g. plan =
"jit,lobes,phase" in mm) - deterministic. Stack rule: the server already knows the depth at the hit (ColumnDepth) and
can cap `depth` before Issue. Blast under a roof (solid sample above the hit within ~2 m, checked inside Carve on the
field - same on every machine): a sphere of r = depth instead of bowl + funnel.

### 3.5 Tunnels: FLOOR 3 m makes thin roofs unavoidable  [design-level]
What the code does: CarveTunnel never digs below `undug - FLOOR` (CODE :3261-3270, FLOOR 3 at :304). The capsule is
r 0.8, half 0.45, middle 0.95 over the feet (CODE :313-316): it spans feet - 0.3 .. feet + 2.2 (2.5 m).
- Standing on the deepest floor (undug - 3), the capsule top is at undug - 0.8. **On flat ground a full-height
  tunnel has at most ~0.8 m of roof**, ~1.6 samples at 0.5 m - the "paper roof / thin bridge" the user saw is the
  normal case, not a digging mistake. Pitching the dig down lowers the roof line only by making the tunnel lower.
- The stability research (2026-10-15) proposes "roof < 1.0 m collapses" (R1). With FLOOR 3 that rule would collapse
  EVERY tunnel. FLOOR must move first.
- Depth boxes (MUD_DepthBoxEntity, the heightfield's per-area floor, CODE ApplyFloor :2958) are not read by voxels.
- T needs no tool (server checks: on foot, 3 m, rate - MUD_VoxelNet.c); the shovel needs a shovel. Design call.
Fix design: FLOOR per op kind (tunnel 6-8 m, craters / trenches stay 3) or per column from DepthBoxEntity.DepthAt at
Convert time (placed entities: same on every machine). Rows grow with depth (more bricks under tunnels only). Then
the dig-time roof guard (server) and R1/R2 from research/2026-10-15-voxel-stability-and-optimisation.md make sense.

### 3.6 Voxel ops ignore the building lock  [untested]
What the code does: the heightfield never moves a vertex inside a building's box (+ margin) or a MUD_CutBoxEntity
(CODE BuildingLock :2907-2950). Voxel Carve / CarveTunnel / CutTrench / FillTrench only skip `m_aHard` (kept terrain
from the skin hole map, CODE MUD_VoxelProbe.c :2684 / :3257 / :3315 / :3382). Buildings stand on skin over hole cells
(design spec: "buildings do not keep terrain"), so the ground under them is diggable.
Determinism trap for the fix: the heightfield lock reads live entities per machine, which was fine because heights
are SENT. Voxel ops are REPLAYED, and buildings can be destroyed (or placed by the GM) at different times on a
joiner -> a per-machine lock would fork the field.
Fix design: the server decides and the op carries it - an extra op field (Decode accepts the old count, as when the
seed field came) with the building boxes the op's box touches (rotated rectangles in cm; a crater near one house =
~30 chars). Every machine treats samples inside them like `m_aHard`. JIP replays the same text.
Test: mortar 2 m from a house wall; shovel trench up to a wall; tunnel under a house.

### 3.7 Nothing wakes or moves objects over a new hole  [untested]
What the code does: no voxel or rut path queries or wakes entities after a go-live (grep: SetActive only in
MUD_VanillaHoleFix.MUD_WakePlaced, which wakes GM-placed dynamic bodies one frame after spawn - 2026-10-12, written
for the "GM vehicle hangs in the air" report; the master list still lists that bug as open, so it is not confirmed
fixed). A parked vehicle keeps its heightfield patches awake (any vehicle within 16 m, CODE MUD_WorldGround.c:3662), so rut patches
under it never sleep - fine. But a crater / trench under a sleeping dynamic body, a static prop, a tree, a mine, a
GM-built sandbag is not handled.
Fix design (after GoLive, over the changed box):
- server: dynamic bodies (vehicles, dropped items, bodies) -> `SetActive(ActiveState.ACTIVE)` (they fall; replication
  carries it). Placed mines -> detonate or delete (design call).
- every machine, deterministic (static world entities are the same everywhere): a static prop / tree / bush whose
  base is now > 0.3 m above the new surface -> hide + collision off locally (the MUD_VoxelHouse hide pattern), or move
  it down. GM-placed replicated statics -> server deletes.
Test: crater under a parked truck, a dropped rifle, a tree, an AT mine, a GM sandbag wall; trench next to a fence.

### 3.8 One line decides whether ruts exist on 1.9 vehicles  [check, minutes]
Every wheel feature (ruts, ahead, belly scrape, ramp, ditch escape) runs only for vehicles registered by
`MUD_Wheels.Register`, which requires `FindComponent(VehicleWheeledSimulation)` (CODE MUD_Wheels.c:24); the world
ground casts to it again (CODE MUD_WorldGround.c:1160, :1336). IDA 2026-10-15 found the 1.9 server-authority class
named VehicleWheeledSimulation_SA_B. If it is not a subclass, everything stops silently on SA vehicles.
Check: spawn a 1.9 truck, look for `MUD wheels: <id> registered` in the log. Tracked vehicles never register.

### 3.9 Past MAX_COLUMNS the old heightfield crater comes back  [untested]
Issue refuses past 6000 columns (CODE :1924-1928); Blast returns false and Crater() falls through to the heightfield
crater (CODE MUD_WorldGround.c:5427 onward) - the 2-system world of 2026-10-13 returns late in a long match:
different crater shape, m_aH + height messages, and a later voxel op there seeds from dug-undug.
MEASURED basis for when: ~195 mortar craters (offline column count) or less with trenches / tunnels; client memory
~110 KB per column (jip-size: +127 MB at 1182 columns) -> ~0.65 GB at the cap.
Test: MAX_COLUMNS 40, shell one area, then shell next to it; look at both kinds side by side, drive across.
Decide: refuse craters past the cap (no crater) vs heightfield craters vs a bigger cap (memory).

### 3.10 What the MP proofs do and do not cover  [coverage]
- DISP_HASH hashes the RUT FLOOR in mm (CODE :2999, :3201). The heightfield HashLines hash patch m_aH. Nothing hashes
  the voxel DENSITY field (craters / trenches / tunnels / fill) across machines - its equality was only seen by eye.
- Every MP run was Windows (WB host or local DS + PeerPlugin on one PC). Hosted Reforger servers often run the Linux
  DS. Different maths libraries can differ in the last bit of Sqrt / Sin (Carve uses both). Nudge (|d| >= 5 mm) makes
  sign flips unlikely, and the floor hash rounds to mm, so INFERENCE: low risk - but unmeasured.
- Rut field JIP (late join) - still open (ruts README).
- No run with more than one driving vehicle (Lane 1 item 6).
Test: add a field hash per op for kinds 0-3 (sum of Round(d x 1000) over the samples Put changed, as DISP_HASH) and
compare server vs client; then one session on a Linux DS.

## 4. One test session plan (everything above that needs no new feature code)

Setup: checkpoint first. SP Workbench, then local DS + 2 peers. Robot code back from the archive for snaps.
| # | Do | Read | Pass |
|---|---|---|---|
| 1 | Start a 1.9 truck | `MUD wheels: ... registered` | line present (3.8) |
| 2 | Shoot into a voxel crater rim, trench parapet, flat voxel ground | hits / Diag bodies | rounds stop (3.3) |
| 3 | Walk out of a mortar and a Hydra crater, no jump | user | walks out (3.4) |
| 4 | RPG into a tunnel; mortar 2 m from a house; tunnel under a house | look | no sky hole / no floating house (3.4, 3.6) |
| 5 | Crater under a parked truck, rifle, tree, AT mine, GM sandbags | look | nothing floats (3.7) |
| 6 | Deepest tunnel: measure roof (trace up from the floor) | log / Diag | expect ~0.8 m (3.5) |
| 7 | DS + 2 peers, 3-4 trucks off-road 5 min, band mesher on then off | new dirty-queue line, robot snaps, user look | queue drains; snaps not worse (3.1) |
| 8 | Late join after 7 | ask -> live ms, server worst frame | joins, hashes match (3.2, 3.10) |
| 9 | MAX_COLUMNS 40, shell two areas | look | decide (3.9) |
Small temporary code for the session: the dirty-queue log line (3.1), a field hash per voxel op (3.10), a fake
long rut log switch (3.2). Archive them after (CLAUDE.md rule).

## 5. Fix order (user decides; feature freeze still on - these protect shipped features)
1. Band mesher off if the look holds (prefab value) + the client distance gate for the rut field (3.1) - before any
   multi-vehicle test means anything.
2. Bullet slab + safety plate under voxels (3.3) if session test 2 fails - PvP cover is the feature.
3. Crater profile in Carve + blast-under-roof sphere (3.4) - one function, offline-proven shape.
4. Building lock in the op (3.6) and wake / hide after go-live (3.7).
5. JIP compaction (3.2) - required before public matches longer than ~30 min.
6. FLOOR per kind for tunnels (3.5), then the tunnel roof guard + collapse rules.
7. Cap policy (3.9) and field hash + Linux DS run (3.10).
