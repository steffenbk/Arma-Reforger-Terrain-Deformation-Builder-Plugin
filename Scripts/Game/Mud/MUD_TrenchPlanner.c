//! Trench plan mode (spec sections 3-4; pattern of CSK_SandbagWallPlanner Start / Stop / Update /
//! TraceAim / MoveBend). Local player only, nothing is sent before F. Shovel in hand, R cycles
//! off -> Dig -> Fill -> Plan -> off (MUD_Dig.NextMode; user 2026-10-10 00:30).
//! Keys: Configs/System/chimeraInputCommon.conf (MUD_TrenchPlanContext).
class MUD_TrenchPlanner
{
	static const string CONTEXT = "MUD_TrenchPlanContext";
	static const float AIM_RANGE = 15;          // m, camera trace length
	static const float AIM_MOVE_REDRAW = 0.05;  // m the aim must move before the live ghost is redrawn
	static const float BEND_SNAP = 0.15;        // m: a smaller bend snaps back to straight
	static const float BEND_PICK = 3;           // m from a segment's middle that V picks it

	protected ref MUD_TrenchPlan m_Plan = new MUD_TrenchPlan();
	protected ref MUD_TrenchGhosts m_Placed = new MUD_TrenchGhosts();    // the planned segments
	protected ref MUD_TrenchGhosts m_Live = new MUD_TrenchGhosts();      // last point -> aim (or the aim marker)
	protected bool m_bHasAim;
	protected vector m_vAim;
	protected vector m_vDrawnAim;
	protected bool m_bDirty = true;
	protected bool m_bFinished;
	protected bool m_bListening;
	protected int m_iBending = -1;      // segment whose bend follows the aim (V), -1 = none
	protected float m_fBendFrom;        // its bend before, for RMB
	protected int m_iPieces;
	protected int m_iRed;
	protected string m_sNote;
	protected string m_sShownHint;

	//------------------------------------------------------------------------------------------------
	void ~MUD_TrenchPlanner()
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
		input.AddActionListener("MUD_TrenchWidth", EActionTrigger.DOWN, OnWidth);
		input.AddActionListener("MUD_TrenchBend", EActionTrigger.DOWN, OnBend);
		input.AddActionListener("MUD_TrenchConfirm", EActionTrigger.DOWN, OnConfirm);
		m_bListening = true;
		Print("MUD trench plan: start");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Stop()
	{
		RemoveListeners();
		m_Placed.Clear();
		m_Live.Clear();
		SCR_HintManagerComponent.ClearLatestHint();
		Print("MUD trench plan: stop");
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
		input.RemoveActionListener("MUD_TrenchWidth", EActionTrigger.DOWN, OnWidth);
		input.RemoveActionListener("MUD_TrenchBend", EActionTrigger.DOWN, OnBend);
		input.RemoveActionListener("MUD_TrenchConfirm", EActionTrigger.DOWN, OnConfirm);
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while planning. False = plan mode ends (confirmed, shovel gone, no player).
	bool Update(float timeSlice)
	{
		if (m_bFinished)
			return false;
		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!player || player.IsInVehicle() || !MUD_Dig.HoldsShovel(player, true))
			return false;

		InputManager input = GetGame().GetInputManager();
		input.ActivateContext(CONTEXT);
		m_bHasAim = TraceAim(m_vAim);

		// mouse wheel: one notch = one 0.25 m layer, the whole trench
		float wheel = input.GetActionValue("MUD_TrenchDepth");
		if (wheel != 0)
		{
			int delta = 1;
			if (wheel < 0)
				delta = -1;
			m_Plan.m_iLayers = Math.ClampInt(m_Plan.m_iLayers + delta, MUD_TrenchPlan.MIN_LAYERS, MUD_TrenchPlan.MAX_LAYERS);
			m_bDirty = true;
		}

		if (m_bHasAim && vector.Distance(m_vAim, m_vDrawnAim) > AIM_MOVE_REDRAW)
		{
			m_vDrawnAim = m_vAim;
			if (m_iBending >= 0)
			{
				MoveBend();
				m_bDirty = true;
			}
			else
			{
				DrawLive();
			}
		}

		if (m_bDirty)
		{
			m_bDirty = false;
			DrawPlaced();
			DrawLive();
		}
		UpdateHint();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Ground point under the crosshair (camera ray, then straight down if the ray hits nothing).
	//! Copy of CSK_SandbagWallPlanner.TraceAim (vanilla SCR_ItemPlacementComponent trace setup).
	protected bool TraceAim(out vector aim)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		CameraManager cameras = GetGame().GetCameraManager();
		if (!character || !cameras || !cameras.CurrentCamera())
			return false;

		vector cameraMat[4];
		cameras.CurrentCamera().GetTransform(cameraMat);
		BaseWorld world = character.GetWorld();

		TraceParam param = new TraceParam();
		param.Start = character.EyePosition();
		param.End = param.Start + cameraMat[2] * AIM_RANGE;
		param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		param.Exclude = character;
		param.LayerMask = EPhysicsLayerPresets.Projectile;
		float hit = world.TraceMove(param, null);
		aim = param.Start + (param.End - param.Start) * hit;
		// the holed heightmap stays above a dug voxel trench (WORLD hit): aim again at entities only (2026-10-15)
		if (hit < 1 && GenericTerrainEntity.Cast(param.TraceEnt) && MUD_Surface.IsHole(world, aim[0], aim[2]))
		{
			param.Flags = TraceFlags.ENTS;
			param.TraceEnt = null;
			hit = world.TraceMove(param, null);
			aim = param.Start + (param.End - param.Start) * hit;
			param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		}

		if (hit >= 1)
		{
			param.Start = param.End + vector.Up;
			param.End = param.Start - vector.Up * 20;
			hit = world.TraceMove(param, null);
			if (hit >= 1)
				return false;

			aim = param.Start + (param.End - param.Start) * hit;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The aim, pulled back to MAX_SEGMENT from `from` (flat) when it is farther
	protected vector ClampAim(vector from)
	{
		vector d = m_vAim - from;
		d[1] = 0;
		float len = d.Length();
		if (len <= MUD_TrenchPlan.MAX_SEGMENT)
			return m_vAim;
		vector p = from + d * (MUD_TrenchPlan.MAX_SEGMENT / len);
		p[1] = m_vAim[1];
		return p;
	}

	//------------------------------------------------------------------------------------------------
	//! Green where the piece middle is over a hole cell, red elsewhere (kept: road, ...). Returns red count.
	protected int States(MUD_TrenchLayout lay, array<int> states)
	{
		int red;
		for (int i = 0; i < lay.Count(); i++)
		{
			if (MUD_WorldGroundEntity.IsOver(lay.PieceMiddle(i)))
			{
				states.Insert(MUD_TrenchGhosts.GREEN);
			}
			else
			{
				states.Insert(MUD_TrenchGhosts.RED);
				red++;
			}
		}
		return red;
	}

	//------------------------------------------------------------------------------------------------
	//! The planned segments, built from the packed text (what the server gets), so ghost = dug
	protected void DrawPlaced()
	{
		m_iPieces = 0;
		m_iRed = 0;
		if (m_Plan.m_aPoints.Count() < 2)
		{
			m_Placed.Clear();
			return;
		}
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!lay.Build(MUD_TrenchPlan.Unpack(m_Plan.Pack())))
		{
			m_Placed.Clear();
			SetNote("Plan: " + lay.m_sError);
			return;
		}
		array<int> states = {};
		m_iRed = States(lay, states);
		m_iPieces = lay.Count();
		m_Placed.Draw(lay, states);
	}

	//------------------------------------------------------------------------------------------------
	//! Last point -> aim (clamped to 12 m); with no point yet a 0.5 m marker across the aim
	protected void DrawLive()
	{
		int n = m_Plan.m_aPoints.Count();
		if (!m_bHasAim || m_iBending >= 0 || n >= MUD_TrenchPlan.MAX_POINTS)
		{
			m_Live.Clear();
			return;
		}
		MUD_TrenchPlan live = new MUD_TrenchPlan();
		live.m_iWidth = m_Plan.m_iWidth;
		live.m_iLayers = m_Plan.m_iLayers;
		if (n == 0)
		{
			live.AddPoint(m_vAim - Vector(0.25, 0, 0));
			live.AddPoint(m_vAim + Vector(0.25, 0, 0));
		}
		else
		{
			live.AddPoint(m_Plan.m_aPoints[n - 1]);
			live.AddPoint(ClampAim(m_Plan.m_aPoints[n - 1]));
		}
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!lay.Build(MUD_TrenchPlan.Unpack(live.Pack())))
		{
			m_Live.Clear();
			return;
		}
		array<int> states = {};
		States(lay, states);
		m_Live.Draw(lay, states);
	}

	//------------------------------------------------------------------------------------------------
	//! LMB: place a point at the aim (bending: keep the bend)
	protected void OnPoint(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_iBending >= 0)
		{
			m_iBending = -1;
			m_bDirty = true;
			return;
		}
		if (!m_bHasAim)
			return;
		int n = m_Plan.m_aPoints.Count();
		if (n >= MUD_TrenchPlan.MAX_POINTS)
		{
			SetNote(string.Format("At most %1 points", MUD_TrenchPlan.MAX_POINTS));
			return;
		}
		vector p = m_vAim;
		if (n > 0)
		{
			p = ClampAim(m_Plan.m_aPoints[n - 1]);
			if (vector.DistanceSqXZ(p, m_Plan.m_aPoints[n - 1]) < MUD_TrenchPlan.MIN_SEGMENT * MUD_TrenchPlan.MIN_SEGMENT)
			{
				SetNote("Too close to the last point");
				return;
			}
		}
		m_Plan.AddPoint(p);
		SetNote("");
	}

	//------------------------------------------------------------------------------------------------
	//! RMB: undo the last point (bending: put the bend back)
	protected void OnUndo(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_iBending >= 0)
		{
			m_Plan.m_aBends[m_iBending] = m_fBendFrom;
			m_iBending = -1;
			m_bDirty = true;
			return;
		}
		int n = m_Plan.m_aPoints.Count();
		if (n == 0)
			return;
		m_Plan.m_aPoints.Remove(n - 1);
		m_Plan.m_aBends.Remove(n - 1);
		// the segment that ended at the removed point is gone: its bend (stored on the point before) too
		if (n >= 2)
			m_Plan.m_aBends[n - 2] = 0;
		SetNote("");
	}

	//------------------------------------------------------------------------------------------------
	//! X: next width (whole trench)
	protected void OnWidth(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		m_Plan.m_iWidth = (m_Plan.m_iWidth + 1) % MUD_TrenchPlan.WIDTHS;
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! V: bend the segment nearest the aim (its middle follows the aim); V / LMB keep, RMB undo
	protected void OnBend(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_iBending >= 0)
		{
			m_iBending = -1;
			m_bDirty = true;
			return;
		}
		int seg = NearestSegment();
		if (seg < 0)
		{
			SetNote(string.Format("V: aim near the middle of a segment of %1 m or more", MUD_TrenchPlan.MIN_BEND_CHORD));
			return;
		}
		m_iBending = seg;
		m_fBendFrom = m_Plan.m_aBends[seg];
		SetNote("");
	}

	//------------------------------------------------------------------------------------------------
	//! Segment (chord >= MIN_BEND_CHORD) whose chord middle is nearest the aim, within BEND_PICK; -1 = none
	protected int NearestSegment()
	{
		if (!m_bHasAim)
			return -1;
		int best = -1;
		float bestSq = BEND_PICK * BEND_PICK;
		for (int i = 0; i < m_Plan.m_aPoints.Count() - 1; i++)
		{
			vector a = m_Plan.m_aPoints[i];
			vector b = m_Plan.m_aPoints[i + 1];
			if (vector.DistanceSqXZ(a, b) < MUD_TrenchPlan.MIN_BEND_CHORD * MUD_TrenchPlan.MIN_BEND_CHORD)
				continue;
			float dSq = vector.DistanceSqXZ((a + b) * 0.5, m_vAim);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = i;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Bent segment follows the aim: its middle goes as far off the chord as the aim is
	//! (CSK_SandbagWallPlanner.MoveBend)
	protected void MoveBend()
	{
		if (m_iBending < 0 || m_iBending >= m_Plan.m_aPoints.Count() - 1)
			return;
		vector a = m_Plan.m_aPoints[m_iBending];
		vector b = m_Plan.m_aPoints[m_iBending + 1];
		a[1] = 0;
		b[1] = 0;
		vector chord = b - a;
		float len = chord.Length();
		if (len < 0.001)
			return;
		vector right = chord * (1 / len);
		vector fwd = Vector(-right[2], 0, right[0]);
		vector rel = m_vAim - (a + b) * 0.5;
		rel[1] = 0;
		float maxBend = len * 0.5 * MUD_TrenchPlan.MAX_BEND;
		float bend = Math.Clamp(vector.Dot(rel, fwd), -maxBend, maxBend);
		if (Math.AbsFloat(bend) < BEND_SNAP)
			bend = 0;
		m_Plan.m_aBends[m_iBending] = bend;
	}

	//------------------------------------------------------------------------------------------------
	//! F: send the plan to the server once, leave plan mode
	protected void OnConfirm(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_Plan.m_aPoints.Count() < 2)
		{
			SetNote("Place at least 2 points");
			return;
		}
		string packed = m_Plan.Pack();
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!lay.Build(MUD_TrenchPlan.Unpack(packed)))
		{
			SetNote("Plan: " + lay.m_sError);
			return;
		}
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;
		array<int> states = {};
		int red = States(lay, states);
		// Ctrl held (MUD_TrenchInstant): dig the whole trench at once (user 2026-10-11, live server test)
		bool instant = GetGame().GetInputManager().GetActionValue("MUD_TrenchInstant") > 0.5;
		Print(string.Format("MUD trench plan: confirm %1, pieces %2, red %3, instant %4", packed, lay.Count(), red, instant));
		pc.MUD_AskPlaceTrench(packed, instant);
		m_bFinished = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetNote(string note)
	{
		m_sNote = note;
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHint()
	{
		string mouse;
		if (m_iBending >= 0)
			mouse = string.Format("Bending segment %1: move the aim to the side\nLMB / V: keep\nRMB: put it back", m_iBending + 1);
		else
			mouse = string.Format("LMB: place point (%1/%2)\nRMB: undo last point\nV: bend the segment under the aim", m_Plan.m_aPoints.Count(), MUD_TrenchPlan.MAX_POINTS);

		string red;
		if (m_iRed > 0)
			red = string.Format(", %1 red (road / hard ground: not dug)", m_iRed);

		string text = string.Format("%1\nWheel: depth %2 m\nX: width %3 m\nF: confirm, then dig it with the shovel\nCtrl+F: confirm, dug at once\nR: leave\n\nPieces: %4%5\n%6",
			mouse, m_Plan.m_iLayers * MUD_TrenchPlan.LAYER, m_Plan.Width(), m_iPieces, red, m_sNote);
		if (text == m_sShownHint)
			return;
		m_sShownHint = text;
		SCR_HintManagerComponent.ShowCustomHint(text, "Trench", 3600, true);
	}
}

//------------------------------------------------------------------------------------------------
//! Trench plan mode: the planner lives on the local player controller; the confirmed plan goes to
//! the server once (the controller is owned by its client, so it can send server RPCs)
modded class SCR_PlayerController
{
	static const float MUD_PLACE_RANGE = 60;      // m: every plan point this near the player (server check)
	static const float MUD_PLACE_GAP_MS = 2000;   // server: one placed trench per player this often
	static const int MUD_PLAN_MAX_CHARS = 400;

	protected ref MUD_TrenchPlanner m_MUD_Planner;    // local player only, while plan mode is on
	protected float m_fMudLastPlace = -100000;

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(float timeSlice)
	{
		super.OnUpdate(timeSlice);

		if (m_MUD_Planner && !m_MUD_Planner.Update(timeSlice))
			MUD_StopTrenchPlanning();
	}

	//------------------------------------------------------------------------------------------------
	bool MUD_IsTrenchPlanning()
	{
		return m_MUD_Planner != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Local player: the shovel mode cycle reached Plan (MUD_Dig.SetMode)
	void MUD_StartTrenchPlanning()
	{
		if (this != GetGame().GetPlayerController() || m_MUD_Planner)
			return;
		MUD_TrenchPlanner planner = new MUD_TrenchPlanner();
		if (!planner.Start())
			return;
		m_MUD_Planner = planner;
		MUD_Dig.RemoveHelper();
	}

	//------------------------------------------------------------------------------------------------
	//! Also the shovel mode goes off (F confirmed, shovel gone, R).
	void MUD_StopTrenchPlanning()
	{
		if (!m_MUD_Planner)
			return;
		MUD_TrenchPlanner planner = m_MUD_Planner;
		m_MUD_Planner = null;
		planner.Stop();
		MUD_Dig.SetMode(MUD_Dig.MODE_OFF);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner client: place a confirmed plan (MUD_TrenchPlan.Pack text)
	void MUD_AskPlaceTrench(string packed, bool instant = false)
	{
		if (Replication.IsServer())
			MUD_RpcAsk_PlaceTrench(packed, instant);
		else
			Rpc(MUD_RpcAsk_PlaceTrench, packed, instant);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_PlaceTrench(string packed, bool instant)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(GetControlledEntity());
		if (!c || c.IsInVehicle())
			return;

		float now = GetGame().GetWorld().GetWorldTime();
		if (now - m_fMudLastPlace < MUD_PLACE_GAP_MS)
		{
			Print("MUD trench site: refused, placed one less than 2 s ago", LogLevel.WARNING);
			return;
		}
		if (!MUD_Dig.HoldsShovel(c, false))
		{
			Print("MUD trench site: refused, server sees no entrenching tool in hand", LogLevel.WARNING);
			return;
		}
		if (packed.Length() > MUD_PLAN_MAX_CHARS)
		{
			Print(string.Format("MUD trench site: refused, plan text %1 chars", packed.Length()), LogLevel.WARNING);
			return;
		}
		MUD_TrenchPlan plan = MUD_TrenchPlan.Unpack(packed);
		if (!plan)
		{
			Print("MUD trench site: refused, plan does not unpack: " + packed, LogLevel.WARNING);
			return;
		}
		vector feet = c.GetOrigin();
		foreach (vector p : plan.m_aPoints)
		{
			if (vector.DistanceSqXZ(p, feet) > MUD_PLACE_RANGE * MUD_PLACE_RANGE)
			{
				Print(string.Format("MUD trench site: refused, point %1 more than %2 m from the player", p, MUD_PLACE_RANGE), LogLevel.WARNING);
				return;
			}
		}

		m_fMudLastPlace = now;
		string why;
		MUD_TrenchSiteComponent site = MUD_TrenchSiteComponent.Spawn(plan, why);
		if (!site)
			Print("MUD trench site: refused, " + why, LogLevel.WARNING);
		else if (instant || MUD_TrenchSiteComponent.DEBUG_INSTANT)
			site.DebugDigAll();
	}
}
