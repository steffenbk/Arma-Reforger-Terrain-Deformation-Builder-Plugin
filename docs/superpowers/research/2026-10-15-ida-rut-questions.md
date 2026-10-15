# IDA questions for the rut work (2026-10-15)

Things only the exe can answer, for docs/superpowers/research/2026-10-15-rut-alternatives-deep-dive.md (rut field,
decals, grass) and the displaced-ruts build. Ranked by how much the answer changes our plan.
Write answers into this file under each item with labels IDA (read in the exe, not tested) / PROVEN (tried in play),
function addresses, and the exe build. Use the same exe as the earlier notes (Diag / Tools Workbench, Exp 192624 or
newer - say which) - research/2026-10-14-ida-voxel-feasibility.md has the known addresses (UpdateVerts
sub_140C38540, normal builder sub_140AB3270, CreateTriMesh sub_140A52060 ...).

Paste to the local Claude with IDA:
> Read docs/superpowers/research/2026-10-15-ida-rut-questions.md and answer items 1-10 in order with IDA. No mod code
> changes. Write the answers into that file under each item. Stop after item 5 and report.

---
Answers below: exe = ArmaReforgerSteamDiag.exe 1.9.0.10 (Experimental, file date 2026-09-24), IDA base 0x140000000.
IDA = read in the exe, not tested in play. PROVEN = tried in play. INFERENCE = my reading, not confirmed.

## 1. Track decals on OUR meshes (decides: decal ruts, option 3)
Script calls: `World.CreateTrackDecal(entity, pos, normal, size, lifetime, material, prev, alpha)` (used in the
spray-can mod, works on hit entities), `TrackDecalFactory(material, width, lifetime)` + `AddPoint(entity, pos,
normal, alpha)` + `FinalizeDecal(fade)` (vanilla SCR_VehicleTrackDecal), and `World.CreateDecal` (our 2026-10-12
try: returned a decal, nothing visible - mud-gotchas line ~228).
- How is the decal drawn: projected box / deferred decal (works on any surface in the box), or a mesh built by
  clipping the TARGET entity's triangles (needs that entity's CPU-side mesh)?
- What does it need from `entity`: a VObject with a mesh? a physics body? terrain only? Does a MeshObject.Create
  mesh (MUD_VoxelSubEntity, MUD_FinePatchEntity) qualify?
- What happens when the entity's mesh is replaced (`SetObject(newMesh)` - every brick / patch rebuild)? Decal kept,
  re-projected, dropped, or dangling (crash risk)?
- Limits: max decals / track segments (pool size), lifetime, per-frame cost; anything material-side that refuses
  decals (an emat flag like "receive decals" - check MatTerrainObject and the DirtCover_01 class).
- World.CreateDecal: parameter list and why ours showed nothing (size / stretch 0 / near-far clip?).

### Answer 1 (IDA, 2026-10-15)
There are two separate decal systems.

**a) TrackDecalFactory (the vanilla SCR_VehicleTrackDecal path): terrain only, so it cannot draw on our meshes.**
- Script API (sub_140B7CAF0): HasActiveDecal sub_140B80170, AddPoint sub_140B7E0A0, FinalizeDecal sub_140B7E930.
- AddPoint (entity, pos, normal, alpha) checks: factory valid, pos valid, normal not zero, alpha 0..1. It then calls
  sub_140B1C020, which starts a new decal when there is none yet or the entity changed.
- A new decal starts only when `entity == the world's terrain entity` (sub_140B1ED70 returns 0 otherwise). So on
  MUD_VoxelSubEntity / MUD_FinePatchEntity no track decal ever starts.
- The strip is built by clipping against the terrain HEIGHTMAP (sub_140B20280: u16 heights on the terrain cell
  grid). Over our hole cells it follows the original heightmap, not our dug ground. A non-terrain branch exists (a flat
  quad strip 0.05 m along the normal) but decal creation never reaches it.
- Limits: a segment over 3000 indices (0xBB8) or 1500 verts (0x5DC) ends the decal. A decal over 3000 indices in
  total starts a new one.
- `World.CreateTrackDecal` does not exist in this exe: no such string. Only the TrackDecalFactory API.

**b) World.CreateDecal: works on MeshObject entities, so it is the one to try on ours.**
- Registered in sub_141479A30 (only in builds with script module flag 0x10). CreateDecal is sub_14147A6A0, and there
  is also CreateDecal2 sub_14147A1B0.
- Parameters: (entity, origin, project, nearclip, farclip, angle, size, stretch, materialName, lifetime, color,
  atlasNum).
  - Inner function sub_14147A420 loads the material and logs "Unable to load material" when that fails.
  - Lifetime <= 0 means permanent.
  - The rotation comes from the normalised `project` vector.
  - The `angle` argument is not stored anywhere I could find (INFERENCE: unused).
- **How it is drawn: a mesh decal, built by clipping the TARGET's own triangles. It is not a projected box.**
  sub_14147A970:
  1. If the entity has a VObject (entity+80), call VObject vtable+304 ("accepts decals?").
  2. Call vtable+312 ("build decal") on each child entity whose bounds overlap, then on the entity itself.
  3. With no VObject, it uses the terrain decal path instead (sub_141063B20).
- **The MeshObject class (enf::MeshObject, vtable 0x141ca3cb0) accepts decals:**
  - Slot +304 = sub_140185760, which always returns 1.
  - Slot +312 = sub_140AA57F0 builds the decal from the mesh: up to 5 LODs, each mesh section in turn.
  - A section is skipped when:
    - its flag 0x400 is set (INFERENCE: the mesh param "NoDecals");
    - its material has flag +51 bit 0 set (INFERENCE: the material param "NoDecals", "Do not stick decals on it",
      a base material checkbox registered in sub_14094B8D0, default 0);
    - it has more than 65536 elements;
    - its bounds miss the decal box.
  - It then clips the section's triangles under a lock (sub_140A47790 / sub_140B20030).
  - RiverMeshObject uses the same two slots.
  - INFERENCE: script MeshObject.Create meshes are this same class, so our bricks and patches qualify. Not tested.
- **Dedupe:** sub_140B22290 checks for an existing decal with the same material closer than
  min((size/2)^2, 0.1) (squared distance). If there is one, it only refreshes that decal's lifetime and builds no
  new one. So repeated stamps at nearly the same spot cost little.
- **Why our 2026-10-12 try showed nothing:**
  - We passed stretch 0. The engine's own DecalEntity attributes (sub_14147EBC0) default to Stretch 1
    (range 0.01-100), FarPlane 0.5 (range 0.01-10), Lifetime 0 = permanent, TerrainOnly, NearestOnly, and
    Mat (class MatPBRDecal).
  - The ENTS|WORLD trace hit GenericTerrainEntity, so the decal went onto the terrain, not our mesh.
  - Next try (INFERENCE): stretch 1, nearclip about 0, farclip 0.5-1, project = -normal, a MatPBRDecal material,
    and the entity = our brick/patch entity itself.
- **When the mesh is replaced (SetObject on rebuild):** the decal list hangs off the mesh section data of the
  VObject (sub_140A47790 is looked up from entity+80, which is the VObject). INFERENCE: when we swap in a new mesh the
  decals stay with the OLD mesh object and drop off visually. They are not re-projected onto the new mesh. I found no
  code path that moves them. Crash risk not checked. So decals on bricks only suit ground that is no longer being
  rebuilt.
- **Pool size / per-frame cost: not found.** There is a warning "Quite suspicious number of created decals for %s"
  in sub_14147A970.
- **MatTerrainObject / DirtCover_01:** the receiver-side switch is the generic NoDecals material/mesh checkbox
  above. I did not open DirtCover_01's emat.

## 2. Grass clutter on our ground (decides: free "grass gone in the track" look)
PROVEN (mud-gotchas ~75): clutter shows on our ground over hole cells if it sits >= 1 mm above the heightmap, and
mortar craters in it come out clean (no grass in the pit). INFERENCE: planted via "ObjectsHeightMap::Build" (log).
- How is clutter placed on a non-terrain mesh: traces against entities? a height map built from them? Which layer /
  flag makes our entity count (Terrain preset? MatTerrainObject?)
- When is it rebuilt after our mesh changes (per region? on a timer? on entity bounds change)? Cost?
- Can a rut clear grass by itself: slope threshold (MatTerrainObject has ClutterThresholdAngle), height below
  something, a material without clutter (the `_disabledclutter` emat variant)? If a "touched" quad in a 2nd section
  with the disabledclutter emat removes grass there, that is a whole-map track look for free.

### Answer 2 (IDA, 2026-10-15)
- **Correction to the earlier INFERENCE:** "ObjectsHeightMap::Build" (sub_1410D2210) is the FAR SHADOW object
  heightmap, not grass.
  - It is built once at world load over the whole terrain at 2048 x 2048 by sub_140DB2420, which logs
    "Initializing objects heightmap ...".
  - Its MeshObject component params are "Bias for object heightmap creation (far shadow)" and
    ObjectHeightmapTraceBias.
  - It takes entities with flags 0x400 and 0x1 that have a MeshObject VObject (sub_1410DA240).
  - It has nothing to do with clutter.
- **Grass placement:** clutter is placed and drawn on the GPU from the terrain:
  - compute shaders terrain/cs_clutter_prepare and cs_clutter_process (MatClutterPrepare / MatClutterProcess);
  - "Terrain '%s' tile ... grass clutter data" per terrain tile;
  - a "clutter mask" pass (ClutterMaskDepthRT, MatClutterMaskPostprocess "computing heightmap discontinuities").

  INFERENCE: grass sits on the terrain heightmap and terrain clutter layers. It does not trace our mesh.
  - That fits the PROVEN 2026-10 notes: grass shows over our ground, and none grows on hole cells (no terrain there).
  - "Shows if >= 1 mm above the heightmap" fits too: the grass is at heightmap height and pokes out of our mesh only
    when our surface is not above it.
- **What removes grass:**
  1. **Terrain holes.** We already use these.
  2. **ClutterOccluder**, a checkbox on the prefab MeshObject component (sub_140A47D00): "Enables clutter
     occlusion", default 0, plus ClutterOccluderClipPlane (default 1.5) and ClutterOccluderEnlarge (default 0.3,
     "Intensity of clutter mask discontinuity detection"). It sets render flag 0x4000 (sub_140AA69A0).
     - Our runtime entities have no MeshObject component, so they are NOT occluders.
     - INFERENCE: an entity spawned from a prefab whose MeshObject component has ClutterOccluder on, and then given our
       mesh with SetObject, might keep the flag and cut grass under its whole footprint. Not tested.
     - That cuts all grass under the brick, not along the track.
  3. **ClutterThresholdAngle** ("removing clutter on slopes higher than this value") is a TERRAIN material param
     (sub_140BF3DC0). INFERENCE: it reads the terrain heightmap normal, so steep walls on our mesh do not affect it.
- **No "disabledclutter" string in the exe.** So a `_disabledclutter` emat is a data convention, i.e. a terrain
  surface without a clutter set. INFERENCE: it would work only by painting terrain layers, and script cannot paint
  terrain layers (item 10).
- **Rebuild timing / cost of clutter:** GPU, per frame around the camera ("PrepareClutter job"). Nothing CPU-side
  to rebuild when our mesh changes.
- **Diag menu to look with:** Render > "Grass/Clutter/Obstacles" with values none, obstacles, obstacles depth,
  clutter depth, clutter mask, discontinuity mask. There are also "Render clutter" sub-items (stats, Enable clutter
  mask, Show clutter occldrs).
- **Verdict:** the free "grass gone in the track" look is not available from the clutter system. Flattening is
  (item 3).

## 3. AffectFoliage* (decides: pressing grass along tracks)
READ (API docs): GenericTerrainEntity.AffectFoliageCircle / Ellipse / Rect / Square(..., flatten, sway, strength,
falloff) "submit an obstacle in this frame".
- Obstacle buffer size per frame, cost per call, does a flattened blade stay down after the call stops (decay
  time) or spring back next frame?
- Does it act on clutter planted on OUR ground (item 2) or only on terrain clutter?
If it decays slowly, re-submitting recent tracks near the camera every frame could look persistent.

### Answer 3 (IDA, 2026-10-15)
- **Registration:** sub_141449370 (GenericTerrainEntity):
  - AffectFoliageEllipse sub_141449CE0, Circle sub_141449990, Rect sub_14144A0B0, Square sub_14144A480.
  - All of them go to one obstacle add, sub_140B65200.
- **What a call does:** it appends one 20-byte obstacle to a list under a lock, so it is cheap (no GPU work per
  call). The obstacle stores:
  - position x/z plus a 16-bit height;
  - radius x and z (packed as *8, max 255, so about 32 m);
  - angle;
  - strength, falloff and direction bytes;
  - flags (flatten / sway).
- **Limits:**
  - Max 1000 obstacles. Past that it logs "ObstacleMask: Adding over a 1000 obstacles!" once and drops the rest.
  - Calls with both flatten and sway false are ignored.
  - Obstacles outside the obstacle mask area are ignored. That area is a circle around the mask centre (INFERENCE:
    the camera), and the height must be inside the mask's vertical range.
  - Nothing happens while the global obstacle switch is off.
- **Decay:** the obstacle mask has a history buffer (ObstacleRTBack / ObstacleDepthBack). The Render diag menu
  "Enable obstacles > Force system params" lists these defaults (range min / max / default / step):
  - FlattenOnset 0.4 (0.1-100);
  - FlattenLying 10.0;
  - FlattenRecovery 5.0;
  - SwayLength 4.0.

  INFERENCE (units are seconds): flattened grass bends down in about 0.4 s, stays down about 10 s after the last
  submit, then recovers over about 5 s. It does NOT spring back next frame.
- **Persistent tracks:** re-submitting each recent track segment near the camera every few seconds (well under
  10 s) keeps it flat. Mind the 1000 per frame cap and the mask radius.
- **Our ground:** there is one terrain clutter system (item 2), so the grass over our ground is the same grass, and
  the grass material's ObstacleFlatten / ObstacleSway / ObstacleFlatteningAffection params decide whether it reacts.
  The obstacle height must lie in the mask's range; pass our ground height. Not tested.

## 4. Own vertex normals (decides: sharp-looking ruts without extra triangles, and seam-free shading)
Known (IDA 2026-10-14/15): UpdateVerts always calls the normal builder (sub_140AB3270), which returns early unless
bit 2 of the section's format byte is set; Create sets that byte to 15.
- What sets the format byte: can any MeshObject.Create flag (CUSTOM = 4, DUMMY_TANGENTS = 32, others) or another
  creation path give a format WITHOUT bit 2 (no rebuild) plus a way to write normals?
- Any script-reachable native that writes normals / a second vertex stream (search the MeshObject / Mesh / VObject
  script registrations again in the current exe - new methods since 192624?).
- If yes: normals from the fine rut field on the coarse mesh = rut walls that LOOK sharp at 0.5 m, plus no light
  seams at brick / patch / section borders (we could average across them).

### Answer 4 (IDA, 2026-10-15)
- **MeshObject script API** in this exe (sub_140C35020): Create sub_140C35D80, UpdateVerts sub_140C38540,
  UpdateIndices sub_140C38320, GetNumGeoms, HasValidMask, HasLayerMask. That is all. There is no normals, tangents
  or second-stream method.
- **MeshObjectFlags** (sub_1409A0F00): CUSTOM 4, DYNAMIC_VERTEXES 16, DUMMY_TANGENTS 32, UPDATE_BOUNDBOX 128.
  - Create passes the flags to sub_140AA7870, which keeps only `flags & 0x10` (DYNAMIC_VERTEXES) and always ORs
    in 4 (CUSTOM).
  - No flag reaches the section format byte, and DUMMY_TANGENTS / UPDATE_BOUNDBOX are not used on this path.
- **UpdateVerts(mesh, section, verts, uvs):**
  1. It writes the positions.
  2. It writes the UVs packed to signed 16-bit as (uv*65534 - 32767), clamped. So **UVs must be within 0..1**;
     outside that range they clamp.
  3. It then always runs the normal builder sub_140AB3270. That builder runs only when bit 1 (value 2) of the
     section byte at +1 is set; the earlier "bit 2" note meant this bit.
  4. Then the tangent builder sub_140AB3630, which runs when the same byte has bit 3 (value 8).
  5. Then the upload, sub_140AB2E70.
- **Verdict:** no script path to own normals. Sharp-looking ruts need real geometry. Light seams at brick borders
  can only be reduced by making the shared border vertices have the same neighbouring triangles, e.g. a 1-cell
  overlap skirt. That is INFERENCE, untested.

## 5. Wheel contact shape (decides: how a 0.25 m rut will feel, jitter risk)
Known: suspension = one batched ray per wheel from the hub, length radius + suspension (sinking-deep-dive,
sub_14056FCD0), layer VehicleCast.
- One ray per wheel, or several across the tyre width / a shape cast? If one centre ray: a wheel half in a sharp rut
  either drops fully or not at all -> step changes = jitter. Is there any width / multi-ray / normal smoothing?
- Which contact normal is used: the hit triangle's face normal? Smoothed? (sharper rut walls = tilted normals)
- Same questions for the SA simulation (VehicleWheeledSimulation_SA - Bohemia moved vehicles to server authority;
  check which class our test vehicles use in the current build).

### Answer 5 (IDA, 2026-10-15)
- **One ray per wheel.** Wheel cast setup sub_14056FCD0 (callers sub_14056A170, sub_14056E1A0):
  - For each wheel it builds ONE segment, from the hub, along the suspension axis, length = radius + suspension
    travel (80-byte entries: start xyz, end xyz, wheel ptr, length).
  - The batch runs in sub_1408FA5D0. For each candidate body it does a Bullet single ray test (sub_14171CBE0,
    callback gamecode::vhc::ResultCallback) and keeps the closest hit fraction.
  - There is no shape cast, no extra rays across the tyre width, and no averaging over several hits.
  - A wheel half over a sharp rut wall therefore drops fully or not at all, so expect step changes at rut edges.
- **Contact normal:** the raw normal Bullet returns for the hit, just normalised (sub_1408FA5D0: hit normal / its
  length, stored per wheel). On a trimesh that is the hit TRIANGLE's face normal. I found no smoothing there.
  Sharper walls give more tilted normals, so more sideways push when the ray lands on a wall triangle.
- **Surface props from the hit:** sub_1408FBD20 adds surface roughness / noise from the hit material, scaled by
  speed. That is not geometry smoothing.
- **SA simulation:** the server-authority class in this exe is VehicleWheeledSimulation_SA_B (strings at
  0x141c31848, class registration sub_1407526E0..sub_140754610). Both wheel sims reach the same wheel-cast core
  sub_14056E1A0, called from sub_14074EB30 and from sub_140755710 next to the SA_B code. INFERENCE (from address
  proximity): SA_B uses the same one-ray contact.
- **Which sim our test vehicles use:** not checked here. Check the vehicle prefab's simulation component in
  Workbench.
- **Ideas for less jitter (INFERENCE):** round the rut wall top and bottom over at least 1 cell, or keep wall slopes
  under about 45 degrees where wheels run. Debug view: Vehicles > Show raycast / Show contacts.

## 5b. Follow-ups from answers 1-5 (ask with items 6-10)
- 1-bis: IDA found no `CreateTrackDecal` string, but steffenbk's spray-can mod calls `world.CreateTrackDecal(...)`.
  Is it bound under another name / in a script proto (search the World script registrations and the game script
  pak)? Or was it removed before 1.9?
- 1-ter: is World.CreateDecal (script module flag 0x10) registered in the GAME client and dedicated server, or only in
  Workbench / Diag builds?
- 2-bis: the clutter "discontinuity mask" (MatClutterMaskPostprocess, ClutterMaskDepthRT): what depth difference
  removes grass (threshold, ClutterOccluderEnlarge link)? Decides whether ruts of 0.1-0.35 m lose their grass.

## 6. Per-triangle material on a trimesh (decides: mud / rut friction per spot without extra geoms)
- CreateTriMesh backend (sub_140A52060 -> Bullet): any per-triangle material index (btTriangleIndexVertexMaterialArray
  style), or one material per geom (PhysicsGeomDef)?
- WheelGetContactMaterial / surface props from a hit: per geom or per triangle?

## 7. Trimesh internal edges (decides: whether split ruts add bumps for bodies / characters)
- Is internal-edge info built for static trimeshes from CreateTriMesh (btGenerateInternalEdgeInfo /
  btAdjustInternalEdgeContacts or an engine equivalent)? If not, contacts at triangle edges can push "up" from
  inside edges - more, smaller triangles = more of it. Wheels use rays (item 5), so this is mainly vehicle bellies,
  characters and debris.

## 8. Partial collider updates (decides: cheaper rebuilds for the rut field)
Known: no refit; Physics.AddGeom(name, geom, frame, material, layer) exists (HANDOFF 2026-10-08).
- Is there a RemoveGeom / geom replace on an existing static body? Cost of AddGeom on a static body (broadphase
  update) vs destroy + CreateStaticEx?
- If cheap: one body per 8 m patch with 4 x 4 sub-geoms of 2 m, and a rut rebuilds only the sub-geoms it touched.

## 9. Mud drag from the surface (decides: MudRunner drag with no script forces)
Known: ContactSurface (.vhcsurf) has RollingResistance / RollingDrag fields; whether the sim reads them was not
found (sinking-deep-dive PROBE 4); tyre .conf RollingResistance says "NOT USED ATM!".
- Does VehicleWheeledSimulation(_SA) read the surface RollingResistance / RollingDrag (or anything else from the
  .vhcsurf besides friction / roughness / noise)? If yes, a "mud" gamemat on rutted / wet ground gives drag for free.

## 10. Runtime terrain height edit - re-check on the current build
Known (IDA 2026-10-08/12): GenericTerrainEntity script API read-only + AffectFoliage*; height / hole edits are
Workbench TerrainToolDesc_* only.
- List the script registrations of GenericTerrainEntity / terrain classes in the CURRENT exe. Anything new
  (height write, hole toggle, layer paint)? Five minutes - a yes would change the whole architecture.

## 11-13 (added 2026-10-15 late, from research/2026-10-15-rut-options-round2.md)
- 11. Runtime spline / road / river mesh: can a road / shape / spline entity (RoadEntity, ShapeEntity, generator
  entities, RiverEntity) be spawned and given points at runtime and generate its mesh in the GAME (not Workbench)?
  Which script API reaches it?
- 12. String / class sweep for deformation features: "deform", "footprint", "snow", "mud", "displacement",
  "tessellat", "track" (non-decal), "SetHeight", "HeightmapEdit", "PaintLayer", "TerrainModif". For each hit: what it
  is, reachable from script?
- 13. Vanilla track decals on our map: does SCR_VehicleTrackDecal's terrain path draw on hole cells (our ground over a
  hole) or skip them? (terrain hole test in the decal clip, sub_140B20280 area)
