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
