# Archived test code (2026-10-15)

Moved out of `Scripts/Game/Mud` so it no longer compiles. Nothing was deleted. Full copy of the code before
the move: checkpoint v066 "pre-archive-test-code".

To bring one back: copy the file to `Scripts/Game/Mud`, drop the `.legacy` ending, put back its call lines from
the matching `*_snippets.c.legacy` file, recompile.

| File | What it was for | Called from (cut) |
|---|---|---|
| MUD_JoltProbe.c | driver correction (snap) logger, MP stutter hunt 2026-10-11/13 | WorldGround EOnFrame `MUD_JoltProbe.Tick` + `ProbeGround()` (both in MUD_WorldGround_snippets) |
| MUD_StutterRobot.c | robot driver (hold Space 3 s -> 20 s reverse circle) + MUD_AllowanceTest / MUD_PredictionTest (both off); modded SCR_CharacterControllerComponent + Vehicle | JoltProbe only |
| MUD_FrameLog.c | per-frame CSV ($profile/MUD_framelog_*.csv) during a robot run | JoltProbe / StutterRobot only |
| MUD_ServerLog.c | server timing log while a player drives (was off: MUD_SERVER_LOG_ON false); modded Vehicle | none (self-contained) |
| MUD_EngineProbes.c | papers plan Step G engine probes (domes, emat on 2 sections, collider patching) | WorldGround EOnFrame `MUD_EngineProbes.Tick` (MUD_WorldGround_snippets) |
| MUD_RadiusProbe.c (COPIED BACK to Scripts/Game/Mud 2026-10-17 for Build 3 Stage 9) | does WheelSetRadiusState move the body? ("radius ruts" idea); modded SCR_CharacterControllerComponent | none (self-contained, ON switch) |
| MUD_HoleCensus.c | "THROWAWAY TEST": lists terrain hole cells; entity, not placed in any layer | none |
| MUD_GetOutFallback.c.off | already disabled before the archive | none |
| MUD_WorldGround_snippets.c.legacy | the two Tick lines + ProbeGround() cut from MUD_WorldGround.c | - |
| MUD_MudSheet_snippets.c.legacy | TEMP DIAG (DiagRead, m_fDiagDG) + TEMP RUT FPS (PERF line split by wheel ground) cut from MUD_MudSheet.c | - |

Left in place on purpose:
- `MUD_FinePatch.m_fColChangeAt` and its two writes in WorldGround (cheap; only ProbeGround read it).
- MudSheet switches FREEZE_TEST, OFF_TEST, DEBUG_SHOW, DEBUG_TELEPORT, DebugQuad (all off; kept as debug options).
- MUD_TrenchSite DEBUG_INSTANT (user wants it as a test option).
- MUD_VoxelHouse.c + MUD_WallPanels.c (parked feature with its own input action, not throwaway test code).
- MUD_TerrainOffProbe.c (still placed in the old map `NEw map/unnamed_Layers/default.layer`).
- MUD_VoxelProbe.c / MUD_VoxelNet.c: named "probe" but this IS the shipped voxel system.

The MP robot test setup in project CLAUDE.md needs JoltProbe + StutterRobot + FrameLog back.

2026-10-17: MUD_JoltProbe.c, MUD_StutterRobot.c, MUD_FrameLog.c COPIED back to Scripts/Game/Mud (Build 0 test kit,
behind MUD_TestKit.TEST_KIT) with the JoltProbe Tick line + ProbeGround(). The .legacy files here stay as the archive.
