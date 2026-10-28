# Terrain deformation (Arma Reforger mod)

Add deformable ground to your map: vehicle ruts, craters, trenches, shovel digging and tunnels.
A World Editor plugin puts the system into your world in a few clicks, and takes it out again.

## What you need
- Arma Reforger Tools (Workbench) from Steam.
- Python 3 (python.org). Tick **Add python.exe to PATH** in the installer. Nothing else needs installing.
- A world in your own mod. It can use a vanilla map (Arland tested) or your own terrain.

## 1. Get the mod
Clone or download this repo into your Workbench addons folder:

```
git clone https://github.com/steffenbk/Terrain-deformation.git "%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\Terrain deformation"
```

(Or use Code > Download ZIP on GitHub and unpack it into that folder.)

## 2. Add it to your mod
1. Start Arma Reforger Tools. If it was already running, restart it so it sees the new folder and finds python.
2. Add Existing Project > pick `addon.gproj` in the folder above.
3. Add it as a dependency of your own mod, then open your mod.
4. Wait for the scripts to compile. The World Editor's Plugins menu now has the 'MUD Ground' entries.

## 3. Build the ground into your world
1. Open your world in the World Editor and save it.
2. Plugins > **MUD Ground: Steps 1-3**. This checks the world, adds the ground entities (layer
   `MUD_Ground`) and reads roads and heights, then starts Play to scan what must stay real terrain (roads,
   buildings). It stops at the first problem and says how to fix it.
3. Wait for the hint 'Scan done', then stop Play.
4. Plugins > **MUD Ground 4: Cut terrain**. This saves and closes the world, cuts the terrain holes, bakes the
   heights and opens the world again.
5. Press Play: the ground now deforms.

**Vanilla maps:** the game's files are never changed. Step 4 writes holed copies of the map's terrain tiles into
your mod (for Arland: `worlds/Arland/Terrain/.Data`). They are used only while your mod is loaded, and the
terrain tools stay locked as on any vanilla map. If the holes do not show in Play, restart Workbench once.
Everon and Kolguyev are not tested yet.

**Own terrain:** the holes go into your terrain files; the originals are backed up first.

## Change or remove it
- **MUD Ground: Status** shows what is in the world and the next step.
- **MUD Ground: Unbuild** removes everything the plugin added and gives the terrain back its original holes
  (on a vanilla map it removes the tile copies from your mod).
- After you change the map, run the steps again: the heights are always baked fresh.

## Troubleshooting
- 'Workbench could not start python': install Python 3 with Add to PATH ticked, then restart Workbench.
- 'no pak terrain fits the height samples': the world's terrain is neither its own nor a vanilla map.
- 'cannot find the game's data folder': restart Workbench once.
- No 'MUD Ground' entries in the Plugins menu: the scripts did not compile. Look in the Workbench log
  (`Documents/My Games/ArmaReforgerWorkbench/logs`) for errors and for lines starting with 'MUD build:'.
