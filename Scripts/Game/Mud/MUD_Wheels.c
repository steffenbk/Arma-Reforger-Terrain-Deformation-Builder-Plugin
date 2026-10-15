//! Mud plan 3: every wheeled vehicle in play, so the world ground never has to search the map for them
class MUD_Wheels
{
	protected static ref array<Vehicle> s_aAll = {};   // weak: a deleted vehicle reads null

	// Server move check (NwkCarMovementComponent). Vanilla: position limit = speed / 300 m, ~0 below 0.3 m/s,
	// angle 0.003 rad, speed 0.3 m/s, spin 0.25 (IDA Exp 192624). Ruts change the ground at slightly different
	// times on server and driver, so slow driving (reverse) broke the mm limit -> 0.2-0.6 m snaps on the driver
	// only (jolt probe 2026-10-10 11:42). Fixed limits instead. TEST VALUES, tune in-engine.
	// 2026-10-11 01:15-01:21 jolt probe (DS + peer, wheel hold on): driver-only snaps 0.11-0.21 m with
	// accel 8-29 m/s2, mostly at 0.4-2.5 m/s while starting / slowing / reversing; user: vanilla drivers
	// stutter too, passengers not. Limits loosened: was 0.15 m / 0.02 rad / 0.5 m/s / 0.4 rad/s.
	static const float NET_DIST = 0.25;
	static const float NET_ANGLE = 0.05;
	static const float NET_SPEED = 1.0;
	static const float NET_SPIN = 0.8;
	// TEST 2026-10-11 16:40: false = leave the engine's own limits (speed / 300 m). Every DS stutter test today ran
	// with the limits above (snap size = NET_DIST); this checks whether true vanilla gives only tiny corrections.
	static const bool NET_LIMITS = false;

	//------------------------------------------------------------------------------------------------
	static void Register(Vehicle v)
	{
		if (!v || s_aAll.Contains(v) || !v.FindComponent(VehicleWheeledSimulation))
			return;
		Prune();
		s_aAll.Insert(v);
		NwkCarMovementComponent nwk = NwkCarMovementComponent.Cast(v.FindComponent(NwkCarMovementComponent));
		if (nwk && NET_LIMITS)
			nwk.SetAllowance(false, NET_DIST, NET_ANGLE, NET_SPEED, NET_SPIN);
		Print(string.Format("MUD wheels: %1 registered, own net limits %2", v.GetID(), NET_LIMITS));
	}

	//------------------------------------------------------------------------------------------------
	//! Live wheeled vehicles (statics outlive a play session: dead entries are pruned here)
	static array<Vehicle> GetAll()
	{
		Prune();
		return s_aAll;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Prune()
	{
		for (int i = s_aAll.Count() - 1; i >= 0; i--)
		{
			if (!s_aAll[i])
				s_aAll.Remove(i);
		}
	}
}

//------------------------------------------------------------------------------------------------
modded class Vehicle
{
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		if (GetGame().InPlayMode())
			MUD_Wheels.Register(this);
	}
}
