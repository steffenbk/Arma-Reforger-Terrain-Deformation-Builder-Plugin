//! TEMP decal probe (2026-10-18, prompts/2026-10-18-wheel-to-rut-decal.md step 1): can a decal land under a wheel?
//! Run 1 (WB 20:32, mud-gotchas): CreateDecal null on every target, even the vehicle body and fresh script quads.
//! Run 2 (user: 'decals on the normal asphalt on a road, then offroad'): the spray-can mod's exact call
//! (origin 0.2 m off the surface, project -normal, near 0, far 1, stretch 1) with its known-good material (Blood10,
//! bright red = easy to spot). Rendering machines only (called from MUD_LookRuts.Tick). Every EVERY s, PROBES times,
//! for the vehicle nearest the camera with wheel 0 on the ground, it tries:
//!   A contact  = the wheel's contact entity (road: RoadEntity / terrain? offroad: our MUD_ chunk / patch), blood, at the wheel
//!   B contact  = the same with the mortar decal (run 1 material), 0.7 m ahead
//!   C terrain  = the terrain entity (GenericTerrainEntity), blood, 0.7 m behind
//!   D trace    = whatever an ENTS|WORLD trace down hits first (vehicle excluded), blood, 0.7 m to the side
//!   E veh      = the vehicle body, blood (control)
//! Tries sit >= 0.7 m apart: the engine dedupes a same-material decal within ~0.32 m (IDA). One 'MUD decal probe'
//! line per probe (1 = a Decal came back, 0 = null, -1 = no target). Decals live LIFE s. false = no probe.
class MUD_DecalProbe
{
	static const bool ENABLED = false;    // run 2 done (ruts README); run 3 = MUD_GrassCutProbe
	static const float EVERY = 1.5;
	static const int PROBES = 120;      // ~3 min of wheel contact: drive the road first, then offroad
	static const float LIFE = 60;
	static const float SIZE = 1.0;
	static const ResourceName MAT_BLOOD = "{9EE38D3540663BCF}Assets/Decals/Blood/Decal_Blood10.emat";   // spray-can mod default (SCR_SprayRaycastComponent.c)
	static const ResourceName MAT_MORTAR = "{039FA43299C7980D}Assets/Decals/Miscellaneous/Mortar/Decal_Impact_Mortar_01.emat";   // as MUD_VoxelProbe.DECAL_MAT

	protected static ref MUD_DecalProbe s_Inst;
	protected float m_fNextAt = 3;
	protected int m_iDone;
	protected ref TraceParam m_Trace = new TraceParam();

	//------------------------------------------------------------------------------------------------
	static void Tick(float now)
	{
		if (!ENABLED)
			return;
		if (!s_Inst)
			s_Inst = new MUD_DecalProbe();
		s_Inst.Run(now);
	}

	//------------------------------------------------------------------------------------------------
	protected void Run(float now)
	{
		if (m_iDone >= PROBES || now < m_fNextAt)
			return;
		vector cm[4];
		GetGame().GetWorld().GetCurrentCamera(cm);
		Vehicle veh;
		float best = 30 * 30;
		foreach (Vehicle v : MUD_Wheels.GetAll())
		{
			if (!v)
				continue;
			float d = vector.DistanceSq(v.GetOrigin(), cm[3]);
			if (d < best)
			{
				best = d;
				veh = v;
			}
		}
		if (!veh)
			return;
		VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
		if (!sim || !sim.IsValid() || sim.WheelCount() < 1 || !sim.WheelHasContact(0))
			return;
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return;
		m_fNextAt = now + EVERY;
		m_iDone++;
		vector cp = sim.WheelGetContactPosition(0);
		vector n = sim.WheelGetContactNormal(0);
		if (n.Length() < 0.5)
			n = "0 1 0";
		vector fwd = veh.GetTransformAxis(2);
		vector side = veh.GetTransformAxis(0);
		IEntity contact = sim.WheelGetContactEntity(0);
		GenericTerrainEntity terrain;
		GenericWorldEntity worldEnt = GetGame().GetWorldEntity();
		if (worldEnt)
			terrain = worldEnt.GetTerrain(cp[0], cp[2]);
		// D: first thing straight down at the side spot (vehicle excluded)
		vector dp = cp + side * 0.7;
		m_Trace.Start = dp + Vector(0, 1, 0);
		m_Trace.End = dp - Vector(0, 1, 0);
		m_Trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		m_Trace.Exclude = veh;
		m_Trace.TraceEnt = null;
		float frac = world.TraceMove(m_Trace, null);
		IEntity traced = m_Trace.TraceEnt;
		vector tn = n;
		if (frac < 1)
		{
			dp = m_Trace.Start + (m_Trace.End - m_Trace.Start) * frac;
			if (m_Trace.TraceNorm.Length() > 0.5)
				tn = m_Trace.TraceNorm;
		}
		int rA = Try(world, contact, cp, n, MAT_BLOOD);
		int rB = Try(world, contact, cp + fwd * 0.7, n, MAT_MORTAR);
		int rC = Try(world, terrain, cp - fwd * 0.7, n, MAT_BLOOD);
		int rD = -1;
		if (frac < 1)
			rD = Try(world, traced, dp, tn, MAT_BLOOD);
		float off = (m_iDone - 1) * 0.1 - 2;     // a fresh spot per probe on the body (dedupe)
		Decal dv = world.CreateDecal(veh, veh.GetOrigin() + Vector(0, 4, 0) + fwd * off, "0 -1 0", 0, 6, 0, SIZE, 1, MAT_BLOOD, LIFE, 0xFFFFFFFF);
		int rE = B(dv != null);
		Print(string.Format("MUD decal probe %1/%2 at %3: A contact %4 = %5 | B contact mortar = %6 | C terrain %7 = %8 | D traced %9",
			m_iDone, PROBES, cp, Name(contact), rA, rB, terrain != null, rC, Name(traced))
			+ string.Format(" = %1 | E veh body = %2 (1 = made, 0 = null, -1 = no target)", rD, rE), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! the spray-can mod's call: origin 0.2 m off the surface along n, projected -n, near 0, far 1, stretch 1
	protected int Try(ChimeraWorld world, IEntity e, vector p, vector n, ResourceName mat)
	{
		if (!e)
			return -1;
		Decal d = world.CreateDecal(e, p + n * 0.2, -n, 0, 1.0, 0, SIZE, 1, mat, LIFE, 0xFFFFFFFF);
		return B(d != null);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Name(IEntity e)
	{
		if (!e)
			return "null";
		return e.ClassName();
	}

	//------------------------------------------------------------------------------------------------
	protected static int B(bool b)
	{
		if (b)
			return 1;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! new session: forget all
	static void Reset()
	{
		s_Inst = null;
	}
}
