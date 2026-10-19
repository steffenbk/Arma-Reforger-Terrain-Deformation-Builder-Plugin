# Research prompt: whole-map mud sheet - redesign or replace (2026-10-19)

You are the research AI for the Arma Reforger mud mod (Enfusion script, addon "Testing plan vech msh"). Research
and propose; do not write the build. The sheet is PARKED; the user decides un-parking.

Read first:
- docs/superpowers/research/2026-10-19-whole-map-sheet-issues.md (your last answer: per-tile streaming cannot cover
  the map - ~622 600 tiles, tens of GB; SPARSE has the driven / undriven step)
- docs/superpowers/results/2026-10-19-whole-map-sheet-test.md, docs/features/mud-sheet/README.md,
  docs/superpowers/research/mud-gotchas.md
- Code: Scripts/Game/Mud/MUD_MudSheet.c, MUD_MudSync.c, MUD_MudZone.c, MUD_WorldGround.c (patches + coarse skin)

Hard limits (all in mud-gotchas / IDA notes): no custom shaders; script MeshObject has one LOD; flags-0 meshes upload
once on first draw, in-place UpdateVerts never shows (a change = a new mesh); no vertex colour; no world-space normal
maps or triplanar (2026-10-19, all .emat); grass clutter is planted at heightmap height; 64 locals per method; perf
only on a DS / standalone, never Workbench; PvP only; the sync layer must stay deterministic.

## The user's question

Find other solutions: redesign the sheet, or use something else. Look hard at the user's own idea:

### Idea A - one large cheap "umbrella" mesh
- One (or few) large, low-detail mesh over all offroad ground, drawn at the same height as the untouched mud
  (ground + THICK 0.15 + LIFT), built once and static.
- When a mud zone has to render in detail (a vehicle or a crater near it), first build the detailed mud tiles, THEN
  cut the matching piece out of the umbrella - so nothing pops in and the swap is invisible.
- Questions: how big can one script mesh be (verts / indices limits, upload cost, culling - one huge mesh is never
  culled); how to "take away a piece" when the mesh cannot be edited in place (rebuild only one umbrella chunk,
  chunks of what size; hole by index list only; a hidden chunk + a ring mesh); does the umbrella follow the ground
  well enough at low detail (it must not cut into or float over the real ground by more than a few cm at the seams
  with the detail tiles); compare with re-using the existing coarse skin (64 m chunks, built once at load) lifted
  0.15 m as the umbrella - is that already Idea A?

### Idea B - mud zones seen from far away (low LOD)
- Driven / cratered mud must show from far away, not only inside the build radius.
- Questions: a cheap far version of a mud zone (coarser grid, merged tiles, baked into the umbrella chunk when it is
  rebuilt); how far it must reach (view distance vs memory); switching near / far with no visible pop (build near
  first, then hide far piece - same rule as Idea A).

### Other options to compare
At least: SPARSE with a fix for the driven / undriven step; drawing the soft layer in the patch / skin meshes
themselves (no separate sheet); decals or a decal-like prefab for mud look; a terrain-material / gamemat route;
anything in the engine (IDA / API) we have not used. For each, say what it costs and what it cannot do.

## Report back

A ranked table: option, how it works, memory, build cost, per-frame cost, pop-in risk, far view, determinism risk,
what to measure first. Label PROVEN-CODE / MEASURED / [repo] / INFERENCE as before. End with one recommended
route and the smallest test that would prove or kill it.
