# Driver stutter hunt - archived test code (2026-10-11 ~16:35)

Record of the hunt: docs/superpowers/research/2026-10-11-driver-stutter-deep-dive.md. Proven facts: docs/superpowers/research/mud-gotchas.md (stutter section).

Files are renamed *.c.bak so Workbench does not compile them. To restore one, copy it to Scripts/Game/Mud/ and drop .bak.

- MUD_StutterRobot.c - robot driver (hold Space 3 s in the driver seat, stopped -> 20 s reverse circle), MUD_AllowanceTest, MUD_PredictionTest, Nearby() physics-entity list, modded Vehicle + SCR_CharacterControllerComponent hooks.
- MUD_JoltProbe.c - per-frame body/visual jump detector ('MUD corr' lines). Called from MUD_WorldGroundEntity.EOnFrame and, on worlds without our ground, from the robot's OnPrepareControls hook.
- MUD_FrameLog.c - per-frame client CSV while a robot run is live ($profile:MUD_framelog_*.csv). Called from MUD_JoltProbe.Tick.
- MUD_ServerLog.c - server per-second timing line ('MUD srv:') + per-frame CSV ($profile:MUD_srvlog_*.csv). Standalone modded Vehicle.
- MUD_WorldGround.c / MUD_FinePatch.c - snapshots WITH the probe hooks (JoltProbe.Tick call, ProbeGround, m_fColChangeAt, hadBody, TEMP_SKIN_ONLY).
- MUD_Wheels.c - snapshot (not TEMP; holds NET_DIST etc.).
- *.py - analysis scripts (framelog compare, per-area, snap positions).
