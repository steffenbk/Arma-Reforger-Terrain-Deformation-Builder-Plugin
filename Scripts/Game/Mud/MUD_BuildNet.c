//! Trench builder F4: the local player's stake mode and manager live on the player controller; every change goes to
//! the server as an RPC (the controller is owned by its client, so it can send server RPCs) and the server checks it.
//! File name sorts after MUD_Answer.c (MUD_Tell) - see the bare-call order rule in that file.
modded class SCR_PlayerController
{
	static const float MUD_STAKE_GAP_MS = 2000;   // server: one staked area per player this often

	protected ref MUD_BuildStake m_MUD_Stake;       // local player only, while shovel mode STAKE is on
	protected ref MUD_BuildManager m_MUD_Manager;   // local player only, while the manager is open
	protected float m_fMudLastStake = -100000;
	protected int m_iMudKeepAt;

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(float timeSlice)
	{
		super.OnUpdate(timeSlice);

		if (m_MUD_Stake && !m_MUD_Stake.Update(timeSlice))
			MUD_StopStake();
		if (m_MUD_Manager)
		{
			if (!m_MUD_Manager.Update(timeSlice))
			{
				MUD_StopManager();
			}
			else if (System.GetTickCount(m_iMudKeepAt) > MUD_Build.KEEP_ALIVE_MS)
			{
				m_iMudKeepAt = System.GetTickCount();
				MUD_AskKeepArea(m_MUD_Manager.AreaId());
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	bool MUD_IsStaking()
	{
		return m_MUD_Stake != null;
	}

	//------------------------------------------------------------------------------------------------
	bool MUD_IsManaging()
	{
		return m_MUD_Manager != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Local player: the shovel mode cycle reached STAKE (MUD_Dig.SetMode)
	void MUD_StartStake()
	{
		if (this != GetGame().GetPlayerController() || m_MUD_Stake || m_MUD_Manager)
			return;
		MUD_BuildStake stake = new MUD_BuildStake();
		if (!stake.Start())
			return;
		m_MUD_Stake = stake;
	}

	//------------------------------------------------------------------------------------------------
	//! Also the shovel mode goes off (shovel gone, R, the manager opened)
	void MUD_StopStake()
	{
		if (!m_MUD_Stake)
			return;
		MUD_BuildStake stake = m_MUD_Stake;
		m_MUD_Stake = null;
		stake.Stop();
		MUD_Dig.SetMode(MUD_Dig.MODE_OFF);
	}

	//------------------------------------------------------------------------------------------------
	//! Local player: the server let us edit area (MUD_RpcDo_AreaOpen)
	protected void MUD_StartManager(int area)
	{
		if (this != GetGame().GetPlayerController() || m_MUD_Manager)
			return;
		MUD_StopStake();
		MUD_BuildManager manager = new MUD_BuildManager();
		if (!manager.Start(area))
		{
			MUD_AskCloseArea(area);
			SCR_HintManagerComponent.ShowCustomHint("The manager could not open here", "Trench builder", 4, true);
			return;
		}
		m_MUD_Manager = manager;
		m_iMudKeepAt = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	void MUD_StopManager()
	{
		if (!m_MUD_Manager)
			return;
		MUD_BuildManager manager = m_MUD_Manager;
		m_MUD_Manager = null;
		manager.Stop();
		MUD_AskCloseArea(manager.AreaId());
		string why = manager.WhyClosed();
		if (!why.IsEmpty() && why != "Backspace")
			SCR_HintManagerComponent.ShowCustomHint("Trench builder closed: " + why, "Trench builder", 5, true);
	}

	//================================================================================================
	// stake

	//------------------------------------------------------------------------------------------------
	//! Owner client: stake an area (MUD_BuildQuad.Pack text)
	void MUD_AskStakeArea(string quad)
	{
		if (Replication.IsServer())
			MUD_RpcAsk_StakeArea(quad);
		else
			Rpc(MUD_RpcAsk_StakeArea, quad);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_StakeArea(string quad)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(GetControlledEntity());
		if (!c || c.IsInVehicle())
			return;
		float now = GetGame().GetWorld().GetWorldTime();
		if (now - m_fMudLastStake < MUD_STAKE_GAP_MS)
		{
			MUD_Tell("Area not staked: wait 2 s");
			return;
		}
		m_fMudLastStake = now;
		if (!MUD_Dig.HoldsShovel(c, false))
		{
			Print("MUD build area: stake refused, server sees no entrenching tool in hand", LogLevel.WARNING);
			MUD_Tell("Area not staked: hold an entrenching tool");
			return;
		}
		MUD_BuildQuad q = MUD_BuildQuad.Unpack(quad);
		if (!q)
		{
			Print("MUD build area: stake refused, corners do not unpack: " + quad, LogLevel.WARNING);
			return;
		}
		// corners are walked out one by one: the player is near the last ones, and inside a 200 m reach of all
		vector feet = c.GetOrigin();
		float nearest = 1000000000;
		foreach (vector p : q.m_aC)
		{
			nearest = Math.Min(nearest, vector.DistanceXZ(p, feet));
		}
		if (nearest > MUD_Build.STAKE_REACH || q.DistOutside(feet[0], feet[2]) > MUD_Build.STAKE_REACH)
		{
			Print(string.Format("MUD build area: stake refused, player at %1 is %2 m from the nearest corner", feet, nearest), LogLevel.WARNING);
			MUD_Tell(string.Format("Area not staked: stand within %1 m of a corner", MUD_Build.STAKE_REACH));
			return;
		}
		string why;
		MUD_BuildAreaComponent area = MUD_BuildAreaComponent.Spawn(q, GetPlayerId(), why);
		if (!area)
		{
			Print("MUD build area: stake refused, " + why, LogLevel.WARNING);
			MUD_Tell("Area not staked: " + why);
			return;
		}
		MUD_Tell(string.Format("Build area %1 staked - F inside it (shovel, build area mode) opens the manager", area.Id()));
	}

	//================================================================================================
	// manager open / close / keep-alive

	//------------------------------------------------------------------------------------------------
	void MUD_AskOpenArea(int area)
	{
		if (Replication.IsServer())
			MUD_RpcAsk_OpenArea(area);
		else
			Rpc(MUD_RpcAsk_OpenArea, area);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_OpenArea(int area)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(GetControlledEntity());
		MUD_BuildAreaComponent a = MUD_BuildAreaComponent.Find(area);
		string why;
		bool ok = false;
		if (!c || c.IsInVehicle())
			why = "get out of the vehicle";
		else if (!a || !a.Quad())
			why = "no such area";
		else
		{
			vector feet = c.GetOrigin();
			if (!a.Quad().Inside(feet[0], feet[2], MUD_Build.INSIDE_TOL + 1))
				why = "stand inside the area";
			else
				ok = a.TryLock(GetPlayerId(), why);
		}
		if (!ok)
			Print(string.Format("MUD build area: player %1 may not open area %2: %3", GetPlayerId(), area, why));
		if (GetPlayerId() == SCR_PlayerController.GetLocalPlayerId())
			MUD_RpcDo_AreaOpen(area, ok, why);     // host / SP player: an owner RPC to ourselves would not arrive
		else
			Rpc(MUD_RpcDo_AreaOpen, area, ok, why);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MUD_RpcDo_AreaOpen(int area, bool ok, string why)
	{
		if (!ok)
		{
			SCR_HintManagerComponent.ShowCustomHint(string.Format("Build area %1: %2", area, why), "Trench builder", 5, true);
			return;
		}
		// next frame: on a host this runs inside the F listener of the stake mode - the manager's own F (Save) must
		// not catch the same press, and the stake listeners are removed outside the input dispatch
		GetGame().GetCallqueue().CallLater(MUD_StartManager, 0, false, area);
	}

	//------------------------------------------------------------------------------------------------
	void MUD_AskCloseArea(int area)
	{
		if (Replication.IsServer())
			MUD_RpcAsk_CloseArea(area);
		else
			Rpc(MUD_RpcAsk_CloseArea, area);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_CloseArea(int area)
	{
		MUD_BuildAreaComponent a = MUD_BuildAreaComponent.Find(area);
		if (a)
			a.Unlock(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	void MUD_AskKeepArea(int area)
	{
		if (Replication.IsServer())
			MUD_RpcAsk_KeepArea(area);
		else
			Rpc(MUD_RpcAsk_KeepArea, area);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_KeepArea(int area)
	{
		MUD_BuildAreaComponent a = MUD_BuildAreaComponent.Find(area);
		if (a)
			a.KeepAlive(GetPlayerId());
	}

	//================================================================================================
	// parts

	//------------------------------------------------------------------------------------------------
	//! Server: area if this player holds its edit lock, else null (and the player is told)
	protected MUD_BuildAreaComponent MUD_EditArea(int area)
	{
		MUD_BuildAreaComponent a = MUD_BuildAreaComponent.Find(area);
		if (!a || !a.HoldsLock(GetPlayerId()))
		{
			Print(string.Format("MUD build part: refused, player %1 does not hold the edit lock of area %2", GetPlayerId(), area), LogLevel.WARNING);
			MUD_Tell("Not saved: the manager of this area is not yours (closed or timed out) - open it again");
			return null;
		}
		a.KeepAlive(GetPlayerId());
		return a;
	}

	//------------------------------------------------------------------------------------------------
	void MUD_AskSavePart(int area, int kind, string text)
	{
		if (Replication.IsServer())
			MUD_RpcAsk_SavePart(area, kind, text);
		else
			Rpc(MUD_RpcAsk_SavePart, area, kind, text);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_SavePart(int area, int kind, string text)
	{
		if (text.Length() > MUD_Build.MAX_TEXT)
		{
			Print(string.Format("MUD build part: refused, text %1 chars", text.Length()), LogLevel.WARNING);
			return;
		}
		MUD_BuildAreaComponent a = MUD_EditArea(area);
		if (!a)
			return;
		string why = MUD_BuildPartComponent.ServerSave(a, kind, text);
		if (!why.IsEmpty())
		{
			Print(string.Format("MUD build part: refused in area %1 (%2): %3", area, MUD_Build.KindName(kind), why), LogLevel.WARNING);
			MUD_Tell(MUD_Build.KindName(kind) + " not saved: " + why);
		}
	}

	//------------------------------------------------------------------------------------------------
	void MUD_AskDeletePart(int area, int uid)
	{
		if (Replication.IsServer())
			MUD_RpcAsk_DeletePart(area, uid);
		else
			Rpc(MUD_RpcAsk_DeletePart, area, uid);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_DeletePart(int area, int uid)
	{
		MUD_BuildAreaComponent a = MUD_EditArea(area);
		if (!a)
			return;
		string why = MUD_BuildPartComponent.ServerDelete(a, uid);
		if (!why.IsEmpty())
			MUD_Tell("Not deleted: " + why);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner client (Ctrl+F in the manager, test): dig every part of the area at once
	void MUD_AskInstantArea(int area)
	{
		if (Replication.IsServer())
			MUD_RpcAsk_InstantArea(area);
		else
			Rpc(MUD_RpcAsk_InstantArea, area);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_InstantArea(int area)
	{
		MUD_BuildAreaComponent a = MUD_EditArea(area);
		if (!a)
			return;
		if (!MUD_TrenchSiteComponent.ALLOW_INSTANT_DIG && !MUD_TrenchSiteComponent.DEBUG_INSTANT)
		{
			Print("MUD build area: instant dig refused (ALLOW_INSTANT_DIG false)", LogLevel.WARNING);
			MUD_Tell("Instant dig is off on this server - dig it with the shovel");
			return;
		}
		array<MUD_BuildPartComponent> parts = {};
		MUD_BuildPartComponent.ForArea(area, parts);
		int swings = 0;
		foreach (MUD_BuildPartComponent p : parts)
		{
			swings += p.DigAll();
		}
		Print(string.Format("MUD build area: %1 instant dig, %2 parts, %3 swings", area, parts.Count(), swings));
	}
}
