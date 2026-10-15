# Mud sheet + ONE GRID (mud zones) - PARKED

**Status: PARKED 2026-10-15 ~17:05 (user).** Switched off: `MUD_MudSheet.ENABLED = false`
(Scripts/Game/Mud/MUD_MudSheet.c). With it off the game runs the plain voxel ground + voxel ruts (v035 behaviour).
Last sheet state saved as **checkpoint v083** ("mud sheet latest before parking"). To resume: set ENABLED true,
recompile, then work the open list below first.

Full assessment (why it was hard, whole-map numbers, voxel alternatives): [2026-10-15-assessment.md](2026-10-15-assessment.md).

## Why parked
- USER 17:0x: ruts in a mud zone tank the frame rate, and it jittered on a dedicated server.
- USER: doing many things at once - park it, get the rest solid.

## What it is
- Client-only soft mud layer (0.15 m cells, 3 m tiles) inside placed `MUD_MudZoneEntity` boxes
  (Scripts/Game/Mud/MUD_MudZone.c). Wheels press ruts and push ridges; a live 12 m window follows the vehicle.
- ONE GRID: inside zones the sheet IS the ground. The skin cells it covers are hidden; its collider is the ground.
  Crater / trench / fill / rut ops in a zone change the sheet's ground (`MUD_MudSheet.TakeOp`), not the voxels.
- Plan: [../../superpowers/plans/2026-10-15-one-grid-proof.md](../../superpowers/plans/2026-10-15-one-grid-proof.md).
- Edge review: [../../superpowers/research/2026-10-15-sheet-edge-review.md](../../superpowers/research/2026-10-15-sheet-edge-review.md).
- Merge notes: [../../superpowers/research/2026-10-15-sheet-voxel-merge.md](../../superpowers/research/2026-10-15-sheet-voxel-merge.md).
- Freeze test (heightfield ruts under the sheet cost ~13 fps): [../../superpowers/results/2026-10-15-freeze-test.md](../../superpowers/results/2026-10-15-freeze-test.md).

## Where we got to (2026-10-15)
User-OK along the way: zones, crater hook, terrain look, no squares, SOFT ruts, SMOOTH_EDGE, trenches / shovel / fill
into the sheet (v078 = the "most stable" sheet version), tunnels with holes in the sheet.

Last session (checkpoints v081-v083), user: "nice now it's much better":
- Shell inside a tunnel -> voxels only (`InVoxels`, IN_VOX 0.3).
- Crater / trench meeting voxel columns (tunnel) or crossing the zone edge -> sheet AND voxels. The sheet part is
  HELD (`m_aPend`) and applied at the voxels' go-live (SYNC), so both halves show together.
- Voxels do not draw a crater / trench in sheet-covered cells (`MUD_VoxelProbe.m_bSheetOp`, `MudSheet.CoversCell`);
  DigNow only opens the sheet where the voxels draw (`MUD_VoxelProbe.DrawsAt`). Fixed rim strips + sky holes.
- Stored tiles built once per change (`QueueBuild` / `FlushOneBuild`), the frame after the go-live.
- TAPER 0.75 m across tile borders (`TaperField`) with a sine shoulder (`TAPER_CURVE`): the mud edge curves down.
- BuildTile `tv` overflow fixed (the cut cap reserves the grid verts still to come).
- Held ops applied PEND_STEP_MS 8 per frame, DigNow after (a 60-op trench beside a tunnel froze ~0.7 s).
  Written after the last test - NOT COMPILED, NOT TESTED.

## Check FIRST when the sheet comes back (user 2026-10-15)
- [ ] **DS stutter.** USER 17:09: with the sheet OFF the dedicated-server stutter was gone (v084). Turn the sheet
  back on, run the same DS test, and find what in the sheet makes it stutter BEFORE any new sheet work.
  Likely cause (INFERENCE from code, see [2026-10-15-assessment.md](2026-10-15-assessment.md) 2.2): ONE_GRID makes
  the client-only sheet the wheel ground, but a DS has no sheet - server and client drive on different ground.
  Test ONE_GRID false vs true on a DS (Diag exe corrections count).
- [ ] **Rut fps tank** in a mud zone (item 1 below) - measure it first.
- [ ] Compile + test the last untested code: held-op spreading (PEND_STEP_MS), tile builds a frame after go-live.

## Open (do these first when it comes back)
1. **Rut cost (USER: fps tank).** LOG 17:01 client: each rut op 4-12 ms every ~0.25 s (`op kind 4 ... tiles in X ms`,
   2-5 stored tiles rebuilt). INFERENCE (not measured): the live window now marks +-6 cells round every change
   (`MarkRect(... TAPER_PAD ...)` in ApplyGroundOp) and every live tile build runs `TaperField`, so a rut rebuilds
   more tiles than before. Try first: skip the taper padding for rut ops (kind 4 makes no bare cells), then measure.
2. **DS jitter (USER).** USER 2026-10-15 17:09: with the sheet OFF (v084) the DS stutter is gone - so the sheet
   caused it (how is not measured). Not looked at further. The sheet is client-only, so check whether the jitter is the sheet's frame
   cost or vehicle corrections (Diag exe: Vehicles > "Vehicle Networking and Reconciliation").
3. **Tunnel mortar stutter.** LOG: go-live frame ~100 ms (voxels ~50 + sheet ~50). Tile builds moved one frame
   later (untested).
4. **GM can't select the tunnel** easily. Not investigated (guess: the sheet collider over the mouth).
5. Earlier LATER list: ridge-side UV stretch, wall-collider probe, CollPending double rebuild.

## Files
- Scripts/Game/Mud/MUD_MudSheet.c - the sheet (TakeOp, ApplyGroundOp, DigNow, BuildTile, TaperField, ...).
- Scripts/Game/Mud/MUD_MudZone.c - zone boxes.
- Scripts/Game/Mud/MUD_VoxelProbe.c - Submit hook (`both`, HasCols, Holds), MarkCarved sheet skip, DrawsAt.
  These voxel-side hooks do nothing while ENABLED is false.

## Checkpoints
v077 pre-trench-into-sheet, v078 sheet base (user: most stable), v081 tunnel craters both (user: big improvement),
v082 before the sync/draw fix, v083 last state before parking.

## Details (moved from CLAUDE.md 2026-10-15, kept word for word)
- Mud sheet (MUD_MudSheet.c, client-only visual mud, 2026-10-14/15): soft layer THICK 0.15 m on our ground, 0.15 m cells, 3 m tiles (TQ 20). Mud only inside placed MUD_MudZoneEntity boxes: all zone tiles built at load (BUILD_MS 8/frame) and stored; a 12 m live window (FOLLOW) follows the nearest vehicle, takes stored tiles over and freezes them back. Wheels press ruts + push ridges (arch round tyre front/back, NOSE/ARCH_H), drag on vehicles. Ground height on hole cells = MUD_Surface.Y (GetSurfaceY is -256 there). Voxel hook: VoxelProbe.GoLive -> MUD_MudSheet.GroundChanged -> tiles redone 0.1 s later; crater blast clears mud (BlastKeep, BLAST_CLEAR), ground moved > MOVED 0.08 m clears mud (trenches). TOP_LOOK: untouched quads draw MAT_TOP (terrain look), touched ones dirt (2 sections, 2 emats). COLLIDE: per-tile static trimesh on CharCollide 0x80000 only (soldiers), live tiles max 1/COLL_S s; NOT FireGeo because all our ground reads are Projectile traces and would read the mud top. User-confirmed: zones, crater hook, mud on our ground, crater clearing, no squares, terrain look. 2026-10-15: ground ruts back to v035 voxel ruts (RUTS true, RUT_BEHIND_GAP 3, RUT_LOOK false) because the heightfield ruts under the sheet cost ~13 fps (docs/superpowers/results/2026-10-15-freeze-test.md).
- ONE GRID (2026-10-15, plan docs/superpowers/plans/2026-10-15-one-grid-proof.md): in mud zones the sheet IS the ground (zone skin cells VOX, collider = sheet ground verts on Terrain|Foliage); crater/trench/fill/rut ops in a zone -> MudSheet.TakeOp, voxels only for tunnels (BARE_VOX holes). Earlier user-OK sheet parts: GRADUAL press (PRESS_STEP 0.05), BLAST_NOW + RIM_BARE, BOX ruts (HALF_PER_R 0.38), MERGE (SOIL_TILE 12, TAPER 0.6, HOLE_SKIP), DIG_NOW, SOFT ruts, SMOOTH_EDGE.
