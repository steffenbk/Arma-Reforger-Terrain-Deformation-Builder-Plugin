//! Mud plan 3 JIP: a joining client asks the server for the deformed patches (keys came in
//! MUD_WorldGroundEntity.RplLoad); the server sends each patch's full heights to that player only
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Owner client
	void MUD_RequestJip()
	{
		if (Replication.IsServer())
			MUD_RpcAsk_Jip();
		else
			Rpc(MUD_RpcAsk_Jip);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_Jip()
	{
		MUD_WorldGroundEntity g = MUD_WorldGroundEntity.GetInstance();
		if (g)
			g.QueueJip(GetPlayerId(), GetControlledEntity());
	}

	//------------------------------------------------------------------------------------------------
	//! Server, send by distance: one live ground change to this player only
	void MUD_SendLive(int key, int seq, int rect, array<int> packed)
	{
		Rpc(MUD_RpcDo_Live, key, seq, rect, packed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MUD_RpcDo_Live(int key, int seq, int rect, array<int> packed)
	{
		MUD_WorldGroundEntity g = MUD_WorldGroundEntity.GetInstance();
		if (!packed)
			return;
		if (g)
			g.OnLivePc(key, seq, rect, packed);
		else
			MUD_WorldGroundEntity.HoldEarly(key, seq, rect, packed);
	}

	//------------------------------------------------------------------------------------------------
	//! Server
	void MUD_SendFull(int key, int seq, array<int> packed)
	{
		Rpc(MUD_RpcDo_Full, key, seq, packed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MUD_RpcDo_Full(int key, int seq, array<int> packed)
	{
		MUD_WorldGroundEntity g = MUD_WorldGroundEntity.GetInstance();
		if (g && packed)
			g.ApplyFull(key, seq, packed);
	}
}
