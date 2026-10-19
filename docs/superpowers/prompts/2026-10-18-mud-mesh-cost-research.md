# Research prompt: cheaper runtime mud / rut meshes in Arma Reforger (Enfusion, Enforce Script)

Copy everything below the line into the research AI.

---

You are helping with a multiplayer Arma Reforger mod (Enfusion engine, Enforce Script) that makes deformable mud:
vehicles leave ruts that every player sees the same. I need research, not code: find real techniques, engine
facts and sources that can cut the script cost of building the mud meshes. Cite sources (docs, forum posts, GitHub
code, talks, papers). Mark each claim as "documented", "seen in real code" or "your inference".

## How the system works now

- Mud is drawn as script-built meshes ("tiles"): each tile is 3 x 3 m, a grid of 21 x 21 points (0.15 m cells,
  400 quads, up to ~1000 vertices with edge cuts and a side skirt). Built with MeshObject.Create +
  UpdateIndices + UpdateVerts, then SetObject on an entity.
- Under the mud there are ground patches (8 x 8 m, 33 x 33 vertices), also script-built meshes with colliders.
- A live 4 x 4 tile window follows the vehicle; tiles that leave it are stored. Ruts are synced as a shared,
  deterministic data layer (same result on every machine), then each client builds its own meshes from it.
- A tile under a wheel rebuilds at every 0.1 s upload; other changed tiles at most every 0.25 s.

## Measured cost (client, driving 5-15 m/s, ~74 fps, frame ~13.5 ms)

- Engine simulation 6.5 ms, engine render 4.3 ms, mud script ~4 ms (30 % of the frame).
- Mud script split (ms per frame): mud sheet tiles 1.5-1.7 (wheel tile builds 0.5, wheel tracking 0.25-0.45,
  stored tile rebuilds 0.3), ground patches made ahead of the vehicle 0.6-0.8, ground patch rebuilds 0.4-0.7
  (mesh 0.1-0.25, collider 0.2-0.3), shared sync maths 0.4-0.5, quick client ruts 0.15-0.2.
- One tile build ~1 ms: quad loop 0.5, setup 0.2, engine mesh calls 0.12, skirt 0.03. The engine calls are cheap
  (MeshObject.Create ~0 ms, UpdateIndices + UpdateVerts 0.24 ms per 1089 vertices); the Enforce Script loops are
  the cost.
- Script timer: System.GetTickCount only (whole ms).

## Hard limits we already proved (please don't suggest these unless you have proof they work)

- No custom shaders in mods (so no GPU vertex displacement / tessellation of our own).
- No in-place vertex updates: a mesh created with dynamic-vertex flags gives broken normals; a flags-0
  MeshObject never redraws later UpdateVerts. So every change = new mesh for the whole tile.
- No runtime terrain height API (cannot deform the real terrain).
- Enforce Script runs in a VM: loops, array reads and function calls are slow; a method's locals must fit an
  80 KB VM stack; member static bool arrays are broken; string.Format max 9 values.
- The shared sync maths must stay deterministic in script (same on every machine).

## What I want researched

1. Enfusion / Enforce Script facts that could help:
   - Any way to update part of a mesh, or many small meshes cheaply (sub-meshes, mesh sections, partial index
     updates, instancing, shared MeshObject between entities).
   - Any engine-side helpers that do bulk array / vector maths natively (anything that moves a loop out of script).
   - Script threading / jobs / async (anything that runs script off the main thread), and its limits.
   - Decals, procedural decals, track / tire-mark systems, terrain layer painting at runtime, "SCR_" or
     vanilla systems (vehicle tracks, craters, footprints) we could reuse for the look.
   - How Bohemia's own vehicle wheel tracks / skid marks / dust are drawn in Reforger.
   - Known Enforce Script performance tips (what is fast / slow in the VM: static vs dynamic arrays, function
     call cost, inlining, vector vs float maths, foreach vs for).
2. How other games do cheap deformable mud/snow ruts without custom shaders or terrain edits:
   - MudRunner / SnowRunner, BeamNG, Spintires, Red Dead Redemption 2, Horizon, Death Stranding, Battlefield,
     Arma 3 mods. Focus on CPU mesh approaches, LOD, "only rebuild what changed", ring buffers of rut strips,
     trail ribbons, decal stacks, height-map-to-mesh tricks.
3. Mesh-building tricks that cut CPU work:
   - Level of detail by camera distance (fewer points far away) and seamless joins between LODs.
   - Smaller "dirty rectangles" (rebuild only a small strip), strip/ribbon meshes along the wheel path instead of
     grid tiles, premade rut pieces stitched in, caching precomputed index lists.
   - Cheaper smooth edges between mud and untouched ground.
4. Anything that cuts the "patches made ahead of the vehicle" and "patch collider rebuild" costs (physics trimesh
   rebuild cost in Enfusion, heightfield colliders, fewer collider updates).

## How to answer

- Rank ideas by (expected ms saved) x (chance it works in Enfusion without shaders / terrain API).
- For each: what it is, source, how it would fit the setup above, risks, and how to test it quickly.
- Say clearly when something is from another engine (Unity / Unreal / Godot) and may not exist in Enfusion.
- Point to Enfusion API class / method names where possible (e.g. MeshObject, Physics, PhysicsGeom, DecalEntity,
  ShapeEntity) and to the Bohemia wiki (community.bistudio.com), Enfusion script docs, the Reforger modding
  Discord / forums, and public GitHub mods.
