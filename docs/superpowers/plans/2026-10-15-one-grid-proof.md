# One-grid proof (SP, mud zones) - 2026-10-15

User picked option 1 of docs/superpowers/research/2026-10-15-ground-architecture-review.md. Checkpoint v072 = before.
Switch: `MUD_MudSheet.ONE_GRID` (false = today's voxel + sheet).

## What it does (ONE_GRID true)
1. Load: zone tiles are built as today (traced ground + THICK mud). When all are built, every 2 m skin cell fully
   covered by zone tiles goes VOX (skin.SetVoxCells + WorldGround.VoxCellsChanged): our patch mesh AND collider
   are gone there. The sheet is now the only ground in the zone. Ground re-traces stop (they would read nothing).
2. Draw: the sheet draws the ground too - blown-off / bare cells draw at ground height (not dipped, not skipped);
   HOLE_SKIP off; the tile edge toward non-sheet ground dips 3 cm under (no z-fight).
3. Collider: each sheet tile's trimesh is the GROUND (no mud) on Terrain | Foliage (= our patches), so wheels,
   soldiers and bullets all hit it.
4. Ops: crater (kind 0) and rut (kind 4) ops with their point in a zone go into the sheet's ground array
   (MUD_MudSheet.TakeOp, called at the top of MUD_VoxelProbe.Submit after Prepare), no voxels. Same formulas:
   crater = Carve's rim bump + half-ellipsoid bowl + funnel; rut = CarveRut's profile, step m_fDepth, cap m_fB under
   the undug ground (heightmap trace). Craters clear the mud with BlastKeep; ruts keep the mud on top.
   Ruts still cut 3 m behind the truck (server RUT_BEHIND, unchanged).

## Not in the proof
Trenches / shovel / tunnels in a zone (still voxel - they will look wrong there), MP (the sheet is client-only;
a server has no sheet, so ops still go voxel there), steep-wall UVs, JIP.

## Measure / pass
Log lines `MUD one grid: ...` (hidden cells, op ms). Pass = no seam at crater edges, ruts felt, mortar frame
< ~16 ms (voxel-only control: 3 ms go-live with no mud tiles).

## Built (2026-10-15, not compiled yet)
- MUD_MudSheet: ONE_GRID + ONE_DIP 0.03, HideZoneGround (after "all N tiles built"), Grounds skips re-traces once
  hidden, SetTraceHidden / BuildTile keep the collider traceable, BuildTile: bare = ground, edge dip, no bare/HOLE_SKIP
  quad skips, collider = ground verts (cvv) on Terrain | Foliage. TakeOp -> ApplyGroundOp -> OpGround (crater / rut).
- MUD_VoxelProbe.Submit: `if (MUD_MudSheet.TakeOp(op)) return true;` right after Prepare.
- Known gaps: a crater whose box reaches past the zone only changes the zone part; MUD_Surface.Y in a hidden zone
  falls back to the undug heightmap (sheet tiles are not IsOurs).
