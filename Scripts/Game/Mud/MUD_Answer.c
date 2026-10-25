//! Audit E2 / E3 / E4 (2026-10-22): the server tells the asking player why a trench plan, a free dig / fill swing or a
//! tunnel swing did nothing. Before, each refusal was a server log line only - the plan vanished, the swing did nothing.
//! One owner RPC on the player controller (same path as MUD_RpcDo_VoxelJip); the server log lines stay as they were.
//! File name starts with "MUD_Answer" on purpose: it must sort before MUD_Dig.c. A BARE call (no "pc.") inside a
//! modded class can not see a method that a later-sorting file adds to the same class (WB compile 2026-10-22:
//! MUD_Dig.c "Undefined function MUD_Tell" x10 while it sat in MUD_Hint.c). Tools/enforce_lint.py checks this.
modded class SCR_PlayerController
{
	static const int MUD_HINT_REPEAT_MS = 3000;    // the same text again within this is dropped (swings repeat fast)
	protected int m_iMudHintAt = -100000;
	protected string m_sMudHintLast;

	//------------------------------------------------------------------------------------------------
	//! Server: show text as a hint on this player's screen only
	void MUD_Tell(string text)
	{
		if (!Replication.IsServer())
			return;
		int now = System.GetTickCount();
		if (text == m_sMudHintLast && now - m_iMudHintAt < MUD_HINT_REPEAT_MS)
			return;
		m_sMudHintLast = text;
		m_iMudHintAt = now;
		if (GetPlayerId() == SCR_PlayerController.GetLocalPlayerId())
			MUD_RpcDo_Tell(text);     // host / SP player: an owner RPC to ourselves would not arrive
		else
			Rpc(MUD_RpcDo_Tell, text);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MUD_RpcDo_Tell(string text)
	{
		if (!System.IsConsoleApp())
			SCR_HintManagerComponent.ShowCustomHint(text, "Mud", 4, true);
	}
}
