# Arma Reforger Terrain Deformation - map maker tools

Deformable ground for your Arma Reforger map: vehicle ruts, craters, trenches, shovel digging and tunnels.

The ground system itself is the Workshop mod **Terrain deformation** (GUID `6A940DE9528954EE`). It comes with
a ready Arland scenario and the World Editor plugin that builds the system into your own world.
This repo only holds the python tools that plugin runs. They can't run from inside the Workshop download, so you
get them here: **Terrain_deformation_map_maker_tools.zip**.

## What you need
- Arma Reforger Tools (Workbench) from Steam.
- Python 3 from python.org. Tick **Add python.exe to PATH** in the installer.
- Your own Workbench project.

## Setup
1. Download `Terrain_deformation_map_maker_tools.zip` and unzip it into your project's folder, so you get
   `<your project>/Tools/mud_build.py` next to your `addon.gproj`.
2. In Workbench, add **Terrain deformation** to your project's dependencies, then restart Workbench so it loads
   the mod and finds python.
3. Make your world a SubScene of a game map (Arland is tested), open it in the World Editor and save it.

## Build the ground into your world
1. Plugins > **MUD Ground: Status** checks python and the Tools folder.
2. Plugins > **MUD Ground: Steps 1-3** checks the world, adds the ground entities, makes the world its own
   scenario (`Missions/<world>_Mud.conf`) and starts Play to scan what must stay real terrain (roads, buildings).
   Wait for 'Scan done', then stop Play.
3. Plugins > **MUD Ground 4: Cut terrain** cuts the terrain holes and bakes the heights. The world is closed and
   opened again while python runs.
4. Press Play: the ground now deforms.

## Good to know
- The game files are never changed. Step 4 writes cut copies of the map's terrain tiles into your mod, so every
  world on that map is cut while your mod is loaded. Unload it to play the normal map.
- One built world per game map per mod: to build another world on the same map, run **MUD Ground: Unbuild** on
  the first one.
- **MUD Ground: Unbuild** takes everything out again (entities and terrain holes).
