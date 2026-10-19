# Mud sheet: can the script cost come down enough? Research answer (2026-10-19)

Answer to the prompt "make the mud sheet work in Arma Reforger (script cost is the blocker)". Research only, no code
changed. Repo read on branch `displaced-ruts-partial-split` (43c2ada); the Enfusion API dump in
`steffenbk/enfusion-mcp-BK` (`data/api/*.json`) was searched for classes and methods; plus a web pass.

Labels: **[repo]** = from our repo (log, code, IDA note, with the file), **[doc]** = documented by Bohemia or the
source, **[code]** = seen in public code / the API dump, **[inf]** = my inference, not measured.
"ms/frame" = client frame on the DS-connected PeerPlugin1 client unless stated (the repo's numbers; a WB-started peer
client - fine for A/B on one machine, per TESTING RULES not a final perf number).

---

## 0. Read this first: five premises in the prompt that our own logs already overturn

1. **"Render +3 ms comes from the sheet" is refuted.** Hiding 354-742 stored sheet meshes saved ~0.2 ms render,
   inside noise; render rises from ~1.6 to ~3-4.5 ms when *driving* both in and out of the zone
   [repo: research/2026-10-18-sheet-alternatives-and-perf.md sec 7-8]. All-offroad vs zone: render 4.3 vs 3.8 ms
   [repo: all-offroad deep dive 2.1]. So prompt item 3's "cut the render cost of 300-460 two-material entities"
   targets a non-problem. Do not merge tiles for render.
2. **"140 -> 90 fps with the sheet" is not a sheet number.** 140 is standing. Driving OFF the zone (no sheet) was
   81-100 fps with MUD script 2.4-3.5 ms; with client look ruts off: off zone 94.5 fps / 1.26 ms, in zone
   101.4 fps / 2.13 ms [repo: sheet-alternatives sec 8, 10]. In the 64 m zone the sheet cost about what the
   heightfield look ruts cost outside it. The measured blockers are narrower (section 1).
3. **Engine track decals are a proven dead end on our ground.** `TrackDecalFactory` only starts a decal when the
   contact entity IS the world terrain entity and clips against the heightmap with no hole test [repo: IDA,
   research/2026-10-15-ida-rut-questions.md answers 1a + 13]; WB probe 2026-10-18 20:32: `AddPoint` on our patch
   returned false, no active decal [repo: mud-gotchas]. `World.CreateTrackDecal` is gone in 1.9 [repo: same doc
   5b] - note the MCP API dump still lists `BaseWorld.CreateTrackDecal` and the `TrackDecal` class [code:
   enfusion-classes.json]; that dump is older than 1.9. Vanilla `DecalEntity` hangs under the map on hole cells
   [repo: gotchas 2026-10-12]. Option B in sheet-alternatives (and item 2's "TrackDecal ribbons") should be struck.
   Only `World.CreateDecal` draws on our MeshObjects (judge by eye, it returns null even on success), and per IDA
   the decal stays on the old mesh when the entity gets a new one [repo: gotchas 2026-10-18 20:50; IDA 1b, inf].
4. **"Same results" is a stricter rule than determinism needs.** Determinism needs the same code and the same inputs
   on every machine, not the same output as today's code. P1 was built bit-identical to today; that was a choice.
   Any formula change in MUD_MudSync is MP-safe as long as every machine runs the new one (re-baseline the hash
   compare). This opens most of section 3.
5. **The sheet is look-only by design** (no collider, hybrid plan sec 2 [repo]). Yet its layer is kept bit-exact on
   every client AND on the DS, quantized, hashed and snapshotted. That is the most expensive part of the design to
   keep, and nothing in gameplay reads it (feel = the rut field). See section 2 - it is a decision for you, not a fix.

---

## 1. What is actually left to cut (last measured state)

| part (ms/frame unless stated) | value | where measured [repo] |
|---|---|---|
| All-offroad approach tile builds (`sh zones`) | ~2.9 before SPARSE, ~0.03 after | deep dive 2.1; fixes-2-4 sec 7 |
| Live window upload (`sh upload`) | 0.4-0.84 (1.2-2.0 before D1) | sheet-alternatives sec 15; fixes-2-4 sec 7 |
| Stored tile rebuilds (`sh stored`) | 0.68 (zone run), ~24 rebuilds/s | deep dive 2.1, 5 |
| Sync layer per 5 s, ONE vehicle: stamps / slump / sheet restore / drag limit | ~60 / ~70 / ~50 / 14-72 ms (after P1) | sheet-alternatives sec 7 |
| DS sync layer (only for joiner snapshots) | 44-502 ms per 5 s = up to ~3.3 ms/frame at 30 fps | deep dive B6 |
| One tile build (fast path) | 0.88 ms DS (old 1.10); quad loop 0.5, setup 0.2, engine 0.12 | mud-sheet README lesson 1; fixes-2-4 sec 7 |
| Engine share of a mesh build | 6-11 % | perf-research-ida-web Q1 |
| Last DS run with SPARSE + D (16:16) | fps 60-93, MUD 2.6-7.4 ms | mesh-cost findings, DS 16:16 |

Two costs nobody has measured yet, and they decide the whole-map question:
- **Vehicle count.** `MUD_MudSync.RutOp` queues every kind-4 op on every client and the DS; with ALL_OFFROAD
  `BoxNearZone` always returns true [repo: MUD_MudSync.c 171-185, 728-746]. So sync work scales with *all* vehicles
  driving offroad anywhere on the map, not with what the player sees. One vehicle ~180 ms per 5 s (stamps + slump +
  restore) at ~95 fps = ~0.38 ms/frame [inf from sec 7 numbers]; 8 trucks = ~3 ms/frame on every client [inf, linear
  guess]. The restore part is only for ops inside the drawn area, so the real slope is lower - unknown.
- **Memory over a match.** Sync tiles are never freed: ~135 KB/s per truck on fresh ground, ~0.5 GB/h [repo: deep
  dive 4.2, inf]. Any whole-map sheet needs a bound.

---

## 2. The design lever (decision, not a fix): bit-exact vs "same inputs, close result"

What bit-exactness buys: every player sees the identical 0-0.28 m of mud on top of the shared felt rut.
What it costs [repo]: map-wide processing on every client (above), the DS layer (B6), JOB_QUANT, hash lines, the
"M|S|i" snapshot parts, J1/J2 special cases, and drag (texture smear, pure look) inside the hash.

If relaxed to "every client applies the same ops, approximately" [inf, all of it]:
- **Lazy far ops.** Ops whose box is > R (e.g. 150 m) from the camera are stored per sync tile as the compact op
  record (segments + tag, ~350 B per op [repo: perf-assessment item 7]) and applied when the tile comes within R.
  The ground read then is today's ground, not op-time ground: a few cm of difference, look only.
- **DS layer off** (`SERVER_LAYER` false): joiners rebuild near them from the op log over several frames, or use the
  rut-derived fallback (proto shape distance 0.18 [repo: sync proto]).
- **Drag out of the synced data**: client-local arrays, not quantized / hashed / sent.
- Risk: two clients can differ by a few cm where a far op was applied late. PvP concern = prone concealment in a
  rut; the felt rut (shared) carries the real depth, so the difference is the top 0.15-0.28 m only.
- Test: none needed to decide - it is a design call. If you keep bit-exact, sections 3.2-3.3 are the only levers
  on vehicle scaling and they are each worth tens of percent, not 10x.

---

## 3. Ranked ideas (expected ms saved x chance it works in Enfusion)

### 3.1 Purge native calls from the hot loops (tile build, Stamp, Slump, GroundY) - HIGH chance, medium gain
- What: every `Math.Min/Max/Clamp/AbsFloat/Floor/Round/Sqrt/Cos/Sin`, `Vector()`, `System.GetTickCount` and
  `map.Get` is a native call through a generic bridge (call opcode + bridge + ~4 helper calls + type-checked return)
  [repo: IDA, research/2026-10-18-enforce-perf-research.md Q1]. Plain compares and `+ - *` are VM opcodes. The VM is
  a switch interpreter, no JIT [repo: same]. Only `sealed` single-return methods are inlined [doc:
  reforger.armaplatform.com modding-update-scripting-1-1, quoted in mesh-cost findings].
- Count in `MUD_MudSheet.BuildTileMesh` [repo: code read 5754-6400]: per grid vertex 1 `Vector()` + 2 `Math.Clamp`
  (UVs); the dirt-noise loop has `Math.AbsFloat` + `Math.Sqrt` per vertex; the fade loop up to 4 `Math.Min` + 1
  `Math.Clamp` per vertex; mixed quads 6 Min/Max. Rough total ~2500-3500 natives per touched tile [inf]. Paper
  test 1 only inlined `Bare()` and got -20 % [repo: README lesson 1]; the natives are still there.
  `MUD_MudSync.Stamp` per stamped cell: `Math.AbsFloat`, `TileOf` (script call), `Math.Min` x2-3, `Math.Max`,
  `Math.Cos` or `Math.Sin`, `Math.Sqrt` [repo: MUD_MudSync.c 828-1015]; 3400-9100 stamped cells per 5 s.
- How (all in-place, no design change):
  - `if (a < b) m = a; else m = b;` instead of Min/Max/Clamp; `if (d < 0) d = -d;` for abs.
  - A static template of the 441 x/z offsets, built once: `vector p = s_aXZ[lv]; p[1] = y - orgY; tv[nv] = p;`
    (component assignment on a local vector works in our code, e.g. `fwd[1] = 0` in MUD_Dig.c 93 [repo]).
  - Static base-UV table per (tile x mod, tile z mod) so the UV is one subtract; clamp only when |drag| can push past
    0/1 (cells within DRAG_MAX of the texture edge).
  - Lookup tables for the shape curves in Stamp (`0.5 * (1 - cos(pi x))`, `sin(0.5 pi x)`, ridge cosine, front
    cosine): 65-entry static float table + linear interpolation. Deterministic: same code, same table on every
    machine (build it from integer steps, not from Math.Cos at runtime, if you want zero doubt) [inf].
  - `GetTickCount` inside Stamp (3 calls per stamp) -> time per op, not per stamp.
- Expected: 20-40 % of a tile build and of the stamp cells time [inf - the per-native cost is not measured]. Note the
  ceiling: script is ~90 % of the build, so even 2x faster loops halve upload + stored, not more.
- Risk: low; same output (except the table option - look-identical within float error).
- Test (WB compile, DS numbers): first a micro-bench line in the TEST_KIT - 100 000 `Math.Max(a, b)` vs the inline
  if, 100 000 `Vector(x, y, z)` vs template copy, between two `GetTickCount` calls - gives ns per native and decides
  how far to go. Then a third path in the existing PAPER TEST 1 bench (`old / fast / natives-free`, 'same quads' 1).

### 3.2 Make the live window cheaper: smaller live tiles + only-visible stored rebuilds - HIGH chance, medium gain
- Live upload is now the largest sheet part (0.4-0.84 ms/frame). A wheel tile rebuilds the whole 3 m / 441-vertex
  tile every 0.1 s; at 10 m/s a wheel crosses ~1 m per upload, the rut is ~0.3-0.5 m wide [inf, mesh-cost findings
  item 4]. A flags-0 MeshObject cannot be updated in part, and a MeshObject with sub-meshes is still one resource, so
  the only "dirty rectangle" in Enfusion is a smaller entity [repo: IDA Q1, Q4].
- How: live slots as 1.5 m entities (11 x 11 = 121 vertices), stored tiles stay 3 m (the store already knows per-cell
  data; a stored 3 m tile is built when the live sub-tiles freeze back). 4x fewer vertices per wheel rebuild; a
  wheel touches 1-2 sub-tiles per upload. Setup (0.2 ms, mostly 441-entry array clears) shrinks with the tile.
- Stored rebuilds (~24/s, 0.68 ms/frame): gate on `IEntity.IsRendered()` (needs `EntityFlags.SET_RENDERED`; true when
  drawn last frame) [code: API dump / repo IDA Q4] plus distance (always build < 15 m). Ruts behind the truck in
  first person are off screen. Keep them dirty and build on first IsRendered.
- Expected: upload ~-50 % (4x fewer verts per build, more builds) [inf]; stored -30-60 % in first person [inf].
- Risk: per-section normals mean a light seam at every 1.5 m border [repo: gotchas 2026-10-14 normals]; the
  existing skirts hide gaps, not shading seams. Check by eye in low sun.
- Test: one DS session, existing 'MUD prof parts' (sh upload, sh stored) + delay meter, A/B by a switch; WB 2-min
  check that IsRendered flips for a tile behind the camera.

### 3.3 Cheaper sync maths (keep deterministic, change formulas) - MEDIUM-HIGH chance, small-medium gain per vehicle
Per point 0.4: allowed if every machine runs the new code.
- **Drag: LimitDrag stays, but over fewer cells.** ~~Scale the press drag by the soft-wall factor and drop
  LimitDrag~~ - REFUTED offline 2026-10-19 (sec 8): without LimitDrag texture stretch p99 goes 0.40 -> 2.8 with or
  without the pf scale. What works: LimitDrag over the stamp's own stamped cells +1 (not the whole axis-aligned
  stamp box) = -64 % drag work, stretch p99 0.48-0.60; once per op = -77 %, p99 up to 0.69. Drag out of the synced
  layer (section 2) remains the memory / snapshot / hash win.
- **Slump** (~70 ms per 5 s, ~227 ground reads + 3 passes over ~700 cells per op [repo: README Stage 2 run 2]):
  MEASURED offline (sec 8): **1 pass at factor 0.6 = 3 passes at 0.25** within 0.6-2.2 mm rms (worst cell 2-3 cm)
  for -67 % work; plus only cells within 1 of the op's stamps = -85-87 % work at 3-5 mm rms. Also: read the ground
  through the rut-field arrays directly, not `GroundY` -> `ColumnAt` / `Heights` (~40-80 script / native calls per
  read: Vector, ToFine, BaseAt, 4x GetH, FieldFloorAt, 8x LockAt ... [repo: MUD_WorldGround.c Heights]).
- **Sheet restore + replay** (~50 ms per 5 s): only needed where the op box overlaps own prediction stamps; skip
  restore for ops of other vehicles with no own stamps in the box (check whether it already does) [inf].
- Expected: sync per vehicle about -40 % (sec 8: offline work counts x measured ms). This matters most with many
  vehicles (section 1 / 11) and on the DS.
- Test: hash compare client A vs B after the change (must still match each other), sync COST line before / after.

### 3.4 Premade rut pieces from the op stream (a different way to draw, very little script) - MEDIUM chance, large gain
- What: model the soft-rut look as imported meshes (.xob) - a 1 m rut-edge strip with ridges, a wet groove, a
  smeared mud texture - and place them along each op segment. Every client places the same pieces from the replicated
  op segments (positions are pure maths from the op = deterministic for free, no stamp / slump / hash).
- Why it escapes our limits: an imported mesh has authored normals, vertex colours, a 2nd UV set, LODs, a parallax
  material, engine culling, and is the same resource for every piece (the script mesh has none of these [repo:
  gotchas 2026-10-14]). Per piece the script cost is one spawn or one `SetTransform` on a pooled entity, no vertex
  loops.
- Look it can give: tread, wet dark mud, low pushed ridges, smear, fade at ends (alpha in the material / vertex
  colour). Depth: the felt rut (rut field) already cuts real depth in the patch; pieces only add the top 0.05-0.2 m.
- What it loses: continuous accumulation (pass 3 looks like pass 1 unless you swap to a "pressed" variant), slump
  animation, clean crossing of two ruts (pieces overlap; z-fight -> pick a fixed draw order / small height steps).
- Optional bend: if the probe in 3.5 works, give the piece 4-8 bones to follow the ground, or a morph target
  ("fresh" -> "pressed") driven by `Animation.SetMorphState(name, value)` [code: API dump, Animation class] - one
  native call per piece per change.
- Fit with ONE OWNER PER SURFACE (lesson 7): the patch stays the only ground; pieces are a decal-like layer over it.
  That is the MudRunner split: tracks as a separate mesh drawn over the terrain [doc: mudrunnermods.com "The Mud of
  MudRunner"; repo: research/2026-10-14-mudrunner-mud-article.md] - their rendering is GPU, the split carries over.
- Scale: a piece per ~1 m per wheel = ~2 per m driven; 10 km of tracks = ~20 000 static instances. Arland already
  runs ~66 000 RELATIVE_Y roots [repo: gotchas 2026-10-17 PinProbe]; with a view distance on the prefab it is
  plausible [inf]. Pieces must follow our ground: place them from the felt floor (DugUndug) at op time.
- Cost unknown: `SpawnEntityPrefab` per piece (guess 0.05-0.3 ms [inf]); pool + `SetTransform` + `Update` avoids it.
- Risk: medium; look may read as "stickers"; z-fight on fresh cuts if the felt rut cut lands after the piece.
- Test (one WB look session + one DS timing): author one piece (Blender, MatPBRBasic with HeightMap +
  ParallaxScale as vanilla Piles_01_Parallax.emat [repo: sheet-alternatives option D]), place it along a driven path
  from op segments, time 100 spawns vs 100 pooled SetTransforms with GetTickCount, look at it in low sun.

### 3.5 Skinned grid moved by bones instead of rebuilt (option C, sharpened) - LOW-MEDIUM chance, large gain if it works
- What exists [code: API dump]: `IEntity.SetBone(TNodeId bone, vector angles, vector trans, float scale)`,
  `SetBoneMatrix`, `GetBoneMatrix` on every entity class; `Animation.SetBone(ent, ...)`,
  `Animation.SetMorphState(name, value)`, `GetBoneIndex(name)`. Vanilla skeletons reach 231 bones with 4 influences
  per vertex [repo: KB character-gear-and-anim-data.md, quoted in sheet-alternatives].
- Idea: an imported 3 m grid (21 x 21 verts) skinned to a 15 x 15 bone grid (0.2 m); a changed tile = up to 225
  `SetBone` calls (one native each) vs a full rebuild (~3000 natives + loops). GPU skins it.
- Unknowns that kill it if wrong (all [inf]): SetBone may need an animation component / graph to take effect on a
  static entity; the bounding box may stay at bind pose (tile culled when bones move it up/down); normals only turn
  with bone rotation - script must tilt each bone from neighbour heights (cheap: 2 subtracts); bone count limit per
  skin; GPU cost of ~300 skinned instances.
- No public example of SetBone on a non-animated prop found (web search 2026-10-19, nothing on point).
- Test (WB, 30 min): import a 3 x 3 m plane skinned to 4 x 4 bones; spawn it bare (no anim component) and with a
  BaseProcAnimComponent; call SetBone on one bone each frame (+0.3 m); check (a) does it move, (b) does it vanish when
  you move the bone 2 m out of the bind bounds and look away / back, (c) GetTickCount around 10 000 SetBone calls.
  Same session: SetMorphState on a 1-morph mesh.

### 3.6 Coarser mesh near the camera + texture carries the fine detail - MEDIUM chance, large gain on builds
- The single biggest build cut is resolution: 0.30 m cells = 4x fewer vertices and quads. Far LOD at 0.30 m already
  works [repo: README lesson 6]. Near the camera the 0.15 m grid carries tread-scale bumps that a parallax height map
  can fake: `ParallaxScale / HeightMap` on MatPBRBasic, MatPBRDecal, MatPBR2Layers [repo: IDA 2026-10-15 item 12];
  vanilla Decal_Tracks_01.emat uses HeightMap + ParallaxScale 0.02 + WaterAccumulation 0.65 [repo:
  sheet-alternatives option B text]. Rut walls to dirt by slope: MatPBRMulti `TopDownProjection` [repo: IDA Q3].
- Keep 0.15 m only in the 1.5 m live sub-tiles under the wheels (3.2) and 0.30 m everywhere else.
- Risk: ridges lose sharpness at 0.30 m (offline: the 0.25 m grid keeps ~90 % of the ridge [repo: 2026-10-16 soft
  mud alternatives]); parallax breaks at steep view angles.
- Test (WB look only): same driven area at 0.15 vs 0.30 with a parallax emat on the dirt section; user judges.

### 3.7 Hybrid ownership: the sheet only while mud is moving, then bake into the patch - MEDIUM chance, fixes memory
- What: sheet tiles exist only where stamps landed in the last N seconds (live / settling); when a tile has been
  quiet for N s, its final shape (from the synced layer, so the same everywhere) is resampled into a look channel of
  the patch (0.25 m grid) and the tile + its sync tile are freed. Patch rebuild ~1 ms per 64 m2 vs tile ~2.5 ms per
  9 m2 [repo: sheet-alternatives sec 1].
- Fixes: unbounded sync memory (B7), the stored-tile count, the stored rebuild cost; leaves the soft live look where
  players watch it happen.
- Cost: one pop per tile at the handoff (resample loses ~10 % ridge) - the exact two-owner seam lesson 7 warns
  about, but once per tile instead of continuously. Handoff = patch built first with the baked channel, tile hidden
  the frame the patch mesh is up (the LIFT_BY_DRAWN ordering idea).
- Test: WB look of the handoff pop; DS memory line (sync tiles held) over a 10 min drive.

### 3.8 Already proposed, still valid, small
- One-pass triangle bucketing per material slot in the collider builder; per-tile cache in BuildColliderBand
  [repo: perf-research-ida-web Q2, enforce-perf Q4] - patch side, helps the ground under any sheet route.
- Prints off on the DS (~2.2 ms per Print line) [repo: perf-assessment item 1].
- Heading-first build order in QueueNear [repo: deep dive option 2].

---

## 4. How other games do it (all other engines; what carries over)

| Game | How | Carries over to Enfusion? |
|---|---|---|
| MudRunner / Spintires | mud grid drawn by a vertex shader from a height texture the CPU paints; dense mud only in 16 m blocks that have mud; separate physics data; tracks as a separate mesh; mud particles on a CPU thread [doc: mudrunnermods.com/the-mud-of-mudrunner; gamedeveloper.com mud-and-water-of-spintires-mudrunner, via repo] | shape on GPU: no. Blocks only where mud is, separate look / feel data, tracks as their own mesh pieces: yes (3.4, 3.7) |
| SnowRunner | dynamic heightmaps + particle solver + LOD (secondary article, unverified [doc: foro3d.com 2026-05]) | no primary source; GPU-side assumed |
| Batman: Arkham Origins | prints rendered into a height texture, tessellation (PC) / relief mapping (consoles) [doc: GDC 2014, gdcvault 1020177] | no custom shaders; relief = our parallax materials, generic only (3.6) |
| BeamNG, RDR2, Death Stranding, Horizon, Battlefield | no CPU-mesh write-ups found (repo search 2026-10-18, again 2026-10-19) | - |
| Arma 3 / Reforger mods | no runtime terrain deformation mod found; "Explosive Crater System" places crater objects [repo: IDA-web Q5] | placing premade objects = 3.4 |

Every published deformable-ground system does the shape on the GPU. What a script-only engine can copy is *where*
the dense data exists (only where mud moved) and *who owns a surface* (one owner, tracks as their own pieces).

---

## 5. Dead ends (do not spend time; proof given)

- TrackDecalFactory / World.CreateTrackDecal / vanilla tyre tracks / DecalEntity on holes - section 0.3.
- Merging tiles for render - section 0.1.
- DYNAMIC_VERTEXES, UPDATE_BOUNDBOX, in-place UpdateVerts, sub-mesh partial updates [repo: IDA 2026-10-15, Q1].
- Script threads: `thread` is a coroutine on the main thread; the only async engine call found is
  `BaseWorld.AsyncTraceMove` [repo: mesh-cost findings, dead ends].
- Native bulk array / vector maths: none in the API dump (searched 2026-10-19: no array copy / resize / math helper;
  AABGridMap / CircleGridMap / PointGridMap are spatial indexes only) [code: enfusion-mcp-BK data/api].
- `Shape` (debug visualizer) as a cheap mesh: debug-only, unlit colour [code: API dump "Instance of created debug
  visualizer"; inf for the look].
- A finer script clock: the 1.9 timestamp API is millisecond precision [doc: reforger.armaplatform.com
  modding-update-sept-24-2026]; GetTickCount sums stay the tool.
- Heightfield collider, terrain height edits, custom shaders - proven in earlier docs.

---

---

# Deeper pass (2026-10-19, second round)

Sources added this round: the offline Bohemia wiki + KB copy in enfusion-mcp-BK (`data/wiki/pages.json`, 258 pages;
`data/kb/patterns`), more API dump searches, a code read of MUD_MudSync (job steps, slump, LimitDrag), MUD_MudSheet
(SyncBegin, TakeCovered, Wheels, BuildTileMesh setup), MUD_WorldGround.Heights, and an offline experiment on the
synced maths.

## 8. Cheaper sync maths, measured offline
Full table: [results/2026-10-19-sheet-sync-maths-offline.md](../results/2026-10-19-sheet-sync-maths-offline.md);
tool `Tools/mud_sheet_cost_exp.py` (new, reuses the 2026-10-17 sync proto). Three scenarios (base, 5 passes,
straight), reference = game today.
- **Slump: 1 pass at factor 0.6 = 3 passes at 0.25** within 0.6-2.2 mm rms, worst cell 2-3 cm, for -67 % slump
  work. Only cells within 1 of the op's stamps: -85-87 %, rms 3-5 mm. [MEASURED offline]
- **LimitDrag over the stamp's own cells +1** instead of its whole box: -64 % drag work, heights identical, stretch
  p99 0.48-0.60 (today 0.40). Dropping LimitDrag, or my soft-wall-drag idea, gives stretch p99 2.8 - refuted.
- **Sync grid 0.30 m drawn at 0.15 m**: -67-76 % on every sync step, but groove -11-14 % depth, ~12 % wider, single
  cells up to 0.2 m off. Look test needed; not first.
- In game (inference): sync layer about -40 % per driving vehicle on every client and the DS.

## 9. Tile build: where the natives are (code count, BuildTileMesh 5754-6400)
| phase (DS ms per tile, fast path) | per touched vertex / quad | natives |
|---|---|---|
| setup (0.2) | dirt score per touched vertex: `Math.AbsFloat` + `Math.Sqrt` + an int hash; 4x `HasTile` (map lookups) per tile; `TaperField` if bare cells; clears of 3-4 x 441-entry arrays | ~2 per touched vertex |
| heights `vy` (in setup/quads) | tiles with an open side: up to 4x `Math.Min` + `Math.Clamp` per vertex; `TaperFade` call near bare | 0-5 per vertex |
| quads (0.5) | per emitted vertex `Vector()` + 2x `Math.Clamp` (UVs); mixed quads 6x Min/Max; SMOOTH_EDGE cuts; `DirtQuad` / `Changed` calls when smooth is off | ~3 per vertex + cuts |
| skirt (0.03) | `Vector()` subtract per skirt vertex | small |
| engine (0.12) | Create + 2x UpdateIndices + 2x UpdateVerts; both sections are given the FULL vertex array `tv`, so normals / tangents / bounds run twice over all vertices | - |

So ~1300-2600 native calls per touched 441-vertex tile [inf from the code]. Two cheap extras the count shows:
the per-vertex dirt noise (`qhs` hash -> `qnz`) can be a static 64 x 64 table indexed by `(gi % 64, gk % 64)`;
the base UV `((um * TQ + ci) * CELL) / SIZE` is constant per (um, vm, ci, ck) and can be a static table, leaving one
subtract and a clamp only near the UV edge.

Micro-bench (put in the TEST_KIT, read on the DS client; GetTickCount sums are unbiased [repo: gotchas]):

```c
// MUD bench natives (research 2026-10-19 sec 9): ms for N calls each way; s keeps the loops from being dead code
static void BenchNatives()
{
	int n = 200000;
	float a = 0.3;
	float b = 0.7;
	float s = 0;
	int t0 = System.GetTickCount();
	for (int i = 0; i < n; i++)
		s += Math.Max(a, b + i * 0.000001);
	int tMax = System.GetTickCount(t0);
	t0 = System.GetTickCount();
	for (int j = 0; j < n; j++)
	{
		float m = b + j * 0.000001;
		if (a > m)
			m = a;
		s += m;
	}
	int tIf = System.GetTickCount(t0);
	vector tv[64];
	t0 = System.GetTickCount();
	for (int k = 0; k < n; k++)
	{
		int slot = k % 64;
		tv[slot] = Vector(k * 0.15, s, 0.3);
	}
	int tVec = System.GetTickCount(t0);
	t0 = System.GetTickCount();
	for (int q = 0; q < n; q++)
	{
		int slot2 = q % 64;
		vector p = tv[63 - slot2];
		p[0] = q * 0.15;
		p[1] = s;
		tv[slot2] = p;
	}
	int tTpl = System.GetTickCount(t0);
	Print(string.Format("MUD bench natives: n %1 | Math.Max %2 ms, inline if %3 ms | Vector() %4 ms, copy+set %5 ms | s %6", n, tMax, tIf, tVec, tTpl, s));
}
```
Reading it: (tMax - tIf) / n = cost of one Math.* native over an inline compare; multiply by ~2000 per tile and by
the ~55 live builds/s (sec 10) to get the ms/s 3.1 can save. If that is < ~10 % of a build, skip 3.1 and go to 3.2.

## 10. Live upload: a model that fits the logs
- Live builds ~517-571 per 10 s while driving (deep dive table 3.3) x ~1.6 ms (BuildTileMesh mean on the DS before
  the fast path) = ~88 ms/s = ~1.0 ms/frame at ~90 fps, which matches 'sh upload' 0.84-1.27 ms/frame [repo, checked
  against both runs]. So the live window is ~55 tile builds a second for one truck.
- Why so many: at 10 m/s a wheel crosses a 3 m tile in 0.3 s = 3 uploads, the left and right tracks (~2 m apart)
  often sit in different tiles, front and rear axles too, and every op restore marks the tiles of its box again.
- 1.5 m live sub-tiles (3.2): a wheel crosses one in 0.15 s (1-2 uploads), each build ~1/4 of the vertices plus a
  fixed part. Estimate ~8-10 sub-builds per upload at ~0.45 ms instead of ~5 builds at ~1.6 ms: ~-45 % on sh upload
  [inf; the fixed part per build is the unknown - the bench in PAPER TEST 1 can time a 11 x 11 build directly].

## 11. Vehicle count, and the DS is the first wall
Per driving vehicle, ms per second of DS main thread (sources in brackets):
| part | ms/s per vehicle | source |
|---|---|---|
| rut ops: 6-14 ms each, ~1.3/s under the sheet (RUT_SHEET_MS 750) or ~4/s off it | 8-56 | perf-assessment 3.x, mud-zone-rut-perf R1 |
| patch colliders: ~8 ms each, ~3 per op | up to ~100 ("roughly 100 ms of every second") | perf-assessment item 2 / 3.1 |
| sync layer on the DS (SERVER_LAYER, only for joiner snapshots) | ~40-50 (client-side numbers); DS log 9-100 per s before P1 | sheet-alternatives sec 7; deep dive B6 |
| total | ~150-200 = 15-20 % of the DS frame budget | inference |

So ~5-6 trucks driving at once fill a 30 fps DS main thread with ground work alone, and the one-collider-per-frame
cap saturates at 2-3 trucks [repo: perf-assessment summary 5]. The sheet is not the first limit for a many-vehicle
PvP match - the felt rut pipeline is. The sheet's share on the DS (SERVER_LAYER) is ~25 % of the per-truck cost and
buys only joiner snapshots; turning it off (section 2) is the cheapest DS win in this whole doc.
On each client: every remote truck costs its sync layer work (~0.45-0.55 ms/frame today, ~0.3 after sec 8) wherever
it drives, plus restores + stored rebuilds when it is inside the drawn radius.

## 12. Premade rut pieces (3.4), deeper
- Spawn cost, from our own log: `SpawnTile` (a plain script entity, `GetGame().SpawnEntity`) shows as 'z spawn'
  0.01 ms per frame at ~1 spawn per frame [repo: fixes-2-4 sec 1] = ~10-20 us per spawn (1 ms ticks, averaged). A
  prefab with an .xob loads its resource once; or skip spawning: pool GenericEntities, `SetObject` the piece's
  VObject once, then only `SetTransform` + `Update` per placement.
- Rate: one piece per 0.5 m op segment per track = ~40 pieces/s per truck at 10 m/s -> ~0.5-1 ms per second
  (~0.01 ms/frame) vs the sheet's ~1.3-2.2 ms/frame [inf].
- Count is the real problem: 0.5 m pieces = ~144 000 per truck-hour. Use 1.5-2 m pieces (one per 3-4 segments), a
  global cap that recycles the oldest pieces far from every player, and a view distance on the prefab. Render: 350-750
  small script entities cost ~0.2 ms render in our A/B (sec 0.1); instanced .xob pieces should not cost more [inf].
- Same on every client: placement is maths on the op segments; "pressed" variants from a per-0.5 m-cell pass counter
  keyed by op seq (deterministic, a few ints per cell).
- Following the ground: a rigid 1.5-2 m piece on 0.25 m patch ground leaves gaps on bumps; sink it 3-5 cm with a
  short skirt, or bend it: ProcAnim (below) or SetBone.
- Imported meshes have vertex colours / alpha (fade the ends), authored normals, LODs [doc: KB
  resource-manager-blender import settings; repo: script meshes have none of these].

## 13. Bones and morphs (3.5), deeper
- Two engine paths move bones, and one of them is native per frame [doc: wiki "Procedural Animation Editor" /
  "Nodes" / "Basics Tutorial", offline copy]: a `ProcAnimComponent` with a `.pap` (bones: RotationSet / TranslateSet /
  ScaleSet, "Update collider" off by default) driven by a `.siga` (signals -> maths). Script only sets a signal:
  `SignalsManagerComponent.AddOrFindSignal(name)` + `SetSignalValue(index, value)` (local, not replicated)
  [code: API dump, arma-classes.json]. So a piece can deepen / bend with ONE native call per change, not one per bone.
  Vanilla uses it for dashboards, exhausts, gear [doc: wiki tutorial uses UAZ469 `v_exhaust`].
- The script path: `IEntity.SetBone / SetBoneMatrix`, `Animation.SetMorphState(name, value)` [code: API dump].
  No public example of SetBone on a static prop found (web search 2026-10-19).
- Skinning import: "Export Skinning", max 4 bones per vertex [doc: KB resource-manager-blender].
- What this means: a full 225-bone "deformable tile" stays a long shot (bounds / culling / per-bone calls unknown),
  but a rut PIECE with a few bones driven by 1-2 signals ("pressed", "bend") is a well-trodden engine feature. Probe
  both in the same WB session (3.5 test + a 2-bone piece with a .pap / .siga and SetSignalValue).

## 6. Top 3 to try first (revised after the deeper pass)

1. **One DS session, measurements only + two switches**: (a) 2 then 4 robot trucks, sync COST line on a far client
   and on the DS; (b) the native micro-bench (sec 9); (c) `SERVER_LAYER` false A/B on the DS; (d) slump 1 pass f 0.6
   near 1 + LimitDrag per stamp strip (sec 8 - same code everywhere, re-baseline the hash compare; check client A vs
   B still match each other).
2. **3.2 (1.5 m live sub-tiles + IsRendered gate on stored rebuilds)**, plus 3.1 only if the bench says natives are
   a real share. Read sh upload, sh stored, delay meter.
3. **One WB probe session for the cheap-look routes**: a premade 1.5 m rut piece placed from op segments with a
   2-bone ProcAnim "pressed" signal (sec 12-13), SetBone on a skinned plane (3.5), 0.30 m tiles with a parallax dirt
   emat (3.6). Look by eye; time spawn vs pooled SetTransform.

## 7. What result says the sheet is still not worth it

Proposed budget (your call; numbers are mine): the whole sheet (sheet + sheet sync + its upload) at <= 1.0 ms/frame
on the client at 10-14 m/s with one local vehicle, plus <= 0.15 ms/frame per extra remote vehicle, and <= 1 ms/frame
on the DS.

The sheet is not worth it if, after top-3 items 1-2:
- sync cost per extra remote vehicle stays >= ~0.3 ms/frame and you keep bit-exactness (no lazy far ops) - a
  10-truck PvP match then costs ~3 ms/frame on every client for mud nobody walks on; or
- the sheet still costs > ~1.5 ms/frame at speed with one vehicle after the native purge and 1.5 m live tiles
  (that would mean script loops are not the limit you think, and nothing in Enfusion moves them out of script); or
- the probe session shows premade pieces + parallax (3.4 + 3.6) give most of the look at < 0.2 ms/frame - then the
  whole-map route is shape on the patches (old option A) + pieces, and the sheet stays parked or becomes the
  short-lived "mud is moving" layer of 3.7 only.

## Sources

- Repo docs cited inline (branch displaced-ruts-partial-split): features/mud-sheet/README.md; research/2026-10-18-
  sheet-alternatives-and-perf.md, -perf-research-ida-web.md, -mud-mesh-cost-research-findings.md,
  -enforce-perf-research.md, -fixes-2-4-and-brainstorm.md, -all-offroad-sheet-deep-dive.md, -perf-assessment.md,
  -mud-zone-rut-perf.md; 2026-10-16-soft-mud-alternatives.md, -custom-shaders.md; 2026-10-15-ida-rut-questions.md;
  mud-gotchas.md; plans/2026-10-18-hybrid-sheet-feel-plan.md; results/2026-10-17-mud-sheet-sync-proto.md,
  2026-10-18-h1-snap-probe.md, 2026-10-18-look-rut-delay-split.md. Code: Scripts/Game/Mud/MUD_MudSync.c,
  MUD_MudSheet.c (BuildTileMesh), MUD_Dig.c.
- API dump: github.com/steffenbk/enfusion-mcp-BK data/api/enfusion-classes.json, arma-classes.json (IEntity.SetBone,
  Animation.SetMorphState, TrackDecal, World.CreateDecal, AABGridMap, Shape, SCR_TerrainMeshEntity).
- [The Mud of MudRunner](https://www.mudrunnermods.com/the-mud-of-mudrunner/)
- [Terrain Physics - SnowRunner (fan site)](https://mudrunnermods.com/?p=40604)
- [Swarm Engine and terrain deformation in SnowRunner (secondary, unverified)](https://foro3d.com/en/2026/mayo/swarm-engine-y-el-arte-de-deformar-terreno-en-snowrunner.html)
- [Deformable Snow Rendering in Batman: Arkham Origins, GDC 2014](https://gdcvault.com/play/1020177/Deformable-Snow-Rendering-in-Batman)
- [Arma Reforger: Scripting Performance (biki)](https://community.bistudio.com/wiki/Arma_Reforger:Scripting_Performance) - unreachable from here (DNS / Cloudflare); claims taken from search snippets and the earlier findings doc
- [Arma Reforger: Scripting Example (biki)](https://community.bistudio.com/wiki/Arma_Reforger:Scripting_Example) - foreach / cached Count() advice (search snippet)
- [Changes and Improvements in 1.9 (modding update)](https://reforger.armaplatform.com/news/modding-update-sept-24-2026)
- [Remote Script Console (workshop)](https://reforger.armaplatform.com/workshop/6686460F377B64FD-RemoteScriptConsole)

## 14. IDA answers 2026-10-19 (local session; results/2026-10-19-sheet-cost-probes.md)
- 3.5 / 13 SetBone: works without an anim component IF the .xob is skinned and `ent.GetAnimation()` is called once
  (it makes the skeleton instance; SetBone alone silently does nothing). Bounds stay static - set the MeshObject
  `BoundingBox` override. Still needs the WB look (C1). [IDA]
- 3.4 / 13 ProcAnim pieces: REVISED. SetSignalValue is one float write, but every registered ProcAnim is evaluated
  every frame (no change / visibility test), so ProcAnim only on live pieces; static .xob for the bulk. [IDA]
- 12 spawn: SpawnEntityPrefab's native loads nothing when given a held Resource; component init not traced. [IDA]
- Instancing of spawned same-.xob entities: unresolved (only terrain objects shown to batch). [IDA, partial]
- 3.2 IsRendered: already proven on our patches (RENDER_GATE waits counted, audit-ground 2026-10-18). [repo]
- Parts A-C (switches, DS session, WB probes) not run yet.

---

## 15. Round 3 (after the local IDA pass, prompts/2026-10-19-research-after-ida.md)

Inputs: results/2026-10-19-sheet-cost-probes.md (D1-D4 + review), mud-gotchas IDA 2026-10-19 lines, the API dump in
enfusion-mcp-BK (`data/api/*.json`), the offline wiki / KB copy (`data/wiki/pages.json`, `data/kb/patterns`).
Labels as before; [IDA] = from the probes doc / gotchas.

Correction first: sec 3.5 (taken from the 2026-10-18 doc) said "vanilla skeletons have 231 bones" citing KB
character-gear-and-anim-data.md. That number is not in the KB copy (grep 2026-10-19). The only bone counts I can
verify: wiki "Model Performance" table - Basebody_Male_01 100 bones, Basebody_Male_Head_01 89, Jacket_BDU_M81 49,
UAZ469 37 [doc]. Max 4 bones per vertex on import [doc: KB resource-manager-blender "Export Skinning"].

### 15.1 Routes re-ranked

| route | was | now | why |
|---|---|---|---|
| Engine TrackDecal on our ground | dead | dead | terrain-only [repo IDA 2026-10-15] |
| Merge tiles for render | dead | dead | render A/B ~0.2 ms [repo] |
| Drop LimitDrag / soft-wall drag | idea | dead | offline stretch p99 2.8 [repo results 2026-10-19] |
| ProcAnim on every rut piece | medium | dead for the bulk | every registered ProcAnim runs every frame, no change / visibility test [IDA D2]; fine for <= ~20 live pieces |
| A1 SLUMP_ONE + A2 DRAG_STRIP | high | high, spec ready (15.5) | offline -85-87 % slump, -64 % drag work; port details from the review |
| SERVER_LAYER off on the DS | high | high | unchanged; DS-only setting, clients unaffected |
| IsRendered gate on stored rebuilds | medium | higher | the pattern already works on our patches (MUD_WorldGround.RenderWait counts) [repo, probes doc review 5] |
| Static rut pieces (.xob) placed from ops | medium | higher | no per-frame cost if static; SpawnEntityPrefab loads nothing with a held Resource [IDA D4]; variant swap or SetBone-once for "pressed" (15.2) |
| Skinned tile moved by SetBone | long shot | medium | path known: skinned .xob + `GetAnimation()` once, no anim component; bounds via MeshObject `BoundingBox` [IDA D1]; one shared mesh for all tiles; look = bilinear of the bone grid (offline rows exist) - design 15.3 |
| 1.5 m live script-mesh tiles | medium | medium | unchanged; now competes with 1.5 m skinned tiles (15.3) |
| Natives purge | medium | medium | unchanged; the bench decides |
| Coarser synced grid | low | low | look trade (offline) |
| Lazy far ops / relax bit-exactness | decision | decision | unchanged (sec 2) |

Top 3 local tests now:
1. **DS, two launches** (test-kit values are read once per machine at start; A1/A2 change synced maths, so DS and all
   clients must run the same value [repo probes review 2]): launch 1 all off, launch 2 A1 + A2 on; >= 6 runs each
   (CLAUDE.md). SERVER_LAYER off is a DS-only value: a third, DS-only restart, or ask the user. Bench line in both.
2. **WB C1 skinned tile**: skinned plane / tile, `GetAnimation()`, SetBone and SetBoneMatrix, BoundingBox off / on,
   121-bone import, 300 static skinned tiles (counts + `System.MemoryAllocationKB` delta).
3. **WB C3 + D3 pieces**: 100 / 1000 static pieces of one .xob (draw-call / MeshObject stats), variant swap, spawn vs
   pool, memory per 1000.

### 15.2 Rut pieces with no per-frame cost

Methods confirmed in the API dump [code: enfusion-classes.json / arma-classes.json] or our scripts [repo]:
| need | class.method (dump) | also used in our code |
|---|---|---|
| spawn a local, not replicated, prefab | `ArmaReforgerScripted.SpawnEntityPrefabLocal(notnull Resource, BaseWorld, EntitySpawnParams)`; `Game.SpawnEntityPrefab(...)` | MUD_Dig.c:227, MUD_GrassCutProbe.c:151 [repo] |
| hold the prefab / mesh resource | `Resource.GetResource()`, `Resource.IsValid()`; `BaseResourceObject.ToVObject()` | `Resource.Load(...)` MUD_Dig.c:224 [repo] (Load itself is not in the dump) |
| move a pooled piece | `IEntity.SetTransform(vector mat[4])`, `IEntity.Update()` | - |
| swap the look | `IEntity.SetObject(VObject, string)` (+ `Update()`: stale bounds otherwise [repo gotchas 2026-10-09]) | sheet / patches |
| hide / show | `IEntity.ClearFlags / SetFlags (EntityFlags)` | patches (VISIBLE) [repo] |
| bones | `IEntity.GetAnimation()`, `IEntity.SetBone(TNodeId, vector, vector, float)`, `IEntity.SetBoneMatrix(TNodeId, vector mat[4])`, `Animation.GetBoneIndex(string)` | - |
| morphs | `Animation.SetMorphState(string, float)` | - |
| ProcAnim signal | `SignalsManagerComponent.AddOrFindSignal(string, float)`, `SetSignalValue(int, float)` | - |
| component on / off | `GenericComponent.Activate(IEntity)`, `Deactivate(IEntity)`, `IsActive()` | - |
| memory delta | (not in the dump) | `System.MemoryAllocationKB()` MUD_CoarseSkin.c:310 [repo] |

Designs (piece = 1.5 m, one per ~3 op segments per track; ~13 pieces/s per truck at 10 m/s [inf]):
| design | how "pressed" works | script calls per piece | per-frame engine cost | memory per piece beyond the entity |
|---|---|---|---|---|
| A. variant swap | 2-3 premade .xob (fresh / pressed / deep); a later pass swaps | place 1 (spawn) or 2 (pooled SetTransform + Update); per pass that changes the level: 2 (SetObject + Update) | none (static) | 0 - mesh shared |
| B. SetBone once | skinned piece, 4 bones along it: bone y from 4 felt-floor samples (follows the ground) + press depth; set at placement and on a pass, then left alone | place 1-2 + `GetAnimation` 1 + SetBone 4 (+ 4 floor reads); per pass <= 4 SetBone | unchanged bones cost nothing on the script side [IDA D1: same matrix returns early]; whether a skinned entity costs per frame when nothing changes = open (D6) | skeleton instance 0x80 B + 48 B per bone override [IDA D1] + bind-pose copy (assume 48 B per bone) = ~0.5 KB for 4 bones [inf] |
| C. live ProcAnim pool | 10-20 ProcAnim pieces near each wheel (bulge / press animated by a signal), handed off to A or B when the wheel is > N m past | per change 1 SetSignalValue; handoff 2 | 1 ProcAnim eval per live piece per frame [IDA D2] - bounded by the pool size | as B |
| D. morph | `SetMorphState` on a morph-target piece | per pass 1 | open: needs an Animation, may evaluate per frame like B | open |

Recommendation [inf]: A for the bulk (2-3 levels), B only where pieces must bend over bumps, C only if the live tyre
bulge is wanted. Per truck-second: A ~26 calls + level swaps, B ~90 calls (7 per piece) - against the sheet's ~55
tile builds/s x ~1300-2600 natives = ~70 000-140 000 natives/s (sec 9-10). Memory per 1000 pieces: A ~entity
overhead only (unmeasured), B +~0.5 MB; for scale, one stored sheet tile keeps its CPU mesh for life (~28 B/vertex x
~700-1000 vertices, IDA 2026-10-18) + GPU copy, plus 11.2 KB sync data - so 1000 pieces ~ the memory of a few dozen
sheet tiles [inf]. Count control: global cap + recycle the oldest piece far from every player; prefab view distance /
LOD Factors on the MeshObject component [doc: wiki "Level Of Detail"].
Open for the probe: does `Deactivate` on a ProcAnimComponent clear the per-frame bit (`+156 & 4`) - if yes, design C
pieces can stay ProcAnim and go to sleep instead of a handoff (D5).

### 15.3 Skinned tile by bones: design

Grid options (sheet cells are 0.15 m):
| option | tile | bones | spacing | evidence for that bone count | look (heights) |
|---|---|---|---|---|---|
| a | 3 m | 11 x 11 = 121 | 0.30 m | above the 100 seen in vanilla [doc] | = offline "sync cell 0.30 drawn 0.15": depth -11-14 %, ~12 % wider, cells up to 0.2 m off |
| b | 3 m | 7 x 7 = 49 | 0.50 m | within | coarser than any offline row - ruts smeared |
| c | 1.5 m | 11 x 11 = 121 | 0.15 m | above 100 | heights identical to today's sheet (one bone per cell) |
| d | 1.5 m | 6 x 6 = 36 | 0.30 m | within | as option a |
Why the offline rows apply: with each vertex weighted to its 4 nearest bones bilinearly, linear blend skinning of
vertical translations is a bilinear upsample of the bone heights - the "sync cell X, drawn 0.15 bilinear" variants in
results/2026-10-19-sheet-sync-maths-offline.md [inf, standard skinning maths].

Normals: GPU skinning turns normals with the bone matrices. Translation-only bones keep the bind-pose (flat) normals,
so a rut would be lit like flat ground. Use `SetBoneMatrix` with a frame from neighbour heights: up = normalize(
(hW - hE) / 2s, 1, (hS - hN) / 2s), right / forward from cross products - one `Math.Sqrt`-class native + ~20 VM ops per
bone; neighbours' blended rotations give smooth shading [inf].

Calls per tile change (option c, 121 bones): full set ~121 SetBoneMatrix + ~121-360 natives = ~250-500 native calls,
no vertex loops, no MeshObject.Create / UpdateVerts / upload; a wheel at 0.1 s changes ~10-30 bones = ~20-90 calls.
Today's rebuild: ~1300-2600 natives + loops over 441 vertices + the engine part (sec 9) [inf].
Memory per tile: skeleton instance ~0x80 + 121 x 96 B (override + bind copy) = ~11.7 KB, mesh shared; today each tile
keeps its own CPU + GPU mesh [IDA 2026-10-18 + D1; inf for the bind copy size].

What a skinned tile cannot do (fixed .xob): texture drag (UVs fixed - the "fluid" cue), per-quad dirt vs top material
(TOP_LOOK two sections by touched cells), holes for bare / crater cells (only by collapsing bones - scale 0 - or hiding
the tile), SMOOTH_EDGE cuts. Skirts and the fade are premade (fine).

Live window or stored tiles?
- Stored tiles: yes - set the bones once at build, then nothing; that is where the count (~460-620 meshes) and the
  memory are.
- Live window: the calls are cheap enough, but the dirt material and texture drag would vanish exactly under the
  player's eyes. Option c (0.15 m bones) keeps heights identical, so a live script-mesh tile (with drag + dirt) and a
  skinned stored tile meet without a height step - the handoff is the one-time swap lesson 7 allows. Options a / b / d
  would show a step at that border.
- Suggested split [inf]: 1.5 m skinned tiles (c) for stored / far ground, 1.5 m script-mesh tiles only in the live
  window (3.2). If C1 shows 121 bones do not import or render, fall back to d (36 bones) for far tiles only.

Risks C1 must answer: (1) the override shows on screen on a bare entity after `GetAnimation()`; (2) culling with and
without the `BoundingBox` override when bones move 0.3-0.5 m; (3) does a 121-bone skin import and draw (max bones per
skeleton / per draw call); (4) does an unchanged skinned entity cost per frame (counts in WB, ms later on the DS); (5)
memory per tile (`System.MemoryAllocationKB` delta over 300 tiles); (6) SetBoneMatrix rotation gives the right
shading; (7) whether SetBone needs a following `Update()` on screen (IDA: SetBone marks the entity dirty itself).

### 15.4 Instancing (D3)

Wiki / KB copies (searched 2026-10-19): nothing on instanced or batched drawing of entities that share one mesh.
Found: Diag Menu > Statistics: "General" (disable / basic / detailed / all), "MeshObject stats" (none / count / mesh /
faces / verts), "Resource memory stats", "Decal stats"; Render: "CB draw calls min limit", "Shape instancing" (debug
shapes only [IDA D3]); "Terrain Entity: 1 block = 1 draw call"; LOD Factors on the MeshObject component [doc].
API dump: no instancing method.

Count test for the local AI (WB, counts only, fixed camera over an empty field):
1. Baseline: Statistics > General = all, MeshObject stats = count, then = mesh; screenshot.
2. 100, then 1000 pieces of ONE .xob in view (1 m grid), same screenshots + `System.MemoryAllocationKB()` delta.
3. 1000 pieces from 10 different .xob (100 each).
4. 1000 script MeshObjects (unique, e.g. sheet tiles or a 2-tri MeshObject.Create each).
Read: if a draw-call / batch line in General grows far less for (2) than for (3) and (4), same-.xob entities are
batched. If General shows no draw-call line, say so - the answer then comes from DS render ms later.

### 15.5 A1 / A2 port spec - UNREVIEWED PLAN CODE

Matches `Tools/mud_sheet_cost_exp.py` variants "slump: 1 pass f0.6, cells within 1 of stamps" and "drag: LimitDrag
over stamped cells +1 only (per stamp)", with the review points (probes doc review 3-4). Code is Enforce-shaped, not
compiled. Keep the 64-locals limit in mind: each helper in its own method.

A1 SLUMP_ONE (MUD_MudSync):
- Collect the stamped cells of the open op in Stamp (cells that passed `TileOf(i, k, true)` and were written), as two
  int lists (gi, gk) - packed keys overflow int at Arland cell counts [inf: 12.8 km / 0.15 = ~85 000 cells a side].
- After the op's last stamp the box is known (StepStamp sets m_iJi0 .. m_iJk1 = stamp boxes + 1). Then: SlumpBegin
  (even grid), then build the mask: seed = stamped cells; mask = seed dilated by 1 over 4 neighbours, computed from a
  copy (never in place); list = mask cells in row order. The +1 box margin holds the dilation.
- Ground: only the even cells some mask cell reads (its a0 / a1 x b0 / b1). Others: not read, ok 0.
- SlumpCells: a cell takes part (`m_aSok = 1`) only if in the mask AND it has a tile AND its 4 even grounds are ok.
- SlumpPass: 1 pass (`SLUMP_ONE_PASSES`), factor 0.6 (`SLUMP_ONE_K`) instead of MUD_MudSheet.SLUMP (the local sheet
  slump keeps SLUMP). Flow only between two `m_aSok` cells (the existing checks already do this once Sok includes the
  mask). Keep `f = Math.Min(drop * k * 0.5, da * 0.25)` and the `0.0002` floor. All flows from the old depths, then
  added (as now).
- SlumpWrite, JOB_QUANT, the hash and the sheet restore: unchanged (whole op box).

```c
// MUD_MudSync - SLUMP_ONE (UNREVIEWED PLAN CODE, research 2026-10-19 sec 15.5)
static const bool SLUMP_ONE = false;
static const float SLUMP_ONE_K = 0.6;       // sync slump factor (MUD_MudSheet.SLUMP 0.25 x 3 passes before)
static const int SLUMP_ONE_PASSES = 1;
protected ref array<int> m_aStI = {};       // open op: stamped cells, gi
protected ref array<int> m_aStK = {};       //                         gk
protected ref array<int> m_aSmSeed = {};    // per box cell: stamped
protected ref array<int> m_aSm = {};        // per box cell: in the mask (seed dilated by 1)
protected ref array<int> m_aSmList = {};    // box cell indices of the mask, row order
protected ref array<int> m_aEgNeed = {};    // per even cell: a mask cell reads it

// StartJob (rut op): m_aStI.Clear(); m_aStK.Clear();
// Stamp, where a cell is written (next to m_iStatStHit++):
//     if (SLUMP_ONE) { m_aStI.Insert(i); m_aStK.Insert(k); }      // duplicates are fine
// StepStamp, after the BOX_MAX test passed: SlumpBegin(); if (SLUMP_ONE) BuildSlumpMask();

protected void BuildSlumpMask()
{
	int w = m_iJi1 - m_iJi0 + 1;
	int h = m_iJk1 - m_iJk0 + 1;
	int n = w * h;
	m_aSmSeed.Resize(n);
	m_aSm.Resize(n);
	for (int z = 0; z < n; z++)
	{
		m_aSmSeed[z] = 0;
		m_aSm[z] = 0;
	}
	int cnt = m_aStI.Count();
	for (int s = 0; s < cnt; s++)
	{
		int c = (m_aStK[s] - m_iJk0) * w + (m_aStI[s] - m_iJi0);
		m_aSmSeed[c] = 1;
	}
	m_aSmList.Clear();
	for (int k = 0; k < h; k++)
	{
		for (int i = 0; i < w; i++)
		{
			int c2 = k * w + i;
			bool on = m_aSmSeed[c2] != 0;
			if (!on && i > 0 && m_aSmSeed[c2 - 1] != 0)
				on = true;
			if (!on && i < w - 1 && m_aSmSeed[c2 + 1] != 0)
				on = true;
			if (!on && k > 0 && m_aSmSeed[c2 - w] != 0)
				on = true;
			if (!on && k < h - 1 && m_aSmSeed[c2 + w] != 0)
				on = true;
			if (on)
			{
				m_aSm[c2] = 1;
				m_aSmList.Insert(c2);
			}
		}
	}
	MarkEvenNeed(w);
}

protected void MarkEvenNeed(int w)
{
	int ne = m_iEw * m_iEh;
	m_aEgNeed.Resize(ne);
	for (int z = 0; z < ne; z++)
		m_aEgNeed[z] = 0;
	foreach (int c : m_aSmList)
	{
		int i = c % w;
		int k = c / w;
		int oi = m_iJi0 + i - m_iEi0;
		int ok2 = m_iJk0 + k - m_iEk0;
		int mi = oi % 2;
		int mk = ok2 % 2;
		int a0 = (oi - mi) / 2;
		int b0 = (ok2 - mk) / 2;
		m_aEgNeed[b0 * m_iEw + a0] = 1;
		m_aEgNeed[b0 * m_iEw + a0 + mi] = 1;
		m_aEgNeed[(b0 + mk) * m_iEw + a0] = 1;
		m_aEgNeed[(b0 + mk) * m_iEw + a0 + mi] = 1;
	}
}

// SlumpGroundRow, per even cell e:
//     if (SLUMP_ONE && m_aEgNeed[e] == 0) { m_aEg[e] = 0; m_aEgOk[e] = 0; continue; }
// SlumpCells, per box cell c (before the TileOf call):
//     if (SLUMP_ONE && m_aSm[c] == 0) { m_aSok[c] = 0; m_aSd[c] = 0; continue; }
// SlumpPass: float kf = MUD_MudSheet.SLUMP; if (SLUMP_ONE) kf = SLUMP_ONE_K;
//     f = Math.Min(drop * kf * 0.5, da * 0.25);
//     with SLUMP_ONE loop over m_aSmList (i2 = v % w; k2 = v / w - each on its own int line) instead of every box
//     cell; zero and apply m_aSdd for list cells only (receivers are Sok cells = list cells).
// Passes: int passes = SLUMP_PER_OP; if (SLUMP_ONE) passes = SLUMP_ONE_PASSES;  (JOB_PASS end test)
// Stat: m_iStatSlumpCells += list count x passes.
```

A2 DRAG_STRIP (MUD_MudSync.Stamp + new LimitDragStrip):
- Per stamp, mark the cells that pass the geometric test (the offline `ok` set: before `TileOf`) in a scratch over the
  stamp box + 1 cell each side; dilate by 1 (4 neighbours, from the marks, not in place); sweep 0 over the marked
  cells in row order (k up, i up), sweep 1 over the same list reversed (= the game's reversed k and i). Per cell the
  existing LimitDrag body: `TileOf(i, k, false)` null -> skip; interior cells read the tile directly (P1 path), edge
  cells via `GetI` (no tile = THICK / 0 drag); write through the cell's own tile.

```c
// MUD_MudSync - DRAG_STRIP (UNREVIEWED PLAN CODE, research 2026-10-19 sec 15.5)
static const bool DRAG_STRIP = false;
protected ref array<int> m_aDm = {};        // scratch per cell of the stamp box + 1: 1 = passed the stamp test
protected ref array<int> m_aDl = {};        // marked + ring, as dw-box indices, row order

// Stamp, before the row loop:
//     int dw = i1 - i0 + 3;  int dh = k1 - k0 + 3;
//     if (DRAG_STRIP) { m_aDm.Resize(dw * dh); for (...) m_aDm[z] = 0; }
// Stamp, right after the geometric test passed (before TileOf):
//     if (DRAG_STRIP) m_aDm[(k - k0 + 1) * dw + (i - i0 + 1)] = 1;
// Stamp, end: if (MUD_MudSheet.DRAG_LIM > 0) { if (DRAG_STRIP) LimitDragStrip(i0, k0, dw, dh); else LimitDrag(i0, i1, k0, k1); }

protected void LimitDragStrip(int i0, int k0, int dw, int dh)
{
	m_aDl.Clear();
	for (int y = 0; y < dh; y++)
	{
		for (int x = 0; x < dw; x++)
		{
			int c = y * dw + x;
			bool on = m_aDm[c] != 0;
			if (!on && x > 0 && m_aDm[c - 1] != 0)
				on = true;
			if (!on && x < dw - 1 && m_aDm[c + 1] != 0)
				on = true;
			if (!on && y > 0 && m_aDm[c - dw] != 0)
				on = true;
			if (!on && y < dh - 1 && m_aDm[c + dw] != 0)
				on = true;
			if (on)
				m_aDl.Insert(c);
		}
	}
	int cnt = m_aDl.Count();
	for (int sweep = 0; sweep < 2; sweep++)
	{
		for (int q = 0; q < cnt; q++)
		{
			int idx = q;
			if (sweep == 1)
				idx = cnt - 1 - q;
			int c2 = m_aDl[idx];
			int xx = c2 % dw;
			int yy = c2 / dw;
			LimitDragCell(i0 - 1 + xx, k0 - 1 + yy);   // the body of today's LimitDrag inner loop, moved into a method
		}
	}
}
```

Checks before a game run: the offline tool's two variants are the reference; a quick way to check the port is a
TEST_KIT self-test that runs one recorded op through old and new paths and prints cells changed / max diff (old vs
new are expected to differ; DS vs client vs client must not). Hash lines: re-baseline, then client A vs B must match.

### 15.6 Numbers the user needs for the un-park decision
1. Sync ms per 5 s per truck, client and DS: launch 1 vs launch 2 (A1 + A2) - target about -40 % (sec 8).
2. DS MUD ms with SERVER_LAYER off vs on.
3. ns per Math.* native (bench) -> worth doing 3.1 or not.
4. C1: does a 121-bone skinned tile import, show, cull right with BoundingBox, and what it costs in memory per tile.
5. C3 / D3: are same-.xob pieces batched; memory per 1000 pieces; spawn vs pool.
6. The user's eye: pieces + parallax (C3 / C4) vs the sheet.
With 1-2 in hand, sec 7's budget test can be run; with 4-6, the cheap-look route can be judged against it.

### 15.7 IDA D5-D8 (local session 2026-10-19; results/2026-10-19-sheet-cost-probes.md "Part D, round 2")
- D5 [IDA]: `Deactivate` on a ProcAnimComponent unregisters it from ProcAnimSystem (no per-frame eval); `Activate`
  re-registers. Design C can park pieces. No script path to `OnFrameUpdate` (prefab property only).
- D6 [IDA, medium]: script side free when bones do not change, but every DRAWN skinned object copies its bone palette
  each frame (48 B per bone); no unchanged skip found. Design B / skinned tiles are not zero per-frame cost.
- D7 [IDA, medium]: 384-bone stack buffers in the pose / bounds code; 121 bones fit. No limit string found.
- D8 [IDA]: SetObject (different object or remap) makes a new MeshObject - bone overrides lost. A + B on one piece do
  not mix.
