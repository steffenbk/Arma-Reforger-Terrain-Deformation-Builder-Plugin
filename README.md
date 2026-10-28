# Arma Reforger Terrain Deformation Builder Plugin

Add deformable ground to your map: vehicle ruts, craters, trenches, shovel digging and tunnels.
Copy these files into your project, then a World Editor plugin builds the system into your world in a few
clicks, and takes it out again.

## What you need
- Arma Reforger Tools (Workbench) from Steam.
- Python 3 (python.org). Tick **Add python.exe to PATH** in the installer. Nothing else needs installing.
- Your own Workbench project with a world in it. The world can use a vanilla map (Arland tested) or your own
  terrain.

## 1. Copy the files into your project
1. Download this repo (Code > Download ZIP) and unpack it, or clone it:
   `git clone https://github.com/steffenbk/Arma-Reforger-Terrain-Deformation-Builder-Plugin.git`
2. Copy all its folders (`Assets`, `Common`, `Configs`, `Prefabs`, `Scripts`, `Tools`, `UI`) into your
   project's folder, next to your `addon.gproj`.
   - If your project already has `Configs/System/chimeraInputCommon.conf` or `keyBindingMenu.conf`, merge ours
     into yours instead of overwriting them (they hold the trench and build tool key bindings).
3. Start Arma Reforger Tools (restart it if it was running, so it sees the new files and finds python) and open
   your project. Wait for the scripts to compile. The World Editor's Plugins menu now has 'MUD Ground' entries.

## 2. Build the ground into your world
1. Open your world in the World Editor and save it.
2. Plugins > **MUD Ground: Steps 1-3**. This checks the world, adds the ground entities (layer
   `MUD_Ground`) and reads roads and heights, then starts Play to scan what must stay real terrain (roads,
   buildings). It stops at the first problem and says how to fix it.
3. Wait for the hint 'Scan done', then stop Play.
4. Plugins > **MUD Ground 4: Cut terrain**. This saves and closes the world, cuts the terrain holes, bakes the
   heights and opens the world again.
5. Press Play: the ground now deforms.

**Vanilla maps:** the game's files are never changed. Step 4 writes holed copies of the map's terrain tiles into
your project (for Arland: `worlds/Arland/Terrain/.Data`). They are used only while your project is loaded, and the
terrain tools stay locked as on any vanilla map. If the holes do not show in Play, restart Workbench once.
Everon and Kolguyev are not tested yet.

**Own terrain:** the holes go into your terrain files; the originals are backed up first.

## Change or remove it
- **MUD Ground: Status** shows what is in the world and the next step.
- **MUD Ground: Unbuild** removes everything the plugin added and gives the terrain back its original holes
  (on a vanilla map it removes the tile copies from your project).
- After you change the map, run the steps again: the heights are always baked fresh.

## Troubleshooting
- 'Workbench could not start python': install Python 3 with Add to PATH ticked, then restart Workbench.
- 'Tools/mud_build.py not found': the `Tools` folder is missing from your project (step 1).
- 'no pak terrain fits the height samples': the world's terrain is neither its own nor a vanilla map.
- 'cannot find the game's data folder': restart Workbench once.
- No 'MUD Ground' entries in the Plugins menu: the scripts did not compile. Look in the Workbench log
  (`Documents/My Games/ArmaReforgerWorkbench/logs`) for errors and for lines starting with 'MUD build:'.
