# Terrain deformation (Arma Reforger mod)

Deformable ground for Arma Reforger: vehicle ruts, craters, trenches, shovel digging and tunnels.

## Contents
- `Scripts/Game/Mud` - the ground system (runtime).
- `Scripts/WorkbenchGame/Mud` - World Editor plugins:
  - `MUD_GroundBuildPlugin.c` - Plugins menu 'MUD Ground': puts the ground system into your world, and
    takes it out again (Unbuild).
  - `MUD_RoadExportPlugin.c` - exports the world's road list.
- `Tools` - python tools the build plugin runs (`mud_build.py`, `mud_holes.py`, `ttile_holes.py`,
  `pak_files.py`, `mud_roads.py`) and their tests.
- `Prefabs`, `Assets`, `Configs`, `Common`, `Data`, `UI`, `Terrains` - mod content.

## Install

### What you need
- Arma Reforger Tools (Workbench) from Steam.
- Python 3 (python.org). Tick **Add python.exe to PATH** in the installer. The tools use only the standard
  library, so nothing else needs to be installed.
- A world in your mod: either a vanilla map (Arland tested) or your own map with its own terrain (see step 3).

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

### 3. Pick the world
The build plugin cuts holes into the terrain files. It works on two kinds of world:
- **A vanilla map** (for example a world in your mod that uses Arland as its parent, like a Game Master
  subscene). The game's own files are never changed. The plugin finds the map's terrain in the game's .pak
  files and writes holed copies of its tiles into your mod at the same path (for Arland:
  `worlds/Arland/Terrain/.Data`). Those copies replace the game's tiles only while your mod is loaded. The
  World Editor terrain tools stay locked, as on any vanilla map. Tested on Arland; Everon and Kolguyev not yet.
- **Your own map**, whose terrain files sit in your mod next to the world file. The holes go straight into
  those files; the originals are backed up first.

### 4. Build the ground into your world
1. Open your world in the World Editor and save it (File > Save As if it is new).
2. Plugins > **MUD Ground: Steps 1-3** runs steps 1 to 3 in one go and starts Play for the scan (it stops
   at the first problem). When the scan is done, stop Play and run **MUD Ground 4: Cut terrain**.
   Or use **MUD Ground: Next step** and click it again after each step. Each step says what it did and
   what comes next:
   1. **Check world** - checks the terrain, the saved world file, the terrain files and python. Each
      problem comes with how to fix it.
   2. **Add entities** - adds the ground system to a new layer `MUD_Ground`. This is one Ctrl+Z step.
   3. **Scan map** - reads roads and heights, then starts Play to find what must stay real terrain
      (roads, buildings and similar). Wait for the hint 'Build scan done', then stop Play.
   4. **Cut terrain** - saves and closes the world, cuts the terrain holes and bakes the heights, then
      opens the world again. The original terrain files are backed up first. On a vanilla map it first
      copies the map's tiles from the game's .pak files into your mod (it reads where the game is installed
      from the Workbench log, so Stable and Experimental both work).
3. Press Play: the ground now deforms. On a vanilla map, if the holes do not show, restart Workbench once
   (the holed tiles are new files).

**MUD Ground: Status** shows what of the system is in the world and the next step.
**MUD Ground: Unbuild** removes everything the plugin added and puts the original terrain holes back. On a
vanilla map it moves the holed tile copies out of your mod, so the game's own tiles are used again.
Build again any time, for example after you change the map: Build always re-bakes the heights.

### Troubleshooting
- 'Workbench could not start python': install Python 3 with Add to PATH ticked, then restart Workbench.
- 'no pak terrain fits the height samples': the world's terrain is neither its own nor a vanilla map the
  plugin can find in the game's .pak files (step 3).
- 'cannot find the game's data folder': restart Workbench once, so its log names the game folder.
- No 'MUD Ground' entries in the Plugins menu: the scripts did not compile. Look in the Workbench log
  (`Documents/My Games/ArmaReforgerWorkbench/logs`) for errors and for lines starting with 'MUD build:'.
- Run the tool tests with `python -m pytest -q Tools` (needs `pip install pytest`).
