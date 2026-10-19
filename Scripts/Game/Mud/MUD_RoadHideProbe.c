//! TEMP road hide probe (user 2026-10-18: real craters on roads, option B, PvP only - AI pathing not needed).
//! Question: can script hide a road mesh at runtime, and how big is one RoadEntity (one short piece or a whole spline)?
//! Rendering machines only (called from MUD_LookRuts.Tick). Look at a road (centre of the screen), then:
//!   Numpad 7 = RoadEntity.Show(false) on the road under the crosshair
//!   Numpad 9 = ClearFlags(EntityFlags.VISIBLE) on it (second way, if 7 does nothing)
//!   Numpad 8 = undo both on every road this probe touched
//! One 'MUD road probe:' line per press: entity class, origin, bounds size (m), physics, flags. false = no probe.
class MUD_RoadHideProbe
{
	static const bool ENABLED = true;
	static const float REACH = 300;     // trace length from the camera (m)

	protected static ref array<IEntity> s_aTouched = {};
	protected static ref TraceParam s_Trace;

	//------------------------------------------------------------------------------------------------
	static void Tick(float now)
	{
		if (!ENABLED)
			return;
		if (Debug.KeyState(KeyCode.KC_NUMPAD7))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD7);
			Hide(true);
		}
		if (Debug.KeyState(KeyCode.KC_NUMPAD9))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD9);
			Hide(false);
		}
		if (Debug.KeyState(KeyCode.KC_NUMPAD8))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD8);
			Restore();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! show = true: RoadEntity.Show(false); false: clear the VISIBLE flag
	protected static void Hide(bool useShow)
	{
		BaseWorld world = GetGame().GetWorld();
		vector cm[4];
		world.GetCurrentCamera(cm);
		if (!s_Trace)
			s_Trace = new TraceParam();
		s_Trace.Start = cm[3];
		s_Trace.End = cm[3] + cm[2] * REACH;
		s_Trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		s_Trace.Exclude = SCR_PlayerController.GetLocalControlledEntity();
		s_Trace.TraceEnt = null;
		float frac = world.TraceMove(s_Trace, null);
		IEntity hit = s_Trace.TraceEnt;
		vector at = s_Trace.Start + (s_Trace.End - s_Trace.Start) * frac;
		if (!hit)
		{
			Print(string.Format("MUD road probe: nothing hit (frac %1) at %2", frac, at));
			return;
		}
		RoadEntity road = RoadEntity.Cast(hit);
		vector mins;
		vector maxs;
		hit.GetWorldBounds(mins, maxs);
		vector size = maxs - mins;
		string how = "none (not a RoadEntity)";
		if (road)
		{
			if (useShow)
			{
				road.Show(false);
				how = "Show(false)";
			}
			else
			{
				road.ClearFlags(EntityFlags.VISIBLE, false);
				how = "ClearFlags VISIBLE";
			}
			if (!s_aTouched.Contains(road))
				s_aTouched.Insert(road);
		}
		Print(string.Format("MUD road probe: hit %1 at %2, origin %3, bounds size %4 m, physics %5, flags %6, did %7",
			hit.ClassName(), at, hit.GetOrigin(), size, hit.GetPhysics() != null, hit.GetFlags(), how));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Restore()
	{
		int n;
		foreach (IEntity e : s_aTouched)
		{
			RoadEntity road = RoadEntity.Cast(e);
			if (!road)
				continue;
			road.Show(true);
			road.SetFlags(EntityFlags.VISIBLE, false);
			n++;
		}
		s_aTouched.Clear();
		Print(string.Format("MUD road probe: restored %1 roads", n));
	}
}
