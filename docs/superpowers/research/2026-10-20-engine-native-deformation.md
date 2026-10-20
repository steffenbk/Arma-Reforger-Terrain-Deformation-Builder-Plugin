# Deforming mud without per-vertex script: physics, bones, render targets (research, 2026-10-20)

User 2026-10-20: "more out of the box thinking ... other ways to deform without scripts, like a plane that worked like
ragdolls, or planes with bones and colliders that can react ... we are almost trying to make a volumetric design here,
I've seen someone do it with smokes."

Every route we have tried so far builds vertices in script: the sheet, D1, D2 and the patches. That is the cost wall.
This doc looks for routes where the ENGINE moves the shape (physics, skinning, GPU), and script only starts things or
feeds them a few numbers.

Sources:
- API dump in enfusion-mcp-BK (`data/api/enfusion-classes.json`, `arma-classes.json`), labelled [API];
- the offline wiki copy (`data/wiki/pages.json`), labelled [WIKI];
- our own IDA and gotchas notes, labelled [repo];
- web searches, labelled [WEB].
Anything not tested is INFERENCE. Nothing in this doc has been run in game yet.

## 0. Short answer
- **The engine does have the parts.**
  - [API] Ragdolls can be created on any entity that has an ANIMATED (skinned) mesh:
    `PhysicsRagdoll.CreateRagdoll(owner, ragdollDefName, mass, layerMask)`, then `GetBoneRigidBody(i)`,
    `SetActive`, `Enable`/`Disable`.
  - [API] Spring joints: `Physics6DOFSpringJoint.SetSpring(axis, stiffness, damping)` and `SetLimit` /
    `SetLinearLimits`.
  - [API] Bodies can run without gravity and with damping: `Physics.EnableGravity(false)`, `SetDamping`,
    `SetSleepingTreshold`, `SetActive`.
  - [API] A UI canvas can be drawn into a texture that a WORLD MATERIAL samples:
    `RTTextureWidget.SetRenderTarget(ent)` ("render resource which can be referenced in material as `$rendertarget`")
    and `SetRenderView(ent, cameraIndex)` (`$renderview`). The canvas draws lines, polygons and triangle meshes
    (`CanvasWidget.SetDrawCommands`: LineDrawCommand, PolygonDrawCommand, TriMeshDrawCommand), with an optional
    texture as the brush.
  - [API] Cameras can be orthographic (`SetOrthoLinear`).
- **What is NOT there:**
  - No soft body or cloth physics for script. Physics is Bullet [repo IDA 2026-10-13: `enf::BulletWorld`], and Bullet
    has soft bodies, but the API dump has no class for them. The "Cloth" classes in the dump are clothing items
    (loadout), not cloth physics.
  - No runtime terrain height edits. `WorldEditorAPI.ModifyHeightMap` is a Workbench editor API.
  - No custom shaders.
  - "Volumetric smoke" in Reforger is particles: billboard quads that give a "volumetric feel" [WIKI Particle
    Editor]. A search found no true volume smoke mod [WEB]. Particles cannot hold a shape, so they do not carry over
    to mud.
- **The ideas worth probing, best chance first:**
  - **N1, a deformer attached to the wheel.** A premade groove plus bow-wave mesh that moves with each wheel, with
    pieces dropped behind it. No live rebuild under the eye at all.
  - **N2, painting ruts into a texture.** Wheel tracks drawn as 2D strokes into a render target, which a material on
    the mud surface reads. The GPU shows wet tracks and fake depth. With N3 "shells" (stacked alpha-cut layers) it
    could even give real-looking volume from a texture. This has the biggest payoff, because it scales to the whole
    map on the GPU, but it carries the most unknowns.
  - **N4 / N5, a physics mud mat (your ragdoll idea).** Small bodies that wheel proxies push and that hold their new
    place: no gravity, damped, jointed. The engine (ragdoll) or a few SetBone calls carry the result onto a skinned
    surface.
  - **N6, granular mud.** Many small bodies that pile up into ridges and bow waves. It is truly volumetric but likely
    too costly. Probe it for fun.

## 1. Routes

### N1 - Wheel-attached deformer + dropped pieces (highest chance)
- **Idea.** What the eye watches is the mud right at the tyre: the bow wave, the groove forming, the ridge rolling
  out. Make that a premade mesh that rides with each wheel:
  - a skinned or morph .xob of a groove with a bow wave and side ridges, sunk into the ground;
  - attached with `IEntity.AddChild(child, pivot)` [API] to a NON-spinning node: the suspension or hub bone, or the
    vehicle root with an offset updated from `WheelGetPosition` (one SetTransform per wheel per frame).
  - Its "size" (bow wave height, groove depth) is driven by speed and load through a ProcAnim signal
    (`SetSignalValue`, one call per change [repo research 2026-10-19 sec 13]) or `Animation.SetMorphState` [API].
- **The trail.** Behind the wheel, static rut pieces are dropped (R5 in the plan: variant swap, zero per-frame cost),
  or the sheet's stored tiles are built at the slow stored pace, because the live look is now carried by the
  attached mesh.
- **Cost.** One transform per wheel per frame, plus a signal now and then. No live window rebuild at 10 Hz. The
  measured cost N1 removes is the sheet's 'sh upload' at 0.4-0.84 ms/frame.
- **Risks:**
  - finding a non-spinning node to attach to (vehicle bone names);
  - the mesh clipping into the ground on slopes (sink it; skin 4 bones to follow the ground);
  - the hand-off seam from the moving mesh to the trail.
- **Probe P-N1.** Attach a box to each wheel of a Ural, using (a) the hub/suspension bone and (b) the root plus
  `WheelGetPosition`. Does it follow without spinning or lag? Then drive one ProcAnim signal from speed.

### N2 - Painting the ruts into a render target (GPU look, scales)
- **Idea.** Per 64 m chunk near the camera, a `CanvasWidget` inside an `RTTextureWidget` holds the chunk's wheel
  tracks as 2D commands (one LineDrawCommand per wheel track, width = tyre, a soft brush texture).
  `SetRenderTarget(ent)` hands the result to the chunk's mud surface entity as `$rendertarget` [API]. The material
  then uses it as:
  - (a) a wet / dark mud mask: colour and roughness where wheels went;
  - (b) a normal or height map, which fakes depth and ridges.
- **Accumulation.**
  - Retained canvas commands: adding a point to a polyline is the only script work, and the GPU redraws every
    command in each refresh.
  - `RenderTargetWidget.SetRefresh(period, offset)` / `SetMaxFPS` limit how often it redraws.
  - `SetClearColor(false, ...)` together with `SetBlendMode` MIGHT keep old pixels, which would make it a true paint
    buffer (only new strokes drawn). The enum values exist in `RenderTargetWidgetBlendMode.c`, but are not in the dump.
- **The snow-trail trick.** `SetRenderView(ent, cameraIndex)` (`$renderview`) with an orthographic camera
  (`SetOrthoLinear`) looking UP at the wheels from below the ground. Rendered into a non-clearing min-blend target, the
  GPU would stamp the wheels' imprint by itself, with no script stamps at all. This is the classic snow deformation
  method. Whether the camera can see the wheels from below the ground, and whether the blend modes allow min, are
  both unknown.
- **Hard limits:**
  - [WIKI Textures] the Height map is "used for parallax effect ... of decal or terrain surface (currently no usage
    for model shaders)". Our earlier IDA notes found `ParallaxScale` on MatPBRBasic [repo IDA 2026-10-15 item 12], so
    whether parallax works on a model material is open.
  - Even with parallax it is fake depth: no silhouette. The rut edge seen from the side stays flat unless N3 is used
    or the real mesh does the big shape.
- **Why it is worth it anyway.** A texture is per-pixel detail at almost no script cost. It could carry everything the
  0.15 m grid was carrying (tread, smear, wet edges) on a coarse 0.30-0.5 m mesh (R3), or on the voxel / patch ground
  as a decal-like overlay.
- **Probe P-N2.**
  1. A script MeshObject plane with an emat that references `$rendertarget` in the colour slot, then in the normal
     slot, then in the height slot. An RTTextureWidget plus CanvasWidget draws one moving line. Does it show? Does it
     update?
  2. Print `RenderTargetWidgetBlendMode` and `RenderTargetWidgetFormat` (typename enum to string), then try a
     non-clearing target.
  3. An orthographic camera with `SetRenderView`: can it see a wheel from below?

### N3 - Shell layers driven by the painted texture (fake volume, needs N2)
- **Idea.** The fur and grass trick: 6-10 stacked thin planes over a mud area, 2-3 cm apart, built once per chunk as
  static script meshes. Each layer is alpha-tested against the N2 height texture with its own cutoff, one emat per
  layer. Where the texture says "pressed 9 cm", the top three layers are cut away, so you see a groove with
  terraced walls. Ridges are the extra layers above the base. That gives a real silhouette and real volume from a 2D
  texture, and the GPU does all of it.
- **Cost.** Overdraw of N alpha-tested layers (GPU), static meshes, no per-frame script beyond N2's strokes.
- **Risks:**
  - which material classes take `$rendertarget` as opacity with an alpha-test cutoff;
  - the terrace steps showing;
  - grass rides on any layer above the heightmap (gotchas 2026-10-19);
  - shadows.
- **Probe P-N3** (after P-N2 passes): 6 layers over 4 x 4 m, one painted line.

### N4 - Physics mud mat, joints (your "plane with bones and colliders")
- **Idea.** A grid of small dynamic bodies over the mud area, about 0.25-0.3 m apart, 10 x 10 per 3 m tile. Each
  body:
  - has gravity OFF (`EnableGravity(false)`) and high linear damping (`SetDamping`), so a push moves it and it then
    STAYS there. That gives plastic, mud-like deformation with no spring-back, from plain rigid-body physics.
  - has a joint to a static anchor (`PhysicsJoint` 6DOF or slider [API]) that limits it to vertical travel between
    -THICK and +RIDGE_MAX. An optional weak spring (`Physics6DOFSpringJoint.SetSpring`) gives slow recovery or
    settling.
- **Who pushes.** NOT the real vehicle (that would push back on the truck and put physics into MP). Each wheel gets a
  client-only kinematic proxy shape (a cylinder the size of the tyre) on a collision layer that only the mud bodies
  collide with. The truck's physics never sees the mud. That makes it MP-safe and look only.
- **Drawing.**
  - (a) the cheapest probe: each body carries a small mud-blob mesh (scales or overlapping discs);
  - (b) a skinned grid .xob whose bones script sets from the body positions, only for bodies that moved: ~10-30
    SetBone calls per wheel per frame, not ~1300-2600 natives per tile rebuild [repo research sec 9].
- **Sleep.** Bodies far from any wheel proxy go `SetActive(inactive)`, so only the bodies around the wheels simulate.
- **Risks:**
  - Bullet cost per active body (unknown; DS client ms);
  - stability at 100-300 active bodies;
  - the skinned grid's per-frame bone palette (48 B per bone for each drawn skinned object [repo IDA D6]). It is fine
    for a few live tiles, too much for hundreds, so bake to a static mesh when a tile goes quiet, as in R6.
- **Probe P-N4.** 10 x 10 boxes over 3 x 3 m, gravity off, damped, vertical limit joints. A kinematic sphere is
  moved through on a fixed path. Do they press and stay? Do they jitter? Do they sleep? Then the same with the
  sphere on the wheel of a driven truck, on its own layer.

### N5 - Physics mud mat as a real ragdoll (the engine binds bodies to bones)
- **Idea.** The same mat, but as a RAGDOLL. A skinned grid .xob with a skeleton plus a ragdoll definition (one body
  per bone, joints between neighbouring bones). `PhysicsRagdoll.CreateRagdoll(ent, def, mass, layerMask)` [API]. The
  engine simulates it and moves the bones itself, the way a dead soldier falls, so script sets no bones at all.
  `GetBoneRigidBody(i)` gives each body for gravity / damping changes.
- **Known.** [repo IDA] the engine refuses a ragdoll on anything but an ANIMATED MeshObject ("Can't create ragdoll from
  other than animated MeshObject"), so the mat must be a skinned .xob. Ragdoll definitions are a resource type
  ("Ragdoll Definition", Resource Manager [WIKI File Types]). The user has a local addon "Euphoria ragdoll" (it shows
  in our census logs). It is the best place to see how a ragdoll definition is written.
- **Open:**
  - can a ragdoll definition describe a GRID (a ragdoll is normally a bone tree)?
  - which joint types can a definition use?
  - can a ragdoll's root be pinned to the world?
  - cost per bone body;
  - the same bone palette cost as N4(b).
- **Probe P-N5.** Read a ragdoll definition from the Euphoria addon. Make a 4 x 4-bone skinned plane with a
  definition (a comb tree: root -> rows -> bones). CreateRagdoll, gravity off, then the wheel proxy pushes it.

### N6 - Granular mud (true volume, likely too costly)
- **Idea.** A few hundred small dynamic spheres or boxes (10-15 cm), gravity on, high friction, filling the mud layer
  around the wheels. The proxy ploughs through them, and ridges and bow waves pile up by themselves. That is the real
  "volumetric" behaviour.
- **Drawing.** Each body is a blob mesh, which will read as gravel or clods unless a skin is stretched over it (N5 with
  loose joints).
- **Cost.** Probably the highest of all. Bodies must be spawned ahead of the wheel and frozen or removed behind it.
  Probe it only for a look reference.

### N7 - Morph / ProcAnim pieces (known, part of R5/R1 in the plan)
`Animation.SetMorphState(name, value)` [API] blends premade "fresh -> pressed -> deep" shapes in the engine, one call
per change. ProcAnim with a signal does the same with bones. Covered in research/2026-10-19-mud-sheet-script-cost-research.md
sec 13 and 15.2 (designs C, D). Listed here because it is the engine-native way to animate a rut piece.

### Not worth probing (dead or no fit)
| idea | why not |
|---|---|
| Soft body / cloth physics | not in the script API, although Bullet has it. Only IDA could find a hidden path: search the exe for `btSoftBody`. |
| Volumetric smoke / particles as mud | particles are billboards with no persistent shape [WIKI Particle Editor] |
| Terrain heightmap edits at runtime | `WorldEditorAPI.ModifyHeightMap` is Workbench only; no runtime API [repo] |
| Ocean displacement | ocean only (`GetOceanHeightAndDisplace`) |
| Destructible multi-phase mud lumps | engine collision damage plus a phase mesh swap would work on placed lumps, but it is server-replicated destruction (network) and not ground deformation |
| Material vertex deform (`DeformFunc@MaterialSource`, IDA string) | still an IDA lead only. If some vanilla material moves vertices from a texture, N2 becomes real displacement. One IDA question. |

## 2. MP for all of these
Every route above is client-only look: local entities, local physics, local render targets. The truck's physics and
the server never see them, so they are as MP-safe as the sheet in local mode. As with the sheet, each client's mud is
its own (close, not identical). The felt rut stays the voxel rut.

## 3. Probe session (one WB launch, function and look only; ms later on the DS)
Order = cheapest first, and each probe stands alone (TEST_KIT, its own file, nothing in rut files):
1. **P-N1:** a box on each wheel, two attach methods; a ProcAnim signal from speed.
2. **P-N2:** a $rendertarget plane (colour / normal / height slots); the enum print; a non-clearing target; an
   orthographic `$renderview` from below.
3. **P-N4:** a 10 x 10 gravity-off damped body grid with a kinematic proxy, then a proxy on a driven wheel.
4. **P-N5:** read the Euphoria ragdoll definition; a 4 x 4 ragdoll plane (needs a skinned .xob; see sec 4).
5. **P-N3:** only if P-N2 shows `$rendertarget` in an alpha / opacity slot.
6. **P-N6:** 300 spheres with a proxy, look only.
For each probe the result is yes / no plus a screenshot. Any ms comes later, in one DS client session for the probes
that pass.

## 4. Assets the probes need
- A skinned grid .xob, 3 m, 11 x 11 verts, 4 x 4 bones, "Export Skinning", max 4 bones per vertex [repo research
  sec 13]. A 4 x 4-bone version is enough for P-N5. A wheel groove / bow-wave mesh for N1: a plain mesh at first, a
  morph or bones later.
- A Blender script can make the grid. The local AI may write it, or the user runs it.
- No asset is needed for P-N2 (a script mesh plus an emat) or for P-N4 (spawned boxes).

## 5. IDA questions (local session)
1. Which shader / material classes read `$rendertarget` / `$renderview`, and in which slots: colour, normal, height,
   opacity? Does any model material use Height for parallax?
2. The `RenderTargetWidgetBlendMode` values. Does a non-cleared target keep last frame's pixels?
3. Is `btSoftBody` (or any soft body type) linked in the exe and reachable from any component or resource?
4. What does a ragdoll definition hold: tree only, or extra joints; pinned bones; joint types?
5. `DeformFunc@MaterialSource`: which materials have a deform function, and can it read a texture?
