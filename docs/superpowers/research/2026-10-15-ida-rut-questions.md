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

## 2. Grass clutter on our ground (decides: free "grass gone in the track" look)
PROVEN (mud-gotchas ~75): clutter shows on our ground over hole cells if it sits >= 1 mm above the heightmap, and
mortar craters in it come out clean (no grass in the pit). INFERENCE: planted via "ObjectsHeightMap::Build" (log).
- How is clutter placed on a non-terrain mesh: traces against entities? a height map built from them? Which layer /
  flag makes our entity count (Terrain preset? MatTerrainObject?)
- When is it rebuilt after our mesh changes (per region? on a timer? on entity bounds change)? Cost?
- Can a rut clear grass by itself: slope threshold (MatTerrainObject has ClutterThresholdAngle), height below
  something, a material without clutter (the `_disabledclutter` emat variant)? If a "touched" quad in a 2nd section
  with the disabledclutter emat removes grass there, that is a whole-map track look for free.

## 3. AffectFoliage* (decides: pressing grass along tracks)
READ (API docs): GenericTerrainEntity.AffectFoliageCircle / Ellipse / Rect / Square(..., flatten, sway, strength,
falloff) "submit an obstacle in this frame".
- Obstacle buffer size per frame, cost per call, does a flattened blade stay down after the call stops (decay
  time) or spring back next frame?
- Does it act on clutter planted on OUR ground (item 2) or only on terrain clutter?
If it decays slowly, re-submitting recent tracks near the camera every frame could look persistent.

## 4. Own vertex normals (decides: sharp-looking ruts without extra triangles, and seam-free shading)
Known (IDA 2026-10-14/15): UpdateVerts always calls the normal builder (sub_140AB3270), which returns early unless
bit 2 of the section's format byte is set; Create sets that byte to 15.
- What sets the format byte: can any MeshObject.Create flag (CUSTOM = 4, DUMMY_TANGENTS = 32, others) or another
  creation path give a format WITHOUT bit 2 (no rebuild) plus a way to write normals?
- Any script-reachable native that writes normals / a second vertex stream (search the MeshObject / Mesh / VObject
  script registrations again in the current exe - new methods since 192624?).
- If yes: normals from the fine rut field on the coarse mesh = rut walls that LOOK sharp at 0.5 m, plus no light
  seams at brick / patch / section borders (we could average across them).

## 5. Wheel contact shape (decides: how a 0.25 m rut will feel, jitter risk)
Known: suspension = one batched ray per wheel from the hub, length radius + suspension (sinking-deep-dive,
sub_14056FCD0), layer VehicleCast.
- One ray per wheel, or several across the tyre width / a shape cast? If one centre ray: a wheel half in a sharp rut
  either drops fully or not at all -> step changes = jitter. Is there any width / multi-ray / normal smoothing?
- Which contact normal is used: the hit triangle's face normal? Smoothed? (sharper rut walls = tilted normals)
- Same questions for the SA simulation (VehicleWheeledSimulation_SA - Bohemia moved vehicles to server authority;
  check which class our test vehicles use in the current build).

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
