//! TEMP grass-cut probe (2026-10-18, ruts README 'Wheel-to-rut track mark'): can a track mark remove grass offroad?
//! IDA (research/2026-10-15-ida-rut-questions.md item 2): only meshes with the MeshObject ClutterOccluder flag cut grass;
//! our runtime entities have no MeshObject component. RoadForger cuts grass with both ideas below (user's mod).
//! Rendering machines only (called from MUD_LookRuts.Tick). Every EVERY s, SETS times, for the vehicle nearest the
//! camera with wheel 0 on the ground, it lays three marks beside the vehicle (out of the wheels' own grass flattening):
//!   A left  SIDE_A m = MudGrassCutDecal.et: GenericEntity + DecalEntity child, ClutterOccluder 1, Blood10 (red), 2 m.
//!                      Open question: a DecalEntity is built from the heightmap = under our ground on hole cells.
//!   B right SIDE_B m = MudGrassCutMesh.et: GenericEntity + MeshObject (ClutterOccluder 1, clip plane 2), then our own
//!                      dirt strip via SetObject. Open question: does the flag survive SetObject?
//!   C right SIDE_C m = CONTROL: the same strip from MudGrassCutMeshOff.et (ClutterOccluder 0): grass pokes through.
//!                      Run 1 (WB 20:55) used a plain GenericEntity: made 15/15 in the log but never drawn (cause unknown).
//! Run 1 result (user): A shows on asphalt only (decal = heightmap, under our ground offroad); B shows offroad + asphalt.
//! Strips are STRIP_W x STRIP_L m, LIFT over the traced ground, corners follow the ground. Judge by eye: B without
//! grass and C with grass = the flag survives. One 'MUD grass cut' line per set. Marks live LIFE s. false = no probe.
class MUD_GrassCutProbe
{
	static const bool ENABLED = false;    // runs 1-2 done (ruts README): grass not cut
	static const float EVERY = 4;
	static const int SETS = 15;
	static const float LIFE = 180;
	static const float SIDE_A = -3;
	static const float SIDE_B = 3;
	static const float SIDE_C = 5.5;
	static const float STRIP_W = 0.6;
	static const float STRIP_L = 3;
	static const float LIFT = 0.03;
	static const ResourceName PREFAB_DECAL = "{5B374ED5DDC380D5}Prefabs/Mud/MudGrassCutDecal.et";
	static const ResourceName PREFAB_MESH = "{4A1C7E93B25D06F8}Prefabs/Mud/MudGrassCutMesh.et";
	static const ResourceName PREFAB_MESH_OFF = "{7C2E05B9D14A63F1}Prefabs/Mud/MudGrassCutMeshOff.et";   // run 2: C = same prefab, ClutterOccluder 0
	static const ResourceName MAT = "{6EBF7ADA9FD582F7}Assets/Structures/Military/Fortifications/DirtMounds/Data/DirtCover_01.emat";   // = MUD_VoxelProbe.MAT_DIRT

	protected static ref MUD_GrassCutProbe s_Inst;
	protected float m_fNextAt = 3;
	protected int m_iDone;
	protected ref TraceParam m_Trace = new TraceParam();
	protected ref array<IEntity> m_aEnts = {};
	protected ref array<float> m_aDieAt = {};
	protected ref array<ref Resource> m_aRes = {};      // keep the script meshes alive

	//------------------------------------------------------------------------------------------------
	static void Tick(float now)
	{
		if (!ENABLED)
			return;
		if (!s_Inst)
			s_Inst = new MUD_GrassCutProbe();
		s_Inst.Run(now);
	}

	//------------------------------------------------------------------------------------------------
	protected void Run(float now)
	{
		Expire(now);
		if (m_iDone >= SETS || now < m_fNextAt)
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
		m_fNextAt = now + EVERY;
		m_iDone++;
		vector org = veh.GetOrigin();
		vector side = veh.GetTransformAxis(0);
		vector fwd = veh.GetTransformAxis(2);
		fwd[1] = 0;
		if (fwd.LengthSq() < 0.01)
			fwd = vector.Forward;
		fwd.Normalize();
		side = Vector(fwd[2], 0, -fwd[0]);     // flat right

		// A: decal occluder prefab
		string hitA;
		vector pa;
		int rA = -1;
		if (Ground(org + side * SIDE_A, veh, pa, hitA))
			rA = B(SpawnPrefab(PREFAB_DECAL, pa, fwd, now) != null);

		// B: occluder MeshObject prefab + our strip
		string hitB;
		vector pb;
		int rB = -1;
		if (Ground(org + side * SIDE_B, veh, pb, hitB))
		{
			IEntity eb = SpawnPrefab(PREFAB_MESH, pb, fwd, now);
			rB = B(eb != null && Strip(eb, pb, fwd, side, veh));
		}

		// C: control strip, same prefab with the flag off
		string hitC;
		vector pc;
		int rC = -1;
		if (Ground(org + side * SIDE_C, veh, pc, hitC))
		{
			IEntity ec = SpawnPrefab(PREFAB_MESH_OFF, pc, fwd, now);
			rC = B(ec != null && Strip(ec, pc, fwd, side, veh));
		}
		Print(string.Format("MUD grass cut %1/%2: A decal-occluder %3 on %4 at %5 | B mesh-occluder strip %6 on %7 at %8 | C control strip %9",
			m_iDone, SETS, rA, hitA, pa, rB, hitB, pb, rC) + string.Format(" on %1 at %2 (1 = made, 0 = failed, -1 = no ground)", hitC, pc), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! first thing straight down (vehicle excluded): its point + class name
	protected bool Ground(vector p, IEntity veh, out vector hit, out string what)
	{
		m_Trace.Start = p + Vector(0, 3, 0);
		m_Trace.End = p - Vector(0, 3, 0);
		m_Trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		m_Trace.Exclude = veh;
		m_Trace.TraceEnt = null;
		float frac = GetGame().GetWorld().TraceMove(m_Trace, null);
		what = "none";
		if (frac >= 1)
			return false;
		hit = m_Trace.Start + (m_Trace.End - m_Trace.Start) * frac;
		if (m_Trace.TraceEnt)
			what = m_Trace.TraceEnt.ClassName();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnPrefab(ResourceName prefab, vector p, vector fwd, float now)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("MUD grass cut: prefab not loaded " + prefab, LogLevel.WARNING);
			return null;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = Vector(fwd[2], 0, -fwd[0]);
		params.Transform[1] = vector.Up;
		params.Transform[2] = fwd;
		params.Transform[3] = p;
		IEntity ent = GetGame().SpawnEntityPrefabLocal(res, GetGame().GetWorld(), params);
		if (ent)
			Keep(ent, now);
		return ent;
	}

	//------------------------------------------------------------------------------------------------
	//! STRIP_W x STRIP_L dirt quad along fwd round mid, corners LIFT over the traced ground. Verts are in the entity's
	//! space (SpawnPrefab turns the prefab entity to fwd; the control is unturned) - so build them from its transform.
	protected bool Strip(IEntity ent, vector mid, vector fwd, vector side, IEntity veh)
	{
		vector mat[4];
		ent.GetWorldTransform(mat);
		vector verts[4];
		float uvs[] = {0, 0, 1, 0, 0, 1, 1, 1};
		for (int j = 0; j < 4; j++)
		{
			float sx = STRIP_W * 0.5;
			if (j % 2 == 0)
				sx = -sx;
			float sz = -STRIP_L * 0.5;
			if (j >= 2)
				sz = -sz;
			vector w = mid + side * sx + fwd * sz;
			vector g;
			string unused;
			if (Ground(w, veh, g, unused))
				w[1] = g[1];
			w[1] = w[1] + LIFT;
			vector rel = w - mat[3];
			verts[j] = Vector(vector.Dot(rel, mat[0]), vector.Dot(rel, mat[1]), vector.Dot(rel, mat[2]));
		}
		int idx[] = {0, 2, 1, 1, 2, 3};
		int numVerts[] = {4};
		int numIndices[] = {6};
		string materials[] = {MAT};
		Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!res || !res.GetResource())
			return false;
		MeshObject mesh = res.GetResource().ToMeshObject();
		mesh.UpdateIndices(0, idx);       // indices first: UpdateVerts rebuilds normals from the triangle list
		mesh.UpdateVerts(0, verts, uvs);
		ent.SetObject(mesh, "");
		ent.Update();
		m_aRes.Insert(res);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Keep(IEntity ent, float now)
	{
		m_aEnts.Insert(ent);
		m_aDieAt.Insert(now + LIFE);
	}

	//------------------------------------------------------------------------------------------------
	protected void Expire(float now)
	{
		for (int i = m_aEnts.Count() - 1; i >= 0; i--)
		{
			if (now < m_aDieAt[i])
				continue;
			if (m_aEnts[i])
				SCR_EntityHelper.DeleteEntityAndChildren(m_aEnts[i]);
			m_aEnts.Remove(i);
			m_aDieAt.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static int B(bool b)
	{
		if (b)
			return 1;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! new session: forget all (the world and its entities are gone)
	static void Reset()
	{
		s_Inst = null;
	}
}
