# Pressed mud layer: the mud look without the sheet (plan, 2026-10-19)

Why: the look lift test (2026-10-19 22:53) proved two things (research/2026-10-19-ruts-invisible.md, gotchas):
- a lifted surface reads as a soft layer (tyres sit in it), and
- grass rides on top of any ground above the heightmap, so a lift hides every rut shallower than it.
So the lift must be PRESSED OUT where wheels go. Done in the patch meshes, from data every client already has
(the rut floor, look ruts, wear count), it needs no sheet tiles and no MUD_MudSync layer at all.

## The rule (per patch vertex, clients only, mesh only - colliders untouched)
```
cut   = max(0, -(rut floor - base))            // rfF in BuildMesh / BandMeshIn; includes client look ruts (instant)
wear  = min(1, MUD_VoxelProbe.WearAt(x, z))    // pass 1 on grass only counts wear (WEAR_PASSES 1)
keep  = (1 - WEAR_PRESS * wear) * (1 - clamp(cut / PRESS_M, 0, 1))
keep  = 0 where the vertex touches a dug / voxel cell (else a lip over the hole - issues doc sec 3)
lift  = MUD_LIFT_M * p.m_aLock[v] * keep        // m_aLock already fades to 0 at kept terrain / buildings
drawn = base + d + lift                         // d keeps the floor + ridge as today
```
Start values: MUD_LIFT_M 0.08 and 0.16 (user picks), PRESS_M 0.03 (one cutting pass presses the layer out fully),
WEAR_PRESS 0.5 (pass 1 leaves a half-pressed track with grass; pass 2 cuts and clears the grass).
Coarse skin: the same MUD_LIFT_M on hole cells (no ruts there; patches replace skin cells where ground moves).

What it looks like: untouched offroad = a soft layer MUD_LIFT_M above the collider (tyres and boots sit in it); a
rutted line = a groove MUD_LIFT_M + cut deep, its floor below the heightmap -> grass-free, dirt from FIELD_SOIL_D;
ridges stay on top of the layer; roads / buildings: the layer slopes to 0 over Edge Fade (1 m).
What it does not do (vs the parked sheet): texture smear, slump animation, live bow wave, per-quad wet material.
Pieces (research 15.2 design A) can add tread / wet detail later.

## Cost
- No sync layer, no sheet tiles: the sheet's whole cost line goes (sync 0.4-0.5 ms/frame per truck, tile builds).
- Per patch vertex: a few float ops + WearAt (a map lookup) - only for patches in a rut field box (rfOn), the rest
  get a constant lift. Patch meshes rebuild on rut changes as today.
- New: a wear-only pass changes no floor, so nothing rebuilds the mesh - a wear change must mark the patch mesh dirty on
  clients (look only, through the same mesh pacing).
- DS: nothing (it builds no meshes).

## Risks to judge in the look test
- Prone soldiers: their lower MUD_LIFT_M is drawn under the layer but still hittable (PvP visual-only cover). This
  decides MUD_LIFT_M more than looks do.
- Props / rocks / trees on holed ground: do they look sunk by MUD_LIFT_M (MUD_PinLift height)?
- The groove wall is one 0.25 m patch cell wide (MUD_LIFT_M over 0.25 m) - too sharp? A 1-cell half-press ring
  softens it.
- Patch / skin seam and road edges.
Determinism: none (drawn only, from replicated data).

## Stages
1. Build behind `MUD_WorldGround.PRESS_LIFT` (default false) + WB look session (prompts/2026-10-19-local-pressed-mud-layer.md).
2. User picks MUD_LIFT_M / PRESS_M / WEAR_PRESS by eye.
3. DS session for cost (patch mesh rebuilds with wear dirt marks) and MP look on a second client.
4. Later, optional: pieces for tread / wet; the sheet stays parked unless the 0.25 m groove is not soft enough.
