//! TEMP probe 2026-10-13 (MUD_VoxelProbe ground test, MP): a client asks the server for the voxel op log (from a seq; J1)
//! (late join / ops made before its ground was up), and a client's tunnel dig (debug T, or a shovel TUNNEL swing) goes
//! to the server.
//! The live ops come by broadcast (MUD_WorldGroundEntity.RpcDo_VoxelOp). Remove with MUD_VoxelProbe.c.
modded class SCR_PlayerController
{
	protected int m_iMudVoxelTunnelAt = -100000;
	protected int m_iMudVoxelJipAskAt = -100000;
	protected bool m_bMudDebugTLogged;      // 'debug T by player N' printed once
	protected float m_fMudShovelTunnelAt = -100000; // world time of the last shovel TUNNEL swing (U1 stage 2)

	//------------------------------------------------------------------------------------------------
	//! Owner client: every op from fromSeq on (first ask, and again after ops were lost - J1 2026-10-17)
	void MUD_RequestVoxelJip(int fromSeq)
	{
		if (!Replication.IsServer())
			Rpc(MUD_RpcAsk_VoxelJip, fromSeq);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one ask per second per player; the send itself is spread over frames (MUD_VoxelProbe.JipSendTick)
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_VoxelJip(int fromSeq)
	{
		int now = System.GetTickCount();
		if (now - m_iMudVoxelJipAskAt < 900)
			return;
		m_iMudVoxelJipAskAt = now;
		MUD_VoxelProbe.ServerJipAsk(GetPlayerId(), fromSeq);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one JIP message to this player (MUD_VoxelProbe.JipSendTick)
	void MUD_SendVoxelJip(string batch)
	{
		Rpc(MUD_RpcDo_VoxelJip, batch);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MUD_RpcDo_VoxelJip(string batch)
	{
		MUD_VoxelProbe.ReceiveBatch(batch);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner client: one tunnel dig (capsule middle mid) for the server to check and make
	void MUD_RequestVoxelTunnel(vector mid)
	{
		if (Replication.IsServer())
			MUD_RpcAsk_VoxelTunnel(mid);
		else
			Rpc(MUD_RpcAsk_VoxelTunnel, mid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: debug T (MUD_VoxelProbe.TUNNEL_DEBUG_T, no tool): on foot, the capsule near and not too steep, no faster
	//! than the client's own repeat
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_VoxelTunnel(vector mid)
	{
		// rate first and stamped on a refusal too: a client sending every frame can't flood the log
		int now = System.GetTickCount();
		if (now - m_iMudVoxelTunnelAt < MUD_VoxelProbe.TUNNEL_EVERY_MS - 50)
			return;
		m_iMudVoxelTunnelAt = now;
		if (!MUD_VoxelProbe.TUNNEL_DEBUG_T)
		{
			Print(string.Format("MUD tunnel: debug T refused for player %1 (TUNNEL_DEBUG_T false)", GetPlayerId()), LogLevel.WARNING);
			return;
		}
		ChimeraCharacter c = ChimeraCharacter.Cast(GetControlledEntity());
		if (!c || c.IsInVehicle())
			return;
		if (!MUD_TunnelAimOk(c, mid, "debug T"))
			return;
		if (!m_bMudDebugTLogged)
		{
			m_bMudDebugTLogged = true;
			Print(string.Format("MUD tunnel: debug T by player %1", GetPlayerId()), LogLevel.NORMAL);
		}
		MUD_VoxelProbe.ServerTunnel(mid);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner client: one shovel TUNNEL swing (MUD_DigAction in MUD_Dig.MODE_TUNNEL, capsule middle mid)
	void MUD_RequestShovelTunnel(vector mid)
	{
		if (Replication.IsServer())
			MUD_RpcAsk_ShovelTunnel(mid);
		else
			Rpc(MUD_RpcAsk_ShovelTunnel, mid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server (U1 stage 2, user 2026-10-16: tunnels need a shovel): on foot, a shovel in hand, the capsule near and not
	//! too steep, one swing per MUD_Dig.SHOVEL_TUNNEL_MS. The same tunnel op as T (MP / JIP / hashes unchanged).
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_ShovelTunnel(vector mid)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(GetControlledEntity());
		if (!c || c.IsInVehicle())
			return;
		// rate first and stamped on a refusal too (no log flood)
		float now = GetGame().GetWorld().GetWorldTime();
		if (now - m_fMudShovelTunnelAt < MUD_Dig.SHOVEL_TUNNEL_MS)
			return;
		m_fMudShovelTunnelAt = now;
		if (!MUD_Dig.HoldsShovel(c, true))
		{
			Print(string.Format("MUD tunnel: shovel swing refused, server sees no entrenching tool in hand (player %1)", GetPlayerId()), LogLevel.WARNING);
			return;
		}
		if (!MUD_TunnelAimOk(c, mid, "shovel swing"))
			return;
		MUD_VoxelProbe.ServerTunnel(mid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, both tunnel RPCs (U1 stage 2): the capsule middle within 3 m of c's feet, ahead of them (no shaft in
	//! place, review 2026-10-17: the client's 'dig ahead' alone can be skipped) and no steeper down than
	//! MUD_VoxelProbe.TunnelDownOk. Feet on our ground first (WB 2026-10-17: T 2 s after start dug 24-31 m under the
	//! ground, the character not placed yet). Logs a refusal (what = which path asked).
	protected bool MUD_TunnelAimOk(notnull ChimeraCharacter c, vector mid, string what)
	{
		vector feet = c.GetOrigin();
		if (!MUD_WorldGroundEntity.AtGround(feet))
		{
			Print(string.Format("MUD tunnel: %1 refused, player %2 at %3 not on the ground", what, GetPlayerId(), feet), LogLevel.WARNING);
			return false;
		}
		if (vector.Distance(mid, feet) > 3)
		{
			Print(string.Format("MUD tunnel: %1 refused, %2 too far from player %3 at %4", what, mid, GetPlayerId(), feet), LogLevel.WARNING);
			return false;
		}
		float ahead = MUD_VoxelProbe.TUNNEL_AHEAD - MUD_VoxelProbe.TUNNEL_LAG;
		if (vector.DistanceSqXZ(mid, feet) < ahead * ahead)
		{
			Print(string.Format("MUD tunnel: %1 refused, %2 not ahead of player %3 at %4 (in place)", what, mid, GetPlayerId(), feet), LogLevel.WARNING);
			return false;
		}
		if (!MUD_VoxelProbe.TunnelDownOk(mid, feet))
		{
			Print(string.Format("MUD tunnel: %1 refused, %2 too steep down from player %3 at %4", what, mid, GetPlayerId(), feet), LogLevel.WARNING);
			return false;
		}
		return true;
	}
}
