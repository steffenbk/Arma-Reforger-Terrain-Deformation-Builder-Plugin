# Terrain deformation (Arma Reforger mod)

Deformable ground for Arma Reforger: vehicle ruts, craters, trenches, shovel digging and tunnels.

## Contents
- `Scripts/Game/Mud` - the ground system (runtime).
- `Scripts/WorkbenchGame/Mud` - World Editor plugins:
  - `MUD_GroundBuildPlugin.c` - Plugins menu 'MUD Ground': Next step / 1 Check world / 2 Add entities /
    3 Scan map / 4 Cut terrain, plus Status and Unbuild. Build puts the ground system into the open world;
    Unbuild removes everything it added and restores the terrain.
  - `MUD_RoadExportPlugin.c` - exports the world's road list.
- `Tools` - python tools the build plugin runs (`mud_build.py`, `mud_holes.py`, `ttile_holes.py`,
  `mud_world_copy.py`, `mud_roads.py`) and their tests (`python -m pytest -q Tools`).
- `Prefabs`, `Assets`, `Configs`, `Common`, `Data`, `UI`, `Terrains` - mod content.

## Using the build plugin
1. Open your world in the World Editor (a world with its own terrain tiles).
2. Plugins > 'MUD Ground: Next step', and repeat. Each step says what it did and what comes next.
3. 'MUD Ground: Unbuild' puts the world back to how it was.

Requires python on the PATH that Workbench sees. `mud_holes.py` / `mud_world_copy.py` read game paks with
`pakread.py` from blender-mcp-BK (`--bk <path>`).
