# IDA check: what the engine allows for the voxel papers plan

Date 2026-10-14. Read-only IDA pass for plan docs/superpowers/plans/2026-10-14-voxel-papers-plan.md.
Binary: IDA database `ArmaReforgerSteamDiag.exe` (image base 0x140000000). The KB addresses from 2026-10-07
(runtime-procedural-mesh.md: sub_140AB3270, sub_140AB3630, sub_140AA7870, sub_140C35D80, sub_1409A0F00) all
resolve to the same functions here, so this is the same build the KB was made on (Experimental 192624 - inferred
from the matching addresses, not from a version string).
Cross-check: enfusion-mcp `api_search` (pak API dump) for MeshObject, PhysicsGeom, Physics, ShapeUtils, PhysicsUtils.
Labels: PROVEN-IDA = read from the decompiled code; INFERENCE = my reading, not tested in game.

## Script bindings (the whole surface)

- `MeshObject` natives are registered in sub_140C35020: `Create` (sub_140C35D80), `UpdateVerts` (sub_140C38540),
  `UpdateIndices` (sub_140C38320), `GetNumGeoms`, `HasValidMask`, `HasLayerMask`. Nothing else. api_search agrees
  (the extra methods it lists - GetMaterials, ToMeshObject... - come from VObject).
- `PhysicsGeom` natives (sub_140E626C0): `Destroy`, `CreateBox`, `CreateSphere`, `CreateCapsule`, `CreateCylinder`,
  `CreateTriMesh` (sub_140E65BF0). No update / refit call.
- `Physics` natives (sub_140E67DF0, 57 methods) include `AddGeom` (sub_140E63590), `GetGeom`, `GetNumGeoms`,
  `SetGeomInteractionLayer`, `UpdateGeometries` (sub_140E69E50). There is no `RemoveGeom` string in the binary.

---

## 1. Script mesh normals - can we keep our own? **No.** Shared at seams? **No.**

Evidence (PROVEN-IDA):
- `UpdateVerts(meshIdx, verts[], uv[])` (sub_140C38540) copies positions (16-byte slots) and UV0 (packed int16),
  then always calls sub_140AB3270 (normals) and sub_140AB3630 (tangents), then sub_140AB2E70. No flag skips it,
  and no binding takes a normal array.
- sub_140AB3270 runs only when vertex-format bit 2 is set; `Create` always sets the format byte to 15
  (sub_140AA5D80: `*(geom+1) = 15` when no skinning), so it always runs.
- How it builds them: for every triangle in THIS mesh section's current index buffer, face normal =
  cross product, **normalised** (sub_1409F6E60), and added to each of its 3 vertices. Then each vertex sum is
  normalised and packed 11/11/10 bits. So:
  - every triangle has weight 1, whatever its size or corner angle (not area-weighted);
  - a zero-area triangle adds nothing (sub_1409F6E60 returns the zero vector unchanged when length^2 <= 1.18e-38);
  - only triangles of the same section count. Neighbour bricks are other MeshObjects, so seam vertices never see
    the other brick's faces. The same is true for our two sections in one brick (top and dirt, MakeRes
    MUD_VoxelProbe.c ~3703): a vertex on the top/dirt border gets one normal in each section.
- Normals are computed from the index buffer present when UpdateVerts runs. Our order (UpdateIndices first, then
  UpdateVerts, MUD_VoxelProbe.c ~3720) is correct; the other order gives normals from the old triangles.

What this changes:
- Plan F3 idea "zero-area copies of the neighbour ring's quads" **cannot work**: zero-area triangles add nothing.
- A variant that could work (INFERENCE, needs a probe): copies that are tiny but not zero (e.g. the neighbour
  ring's triangles shrunk to ~1 mm around the shared seam vertex). They count with full weight 1 because of the
  equal weighting. Costs: extra indices; they must NOT go into the collider (today the collider uses the same
  m_IbTop / m_IbDirt arrays, CreateTriMesh at ~3750).
- Step B (DC vertex): DC makes more thin slivers near sharp edges. Because every triangle counts the same, slivers
  pull vertex normals as hard as big faces (INFERENCE: shading on DC edges will look harder / noisier than the
  shape suggests). The B2 note "Normals for UVs stay as now" is fine; there is no way to supply DC normals.
- The top/dirt split also makes a light seam along the material border inside a brick (INFERENCE from the code;
  not looked for in game). F2 (more materials = more sections) multiplies these borders.

## 2. Vertex colours, second UV set, extra streams - **No.**

Evidence (PROVEN-IDA):
- `Create` -> sub_140AA6190 -> sub_140AA5D80 per section, called with bone/weight counts 0. With no skinning the
  section gets format byte 15 and 28 bytes per vertex; the UV-set count byte (+38) is set to 1. Buffers allocated:
  positions (16 B/vert), UV (4 B/vert = one int16 pair), indices (u16), normals (4 B packed), one 12-byte/vert
  helper array. No colour buffer and no second UV buffer are ever allocated on this path.
- `UpdateVerts` writes positions and UV set 0 only (`uv*65534-32767`, clamped -> UVs must be 0..1, as in the KB).
- The only other stream the struct supports is skinning (bone indices / weights, format 31), and the script path
  passes 0 for it.

Can an emat read them? There is nothing to read: an emat expecting vertex colour or "UV set 2" (e.g. MatPBRMulti
global maps, gotchas line ~243) gets nothing from our meshes. INFERENCE: such a material either falls back to
defaults or shows wrong; not tested.

What this changes:
- F4 (triplanar / texture blend by vertex weight): no per-vertex blend weights are possible. Only blends the
  shader does by itself (world position / normal / slope, e.g. a MatTerrainObject-style angle blend found in A4)
  are usable.
- F2 (material per voxel): must be done by section (one material per section), not by vertex data.

## 3. Several materials per script mesh - **Yes, one per section; no hard limit found.**

Evidence (PROVEN-IDA):
- `Create(numMeshes, numVerts[], numIndices[], materials[], flags)` builds ONE LOD with numMeshes sections
  (sub_140AA7870 with LOD count 1; section count stored as u16 at LOD+148). Section i gets material slot i
  (sub_140AA6190 is passed an identity array 0..n-1).
- Materials are added by sub_140AB53A0, which **de-duplicates by name**: the same path twice returns the earlier
  slot and adds none. Sections still point at slot i. So with two sections on the same material, section 1 points
  at a slot that was never filled.
  INFERENCE (strong): this is why the 2026-10-10 "skirt" second section with the SAME material crashed Workbench
  (gotchas line 16). Rule: every section of one MeshObject needs a different material path (a copy of the .emat
  is enough). Our current 2-section brick uses MAT_TOP + MAT_DIRT (different) and works.
- Each section has its own vertex buffer of its own size: a vertex used by two sections is stored twice. Each
  section also gets its own normals (see 1).
- No limit check seen besides the u16 section count. Cost per section (INFERENCE, not measured): one more draw
  call and one more buffer set; plus seams in lighting at section borders.
- Per section, vertex count is u16 (65535 max) and indices are u16: UpdateIndices (sub_140C38320) copies only the
  low 16 bits of each int, so an index >= 65536 silently wraps.
- Bound checks: UpdateIndices refuses meshIdx >= count. UpdateVerts returns only when `count < meshIdx`, so
  `meshIdx == count` gets through (off by one) - PROVEN-IDA that the check is `<`; what happens next is not traced.
  Neither copies with a check of the script array length: they read numVerts / numIndices entries as given at
  Create (INFERENCE: too-short script arrays read garbage).

What this changes: F2 is possible with a section per material, each with its own distinct emat path. Budget the
duplicated border vertices and the extra lighting seams.

## 4. Physics trimesh - partial / in-place update? **No.** Degenerate triangles? **Kept, not filtered.**

Evidence (PROVEN-IDA):
- `CreateTriMesh(verts[], indices[], numVerts, numIndices)` (sub_140E65BF0) calls PhysicsManager vtable+256 =
  BulletManager sub_140A52060. That allocates `enf::BulletTriangleMeshShape` (a btStridingMeshInterface,
  sub_140A4E520), **copies** the vertices (12 B) and the indices as **u16** (low 16 bits of each int; same 65535
  vertex limit as the render mesh), then builds a Bullet `btBvhTriangleMeshShape` with a quantized
  `btOptimizedBvh` (sub_14172BBC0; RTTI strings btBvhTriangleMeshShape / btOptimizedBvh). The script arrays can be
  reused right after.
- No triangle is checked or dropped on the way in: every index triple is copied as is. So degenerate (zero-area)
  and folded triangles go into the collider.
- No binding exposes Bullet's refit / partialRefitTree. The only in-place tools on a body:
  - `Physics.AddGeom(name, geom, frame, material, layer)` (sub_140E63320): adds a geom to an existing body. It
    refuses (returns -1, logs) for an entity with no world and for bodies with merged physics.
  - `Physics.SetGeomInteractionLayer(index, mask)`: can switch an old geom off (mask 0). No RemoveGeom exists, so
    switched-off geoms stay in the body (INFERENCE: memory and broadphase cost grow per edit).
  - `Physics.UpdateGeometries(and, or)` (sub_140B179C0) re-reads geoms from the entity's VObject. A script
    MeshObject carries no collision geometry (KB), so INFERENCE: useless for us.
- No `btTriangleInfoMap` / internal-edge string in the binary. INFERENCE: no internal-edge smoothing, so wheels can
  bump on edges between triangles; that is the same today.
- What Bullet does with a zero-area triangle: NOT traced here. From Bullet's design (INFERENCE): contact normals on
  such a triangle are unreliable; sliver triangles make thin contacts that objects can slip through.

What this changes for Step B: a DC collider must filter itself - drop triangles with area below a small epsilon
(and those whose face normal flips against the surface-nets normal) BEFORE CreateTriMesh. The render mesh can keep
them (zero area adds nothing to normals, see 1). A partial collider update is possible only as "AddGeom a new
piece + switch the old piece off", and is unproven in cost.

## 5. Anything we missed - **No native helpers found.**

- No script binding for marching cubes, surface nets, voxels, SDFs, normal smoothing or mesh simplification.
  The "Voxel" strings in the binary belong to occlusion culling (VoxelTerrainOcclusionMethod,
  RenderVoxelDepthBuffer) and the "SDF" strings to fonts / UI (SDFFont, MainShapeSDF*). PROVEN-IDA (strings) +
  api_search "voxel" / "normals" return nothing relevant.
- `ShapeUtils.CreateShapeFromMesh(ByIndex)` turns a MeshObject into a debug Shape - debug only.
- `MeshObjectVolumeCalculator.GetVolumeFromVisualMesh / GetVolumeFromColliders` exists (api_search) - could
  measure dug volume, not needed now.
- Background building: MeshObject Create / UpdateVerts and CreateTriMesh run synchronously in the calling script
  (no job/async path in the bindings above). Strings `thread` and `Sleep` exist in the script compiler area
  (sub_140B5FED0) - INFERENCE: Enforce's `thread` keyword is a coroutine on the main thread, so it can spread
  work over frames (as our sub-block budget already does) but not run it in parallel. Unresolved: not traced.
- Unexplored, possibly useful for looks: the `Decal` script class (registered, not read here) - a decal over the
  crater could blend textures without vertex data. Not checked.

---

## Which plan steps change

| Plan step | Before | After this check |
|---|---|---|
| A1 / item 2 (brick-edge light) | look in game | Confirmed by code: seams WILL have different normals (per-section normals). A1 only decides if it is visible. Also look at the top/dirt border inside a brick. |
| B1 (DC proto) | measure folds | Add: count sliver triangles (they pull normals with full weight) and tris below an area epsilon. |
| B2 (DC in game) | "watch collider faces folding" | Add a collider filter: drop near-zero-area / flipped triangles before CreateTriMesh (engine keeps them). No DC normals possible - engine rebuilds. |
| C (smoothing pass) | unchanged | Unchanged; smoothing the field is the only way to smooth normals. |
| D (lazy JIP queues) | unchanged | Unchanged (no async build path to lean on). |
| E (finer cells) | measure cost | Add: stay under 65535 verts per section (u16 indices, silent wrap) - fine at 8^3 sub-blocks. |
| F2 (material per voxel) | one section per material | Confirmed; each section needs its own DISTINCT emat path (same path twice = unfilled slot, likely the skirt crash). Border verts duplicated, extra light seams. |
| F3 (brick-edge normal fix) | zero-area copies | Zero-area copies do nothing. Only tiny non-zero copies could work; keep them out of the collider. Probe before use. |
| F4 (triplanar blend) | blend by vertex | No colours / UV2 on script meshes: only a material that blends by itself (world / normal / slope) can work. |
