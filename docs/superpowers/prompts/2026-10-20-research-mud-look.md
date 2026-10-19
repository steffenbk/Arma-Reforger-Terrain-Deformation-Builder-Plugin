# Research prompt: why the two new mud designs look worse than the old sheet (2026-10-20)

You are the research AI for the Arma Reforger mud mod (Enfusion script, addon "Testing plan vech msh"). Research
and propose; do not write the build. The user compares and picks; nothing gets enabled by default.

Read first:
- docs/superpowers/results/2026-10-20-mud-two-designs-ds.md (the DS run: full fps / ms tables, parts ms, D2 counters,
  user verdicts)
- Your plans: docs/superpowers/plans/2026-10-19-mud-overlay-v3.md (design 1), 2026-10-19-pressed-lift-ruts.md
  (design 2); your last answer docs/superpowers/research/2026-10-19-sheet-redesign.md
- docs/features/mud-sheet/README.md, docs/superpowers/research/mud-gotchas.md
- Code: Scripts/Game/Mud/MUD_MudOverlay.c (D1, new), MUD_WorldGround.c (D2: PRESS_LIFT, PressAt, PressNearHole,
  SetPressLift, call sites in BuildMesh / BandMeshIn), MUD_VoxelProbe.c (WearPass -> PressWearMark), MUD_CoarseSkin.c
  (LookLift, PressLiftRebuild), and the parked MUD_MudSheet.c + MUD_MudSync.c (the look the user wants back)

Hard limits (mud-gotchas / IDA notes): no custom shaders; script MeshObject = one LOD, a change = a new mesh (in-place
UpdateVerts never shows); no vertex colour; grass clutter planted at heightmap height (rides our ground when >= 1 mm
above it); 64 locals per method; perf only on a DS / standalone; PvP only; the voxel ruts stay (RUTS STAY VOXEL,
RUT WEAR v234: pass 1 on grass = wear only, each later pass cuts 3 cm); rut ops stay floor-blind (J2).

## What the user saw (DS, one run)

- D1 overlay (preset 1): "horrible fps". Client MUD script 5.6-6.6 ms/frame (38-46 % of the frame), max 82-89 ms,
  fps ~68-72 against ~84-100 without it. Parts per 10 s: stamps + re-centre ~1.1 s, builds 1.2-1.4 s (ground reads
  ~0.6 s, replays 0.2-0.5 s), poll + keep 0.26-0.65 s, fallback 0.1-0.17 s; 58-66 tiles, 480-620 builds / 10 s.
  Preset 2 ~2-3.8 ms. Look: blocky, patchy grooves.
- D2 pressed lift: cheap (1.2-2.5 ms total MUD script while driving), "worked but didn't look so good and was very
  slow to get any rut".
- The user: "the original sheet looked way better with how it deformed and flowy it was - why isn't ours so good?"

## Questions

1. Why do D1 and D2 look worse than the old MudSheet? Check our reading against the code (INFERENCE so far):
   - D1's groove follows the voxel rut floor and D2 only presses where that floor is cut / worn; with RUT WEAR pass 1
     shows ~nothing and later passes only 3 cm, while the sheet pressed its own 15 cm layer from pass 1
     (BOX + SOFT + GRADUAL).
   - The sheet's flow came from slump (SLUMP 0.25 at 10 Hz, MudSync SLUMP_PER_OP 3), pushed-out ridges / bow wave,
     SMOOTH_EDGE, SKIRT_ALL and texture drag. D1 has none of these and draws only stamped quads on 0.15 m; D2 has no
     ridges at all on the 0.25 m patch grid.
   - Anything else (normals, UVs, material, the reveal depth, how fast a tile shows after a wheel passes).
2. Which of the sheet's look parts matter most to the eye, and what does each cost? Rank: own soft depth from pass 1,
   ridges / bow wave, slump, smooth edge, skirt, texture drag, grid size.
3. How do we get the sheet's look at an acceptable cost? The sheet was parked for cost. Options to compare at least:
   - D1 with its own soft depth (not tied to the rut floor) plus a cheap slump / ridge pass, and how to cut its two
     big parts (per-frame live stamps + re-centre; ground reads on rebuild).
   - D2 with a pressed depth that does not wait for the cut (e.g. a look-only press on pass 1) plus ridges from the
     pushed-out volume, on the patch grid it already has.
   - The old MudSheet un-parked with only its cheapest look parts, client-only, view-limited.
   - A mix: D2 cheap base + a small D1-style detail mesh only right behind the wheels.
   For each: per-frame cost, build cost, memory, pop-in risk, MP risk (client-only look must not touch J2 / ops), and
   what it cannot do.

## Report back

Answer 1 in a short list (each point PROVEN-CODE with file:line, or INFERENCE). Then a ranked table for 2 and 3:
option, look parts it gets, per-frame cost estimate, build cost, memory, pop-in, MP risk, what to measure first.
Label PROVEN-CODE / MEASURED / [repo] / INFERENCE as before. End with one recommended route and the smallest DS
test (one session, a checklist) that would prove or kill it.
