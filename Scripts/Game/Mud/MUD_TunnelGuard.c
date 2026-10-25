//! Tunnel guard (2026-10-22, user: "I can easily squeeze through tight gaps in tunnels and fall through the map, or
//! vault onto a ledge and end up outside the map"). NOT COMPILED YET.
//! Runs on the machine that drives the character (character movement is owner-side), so a rescue is an owner
//! teleport the way SCR_Global.TeleportPlayer does it - no net message of ours. A DS has no local character: nothing.
//!  - Lost: feet far under the deepest legal ground (converted column: TUNNEL_FLOOR under the undug ground;
//!    heightfield: under the dug ground top), or chest AND head inside solid voxel ground for STUCK_S.
//!  - Rescue: back to the last safe spot (on the ground, chest in open air, SAFE_HOLD ticks in a row), else onto
//!    our ground top (MUD_Surface trace, as MUD_VehicleAccess.Rescue).
//!  - Test kit: every rescue prints 'MUD tunnel guard: RESCUE ...' with where and why, so a test shows the gaps.
//! Climb guard (modded SCR_CharacterCommandHandlerComponent below): underground in a converted column, a climb /
//! vault whose landing spot is not open air is refused - the jump then does nothing.
class MUD_TunnelGuard
{
	static const bool ENABLED = true;
	static const int EVERY_MS = 250;
	static const float LOST_BELOW_M = 2.5;   // m under the deepest legal ground = fallen out of the world
	static const float STUCK_D = 0.15;       // m into solid at chest and head = inside the wall
	static const float STUCK_S = 1.0;        // s inside the wall before a rescue (a squeeze past a corner is shorter)
	static const int SAFE_HOLD = 2;          // ticks a spot must stay safe before it is the rescue spot
	static const float SAFE_OPEN_D = -0.1;   // chest field at most this (open air) for a safe spot
	static const float KNEE_M = 0.5;
	static const float CHEST_M = 1.0;
	static const float HEAD_M = 1.6;
	static const float RESCUE_UP_M = 0.2;
	static const float SAFE_MAX_M = 30;      // the last safe spot counts only this close (XZ) to where the fall is
	static const int RESCUE_GAP_MS = 2000;   // at most one rescue per this
	static const bool CLIMB_GUARD = true;
	static const float CLIMB_UNDER_M = 1.0;  // feet this far under the undug ground in a converted column = underground
	static const float CLIMB_OPEN_D = -0.05; // landing points must be at least this open

	protected static int s_iLast;
	protected static int s_iLastRescue;
	protected static IEntity s_Char;         // whose safe spot this is
	protected static vector s_vSafe;
	protected static bool s_bSafe;
	protected static int s_iSafeN;
	protected static float s_fStuck;
	protected static int s_iRescues;

	//------------------------------------------------------------------------------------------------
	//! Every frame from MUD_WorldGroundEntity (after MUD_VoxelProbe.Tick); does its work every EVERY_MS
	static void Tick()
	{
		if (!ENABLED)
			return;
		int now = System.GetTickCount();
		int step = now - s_iLast;
		if (step < EVERY_MS)
			return;
		s_iLast = now;
		float dt = Math.Min(step, EVERY_MS * 2) * 0.001;

		ChimeraCharacter c = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!c)
		{
			s_Char = null;
			return;
		}
		if (c != s_Char)
		{
			s_Char = c;
			s_bSafe = false;
			s_iSafeN = 0;
			s_fStuck = 0;
		}
		CharacterControllerComponent ctl = c.GetCharacterController();
		if (!ctl || ctl.GetLifeState() == ECharacterLifeState.DEAD)
			return;
		CompartmentAccessComponent access = c.GetCompartmentAccessComponent();
		if (access && (access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut()))
		{
			s_bSafe = false;     // review 2026-10-22: else a fall after a long drive went back to where the drive began
			s_iSafeN = 0;
			s_fStuck = 0;
			return;      // vehicles: MUD_VehicleAccess
		}

		vector o = c.GetOrigin();
		float undug;
		if (!MUD_WorldGroundEntity.UndugY(o[0], o[2], undug))
			return;      // not over our ground (or not ready): nothing to judge

		float dChest;
		float dHead;
		bool vox = MUD_VoxelProbe.FieldAt(o + Vector(0, CHEST_M, 0), dChest);
		if (vox && !MUD_VoxelProbe.FieldAt(o + Vector(0, HEAD_M, 0), dHead))
			vox = false;
		float floorY;
		if (vox)
		{
			floorY = undug - MUD_VoxelProbe.TUNNEL_FLOOR;
		}
		else
		{
			float dug;
			if (!MUD_WorldGroundEntity.SurfaceY(o[0], o[2], dug))
				dug = undug - MUD_TrenchPlan.MAX_LAYERS * MUD_TrenchPlan.LAYER;
			floorY = dug;
		}

		string why;
		if (o[1] < floorY - LOST_BELOW_M)
			why = "under the ground";
		if (vox && dChest > STUCK_D && dHead > STUCK_D)
		{
			s_fStuck += dt;
			if (s_fStuck >= STUCK_S && why.IsEmpty())
				why = "inside the wall";
		}
		else
		{
			s_fStuck = 0;
		}
		if (!why.IsEmpty())
		{
			if (now - s_iLastRescue >= RESCUE_GAP_MS)
				Rescue(c, o, why, vox, dChest, dHead, floorY);
			return;
		}

		bool safe = !ctl.IsFalling() && !ctl.IsClimbing() && o[1] > floorY - 0.3;
		if (vox && dChest > SAFE_OPEN_D)
			safe = false;
		if (!safe)
		{
			s_iSafeN = 0;
			return;
		}
		s_iSafeN++;
		if (s_iSafeN >= SAFE_HOLD)
		{
			s_vSafe = o;
			s_bSafe = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The last safe spot if it is still open (a blast may have filled it), else our ground top over the feet
	protected static void Rescue(ChimeraCharacter c, vector o, string why, bool vox, float dChest, float dHead, float floorY)
	{
		vector dest;
		string to = "ground top";
		bool useSafe = s_bSafe;
		float d;
		if (useSafe && MUD_VoxelProbe.FieldAt(s_vSafe + Vector(0, CHEST_M, 0), d) && d > SAFE_OPEN_D)
			useSafe = false;
		if (useSafe && vector.DistanceXZ(s_vSafe, o) > SAFE_MAX_M)
			useSafe = false;     // a GM / respawn teleport or a fast fall sideways: not a spot near here
		if (useSafe)
		{
			dest = s_vSafe + Vector(0, 0.05, 0);
			to = "last safe spot";
		}
		else
		{
			float top = MUD_Surface.Get().Y(c.GetWorld(), o[0], o[2]);
			dest = Vector(o[0], top + RESCUE_UP_M, o[2]);
		}

		vector tm[4];
		c.GetWorldTransform(tm);
		tm[3] = dest;
		c.Teleport(tm);
		Physics phys = c.GetPhysics();
		if (phys)
		{
			phys.SetVelocity(vector.Zero);
			phys.SetAngularVelocity(vector.Zero);
		}
		s_iLastRescue = System.GetTickCount();
		s_iSafeN = 0;
		s_fStuck = 0;
		s_iRescues++;
		if (MUD_TestKit.TEST_KIT)
			Print(string.Format("MUD tunnel guard: RESCUE %1 (%2) at %3 - voxel %4, chest %5 head %6, floor %7 -> %8 %9",
				s_iRescues, why, o, vox, dChest, dHead, floorY, to, dest), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Climb guard: true = refuse this climb. Only underground in a converted column (tunnel, deep voxel crater);
	//! trenches and open ground climb as vanilla. The landing spot must be open air at knee, chest and head height.
	static bool ClimbBlocked(ChimeraCharacter c, CharacterCommandHandlerComponent h)
	{
		if (!ENABLED || !CLIMB_GUARD || !c || !h)
			return false;
		vector o = c.GetOrigin();
		float undug;
		if (!MUD_WorldGroundEntity.UndugY(o[0], o[2], undug) || o[1] > undug - CLIMB_UNDER_M)
			return false;
		float d;
		if (!MUD_VoxelProbe.FieldAt(o + Vector(0, CHEST_M, 0), d))
			return false;

		CharacterCommandClimbResult res = new CharacterCommandClimbResult();
		CharacterCommandClimb.DoClimbTest(c, h.GetCommandClimbSettings(), res);
		if (!res.m_bIsClimb && !res.m_bIsClimbOver)
			return false;     // a plain jump
		vector land = res.m_ClimbGrabPointLS;
		if (res.m_bIsClimbOver)
			land = res.m_ClimbOverStandPointLS;
		if (res.m_ClimbParent)
		{
			vector ptm[4];
			res.m_ClimbParent.GetWorldTransform(ptm);
			land = land.Multiply4(ptm);
		}
		if (Open(land, KNEE_M) && Open(land, CHEST_M) && Open(land, HEAD_M))
			return false;
		if (MUD_TestKit.TEST_KIT)
			Print(string.Format("MUD tunnel guard: climb refused at %1 - landing %2 not open (over %3)", o, land, res.m_bIsClimbOver), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Open air up m above p (no converted column there = our heightfield, counted open)
	protected static bool Open(vector p, float up)
	{
		float d;
		if (!MUD_VoxelProbe.FieldAt(p + Vector(0, up, 0), d))
			return true;
		return d < CLIMB_OPEN_D;
	}
}

//! Climb guard hook (2026-10-22). Vanilla SCR_CharacterCommandHandlerComponent does not override this event (Exp 1.9
//! scripts: only the SCR_CharacterCommandHandler_Tests example does), so the default transition is called as is.
modded class SCR_CharacterCommandHandlerComponent
{
	override bool TransitionMove_JumpClimb(CharacterInputContext pInputCtx)
	{
		if (pInputCtx.GetJump() > 0 && MUD_TunnelGuard.ClimbBlocked(m_OwnerEntity, this))     // m_OwnerEntity: vanilla, set in OnInit (no GetOwner here)
			return false;
		return TransitionMove_JumpClimbDefault(pInputCtx);
	}
}
