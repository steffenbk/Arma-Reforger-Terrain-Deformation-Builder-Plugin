# Mud / rut mesh cost: research findings

Arma Reforger (Enfusion, Enforce Script). Answer to `2026-10-18-mud-mesh-cost-research.md`.

Labels: **[doc]** = documented by Bohemia or the source, **[code]** = seen in real code, **[inf]** = my inference.

## Main point

The biggest saving probably comes from changing what each rebuild recomputes, not from the "no in-place updates" limit. Your own numbers point there: the engine mesh calls cost about 0.12 ms, and the script loops cost about 0.7 ms per tile. Making a new mesh object each time costs very little. Rebuilding every vertex, UV and index in script each time is what's expensive.

## Ranked ideas (expected ms saved × chance it works)

### 1. Precompute everything that doesn't change, and keep per-tile vertex arrays alive between rebuilds
**~0.4–0.6 ms, high chance**

- In a 21×21 grid, the X/Z positions, the UVs and the index list are the same on every rebuild. Only the heights change. Build those once as static arrays, keep one vertex array per live tile, and on each upload rewrite only the Y values inside the dirty rectangle. Then pass the whole array to a fresh mesh. **[inf]**
- Bohemia describes static arrays as extremely fast compared with dynamic `array<T>`; static arrays have a fixed size, dynamic ones can be resized. **[doc]** 16 live tiles × 1089 vectors is about 200 KB of members. That's fine as long as these are class members and not locals on the 80 KB VM stack. **[inf]**
- Edge cuts: instead of building a new index list for each cut pattern, keep the topology fixed and collapse cut quads into zero-area triangles, or pin their vertices under the terrain. Then the index buffer is a single shared constant. **[inf]**
- Avoid helper calls inside the quad loop. The compiler only inlines a method when the class or method is `sealed` and the body is a single return statement. **[doc]** Anything bigger is a full VM call per quad, so inline it by hand.
- Cache `Count()` outside loops; the BI wiki flags calling it every iteration as waste and notes `foreach` is technically still faster than `for`. **[doc]**
- **Test:** time 100 builds in a loop between two `System.GetTickCount` calls to get sub-millisecond resolution per build. Compare the old and new builder on identical rut data. **[inf]**

### 2. Use a lower-resolution collider than the visual mesh, and gate its rebuilds
**~0.2–0.4 ms, high chance**

- The script API has no heightfield collider. `PhysicsGeom` only exposes `CreateBox`, `CreateSphere`, `CreateCapsule`, `CreateCylinder` and `CreateTriMesh`. **[doc]** A trimesh is the only option. A public gist builds collision exactly this way: `PhysicsGeom.CreateTriMesh` from verts/indices with a dirt physmat, then `Physics.CreateStaticEx`. **[code]**
- MudRunner separates the two explicitly: its mud physics and mud rendering run on completely different data sets, partly because rendering detail isn't needed for physics. **[doc]**
- For your patches: a 17×17 or 9×9 collider instead of 33×33 is 4–13× fewer triangles. Wheels with ~0.4 m radius don't resolve 0.25 m bumps anyway. **[inf]**
- Use a shared constant index array, rebuild only when the maximum height change since the last collider exceeds a threshold (e.g. 2–3 cm), and create colliders only within X m of a vehicle. **[inf]**
- **Risk:** wheels sinking through or floating above a visibly deeper rut. Tune the threshold against what players notice.

### 3. Lower resolution by distance for stored tiles and ahead-of-vehicle patches
**~0.3–0.6 ms, medium-high chance**

- Halving resolution (0.15 → 0.30 m) cuts vertices about 4×. Your existing side skirt is the usual way to hide cracks between resolution levels, so the joins come almost for free. **[inf]**
- MudRunner uses the same principle: levels are split into 16 m blocks, only blocks near the player's truck run mud simulation, and the 128×128 mud texture is only allocated for blocks that contain mud. **[doc]**
- **Test:** force the low-resolution path on all stored tiles and compare frame time and visual pop at 20–40 m.

### 4. Smaller wheel tiles or a dirty strip
**~0.2–0.35 ms of the 0.5 ms wheel-build cost, medium chance**

- At 15 m/s the wheel moves 1.5 m per 0.1 s upload, and the rut is maybe 0.3–0.5 m wide. Most of a 3×3 m tile is unchanged on each wheel rebuild. **[inf]** With item 1 in place you already skip recomputing those cells. Splitting further into 1.5 m tiles also cuts the per-build vertex push and the Create/Update cost.
- **Downside:** the 0.2 ms fixed setup per build gets paid more often, so do item 1 first. If setup is still large afterwards, this doesn't pay.

### 5. Detail from decals or skid marks, so the mesh can drop to 0.3 m cells
**Potentially the largest saving, medium chance (depends on the look)**

- Script decals exist: `World.CreateDecal` / `CreateDecal2` project a mark onto an entity; with lifetime ≤ 0 the decal is static and returns a handle you can remove with `BaseWorld.RemoveDecal`. **[doc]** You already use these in your Vehicle Track System.
- Parallax without custom shaders: Reforger's Height texture type drives a parallax effect that emulates per-pixel tessellation on decal and terrain surfaces. **[doc]** A PBRDecal material with a height map could carry tread depth the mesh no longer needs to model. **[inf]**
- Vanilla since 1.9: `SCR_DustPerWheel` moved from script to game code as `VehicleWheelEffectComponent`, and now supports skid marks, including static tire effects on soft surfaces like mud without sliding. Bohemia advises against static effects on lower-end platforms for performance reasons. **[doc]** Because it runs in game code it costs no script time, but it isn't deterministic across clients the way your data layer is.
- MudRunner splits it this way too: wheel tracks are a separate mesh of trapezes following each wheel's path, drawn after the terrain with a parallax pass. **[doc]** Their parallax is custom shader work, so only the split carries over, not the technique.

### 6. A per-frame build budget
**0 ms saved on average, but removes spikes; high chance**

- The BI performance guide's own remedies are to spread work over multiple frames and run it less often, caching results in between. **[doc]**
- Put all tile and patch builds in a single queue with a budget of about 0.5 ms per frame, wheel tiles first. That helps your 1% lows more than your average. **[inf]**

### 7. One cheap long-shot test on mesh flags
**Low chance, low payoff once item 1 is done**

- `MeshObjectFlags` has `CUSTOM`, `DYNAMIC_VERTEXES` (a hint for dynamic hardware buffers), `DUMMY_TANGENTS`, and `UPDATE_BOUNDBOX` (marks the bound box dirty). **[doc]**
- If you only tested `DYNAMIC_VERTEXES` on its own, try it combined with `DUMMY_TANGENTS` and `UPDATE_BOUNDBOX`. Broken normals could be stale tangent data, and "never redraws" could be a stale bounding box causing culling. **[inf]**
- Five minutes to test. Once item 1 is in, in-place updates would only save the ~0.12 ms engine calls, so don't spend more time than that on it.

## Dead ends

- **Threads:** the `thread` keyword creates a script thread from a method, and the wiki's example uses `Sleep` inside it. **[doc]** That makes it a coroutine for spreading work over time, almost certainly on the main thread, not a worker. It won't take load off the frame. **[inf]** The only async engine operation found is `BaseWorld.AsyncTraceMove`, which runs a trace asynchronously with a callback. **[doc]** Traces only.
- **Native bulk vector maths:** nothing found in the public API that moves a per-vertex loop into native code.
- **Partial mesh updates:** `MeshObject.Create` takes a sub-mesh count with per-sub-mesh vertex/index counts, and `UpdateVerts` / `UpdateIndices` take a `meshIdx`. **[doc]** So one MeshObject can hold several sub-meshes. Given that flags-0 meshes never redraw after `UpdateVerts`, this is unlikely to give partial in-place updates, and it matters little once item 1 is done. **[inf]**
- **Other games:** no CPU-mesh write-ups found for BeamNG, RDR2, Death Stranding, Horizon or Battlefield; they are generally understood to use GPU displacement or tessellation, which you can't do. **[inf]** MudRunner is the only source with real detail, and its mud surface is also drawn by a vertex shader from a texture the CPU draws primitives into. **[doc]** Its structure carries over (separate physics and render data, active blocks only, ribbon tracks); its rendering method doesn't.

## Suggested order

1. Item 1, then item 2, then item 3. Together they target roughly 1–1.5 ms of the ~4 ms with low risk.
2. Re-measure before trying item 4 or 5, because item 1 changes how much item 4 is worth.

## Open question

Why are ground patches built ahead of the vehicle every frame instead of drawn from a pre-warmed pool? At 0.6–0.8 ms it's the single largest line item. If those patches are mostly flat terrain copies until a wheel touches them, building them lazily on first contact could remove most of that cost.

## Sources

- [Enfusion Script API: MeshObject](https://community.bistudio.com/wikidata/external-data/arma-reforger/EnfusionScriptAPIPublic/interfaceMeshObject.html)
- [Enfusion Script API: Visual (MeshObjectFlags)](https://community.bistudio.com/wikidata/external-data/arma-reforger/EnfusionScriptAPIPublic/group__Visual.html)
- [Enfusion Script API: PhysicsGeom](https://community.bistudio.com/wikidata/external-data/arma-reforger/EnfusionScriptAPIPublic/interfacePhysicsGeom.html)
- [Enfusion Script API: World (CreateDecal)](https://community.bistudio.com/wikidata/external-data/arma-reforger/EnfusionScriptAPIPublic/interfaceWorld.html)
- [Enfusion Script API: BaseWorld](https://community.bistudio.com/wikidata/external-data/arma-reforger/EnfusionScriptAPIPublic/interfaceBaseWorld.html)
- [Arma Reforger: Scripting Performance](https://community.bistudio.com/wiki/Arma_Reforger:Scripting:_Performance)
- [Arma Reforger: Scripting Example](https://community.bistudio.com/wiki/Arma_Reforger:Scripting_Example)
- [Arma Reforger: Scripting Keywords](https://community.bistudio.com/wiki/Arma_Reforger:Scripting:_Keywords)
- [Arma Reforger: From SQF to Enforce Script](https://community.bistudio.com/wiki/Arma_Reforger:From_SQF_to_Enforce_Script)
- [Arma Reforger: Textures](https://community.bistudio.com/wiki/Arma_Reforger:Textures)
- [Changes and Improvements for Scripting in 1.1](https://reforger.armaplatform.com/news/modding-update-scripting-1-1)
- [Changes and Improvements in 1.9](https://reforger.armaplatform.com/news/modding-update-sept-24-2026)
- [The Mud of MudRunner](https://www.mudrunnermods.com/the-mud-of-mudrunner/)
- [Enfusion trimesh physics gist](https://gist.github.com/KeganHollern/1aec10cde9d4339dbad4010540867268)

## Review against proven facts (Claude, 2026-10-18)

- Item 1: partly does not fit. UVs are NOT constant (texture drag: tuv uses m_aOu / m_aOv, MUD_MudSheet.BuildTileMesh)
  and the index list changes (untraced / bare / voxel-hole quads skipped, SMOOTH_EDGE cuts). Member arrays fed to
  UpdateVerts crashed twice (code comment 'the member holder MUD_MudSheetTile crashed in UpdateVerts twice').
  Still useful (inference): cache the constant x / z and the per-cell hash noise, hand-inline Bare() / small helpers
  in the vy and quad loops, time 100 builds in one GetTickCount pair.
- Item 2: plausible for 'rb collider' (0.2-0.3 ms) but the feel rut lives in the patch collider (crude cut ~0.1 m at
  0.25 m cells); a 0.5 m collider loses it. Height-change threshold gating = worth a test.
- Item 3 (LOD for stored tiles): agrees with our own plan (brainstorm doc option E).
- Item 5: VehicleWheelEffectComponent skid marks (1.9) = unverified here; check the pak before relying on it.
- Item 6: budgets already exist (STORED_MS, MayRun / COST_* in MUD_WorldGround).
- Item 7: dead - mud-gotchas 2026-10-15 [IDA]: DYNAMIC_VERTEXES cannot fix the sheet cost.
- Open question (patches ahead): offroad the real terrain is holed (TERRAIN RULE), so a patch (mesh + collider) must
  exist before a wheel arrives or the vehicle falls through - lazy-on-contact is not possible; cheaper making is.

## Paper tests built (2026-10-18, v204 = before, NOT COMPILED, each behind its own switch)

- Test 1 (item 1): MUD_MudSheet `s_bFastBuild` (true) = BuildTileMesh fast path: bare flags per vertex once
  (bareV[441], Bare() inlined), fade skipped on tiles with no open side, untouched cells skip the smooth maths.
  Old path kept. TEST KIT bench every BENCH_EVERY_S 30 s: BENCH_N 16 old + 16 fast builds of one touched live tile,
  same data -> 'MUD bench (PAPER TEST 1)' line (old / fast ms, 'same quads' must say 1). The bench frame is a
  ~20-30 ms hitch on purpose (inference: 32 builds x ~1 ms) and lands in the MUD ms / t-part numbers.
- Test 2 (item 3): `LOD_ON` (true): a STORED tile built while the camera is > LOD_M 20 m from it gets a 0.3 m
  mesh (every 2nd vertex, own coarse skirt); when the camera comes within LOD_M - LOD_BACK (17 m) it is queued for a
  full rebuild (LodUpgrade, skipped while no tile is low). Tiles stored near the camera stay full - so low builds
  come mostly from far rebuilds (zone load, blasts, other vehicles' ruts). 'MUD lod 10 s (PAPER TEST 2)' line.
- Test 3 (item 2, threshold gating only - no lower-res collider, it would lose the feel rut): MUD_WorldGround
  `COL_SKIP_SMALL` (true): a rebuild-queue band collider build is skipped while no vertex moved COL_SMALL_M 0.02 m
  and the last real build is < COL_SMALL_MAX_S 1 s old; the patch stays dirty and retries every collider interval.
  Saves the band build + body swap only (the vertex loop still runs on each try). Risk: server and client bodies
  can differ up to 2 cm for up to 1 s -> watch 'MUD corr' snaps. Count in the 'MUD world: budget' line.
- Check 4 (item 5): VehicleWheelEffectComponent is in the game (wheel / body mud and dust setters); its skid-mark
  config is not checked yet. Possible later use: mud on the truck body. Not deterministic, look only.

### DS 16:16 results (client PeerPlugin1 logs_2026-10-18_16-16-20, ~2 min, 3-9.6 m/s; 0 script errors both sides)

- Test 1 bench, 3 runs x 16 + 16 builds, same quads 1 every run (645 / 566 / 515 quads): old 19 / 17 / 17 ms,
  fast 16 / 14 / 12 ms -> 1.10 vs 0.88 ms per build (-20 %).
- Test 2 lod: low builds 5-121 per 10 s once driving, back to full 0-33 per 10 s, 7 -> 137 low of 100 -> 591 stored.
- Test 3: small collider skips 0-4 per 10 s on client and server = almost nothing to save (gate rarely true).
- Snaps ('MUD corr: ... mine 1'): 0 (15:47 run: 2 in ~9 min). 'visual only' lines 181.
- Client 'MUD prof 5 s' driving (fps / MUD ms): 75/3.5 82/4.5 66/6.9 60/7.4 86/3.8 93/2.6 93/3.1 88/3.0 80/2.9
  87/2.6 78/3.8 77/4.2 88/4.1 72/4.4 85/3.7 75/4.3 75/4.7 88/3.0. 15:47 driving: 81/3.8 61/5.8 71/6.3 57/7.6
  77/5.4 73/6.4 69/5.3 86/4.3 71/3.6 68/3.0 ... 79/4.8 65/6.6 64/6.6. Sheet delay avg 82 ms (~0.45 m behind).
- User: better, but tiles still pop in and there is a height step between driven and not driven ground.

### Step / pop cause and fix (2026-10-18, v205 = before, NOT COMPILED)

- Proven from the code: untouched sheet top = G + LIFT + THICK = G + 0.16 (fade 1: HasTile = TileInZone, true on all
  offroad); A2 lifted patch = G + THICK - PATCH_GAP = G + 0.12 -> a 4 cm step at every sheet edge. LiftAt dropped a
  vertex by its own tile only, so on the low x / z side of a drawn tile one patch quad (<= 0.25 m) sloped down
  outside the sheet edge (a groove).
- Inference (not measured): the pop = order. The patch unlift (sync tile came / tile entered the live window) and
  the sheet tile build run in separate queues; patch first = 16 cm hole lip, then the tile pops in higher. A tile
  leaving the window was hidden at once and its patch re-lifted later.
- Fix LIFT_BY_DRAWN (MUD_MudSheet, switch): patch unlifted only under tiles whose sheet mesh is on screen (flag set
  in BuildTile after the mesh is up, moves with the slot / store in Follow, Freeze, Unpack); DropMesh keeps the mesh
  visible until it is deleted 1 s later and tells the patch first; lift = G + 0.15 (LIFT_UNDER 0.01); LiftAt drops
  a vertex only when every tile within one patch cell is drawn. LiftDirtyWindow and the two MudSync lift calls are
  off under it (fewer patch rebuilds). Log: 'LIFT BY DRAWN 1: patch tells N' on the 'MUD lod 10 s' line.
- Risk: a far tile dropped and wanted again within 1 s draws twice for a moment (z-fight, far away).
