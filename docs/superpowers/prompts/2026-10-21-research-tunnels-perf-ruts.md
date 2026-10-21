# Research prompt: dark tunnels, idle frame cost + release gate, first-pass rut look (2026-10-21)

You are the research AI for the Arma Reforger mud mod (Enfusion script, addon "Testing plan vech msh", repo
steffenbk/Terrain-deformation, branch claude/upbeat-lovelace-q0w022). Research and propose; do not write the build.
Label every claim PROVEN-CODE (file:line), MEASURED (log / doc + line), WEB (url), [repo] or INFERENCE.

BUDGET: about 10 USD of credit for the whole job. Grep inside the big files, never read MUD_WorldGround.c (~10.4k
lines), MUD_VoxelProbe.c (~11k) or MUD_MudSheet.c whole. One pass, no re-reads, a short report. If you run low:
Problem 2 first, then 3, then 1.

State of the repo (read this, it changes what you see):
- The mud sheet is PARKED (MUD_MudSheet.ENABLED false, FAR_PRESS false). Ignore sheet ideas unless a finding is a
  cost that still runs with ENABLED false.
- Craters / trenches / shovel are on the heightfield (MUD_VoxelProbe.SURFACE_OPS false); voxel bricks only for tunnels.
- These scripts are ARCHIVED (moved off the PC build 2026-10-21) but still in git because deletions are not committed:
  MUD_DecalProbe.c, MUD_RoadHideProbe.c, MUD_RoadLookProbe.c, MUD_GrassCutProbe.c, MUD_TrackStamps.c. Treat them as
  gone. Also removed from the PC build but maybe still in this commit's history: MESHER_BENCH, PARTIAL_CHECK,
  BAND_FILL_CHECK (MUD_WorldGround), SCAN_CHECK_EVERY, CRUDE_AB_OPS, RUT_SHEET_ONLY_TEST, TUNNEL_DIAG_SMASK (VoxelProbe).
- Latest changes, NOT compiled yet: craters back to the big preset + m_fCraterNoRimChance 0.5; FIELD_SOIL_D 0.06 ->
  0.005 (rut dirt shows from the first real cut, pass 2).

Hard limits: no custom shaders (IDA 2026-10-16); a script mesh change = a new MeshObject (in-place UpdateVerts never
shows); script meshes have no vertex colour; CreateDecal returns null even when a decal is made, and decals are hidden
under our ground offroad; 64 locals per method; RUTS STAY VOXEL, rut ops stay floor-blind (J2 replay); PvP only;
perf is judged only on a DS / standalone game - Workbench numbers are clues, not results. Goal: whole Arland, MP + JIP.

Always read: docs/superpowers/research/mud-gotchas.md (grep per problem, do not read whole), CLAUDE.md (project root).

## Problem 1: dark tunnels (asked again by the user; PARKED since 2026-10-17)
Tunnels are lit like daylight inside. Read: docs/features/trenches-tunnels/README.md ('Tunnel darkness'),
mud-gotchas.md lines ~476-478 (three PROVEN WB 2026-10-17 entries), docs/superpowers/plans/2026-10-21-mud-lab-stage3-plan.md
sec 5 (Part E, the untried steps). Code: MUD_VoxelProbe.c TUNNEL_DARK (~485), DARK_MAT (~575), TunnelDarkCell
(~6092), DirtMat; prefabs Prefabs/Mud/MudTunnelProbe.et + _1.._4; materials Assets/Mud/Data/MUD_TunnelDark*.emat.
The raw 2026-10-17 logs are gone (Workbench keeps ~55 log folders); the gotchas entries are the record.
What is proven: space outside every probe box = full outdoor ambient; probe per grid cell (4 m / 2.5 m up, BoxLerp
0.15) fixed the bright pockets; lit spots left inside full probe cover are AMBIENT (IBL) on our wall material; a wall
material with DiffuseIBL / SpecularIBL 0 removes them but everything goes flat black; our heightfield / brick tops
(MatTerrainObject) cast NO sun shadow; lids did not help. Never tried: the probe exposure override
(EnableHDRModifications, UserEVMode, UserEV_LV on GameEnvironmentProbeEntity); the graded IBL materials by roof depth.
Questions:
1. Find working implementations of dark interiors in Enfusion: vanilla caves, bunkers, cellars, building interiors
   (which entity / prefab / probe / volume settings they use - ProbeEnvCave_base and its children, interior light
   volumes, any 'indoor' flag), Workshop mods with tunnels or bunkers, Bohemia docs / forums / Discord notes on probes,
   EV / exposure and interior lighting. Give the exact props and values they set.
2. Rank ways to get a believable dark tunnel (not flat black): graded IBL walls by roof depth; probe exposure override;
   a shadow-casting lid (which material / entity type casts sun shadow and could sit over carved bricks); spawned
   point lights / fake ambient; a vanilla interior prefab spawned per tunnel cell. For each: what it fixes, what it
   cannot (mouth glow, eye adaptation pumping when looking at the mouth), cost per tunnel cell, MP (must be client
   look only, no op / J2 change).
3. The smallest WB look test that decides between the top two (numbered steps, which diag menus to watch).

## Problem 2: why Workbench play is slow even standing still + a release gate for debug code
User: "in just Workbench alone sitting still we should have much higher frame rate like we do when we start it in DS".
Data: docs/superpowers/results/2026-10-21-wb-prof-rows.txt (every 'MUD prof 5 s' row of four WB runs, full table).
Standing still, 21:20:10-21:20:55 (log 21-12-36): 51.5-60.6 fps, render 5.6-6.4 ms, simulation 2.3-2.5 ms, MUD script
0.42-0.55 ms. Standing still 21:47:50-21:48:00 (log 21-45-22): 92.9-94.1 fps, render 3.2-3.3, sim 2.1-2.3, MUD
0.41-0.57. Same MUD cost, very different fps -> the gap is NOT in the parts MUD_Prof measures (INFERENCE from those
rows). The idle 'MUD prof parts' lines in those windows show no part above 0.45 ms avg (most <= 0.1).
Questions:
4. List per-frame or per-entity code the profiler does NOT cover: every EOnFrame / EOnPostFrame / OnFrame / CallLater
   loop / component Tick in Scripts/Game/Mud (e.g. MUD_CoarseSkin.EOnFrame runs per chunk entity - how many exist?),
   modded vanilla classes (grep 'modded class'), components on every vehicle / character, MUD_CameraSmooth,
   MUD_KeepScan, MUD_StutterRobot, MUD_VehicleAccess (TEMP fall probe, OnMud always-true), Print / log volume, the
   MUD_Prof CSV row per frame. Estimate each one's idle cost and say how to measure it.
5. Render side: how many of our entities / MeshObjects / draw sections exist at idle (patches, coarse chunks, skin,
   bricks), and does render time scale with that (render 3.2 vs 6.2 ms between the runs)? Cheaper options (merge
   far patches, hide far ones, LOD).
6. Is anything WB-only (m_bDebug on the MudWorldGround prefab, diag menus, debug shapes, script debugger) that a DS
   or standalone client skips? What fps gap between WB and the game is normal for Enfusion (WEB)?
7. RELEASE GATE: list every switch that must be off before publishing, with file:line. Already known ON now:
   MUD_Prof.ON + CSV, MUD_TestKit.TEST_KIT, MUD_StutterRobot.ON, MUD_BandMesher.LAYOUT_CHECK (TEMP),
   MUD_MudSheet.EDGE_AUDIT, MUD_VoxelProbe.TRAIL_PROBE (TEMP), TUNNEL_DEBUG_T (T key), MUD_TrenchSite.ALLOW_INSTANT_DIG,
   CRATER_DECAL (CreateDecal always returns null - dead cost?), prefab MudWorldGround.et m_bDebug 1. Find the rest.
   Propose ONE mechanism that fails loudly if any is left on in a published build (e.g. one RELEASE const that the
   others read, plus a startup Print / error listing what is on). Also: remember.md says '$Testingplanvechmsh:' paths
   break under the published addon ID (5 s freeze) - grep them and say what to change.

## Problem 3: show the rut path on the first drive without a perf hit
Read: docs/features/ruts/README.md (steps 4-13 and the end: RUT WEAR, DIRT HIGHER), CLAUDE.md 'RUT WEAR',
docs/superpowers/research/2026-10-20-mud-look.md, 2026-10-19-ruts-invisible.md (skim conclusions only).
Today: pass 1 on grass only counts wear (MUD_VoxelProbe.WEAR_PASSES, WearOnly ~5066) - NOTHING is drawn, so a first
drive leaves no visible track. Later passes cut RUT_PASS_MAX 0.03 m; dirt reveal at FIELD_SOIL_D (now 0.005) plus
ragged noise RAG_D 0.035 (MUD_WorldGround RevealT ~7637); client look ruts (MUD_LookRuts) skip worn-only samples and
mesh at most every LOOK_MESH_EVERY 0.25 s. Proven: slow, shallow pressing feels and costs best (mesh ruts cost more
on the DS: results/2026-10-19-mesh-vs-voxel-ruts-ds.md). User: "how do we then show the dirt - can the grass and
dirt layer be closer, or how to optimise showing that".
Questions:
8. With FIELD_SOIL_D 0.005, what does pass 2 actually show (RevealT + RAG_D / RAG_BLEND)? Is RAG_D 0.035 now hiding
   most of the dirt on a 3 cm cut - should the noise scale with FIELD_SOIL_D?
9. Cheap ways to make pass 1 (wear only) visible with NO collider change, NO extra J2 state and no more mesh builds
   than a look rut costs: e.g. a 1-2 cm look-only press drawn by the existing look rut path; a dirt / flattened-grass
   material on the patch mesh for worn cells (material split = which mesh, how many rebuilds); grass clutter removal
   on worn cells (is there any script way - ClutterOccluder did NOT cut grass, mud-gotchas 2026-10-18); a decal per
   wheel segment despite CreateDecal returning null (decals DO show on asphalt only). Rank by look, builds per 10 s
   per truck, MP risk (look only, client side), and what each cannot show.
10. Is there a way to show the dirt without a cut at all (texture / UV / material swap of the existing patch triangles)
    and what it costs per patch rebuild?

## Problem 4 (if budget is left): anything else slowing us down
11. Any per-op or per-rebuild hot path you spot while grepping (loops over whole boxes per op, map iteration per
    frame, string building in hot loops, Print every frame). One line each, file:line, estimated cost.

## Report back
Short answers 1-11, each claim labelled. Per problem: one ranked table and one recommended plan with the smallest
test that proves it - a WB look test for looks / function, a DS or standalone run for any cost - as a numbered
checklist. For Problem 2, a separate copy-paste RELEASE CHECKLIST (switch, file:line, release value).
