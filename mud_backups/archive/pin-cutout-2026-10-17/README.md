# Pin cut-out tools (archived 2026-10-17)

Moved out after the whole-map keep mask + pin list were made (checkpoint v164 = before the move). Nothing deleted.
Results: docs/superpowers/results/2026-10-17-mask2.txt, 2026-10-17-pins2.txt (+ area-*.txt for the test square).

| File | What it was | Call site (removed) |
|---|---|---|
| MUD_PinProbe.c | RELATIVE_Y root census by class + lift test + road probe | MUD_WorldGroundEntity.EOnFrame: `MUD_PinProbe.Tick(m_fTime);` after MUD_VoxelProbe.Tick |
| MUD_KeepScan.c | play-time scan: new keep mask (no trails) + pin list (MUD_PinLift.KeepsTerrain roots) on the holed map | same place: `MUD_KeepScan.Tick(m_fTime);` |
| MUD_RoadListPlugin.c | World Editor plugin "MUD List Roads" -> $profile:mud_roads.txt (trail flag per RoadEntity origin) | Scripts/WorkbenchGame/Mud (no call site) |

Re-run after a game update (new roads / trees): copy the three back (drop .legacy; plugin to Scripts/WorkbenchGame/Mud,
the other two to Scripts/Game/Mud), re-add the two Tick lines, run the plugin on MudArland, Play ~60 s, then
mud_holes.py write <mask2> --pins <pins2> -> bake. Rename profile/mud_roads.txt back from mud_roads.txt.done.
Known limit: 'trail hits skipped 0' - the trace filter never sees RoadEntity, so a real road under a trail is
dropped with the trail.
