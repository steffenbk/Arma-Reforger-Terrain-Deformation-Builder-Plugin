# Terrain deformation (Arma Reforger mod)

Deformable ground for Arma Reforger: vehicle ruts, craters, trenches, shovel digging and tunnels.

## Contents
- `Scripts/Game/Mud` - the ground system (runtime).
- `Scripts/WorkbenchGame/Mud` - World Editor plugins:
  - `MUD_GroundBuildPlugin.c` - Plugins menu 'MUD Ground': puts the ground system into your world, and
    takes it out again (Unbuild).
  - `MUD_RoadExportPlugin.c` - exports the world's road list.
- `Tools` - python tools the build plugin runs (`mud_build.py`, `mud_holes.py`, `ttile_holes.py`,
  `mud_world_copy.py`, `mud_roads.py`) and their tests.
- `Prefabs`, `Assets`, `Configs`, `Common`, `Data`, `UI`, `Terrains` - mod content.

## Install

### What you need
- Arma Reforger Tools (Workbench) from Steam.
- Python 3 (python.org). Tick **Add python.exe to PATH** in the installer. The tools use only the standard
  library, so nothing else needs to be installed.
- Only for making a world copy (see step 3): the blender-mcp-BK folder with `tools/pakread.py`, which reads the
  game paks. It is not published yet.

### 1. Get the mod
Clone or download this repo into your Workbench addons folder:

```
git clone https://github.com/steffenbk/Terrain-deformation.git "%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\Terrain deformation"
```

(Or use Code > Download ZIP on GitHub and unpack it into that folder.)

### 2. Open it in Workbench
1. Start Arma Reforger Tools. If it was already running, restart it so it sees the new folder and finds python
   on the PATH.
2. Add Existing Project > pick `addon.gproj` in the folder above, then open it.
3. Wait for the scripts to compile. The World Editor's Plugins menu now has the 'MUD Ground' entries.

### 3. Have a world with its own terrain
The build plugin cuts holes into the terrain files, so it only works on a world that owns its terrain
(the terrain files sit next to the world file, in the mod). It refuses vanilla worlds such as Arland:
changing those would change the map for everyone.

- If your map is your own terrain: nothing to do.
- If you want a vanilla map (for example Arland): make your own copy first with `Tools/mud_world_copy.py`.
  Open a command prompt in the mod folder and run:

  ```
  python -I Tools/mud_world_copy.py --world worlds/Arland/Arland.ent --gm worlds/GameMaster/GM_Arland.ent --name MyArland --addon . --bk <path to blender-mcp-BK>
  ```

  Then **restart Workbench** (it has to learn the new world before it can load it) and open
  `worlds/MyArland/MyArland.ent`. Point your scenario's SubScene parent at the line the tool prints.

### 4. Build the ground into your world
1. Open your world in the World Editor and save it (File > Save As if it is new).
2. Plugins > **MUD Ground: Next step**. Click it again after each step. Each step says what it did and
   what comes next:
   1. **Check world** - checks the terrain, the saved world file, the terrain files and python. Each
      problem comes with how to fix it.
   2. **Add entities** - adds the ground system to a new layer `MUD_Ground`. This is one Ctrl+Z step.
   3. **Scan map** - reads roads and heights, then starts Play to find what must stay real terrain
      (roads, buildings and similar). Wait for the hint 'Build scan done', then stop Play.
   4. **Cut terrain** - saves and closes the world, cuts the terrain holes and bakes the heights, then
      opens the world again. The original terrain files are backed up first.
3. Press Play: the ground now deforms.

**MUD Ground: Status** shows what of the system is in the world and the next step.
**MUD Ground: Unbuild** removes everything the plugin added and puts the original terrain holes back.
Build again any time, for example after you change the map: Build always re-bakes the heights.

### Troubleshooting
- 'Workbench could not start python': install Python 3 with Add to PATH ticked, then restart Workbench.
- 'the terrain is not this world's own': make a world copy (step 3).
- No 'MUD Ground' entries in the Plugins menu: the scripts did not compile. Look in the Workbench log
  (`Documents/My Games/ArmaReforgerWorkbench/logs`) for errors and for lines starting with 'MUD build:'.
- Run the tool tests with `python -m pytest -q Tools` (needs `pip install pytest`).
