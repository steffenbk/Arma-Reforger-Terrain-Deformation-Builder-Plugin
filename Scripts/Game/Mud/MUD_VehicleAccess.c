//! Vehicle get in/out on mud ground. The native check (CanGetInCompartmentViaDoor, and GetInVehicle /
//! GetOutVehicle again when performed - IDA Exp 192624: sub_1401D3120 / sub_1401D4FB0 -> sub_1401D74F0)
//! sweeps a box whose bottom is exactly at the soldier's feet against entities only. On our mud it
//! always grazes the ground the soldier stands on: get in grayed out "SeatObstructed" (2026-10-08,
//! flat mud too; worked only when jumping or at random spots). Each check is wrapped so the mud is
//! invisible to traces for that one call - see MUD_VehicleAccess.HideFromTraces.
//! The started animation checks again by itself (CharacterVehicleActionGetInAnimated vtable slot 2,
//! sub_1408B9E60, same two traces): wrapping only the call let get in start and stop at once
//! (2026-10-08 13:10), so the mud also stays hidden while someone is getting in or out.

//------------------------------------------------------------------------------------------------
modded class SCR_GetInUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		bool mud = MUD_VehicleAccess.OnMud(GetOwner(), user);
		if (mud)
			MUD_VehicleAccess.HideFromTraces(true);
		bool ok = super.CanBePerformedScript(user);
		if (mud)
			MUD_VehicleAccess.HideFromTraces(false);
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		bool mud = MUD_VehicleAccess.OnMud(pOwnerEntity, pUserEntity);
		if (mud)
			MUD_VehicleAccess.HideFromTraces(true);
		super.PerformAction(pOwnerEntity, pUserEntity);
		MUD_VehicleAccess.LogStart("get in", pUserEntity, mud);
		if (mud)
		{
			MUD_VehicleAccess.HoldWhileMoving(ChimeraCharacter.Cast(pUserEntity));
			MUD_VehicleAccess.HideFromTraces(false);
		}
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_GetOutAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		bool mud = MUD_VehicleAccess.OnMud(pOwnerEntity, pUserEntity);
		if (mud)
			MUD_VehicleAccess.HideFromTraces(true);
		super.PerformAction(pOwnerEntity, pUserEntity);
		MUD_VehicleAccess.LogStart("get out", pUserEntity, mud);
		if (mud)
		{
			MUD_VehicleAccess.HoldWhileMoving(ChimeraCharacter.Cast(pUserEntity));
			MUD_VehicleAccess.HideFromTraces(false);
		}
	}
}

//------------------------------------------------------------------------------------------------
class MUD_VehicleAccess
{
	static const int MIN_MS = 500;      // hold at least this long: the transition may not have begun yet
	static const int MAX_MS = 10000;    // safety cap per character

	protected static ref array<ChimeraCharacter> s_aChars = {};
	protected static ref array<int> s_aStart = {};
	protected static bool s_bTicking;
	// TEMP fall probe (DS 2026-10-19 23:35: get in from a rut put the player ~4 m under the ground)
	static const int PROBE_AFTER_MS = 3000;
	static const int PROBE_EVERY_MS = 200;
	protected static ref array<ChimeraCharacter> s_aWatch = {};
	protected static ref array<int> s_aWatchEnd = {};
	protected static ref array<float> s_aWatchY = {};   // soldier y when the hold started
	// Fall rescue (DS 2026-10-19 23:41, probe): a get in that starts and is cancelled while the mud is hidden puts
	// the soldier 4.5 m down in < 0.2 s (37.8 -> 33.26, our ground 37.77) - he stays under our ground after the
	// unhide. Once the mud is traceable again: on foot, under our ground by RESCUE_M and FALL_M below the start
	// -> put back on our ground (the FALL_M rule keeps a soldier in a tunnel under the surface where he is).
	static const float RESCUE_M = 0.5;
	static const float FALL_M = 1.0;
	static const float RESCUE_UP_M = 0.05;
	protected static int s_iLastProbe;

	//------------------------------------------------------------------------------------------------
	protected static int s_iTraceHideDepth;

	//------------------------------------------------------------------------------------------------
	//! Hide all our ground from traces (hide = true) or bring it back (false). Calls nest.
	//! For the native vehicle get in/out check: its box starts exactly at the soldier's feet (stance
	//! shape offset Y 0, Character_Base.et ColliderStand), so standing on our mud it always touches
	//! it - real terrain is never tested there (VehicleActionObstructionTrace, flags ENTS|ANY_CONTACT).
	//! Also held across frames while someone gets in/out (HoldWhileMoving): every trace on this machine
	//! misses all our ground meanwhile. (Moved from the archived MUD_GroundEntity, audit C6 2026-10-22.)
	static void HideFromTraces(bool hide)
	{
		if (hide)
		{
			s_iTraceHideDepth++;
			if (s_iTraceHideDepth > 1)
				return;
		}
		else
		{
			if (s_iTraceHideDepth <= 0)
				return;
			s_iTraceHideDepth--;
			if (s_iTraceHideDepth > 0)
				return;
		}
		// plan 3: the world ground's fine patches lie under parked vehicles too
		MUD_WorldGroundEntity.SetTraceHidden(hide);
		// plan 3 task 1 row R6 FAILED (get in from the skin): hide the skin chunks too
		MUD_CoarseSkinEntity.SetTraceHidden(hide);
		// mud sheet colliders (2026-10-15: get in / out blocked on the mud; the native check ignores layers)
		MUD_MudSheet.SetTraceHidden(hide);
		// voxel bricks (2026-10-15: can't get in / out in a rut, they were never hidden)
		MUD_VoxelProbe.SetTraceHidden(hide);
	}

	//------------------------------------------------------------------------------------------------
	//! New play session (MUD_WorldGroundEntity.Setup): statics outlive the session, its call queue (Tick) does not
	static void ResetTraceHide()
	{
		s_iTraceHideDepth = 0;
		Reset();
	}

	//------------------------------------------------------------------------------------------------
	//! Forget all holds (new play session, from ResetTraceHide)
	static void Reset()
	{
		s_aChars.Clear();
		s_aStart.Clear();
		s_aWatch.Clear();
		s_aWatchEnd.Clear();
		s_aWatchY.Clear();
		s_bTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP (2026-10-19 server hide fix): which side ran the action, mud hidden or not, and whether the
	//! get in / out really started (IsGettingIn / Out right after the native call)
	static void LogStart(string what, IEntity user, bool mud)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(user);
		CompartmentAccessComponent access;
		if (c)
			access = c.GetCompartmentAccessComponent();
		bool started = access && (access.IsGettingIn() || access.IsGettingOut());
		string side = "client";
		if (Replication.IsServer())
			side = "server";
		if (MUD_TestKit.TEST_KIT)     // audit C1: test prints only
			Print(string.Format("MUD vehicle access: %1 on %2, mud hidden %3, started %4", what, side, mud, started), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! True if the vehicle or the user is over a mud ground. With the whole-map ground always true: DS
	//! 2026-10-19 23:24 the server ran get out (door open + shut at once) but never logged a hide - the
	//! hole / sheet test below said no there, so the server's own native check hit our ground.
	static bool OnMud(IEntity vehicleOwner, IEntity user)
	{
		if (MUD_WorldGroundEntity.GetInstance())
			return true;
		if (vehicleOwner)
		{
			IEntity root = vehicleOwner.GetRootParent();
			if (root && OnWorldGround(root))
				return true;
		}
		return user && OnWorldGround(user);
	}

	//------------------------------------------------------------------------------------------------
	//! Plan 3: over a hole cell of the holed map (skin or fine patch under it)
	static bool OnWorldGround(IEntity e)
	{
		if (!MUD_WorldGroundEntity.GetInstance())
			return false;
		vector o = e.GetOrigin();
		// mud sheet tiles lie on runways / roads too (not hole cells)
		return MUD_Surface.IsHole(e.GetWorld(), o[0], o[2]) || MUD_MudSheet.MudOver(o[0], o[2]) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Keep the mud hidden from traces until this character has finished getting in/out
	static void HoldWhileMoving(ChimeraCharacter character)
	{
		if (!character || s_aChars.Contains(character))
			return;

		s_aChars.Insert(character);
		s_aStart.Insert(System.GetTickCount());
		int w = s_aWatch.Find(character);
		if (w < 0)
		{
			s_aWatch.Insert(character);
			s_aWatchEnd.Insert(System.GetTickCount() + MAX_MS + PROBE_AFTER_MS);
			s_aWatchY.Insert(character.GetOrigin()[1]);
		}
		else
		{
			s_aWatchEnd[w] = System.GetTickCount() + MAX_MS + PROBE_AFTER_MS;
			s_aWatchY[w] = character.GetOrigin()[1];
		}
		MUD_VehicleAccess.HideFromTraces(true);
		if (MUD_TestKit.TEST_KIT)     // audit C1: test prints only
			Print("MUD vehicle access: mud hidden from traces while getting in/out", LogLevel.NORMAL);

		if (!s_bTicking)
		{
			s_bTicking = true;
			GetGame().GetCallqueue().CallLater(Tick, 0, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP fall probe: soldier height every 200 ms while held and 3 s after; our ground height only when
	//! nothing is held (the height reader traces, so it sees nothing while the mud is hidden)
	protected static void Probe(int now)
	{
		for (int i = s_aWatch.Count() - 1; i >= 0; i--)
		{
			if (!s_aWatch[i] || now > s_aWatchEnd[i])
			{
				s_aWatch.Remove(i);
				s_aWatchEnd.Remove(i);
				s_aWatchY.Remove(i);
			}
		}

		bool held = !s_aChars.IsEmpty();
		if (!held)
		{
			for (int r = 0; r < s_aWatch.Count(); r++)
				Rescue(s_aWatch[r], s_aWatchY[r]);
		}

		if (s_aWatch.IsEmpty() || now - s_iLastProbe < PROBE_EVERY_MS)
			return;
		s_iLastProbe = now;

		foreach (ChimeraCharacter c : s_aWatch)
		{
			vector o = c.GetOrigin();
			CompartmentAccessComponent access = c.GetCompartmentAccessComponent();
			bool inVeh = access && access.IsInCompartment();
			bool moving = access && (access.IsGettingIn() || access.IsGettingOut());
			string ground = "hidden";
			if (!held)
				ground = MUD_Surface.Get().Y(c.GetWorld(), o[0], o[2]).ToString();
			if (MUD_TestKit.TEST_KIT)     // audit C1: test prints only
				Print(string.Format("MUD vehicle access probe: at %1 | ground y %2 | held %3, in vehicle %4, getting in/out %5", o, ground, held, inVeh, moving), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Put a soldier who ended under our ground back on top (see RESCUE_M). Mud must be traceable.
	protected static void Rescue(ChimeraCharacter c, float startY)
	{
		CompartmentAccessComponent access = c.GetCompartmentAccessComponent();
		if (!access || access.IsInCompartment() || access.IsGettingIn() || access.IsGettingOut())
			return;
		vector o = c.GetOrigin();
		if (o[1] > startY - FALL_M)
			return;
		float top = MUD_Surface.Get().Y(c.GetWorld(), o[0], o[2]);
		if (o[1] > top - RESCUE_M)
			return;

		vector tm[4];
		c.GetWorldTransform(tm);
		tm[3] = Vector(o[0], top + RESCUE_UP_M, o[2]);
		c.Teleport(tm);
		Physics phys = c.GetPhysics();
		if (phys)
		{
			phys.SetVelocity(vector.Zero);
			phys.SetAngularVelocity(vector.Zero);
		}
		if (MUD_TestKit.TEST_KIT)     // audit C1: test prints only
			Print(string.Format("MUD vehicle access: RESCUE soldier from y %1 (start %2) up to our ground %3", o[1], startY, top), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		int now = System.GetTickCount();
		for (int i = s_aChars.Count() - 1; i >= 0; i--)
		{
			ChimeraCharacter c = s_aChars[i];
			int age = now - s_aStart[i];
			bool busy = false;
			if (c && age < MAX_MS)
			{
				if (age < MIN_MS)
				{
					busy = true;
				}
				else
				{
					CompartmentAccessComponent access = c.GetCompartmentAccessComponent();
					busy = access && (access.IsGettingIn() || access.IsGettingOut());
				}
			}
			if (busy)
				continue;

			if (MUD_TestKit.TEST_KIT)     // audit C1: test prints only
				Print(string.Format("MUD vehicle access: done after %1 ms, mud traceable again", age), LogLevel.NORMAL);
			int wi = s_aWatch.Find(c);
			if (wi >= 0)
				s_aWatchEnd[wi] = now + PROBE_AFTER_MS;
			s_aChars.Remove(i);
			s_aStart.Remove(i);
			MUD_VehicleAccess.HideFromTraces(false);
		}

		Probe(now);

		if (s_aChars.IsEmpty() && s_aWatch.IsEmpty())
		{
			s_bTicking = false;
			GetGame().GetCallqueue().Remove(Tick);
		}
	}
}
