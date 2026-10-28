#ifdef WORKBENCH
//! Ground system Build / Unbuild (2026-10-22, user: "click Build to apply the system to their world ... click
//! Unbuild, which removes everything the plugin added ... click Build again to restore"). 2026-10-23 (user: progress,
//! easier for anyone, plugins in steps): menu 'MUD Ground: Next step' + numbered steps 1 Check / 2 Add entities /
//! 3 Scan map / 4 Cut terrain, progress bars (WBProgressDialog) on height sampling + the python run, scan percent
//! hint in Play. NOT COMPILED since this change.
//! Architecture + test: docs/features/build-plugin/README.md. World Editor menu: Plugins > 'MUD Ground: Build /
//! Unbuild / Status'.
//!  - Entities (stage 1): the layer LAYER in the open world's current subscene gets the SYSTEM prefabs, each named
//!    NAME_PREFIX + its role, at the terrain centre. One edit action. A system entity already in the world OUTSIDE
//!    the layer (hand-placed, as in MudArland) is reported and NOT duplicated - and then the terrain steps are
//!    skipped (that world was set up by hand with Tools/mud_holes.py).
//!  - Manifest (stage 2): DIR/<world name>/manifest.txt holds the world, the state (scan / built / failed /
//!    unbuilt), the tile folder, the tile backup and the heights file. Status reads it.
//!  - Terrain (stages 3-4), two Build runs:
//!    run 1 writes the road list (ROADS, for MUD_KeepScan), height samples of the terrain and REQUEST, then starts
//!      Play. MUD_KeepScan sees REQUEST and writes the keep mask + pins (hint 'Build scan done').
//!    run 2 (after Play is stopped) saves the world, opens BLANK_WORLD so the tiles are not in use, runs
//!      Tools/mud_build.py build (fit, tile backup, holes, heights) and waits, opens the world again, points the
//!      skin at the heights file and saves.
//!    Unbuild deletes our entities, saves, and with the terrain built runs mud_build.py unbuild (the pristine
//!      holes back into every tile, the heights file moved into the backup folder).
//! INFERENCE (untested): SetOpenedResource releases the tiles of the closed world; the reopened world shows the
//! new holes; GetAbsolutePath resolves '$profile:'; 'python' is on the PATH Workbench sees.
//------------------------------------------------------------------------------------------------
//! DIR/<world>/manifest.txt: one 'key value' line each (value = the rest of the line)
class MUD_BuildManifest
{
	string m_sWorld;
	string m_sState;
	string m_sTiles;
	string m_sBackup;
	string m_sHeights;
	string m_sHeightsAbs;
	float m_fCell;
	int m_iTilesX;
	string m_sVanilla;      // vanilla mode: the pak terrain's entity name (empty = the world's own tiles)
	string m_sGameData;     // vanilla mode: the game's addons/data folder (the .pak files)
	string m_sAddon;        // vanilla mode: root folder of the mod that holds the world (override tiles go under it)

	//------------------------------------------------------------------------------------------------
	bool Load(string path)
	{
		FileHandle f = FileIO.OpenFile(path, FileMode.READ);
		if (!f)
			return false;
		string line;
		while (f.ReadLine(line) >= 0)
		{
			line = line.Trim();
			int sp = line.IndexOf(" ");
			if (sp <= 0)
				continue;
			string k = line.Substring(0, sp);
			string v = line.Substring(sp + 1, line.Length() - sp - 1).Trim();
			if (k == "world")
				m_sWorld = v;
			else if (k == "state")
				m_sState = v;
			else if (k == "tiles")
				m_sTiles = v;
			else if (k == "backup")
				m_sBackup = v;
			else if (k == "heights")
				m_sHeights = v;
			else if (k == "heights_abs")
				m_sHeightsAbs = v;
			else if (k == "cell")
				m_fCell = v.ToFloat();
			else if (k == "tiles_x")
				m_iTilesX = v.ToInt();
			else if (k == "vanilla")
				m_sVanilla = v;
			else if (k == "game_data")
				m_sGameData = v;
			else if (k == "addon")
				m_sAddon = v;
		}
		f.Close();
		return !m_sState.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	bool Save(string path)
	{
		FileHandle f = FileIO.OpenFile(path, FileMode.WRITE);
		if (!f)
			return false;
		f.WriteLine("world " + m_sWorld);
		f.WriteLine("state " + m_sState);
		f.WriteLine("tiles " + m_sTiles);
		f.WriteLine("backup " + m_sBackup);
		f.WriteLine("heights " + m_sHeights);
		f.WriteLine("heights_abs " + m_sHeightsAbs);
		f.WriteLine(string.Format("cell %1", m_fCell));
		f.WriteLine(string.Format("tiles_x %1", m_iTilesX));
		if (!m_sVanilla.IsEmpty())
		{
			f.WriteLine("vanilla " + m_sVanilla);
			f.WriteLine("game_data " + m_sGameData);
			f.WriteLine("addon " + m_sAddon);
		}
		f.Close();
		return true;
	}
}

class MUD_GroundBuilder
{
	static const string LAYER = "MUD_Ground";
	static const string NAME_PREFIX = "MUD_SYS_";
	static const int SYSTEM_N = 2;
	static const string DIR = "$profile:MUD_Build/";
	static const string REQUEST = "$profile:MUD_Build/request.txt";      // = MUD_KeepScan.REQUEST
	static const string ROADS = "$profile:mud_roads.txt";                // = MUD_KeepScan.ROADS
	static const string BLANK_WORLD = "worlds/Arland/EmptyArland.ent";   // vanilla, own terrain in the pak (not ours)
	//! Vanilla mode writes over a PAK terrain (Arland, Everon, Kolguyev), so it opens a world on none of them: the
	//! MP test world has its own small MPTestTerrain (pak listing 2026-10-28 Stable, worlds001.pak)
	static const string BLANK_WORLD_VANILLA = "worlds/MP/MpTest/MpTest_Empty.ent";
	static const string GAME_DATA = "auto";   // mud_build.py reads the game's .pak folder from the Workbench log ('$ArmaReforger:' is packed: GetAbsolutePath fails, WB 2026-10-28)
	static const string OVERRIDE_LINE = "MUD build: override tiles ";   // mud_build.py report: the tiles dir it wrote
	static const string PYTHON = "python";
	static const int PY_WAIT_MS = 1800000;      // 30 min
	static const int SAMPLE_STRIDE = 3;         // height samples in every 3rd skin chunk (x and z)

	static bool s_bPlay;    // Build run 1 asks Run() to start Play after the dialog

	//------------------------------------------------------------------------------------------------
	//! System entity i: prefab, role name, class (for the hand-placed check)
	static void SystemEntry(int i, out ResourceName prefab, out string role, out string cls)
	{
		if (i == 1)
		{
			prefab = "{4D5544534B494E30}Prefabs/Mud/MudCoarseSkin.et";
			role = "CoarseSkin";
			cls = "MUD_CoarseSkinEntity";
			return;
		}
		prefab = "{C015189C825FFA71}Prefabs/Mud/MudWorldGround.et";
		role = "WorldGround";
		cls = "MUD_WorldGroundEntity";
	}

	//------------------------------------------------------------------------------------------------
	protected static WorldEditorAPI Api(out WorldEditor we)
	{
		we = Workbench.GetModule(WorldEditor);
		if (!we)
			return null;
		return we.GetApi();
	}

	//------------------------------------------------------------------------------------------------
	//! Is this entity source ours: in our layer with our name prefix, or our prefix on a system class
	protected static bool Ours(WorldEditorAPI api, int subScene, IEntitySource src)
	{
		string name = src.GetName();
		if (!name.StartsWith(NAME_PREFIX))
			return false;
		string path = api.GetSubsceneLayerPath(subScene, src.GetLayerID());
		if (path.Contains(LAYER))
			return true;
		return SystemIndex(src.GetClassName()) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static int SystemIndex(string cls)
	{
		for (int i = 0; i < SYSTEM_N; i++)
		{
			ResourceName prefab;
			string role;
			string c;
			SystemEntry(i, prefab, role, c);
			if (c == cls)
				return i;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills ours (sources we made) and found[i] = 1 when system entity i exists (ours or hand-placed);
	//! handPlaced lists system entities that are not ours
	protected static void Scan(WorldEditorAPI api, int subScene, array<IEntitySource> ours, array<int> found, array<string> handPlaced)
	{
		for (int i = 0; i < SYSTEM_N; i++)
			found.Insert(0);
		for (int e = 0, count = api.GetEditorEntityCount(); e < count; e++)
		{
			IEntitySource src = api.GetEditorEntity(e);
			if (!src)
				continue;
			int si = SystemIndex(src.GetClassName());
			if (Ours(api, subScene, src))
			{
				ours.Insert(src);
				if (si >= 0)
					found[si] = 1;
				continue;
			}
			if (si >= 0)
			{
				found[si] = 1;
				handPlaced.Insert(string.Format("%1 '%2' in layer %3", src.GetClassName(), src.GetName(),
					api.GetSubsceneLayerPath(subScene, src.GetLayerID())));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! World path without a {GUID} or $addon: prefix, forward slashes ("worlds/X/X.ent")
	static string PlainPath(string path)
	{
		path.Replace("\\", "/");
		int close = path.IndexOf("}");
		if (path.StartsWith("{") && close > 0)
			path = path.Substring(close + 1, path.Length() - close - 1);
		int colon = path.IndexOf(":");
		if (path.StartsWith("$") && colon > 0)
			path = path.Substring(colon + 1, path.Length() - colon - 1);
		while (path.StartsWith("/"))
			path = path.Substring(1, path.Length() - 1);
		return path;
	}

	//------------------------------------------------------------------------------------------------
	//! "worlds/X/MyMap.ent" -> "MyMap"
	static string WorldKey(string worldPath)
	{
		string p = PlainPath(worldPath);
		int slash = p.LastIndexOf("/");
		if (slash >= 0)
			p = p.Substring(slash + 1, p.Length() - slash - 1);
		if (p.EndsWith(".ent"))
			p = p.Substring(0, p.Length() - 4);
		return p;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Abs(string path)
	{
		string abs;
		if (!Workbench.GetAbsolutePath(path, abs, false))
			return string.Empty;
		abs.Replace("\\", "/");
		if (path.EndsWith("/") && !abs.EndsWith("/"))
			abs += "/";
		return abs;
	}

	//------------------------------------------------------------------------------------------------
	//! Tools/mud_build.py of the project that holds this plugin, empty when not found
	protected static string ToolPath()
	{
		// plain path first: the files may be copied into any project (GitHub setup, user 2026-10-28)
		array<string> tries = {"Tools/mud_build.py", "$Testingplanvechmsh:Tools/mud_build.py", "$Terraindeformation:Tools/mud_build.py"};   // lint-ok: fallbacks after the plain path
		foreach (string t : tries)
		{
			string abs;
			if (Workbench.GetAbsolutePath(t, abs, true))
			{
				abs.Replace("\\", "/");
				return abs;
			}
		}
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Name of the world's GenericTerrainEntity (= its tile folder and tile file prefix), empty when none
	protected static string TerrainName(WorldEditorAPI api)
	{
		for (int e = 0, count = api.GetEditorEntityCount(); e < count; e++)
		{
			IEntitySource src = api.GetEditorEntity(e);
			if (src && src.GetClassName() == "GenericTerrainEntity")
				return src.GetName();
		}
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Loose tile folder of the world's terrain: <world dir>/<terrain entity name>/.Data with <name>_<n>.ttile (a
	//! world finds its terrain by location). Empty + why when the tiles are not loose files next to this world.
	protected static string TilesDir(WorldEditorAPI api, string worldAbs, out string why)
	{
		string terrainName = TerrainName(api);
		if (terrainName.IsEmpty())
		{
			why = "no GenericTerrainEntity with a name in this world";
			return string.Empty;
		}
		int slash = worldAbs.LastIndexOf("/");
		string dir = worldAbs.Substring(0, slash + 1) + terrainName + "/.Data";
		if (!FileIO.FileExists(dir + "/" + terrainName + "_0.ttile"))
		{
			why = string.Format("no %1/%2_0.ttile - the terrain comes from a parent world or the game's paks.", dir, terrainName);
			return string.Empty;
		}
		return dir;
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla mode inputs for a world on a pak terrain: terrain name, the game's .pak folder and the root of the mod
	//! that holds the world file. False + why when one is missing.
	protected static bool VanillaSetup(WorldEditorAPI api, string worldPath, string worldAbs, out string terrainName, out string gameData, out string addon, out string why)
	{
		terrainName = TerrainName(api);
		if (terrainName.IsEmpty())
		{
			why = "no GenericTerrainEntity with a name in this world";
			return false;
		}
		gameData = GAME_DATA;
		string plain = PlainPath(worldPath);
		if (worldAbs.IsEmpty() || !worldAbs.EndsWith(plain))
		{
			why = "cannot tell which mod folder holds " + worldPath;
			return false;
		}
		addon = worldAbs.Substring(0, worldAbs.Length() - plain.Length());
		while (addon.EndsWith("/"))
			addon = addon.Substring(0, addon.Length() - 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the text file holds a line starting with prefix (the scan's 'done' lines)
	protected static bool HasLine(string path, string prefix)
	{
		FileHandle f = FileIO.OpenFile(path, FileMode.READ);
		if (!f)
			return false;
		string line;
		bool hit;
		while (f.ReadLine(line) >= 0)
		{
			if (line.StartsWith(prefix))
			{
				hit = true;
				break;
			}
		}
		f.Close();
		return hit;
	}

	//------------------------------------------------------------------------------------------------
	//! Every RoadEntity (origin + trail flag) for MUD_KeepScan.LoadRoads: trails do not keep terrain. A trail =
	//! Material containing 'trail' (from the archived MUD_RoadListPlugin, 2026-10-17). Returns the road count, -1 on fault.
	protected static int WriteRoads(WorldEditorAPI api)
	{
		FileHandle f = FileIO.OpenFile(ROADS, FileMode.WRITE);
		if (!f)
			return -1;
		int roads;
		int trails;
		for (int i = 0, count = api.GetEditorEntityCount(); i < count; i++)
		{
			IEntitySource src = api.GetEditorEntity(i);
			if (!src || src.GetClassName() != "RoadEntity")
				continue;
			IEntity e = api.SourceToEntity(src);
			if (!e)
				continue;
			ResourceName mat;
			src.Get("Material", mat);
			float width;
			src.Get("Width", width);
			string low = mat;
			low.ToLower();
			int trail = 0;
			if (low.Contains("trail"))
			{
				trail = 1;
				trails++;
			}
			vector o = e.GetOrigin();
			f.WriteLine(string.Format("MUD road %1 %2 %3 %4 width %5 mat %6", trail, o[0], o[1], o[2], width, mat));
			roads++;
		}
		f.WriteLine(string.Format("MUD roads done roads %1 trails %2", roads, trails));
		f.Close();
		return roads;
	}

	//------------------------------------------------------------------------------------------------
	//! 'MUD bake sample <chunk x> <chunk z> <vertex> <mm>' lines (Tools/mud_holes.py fit) at skin chunk vertices
	//! (33 x 33 a chunk, x fastest, from the terrain's min corner). Returns the sample count, -1 on fault.
	protected static int WriteSamples(WorldEditor we, WorldEditorAPI api, string path)
	{
		vector tMin, tMax;
		if (!we.GetTerrainBounds(tMin, tMax))
			return -1;
		float cell = api.GetTerrainUnitScale();
		if (cell <= 0)
			return -1;
		int cellsX = Math.Round((tMax[0] - tMin[0]) / cell);
		int cellsZ = Math.Round((tMax[2] - tMin[2]) / cell);
		int chunksX = cellsX / 32;
		int chunksZ = cellsZ / 32;
		FileHandle f = FileIO.OpenFile(path, FileMode.WRITE);
		if (!f)
			return -1;
		array<int> verts = {0, 16 * 33 + 16, 5 * 33 + 7, 31 * 33 + 30, 10 * 33 + 25, 3 * 33 + 29};
		int n;
		WBProgressDialog bar = new WBProgressDialog("MUD Ground 3: reading terrain heights", we);
		for (int bz = 0; bz < chunksZ; bz += SAMPLE_STRIDE)
		{
			bar.SetProgress(bz / Math.Max(chunksZ, 1.0));
			for (int bx = 0; bx < chunksX; bx += SAMPLE_STRIDE)
			{
				foreach (int v : verts)
				{
					int vx = v % 33;
					int vz = v / 33;
					float x = tMin[0] + (bx * 32 + vx) * cell;
					float z = tMin[2] + (bz * 32 + vz) * cell;
					int mm = Math.Round(api.GetTerrainSurfaceY(x, z) * 1000);
					f.WriteLine(string.Format("MUD bake sample %1 %2 %3 %4", bx, bz, v, mm));
					n++;
				}
			}
		}
		f.Close();
		bar.SetProgress(1);
		return n;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool WriteRequest(string worldPath, string folder)
	{
		FileHandle f = FileIO.OpenFile(REQUEST, FileMode.WRITE);
		if (!f)
			return false;
		f.WriteLine("MUD build request world " + worldPath);
		f.WriteLine("MUD build request mask " + folder + "mask.txt");
		f.WriteLine("MUD build request pins " + folder + "pins.txt");
		f.WriteLine("MUD build request done");
		f.Close();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Report lines of mud_build.py; ok when the last line is 'MUD build exit 0'
	protected static string ReadReport(string path, out bool ok)
	{
		ok = false;
		FileHandle f = FileIO.OpenFile(path, FileMode.READ);
		if (!f)
			return "No report from Tools/mud_build.py (" + path + ") - did python start? Log: 'MUD build: running'.";
		string text;
		string line;
		string last;
		while (f.ReadLine(line) >= 0)
		{
			text += line + "\n";
			if (!line.Trim().IsEmpty())
				last = line.Trim();
		}
		f.Close();
		ok = last == "MUD build exit 0";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens BLANK_WORLD, runs cmd, waits, opens worldPath again. result = the tool's report (+ any fault).
	protected static int CountLines(string path)
	{
		FileHandle f = FileIO.OpenFile(path, FileMode.READ);
		if (!f)
			return 0;
		int n;
		string line;
		while (f.ReadLine(line) >= 0)
			n++;
		f.Close();
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! steps = the report lines the tool writes when it ends well (progress bar)
	protected static bool RunTool(WorldEditor we, string worldPath, string cmd, string reportPath, int steps, string title, string blank, out string result)
	{
		FileIO.DeleteFile(reportPath);
		if (!we.SetOpenedResource(blank))
		{
			result = "Could not open " + blank + " to release the tiles - nothing touched. Close the world and run by hand:\n" + cmd;
			return false;
		}
		Print("MUD build: running " + cmd, LogLevel.NORMAL);
		ProcessHandle handle = Workbench.RunProcess(cmd);
		if (!handle)
		{
			we.SetOpenedResource(worldPath);
			result = "Could not start '" + PYTHON + "' (Workbench.RunProcess) - nothing touched. Command:\n" + cmd;
			return false;
		}
		WBProgressDialog bar = new WBProgressDialog(title, we);
		bar.SetProgress(0.02);
		int t0 = System.GetTickCount();
		while (Workbench.IsRunningProcess(handle) && System.GetTickCount(t0) < PY_WAIT_MS)
		{
			Workbench.WaitProcess(handle, 500);
			bar.SetProgress(Math.Clamp(0.02 + 0.96 * CountLines(reportPath) / Math.Max(steps, 1), 0.02, 0.98));
		}
		bar.SetProgress(1);
		if (Workbench.IsRunningProcess(handle))
		{
			result = string.Format("Tools/mud_build.py still runs after %1 min. The world stays closed: when python ends, open %2 and run MUD Ground: Status.", PY_WAIT_MS / 60000, worldPath);
			return false;
		}
		Workbench.FinishProcess(handle);
		bool ok;
		result = ReadReport(reportPath, ok);
		if (!we.SetOpenedResource(worldPath))
			result += "\nCould not open " + worldPath + " again - open it by hand.";
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 1: our entities into LAYER. Returns the report; handPlaced = system entities that are not ours.
	protected static string BuildEntities(WorldEditor we, WorldEditorAPI api, out int handPlacedN)
	{
		vector tMin, tMax;
		we.GetTerrainBounds(tMin, tMax);
		int subScene = api.GetCurrentSubScene();
		array<IEntitySource> ours = {};
		array<int> found = {};
		array<string> handPlaced = {};
		Scan(api, subScene, ours, found, handPlaced);
		handPlacedN = handPlaced.Count();

		string before = api.GetActiveSubsceneLayer(subScene);
		string layerPath;
		foreach (IEntitySource own : ours)
		{
			string ownPath = api.GetSubsceneLayerPath(subScene, own.GetLayerID());
			if (ownPath.Contains(LAYER))
			{
				layerPath = ownPath;     // a partial build: fill the layer it is in, never a second one
				break;
			}
		}
		if (layerPath.IsEmpty())
			layerPath = api.CreateSubsceneLayer(subScene, LAYER);
		if (layerPath.IsEmpty())
			layerPath = LAYER;     // INFERENCE: empty = it exists already; SetActive then picks it by name
		api.SetActiveSubsceneLayer(subScene, layerPath);
		int layerId = api.GetCurrentEntityLayerId();
		string activePath = api.GetSubsceneLayerPath(subScene, layerId);
		if (!activePath.Contains(LAYER))
		{
			api.SetActiveSubsceneLayer(subScene, before);
			handPlacedN = -1;
			return string.Format("Could not make or select layer '%1' (got '%2'). Nothing added.", LAYER, activePath);
		}

		vector mid = (tMin + tMax) * 0.5;
		mid[1] = api.GetTerrainSurfaceY(mid[0], mid[2]);
		string report;
		int made;
		api.BeginEntityAction("MUD Ground: Build");
		for (int i = 0; i < SYSTEM_N; i++)
		{
			ResourceName prefab;
			string role;
			string cls;
			SystemEntry(i, prefab, role, cls);
			if (found[i] != 0)
			{
				report += string.Format("%1: already in the world, kept.\n", role);
				continue;
			}
			IEntitySource src = api.CreateEntity(prefab, NAME_PREFIX + role, layerId, null, mid, vector.Zero);
			if (!src)
			{
				report += string.Format("%1: CreateEntity failed (%2).\n", role, prefab);
				continue;
			}
			if (src.GetName() != NAME_PREFIX + role)
				api.RenameEntity(src, NAME_PREFIX + role);
			// the prefab default is Arland's file; until run 2 bakes this world's own, the skin traces every vertex
			if (i == 1)
				api.SetVariableValue(src, null, "m_sBaked", "");
			made++;
			report += string.Format("%1: added to layer %2.\n", role, activePath);
		}
		api.EndEntityAction();
		api.SetActiveSubsceneLayer(subScene, before);
		foreach (string h : handPlaced)
			report += "Hand-placed (not ours, Unbuild keeps it): " + h + "\n";
		report += string.Format("Entities: %1 added.\n", made);
		return report;
	}

	//------------------------------------------------------------------------------------------------
	// Steps (each its own menu entry, 'Next step' runs whichever is due)
	static const int STEP_CHECK = 1;
	static const int STEP_ENTITIES = 2;
	static const int STEP_SCAN = 3;
	static const int STEP_CUT = 4;
	static const int STEP_DONE = 5;

	//------------------------------------------------------------------------------------------------
	static string StepName(int step)
	{
		if (step == STEP_CHECK)
			return "1: Check world";
		if (step == STEP_ENTITIES)
			return "2: Add entities";
		if (step == STEP_SCAN)
			return "3: Scan map (Play)";
		if (step == STEP_CUT)
			return "4: Cut terrain";
		return "done";
	}

	//------------------------------------------------------------------------------------------------
	//! Can Workbench start python? (exit code of 'python --version'; INFERENCE: WaitProcess returns the exit code)
	protected static bool PythonOk(out string why)
	{
		ProcessHandle h = Workbench.RunProcess(PYTHON + " --version");
		if (!h)
		{
			why = "Workbench could not start '" + PYTHON + "'. Install Python 3 (python.org, tick 'Add to PATH') and restart Workbench.";
			return false;
		}
		int code = Workbench.WaitProcess(h, 10000);
		bool running = Workbench.IsRunningProcess(h);
		if (running)
			Workbench.KillProcess(h);
		Workbench.FinishProcess(h);
		if (running || code != 0)
		{
			why = string.Format("'%1 --version' did not finish cleanly (exit %2). Install Python 3 and restart Workbench.", PYTHON, code);
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Which step is due. why = one line on the state; folder = this world's build folder.
	static int Due(out string why)
	{
		WorldEditor we;
		WorldEditorAPI api = Api(we);
		if (!api)
		{
			why = "No world open in the World Editor.";
			return STEP_CHECK;
		}
		vector tMin, tMax;
		string worldPath;
		api.GetWorldPath(worldPath);
		if (!we.GetTerrainBounds(tMin, tMax) || !worldPath.EndsWith(".ent"))
		{
			why = "World not ready (no terrain, or not saved to a file).";
			return STEP_CHECK;
		}
		int subScene = api.GetCurrentSubScene();
		array<IEntitySource> ours = {};
		array<int> found = {};
		array<string> handPlaced = {};
		Scan(api, subScene, ours, found, handPlaced);
		for (int i = 0; i < SYSTEM_N; i++)
		{
			if (found[i] == 0)
			{
				why = "The ground entities are not in this world yet.";
				return STEP_ENTITIES;
			}
		}
		if (!handPlaced.IsEmpty())
		{
			why = "Set up by hand (hand-placed entities, terrain cut with Tools/mud_holes.py) - nothing more to do here.";
			return STEP_DONE;
		}
		string folder = DIR + WorldKey(worldPath) + "/";
		MUD_BuildManifest man = new MUD_BuildManifest();
		bool have = man.Load(folder + "manifest.txt");
		if (have && man.m_sState == "built")
		{
			why = "Built. After map edits: Unbuild, then run the steps again.";
			return STEP_DONE;
		}
		bool scanned = HasLine(folder + "mask.txt", "MUD mask done") && HasLine(folder + "pins.txt", "MUD pins done");
		if (have && (man.m_sState == "scan" || man.m_sState == "failed") && scanned)
		{
			why = "Map scanned - ready to cut the terrain.";
			if (man.m_sState == "failed")
				why = "The last cut failed (see Status) - fix and run step 4 again.";
			return STEP_CUT;
		}
		why = "The map needs its scan (Play runs it, a few minutes).";
		if (have && man.m_sState == "scan")
			why = "Scan started but not finished - run step 3 again and leave Play running until 'Scan done'.";
		return STEP_SCAN;
	}

	//------------------------------------------------------------------------------------------------
	//! Step 1: everything the later steps need, with how to fix each fault. ok = nothing blocks.
	static string Check(out bool ok)
	{
		ok = false;
		WorldEditor we;
		WorldEditorAPI api = Api(we);
		if (!api)
			return "FAIL  No World Editor / no world open. Open your world first.";
		string report;
		bool bad;
		vector tMin, tMax;
		if (we.GetTerrainBounds(tMin, tMax))
		{
			report += string.Format("ok    Terrain %1 x %2 m, cell %3 m.\n", tMax[0] - tMin[0], tMax[2] - tMin[2], api.GetTerrainUnitScale());
		}
		else
		{
			report += "FAIL  This world has no terrain - the ground system needs one.\n";
			bad = true;
		}
		string worldPath;
		api.GetWorldPath(worldPath);
		if (worldPath.EndsWith(".ent"))
		{
			report += "ok    World file " + worldPath + "\n";
		}
		else
		{
			report += "FAIL  Save the world to a file first (File > Save As).\n";
			bad = true;
		}
		string worldAbs = Abs(worldPath);
		string why;
		string vName, vData, vAddon;
		if (!worldAbs.IsEmpty() && !TilesDir(api, worldAbs, why).IsEmpty())
		{
			report += "ok    The terrain files belong to this world (steps 3-4 cut holes into them).\n";
		}
		else if (VanillaSetup(api, worldPath, worldAbs, vName, vData, vAddon, why))
		{
			report += "ok    Game terrain '" + vName + "' (from the game's paks): step 4 writes holed copies of its tiles into this mod at the same path,\n      which replace the game's tiles only while this mod is loaded. The terrain tools stay locked; Unbuild removes the copies.\n";
			report += "      Mod folder: " + vAddon + "\n      Game data: step 4 reads the game's .pak folder from the Workbench log.\n";
		}
		else
		{
			report += "FAIL  Terrain files: " + why + "\n";
			bad = true;
		}
		if (ToolPath().IsEmpty())
		{
			report += "FAIL  Tools/mud_build.py not found - copy the Tools folder into your project.\n";
			bad = true;
		}
		else
		{
			report += "ok    Terrain tool found.\n";
		}
		if (PythonOk(why))
		{
			report += "ok    Python starts.\n";
		}
		else
		{
			report += "FAIL  " + why + "\n";
			bad = true;
		}
		ok = !bad;
		string dueWhy;
		int due = Due(dueWhy);
		if (ok && due == STEP_CHECK)
			due = STEP_ENTITIES;
		report += "\nNext: MUD Ground " + StepName(due) + " - " + dueWhy;
		return report;
	}

	//------------------------------------------------------------------------------------------------
	//! Common start of steps 2-4: world open + saved with terrain. Empty = ok, else the reason.
	protected static string Ready(out WorldEditor we, out WorldEditorAPI api, out string worldPath, out string folder)
	{
		api = Api(we);
		if (!api)
			return "No World Editor / no world open.";
		vector tMin, tMax;
		if (!we.GetTerrainBounds(tMin, tMax))
			return "This world has no terrain - the ground system needs one.";
		api.GetWorldPath(worldPath);
		if (!worldPath.EndsWith(".ent"))
			return "Save the world to a file first (Build keys its files by the world file).";
		folder = DIR + WorldKey(worldPath) + "/";
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	static string StepEntities()
	{
		WorldEditor we;
		WorldEditorAPI api;
		string worldPath, folder;
		string fault = Ready(we, api, worldPath, folder);
		if (!fault.IsEmpty())
			return fault;
		int handPlaced;
		string report = BuildEntities(we, api, handPlaced);
		Print("MUD build: " + report, LogLevel.NORMAL);
		return report + WhatNext();
	}

	//------------------------------------------------------------------------------------------------
	static string StepScan()
	{
		s_bPlay = false;
		WorldEditor we;
		WorldEditorAPI api;
		string worldPath, folder;
		string fault = Ready(we, api, worldPath, folder);
		if (!fault.IsEmpty())
			return fault;
		string why;
		int due = Due(why);
		if (due == STEP_ENTITIES || due == STEP_DONE)
			return "Step 3 not due: " + why;
		MUD_BuildManifest man = new MUD_BuildManifest();
		man.Load(folder + "manifest.txt");
		string report = BuildScan(we, api, worldPath, folder, man);
		Print("MUD build: " + report, LogLevel.NORMAL);
		return report;
	}

	//------------------------------------------------------------------------------------------------
	static string StepCut()
	{
		WorldEditor we;
		WorldEditorAPI api;
		string worldPath, folder;
		string fault = Ready(we, api, worldPath, folder);
		if (!fault.IsEmpty())
			return fault;
		string why;
		if (Due(why) != STEP_CUT)
			return "Step 4 not due: " + why;
		MUD_BuildManifest man = new MUD_BuildManifest();
		man.Load(folder + "manifest.txt");
		string report = BuildTerrain(we, worldPath, folder, man);
		Print("MUD build: " + report, LogLevel.NORMAL);
		return report + WhatNext();
	}

	//------------------------------------------------------------------------------------------------
	static string WhatNext()
	{
		string why;
		int due = Due(why);
		if (due == STEP_DONE)
			return "\n\nAll steps done. " + why;
		return "\n\nNext: MUD Ground " + StepName(due) + " (or 'MUD Ground: Next step') - " + why;
	}

	//------------------------------------------------------------------------------------------------
	//! 'Next step': runs whichever step is due (step 1 runs first if it was never passed for this world)
	static string Next()
	{
		s_bPlay = false;
		string why;
		int due = Due(why);
		if (due == STEP_CHECK)
		{
			bool ok;
			return Check(ok);
		}
		if (due == STEP_ENTITIES)
		{
			bool ok;
			string check = Check(ok);
			if (!ok)
				return "Step 1 found faults - fix them first:\n\n" + check;
			return StepEntities();
		}
		if (due == STEP_SCAN)
			return StepScan();
		if (due == STEP_CUT)
			return StepCut();
		return "Nothing to do: " + why;
	}

	//------------------------------------------------------------------------------------------------
	//! 'Steps 1-3' (user 2026-10-28: one click): check, add entities when due, then the scan (Play starts after the
	//! dialog). Stops at the first step that fails.
	static string Prepare()
	{
		s_bPlay = false;
		bool ok;
		string check = Check(ok);
		if (!ok)
			return "Step 1 found faults - fix them first:\n\n" + check;
		string report = "Step 1: ok.\n\n";
		string why;
		int due = Due(why);
		if (due == STEP_ENTITIES)
		{
			report += StepEntities() + "\n\n";
			due = Due(why);
		}
		if (due != STEP_SCAN)
			return report + "Step 3 not run: " + why;
		return report + StepScan();
	}

	//------------------------------------------------------------------------------------------------
	//! Build run 1: road list, height samples, scan request, manifest 'scan'; asks Run() to start Play
	protected static string BuildScan(WorldEditor we, WorldEditorAPI api, string worldPath, string folder, MUD_BuildManifest man)
	{
		string worldAbs = Abs(worldPath);
		if (worldAbs.IsEmpty())
			return "Terrain: cannot resolve " + worldPath + " to a disk path.";
		string why;
		string tiles = TilesDir(api, worldAbs, why);
		string vName, vData, vAddon;
		if (tiles.IsEmpty() && !VanillaSetup(api, worldPath, worldAbs, vName, vData, vAddon, why))
			return "Terrain: not automated - " + why;
		string folderAbs = Abs(folder);
		if (folderAbs.IsEmpty())
			return "Terrain: cannot resolve " + folder + " to a disk path.";
		FileIO.MakeDirectory(folder);
		FileIO.DeleteFile(folder + "mask.txt");
		FileIO.DeleteFile(folder + "pins.txt");
		int roads = WriteRoads(api);
		int samples = WriteSamples(we, api, folder + "samples.txt");
		if (roads < 0 || samples <= 0 || !WriteRequest(worldPath, folder))
			return string.Format("Terrain: could not write the scan inputs (roads %1, samples %2, request %3).", roads, samples, REQUEST);

		string key = WorldKey(worldPath);
		string plain = PlainPath(worldPath);
		man.m_sWorld = worldPath;
		man.m_sState = "scan";
		man.m_sTiles = tiles;   // vanilla mode: empty until step 4 reports the override folder
		man.m_sVanilla = vName;
		man.m_sGameData = vData;
		man.m_sAddon = vAddon;
		man.m_sBackup = folderAbs + "tiles_backup";
		man.m_sHeights = plain.Substring(0, plain.LastIndexOf("/") + 1) + key + "_mud_heights.c";
		man.m_sHeightsAbs = worldAbs.Substring(0, worldAbs.LastIndexOf("/") + 1) + key + "_mud_heights.c";
		man.m_fCell = api.GetTerrainUnitScale();
		man.m_iTilesX = api.GetTerrainTilesX();
		if (!man.Save(folder + "manifest.txt"))
			return "Terrain: could not write " + folder + "manifest.txt.";
		s_bPlay = true;
		return string.Format("Step 3: %1 roads, %2 height samples, scan request written.\n\nPlay starts when you close this. The scan shows its progress on screen ('Scanning the map: N percent', minutes on a big map). When it says 'Scan done', stop Play and run MUD Ground 4: Cut terrain (or Next step).", roads, samples);
	}

	//------------------------------------------------------------------------------------------------
	//! Build run 2: save, close the world, mud_build.py build, reopen, skin -> heights file, save
	protected static string BuildTerrain(WorldEditor we, string worldPath, string folder, MUD_BuildManifest man)
	{
		bool isPak = !man.m_sVanilla.IsEmpty();
		string askText = "Step 4 writes terrain holes into " + man.m_sTiles + " (first copy backed up to " + man.m_sBackup + ").";
		if (isPak)
			askText = "Step 4 finds the game terrain '" + man.m_sVanilla + "' in the game's paks and writes holed copies of its tiles into " + man.m_sAddon + " at the same path (they replace the game's tiles only while this mod is loaded; the game files are not touched).";
		if (Workbench.ScriptDialog("MUD Ground 4: Cut terrain", askText + " The world is saved, closed while python runs, then opened again. Continue?", new WorkbenchDialog_OKCancel()) == 0)
			return "Terrain: cancelled, nothing touched.";
		if (!we.Save())
			return "Terrain: could not save the world - nothing touched.";
		string tool = ToolPath();
		string folderAbs = Abs(folder);
		if (tool.IsEmpty() || folderAbs.IsEmpty())
			return "Terrain: Tools/mud_build.py or " + folder + " not found - nothing touched.";
		string cmd = string.Format("%1 -I \"%2\" build --tiles \"%3\"", PYTHON, tool, man.m_sTiles);
		string blank = BLANK_WORLD;
		int steps = 5;
		if (isPak)
		{
			cmd = string.Format("%1 -I \"%2\" build --vanilla \"%3\" --game-data \"%4\" --addon \"%5\"", PYTHON, tool, man.m_sVanilla, man.m_sGameData, man.m_sAddon);
			blank = BLANK_WORLD_VANILLA;
			steps = 7;
		}
		cmd += string.Format(" --backup \"%1\" --mask \"%2mask.txt\" --pins \"%2pins.txt\" --samples \"%2samples.txt\" --heights \"%3\"",
			man.m_sBackup, folderAbs, man.m_sHeightsAbs);
		cmd += string.Format(" --tiles-x %1 --cell %2 --report \"%3report.txt\"", man.m_iTilesX, man.m_fCell, folderAbs);
		string result;
		bool ok = RunTool(we, worldPath, cmd, folder + "report.txt", steps, "MUD Ground 4: cutting terrain holes", blank, result);
		if (isPak)
		{
			string dir = OverrideDir(result);
			if (!dir.IsEmpty())
				man.m_sTiles = dir;
		}
		man.m_sState = "failed";
		if (ok)
		{
			man.m_sState = "built";
			FileIO.DeleteFile(REQUEST);
			result += PointSkin(we, man.m_sHeights);
			if (isPak)
				result += "\nThe holed tiles are new files: if the holes do not show in Play, restart Workbench once.";
		}
		man.Save(folder + "manifest.txt");
		return "Terrain run 2 of 2: " + man.m_sState + "\n" + result;
	}

	//------------------------------------------------------------------------------------------------
	//! The override tile folder from a vanilla build report ('MUD build: override tiles <dir> (...)'), else empty
	protected static string OverrideDir(string report)
	{
		int at = report.IndexOf(OVERRIDE_LINE);
		if (at < 0)
			return string.Empty;
		string rest = report.Substring(at + OVERRIDE_LINE.Length(), report.Length() - at - OVERRIDE_LINE.Length());
		int end = rest.IndexOf(" (");
		if (end < 0)
			return string.Empty;
		return rest.Substring(0, end);
	}

	//------------------------------------------------------------------------------------------------
	//! Our skin reads this world's heights file; saves the world
	protected static string PointSkin(WorldEditor we, string heights)
	{
		WorldEditorAPI api = we.GetApi();
		if (!api)
			return "\nSkin: no World Editor API after reopening - set MUD_SYS_CoarseSkin 'm_sBaked' to " + heights + " by hand.";
		IEntitySource skin = api.FindEntityByName(NAME_PREFIX + "CoarseSkin");
		if (!skin)
			return "\nSkin: MUD_SYS_CoarseSkin not found after reopening - set its 'm_sBaked' to " + heights + " by hand.";
		api.BeginEntityAction("MUD Ground: heights file");
		bool changed = api.SetVariableValue(skin, null, "m_sBaked", heights);
		api.EndEntityAction();
		bool saved = we.Save();
		return string.Format("\nSkin: heights file %1 (set %2, world saved %3).", heights, changed, saved);
	}

	//------------------------------------------------------------------------------------------------
	static string Unbuild()
	{
		WorldEditor we;
		WorldEditorAPI api = Api(we);
		if (!api)
			return "No World Editor / no world open.";
		int subScene = api.GetCurrentSubScene();
		array<IEntitySource> ours = {};
		array<int> found = {};
		array<string> handPlaced = {};
		Scan(api, subScene, ours, found, handPlaced);
		int removed;
		string report;
		api.BeginEntityAction("MUD Ground: Unbuild");
		foreach (IEntitySource src : ours)
		{
			string name = src.GetName();
			if (api.DeleteEntity(src))
			{
				removed++;
				report += "Removed " + name + "\n";
			}
			else
			{
				report += "Could NOT remove " + name + "\n";
			}
		}
		api.EndEntityAction();
		foreach (string h : handPlaced)
			report += "Kept (hand-placed, not ours): " + h + "\n";
		report += string.Format("Entities: %1 removed (layer '%2' left empty).\n", removed, LAYER);

		string worldPath;
		api.GetWorldPath(worldPath);
		string folder = DIR + WorldKey(worldPath) + "/";
		MUD_BuildManifest man = new MUD_BuildManifest();
		FileIO.DeleteFile(REQUEST);
		if (!man.Load(folder + "manifest.txt") || (man.m_sState != "built" && man.m_sState != "failed"))
		{
			report += "Terrain: nothing to put back (no built manifest in " + folder + "). Save the world to keep the removal.";
			Print("MUD unbuild: " + report, LogLevel.NORMAL);
			return report;
		}
		report += UnbuildTerrain(we, worldPath, folder, man);
		Print("MUD unbuild: " + report, LogLevel.NORMAL);
		return report;
	}

	//------------------------------------------------------------------------------------------------
	protected static string UnbuildTerrain(WorldEditor we, string worldPath, string folder, MUD_BuildManifest man)
	{
		bool isPak = !man.m_sVanilla.IsEmpty();
		string askText = "Put the original terrain holes back into " + man.m_sTiles + "?";
		if (isPak)
			askText = "Remove the holed tile copies this plugin wrote into the mod (" + man.m_sTiles + "), so the game's own '" + man.m_sVanilla + "' tiles are used again?";
		if (Workbench.ScriptDialog("MUD Ground: Unbuild", askText + " The world is saved, closed while python runs, then opened again.", new WorkbenchDialog_OKCancel()) == 0)
			return "Terrain: not restored (cancelled). Save the world to keep the entity removal.";
		if (!we.Save())
			return "Terrain: could not save the world - terrain not restored.";
		string tool = ToolPath();
		string folderAbs = Abs(folder);
		if (tool.IsEmpty() || folderAbs.IsEmpty())
			return "Terrain: Tools/mud_build.py or " + folder + " not found - terrain not restored.";
		string cmd = string.Format("%1 -I \"%2\" unbuild", PYTHON, tool);
		if (!man.m_sTiles.IsEmpty())   // vanilla mode without it: mud_build.py reads its override.txt in the backup
			cmd += " --tiles \"" + man.m_sTiles + "\"";
		cmd += string.Format(" --backup \"%1\" --heights \"%2\" --report \"%3report.txt\"", man.m_sBackup, man.m_sHeightsAbs, folderAbs);
		string blank = BLANK_WORLD;
		int steps = 2;
		if (isPak)
		{
			blank = BLANK_WORLD_VANILLA;
			steps = 3;
		}
		string result;
		if (RunTool(we, worldPath, cmd, folder + "report.txt", steps, "MUD Ground: putting the terrain back", blank, result))
		{
			man.m_sState = "unbuilt";
			man.Save(folder + "manifest.txt");
		}
		return "Terrain: " + man.m_sState + "\n" + result;
	}

	//------------------------------------------------------------------------------------------------
	static string Status()
	{
		WorldEditor we;
		WorldEditorAPI api = Api(we);
		if (!api)
			return "No World Editor / no world open.";
		int subScene = api.GetCurrentSubScene();
		array<IEntitySource> ours = {};
		array<int> found = {};
		array<string> handPlaced = {};
		Scan(api, subScene, ours, found, handPlaced);
		string report;
		for (int i = 0; i < SYSTEM_N; i++)
		{
			ResourceName prefab;
			string role;
			string cls;
			SystemEntry(i, prefab, role, cls);
			string state = "missing";
			if (found[i] != 0)
				state = "present";
			report += string.Format("%1 (%2): %3\n", role, cls, state);
		}
		foreach (IEntitySource src : ours)
			report += "Ours: " + src.GetName() + " in " + api.GetSubsceneLayerPath(subScene, src.GetLayerID()) + "\n";
		foreach (string h : handPlaced)
			report += "Hand-placed: " + h + "\n";

		string worldPath;
		api.GetWorldPath(worldPath);
		string folder = DIR + WorldKey(worldPath) + "/";
		MUD_BuildManifest man = new MUD_BuildManifest();
		if (!man.Load(folder + "manifest.txt"))
			return report + "Terrain: no manifest (" + folder + "manifest.txt) - not built by the plugin." + WhatNext();
		report += string.Format("Terrain: %1\nTiles: %2\nBackup: %3\nHeights: %4\n", man.m_sState, man.m_sTiles, man.m_sBackup, man.m_sHeights);
		if (!man.m_sVanilla.IsEmpty())
			report += "Mode: game terrain '" + man.m_sVanilla + "' - holed tile copies in the mod replace the game's tiles while it is loaded\n";
		report += string.Format("Scan: mask done %1, pins done %2, request open %3",
			HasLine(folder + "mask.txt", "MUD mask done"), HasLine(folder + "pins.txt", "MUD pins done"), FileIO.FileExists(REQUEST));
		string last = folder + "report.txt";
		if (FileIO.FileExists(last))
		{
			bool ok;
			report += "\n\nLast terrain tool report:\n" + ReadReport(last, ok);
		}
		return report + WhatNext();
	}

	//------------------------------------------------------------------------------------------------
	//! Menu entry body: show the text; step 3 then starts Play
	static void Show(string title, string text)
	{
		Workbench.Dialog(title, text);
		if (!s_bPlay)
			return;
		s_bPlay = false;
		WorldEditor we = Workbench.GetModule(WorldEditor);
		if (we)
			we.SwitchToGameMode();
	}
}

//------------------------------------------------------------------------------------------------
// Menu: Plugins > 'MUD Ground: Next step' does whichever step is due; the numbered entries run one step each.
[WorkbenchPluginAttribute(name: "MUD Ground: Next step", description: "Run the next setup step for this world (check, entities, scan, cut terrain)", wbModules: { "WorldEditor" }, awesomeFontCode: 0xF04E)]
class MUD_GroundBuildPlugin : WorkbenchPlugin
{
	override void Run()
	{
		MUD_GroundBuilder.Show("MUD Ground: Next step", MUD_GroundBuilder.Next());
	}
}

//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "MUD Ground: Steps 1-3", description: "Check the world, add the entities, read roads + heights, then Play scans (stops at the first fault)", wbModules: { "WorldEditor" }, awesomeFontCode: 0xF04B)]
class MUD_GroundPreparePlugin : WorkbenchPlugin
{
	override void Run()
	{
		MUD_GroundBuilder.Show("MUD Ground: Steps 1-3", MUD_GroundBuilder.Prepare());
	}
}

//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "MUD Ground 1: Check world", description: "Check everything the setup needs, with how to fix each problem", wbModules: { "WorldEditor" }, awesomeFontCode: 0xF00C)]
class MUD_GroundCheckPlugin : WorkbenchPlugin
{
	override void Run()
	{
		bool ok;
		MUD_GroundBuilder.Show("MUD Ground 1: Check world", MUD_GroundBuilder.Check(ok));
	}
}

//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "MUD Ground 2: Add entities", description: "Add the ground system entities (layer MUD_Ground, one Undo step)", wbModules: { "WorldEditor" }, awesomeFontCode: 0xF1B2)]
class MUD_GroundEntitiesPlugin : WorkbenchPlugin
{
	override void Run()
	{
		MUD_GroundBuilder.Show("MUD Ground 2: Add entities", MUD_GroundBuilder.StepEntities());
	}
}

//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "MUD Ground 3: Scan map", description: "Read roads + heights, then Play scans what must stay terrain", wbModules: { "WorldEditor" }, awesomeFontCode: 0xF002)]
class MUD_GroundScanPlugin : WorkbenchPlugin
{
	override void Run()
	{
		MUD_GroundBuilder.Show("MUD Ground 3: Scan map", MUD_GroundBuilder.StepScan());
	}
}

//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "MUD Ground 4: Cut terrain", description: "Cut the terrain holes and bake heights (original tiles backed up)", wbModules: { "WorldEditor" }, awesomeFontCode: 0xF0C4)]
class MUD_GroundCutPlugin : WorkbenchPlugin
{
	override void Run()
	{
		MUD_GroundBuilder.Show("MUD Ground 4: Cut terrain", MUD_GroundBuilder.StepCut());
	}
}

//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "MUD Ground: Unbuild", description: "Remove only what MUD Ground: Build added (entities, terrain holes)", wbModules: { "WorldEditor" }, awesomeFontCode: 0xF2ED)]
class MUD_GroundUnbuildPlugin : WorkbenchPlugin
{
	override void Run()
	{
		Workbench.Dialog("MUD Ground: Unbuild", MUD_GroundBuilder.Unbuild());
	}
}

//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "MUD Ground: Status", description: "Show what of the mud ground system is in this world", wbModules: { "WorldEditor" }, awesomeFontCode: 0xF05A)]
class MUD_GroundStatusPlugin : WorkbenchPlugin
{
	override void Run()
	{
		Workbench.Dialog("MUD Ground: Status", MUD_GroundBuilder.Status());
	}
}
#endif
