//! Trench builder F4 (spec sec 2 steps 1-2): shovel mode STAKE (R cycle ... Plan -> Stake -> off). Local player only.
//! LMB puts a corner where the crosshair is (walk out to the next corner, they stay), RMB undoes the last one, F
//! with 4 corners stakes the area (server checks it); F with no corner while standing in an area opens its manager.
//! Keys: the trench plan context (MUD_TrenchPoint / Undo / Confirm), same buttons as the trench planner.
class MUD_BuildStake
{
	static const float FLAG_H = 2.5;          // m corner flag pole
	static const float LINE_W = 0.35;         // m stake line (was 0.15 / 0.2, hard to see)
	protected ref array<vector> m_aCorners = {};
	protected ref MUD_BuildDraw m_Draw = new MUD_BuildDraw();
	protected bool m_bHasAim;
	protected vector m_vAim;
	protected vector m_vDrawnAim;
	protected bool m_bDirty = true;
	protected bool m_bListening;
	protected string m_sNote;
	protected string m_sShownHint;

	//------------------------------------------------------------------------------------------------
	void ~MUD_BuildStake()
	{
		RemoveListeners();
	}

	//------------------------------------------------------------------------------------------------
	bool Start()
	{
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return false;
		input.AddActionListener("MUD_TrenchPoint", EActionTrigger.DOWN, OnPoint);
		input.AddActionListener("MUD_TrenchUndo", EActionTrigger.DOWN, OnUndo);
		input.AddActionListener("MUD_TrenchConfirm", EActionTrigger.DOWN, OnConfirm);
		m_bListening = true;
		Print("MUD build stake: start");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Stop()
	{
		RemoveListeners();
		m_Draw.Clear();
		SCR_HintManagerComponent.ClearLatestHint();
		Print("MUD build stake: stop");
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveListeners()
	{
		if (!m_bListening)
			return;
		m_bListening = false;
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;
		input.RemoveActionListener("MUD_TrenchPoint", EActionTrigger.DOWN, OnPoint);
		input.RemoveActionListener("MUD_TrenchUndo", EActionTrigger.DOWN, OnUndo);
		input.RemoveActionListener("MUD_TrenchConfirm", EActionTrigger.DOWN, OnConfirm);
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame. False = stake mode ends (shovel gone, no player, in a vehicle).
	bool Update(float timeSlice)
	{
		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!player || player.IsInVehicle() || !MUD_Dig.HoldsShovel(player, true))
			return false;
		GetGame().GetInputManager().ActivateContext(MUD_TrenchPlanner.CONTEXT);
		m_bHasAim = MUD_TrenchPlanner.TraceAim(m_vAim);
		if (m_bHasAim && vector.Distance(m_vAim, m_vDrawnAim) > 0.1)
		{
			m_vDrawnAim = m_vAim;
			m_bDirty = true;
		}
		if (m_bDirty)
		{
			m_bDirty = false;
			Draw();
		}
		UpdateHint(player);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Corners so far (a flag each, seen from afar), a bright line on to the aim, the closed outline with 4
	//! (round 2, user 2026-10-24: "hard to see your points - clearer line + small flags")
	protected void Draw()
	{
		m_Draw.Begin();
		array<vector> line = {};
		foreach (vector c : m_aCorners)
		{
			line.Insert(c);
			m_Draw.Flag(c, FLAG_H);
			m_Draw.Square(c, 0.5, 0, MUD_BuildDraw.MARK, 0.06);
		}
		if (m_aCorners.Count() == 4)
		{
			MUD_BuildQuad q = MUD_BuildQuad.FromPoints(m_aCorners);
			string why;
			int state = MUD_BuildDraw.MARK;
			if (!q || !q.Valid(why))
				state = MUD_TrenchGhosts.RED;
			if (q)
				m_Draw.Ribbon(q.m_aC, LINE_W, state, true, 0.08);
		}
		else
		{
			if (m_bHasAim)
			{
				line.Insert(m_vAim);
				m_Draw.Square(m_vAim, 0.35, 45, MUD_BuildDraw.FLAG, 0.06);
				if (m_aCorners.Count() == 3)
				{
					array<vector> close = {};
					close.Insert(m_vAim);
					close.Insert(m_aCorners[0]);
					m_Draw.Ribbon(close, LINE_W * 0.5, MUD_TrenchGhosts.DUG, false, 0.08);     // where the 4th side will run
				}
			}
			if (line.Count() >= 2)
				m_Draw.Ribbon(line, LINE_W, MUD_BuildDraw.MARK, false, 0.08);
		}
		m_Draw.End();
	}

	//------------------------------------------------------------------------------------------------
	//! LMB: a corner at the aim (snapped to the ground grid)
	protected void OnPoint(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (!m_bHasAim)
			return;
		if (m_aCorners.Count() >= 4)
		{
			SetNote("4 corners placed: F stakes the area, RMB undoes the last");
			return;
		}
		vector p = MUD_WorldGroundEntity.SnapFine(m_vAim);
		foreach (vector c : m_aCorners)
		{
			if (vector.DistanceXZ(c, p) < MUD_Build.AREA_MIN_SIDE)
			{
				SetNote(string.Format("Too close to a corner (sides %1 to %2 m)", MUD_Build.AREA_MIN_SIDE, MUD_Build.AREA_MAX_SIDE));
				return;
			}
		}
		m_aCorners.Insert(p);
		SetNote("");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnUndo(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_aCorners.Count() > 0)
			m_aCorners.Remove(m_aCorners.Count() - 1);
		SetNote("");
	}

	//------------------------------------------------------------------------------------------------
	//! F: 4 corners -> stake; none and standing in an area -> open its manager
	protected void OnConfirm(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!pc || !player)
			return;
		if (m_aCorners.Count() == 0)
		{
			vector feet = player.GetOrigin();
			MUD_BuildAreaComponent area = MUD_BuildAreaComponent.AreaAt(feet[0], feet[2], MUD_Build.INSIDE_TOL);
			if (!area)
			{
				SetNote("Not in a build area: place 4 corners first");
				return;
			}
			Print(string.Format("MUD build stake: ask to open area %1", area.Id()));
			pc.MUD_AskOpenArea(area.Id());
			SetNote(string.Format("Opening build area %1 ...", area.Id()));
			return;
		}
		if (m_aCorners.Count() < 4)
		{
			SetNote(string.Format("%1 of 4 corners placed", m_aCorners.Count()));
			return;
		}
		MUD_BuildQuad q = MUD_BuildQuad.FromPoints(m_aCorners);
		string why;
		if (!q || !q.Valid(why))
		{
			SetNote("Area: " + why);
			return;
		}
		Print("MUD build stake: ask to stake " + q.Pack());
		pc.MUD_AskStakeArea(q.Pack());
		m_aCorners.Clear();
		SetNote("Staking ... then F inside the area opens its manager");
	}

	//------------------------------------------------------------------------------------------------
	protected void SetNote(string note)
	{
		m_sNote = note;
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHint(IEntity player)
	{
		string where = "Not in a build area";
		vector feet = player.GetOrigin();
		MUD_BuildAreaComponent area = MUD_BuildAreaComponent.AreaAt(feet[0], feet[2], MUD_Build.INSIDE_TOL);
		if (area)
		{
			string who = area.OwnerName() + " only (for now)";
			if (area.IsOpen())
				who = "everyone";
			where = string.Format("In build area %1 - edit: %2\nF (no corners): open the manager", area.Id(), who);
		}
		string size = "";
		if (m_aCorners.Count() >= 2)
		{
			int last = m_aCorners.Count() - 1;
			size = string.Format("\nLast side %1 m", Math.Round(vector.DistanceXZ(m_aCorners[last], m_aCorners[last - 1])));
		}
		string text = string.Format("LMB: corner here (%1 / 4) - walk to the next\nRMB: undo last corner\nF: stake the area (4 corners)\nR: leave\nSides %2 to %3 m%4\n\n%5\n%6",
			m_aCorners.Count(), MUD_Build.AREA_MIN_SIDE, MUD_Build.AREA_MAX_SIDE, size, where, m_sNote);
		if (text == m_sShownHint)
			return;
		m_sShownHint = text;
		SCR_HintManagerComponent.ShowCustomHint(text, "Build area", 3600, true);
	}
}
