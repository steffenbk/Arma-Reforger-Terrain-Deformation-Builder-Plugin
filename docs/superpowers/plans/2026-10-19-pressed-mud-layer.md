# Mud overlay beside the voxel ruts - read-only, ruts untouched (plan, 2026-10-19, v2)

v2 replaces the first version of this file (a lift inside the patch meshes). User 2026-10-19: "don't touch the ruts,
we only focus on the mud". The voxel ruts are OFF LIMITS: rut ops, the rut field, wear, look ruts, the patch meshes
and colliders that draw / feel them, and the op format. The mud may only READ their data and draw its own meshes.

## What the lift test taught (keeps the design honest)
- Grass rides on top of any ground above the heightmap (gotchas 2026-10-19): a mud layer that covers a rut hides it.
- A lifted layer everywhere costs a 0.16 m visual-only offset (prone soldiers, props) and whole-map streaming.
So the mud is NOT a layer over all ground. It is decoration around the ruts: it starts at 0 height at its outer edge,
rises into a soft berm / wet band beside the rut, and has no quads over the rut floor. The rut itself is drawn by the
untouched voxel / patch path and stays visible (grass-free floor, dirt from FIELD_SOIL_D).

## The overlay
- Own entities: 3 m mud tiles (the sheet's tile code is the starting point), no collider, clients only.
- Exists only where the rut field has ruts or wear (read-only `MUD_VoxelProbe.FieldFloor / FieldFloorAt`, `WearAt`).
  No tile on untouched ground -> nothing to stream on fresh offroad, no step against untouched ground (height 0 at the
  edge), and far view comes from keeping the few touched tiles.
- Height per vertex: drawn ground (read-only: `MUD_WorldGroundEntity.DugUndug`, `MUD_VoxelProbe.TopAt` on voxel
  columns) + mud m, with
  - m = 0 over the rut floor (cut > ~1 cm) -> quad skipped, the rut shows through untouched;
  - m = berm profile beside the cut (a few cm, wider / higher with depth and passes), 0 at the outer edge;
  - m from wear only (pass 1, no cut yet): a thin flattened band, no berm.
- Look: a wet / dark mud material on the overlay; texture smear and the sheet's live wheel stamps are optional extras
  (client-only prediction under the wheel, dropped when the rut field catches up).
- MP: the same on every client because it is computed from replicated data (rut floor, wear) - no MUD_MudSync layer,
  no op tag, no snapshot parts. The DS does nothing.

## What must stay OFF / unused (couplings the parked sheet has into the rut path)
With MUD_MudSheet.ENABLED false these are all inert today; the overlay must not switch any of them on:
| where | what it does | file / line (44335db) |
|---|---|---|
| sheet tag in kind-4 rut ops | changes the op's seed field | MUD_VoxelProbe.c 3396-3425 |
| RUT_SHEET_MS / RutUnderSheet | slows the feel rut ops under the sheet | MUD_VoxelProbe.c 256, 1384-1401 |
| RUT_SHEET_ONLY_TEST | skips feel ruts under the sheet | MUD_VoxelProbe.c 241, 3942 |
| TakeOp / Holds / m_bSheetOp / CoversCell / SheetDepth | ops or voxel drawing handed to the sheet (ONE_GRID era) | MUD_VoxelProbe.c 3090, 3933, 4081-4086, 6551 |
| MUD_MudSync hooks (Flush, RutOp, ClearBefore / ClearOp, snapshot parts) | sync layer work inside Submit / J2 | MUD_VoxelProbe.c 1717, 2015-2436, 4068, 4170, 4217-4220 |
| LookRuts SHEET_SKIP / H2 LookChanged | look ruts skipped or redirected under the sheet | MUD_LookRuts.c 46, 532, 600 |
| LAZY_RUT_MESH, SPARSE A2 lift, LOOK_LIFT_TEST | hide / lift the patch meshes | MUD_WorldGround.c 4615-4630, 6607-6690; MUD_CoarseSkin.c 617-751 |
The overlay uses only read calls. To know when to rebuild it either polls the rut field per tile near the camera
(every ~0.25-0.5 s: compare a per-tile change counter if the rut field has one, else a cheap checksum of the floor
samples it covers) or listens to the existing `MUD_MudSheet.FloorChanged` call that Submit already makes (it is a
notification after the rut work, not a change to it). Polling is the zero-touch option.

## Open risk: grass on the overlay
Grass rode on top of our lifted ground (gotchas 2026-10-08 / 2026-10-19). The overlay also sits above the heightmap,
so grass may ride on the berm and the film too - then the mud reads as a grassy bump. Unknown until seen (the clutter
mechanism is not proven, gotchas 2026-10-08 'inference'). First item of the look session; if it happens, the overlay
needs a way to stay out of the clutter (to research) or the mud is colour-only.

## Cost (inference)
Tiles only along tracks; built once per change, kept; no sync layer; no streaming over fresh ground. The build is the
sheet's tile build (~0.55-0.9 ms per tile, research sec 9) - only for tiles near ruts that changed.

## Stages
1. Build behind a new switch (e.g. `MUD_MudOverlay.ENABLED`, default false, separate from MUD_MudSheet), WB look
   session (prompts/2026-10-19-local-pressed-mud-layer.md).
2. User tunes berm width / height, wet material, whether pass 1 shows.
3. DS session: client cost + MP look on a second client.
