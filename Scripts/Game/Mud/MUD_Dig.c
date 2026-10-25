//! Shovel digging on mud ground. Same idea as the shipped foliage cutting (SCR_CampaignBuildingGadgetToolComponent
//! moves Prefabs/Systems/Destruction/FoliageCutHelper.et to the bush, the helper carries the action):
//! while the entrenching tool is in hand and the spot in front of the soldier is mud, a LOCAL helper
//! entity (MudDigHelper.et) sits there and offers a looping "Dig" action. Each loop plays the shipped
//! shovel animation (CMD_Item_Action arg 1, as SCR_CampaignBuildingBuildUserAction) and asks the server
//! for one swing. On the world ground the swing digs the 'free dig' square the player sees as a green
//! ghost (one trench piece in front, MUD_Dig.FreePlan) one 0.25 m layer deeper.
//! Over an unfinished trench piece (MUD_TrenchSiteComponent) the same action reads 'Dig trench' and
//! the server digs one layer of that piece instead.

//------------------------------------------------------------------------------------------------
class MUD_Dig
{
	static const ResourceName HELPER = "{01EE55B2D1E71522}Prefabs/Mud/MudDigHelper.et";
	static const float REACH = 1.0;         // dig this far in front of the soldier's feet (m)
	static const float MIN_GAP_MS = 1000;   // server: at most one swing per player this often
	static const float SHOVEL_TUNNEL_MS = 1000; // server: at most one TUNNEL swing per player this often (U1 stage 2; tune in 1-WB)
	// free dig square: one trench section, 1 x 1 m (user 2026-10-10 00:30; the sloped walls reach past
	// it), middle FREE_REACH in front, snapped to the 0.25 m grid, turned in 45 deg steps, so squares
	// dug next to each other line up (user 00:21)
	static const float FREE_LENGTH = 1.0;   // m along the (snapped) facing
	static const int FREE_WIDTH = 1;        // MUD_TrenchPlan.WidthOf index: 1.0 m (the plan default)
	static const float FREE_REACH = 0.75;   // m from the feet to the square's middle (near edge 0.25 m ahead)
	static const float FREE_RANGE = 2.5;    // server: square middle at most this far from the feet (m)

	// shovel modes, R cycles them (user 2026-10-10 00:30, like the sandbag mode): OFF = no ghost, no
	// free dig; DIG = green square, 'Dig' sinks it a layer; FILL = square, 'Fill' raises it a layer
	// (user 00:45, was all at once); TUNNEL = each swing digs one tunnel capsule where the player looks (U1 stage 2,
	// user 2026-10-16: tunnels need a shovel; MUD_VoxelProbe.TunnelAim); PLAN = trench plan mode (MUD_TrenchPlanner);
	// STAKE = trench builder build areas (MUD_BuildStake, F4 2026-10-24: stake 4 corners, F opens the manager)
	static const int MODE_OFF = 0;
	static const int MODE_DIG = 1;
	static const int MODE_FILL = 2;
	static const int MODE_TUNNEL = 3;
	static const int MODE_PLAN = 4;
	static const int MODE_STAKE = 5;
	static const int MODE_COUNT = 6;
	protected static int s_iMode;

	protected static IEntity s_Helper;
	protected static ref MUD_TrenchGhosts s_FreeGhost;
	protected static string s_sFreeShown;   // plan text of the ghost now drawn, "" = none

	//------------------------------------------------------------------------------------------------
	static int Mode()
	{
		return s_iMode;
	}

	//------------------------------------------------------------------------------------------------
	//! Local: R with the shovel in hand: off -> Dig -> Fill -> Tunnel -> Plan -> Stake -> off
	static void NextMode()
	{
		SetMode((s_iMode + 1) % MODE_COUNT);
	}

	//------------------------------------------------------------------------------------------------
	static void SetMode(int mode)
	{
		if (mode == s_iMode)
			return;
		int was = s_iMode;
		s_iMode = mode;
		HideFreeGhost();
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		// leaving Plan: the planner stops (it calls SetMode(OFF) itself when F / no shovel ends it)
		if (was == MODE_PLAN && pc)
			pc.MUD_StopTrenchPlanning();
		// leaving Stake: the stake mode stops (it calls SetMode(OFF) itself when the shovel goes / the manager opens)
		if (was == MODE_STAKE && pc)
			pc.MUD_StopStake();
		if (mode == MODE_STAKE)
		{
			RemoveHelper();
			if (pc)
				pc.MUD_StartStake();
			if (!pc || !pc.MUD_IsStaking())
				s_iMode = MODE_OFF;
			return;
		}
		if (mode == MODE_PLAN)
		{
			RemoveHelper();
			if (pc)
				pc.MUD_StartTrenchPlanning();
			if (!pc || !pc.MUD_IsTrenchPlanning())
				s_iMode = MODE_OFF;
			return;
		}
		if (mode == MODE_DIG)
			SCR_HintManagerComponent.ShowCustomHint("Shovel: DIG\nThe green square sinks 0.25 m per swing (max 2.5 m).\nR: fill mode", "Free dig", 4, true);
		else if (mode == MODE_FILL)
			SCR_HintManagerComponent.ShowCustomHint("Shovel: FILL\nThe square rises 0.25 m per swing.\nR: tunnel mode", "Free dig", 4, true);
		else if (mode == MODE_TUNNEL)
			SCR_HintManagerComponent.ShowCustomHint("Shovel: TUNNEL\nEach swing digs ahead where you look (look down / up for a ramp).\nSlope " + MUD_VoxelProbe.TunnelSteepText() + "\nX: change slope. R: plan a trench", "Free dig", 5, true);
		else
			SCR_HintManagerComponent.ShowCustomHint("Shovel: off\nR: dig mode", "Free dig", 2, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Flat facing of c
	static vector Facing(notnull IEntity c)
	{
		vector m[4];
		c.GetWorldTransform(m);
		vector fwd = m[2];
		fwd[1] = 0;
		fwd.Normalize();
		return fwd;
	}

	//------------------------------------------------------------------------------------------------
	//! Free dig (user 2026-10-10 00:04, replaces the round hole): one FREE_LENGTH trench piece in front of c,
	//! facing snapped to 45 deg, middle snapped to the ground grid. Packed MUD_TrenchPlan text (what the
	//! server gets).
	static string FreePlan(notnull IEntity c)
	{
		vector f = Facing(c);
		float step = Math.PI * 0.25;
		float yaw = Math.Round(Math.Atan2(f[0], f[2]) / step) * step;
		vector fwd = Vector(Math.Sin(yaw), 0, Math.Cos(yaw));
		vector mid = MUD_WorldGroundEntity.SnapFine(c.GetOrigin() + f * FREE_REACH);
		MUD_TrenchPlan plan = new MUD_TrenchPlan();
		plan.m_iWidth = FREE_WIDTH;
		plan.m_iLayers = MUD_TrenchPlan.MAX_LAYERS;
		plan.AddPoint(mid - fwd * (FREE_LENGTH * 0.5));
		plan.AddPoint(mid + fwd * (FREE_LENGTH * 0.5));
		return plan.Pack();
	}

	//------------------------------------------------------------------------------------------------
	//! Local: green ghost of the free dig square, hidden over an unfinished trench piece (its own ghost
	//! shows) or off the world ground. Redrawn when the snapped square changes, or after a swing (redraw).
	static void UpdateFreeGhost(ChimeraCharacter c, bool redraw = false)
	{
		vector dp = DigPoint(c);
		MUD_TrenchSiteComponent site;
		int piece;
		if (s_iMode == MODE_OFF || s_iMode == MODE_PLAN || s_iMode == MODE_STAKE || s_iMode == MODE_TUNNEL || !MUD_WorldGroundEntity.IsOver(dp) || MUD_TrenchSiteComponent.FindPiece(dp, site, piece) || OverBuildPiece(c, dp))
		{
			HideFreeGhost();
			return;
		}
		string packed = FreePlan(c);
		if (!redraw && packed == s_sFreeShown)
			return;

		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!lay.Build(MUD_TrenchPlan.Unpack(packed)))
		{
			HideFreeGhost();
			return;
		}
		if (!s_FreeGhost)
			s_FreeGhost = new MUD_TrenchGhosts();
		// fill: the 'dug' ghost colour, so the two modes are told apart at a glance
		int state = MUD_TrenchGhosts.GREEN;
		if (s_iMode == MODE_FILL)
			state = MUD_TrenchGhosts.DUG;
		array<int> states = {state};
		s_FreeGhost.Draw(lay, states);
		s_sFreeShown = packed;
	}

	//------------------------------------------------------------------------------------------------
	//! Trench builder F4: an unfinished builder piece is in front of c (a trench piece at dp, or a capsule it can dig)
	static bool OverBuildPiece(notnull IEntity c, vector dp)
	{
		MUD_BuildPartComponent part;
		int piece;
		if (MUD_BuildPartComponent.FindTrenchPiece(dp, part, piece))
			return true;
		return MUD_BuildPartComponent.FindCapsule(c.GetOrigin(), dp, part, piece);
	}

	//------------------------------------------------------------------------------------------------
	static void HideFreeGhost()
	{
		if (s_FreeGhost)
			s_FreeGhost.Clear();
		s_sFreeShown = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Where character c digs: REACH in front of its feet, flat
	static vector DigPoint(notnull IEntity c)
	{
		vector m[4];
		c.GetWorldTransform(m);
		vector fwd = m[2];
		fwd[1] = 0;
		fwd.Normalize();
		return m[3] + fwd * REACH;
	}

	//------------------------------------------------------------------------------------------------
	//! True if c holds an entrenching tool in hand (and, with notPlacing, is not placing an item with it)
	static bool HoldsShovel(IEntity c, bool notPlacing)
	{
		SCR_GadgetManagerComponent gm = SCR_GadgetManagerComponent.GetGadgetManager(c);
		if (!gm)
			return false;

		SCR_CampaignBuildingGadgetToolComponent tool = SCR_CampaignBuildingGadgetToolComponent.Cast(gm.GetHeldGadgetComponent());
		if (!tool || tool.GetMode() != EGadgetMode.IN_HAND)
			return false;

		return !notPlacing || !tool.GetIsPlacementModeEnabled();
	}

	//------------------------------------------------------------------------------------------------
	//! Local player only: keep the helper in front of c while there is something to do there, else
	//! remove it. Old mud ground: always. World ground: an unfinished trench piece ('Dig trench', not
	//! in fill mode), or the free square in dig / fill mode.
	static void UpdateHelper(ChimeraCharacter c)
	{
		if (!c || c != SCR_PlayerController.GetLocalControlledEntity() || c.IsInVehicle())
		{
			RemoveHelper();
			return;
		}

		// plan / stake mode / builder manager open: no helper (F confirms the plan / stakes the area / saves there)
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (s_iMode == MODE_PLAN || s_iMode == MODE_STAKE || (pc && pc.MUD_IsManaging()))
		{
			RemoveHelper();
			return;
		}

		vector wp = DigPoint(c);
		bool wanted = false;
		if (!wanted && MUD_WorldGroundEntity.IsOver(wp))
		{
			MUD_TrenchSiteComponent site;
			int piece;
			if (MUD_TrenchSiteComponent.FindPiece(wp, site, piece) || (s_iMode != MODE_TUNNEL && OverBuildPiece(c, wp)))
				wanted = s_iMode != MODE_FILL;
			else
				wanted = s_iMode != MODE_OFF;
		}
		if (!wanted)
		{
			RemoveHelper();
			return;
		}

		if (!s_Helper)
		{
			Resource res = Resource.Load(HELPER);
			if (!res || !res.IsValid())
				return;
			s_Helper = GetGame().SpawnEntityPrefabLocal(res, c.GetWorld(), null);
			if (!s_Helper)
				return;
		}

		s_Helper.SetOrigin(wp);
		s_Helper.Update();
		UpdateFreeGhost(c);
	}

	//------------------------------------------------------------------------------------------------
	static void RemoveHelper()
	{
		if (s_Helper)
			SCR_EntityHelper.DeleteEntityAndChildren(s_Helper);
		s_Helper = null;
		HideFreeGhost();
	}
}

//------------------------------------------------------------------------------------------------
//! Looping "Dig" on MudDigHelper.et: one server swing per loop (Duration < 0 in the prefab)
class MUD_DigAction : SCR_ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(user);
		if (!c || c.IsInVehicle() || !MUD_Dig.HoldsShovel(c, true))
			return false;

		return super.CanBeShownScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(user);
		if (!c)
			return false;

		CharacterControllerComponent cc = c.GetCharacterController();
		if (cc && !IsInProgress() && !cc.CanUseItem())
			return false;

		return MUD_Dig.HoldsShovel(c, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Tunnel mode: 'Dig tunnel' (anywhere); over an unfinished trench piece: 'Dig trench (layers dug / layers)';
	//! fill mode: 'Fill'
	override bool GetActionNameScript(out string outName)
	{
		if (MUD_Dig.Mode() == MUD_Dig.MODE_TUNNEL)
		{
			outName = "Dig tunnel";
			return true;
		}
		MUD_TrenchSiteComponent site;
		int piece;
		if (MUD_TrenchSiteComponent.FindPiece(GetOwner().GetOrigin(), site, piece))
		{
			outName = string.Format("Dig trench (%1 / %2)", site.Digit(piece), site.Layers());
			return true;
		}
		// trench builder F4: a planned trench / step piece, or the next capsule of a tunnel / roof / dugout
		MUD_BuildPartComponent part;
		if (MUD_Dig.Mode() != MUD_Dig.MODE_FILL && MUD_BuildPartComponent.FindTrenchPiece(GetOwner().GetOrigin(), part, piece))
		{
			outName = string.Format("Dig %1 (%2 / %3)", MUD_Build.KindName(part.Kind()), part.Digit(piece), part.Layers());
			return true;
		}
		IEntity me = SCR_PlayerController.GetLocalControlledEntity();
		if (me && MUD_Dig.Mode() != MUD_Dig.MODE_FILL && MUD_BuildPartComponent.FindCapsule(me.GetOrigin(), GetOwner().GetOrigin(), part, piece))
		{
			int mine;
			int done;
			part.Counts(mine, done);
			outName = string.Format("Dig %1 (%2 / %3)", MUD_Build.KindName(part.Kind()), done, mine);
			return true;
		}
		if (MUD_Dig.Mode() == MUD_Dig.MODE_FILL)
		{
			outName = "Fill";
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionStart(IEntity pUserEntity)
	{
		super.OnActionStart(pUserEntity);

		ChimeraCharacter c = ChimeraCharacter.Cast(pUserEntity);
		if (!c)
			return;
		CharacterControllerComponent cc = c.GetCharacterController();
		SCR_GadgetManagerComponent gm = SCR_GadgetManagerComponent.GetGadgetManager(c);
		if (!cc || !gm)
			return;

		// shipped shovel loop, same parameters as SCR_CampaignBuildingBuildUserAction.OnActionStart
		ItemUseParameters params = new ItemUseParameters();
		params.SetEntity(gm.GetHeldGadget());
		params.SetAllowMovementDuringAction(false);
		params.SetKeepInHandAfterSuccess(true);
		params.SetCommandID(cc.GetAnimationComponent().BindCommand("CMD_Item_Action"));
		params.SetCommandIntArg(1);
		cc.TryUseItemOverrideParams(params);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformContinuousAction(IEntity pOwnerEntity, IEntity pUserEntity, float timeSlice)
	{
		if (LoopActionUpdate(timeSlice))
			PerformAction(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity())
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		// tunnel mode (U1 stage 2): one capsule where the player looks, the server checks the tool and the aim
		if (MUD_Dig.Mode() == MUD_Dig.MODE_TUNNEL)
		{
			vector mid;
			if (pc && MUD_VoxelProbe.TunnelAimLocal(pUserEntity, mid))
				pc.MUD_RequestShovelTunnel(mid);
			return;
		}
		if (pc)
			pc.MUD_RequestDig(MUD_Dig.FreePlan(pUserEntity), MUD_Dig.Mode() == MUD_Dig.MODE_FILL);
		// the ground under the ghost just changed: lay it on the new floor
		ChimeraCharacter c = ChimeraCharacter.Cast(pUserEntity);
		if (c)
			MUD_Dig.UpdateFreeGhost(c, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionCanceled(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		super.OnActionCanceled(pOwnerEntity, pUserEntity);
		StopAnimation(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override void OnConfirmed(IEntity pUserEntity)
	{
		StopAnimation(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void StopAnimation(IEntity user)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(user);
		if (!c)
			return;
		CharacterControllerComponent cc = c.GetCharacterController();
		if (!cc || !cc.GetAnimationComponent())
			return;
		CharacterCommandHandlerComponent handler = cc.GetAnimationComponent().GetCommandHandler();
		if (handler)
			handler.FinishItemUse(true);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_CampaignBuildingGadgetToolComponent
{
	//------------------------------------------------------------------------------------------------
	//! The helper, ghost and mode are the LOCAL player's: another character's shovel on this machine
	//! (host, AI) must not touch them (review 2026-10-10: it removed the local helper every frame)
	override void Update(float timeSlice)
	{
		super.Update(timeSlice);

		if (!m_CharacterOwner || m_CharacterOwner != SCR_PlayerController.GetLocalControlledEntity())
			return;
		if (MUD_Dig.HoldsShovel(m_CharacterOwner, true))
		{
			MUD_Dig.UpdateHelper(m_CharacterOwner);
			if (MUD_Dig.Mode() == MUD_Dig.MODE_TUNNEL)
				MUD_VoxelProbe.TunnelSteepInput();
		}
		else
		{
			MUD_Dig.RemoveHelper();
			MUD_Dig.SetMode(MUD_Dig.MODE_OFF);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void ToolToInventory()
	{
		// owner read before super: it may clear m_CharacterOwner
		IEntity owner = m_CharacterOwner;
		super.ToolToInventory();
		if (!owner || owner != SCR_PlayerController.GetLocalControlledEntity())
			return;
		MUD_Dig.RemoveHelper();
		MUD_Dig.SetMode(MUD_Dig.MODE_OFF);
	}

	//------------------------------------------------------------------------------------------------
	//! R (vanilla "GadgetActivate", as CSK_BuildingGadgetTool.c): cycles the shovel modes off -> Dig ->
	//! Fill -> Tunnel (U1 2026-10-17) -> Plan -> off (user 2026-10-10 00:30). Vanilla deployable placement keeps the key while it is on,
	//! or when the player carries something it can place. (CSK mods the same method for its wall plan
	//! mode: with both mods loaded the one loaded last wins.)
	override protected void ActivateAction()
	{
		if (m_bPlacementModeEnabled || FindDeployableItems(null, true) > 0)
		{
			MUD_Dig.SetMode(MUD_Dig.MODE_OFF);
			super.ActivateAction();
			return;
		}

		MUD_Dig.NextMode();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	protected float m_fMudLastDig = -100000;

	//------------------------------------------------------------------------------------------------
	//! Owner client: ask the server for one shovel swing in front of the controlled character.
	//! freePlan = the free dig square the player sees (MUD_Dig.FreePlan), used off trench pieces.
	void MUD_RequestDig(string freePlan, bool fill)
	{
		if (Replication.IsServer())
			MUD_RpcAsk_Dig(freePlan, fill);
		else
			Rpc(MUD_RpcAsk_Dig, freePlan, fill);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_Dig(string freePlan, bool fill)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(GetControlledEntity());
		if (!c || c.IsInVehicle())
			return;

		float now = GetGame().GetWorld().GetWorldTime();
		if (now - m_fMudLastDig < MUD_Dig.MIN_GAP_MS)
			return;

		if (!MUD_Dig.HoldsShovel(c, false))
		{
			Print("MUD dig: refused, server sees no entrenching tool in hand", LogLevel.WARNING);
			MUD_Tell("Dig: hold an entrenching tool");
			return;
		}

		m_fMudLastDig = now;
		vector dp = MUD_Dig.DigPoint(c);

		// an unfinished trench piece there takes the swing (one layer of that piece)
		MUD_TrenchSiteComponent site;
		int piece;
		if (MUD_TrenchSiteComponent.FindPiece(dp, site, piece))
		{
			// the site counts its layers: filling under it would leave it out of step
			if (fill)
			{
				Print(string.Format("MUD fill: refused at %1, unfinished trench piece there", dp));
				MUD_Tell("Fill: not on an unfinished trench - finish or leave it");
			}
			else if (!MUD_WorldGroundEntity.AtGround(dp))
			{
				Print(string.Format("MUD trench dig: refused at %1, soldier not on the ground there", dp));
				MUD_Tell("Dig: stand on the ground to dig");
			}
			else if (!site.DigPiece(piece))
				Print(string.Format("MUD trench dig: swing not counted at %1 (patch building)", dp));
			return;
		}

		// trench builder F4: a planned trench / step piece there, or the next capsule of a tunnel / roof / dugout
		MUD_BuildPartComponent part;
		if (MUD_BuildPartComponent.FindTrenchPiece(dp, part, piece))
		{
			if (fill)
			{
				Print(string.Format("MUD fill: refused at %1, unfinished builder piece there", dp));
				MUD_Tell("Fill: not on an unfinished planned trench - finish it or delete it in the manager");
			}
			else if (!MUD_WorldGroundEntity.AtGround(dp))
			{
				Print(string.Format("MUD build dig: refused at %1, soldier not on the ground there", dp));
				MUD_Tell("Dig: stand on the ground to dig");
			}
			else if (!part.DigPiece(piece))
				Print(string.Format("MUD build dig: swing not counted at %1 (patch building)", dp));
			return;
		}
		vector feet = c.GetOrigin();
		if (!fill && MUD_BuildPartComponent.FindCapsule(feet, dp, part, piece))
		{
			if (!MUD_WorldGroundEntity.AtGround(feet))
			{
				Print(string.Format("MUD build dig: capsule refused, player at %1 not on the ground", feet));
				MUD_Tell("Dig: stand on the ground to dig");
			}
			else if (!part.DigPiece(piece))
				Print(string.Format("MUD build dig: capsule swing not counted at %1", dp));
			return;
		}

		// world ground: the free dig square (the round hole is gone, user 2026-10-10 00:04)
		if (MUD_WorldGroundEntity.IsOver(dp))
		{
			MUD_FreeDig(c, freePlan, fill);
			return;
		}

		// (the old per-area MUD_GroundEntity round hole is archived, audit C6 2026-10-22)
		if (fill)
		{
			Print(string.Format("MUD fill: nothing to fill at %1 (not on the world ground)", dp));
			MUD_Tell("Fill: nothing to fill here");
		}
		else
		{
			Print(string.Format("MUD dig: nothing dug at %1 (not on the world ground)", dp));
			MUD_Tell("Dig: this ground can not be dug");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: check the client's free dig square (one piece, FREE_LENGTH long, near the player, on the
	//! ground the player stands on), then dig it one layer deeper, or (fill) fill it back to the ground
	protected void MUD_FreeDig(ChimeraCharacter c, string freePlan, bool fill)
	{
		MUD_TrenchPlan plan;
		if (freePlan.Length() <= 60)
			plan = MUD_TrenchPlan.Unpack(freePlan);
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!plan || plan.m_iWidth != MUD_Dig.FREE_WIDTH || plan.m_aPoints.Count() != 2 || !lay.Build(plan) || lay.SegCount() != 1 || Math.AbsFloat(lay.SegLength(0) - MUD_Dig.FREE_LENGTH) > 0.05)
		{
			Print("MUD free dig: refused, bad square " + freePlan, LogLevel.WARNING);
			return;
		}
		vector feet = c.GetOrigin();
		vector mid = (plan.m_aPoints[0] + plan.m_aPoints[1]) * 0.5;
		if (vector.DistanceSqXZ(mid, feet) > MUD_Dig.FREE_RANGE * MUD_Dig.FREE_RANGE)
		{
			Print(string.Format("MUD free dig: refused, square at %1 too far from the player %2", mid, feet), LogLevel.WARNING);
			MUD_Tell("Dig: too far away");
			return;
		}
		mid[1] = feet[1];
		if (!MUD_WorldGroundEntity.AtGround(mid))
		{
			Print(string.Format("MUD free dig: refused at %1, soldier not on the ground there", mid));
			MUD_Tell("Dig: stand on the ground to dig");
			return;
		}

		if (fill)
		{
			int f = MUD_WorldGroundEntity.FreeFill(lay);
			if (f == -1)
				Print(string.Format("MUD fill: swing not counted at %1 (patch building)", mid));
			else if (f == -2)
			{
				Print(string.Format("MUD fill: nothing to fill at %1 (not dug)", mid));
				MUD_Tell("Fill: nothing dug here");
			}
			return;
		}

		int r = MUD_WorldGroundEntity.FreeDig(lay);
		if (r == -1)
			Print(string.Format("MUD free dig: swing not counted at %1 (patch building)", mid));
		else if (r == -2)
		{
			Print(string.Format("MUD free dig: nothing diggable at %1 (hard / locked ground)", mid));
			MUD_Tell("Dig: hard ground (road, rock, building)");
		}
		else if (r == -3)
		{
			Print(string.Format("MUD free dig: at %1 already %2 m deep", mid, MUD_TrenchPlan.MAX_LAYERS * MUD_TrenchPlan.LAYER));
			MUD_Tell(string.Format("Dig: already %1 m deep (the most)", MUD_TrenchPlan.MAX_LAYERS * MUD_TrenchPlan.LAYER));
		}
	}
}
