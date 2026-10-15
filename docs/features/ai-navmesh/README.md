# AI navmesh refresh

**Status: OFF** (code stays: Scripts/Game/Mud/MUD_NavRefresh.c). Dead end - see below.

## Details (moved from CLAUDE.md 2026-10-15, kept word for word)
- AI navmesh (2026-10-12): MUD_NavRefresh rebuilds the navmesh over craters/trenches/shovel holes after 8 s quiet (ruts skipped), entity category "AI navmesh". OFF by default: on the holed map any navmesh rebuild empties the tile (generator ignores our ground). Probe 2026-10-12: the generator ignores ALL script static bodies (any layer, trimesh or box, with or without MeshObject) - AI pathing around holes via navmesh rebuild is a dead end; AI keep the baked navmesh of the undug ground.
