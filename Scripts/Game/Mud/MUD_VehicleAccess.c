//! Vehicle get in/out on mud ground. The native check (CanGetInCompartmentViaDoor, and GetInVehicle /
//! GetOutVehicle again when performed - IDA Exp 192624: sub_1401D3120 / sub_1401D4FB0 -> sub_1401D74F0)
//! sweeps a box whose bottom is exactly at the soldier's feet against entities only. On our mud it
//! always grazes the ground the soldier stands on: get in grayed out "SeatObstructed" (2026-10-08,
//! flat mud too; worked only when jumping or at random spots). Each check is wrapped so the mud is
//! invisible to traces for that one call - see MUD_GroundEntity.HideFromTraces.
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
			MUD_GroundEntity.HideFromTraces(true);
		bool ok = super.CanBePerformedScript(user);
		if (mud)
			MUD_GroundEntity.HideFromTraces(false);
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		bool mud = MUD_VehicleAccess.OnMud(pOwnerEntity, pUserEntity);
		if (mud)
			MUD_GroundEntity.HideFromTraces(true);
		super.PerformAction(pOwnerEntity, pUserEntity);
		if (mud)
		{
			MUD_VehicleAccess.HoldWhileMoving(ChimeraCharacter.Cast(pUserEntity));
			MUD_GroundEntity.HideFromTraces(false);
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
			MUD_GroundEntity.HideFromTraces(true);
		super.PerformAction(pOwnerEntity, pUserEntity);
		if (mud)
		{
			MUD_VehicleAccess.HoldWhileMoving(ChimeraCharacter.Cast(pUserEntity));
			MUD_GroundEntity.HideFromTraces(false);
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

	//------------------------------------------------------------------------------------------------
	//! Forget all holds (new play session, see MUD_GroundEntity.EOnInit)
	static void Reset()
	{
		s_aChars.Clear();
		s_aStart.Clear();
		s_bTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	//! True if the vehicle or the user is over a mud ground
	static bool OnMud(IEntity vehicleOwner, IEntity user)
	{
		if (vehicleOwner)
		{
			IEntity root = vehicleOwner.GetRootParent();
			if (root && (MUD_GroundEntity.IsOnAnyGround(root.GetOrigin()) || OnWorldGround(root)))
				return true;
		}
		return user && (MUD_GroundEntity.IsOnAnyGround(user.GetOrigin()) || OnWorldGround(user));
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
		MUD_GroundEntity.HideFromTraces(true);
		Print("MUD vehicle access: mud hidden from traces while getting in/out", LogLevel.NORMAL);

		if (!s_bTicking)
		{
			s_bTicking = true;
			GetGame().GetCallqueue().CallLater(Tick, 0, true);
		}
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

			Print(string.Format("MUD vehicle access: done after %1 ms, mud traceable again", age), LogLevel.NORMAL);
			s_aChars.Remove(i);
			s_aStart.Remove(i);
			MUD_GroundEntity.HideFromTraces(false);
		}

		if (s_aChars.IsEmpty())
		{
			s_bTicking = false;
			GetGame().GetCallqueue().Remove(Tick);
		}
	}
}
