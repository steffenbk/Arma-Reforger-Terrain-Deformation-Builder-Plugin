# Research prompt: whole-map mud sheet - pop-in, late craters, sheet over holes (2026-10-19)

You are the research AI for the Arma Reforger mud mod (Enfusion script, addon "Testing plan vech msh"). Research
and propose; do not write the build. The sheet is PARKED (MUD_MudSheet.ENABLED false in the repo); the user decides
un-parking. This prompt is about what a whole-map sheet would need to look right.

Read first:
- docs/superpowers/results/2026-10-19-whole-map-sheet-test.md (the test, every number, what is MEASURED vs INFERENCE)
- docs/features/mud-sheet/README.md, docs/superpowers/research/mud-gotchas.md
- docs/superpowers/research/2026-10-19-mud-sheet-script-cost-research.md (the earlier cost research)
- Code: Scripts/Game/Mud/MUD_MudSheet.c, MUD_MudSync.c, MUD_MudZone.c

Hard limits: no custom shaders; script MeshObject only (flags-0 meshes upload once on first draw, in-place
UpdateVerts never shows, no vertex colour); 64 locals per method; perf may only be judged on a DS / standalone game,
never in Workbench. PvP only. The sync layer (MUD_MudSync) must stay deterministic on every machine (hash lines).

## Issue 1 - pop-in while driving (user: "need everything loaded but static until we need it")

Now: RADIUS_ON, RADIUS 40 m (shrinks to RADIUS_MIN 20 m with speed), RADIUS_HYST 6 m, RAD_BUILD_MS 2 ms a frame.
Tile = 20 x 20 quads of 0.15 m (3 m). About 600 meshes live; driving at 10 m/s = ~900-1700 builds and ~400-900 drops
per 5 s; the queue reached 284 tiles. Code: MUD_MudSheet.Radius() (~line 1628), QueueNear() (~1729),
BuildZoneTile() (~1968).

Questions:
1. Can every offroad tile be built once and kept static (no per-frame work until a tile changes)? Count the tiles for
   all holed offroad Arland ground (use the hole data / ALL_OFFROAD zone), the memory per tile mesh (verts + indices
   + entity), and the load-time build cost from the measured ~0.55 ms per tile. Is that within reason for a client?
2. If not all at once: what gives "no visible draw-in" at lower cost - bigger tiles far away (fewer entities,
   fewer verts per m), building far tiles over load / idle frames and never dropping them, a larger keep radius than
   build radius, a low-detail far version, or hiding new tiles until they are built (the old heightfield ground draws
   under them meanwhile)? Is there an engine LOD / view-distance setting for script entities that does the culling
   for free (so we never drop meshes ourselves)?
3. Why does the build radius shrink with speed (the opposite of what hides pop-in)? Propose the rule.

## Issue 2 - a crater shows ~1 s late (user eyes)

The logs say ~0.2 s: server blast 22:12:09.417 -> client voxel live and sheet 'blast now' + 'dig now' rebuilt by
22:12:09.631 (table in the results doc). So the 1 s is not visible in the logs.
Code: MUD_MudSheet.GroundChanged() (~2061, 'blast now'), the dig-now path, FlushOneBuild() (~726, STORED_MS 3),
MUD_MudSync Drain() (~282, DRAIN_MS 3) and its JOB_CLEAR crater clear rows.

Questions:
1. Which step can add ~1 s that the logs do not show? Check: stored tiles waiting in FlushOneBuild; the voxel
   bricks (14 new meshes) drawing before / after the sheet tiles over them; first-draw GPU upload; the sync-layer
   clear (JOB_CLEAR) re-covering then baring tiles; the blast effect hiding the hole.
2. Propose one log line (frame time per crater tile: queued, built, mesh set) that would settle it in one run.
3. Could the sheet over a blast area simply hide at once (tile entity hidden in the blast frame) and rebuild later?

## Issue 3 - sheet sticks out of a trench (and sometimes a crater hole)

The sheet is a soft layer THICK 0.15 m above the hard ground, with TAPER 0.75 m (TAPER_PAD 6 cells) to bare cells and
a skirt (SKIRT_ALL). Voxel holes bare cells (BARE_VOX 2). A trench = ~71 ops; 'op clearing: 71 crater / dig ops,
10178 cells bared, 1272 ms', 71 flushes 1187 ms; 19 'dig now' rebuilds over ~4 s.

Questions:
1. Why does the sheet stand above the cut rim? Check the taper (does it start at the bared cell or 0.75 m away from
   it), the skirt edge, BARE_VOX margin, and tiles built between trench pieces that keep an old ground height.
2. Fix options: bare a wider ring round voxel holes; make the taper reach 0 height at the hole edge; drop the sheet
   to the voxel top inside a dig box; rebuild every tile a trench touched once at the end.
3. Is the 17 ms per op flush on the trench path acceptable, or should trench ops batch into one rebuild?

## Report back

Per issue: cause (PROVEN with a file / line, or INFERENCE), the fix you propose, its cost, and what to log to check
it. Keep the sync layer deterministic and do not touch the rut / voxel code paths.
