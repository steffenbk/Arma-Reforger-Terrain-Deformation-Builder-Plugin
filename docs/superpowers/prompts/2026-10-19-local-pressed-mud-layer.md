# Prompt for the local AI: mud overlay beside the voxel ruts (stage 1), 2026-10-19 - v2

v2 (user 2026-10-19: "don't touch the ruts, we only focus on the mud"). Copy everything below the line.

---

Build stage 1 of `docs/superpowers/plans/2026-10-19-pressed-mud-layer.md` (v2, branch `claude/upbeat-lovelace-q0w022`):
a mud overlay drawn beside the voxel ruts. Build + one WB look session (look only, no ms). Stop and report after it.

## Hard rule: do not touch the ruts
No change in the rut ops, rut field, wear, look ruts, MUD_VoxelProbe Submit / op format, the patch meshes or colliders
(MUD_WorldGround mesh / collider builders, LiftAt, lazy hide), MUD_CoarseSkin, or MUD_MudSync. The overlay only READS:
`MUD_VoxelProbe.FieldFloor / FieldFloorAt / WearAt / TopAt / ColumnAt`, `MUD_WorldGroundEntity.DugUndug`. Every sheet
coupling listed in the plan's "must stay OFF" table stays off (MUD_MudSheet.ENABLED false, LOOK_LIFT_TEST false).
If something seems to need a change in a rut file, stop and ask me.

## Read first
- `CLAUDE.md`; the plan (v2); `research/2026-10-19-ruts-invisible.md` (grass rides on top of anything above the
  heightmap); `mud-gotchas.md` (Enforce limits, MeshObject rules: one fresh mesh per change, distinct emat per section,
  indices before verts, 64 locals).
- Code to borrow from (copy, do not call into its state): `MUD_MudSheet.BuildTileMesh` / `SpawnTile` (tile mesh,
  skirt), `MUD_LookRuts` (how a rut sample maps to world x, z).

## Rules
- Checkpoint first: `mud_backups/checkpoint.ps1 save "before mud overlay"`.
- New file `Scripts/Game/Mud/MUD_MudOverlay.c`, switch `MUD_MudOverlay.ENABLED` (default false), clients only (return
  on a DS), no collider. Ticked from one existing place with a single guarded call is fine (say where); nothing else in
  existing files.
- Tuning values as static floats with a TEST_KIT key that cycles presets and rebuilds the overlay tiles in view.
- Git: `git add --ignore-removal .`, commit, push to this branch. No PR.

## Build
1. Tile set: 3 m tiles at 0.15 m (21 x 21), only where any rut-field floor sample or wear in the tile is set; built
   near the camera (start 60 m), kept until far + a memory cap; rebuilt when the rut data under it changed (poll a
   cheap per-tile check every 0.25-0.5 s near the camera - no hook into the rut code).
2. Per vertex: ground = DugUndug dug height (TopAt on voxel columns), cut = undug - floor (read-only).
   m = 0 where cut > CUT_BARE (start 0.01) -> those quads are skipped; beside the cut a berm: m = BERM_H x profile(distance
   to the nearest cut sample / BERM_W), BERM_H growing with cut depth and passes (cap), 0 at the outer edge; wear-only
   cells (pass 1): a thin flat band WEAR_H. Vertex = ground + m + 0.005 (never below the drawn ground).
3. Material: one wet / dark mud emat on the overlay (distinct path; MatPBRBasic or MatPBRMulti from vanilla mud), a
   short skirt on open sides.
4. Log every 10 s: 'MUD mud overlay: tiles N (built N, kept N, dropped N), polls N, rebuilds N, worst build ms (WB, not
   perf), preset P'.

## WB look session (one launch)
Urals on grass: one line 1 / 2 / 4 passes, a crossing, a crater and a trench near it, a road edge. For each preset look
from 2 / 20 / 60 m: is the rut itself unchanged (grass-free floor, dirt as before), does the berm read as soft mud, any
z-fight with the patch, any overlay over the crater / trench (must not), pop when tiles appear. Screenshots per preset.

## Report back
Files / methods, the one call site you added, compile result, log lines, screenshots, anything odd. Do not tune or
enable anything by default - I pick.
