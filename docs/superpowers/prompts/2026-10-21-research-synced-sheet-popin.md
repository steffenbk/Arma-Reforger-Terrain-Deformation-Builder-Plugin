# Research prompt: synced sheet on a real server - far track pop-in, sync memory, per-client op cost (2026-10-21)

You are the research AI for the Arma Reforger mud mod (Enfusion script, addon "Testing plan vech msh"). Research
and propose; do not write the build. Label every claim PROVEN-CODE (file:line), MEASURED (log / doc), [repo] or INFERENCE.

BUDGET: about 10 USD of credit for this whole job. Read only the files below (grep inside the big ones, do not read
MUD_MudSheet.c / MUD_WorldGround.c / MUD_VoxelProbe.c whole - they are 5000-11000 lines). One pass, no re-reads, a
short report. If you run low, finish Problem 1 first.

Read first:
- docs/superpowers/results/2026-10-21-mud-lab-stage3-measure.md sec 12 (the hosted-server session: every 5 s window,
  pop-in evidence, other problems) - the source for everything below
- docs/features/mud-sheet/README.md (top: lessons 1-8, why it was parked), docs/superpowers/research/mud-gotchas.md
  (grep it for 'sheet', 'MeshObject', 'LOD' only)
- Code (grep): Scripts/Game/Mud/MUD_MudSheet.c (RADIUS 212, RADIUS_HYST 213, SPARSE 234, SPEED_RADIUS 250 / _K 253,
  LOD_M 586, QueueNear, BuildTile, PatchLiftOn), MUD_MudSync.c (OnHere 177, RutOp 219, m_mTiles 54 / Set 867, tile
  class 2185, SERVER_LAYER 26, LoadSnap 2013), MUD_VoxelProbe.c (WEAR_PASSES 289, the rut wear path), MUD_LookRuts.c.

Hard limits: the voxel ruts stay (RUTS STAY VOXEL, RUT WEAR v234: pass 1 on grass only counts wear); rut ops stay
floor-blind (J2); no custom shaders; a script mesh change = a new MeshObject (in-place UpdateVerts never shows);
script meshes have no vertex colour; 64 locals per method; perf only on a DS / standalone game; PvP only. Goal is the
sheet over the WHOLE Arland offroad, MP + JIP, many trucks.

## What we measured (standalone game on the hosted server, one human-driven truck, client log only)
- Offroad driving: 91 fps, MUD script 3.41 ms of 10.98 ms (sheet 2.12, sheet sync 0.51); 3 frames over 33 ms.
- ~90 % of wheel samples were 'only worn (no cut)' (16138 vs 1715 cut): a drive-through track exists almost only as
  sheet mud. The sheet exists only inside the build ring: RADIUS 40 m, shrunk by SPEED_RADIUS down to 20 m when fast,
  meshes dropped past 46 m; 'to build 0' while driving (no backlog). Tiles past LOD_M 20 m are coarse, 251-305
  rebuilt to full per 10 s inside 17 m. SPARSE was false (no patch lift).
- User's eyes: "pretty decent fps, but a lot of pop-in of tracks from far away".
- MUD_MudSync m_mTiles is only ever Set, never removed: 'tiles touched' 702 after ~75 s of driving (~11.2 KB a
  tile by the code -> ~8 MB). Every machine runs every rut op (no distance check in RutOp / OnHere).
- 4 robot-style 'mine 1' corrections in ~75 s, two next to 52 / 62 ms frames.

## Problem 1 (main): far track pop-in
Tracks appear at the ring edge. A bigger ring only moves the pop out and costs builds.
Questions:
1. Confirm or reject from the code: is the ring the pop (QueueNear / drop at RADIUS + RADIUS_HYST / SPEED_RADIUS),
   and what does the ground show outside the ring for a worn-only track (nothing? a 3-6 cm cut?).
2. Options that give a track a cheap far look outside the ring, ranked by look, ms per frame and memory, at least:
   - a coarse far mesh (one big low-detail mesh per 32-64 m block) from the sync layer's depth data;
   - cutting / darkening the base patch mesh (MUD_WorldGround patch) for worn-only passes, look only (no collider,
     not J2) - what it would cost in patch rebuilds;
   - decals or a texture swap per track segment (note: CreateDecal returns null in this build, decals hidden under
     our ground offroad - mud-gotchas);
   - a fade-in at the ring edge instead of a pop (alpha is not available on script meshes - check what is);
   - no speed shrink of the ring (SPEED_RADIUS off) and what that costs.
   For each: what the player sees at 50 / 100 / 200 m, builds per 10 s, MP risk, what it cannot fix.

## Problem 2: sync layer memory and cost on a full server
3. A safe way to free or cap m_mTiles (far from every player, old, stored in the snapshot only) that keeps late
   joiners (LoadSnap / SnapParts) and the J2 replay exact. What must the server keep?
4. Per-client cost of RutOp for trucks far away: can a client skip or defer far ops (store only, press later when it
   comes near) without breaking the op order the snapshot needs? Estimate the cost for 10 / 30 trucks.

## Report back
Short answers 1-4, each claim labelled. One ranked table for Problem 1 (option, how, far look at 50 / 100 / 200 m,
ms per frame, memory, MP risk, what to measure first). One recommended plan per problem, and the smallest test that
proves it: one Workbench run for the look, one DS or standalone run for the cost, as a numbered checklist.
