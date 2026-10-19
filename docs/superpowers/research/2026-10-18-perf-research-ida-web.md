# Cheaper runtime ground: IDA + web pass (2026-10-18)

Read-only research. Exe = `G:\SteamLibrary\steamapps\common\Arma Reforger Experimental\ArmaReforgerSteamDiag.exe`
(IDA base 0x140000000, same build as 2026-10-15-ida-rut-questions.md - all old addresses resolve). Builds on
2026-10-14-ida-voxel-feasibility.md, 2026-10-15-ida-rut-questions.md, 2026-10-16-custom-shaders.md,
2026-10-18-mud-mesh-cost-research-findings.md, KB runtime-procedural-mesh.md - facts there are not repeated.
Labels: IDA = read in the exe (not run), LOG = our own measured log lines, PAK = shipped script proto, WEB = URL,
INFERENCE = my reading.

## Summary (plain)

1. The engine part of a mesh rebuild is small: on the DS client 'mesh object' (Create + Update* + SetObject) is
   ~10 % of the mesh build time; the other ~90 % is our own script loops. Updating a mesh in place cannot fix that.
2. A normal (flags 0) mesh is copied to the GPU once, on its first draw, and never again - so a fresh
   MeshObject.Create per change is the only working path. There is no hidden "update" switch.
3. No heightfield collider from script (the engine has one, terrain only). CreateTriMesh is the only way; our own
   per-material triangle loop around it is a real cost we can cut.
4. Script meshes cannot carry vertex colours, so per-vertex dirt blends stay out; decals (they do show on our ground)
   and slope / terrain-distance blends in existing materials are the routes.
5. New, cheap lever: `IEntity.IsRendered()` (needs the SET_RENDERED flag) tells script whether a patch was drawn
   last frame. Visual-only rebuilds (look ruts, client rut meshes) of patches nobody sees can wait.

---

## Q1. Update a MeshObject in place instead of Create?

**Finding: No for flags 0, and DYNAMIC_VERTEXES stays broken. Create is cheap; the script loops are the cost.**

What the engine does (IDA):
- `UpdateVerts` (sub_140C38540): gets the section's CPU vertex array through sub_140AAD5E0, which marks the section
  dirty (`section+2 |= 1 | 8`) and sets mesh flag `+192 |= 0x80`. It writes positions + int16 UVs, then rebuilds
  normals (sub_140AB3270) and tangents (sub_140AB3630), then sub_140AB2E70.
- sub_140AB2E70 is the **bounding-box rebuild**: when mesh flag 0x80 is set it clears it and recomputes min/max and
  the radius over every vertex of every section. Since UpdateVerts always sets 0x80 first, bounds are always
  recomputed - the UPDATE_BOUNDBOX flag is effectively automatic. It is called with entity = null, so the ENTITY is
  not told -> that is why `ent.Update()` after SetObject is needed (matches gotcha 2026-10-09 08:26).
- **GPU upload (static path), sub_140AAD9D0**, called from the draw prep sub_140AAF5A0 for each visible LOD:
  it creates the index buffer (`lod+120`) and the vertex buffer (`lod+128`) only when they are still null, and returns
  early when `(mesh+192 & 0x10)` (dynamic) or the vertex buffer already exists. Nothing reads the dirty bits that
  UpdateVerts sets. So: **a flags-0 mesh goes to the GPU once, at its first draw; later UpdateVerts change only
  the CPU copy.** This is the code reason for the KB's "flags 0: UpdateVerts runs but is never drawn".
- After that first upload, sub_140AA8680 frees the CPU copies of a section - but returns early when the mesh flag
  has 0x4 (CUSTOM) or 0x10. Create always ORs in 4 (sub_140AA7870), so **script meshes keep their CPU arrays for
  life** (CPU + GPU memory per mesh; INFERENCE: ~28 B/vertex CPU + GPU copy, small at 1089 verts).
- Dynamic path (flag 0x10): draw prep sub_140AADE70 refills a per-frame ring buffer every frame for every visible
  section (known, 2026-10-15). No change to that verdict.
- Create (sub_140AA7870): allocates section structs, keeps `flags & 0x10 | 4`, names the LOD "custom". No GPU work,
  no world registration. GPU buffers are made lazily on the render side at first draw.

Our split (LOG, PeerPlugin1 DS client, `MUD band: mesh split ms`, per 10 s):
| Log | input | mesher | reveal + uv | mesh object (engine) | total |
|---|---|---|---|---|---|
| logs_2026-10-18_16-16-20 16:18:37 | 91 | 118 | 19 | 27 | 255 |
| logs_2026-10-18_15-37-50 15:41:35 | 73 | 99 | 23 | 13 | 209 |
| logs_2026-10-18_14-11-54 14:14:20 | 124 | 187 | 41 | 44 | 396 |
| logs_2026-10-18_13-52-21 13:56:17 | 68 | 154 | 12 | 17 | 251 |
Checksum row 1: 91 + 118 + 19 + 27 = 255 (rest 0). Engine share 27/255 = 11 %, 13/209 = 6 %, 44/396 = 11 %,
17/251 = 7 %. So even a perfect in-place update saves at most ~6-11 % of the mesh time.

Confidence: high (code + logs agree with the 2026-10-09 bench: Create ~0, UpdateIndices+UpdateVerts 0.24 ms).

Try in script / test:
- Nothing to try on flags. Spend the effort on the script part (Q4 / ranked list items 1-3).
- Optional check (5 min, WB): build a flags-0 mesh, call UpdateVerts BEFORE the first frame it is visible (spawned
  hidden / far away), then show it. INFERENCE: the change shows, because the GPU copy is made on first draw. Not
  useful for ruts (patches are visible), so only run it if a "pre-warm" idea needs it.

## Q2. Cheapest collider height update

**Finding: no heightfield or refit from script; CreateTriMesh is the only shape. Our own loop around it is the part
we can cut.**

- IDA: Bullet RTTI has `.?AVEnfHeightfieldTerrainShape@@` and `.?AVHeightfieldTerrainShapeCommon@@` (names
  "ENF_HEIGHTFIELD" / "COMMON_HEIGHTFIELD", vtable name getters sub_141077600 / sub_141077610). No code refs to the
  RTTI from any script binding. PhysicsGeom script natives (sub_140E626C0) are still only Destroy, CreateBox,
  CreateSphere, CreateCapsule, CreateCylinder, CreateTriMesh. Physics natives (sub_140E67DF0, 57 methods) re-listed:
  no RemoveGeom, no refit, no heightfield. So the terrain's heightfield collider is terrain-only.
- Known (not repeated in detail): AddGeom + switch-off costs the same as a fresh body (G3 probe); UpdateGeometries
  re-reads the .xob; CreateTriMesh copies verts + u16 indices and builds a quantized BVH (sub_140A52060).
- Our split (LOG, same DS client logs, `MUD band: collider split ms`, per 10 s): logs_2026-10-18_16-16-20 16:18:37:
  input 44, mesher 72, trimeshes 100 (196 geoms), drop 0, static 1, rest 3 (of 220) - checksum 44+72+100+0+1+3 = 220.
  'static' (CreateStaticEx) and 'drop' (Destroy) are ~0-2 %. 'trimeshes' (~45 %) is NOT only the native call: the
  timer (MUD_WorldGround.c csGeom at line 7245 .. 7371) also covers the script loop at 7276-7312, which scans **every
  triangle once per material slot** (`for k in mats { for ti in tris { if slot != k continue ... } }`) and, with a
  mud mask, computes a centre + floor() per triangle. With 3-4 slots that is 3-4 full passes.
  Native CreateTriMesh alone was 0.28 ms per 2048 tris (gotchas 2026-10-09 bench).

Confidence: high for "no heightfield / no refit" (registration lists); medium for how much of 'trimeshes' is script
(inference from the code layout; not split by a timer yet).

Try in script / test:
1. One pass bucket: walk the triangles once, append each to its slot's own index array (array per slot, or one
   scratch with per-slot offsets from a count pass). Then one CreateTriMesh per non-empty slot.
   Test: add a sub-timer around only the `PhysicsGeom.CreateTriMesh` calls inside the csGeom block; log
   'native trimesh ms' next to 'trimeshes'. Before/after on the same DS robot run (>= 6 runs per setting, per
   CLAUDE.md). Expect the script part of 'trimeshes' to drop by ~(slots-1)/slots.
2. Mud piece: precompute the per-cell mud bit per quad (both triangles of a quad share a cell) instead of a
   centre + 2 x floor per triangle.
3. Still open from older docs: smaller bodies (one per 2-4 m piece) so a rut rebuilds a smaller BVH. Only after 1.

## Q3. Per-vertex dirt blend (vertex colour / alpha)

**Finding: no vertex data path; use material-side blends or decals.**

- Re-confirmed (IDA): the script mesh has one UV set and no colour stream (vertex format 15, 28 B/vertex; see
  2026-10-14 doc). Vertex declarations with colour exist in the exe (`VDNormalUV2Color`, "UV set 1" / "UV set 2"
  strings) but Create never picks them.
- Materials that would read vertex colour: MatTerrainObject `MaterialBlendUseVertexColor` (sub_140BF3DC0) and the
  `DisableVertexColor` param (sub_140BDE2B0). Both need a colour stream we cannot give -> no use.
- What blends WITHOUT vertex data (IDA param lists):
  - **MatPBR2Layers** (sub_140BE0B30): `MaskMap` red = layer 1 vs layer 2, with its own `MaskMapUVTransform` and
    `UVSrcMaskMap`; `BCRMap2` / `NMOMap2` with own UV transforms; `HeightMap2` + `ParallaxScale2`. The mask is a
    fixed texture, so per-vertex control would only come from UVs (we have one UV set, 0..1).
  - **MatPBRMulti** (sub_140BECA20): `TopDownProjection` (Enable, Start 25, AngleEnd 75 degrees, Position, Contrast)
    blends a layer by the angle between the normal and a direction - rut WALLS can turn to dirt by slope alone.
  - **MatTerrainObject** `MaterialBlendUseTerrainDistance` - already half-tested (2026-10-16-custom-shaders.md: blend
    worked, object maps rendered white with ST_Mud_01; try 3 control never ran).
- Decals: PROVEN 2026-10-18 ~20:50 (gotchas) that World.CreateDecal DOES show on our ground although it returns null.
  From the 2026-10-15 IDA answer the decal is clipped onto the mesh it was made on and does not follow a new
  SetObject mesh (INFERENCE). So decals fit settled ground, not a patch that rebuilds every 0.1 s.
- INFERENCE, untested trick: UV-encoded blend on MatPBR2Layers - mesh UV u in two halves (0..0.5 = grass,
  0.5..1 = mud), mask texture = black left half / white right half, layer textures tiled with an even TilingU so a
  +0.5 shift lands on a whole tile. Quads with mixed vertices would smear one cell. Low odds, look-only; listed for
  completeness.

Confidence: high for "no vertex colour"; medium for TopDownProjection doing what its tooltip says on our meshes.

Try in script / test (look only, WB):
1. Rut walls: make a copy of the ground emat as MatPBRMulti with TopDownProjectionEnable on and a dirt layer; put it
   on one test patch. Look at a fresh rut: walls dirt, flat top grass? Cost 0 script.
2. Finish try 3 of the MaterialBlendUseTerrainDistance test (control) - it decides if "dirt below the old ground
   level" is free.
3. Decal on settled patches: after a patch has not rebuilt for N s, stamp a MatPBRDecal (height map + parallax) on
   it near the camera; judge by eye only (return value is null even on success).

## Q4. Batching, instancing, LOD for script meshes

**Finding: no engine batching/LOD for script meshes; the useful new tool is IsRendered.**

- IDA: Create builds one LOD (sub_140AA7870, LOD count 1). Each section = its own GPU buffers and draw (static path
  per section in sub_140AAD9D0). The "Instances / Draw calls / Ideal draw calls" stats string belongs to the TERRAIN
  object stats (sub_140BFCF00 "Terrain objects:"), not to script meshes. Each Create makes a unique resource, so
  INFERENCE: no instancing between our patches. Draw calls are not our bottleneck anyway (LOG 2026-10-18 L1 test:
  render 3.6-4.0 ms both ways; the change was in sim / MUD script).
- **IEntity.IsRendered()** (PAK scripts/Core/generated/Entities/IEntity.c: "Tells whether the Entity has been
  rendered last frame, only works when the entity has SET_RENDERED entity flag"). IDA: native sub_140E4D890 ->
  sub_1409B3F80 = `(flags+88 & 0x80) && world && (world frame - entity+92) <= 1`. Also `EOnVisible(owner,
  frameNumber)` event exists. Not used anywhere in our Scripts (grep 2026-10-18).
- `IEntity.SetFixedLOD(int)` exists; useless with one LOD.
- Batch the other way: fewer, bigger meshes would make each rut rebuild bigger. For ruts, smaller pieces win
  (rebuild cost ~ vertex count) as long as the per-entity fixed cost stays small (Create ~0, static ~0 per LOG).

Confidence: high for IsRendered semantics (proto text + code); INFERENCE that a DS / headless client always reads
false (no renderer frame) - check before using it on anything the server needs.

Try in script / test:
1. Set `EntityFlags.SET_RENDERED` on patch entities at spawn (client only). Before a VISUAL-only rebuild (look ruts,
   MUD_LookRuts; client patch mesh when the collider is built separately), skip / defer when `!ent.IsRendered()` and
   the patch is > ~15 m from the camera; keep it dirty and build when IsRendered turns true (it will draw the old
   mesh once, then the new). Colliders are never gated by this.
   Test: client log line 'look builds skipped (not rendered) N of M' per 10 s; first-person drive (ruts behind are
   off-screen) and third-person. Pass = fewer builds, user sees no late pop in front of the camera.
2. Check (WB, 2 min): Print IsRendered for a patch in view vs behind the camera, with and without SET_RENDERED, to
   prove the flag works on our GenericEntity-type patches.

## Q5. Other games / mods / custom shaders

- No new Reforger mod with runtime terrain deformation found. Closest: "Explosive Crater System" (WEB
  https://reforger.armaplatform.com/workshop/66674B40175D63DB) places crater objects; no live deformation.
- Enfusion / Arma 4: no public statement of runtime terrain deformation (WEB search 2026-10-18, nothing primary).
- Batman: Arkham Origins snow (WEB https://gdcvault.com/play/1020177/Deformable-Snow-Rendering-in-Batman, slides
  https://www.slideshare.net/colinbb/gdc2014-deformable-snow-rendering-in-batman-arkham-origins): prints rendered
  into a heightmap texture, shown by relief mapping / tessellation. Needs custom shaders + render-to-texture -> not
  for us. What carries over: footprints are LOW-frequency detail and look-only; gameplay height stays coarse.
- Rise of the Tomb Raider snow (WEB https://history.siggraph.org/learning/labs-rd-rendering-techniques-in-rise-of-the-tomb-raider):
  same GPU family. Not usable.
- MudRunner (already in 2026-10-14-mudrunner-*.md): separate physics and render data, active blocks only, tracks as a
  separate ribbon mesh. The ribbon idea fits our limits: a narrow rut ribbon entity rebuilt per 250 ms flush, the
  8 m patch only when the vehicle leaves it. INFERENCE.
- Custom shaders: nothing new. The 2026-10-16 verdict (precompiled .shadercacheDX12 only) stands; no new compiler
  strings were looked for this pass.

Confidence: low-medium (web gave little primary material on CPU-side deformation).

---

## Try next (ranked: saving x chance / effort)

| # | What | Why / evidence | Effort | Test |
|---|---|---|---|---|
| 1 | IsRendered gate on VISUAL-only rebuilds (look ruts, client look meshes), SET_RENDERED at spawn | Look ruts were ~25 builds/s x ~5.4 ms off-zone (sheet-alternatives doc sec. 9-10); L1 (look ruts off) gave MUD script 3.11 -> 1.26 ms. Gating keeps the look in view and drops the unseen part | low | WB function check (Q4 test 2), then DS client >= 6 runs: 'skipped' count + MUD prof |
| 2 | Collider: one-pass triangle bucketing per material slot; sub-timer for native CreateTriMesh | 'trimeshes' ~45 % of collider ms; loop scans all tris once per slot (MUD_WorldGround.c 7276-7312, same shape at ~6848 path to check) | low | split log 'native vs script' before / after, same robot run |
| 3 | Rut ribbon: a separate small mesh for the newest rut stretch; full patch rebuild only when the vehicle leaves / on a slower timer | Engine share is ~10 %, so the only big win is fewer vertices per build; a 1-2 m ribbon is ~10-20x fewer verts than 1089 | medium-high | look check in WB; client mesh ms per 10 s on DS |
| 4 | Smaller visual pieces (2-4 m sub-entities) for rut-heavy areas | same reason as 3; Create / SetObject / static per entity ~0 by LOG | medium | count builds x verts; light seams at piece borders (per-section normals, 2026-10-14 doc) |
| 5 | MatPBRMulti TopDownProjection for rut-wall dirt; finish MaterialBlendUseTerrainDistance try 3 | removes geometry tricks for the dirt look | low (emat only) | WB look only |
| 6 | Decals on settled patches only | proven visible on our ground; lost on SetObject (inference) | low-medium | WB by eye |
| - | Not worth it: DYNAMIC_VERTEXES, UPDATE_BOUNDBOX, in-place UpdateVerts, AddGeom patching, heightfield shape | IDA above + G3 probe | - | - |

## Evidence index (IDA addresses this pass)
- sub_140C38540 UpdateVerts native; sub_140AAD5E0 vertex lock (sets dirty + mesh 0x80); sub_140AB2E70 bounds rebuild.
- sub_140AA7870 Create core (flags & 0x10 | 4, LOD name "custom").
- sub_140AAF5A0 draw prep -> sub_140AAD9D0 lazy GPU IB/VB create (once) -> sub_140AA8680 CPU free (skipped for 4/0x10).
- sub_140AADE70 dynamic per-frame copy (known).
- sub_140E626C0 PhysicsGeom natives, sub_140E67DF0 Physics natives (no heightfield / RemoveGeom / refit).
- RTTI .?AVEnfHeightfieldTerrainShape@@ 0x142455ee0; name getters sub_141077600 / sub_141077610.
- sub_140BE0B30 MatPBR2Layers params, sub_140BECA20 MatPBRMulti params, sub_140BF3DC0 MatTerrainObject params.
- sub_140E05050 IEntity natives (SetFixedLOD sub_140E4DCB0, IsRendered sub_140E4D890 -> sub_1409B3F80).
- sub_140BFCF00 terrain object draw stats ("Instances / Draw calls / Ideal draw calls").
