//! MUD LAB (stage 1, plan docs/superpowers/plans/2026-10-20-mud-lab.md, build 2026-10-20; checkpoint v249 = before):
//! the permanent mud-look switcher. TEST_KIT numpad 3 cycles off -> sheet local presets -> D2 pressed lift presets ->
//! D1 overlay presets -> off; one surface mode at a time. A new play resets to off. Called every frame from
//! MUD_WorldGroundEntity's frame (render machines only).
//! Logs: 'MUD lab: surface <name> preset <p> | <why>' on every switch; 'MUD lab 10s: ...' every 10 s while a mode is
//! on. 'MUD ms' there = MUD_Prof's whole MUD_WorldGroundEntity.EOnFrame total per frame (the 'MUD script avg ms/frame'
//! of the 'MUD prof 5 s' line, the meter of results/2026-10-20-mud-two-designs-ds.md), over the 10 s.
class MUD_MudLab
{
	static const float LOG_S = 10;
	static const float OFF_SHOW_S = 3;          // 'off' label shown this long after the key

	protected static ref array<ref MUD_LabMode> s_aModes;
	protected static int s_iMode = -1;          // index in s_aModes, -1 = off
	protected static int s_iPreset;
	protected static int s_iStep;               // 0 = off, 1.. = position in the cycle
	protected static float s_fTickLast = -1;
	protected static float s_fShownAt = -100;
	// 10 s log
	protected static float s_fLogAt = -1;
	protected static int s_iFrames;
	protected static float s_fDtMax;
	protected static int s_iMudAt;              // MUD_Prof running totals at the window start
	protected static int s_iProfFramesAt;
	protected static int s_iModeAt;

	//------------------------------------------------------------------------------------------------
	protected static void Init()
	{
		s_aModes = {};
		s_aModes.Insert(new MUD_LabSheetLocal());
		s_aModes.Insert(new MUD_LabPressLift());
		s_aModes.Insert(new MUD_LabOverlay());
	}

	//------------------------------------------------------------------------------------------------
	//! Called every frame by MUD_WorldGroundEntity (render machines, ground ready)
	static void Tick(float time)
	{
		if (!s_aModes)
			Init();
		// a new play keeps the statics: every mode back to off (defaults)
		if (time < s_fTickLast)
		{
			SetMode(-1, 0, "new play");
			s_iStep = 0;
		}
		float dt = time - s_fTickLast;
		s_fTickLast = time;
		if (MUD_TestKit.TEST_KIT && Debug.KeyState(KeyCode.KC_NUMPAD3))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD3);
			Cycle();
			s_fShownAt = time;
		}
		if (MUD_TestKit.TEST_KIT)
			ShowMode(time);
		MUD_WorldGroundEntity.PressLog(time);     // pressed lift 10 s log (returns at once while it is off)
		if (s_iMode < 0)
			return;
		MUD_LabMode m = s_aModes[s_iMode];
		m.Tick(time);
		Log10(time, dt, m);
	}

	//------------------------------------------------------------------------------------------------
	protected static int StepCount()
	{
		int n = 0;
		foreach (MUD_LabMode m : s_aModes)
			n += m.Presets();
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! TEST_KIT numpad 3: next step of off -> sheet local -> D2 -> D1 -> off
	protected static void Cycle()
	{
		s_iStep++;
		if (s_iStep > StepCount())
			s_iStep = 0;
		if (s_iStep == 0)
		{
			SetMode(-1, 0, "key");
			return;
		}
		int left = s_iStep;
		for (int i = 0; i < s_aModes.Count(); i++)
		{
			int n = s_aModes[i].Presets();
			if (left <= n)
			{
				SetMode(i, left, "key");
				return;
			}
			left -= n;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Mode i (-1 = off) at preset p: another mode exits first (only one mode at a time); a preset change inside one
	//! mode goes straight to Enter(p), as the old overlay cycle did
	protected static void SetMode(int i, int p, string why)
	{
		if (s_iMode >= 0 && s_iMode != i)
			s_aModes[s_iMode].Exit();
		bool same = s_iMode == i && s_iPreset == p;
		s_iMode = i;
		s_iPreset = p;
		if (i >= 0 && !same)
			s_aModes[i].Enter(p);
		s_fLogAt = -1;
		string name = "off";
		if (i >= 0)
			name = s_aModes[i].Name();
		Print(string.Format("MUD lab: surface %1 preset %2 | %3", name, p, why), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! TEST_KIT: which surface is on, top left (while one is on; 'off' for OFF_SHOW_S after the key)
	protected static void ShowMode(float time)
	{
		string txt = "";
		if (s_iMode >= 0)
			txt = s_aModes[s_iMode].Name() + " - preset " + s_iPreset.ToString() + ": " + s_aModes[s_iMode].Label();
		else if (time - s_fShownAt < OFF_SHOW_S)
			txt = "mud look OFF (today's ground)";
		if (txt == "")
			return;
		DbgUI.Begin("MUD lab (numpad 3)", 20, 120);
		DbgUI.Text(string.Format("step %1 of %2: %3", s_iStep, StepCount(), txt));
		DbgUI.End();
	}

	//------------------------------------------------------------------------------------------------
	//! every LOG_S while a mode is on: fps, MUD ms, the mode's own ms, its tiles and extras
	protected static void Log10(float time, float dt, MUD_LabMode m)
	{
		if (s_fLogAt < 0)
		{
			s_fLogAt = time;
			s_iFrames = 0;
			s_fDtMax = 0;
			s_iMudAt = MUD_Prof.MudAll();
			s_iProfFramesAt = MUD_Prof.FramesAll();
			s_iModeAt = m.ModeMsAll();
			return;
		}
		s_iFrames++;
		if (dt > s_fDtMax)
			s_fDtMax = dt;
		float span = time - s_fLogAt;
		if (span < LOG_S)
			return;
		float fps = s_iFrames / span;
		float fpsMin = 0;
		if (s_fDtMax > 0)
			fpsMin = 1 / s_fDtMax;
		float pf = Math.Max(1, MUD_Prof.FramesAll() - s_iProfFramesAt);
		float mudMs = (MUD_Prof.MudAll() - s_iMudAt) / pf;
		float modeMs = (m.ModeMsAll() - s_iModeAt) / pf;
		Print(string.Format("MUD lab 10s: surface %1/%2 | fps avg %3 min %4 | MUD ms %5 | mode ms %6 | %7", m.Name(), s_iPreset, fps.ToString(-1, 1), fpsMin.ToString(-1, 1), mudMs.ToString(-1, 2), modeMs.ToString(-1, 2), m.Log10s()), LogLevel.NORMAL);
		s_fLogAt = time;
		s_iFrames = 0;
		s_fDtMax = 0;
		s_iMudAt = MUD_Prof.MudAll();
		s_iProfFramesAt = MUD_Prof.FramesAll();
		s_iModeAt = m.ModeMsAll();
	}
}

//! One surface mode of the lab
class MUD_LabMode
{
	string Name() { return ""; }
	int Presets() { return 0; }
	void Enter(int preset) {}
	void Exit() {}
	void Tick(float time) {}
	string Label() { return ""; }
	//! 'tiles <n> built <n> kept <n> | <extras>' for the 10 s line (built = with a mesh, kept = data only)
	string Log10s() { return ""; }
	//! running total of this mode's own ms (MUD_Prof parts or its own meter), never reset
	int ModeMsAll() { return 0; }
}

//! Sheet local: the parked MUD_MudSheet, client-only, deposit band (part B). Its own 'MUD sheet builds' line comes from
//! MUD_MudSheet.Log10 every 10 s while it runs.
class MUD_LabSheetLocal : MUD_LabMode
{
	protected int m_iFrAt = -1;     // MUD_Prof / sheet running totals at the last Log10s
	protected int m_iWhAt;
	protected int m_iUpAt;
	protected int m_iStAt;
	protected int m_iSlAt;
	protected int m_iBaAt;
	override string Name() { return "sheet local"; }
	override int Presets() { return MUD_MudSheet.LAB_PRESETS; }
	override void Enter(int preset)
	{
		MUD_MudSheet.LabSet(preset, "key");
		Snap();
	}
	override void Exit() { MUD_MudSheet.LabSet(0, "key"); }
	//! part C: TEST_KIT numpad 0 cycles the cost lever sets (none -> all -> L1 -> L2 -> L3 -> L6 -> L8 -> none)
	override void Tick(float time)
	{
		if (!MUD_TestKit.TEST_KIT || !Debug.KeyState(KeyCode.KC_NUMPAD0))
			return;
		Debug.ClearKey(KeyCode.KC_NUMPAD0);
		MUD_MudSheet.LabLeverStep("key");
	}
	override string Label()
	{
		return string.Format("band in %1 m, fade %2 m | levers (numpad 0): %3", MUD_MudSheet.BAND_IN, MUD_MudSheet.BAND_FADE, MUD_MudSheet.LeverNames());
	}
	override int ModeMsAll() { return MUD_Prof.PartAll(MUD_Prof.SHEET) + MUD_Prof.PartAll(MUD_Prof.SHEET_SYNC); }
	override string Log10s()
	{
		float pf = Math.Max(1, MUD_Prof.FramesAll() - m_iFrAt);
		float wh = (MUD_Prof.PartAll(MUD_Prof.SH_WHEELS) - m_iWhAt) / pf;
		float up = (MUD_Prof.PartAll(MUD_Prof.SH_UPLOAD) - m_iUpAt) / pf;
		float st = (MUD_Prof.PartAll(MUD_Prof.SH_STORED) - m_iStAt) / pf;
		float sl = (MUD_MudSheet.SlumpMsAll() - m_iSlAt) / pf;
		float ba = (MUD_MudSheet.BandMsAll() - m_iBaAt) / pf;
		string s = MUD_MudSheet.LabCounts() + string.Format(" | sh wheels %1 / upload %2 / stored %3 / slump %4 / band %5 ms per frame | levers %6",
			wh.ToString(-1, 2), up.ToString(-1, 2), st.ToString(-1, 2), sl.ToString(-1, 2), ba.ToString(-1, 2), MUD_MudSheet.LeverNames()) + MUD_MudSheet.OpLog();
		Snap();
		return s;
	}
	protected void Snap()
	{
		m_iFrAt = MUD_Prof.FramesAll();
		m_iWhAt = MUD_Prof.PartAll(MUD_Prof.SH_WHEELS);
		m_iUpAt = MUD_Prof.PartAll(MUD_Prof.SH_UPLOAD);
		m_iStAt = MUD_Prof.PartAll(MUD_Prof.SH_STORED);
		m_iSlAt = MUD_MudSheet.SlumpMsAll();
		m_iBaAt = MUD_MudSheet.BandMsAll();
	}
}

//! D2: MUD_WorldGroundEntity.PRESS_LIFT (pressed lift, its own 'MUD pressed lift' 10 s line keeps running)
class MUD_LabPressLift : MUD_LabMode
{
	override string Name() { return "D2 pressed lift"; }
	override int Presets() { return MUD_WorldGroundEntity.PRESS_PRESETS; }
	override void Enter(int preset) { MUD_WorldGroundEntity.SetPressLift(preset, "key"); }
	override void Exit() { MUD_WorldGroundEntity.SetPressLift(0, "key"); }
	override string Label()
	{
		return string.Format("lift %1 m, press %2 m, wear press %3", MUD_WorldGroundEntity.s_fPressLiftM, MUD_WorldGroundEntity.s_fPressM, MUD_WorldGroundEntity.s_fWearPress);
	}
	override string Log10s() { return "tiles - built - kept - (patch meshes) | mode ms = MUD_Prof 'rebuild' part (D2 is drawn by patch rebuilds)"; }
	override int ModeMsAll() { return MUD_Prof.PartAll(MUD_Prof.REBUILD); }
}

//! D1: MUD_MudOverlay (its own 'MUD mud overlay' 10 s lines keep running)
class MUD_LabOverlay : MUD_LabMode
{
	protected int m_iMs;
	override string Name() { return "D1 overlay"; }
	override int Presets() { return MUD_MudOverlay.PRESETS; }
	override void Enter(int preset) { MUD_MudOverlay.SetPreset(preset, "key"); }
	override void Exit() { MUD_MudOverlay.SetPreset(0, "key"); }
	override void Tick(float time)
	{
		int t0 = System.GetTickCount();
		MUD_MudOverlay.LabFrame(time);
		m_iMs += System.GetTickCount(t0);
	}
	override string Label()
	{
		if (MUD_MudOverlay.s_iPreset == 1)
			return "live stamps + re-centre + fallback";
		return "fallback only (late-join view)";
	}
	override string Log10s() { return MUD_MudOverlay.LabCounts(); }
	override int ModeMsAll() { return m_iMs; }
}
