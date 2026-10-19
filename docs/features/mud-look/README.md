# Mud look layer (client-only MudRunner look on the felt rut)

**Status: PARKED 2026-10-18** (user after the first test: "its really not so good, ok stop it here remove what we
have done and log our findings"). Code REMOVED from Scripts and archived: mud_backups/archive/mud-look-layer-2026-10-18
(README lists every call site). Checkpoint v176 = the tree with it in; v173 = before. Kept: Tools/mud_look_proto.py
(--fast) and the results files (offline only). Everything below describes the removed code.

## Findings (2026-10-18, one WB server + PeerPlugin1 test, build compiled 00:16)
- FPS (user "tanking fps"; peer log 00:17 - WB peer tool, a hint not a PERF number): 99 look builds in 10 s, mean 30 ms,
  worst 33 ms, 6 patches in range = ~3 s of script per 10 s. One look build (77 x 77 samples: 5 x 5 max, 2-pass
  chamfer, 5 x 5 blur, 65 x 65 assemble, 4745-vert MeshObject) is ~30 ms in Enforce - 10x the plain band build.
- Cause of the build count: the client look ruts (MUD_LookRuts) mark the patch mesh-dirty every 0.1 s while a truck
  drives on it, and every rebuild ran the full look. A LOOK_QUIET 1.5 s fix (busy patches build plain, look once
  quiet) and COST_LOOK 30 were written but never tested before the stop.
- User saw: updates stop, then catch up (fits the 30 ms builds clogging the one-mesh-per-frame queue - INFERENCE),
  and "very many holes in the ground" in / next to the ruts. Holes NOT explained: no errors, no HOLE lines, same
  triangle winding as FillIndices, skirts on all 4 borders, band neighbours have rim skirts. Unresolved.
- Any retry needs, first: a build well under 5 ms (cache Compute per patch while FieldFloor is unchanged - look-rut
  dirt never changes it; cheaper distance field) and a screenshot of the holes.

## What it does
Rut patches within `LOOK_RADIUS` 20 m of the camera (back to plain past 22 m) draw a 0.125 m mesh (65 x 65 verts)
instead of the 0.25 m one: a narrow tyre groove with a dirt ridge at its wall (the mud sheet's BOX + SOFT profile)
over the felt bowl. Mesh only - collider, ops, net, snapshot and hashes never see it. A DS never builds it.

SAME ON EVERY CLIENT: the look reads only replicated data - the server rut floor (`MUD_VoxelProbe.FieldFloor`, not the
client look ruts), the patch base heights and the edge lock. The camera decides only WHETHER a patch gets it.
A missing or still-building neighbour patch gives the same values (skin `BaseAt` + `EdgeLock` + skin TLBR).

## Files
- `Scripts/Game/Mud/MUD_LookLayer.c` (new, LF): buffers + maths (Compute = look_fast, Assemble, Indices).
- `MUD_WorldGround.c`: consts LOOK_LAYER / LOOK_RADIUS / LOOK_HYST / LOOK_CHECK / COST_LOOK; BuildMeshLook,
  LookBaseLock, LookDiag, LookRange (0.5 s radius check), LookStats (10 s line); hooks in BuildMesh (first), Rebuild
  (asleep patches may build a look-gate mesh; settle hold lets it through), EOnFrame, Stats.
- `MUD_FinePatch.c`: m_bLookNear, m_bLookGate.
- Reference maths: `Tools/mud_look_proto.py --fast`.

## Rules in the mesh
- Outside the tyre strip y = max(look, felt); inside it the look groove (`LOOK_LIFT` 0.02 over the felt floor),
  shifted down where the felt is under the rut floor (heightfield crater over a rut).
- Look x edge lock (none at a road edge). A vertex touching a drawn-nothing cell (kept / voxel / old look probe) keeps
  the felt height, so roads and voxel bricks meet it as they meet the plain mesh.
- Skirts (m_fBandSkirt) on all 4 borders, facing out: hide steps against a plain (band) neighbour.
- Same two sections + materials as BuildMesh (groove reveal trick); dirt = groove reveal.

## Docs
- Prototype: [results/2026-10-17-mud-look-proto.md](../../superpowers/results/2026-10-17-mud-look-proto.md)
- Stage 1 look_fast: [results/2026-10-17-mud-look-fast.md](../../superpowers/results/2026-10-17-mud-look-fast.md)
- Old mud sheet: [mud-sheet](../mud-sheet/README.md)

## Log
`MUD look: patches in range N, builds N, mean ms, worst ms, index misses N (radius 20 m)` every 10 s (render machines).
Build ms only counts in the PERF session, never in Workbench.

## FPS fix that was written (never tested)
Peer client log 00:17 (WB peer tool - a hint, not a PERF number): 99 look builds in 10 s, mean 30 ms, worst 33 ms.
Cause: the look ruts mark the patch every 0.1 s and every rebuild ran the full look. Fix (v175 = before, NOT COMPILED):
LOOK_QUIET 1.5 s - a patch built under 1.5 s ago builds PLAIN (m_bLookOwed), LookRange then gives it one look build
once quiet; COST_LOOK 8 -> 30. Log: 'plain while busy N'. Still open: one look build = ~30 ms, a hitch on its own -
needs a faster build (cache Compute per patch when the floor did not change; cheaper chamfer) before release.

## Open
- Stage 2: user compile + WB check (8 items in the build 3 plan / handoff).
- Stage 3: UV texture drag along the track (DRAG_MAX 0.3, DRAG_LIM 0.06) + tuning LOOK_RIDGE_K, LOOK_LIFT,
  LOOK_HALF, dirt threshold, radius - with the user.
- Known limit (inference, not seen): a rut landing within 0.75 m of a patch border changes the look margin of the
  neighbour; that neighbour rebuilds only if the rut op also marks it dirty.
- Final MP / PERF: see plans/2026-10-17-build-3-plan.md "Final MP" (mud look layer line).
