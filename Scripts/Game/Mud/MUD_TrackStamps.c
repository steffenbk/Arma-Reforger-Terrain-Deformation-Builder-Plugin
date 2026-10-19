//! Wheel track stamps (2026-10-18, ruts README 'Wheel-to-rut track mark'): every STEP m a wheel rolls, one track-
//! shaped decal (MUD_TrackDecal.emat = RoadForger-style MatPBRDecal: vanilla Road_Dirt textures + our MUD_TrackMask
//! black / white mask, track along the texture's V, ~0.39 of the square wide) on the entity the wheel touches.
//! World.CreateDecal draws on our ground (decal probe run 2) but ALWAYS returns null in this build (mud-gotchas) -
//! judge by eye. Known: a decal goes when its patch rebuilds (rut cut close); grass still pokes through.
//! Rendering machines only (called from MUD_LookRuts.Tick), vehicles within RANGE of the camera. false = off.
class MUD_TrackStampWheels
{
	ref array<vector> m_aLast = {};     // last stamp spot per wheel
	ref array<bool> m_aHas = {};
}

class MUD_TrackStamps
{
	static const bool ENABLED = false;    // PARKED 2026-10-18 (user): focus = voxel ruts at the wheel
	static const bool LOG = true;          // one 'MUD track stamps' line every 5 s (count, last contact class)
	static const float RANGE = 80;
	static const float STEP = 1.0;         // m rolled per stamp (stamp is SIZE long -> overlap)
	static const float JUMP = 5;           // farther than this since the last stamp = teleport / respawn: restart
	static const float SIZE = 1.4;         // decal square (m): track ~0.55 m wide x 1.4 m long
	static const float ANGLE_OFF = 0;      // degrees added to the heading if the track lies across the travel line
	static const float LIFE = 120;
	static const ResourceName MAT = "{4253A1BA2A9FAF38}Assets/Mud/MUD_TrackDecal.emat";

	protected static ref map<Vehicle, ref MUD_TrackStampWheels> s_mVeh = new map<Vehicle, ref MUD_TrackStampWheels>();
	protected static int s_iStamps;
	protected static float s_fLogAt;
	protected static string s_sLastOn = "none";

	//------------------------------------------------------------------------------------------------
	static void Tick(float now)
	{
		if (!ENABLED)
			return;
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return;
		vector cm[4];
		world.GetCurrentCamera(cm);
		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh || vector.DistanceSq(veh.GetOrigin(), cm[3]) > RANGE * RANGE)
				continue;
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
			if (!sim || !sim.IsValid())
				continue;
			MUD_TrackStampWheels st = s_mVeh.Get(veh);
			if (!st)
			{
				st = new MUD_TrackStampWheels();
				s_mVeh.Insert(veh, st);
			}
			int n = sim.WheelCount();
			while (st.m_aLast.Count() < n)
			{
				st.m_aLast.Insert(vector.Zero);
				st.m_aHas.Insert(false);
			}
			for (int w = 0; w < n; w++)
				Wheel(world, sim, st, w);
		}
		if (LOG && now >= s_fLogAt)
		{
			s_fLogAt = now + 5;
			Print(string.Format("MUD track stamps: %1 made so far, %2 vehicles tracked, last on %3", s_iStamps, s_mVeh.Count(), s_sLastOn), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Wheel(ChimeraWorld world, VehicleWheeledSimulation sim, MUD_TrackStampWheels st, int w)
	{
		if (!sim.WheelHasContact(w))
		{
			st.m_aHas[w] = false;      // airborne: start a fresh line on landing
			return;
		}
		vector cp = sim.WheelGetContactPosition(w);
		if (!st.m_aHas[w])
		{
			st.m_aLast[w] = cp;
			st.m_aHas[w] = true;
			return;
		}
		vector d = cp - st.m_aLast[w];
		d[1] = 0;
		float len = d.Length();
		if (len < STEP)
			return;
		if (len > JUMP)
		{
			st.m_aLast[w] = cp;
			return;
		}
		IEntity on = sim.WheelGetContactEntity(w);
		vector nrm = sim.WheelGetContactNormal(w);
		if (nrm.Length() < 0.5)
			nrm = "0 1 0";
		vector mid = (cp + st.m_aLast[w]) * 0.5;
		st.m_aLast[w] = cp;
		if (!on)
			return;
		float angle = Math.Atan2(d[0], d[2]) * Math.RAD2DEG + ANGLE_OFF;
		// spray-mod form (proven to show on our ground): origin 0.2 m off along n, project -n, near 0, far 1, stretch 1
		world.CreateDecal(on, mid + nrm * 0.2, -nrm, 0, 1.0, angle, SIZE, 1, MAT, LIFE, 0xFFFFFFFF);
		s_iStamps++;
		s_sLastOn = on.ClassName();
	}

	//------------------------------------------------------------------------------------------------
	//! new session: forget all
	static void Reset()
	{
		s_mVeh.Clear();
		s_iStamps = 0;
		s_fLogAt = 0;
		s_sLastOn = "none";
	}
}
