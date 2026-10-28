//! Build 0 test kit switches (docs/superpowers/prompts/old/2026-10-16-build-0-test-kit.md, 2026-10-17).
//! Every switch at its release value = the game behaves as without the kit.
//! FLIP TEST_KIT TO false BEFORE RELEASE (master list Lane 2).
//! Settings file (user 2026-10-17: tests run on a dedicated server, no rebuild per setting): while TEST_KIT, each
//! machine reads SETTINGS_FILE once at start, and writes it with the defaults below when it is missing. The server's
//! file drives the fake rut ops; a joining client's own file drives its JIP test cap.
class MUD_TestKit
{
	// true: Stats' 10 s lines print without m_bDebug ('MUD patches ... | queue, oldest dirty, ops'), field / disp
	// hash per op, settings file, robot code (later stages)
	static const bool TEST_KIT = false;
	static const string SETTINGS_FILE = "$profile:MUD_TestKit.txt";
	// defaults (the file overrides them): > 0 = the server issues this many fake kind-4 rut ops once, 30 s after its
	// ground is ready (Stage 3)
	static const int FAKE_RUT_OPS = 0;
	static const float FAKE_RUT_WAIT = 30;      // s after the voxel probe is up
	static const int FAKE_RUT_PER_FRAME = 20;
	static const float FAKE_RUT_X = 1100;       // no player then: here (file: fake_rut_x / z)
	static const float FAKE_RUT_Z = 2650;
	static const float FAKE_RUT_STEP = 0.08;    // cut per pass (= the prefab's Rut Step)
	static const float FAKE_RUT_CAP = 0.3;      // deepest (a ~9 t truck: 0.08 + 9 x 0.025, max 0.35)
	// default: > 0 = a client holds at most this many out-of-order ops instead of 20000 (Stage 3, J1 test)
	static const int JIP_TEST_CAP = 0;
	// default: 1 = client look ruts on (MUD_LookRuts), 0 = off - A/B on one build (CLIENT file, restart the game)
	static const int LOOK_RUTS = 1;
	// default: > 0 = CLIENT render A/B - stored mud sheet meshes hidden / shown every this many s (MUD_MudSheet.RenderAB)
	static const float SHEET_HIDE_AB = 0;
	// bone tile cost probe (MUD_BoneCost, 2026-10-23): CLIENT - tiles (0 = off), tile 9 / 15, s per phase, virtual wheels
	static const int BONE_COST = 0;
	static const int BONE_COST_TILE = 9;
	static const float BONE_COST_PHASE = 20;
	static const int BONE_COST_WHEELS = 12;
	// bone tiles at load (MUD_BoneField, 2026-10-23): CLIENT - 1 = thick mud FIELD around you with the edge fade,
	// 2 = MAP test (whole map's offroad from the map centre outward, slow), 0 = off
	static const int BONE_MAP = 0;     // user 2026-10-23: 'bad' - numpad only (MUD_BoneProbe numpad 1)
	// sheet cost A3 (2026-10-19): DS only - 1 = the DS keeps the sync layer (MUD_MudSync.SERVER_LAYER), 0 = it does not
	// (no layer cost on the DS; joiners get no sheet history). Clients ignore it.
	// sheet cost A4 (research 2026-10-19 sec 9): clients - 'MUD bench natives' line once, BENCH_AT s after start
	static const float BENCH_AT = 30;
	// multi-vehicle load test (MUD_LoadDrivers, 2026-10-23): SERVER - trucks (0 = off), s per arm, cycles
	static const int LOAD_DRIVERS = 0;
	static const float LOAD_PHASE = 60;
	static const int LOAD_CYCLES = 6;

	protected static bool s_bLoaded;
	protected static int s_iFakeRutOps;
	protected static float s_fFakeRutX;
	protected static float s_fFakeRutZ;
	protected static int s_iJipTestCap;
	protected static int s_iLookRuts;
	protected static float s_fSheetHideAB;
	protected static int s_iServerLayer;
	protected static int s_iBoneCost;
	protected static int s_iBoneCostTile;
	protected static float s_fBoneCostPhase;
	protected static int s_iBoneCostWheels;
	protected static int s_iBoneMap;
	protected static int s_iLoadDrivers;
	protected static float s_fLoadPhase;
	protected static int s_iLoadCycles;
	protected static float s_fBenchT;
	protected static bool s_bBenchDone;

	//------------------------------------------------------------------------------------------------
	static int FakeRutOps()
	{
		Load();
		return s_iFakeRutOps;
	}

	//------------------------------------------------------------------------------------------------
	static float FakeRutX()
	{
		Load();
		return s_fFakeRutX;
	}

	//------------------------------------------------------------------------------------------------
	static float FakeRutZ()
	{
		Load();
		return s_fFakeRutZ;
	}

	//------------------------------------------------------------------------------------------------
	static int JipTestCap()
	{
		Load();
		return s_iJipTestCap;
	}

	//------------------------------------------------------------------------------------------------
	static bool LookRuts()
	{
		Load();
		return s_iLookRuts != 0;
	}

	//------------------------------------------------------------------------------------------------
	static float SheetHideAB()
	{
		Load();
		return s_fSheetHideAB;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_BoneCost: tiles (0 = off), tile bones per side, s per phase, virtual wheels
	static int BoneCost()
	{
		Load();
		return s_iBoneCost;
	}

	static int BoneCostTile()
	{
		Load();
		return s_iBoneCostTile;
	}

	static float BoneCostPhase()
	{
		Load();
		return s_fBoneCostPhase;
	}

	static int BoneCostWheels()
	{
		Load();
		return s_iBoneCostWheels;
	}

	//! MUD_BoneField at load: 1 field with edge fade, 2 MAP test, 0 off
	static int BoneMap()
	{
		Load();
		return s_iBoneMap;
	}

	//! MUD_LoadDrivers: trucks (0 = off), s per arm (min 20), cycles (min 1)
	static int LoadDrivers()
	{
		Load();
		return s_iLoadDrivers;
	}

	static float LoadPhase()
	{
		Load();
		return Math.Max(20, s_fLoadPhase);
	}

	static int LoadCycles()
	{
		Load();
		return Math.Max(1, s_iLoadCycles);
	}

	//------------------------------------------------------------------------------------------------
	//! A3: does a DS keep the sync layer? (SERVER_LAYER, or the DS file's server_layer while TEST_KIT)
	static bool ServerLayer()
	{
		Load();
		return s_iServerLayer != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! A4, every frame (MUD_WorldGroundEntity.EOnFrame, TEST_KIT): BenchNatives once, BENCH_AT s in, not on a DS
	static void BenchTick(float dt)
	{
		if (!TEST_KIT || s_bBenchDone)
			return;
		s_fBenchT += dt;
		if (s_fBenchT < BENCH_AT)
			return;
		s_bBenchDone = true;
		if (RplSession.Mode() == RplMode.Dedicated)
			return;
		BenchNatives();
	}

	//------------------------------------------------------------------------------------------------
	//! MUD bench natives (research 2026-10-19 sec 9): ms for n calls each way; s keeps the loops from being dead code.
	//! (tMax - tIf) / n = one Math.* native over an inline compare; (tVec - tTpl) / n = one Vector() over copy + set.
	protected static void BenchNatives()
	{
		int n = 200000;
		float a = 0.3;
		float b = 0.7;
		float s = 0;
		int t0 = System.GetTickCount();
		for (int i = 0; i < n; i++)
			s += Math.Max(a, b + i * 0.000001);
		int tMax = System.GetTickCount(t0);
		t0 = System.GetTickCount();
		for (int j = 0; j < n; j++)
		{
			float m = b + j * 0.000001;
			if (a > m)
				m = a;
			s += m;
		}
		int tIf = System.GetTickCount(t0);
		vector tv[64];
		t0 = System.GetTickCount();
		for (int k = 0; k < n; k++)
		{
			int slot = k % 64;
			tv[slot] = Vector(k * 0.15, s, 0.3);
		}
		int tVec = System.GetTickCount(t0);
		t0 = System.GetTickCount();
		for (int q = 0; q < n; q++)
		{
			int slot2 = q % 64;
			vector p = tv[63 - slot2];
			p[0] = q * 0.15;
			p[1] = s;
			tv[slot2] = p;
		}
		int tTpl = System.GetTickCount(t0);
		Print(string.Format("MUD bench natives: n %1 | Math.Max %2 ms, inline if %3 ms | Vector() %4 ms, copy+set %5 ms | s %6", n, tMax, tIf, tVec, tTpl, s), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Once: the defaults, then (TEST_KIT) SETTINGS_FILE 'key = value' lines ('#' = comment). Logs what this run uses.
	protected static void Load()
	{
		if (s_bLoaded)
			return;
		s_bLoaded = true;
		s_iFakeRutOps = FAKE_RUT_OPS;
		s_fFakeRutX = FAKE_RUT_X;
		s_fFakeRutZ = FAKE_RUT_Z;
		s_iJipTestCap = JIP_TEST_CAP;
		s_iLookRuts = LOOK_RUTS;
		s_fSheetHideAB = SHEET_HIDE_AB;
		s_iServerLayer = 0;
		s_iBoneCost = BONE_COST;
		s_iBoneCostTile = BONE_COST_TILE;
		s_fBoneCostPhase = BONE_COST_PHASE;
		s_iBoneCostWheels = BONE_COST_WHEELS;
		s_iBoneMap = BONE_MAP;
		s_iLoadDrivers = LOAD_DRIVERS;
		s_fLoadPhase = LOAD_PHASE;
		s_iLoadCycles = LOAD_CYCLES;
		if (MUD_MudSync.SERVER_LAYER)
			s_iServerLayer = 1;
		if (!TEST_KIT)
			return;
		string from = SETTINGS_FILE;
		if (!FileIO.FileExists(SETTINGS_FILE))
		{
			WriteDefaults();
			from = "defaults (wrote " + SETTINGS_FILE + ")";
		}
		else
		{
			FileHandle f = FileIO.OpenFile(SETTINGS_FILE, FileMode.READ);
			if (!f)
			{
				from = "defaults (" + SETTINGS_FILE + " did not open)";
			}
			else
			{
				string line;
				int n;
				int seen;     // one bit per key below (WriteKeys order); missing keys get appended (audit E9)
				while (n < 200 && f.ReadLine(line) >= 0)
				{
					n++;
					string row = line.Trim();
					if (row.IsEmpty() || row.Get(0) == "#")
						continue;
					int eq = row.IndexOf("=");
					if (eq < 1)
						continue;
					string key = row.Substring(0, eq).Trim();
					string val = row.Substring(eq + 1, row.Length() - eq - 1).Trim();
					if (key == "fake_rut_ops")
					{
						seen |= 1;
						s_iFakeRutOps = val.ToInt();
					}
					else if (key == "fake_rut_x")
					{
						seen |= 2;
						s_fFakeRutX = val.ToFloat();
					}
					else if (key == "fake_rut_z")
					{
						seen |= 4;
						s_fFakeRutZ = val.ToFloat();
					}
					else if (key == "jip_test_cap")
					{
						seen |= 8;
						s_iJipTestCap = val.ToInt();
					}
					else if (key == "look_ruts")
					{
						seen |= 16;
						s_iLookRuts = val.ToInt();
					}
					else if (key == "sheet_hide_ab")
					{
						seen |= 32;
						s_fSheetHideAB = val.ToFloat();
					}
					else if (key == "server_layer")
					{
						seen |= 64;
						s_iServerLayer = val.ToInt();
					}
					else if (key == "bone_cost")
					{
						seen |= 128;
						s_iBoneCost = val.ToInt();
					}
					else if (key == "bone_cost_tile")
					{
						seen |= 256;
						s_iBoneCostTile = val.ToInt();
					}
					else if (key == "bone_cost_phase")
					{
						seen |= 512;
						s_fBoneCostPhase = val.ToFloat();
					}
					else if (key == "bone_cost_wheels")
					{
						seen |= 1024;
						s_iBoneCostWheels = val.ToInt();
					}
					else if (key == "bone_map")
					{
						seen |= 2048;
						s_iBoneMap = val.ToInt();
					}
					else if (key == "load_drivers")
					{
						seen |= 4096;
						s_iLoadDrivers = val.ToInt();
					}
					else if (key == "load_phase")
					{
						seen |= 8192;
						s_fLoadPhase = val.ToFloat();
					}
					else if (key == "load_cycles")
					{
						seen |= 16384;
						s_iLoadCycles = val.ToInt();
					}
					else
						Print("MUD test kit: unknown setting '" + key + "' in " + SETTINGS_FILE, LogLevel.WARNING);
				}
				f.Close();
				if (seen != 32767)
				{
					FileHandle a = FileIO.OpenFile(SETTINGS_FILE, FileMode.APPEND);
					if (a)
					{
						a.WriteLine("");     // in case the last line has no line end
						WriteKeys(a, seen);
						a.Close();
						from = from + " (added missing keys)";
					}
				}
			}
		}
		Print(string.Format("MUD test kit: settings from %1 - fake rut ops %2 at %3 %4 (no player), jip test cap %5, look ruts %6, sheet hide A/B %7 s, server layer (DS) %8",
			from, s_iFakeRutOps, s_fFakeRutX, s_fFakeRutZ, s_iJipTestCap, s_iLookRuts, s_fSheetHideAB, s_iServerLayer), LogLevel.NORMAL);
		Print(string.Format("MUD test kit: bone cost %1 tiles (0 = off), tile %2, phase %3 s, wheels %4 | bone map at load %5",
			s_iBoneCost, s_iBoneCostTile, s_fBoneCostPhase, s_iBoneCostWheels, s_iBoneMap), LogLevel.NORMAL);
		Print(string.Format("MUD test kit: load drivers %1 trucks (0 = off), %2 s per arm, %3 cycles", s_iLoadDrivers, s_fLoadPhase, s_iLoadCycles), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteDefaults()
	{
		FileHandle f = FileIO.OpenFile(SETTINGS_FILE, FileMode.WRITE);
		if (!f)
			return;
		f.WriteLine("# MUD test kit settings. Read once at start: edit, then restart the server / game.");
		f.WriteLine("# Only used while the mod is built with MUD_TestKit.TEST_KIT true.");
		WriteKeys(f, 0);
		f.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Each key (with its comment) whose bit is NOT in skip: 1 fake_rut_ops, 2 fake_rut_x, 4 fake_rut_z, 8 jip_test_cap,
	//! 16 look_ruts, 32 sheet_hide_ab, 64 server_layer, 128 bone_cost, 256 bone_cost_tile, 512 bone_cost_phase,
	//! 1024 bone_cost_wheels, 2048 bone_map, 4096 load_drivers, 8192 load_phase, 16384 load_cycles (the bits Load sets in 'seen')
	protected static void WriteKeys(FileHandle f, int skip)
	{
		if (!(skip & 1))
		{
			f.WriteLine("# fake_rut_ops: SERVER - this many fake rut ops, 30 s after start, 10 m off the first player");
			f.WriteLine("# (no player: at fake_rut_x / fake_rut_z). 0 = off.");
			f.WriteLine(string.Format("fake_rut_ops = %1", FAKE_RUT_OPS));
		}
		if (!(skip & 2))
			f.WriteLine(string.Format("fake_rut_x = %1", FAKE_RUT_X));
		if (!(skip & 4))
			f.WriteLine(string.Format("fake_rut_z = %1", FAKE_RUT_Z));
		if (!(skip & 8))
		{
			f.WriteLine("# jip_test_cap: CLIENT - hold at most this many out-of-order ops instead of 20000 (J1 test).");
			f.WriteLine("# Set it in the JOINING client's own file. 0 = off.");
			f.WriteLine(string.Format("jip_test_cap = %1", JIP_TEST_CAP));
		}
		if (!(skip & 16))
		{
			f.WriteLine("# look_ruts: CLIENT - 1 = draw ruts right behind the wheels at once (mesh only), 0 = off (A/B).");
			f.WriteLine(string.Format("look_ruts = %1", LOOK_RUTS));
		}
		if (!(skip & 32))
		{
			f.WriteLine("# sheet_hide_ab: CLIENT - > 0 = hide / show the stored mud sheet meshes every this many s (render A/B).");
			f.WriteLine(string.Format("sheet_hide_ab = %1", SHEET_HIDE_AB));
		}
		if (!(skip & 64))
		{
			f.WriteLine("# server_layer: DEDICATED SERVER - 1 = keep the mud sheet sync layer (joiners get sheet history), 0 = not.");
			f.WriteLine("# Clients ignore it.");
			f.WriteLine(string.Format("server_layer = %1", s_iServerLayer));
		}
		if (!(skip & 128))
		{
			f.WriteLine("# bone_cost: CLIENT / standalone - lay this many bone tiles in front of the camera 30 s after start and");
			f.WriteLine("# cycle OFF / IDLE / PRESS / SWEEP phases for ever ('MUD bone cost' lines). 0 = off. Stand still.");
			f.WriteLine(string.Format("bone_cost = %1", BONE_COST));
		}
		if (!(skip & 256))
		{
			f.WriteLine("# bone_cost_tile: 9 (9 x 9 bones) or 15 (15 x 15).");
			f.WriteLine(string.Format("bone_cost_tile = %1", BONE_COST_TILE));
		}
		if (!(skip & 512))
		{
			f.WriteLine("# bone_cost_phase: s per phase (min 5).");
			f.WriteLine(string.Format("bone_cost_phase = %1", BONE_COST_PHASE));
		}
		if (!(skip & 1024))
		{
			f.WriteLine("# bone_cost_wheels: virtual wheels pressing in the PRESS phase (12 = 3 trucks).");
			f.WriteLine(string.Format("bone_cost_wheels = %1", BONE_COST_WHEELS));
		}
		if (!(skip & 2048))
		{
			f.WriteLine("# bone_map: CLIENT / standalone - thick mud bone tiles from load on: 1 = field around you (fades at");
			f.WriteLine("# its edge), 2 = whole-map MAP test (map centre outward, slow), 0 = off.");
			f.WriteLine(string.Format("bone_map = %1", BONE_MAP));
		}
		if (!(skip & 4096))
		{
			f.WriteLine("# load_drivers: SERVER - multi-vehicle load test: spawn this many trucks around fake_rut_x / fake_rut_z,");
			f.WriteLine("# 30 s after start, and step 0 / 1 / 2 / 4 / ... driving ('MUD load' lines, Tools/loadtest.py). 0 = off.");
			f.WriteLine(string.Format("load_drivers = %1", LOAD_DRIVERS));
		}
		if (!(skip & 8192))
		{
			f.WriteLine("# load_phase: s per arm (min 20; the first 15 s of each are not counted).");
			f.WriteLine(string.Format("load_phase = %1", LOAD_PHASE));
		}
		if (!(skip & 16384))
		{
			f.WriteLine("# load_cycles: how many times the whole arm list runs (up, down, up, ...).");
			f.WriteLine(string.Format("load_cycles = %1", LOAD_CYCLES));
		}
	}
}
