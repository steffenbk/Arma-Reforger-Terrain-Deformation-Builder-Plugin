//! TEMP probe 2026-10-13 (MUD_VoxelProbe ground test, MP): a client asks the server for the voxel op log once
//! (late join / ops made before its ground was up), and a client's tunnel dig (hold T) goes to the server.
//! The live ops come by broadcast (MUD_WorldGroundEntity.RpcDo_VoxelOp). Remove with MUD_VoxelProbe.c.
modded class SCR_PlayerController
{
	protected int m_iMudVoxelTunnelAt = -100000;
	protected bool m_bMudVoxelJipSent;

	//------------------------------------------------------------------------------------------------
	//! Owner client
	void MUD_RequestVoxelJip()
	{
		if (!Replication.IsServer())
			Rpc(MUD_RpcAsk_VoxelJip);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_VoxelJip()
	{
		if (m_bMudVoxelJipSent)
			return;     // once per controller: the log can be large
		m_bMudVoxelJipSent = true;
		array<string> batches = {};
		int ops = MUD_VoxelProbe.JipBatches(batches);
		foreach (string b : batches)
			Rpc(MUD_RpcDo_VoxelJip, b);
		Print(string.Format("MUD voxel probe: player %1 asked for the op log - %2 ops in %3 messages", GetPlayerId(), ops, batches.Count()), LogLevel.NORMAL);
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
	//! Server: on foot, the capsule within 3 m of the feet, no faster than the client's own repeat
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_VoxelTunnel(vector mid)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(GetControlledEntity());
		if (!c || c.IsInVehicle())
			return;
		int now = System.GetTickCount();
		if (now - m_iMudVoxelTunnelAt < MUD_VoxelProbe.TUNNEL_EVERY_MS - 50)
			return;
		if (vector.Distance(mid, c.GetOrigin()) > 3)
		{
			Print(string.Format("MUD voxel probe: tunnel dig refused, %1 too far from player %2 at %3", mid, GetPlayerId(), c.GetOrigin()), LogLevel.WARNING);
			return;
		}
		m_iMudVoxelTunnelAt = now;
		MUD_VoxelProbe.ServerTunnel(mid);
	}
}
