//! Build 0 test kit switches (docs/superpowers/prompts/2026-10-16-build-0-test-kit.md, 2026-10-17).
//! Every switch at its release value = the game behaves as without the kit.
//! FLIP TEST_KIT TO false BEFORE RELEASE (master list Lane 2).
//! Settings file (user 2026-10-17: tests run on a dedicated server, no rebuild per setting): while TEST_KIT, each
//! machine reads SETTINGS_FILE once at start, and writes it with the defaults below when it is missing. The server's
//! file drives the fake rut ops; a joining client's own file drives its JIP test cap.
class MUD_TestKit
{
	// true: Stats' 10 s lines print without m_bDebug ('MUD patches ... | queue, oldest dirty, ops'), field / disp
	// hash per op, settings file, robot code (later stages)
	static const bool TEST_KIT = true;
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

	protected static bool s_bLoaded;
	protected static int s_iFakeRutOps;
	protected static float s_fFakeRutX;
	protected static float s_fFakeRutZ;
	protected static int s_iJipTestCap;

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
						s_iFakeRutOps = val.ToInt();
					else if (key == "fake_rut_x")
						s_fFakeRutX = val.ToFloat();
					else if (key == "fake_rut_z")
						s_fFakeRutZ = val.ToFloat();
					else if (key == "jip_test_cap")
						s_iJipTestCap = val.ToInt();
					else
						Print("MUD test kit: unknown setting '" + key + "' in " + SETTINGS_FILE, LogLevel.WARNING);
				}
				f.Close();
			}
		}
		Print(string.Format("MUD test kit: settings from %1 - fake rut ops %2 at %3 %4 (no player), jip test cap %5",
			from, s_iFakeRutOps, s_fFakeRutX, s_fFakeRutZ, s_iJipTestCap), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteDefaults()
	{
		FileHandle f = FileIO.OpenFile(SETTINGS_FILE, FileMode.WRITE);
		if (!f)
			return;
		f.WriteLine("# MUD test kit settings. Read once at start: edit, then restart the server / game.");
		f.WriteLine("# Only used while the mod is built with MUD_TestKit.TEST_KIT true.");
		f.WriteLine("# fake_rut_ops: SERVER - this many fake rut ops, 30 s after start, 10 m off the first player");
		f.WriteLine("# (no player: at fake_rut_x / fake_rut_z). 0 = off.");
		f.WriteLine(string.Format("fake_rut_ops = %1", FAKE_RUT_OPS));
		f.WriteLine(string.Format("fake_rut_x = %1", FAKE_RUT_X));
		f.WriteLine(string.Format("fake_rut_z = %1", FAKE_RUT_Z));
		f.WriteLine("# jip_test_cap: CLIENT - hold at most this many out-of-order ops instead of 20000 (J1 test).");
		f.WriteLine("# Set it in the JOINING client's own file. 0 = off.");
		f.WriteLine(string.Format("jip_test_cap = %1", JIP_TEST_CAP));
		f.Close();
	}
}
