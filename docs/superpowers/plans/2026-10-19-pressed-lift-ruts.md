# Pressed lift: a soft layer drawn by the rut meshes, pressed out by the ruts (plan, 2026-10-19)

The second design, tried AFTER the v3 overlay (plans/2026-10-19-mud-overlay-v3.md). User 2026-10-19: build both, try
v3 first, then this one. Unlike v3 it changes how the ruts are DRAWN (one added term in the patch mesh builders, behind
its own switch). It does not change the rut data, ops, wear rules, colliders, feel, or the server.
The two never run together (each refuses to turn on while the other is on).

## Why it is worth trying
- It is the only design that gives "tyres and boots sit in a soft layer" on fresh ground.
- One surface (the patch), no second mesh: no seams, no z-fight, no pop-in, no extra builds; far view free (kept
  patches + the coarse skin).
- Grass behaviour is already known: lifted untouched ground keeps its grass; a pressed-out rut floor drops below the
  heightmap and loses it (research/2026-10-19-ruts-invisible.md).
Cost of it: every offroad surface is drawn MUD_LIFT_M above the collider - a prone soldier's lower part is under the
drawn ground but still hittable. MUD_LIFT_M is decided by that, by eye.

## The rule (per patch vertex, clients only, mesh only)
```
cut   = max(0, -(rut floor - base))            // rfF in BuildMesh / BandMeshIn, incl. client look ruts (instant)
wear  = min(1, MUD_VoxelProbe.WearAt(x, z))    // only when rfOn, else 0
keep  = (1 - WEAR_PRESS * wear) * (1 - clamp(cut / PRESS_M, 0, 1))
keep  = 0 at vertices touching a voxel / dug cell (no lip over holes)
lift  = MUD_LIFT_M * p.m_aLock[v] * keep        // m_aLock already fades to 0 at kept terrain / buildings
drawn = base + d + lift
```
Coarse skin hole cells: + MUD_LIFT_M (the old LookLift path with the new value). Presets for the look session:
(0.04, 0.03, 0.5), (0.08, 0.03, 0.5), (0.16, 0.03, 0.5), (0.16, 0.06, 0).
A wear-only pass changes no floor: mark the patch mesh dirty on clients when wear changes there (look only, normal
mesh pacing) - the one hook this design adds to the rut flow.

## Where it touches the rut code (all behind `MUD_WorldGround.PRESS_LIFT`, off = exactly today)
- MUD_WorldGround.LiftSetup / LiftAt (~6610-6680) and the two call sites in BuildMesh (~6799) and BandMeshIn (~7467).
- A client-only mesh-dirty mark on wear change (where wear is written client-side).
- MUD_CoarseSkin.LookLift (~617-651) with MUD_LIFT_M.
Nothing in rut ops, rut field data, wear rules, colliders, or the server.

## Risks to check
Prone soldier (per preset), props / rocks / trees sunk, the groove wall one 0.25 m cell wide, crater / trench rims,
road / building edges, the patch / skin seam.

## Stage
Build prompt: prompts/2026-10-19-local-mud-two-designs.md (part 2), tested after part 1.
