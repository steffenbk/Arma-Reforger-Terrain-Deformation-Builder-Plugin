//! RELEASE GATE (research 2026-10-21 Q7): every debug / test switch and its release value in one list.
//! PUBLISH false (testing): one 'MUD debug on: ...' line at start, so every test log records what was on.
//! PUBLISH true (Workshop build): any switch not at its release value = 'MUD RELEASE GATE FAILED' ERROR at start and
//! every 30 s, plus a hint on render machines. Script can't stop a publish: Tools/release_check.py is the hard gate
//! (same list + the prefab / attribute lines; it also wants PUBLISH true). Keep both lists the same.
//! Not a release failure: MUD_CamSmooth.ON (user decides, it changes how the vehicle camera feels) - printed as info.
class MUD_Release
{
	static const bool PUBLISH = true;
	static const float REPEAT_MS = 30000;
	protected static string s_sOn;
	protected static bool s_bRepeat;
	protected static bool s_bBakedFail;     // audit D5: MUD_CoarseSkin baked heights did not open
	protected static bool s_bChecked;
	protected static bool s_bWgDebug;

	//------------------------------------------------------------------------------------------------
	//! Names of every switch not at its release value ("" = all at release). wgDebug = MudWorldGround m_bDebug.
	static string OnList(bool wgDebug)
	{
		array<string> on = {};
		if (MUD_Prof.ON)
			on.Insert("MUD_Prof.ON");
		if (MUD_Prof.CSV)
			on.Insert("MUD_Prof.CSV");
		if (MUD_TestKit.TEST_KIT)
			on.Insert("MUD_TestKit.TEST_KIT");
		if (MUD_StutterRobot.ON)
			on.Insert("MUD_StutterRobot.ON");
		if (MUD_AllowanceTest.ON)
			on.Insert("MUD_AllowanceTest.ON");
		if (MUD_PredictionTest.OFF)
			on.Insert("MUD_PredictionTest.OFF");
		if (MUD_BandMesher.LAYOUT_CHECK)
			on.Insert("MUD_BandMesher.LAYOUT_CHECK");
		if (MUD_MudSheet.EDGE_AUDIT)
			on.Insert("MUD_MudSheet.EDGE_AUDIT");
		if (MUD_MudSheet.FREEZE_TEST || MUD_MudSheet.DEBUG_SHOW || MUD_MudSheet.DEBUG_TELEPORT || MUD_MudSheet.GAP_PROBE || MUD_MudSheet.DRAG_TEST || MUD_MudSheet.OFF_TEST)
			on.Insert("MUD_MudSheet test flags");
		if (MUD_VoxelProbe.TRAIL_PROBE)
			on.Insert("MUD_VoxelProbe.TRAIL_PROBE");
		if (MUD_VoxelProbe.TUNNEL_DEBUG_T)
			on.Insert("MUD_VoxelProbe.TUNNEL_DEBUG_T");
		if (MUD_VoxelProbe.CRATER_DECAL)
			on.Insert("MUD_VoxelProbe.CRATER_DECAL");
		if (MUD_VoxelProbe.OP_PRINTS || MUD_VoxelProbe.OP_LOG || MUD_VoxelProbe.HASH_LOG)
			on.Insert("MUD_VoxelProbe op logs");
		if (MUD_VoxelProbe.DISP_LOG_ALWAYS)
			on.Insert("MUD_VoxelProbe.DISP_LOG_ALWAYS");
		if (MUD_VoxelProbe.DISP_HASH_ALWAYS)
			on.Insert("MUD_VoxelProbe.DISP_HASH_ALWAYS");
		if (MUD_Wheels.NET_LIMITS)
			on.Insert("MUD_Wheels.NET_LIMITS");
		if (MUD_MudSheet.ENABLED)
			on.Insert("MUD_MudSheet.ENABLED");     // parked: a plain static, mud lab numpad 3 can turn it on
		if (MUD_TrenchSiteComponent.ALLOW_INSTANT_DIG)
			on.Insert("MUD_TrenchSiteComponent.ALLOW_INSTANT_DIG");
		if (MUD_TrenchSiteComponent.DEBUG_INSTANT)
			on.Insert("MUD_TrenchSiteComponent.DEBUG_INSTANT");
		if (wgDebug)
			on.Insert("MudWorldGround.et m_bDebug");
		if (s_bBakedFail)
			on.Insert("MUD_CoarseSkin baked heights did not open");
		string txt;
		foreach (int i, string n : on)
		{
			if (i > 0)
				txt += ", ";
			txt += n;
		}
		return txt;
	}

	//------------------------------------------------------------------------------------------------
	//! Once per play, from MUD_WorldGroundEntity.EOnInit (play mode)
	static void Check(bool wgDebug)
	{
		s_bChecked = true;
		s_bWgDebug = wgDebug;
		s_sOn = OnList(wgDebug);
		if (!PUBLISH)
		{
			Print(string.Format("MUD debug on: %1 | camera smooth %2 (user decides)", s_sOn, MUD_CamSmooth.ON), LogLevel.NORMAL);
			return;
		}
		if (s_sOn.IsEmpty())
		{
			Print("MUD release gate: all switches at release values", LogLevel.NORMAL);
			return;
		}
		Report();
		if (!s_bRepeat)
		{
			s_bRepeat = true;
			GetGame().GetCallqueue().CallLater(Report, REPEAT_MS, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Audit D5: MUD_CoarseSkin.LoadBaked result. Not opened in a PUBLISH build = gate failure (the whole map traces
	//! at load, ~5 s freeze, and holes come from live traces instead of the bake).
	static void Baked(bool ok, string path)
	{
		Print(string.Format("MUD release gate: baked heights opened %1 ('%2')", ok, path), LogLevel.NORMAL);
		s_bBakedFail = !ok;
		if (s_bChecked && !ok)
			Check(s_bWgDebug);     // the skin loaded after the gate ran: run it again with the result
	}

	//------------------------------------------------------------------------------------------------
	protected static void Report()
	{
		Print("MUD RELEASE GATE FAILED: " + s_sOn, LogLevel.ERROR);
		if (!System.IsConsoleApp())
			SCR_HintManagerComponent.ShowCustomHint("Debug switches on in a PUBLISH build: " + s_sOn, "MUD RELEASE GATE FAILED", 10, true);
	}
}
