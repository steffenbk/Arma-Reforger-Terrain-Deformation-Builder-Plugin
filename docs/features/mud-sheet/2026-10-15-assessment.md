# Mud sheet - full assessment (2026-10-15 ~17:40)

User question: why is the mud sheet so hard to get working, could it ever cover the whole map, and is there
something with voxels that would have worked better - was the sheet the wrong idea?

Labels: CODE (read in our script today), LOG (our logs, path given), MEASURED (numbers worked out from logs here),
PROVEN (earlier doc that proved it), USER, INFERENCE (my reasoning, not tested).

## 1. Short answer

- **Whole map: no, not as built.** Pre-built 3 m tiles for all of Arland need ~1.87 million tiles, ~340 GB by our
  measured cost per tile, ~6 hours to build at our measured rate (section 3). Even 1 % of the map is ~3.4 GB.
- **Why it was hard:** not mainly bugs. Three design choices made it fight the rest of the mod (section 2):
  1. it is a **second ground mesh** on top of the voxel ground, so every edit edge needs a stitch rule;
  2. it is **client-only**, but ONE_GRID made it the **physical ground** - in MP the server drives on different ground;
  3. **every voxel op had to be done twice** (voxels + sheet), in the same frame.
- **Was the sheet wrong?** The idea (a MudRunner-style mud layer) was fine as a *look-only trail near the truck*.
  It went wrong when it became pre-built zones and then the real ground. MudRunner itself does not have a second
  mud mesh: its mud IS the ground heightfield, drawn by the GPU (section 4).
- **What would have worked with voxels:** keep ONE ground (voxels + heightfield, which already covers the whole map
  sparsely) and make mud **numbers on that ground**, not a mesh: a mud depth per spot (from the terrain's own
  ground material, so no zones are needed), wheel sink + drag from that depth (MudRunner's model), and the look from
  voxel ruts with side ridges + a mud material on wet bricks (section 5).

## 2. Why it was hard - the evidence

### 2.1 Two meshes must look like one ground (inherent)
- CODE: the voxel mesh is surface nets on 0.5 m samples; the sheet is a 0.15 m height grid that reads the same ground
  by tracing / sampling it. Two different rebuilds of one shape never match exactly.
- PROVEN (docs/superpowers/research/2026-10-15-ground-architecture-review.md 2.2): gap of the voxel mesh to the true
  shape rms 0.03-0.04 m, max 0.12-0.29 m at ruts / craters. That is the size of the steps, floating edges and
  "layered like a cake" (USER) we kept seeing.
- Every fix moved the seam: BLAST_NOW, RIM_BARE, TAPER (then TAPER across tiles), DIG_NOW, HOLE_SKIP, BARE_VOX holes,
  ONE_SKIRT curtains, SYNC hold, m_bSheetOp carve skip, DrawsAt. The review said "third time we hit this wall"
  (heightfield vs voxel 10-13, look field vs feel field 10-13/14, voxel vs sheet 10-15).
- CODE (counted today): MUD_MudSheet.c is 4053 lines with 98 constants, 19 of them on/off switches. It is now the 3rd
  biggest file after MUD_WorldGround.c (7343) and MUD_VoxelProbe.c (4222), built in ~20.5 hours
  (checkpoints v046 2026-10-14 20:38 -> v083 2026-10-15 17:02 = 37 checkpoints).

### 2.2 Client-only look turned into the physical ground (the DS stutter, likely)
- CODE MUD_MudSheet.c line 361: `if (!ENABLED || RplSession.Mode() == RplMode.Dedicated) return;` - a dedicated
  server never runs the sheet.
- CODE line 160-161 (ONE_GRID comment): "crater (0) / rut (4) ops in a zone change the sheet's ground (TakeOp), no
  voxels. **SP only (the sheet is client-only).**" ONE_GRID is still `true`.
- CODE line 3914-3918: with ONE_GRID the sheet's collider is on Terrain | Foliage "as our patches (wheels, feet,
  bullets)", and the zone's own patch mesh + collider are switched off on the client (HideZoneGround).
- So on a DS: the **server** drives the truck on our heightfield + voxel ruts (0.5 m), the **client** predicts it on
  the sheet ground with the sheet's own rut shape (0.15 m, cosine profile ONE_W0, CODE line 168-170). Two different
  grounds under one vehicle -> server corrections -> jitter.
- In a WB host test the host IS the server and runs the sheet, so its own truck saw one ground. That would explain why
  the stutter only showed on the DS. USER 17:09: sheet off -> DS stutter gone.
- Status: INFERENCE (strong, from code). Not measured. To prove it: Diag exe, Vehicles > "Vehicle Networking and
  Reconciliation", count path corrections in a mud zone with ONE_GRID true vs false on a DS.

### 2.3 Every op done twice, in one frame (stutter)
- MEASURED (ground-architecture-review 2.1, log logs_2026-10-15_11-54-08): voxel go-live frame 3 ms with no mud tile in
  reach (control), 41-55 ms with mud tiles - the sheet's "dig now" was 21-48 ms of that.
- The later fix spread held ops over frames (PEND_STEP_MS), but then the two layers disagree for those frames - the
  "loads weirdly, not instant" (USER) that DIG_NOW was added to stop. A trade-off with no good end.

### 2.4 The script cost grew while it was being fixed
LOG logs_2026-10-15_16-23-36/console.log, 'MUD mud sheet PERF' 5 s windows while driving (speed > 1 m/s):

| window end | fps | speed max m/s | our ms/frame | drawn quads |
|---|---|---|---|---|
| 16:24:16 | 54.6 | 5.1 | 0.59 | 3294 |
| 16:24:21 | 45.6 | 4.8 | 2.73 | 480 |
| 16:24:27 | 65.1 | 1.6 | 0.52 | 697 |
| 16:34:53 | 58.5 | 4.4 | 0.10 | 0 |
| 16:34:58 | 39.1 | 5.3 | 5.90 | 6394 |
| 16:35:03 | 36.5 | 4.1 | 6.09 | 860 |
| 16:35:08 | 33.7 | 3.6 | 8.98 | 6872 |
| 16:55:30 | 56.4 | 6.2 | 0.66 | 6054 |
| 16:55:35 | 43.7 | 5.9 | 6.69 | 2354 |
| 16:59:09 | 50.7 | 2.5 | 1.90 | 0 |
| 16:59:14 | 45.7 | 5.3 | 3.10 | 4310 |
| 16:59:19 | 34.7 | 3.8 | 8.67 | 3282 |
| 16:59:29 | 41.4 | 4.0 | 5.73 | 4750 |

- Driving through zone ruts the sheet's own script time reached 5.7-9.0 ms per frame (at ~35-40 fps that is a quarter
  to a third of the frame). On 2026-10-14 night (results/2026-10-15-freeze-test.md) it was 2.2-3.1 ms live. The
  stitch rules (taper with 6-cell padding round every change, TaperField on each live build) roughly tripled it.
  INFERENCE for which rule costs what - not split by a timer.
- Voxel ruts with no sheet: 64.1 fps driving (freeze-test.md "After" table) vs 34-45 fps here in the zone.

### 2.5 Engine limits that every sheet fix ran into (PROVEN, mud-gotchas / ida-voxel-feasibility)
- Script meshes: one UV set clamped 0..1, no vertex colours, no own normals (normals per section, so shading breaks at
  every 3 m tile edge), no displacement, no custom shaders. MudRunner's look (blend mask, parallax tracks, mud
  offset in a texture) is all GPU and not open to us.
- Each mesh section needs its own .emat path or it crashes; DYNAMIC_VERTEXES gives broken normals -> every change is a
  full mesh rebuild; colliders have no refit -> every change is a full trimesh rebuild.
- Enforce: 80 KB stack per method (BuildTile was at ~69 KB), member bool arrays give garbage, int-only operators in
  float expressions refused. Each cost time on the sheet.

## 3. Could it ever cover the whole map?

Map size: CODE MUD_WorldGround.c line 779 "Arland 2048 / 4 = 512" patches; LOG "patch grid 512 x 512" with 8 m patches
-> 4096 x 4096 m.

| | number | how |
|---|---|---|
| tiles (3 m) on the map | 1,865,956 | ceil(4096 / 3) = 1366 per side, squared |
| one 64 m zone | 462 tiles = 0.025 % of the map | LOG 'MUD mud zones: all 462 tiles built' |
| zone build time | mean 5.55 s (runs 4.11, 5.89, 5.34, 6.41, 5.58, 5.30, 6.18) = 83 tiles/s | LOG zone start -> 'all built', 7 runs 16:24-16:59 |
| whole map build | ~6.2 hours | 1,865,956 / 83 |
| stored data alone | 10.6 KB per tile -> 19.7 GB | CODE MUD_MudSheetTileData: 6 arrays x 441 x 4 bytes |
| measured memory | ~178 KB per tile -> ~340 GB | first 'mem + KB since ready' line per run: sheet on 156073, 184272, 164511, 137158 (mean 160504 KB); sheet off 85872, 86530, 61997 (mean 78133 KB); 82371 KB / 462 tiles |
| 1 % of the map | 18,660 tiles, ~3.4 GB | same rates |

The memory per tile is rough (MEASURED but noisy: different 10 s windows, also counts the player's first patches).
Even at the data-only floor (19.7 GB) the whole map does not fit. Plus ~1.87 million mesh entities + colliders,
which the engine was never shown to handle (INFERENCE).

What CAN scale: the first sheet version (v047-v054) - a 12 m live window under the truck plus a capped trail of touched
tiles. That is sparse (only where wheels went), like our voxel bricks (sparse 4 m columns where ops are). But that
version is a look-only layer with no feel, and the trail still had to meet the voxel ground at its edges.

## 4. How MudRunner does it, and where we differ

READ (docs/superpowers/research/2026-10-14-mudrunner-mud-article.md, -mudrunner-ida.md):
- The mud is the ground heightfield itself (a texture per 16 m block, 0.125 m, height relative to the terrain). It
  replaces the terrain where mud is - one surface, no second mesh on top.
- Deforming = drawing min / max shapes into that texture on the GPU. The look (mud vs terrain blend, dragged texture,
  parallax wheel tracks) is shader work.
- Only blocks near the player are simulated; the rest is just a stored texture (cheap on the GPU, whole map).
- Wheels have no mud collider: they sit on hard ground, mud adds drag, lift, spin resistance and a paddle push from the
  mud DEPTH under the wheel (PROVEN-IDA).

We copied the wrong half: the visible layer (which needs the GPU) instead of the cheap half (depth -> forces). Our
real "MudRunner layer 1" already exists: the heightfield + voxel ground, which replaces the terrain and can go below it.

## 5. What would have worked (with voxels), ranked

### 5.1 Mud as numbers on the one ground we have (recommended direction)
- **Where mud is: from the terrain, whole map, no zones.** PROVEN (mud-gotchas, 2026-10-08): a WORLD trace returns the
  terrain gamemat (e.g. grass_lush). A table "gamemat -> base mud depth" gives every spot a mud depth with no stored
  data and no net traffic (every machine computes the same). Zones can stay as overrides.
- **Pressed mud: in the voxel op stream we already have.** Ruts are voxel ops (v035, deterministic, MP + JIP proven, 96 %
  of JIP ops). Each rut can lower the mud depth left at that spot (one small number per 0.5 m sample or per 2 m cell,
  stored only where changed - sparse, like the bricks).
- **Feel: forces, not geometry.** Wheel sink by WheelSetRadiusState (PROVEN-IDA: the runtime radius lowers the body,
  docs/superpowers/research/2026-10-15-sinking-deep-dive.md) + drag / spin resistance from depth (MudRunner formulas,
  2026-10-14-mudrunner-ida.md). Server-authoritative, a few bytes per wheel, works on the whole map. Probe 1 in the
  sinking deep dive was never run.
- **Look: on the voxel mesh, not over it.** Voxel ruts with raised side ridges (master list "FIRST IN LINE", research
  2026-10-15-raised-ruts-ideas.md); a mud material as a 2nd section on bricks where depth > 0 (own .emat path - PROVEN
  rule); vanilla track decals as an optional extra (unproven on our ground).
- Cost: INFERENCE low - no new mesh, no new collider, ops already exist. Ruts stay at 0.5 m look (measured limit:
  0.25 m cells cost 3.4x, DC no-go).

### 5.2 One fine height grid for everything single-valued, voxels only for tunnels
- The architecture review's option c / f1: the sheet's 0.15-0.25 m grid becomes THE ground for ruts / craters /
  trenches on every machine (server too), fed by the same op stream; voxels only where there is air under ground.
- Best look and feel, one surface. But it is a rewrite of the ground on the server side, and the old heightfield rut
  path cost ~13 fps (freeze-test.md). Big; against the feature freeze. INFERENCE that it could be made cheap enough.

### 5.3 Sheet as a pure look-only trail (the first version)
- Client-only, never a collider, never takes ops, only near vehicles, ages out. Scales (sparse).
- Keeps the edge seam with voxel ruts and craters (2.1) - you would see the "cake" again. Only worth it if 5.1's look
  is not enough.

### Not worth it (MEASURED dead ends)
Finer voxels for ruts (3.4x cost), dual contouring for ruts (no-go), smoothing (no visible gain), whole-map pre-built
sheet (section 3), any two-mesh look + feel split (review 2.5: 41.6 vs 20.0 ms/s, corrections 3 vs 0).

## 6. Suggested next steps (user decides; feature freeze)
1. Leave the sheet parked. If it ever comes back, set ONE_GRID false first (it is SP-only by its own comment).
2. Optional proof of 2.2 on a DS (sheet on, ONE_GRID true vs false, corrections count) - only if we resume the sheet.
3. When the freeze allows mud work: 5.1 in this order - (a) wheel sink probe (sinking deep dive probe 1), (b) gamemat ->
   mud depth table, (c) side ridges on voxel ruts, (d) mud material on wet bricks.
