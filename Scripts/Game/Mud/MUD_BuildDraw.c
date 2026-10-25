//! Trench builder F4: local see-through marks lying on the ground - ribbons (area outlines, planned paths) and
//! squares (capsules of roofed parts, the manager's aim). Same materials and pattern as MUD_TrenchGhosts (procedural
//! MeshObject per entity, entities reused between draws). Nothing replicated, nothing on a dedicated server.
//! Use: Begin(); Ribbon(...) / Square(...); End() - End removes what this draw did not use.
class MUD_BuildDraw
{
	static const int STEPS = 16;            // sections per ribbon chunk - 1 (MeshObject wants exact static arrays)
	static const float CHUNK = 20;          // m of ribbon per entity at most
	static const int MARK = 10;             // state: solid bright orange (stake lines, flag poles; round 2, 2026-10-24)
	static const int FLAG = 11;             // state: solid red (flag cloth)
	static const ResourceName MAT_MARK = "{4D5544424D4B0001}Assets/Mud/MUD_BuildMark.emat";
	static const ResourceName MAT_FLAG = "{4D5544424D4B0002}Assets/Mud/MUD_BuildFlag.emat";

	protected ref array<IEntity> m_aEnts = {};
	protected ref array<ref Resource> m_aRes = {};  // mesh of each entity (a mesh goes with its last ref)
	protected int m_iUsed;
	protected static bool s_bWarned;

	//------------------------------------------------------------------------------------------------
	void ~MUD_BuildDraw()
	{
		Clear();
	}

	//------------------------------------------------------------------------------------------------
	void Begin()
	{
		m_iUsed = 0;
	}

	//------------------------------------------------------------------------------------------------
	void End()
	{
		Trim(m_iUsed);
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		Trim(0);
		m_iUsed = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void Trim(int keep)
	{
		for (int i = m_aEnts.Count() - 1; i >= keep; i--)
		{
			if (m_aEnts[i])
				SCR_EntityHelper.DeleteEntityAndChildren(m_aEnts[i]);
			m_aEnts.Remove(i);
			m_aRes.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName MaterialOf(int state)
	{
		if (state == MARK)
			return MAT_MARK;
		if (state == FLAG)
			return MAT_FLAG;
		if (state == MUD_TrenchGhosts.RED)
			return MUD_TrenchGhosts.MAT_RED;
		if (state == MUD_TrenchGhosts.DUG)
			return MUD_TrenchGhosts.MAT_DUG;
		return MUD_TrenchGhosts.MAT_GREEN;
	}

	//------------------------------------------------------------------------------------------------
	//! Ground top now at (x, z): our heightfield, else the terrain
	static float GroundY(float x, float z)
	{
		float y;
		if (MUD_WorldGroundEntity.SurfaceY(x, z, y))
			return y;
		return GetGame().GetWorld().GetSurfaceY(x, z);
	}

	//------------------------------------------------------------------------------------------------
	//! A band width m wide along pts (y ignored; closed = back to the first point), lift m over the ground
	void Ribbon(array<vector> pts, float width, int state, bool closed, float lift = 0.05)
	{
		if (RplSession.Mode() == RplMode.Dedicated || !pts)
			return;
		array<vector> line = {};
		foreach (vector p : pts)
		{
			line.Insert(Vector(p[0], 0, p[2]));
		}
		if (closed && line.Count() > 2)
			line.Insert(line[0]);
		if (line.Count() < 2)
			return;
		float total = MUD_Build.PolyLength(line);
		if (total < 0.01)
			return;
		int chunks = Math.Ceil(total / CHUNK);
		if (chunks < 1)
			chunks = 1;
		for (int c = 0; c < chunks; c++)
		{
			Chunk(line, total * c / chunks, total * (c + 1) / chunks, width * 0.5, state, lift);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Chunk(array<vector> line, float s0, float s1, float half, int state, float lift)
	{
		vector dir;
		vector mid = MUD_Build.PolyPoint(line, (s0 + s1) * 0.5, dir);
		mid[1] = GroundY(mid[0], mid[2]);

		// MeshObject takes static arrays (array<T> = compile error)
		vector verts[34];
		float uvs[68];
		for (int k = 0; k <= STEPS; k++)
		{
			float s = s0 + (s1 - s0) * k / STEPS;
			vector p = MUD_Build.PolyPoint(line, s, dir);
			vector side = Vector(-dir[2], 0, dir[0]) * half;
			vector l = p - side;
			vector r = p + side;
			l[1] = GroundY(l[0], l[2]) + lift;
			r[1] = GroundY(r[0], r[2]) + lift;
			verts[k * 2] = l - mid;
			verts[k * 2 + 1] = r - mid;
			float v = k * (1.0 / STEPS);     // k / STEPS alone is int division
			uvs[k * 4] = 0;
			uvs[k * 4 + 1] = v;
			uvs[k * 4 + 2] = 1;
			uvs[k * 4 + 3] = v;
		}
		int idx[96];
		for (int q = 0; q < STEPS; q++)
		{
			int a = q * 2;
			idx[q * 6] = a;
			idx[q * 6 + 1] = a + 2;
			idx[q * 6 + 2] = a + 1;
			idx[q * 6 + 3] = a + 1;
			idx[q * 6 + 4] = a + 2;
			idx[q * 6 + 5] = a + 3;
		}
		int numVerts[] = {34};
		int numIndices[] = {96};
		string materials[] = {MaterialOf(state)};
		Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!res || !res.GetResource())
		{
			Warn(state);
			return;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		// indices first: UpdateVerts rebuilds normals from the triangle list
		mesh.UpdateIndices(0, idx);
		mesh.UpdateVerts(0, verts, uvs);
		Place(res, mesh, mid);
	}

	//------------------------------------------------------------------------------------------------
	//! A square 2 x half wide round mid (y ignored), turned yaw deg, lift m over the ground
	void Square(vector mid, float half, float yaw, int state, float lift = 0.04)
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;
		vector c = Vector(mid[0], GroundY(mid[0], mid[2]), mid[2]);
		float a = yaw * Math.DEG2RAD;
		vector fwd = Vector(Math.Sin(a), 0, Math.Cos(a)) * half;
		vector right = Vector(fwd[2], 0, -fwd[0]);
		vector corners[4];
		corners[0] = c - fwd - right;
		corners[1] = c - fwd + right;
		corners[2] = c + fwd - right;
		corners[3] = c + fwd + right;
		vector verts[4];
		float uvs[8];
		for (int i = 0; i < 4; i++)
		{
			vector p = corners[i];
			p[1] = GroundY(p[0], p[2]) + lift;
			verts[i] = p - c;
		}
		uvs[0] = 0;
		uvs[1] = 0;
		uvs[2] = 1;
		uvs[3] = 0;
		uvs[4] = 0;
		uvs[5] = 1;
		uvs[6] = 1;
		uvs[7] = 1;
		int idx[6];
		idx[0] = 0;
		idx[1] = 2;
		idx[2] = 1;
		idx[3] = 1;
		idx[4] = 2;
		idx[5] = 3;
		int numVerts[] = {4};
		int numIndices[] = {6};
		string materials[] = {MaterialOf(state)};
		Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!res || !res.GetResource())
		{
			Warn(state);
			return;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		mesh.UpdateIndices(0, idx);
		mesh.UpdateVerts(0, verts, uvs);
		Place(res, mesh, c);
	}

	//------------------------------------------------------------------------------------------------
	//! A marker flag standing on the ground at base: an orange pole height m tall (two crossed thin quads, seen from
	//! every side) and a red pennant at its top (two crossed triangles). Two entities.
	void Flag(vector base, float height)
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;
		vector c = Vector(base[0], GroundY(base[0], base[2]), base[2]);
		float w = Math.Max(0.03, height * 0.025);
		vector pv[8];
		pv[0] = Vector(-w, 0, 0);
		pv[1] = Vector(w, 0, 0);
		pv[2] = Vector(-w, height, 0);
		pv[3] = Vector(w, height, 0);
		pv[4] = Vector(0, 0, -w);
		pv[5] = Vector(0, 0, w);
		pv[6] = Vector(0, height, -w);
		pv[7] = Vector(0, height, w);
		int pi[12];
		for (int q = 0; q < 2; q++)
		{
			int a = q * 4;
			pi[q * 6] = a;
			pi[q * 6 + 1] = a + 2;
			pi[q * 6 + 2] = a + 1;
			pi[q * 6 + 3] = a + 1;
			pi[q * 6 + 4] = a + 2;
			pi[q * 6 + 5] = a + 3;
		}
		float puv[16];
		for (int u = 0; u < 16; u++)
		{
			puv[u] = 0;
		}
		int poleVerts[] = {8};
		int poleIdx[] = {12};
		string poleMat[] = {MAT_MARK};
		Resource pole = MeshObject.Create(1, poleVerts, poleIdx, poleMat, 0);
		if (!pole || !pole.GetResource())
		{
			Warn(MARK);
			return;
		}
		MeshObject poleMesh = pole.GetResource().ToMeshObject();
		poleMesh.UpdateIndices(0, pi);
		poleMesh.UpdateVerts(0, pv, puv);
		Place(pole, poleMesh, c);

		float f = height * 0.32;
		vector fv[6];
		fv[0] = Vector(0, height, 0);
		fv[1] = Vector(0, height - f, 0);
		fv[2] = Vector(f * 1.4, height - f * 0.5, 0);
		fv[3] = Vector(0, height, 0);
		fv[4] = Vector(0, height - f, 0);
		fv[5] = Vector(0, height - f * 0.5, f * 1.4);
		int fi[6];
		for (int k = 0; k < 6; k++)
		{
			fi[k] = k;
		}
		float fuv[12];
		for (int m = 0; m < 12; m++)
		{
			fuv[m] = 0;
		}
		int flagVerts[] = {6};
		int flagIdx[] = {6};
		string flagMat[] = {MAT_FLAG};
		Resource cloth = MeshObject.Create(1, flagVerts, flagIdx, flagMat, 0);
		if (!cloth || !cloth.GetResource())
		{
			Warn(FLAG);
			return;
		}
		MeshObject clothMesh = cloth.GetResource().ToMeshObject();
		clothMesh.UpdateIndices(0, fi);
		clothMesh.UpdateVerts(0, fv, fuv);
		Place(cloth, clothMesh, c);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Warn(int state)
	{
		if (!s_bWarned)
			Print(string.Format("MUD build draw: MeshObject.Create failed - material '%1' did not load?", MaterialOf(state)), LogLevel.ERROR);
		s_bWarned = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Next entity slot (reused or new) gets mesh at mid
	protected void Place(Resource res, MeshObject mesh, vector mid)
	{
		int slot = m_iUsed;
		IEntity ent;
		if (slot < m_aEnts.Count())
			ent = m_aEnts[slot];
		if (!ent)
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[0] = vector.Right;
			params.Transform[1] = vector.Up;
			params.Transform[2] = vector.Forward;
			params.Transform[3] = mid;
			ent = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
			if (!ent)
				return;
			if (slot < m_aEnts.Count())
			{
				m_aEnts[slot] = ent;
			}
			else
			{
				m_aEnts.Insert(ent);
				m_aRes.Insert(null);
			}
		}
		ent.SetOrigin(mid);
		ent.SetObject(mesh, "");
		ent.Update();       // fresh bounds, else culling keeps the spawn-time ones
		m_aRes[slot] = res;
		m_iUsed++;
	}
}
