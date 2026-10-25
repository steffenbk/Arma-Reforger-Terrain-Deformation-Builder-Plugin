//! Trench ghosts (spec section 7): one see-through slab per piece, lying 2 cm over the ground on the
//! piece footprint (5 sections along it). Local entities, nothing replicated, none on a dedicated
//! server. Entities are reused between draws; each draw makes fresh meshes.
class MUD_TrenchGhosts
{
	static const ResourceName MAT_GREEN = "{4D55445452454E34}Assets/Mud/MUD_TrenchGhost.emat";
	static const ResourceName MAT_RED = "{4D55445452454E35}Assets/Mud/MUD_TrenchGhostRed.emat";
	static const ResourceName MAT_DUG = "{4D55445452454E36}Assets/Mud/MUD_TrenchGhostDug.emat";
	static const int HIDDEN = 0;
	static const int GREEN = 1;     // can be dug
	static const int RED = 2;       // skipped: kept cell (road, ...) - plan mode only
	static const int DUG = 3;       // site: some layers dug
	static const float LIFT = 0.02;

	protected ref array<IEntity> m_aEnts = {};
	protected ref array<ref Resource> m_aRes = {};  // mesh of each entity (a mesh goes with its last ref)
	protected static bool s_bWarned;
	protected ref TraceParam m_Trace;               // our ground over dug voxels (GroundY)
	//------------------------------------------------------------------------------------------------
	void ~MUD_TrenchGhosts()
	{
		Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! One slab per piece whose state is not HIDDEN (states[i] for piece i; missing = HIDDEN)
	void Draw(MUD_TrenchLayout lay, array<int> states)
	{
		if (!lay || RplSession.Mode() == RplMode.Dedicated)
		{
			Clear();
			return;
		}
		int used;
		for (int i = 0; i < lay.Count(); i++)
		{
			int state = HIDDEN;
			if (i < states.Count())
				state = states[i];
			if (state == HIDDEN)
				continue;
			if (DrawPiece(used, lay, i, state))
				used++;
		}
		Trim(used);
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		Trim(0);
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
	protected static ResourceName MaterialOf(int state)
	{
		if (state == RED)
			return MAT_RED;
		if (state == DUG)
			return MAT_DUG;
		return MAT_GREEN;
	}

	//------------------------------------------------------------------------------------------------
	protected float GroundY(float x, float z)
	{
		float y;
		// over dug voxels our heightfield and the heightmap both still say undug: read our ground entities
		// (2026-10-15), and lie on top of the mud sheet (ghost was hidden under the 0.15 m mud).
		// Own trace since 2026-10-15 17:2x: MudSheet.OurGroundY needs the sheet, and with the sheet parked the
		// ghost fell to our heightfield, sunk 4 m under voxel ground (user: no preview inside a crater)
		// LOOK LIFT (MUD_LookLift, 2026-10-23): the trace / heightfield give the collider; the look can lie higher
		float lift = 0;
		if (OurGroundY(x, z, y))
		{
			if (!MUD_VoxelSubEntity.Cast(m_Trace.TraceEnt))
				lift = MUD_LookLift.DrawnLift(x, z);     // voxel bricks are never lifted
		}
		else if (MUD_WorldGroundEntity.SurfaceY(x, z, y))
		{
			lift = MUD_LookLift.DrawnLift(x, z);
		}
		else
		{
			y = GetGame().GetWorld().GetSurfaceY(x, z);
		}
		return y + MUD_MudSheet.MudOver(x, z) + lift;
	}

	//------------------------------------------------------------------------------------------------
	//! Our ground top on a hole cell: ENTS-only trace that sees only our ground entities (voxel bricks,
	//! patches), as MUD_MudSheet.OurY; false on kept terrain
	protected bool OurGroundY(float x, float z, out float y)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!MUD_Surface.IsHole(world, x, z))
			return false;
		float top = MUD_Surface.Get().Y(world, x, z) + 10;
		if (!m_Trace)
			m_Trace = new TraceParam();
		m_Trace.Start = Vector(x, top, z);
		m_Trace.End = Vector(x, top - 60, z);
		m_Trace.Flags = TraceFlags.ENTS;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_Trace.TraceEnt = null;
		float f = world.TraceMove(m_Trace, OnlyOurs);
		if (f >= 1)
			return false;
		y = top - 60 * f;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OnlyOurs(notnull IEntity e)
	{
		return MUD_Surface.IsOurs(e);
	}

	//------------------------------------------------------------------------------------------------
	protected bool DrawPiece(int slot, MUD_TrenchLayout lay, int piece, int state)
	{
		vector mid = lay.PieceMiddle(piece);
		mid[1] = GroundY(mid[0], mid[2]);

		// MeshObject takes static arrays (array<T> = compile error)
		vector verts[10];
		float uvs[20];
		for (int j = 0; j < 5; j++)
		{
			vector l = lay.PieceEdge(piece, j * 0.25, -1);
			vector r = lay.PieceEdge(piece, j * 0.25, 1);
			l[1] = GroundY(l[0], l[2]) + LIFT;
			r[1] = GroundY(r[0], r[2]) + LIFT;
			verts[j * 2] = l - mid;
			verts[j * 2 + 1] = r - mid;
			uvs[j * 4] = 0;
			uvs[j * 4 + 1] = j * 0.25;
			uvs[j * 4 + 2] = 1;
			uvs[j * 4 + 3] = j * 0.25;
		}
		int idx[24];
		for (int q = 0; q < 4; q++)
		{
			int a = q * 2;
			idx[q * 6] = a;
			idx[q * 6 + 1] = a + 2;
			idx[q * 6 + 2] = a + 1;
			idx[q * 6 + 3] = a + 1;
			idx[q * 6 + 4] = a + 2;
			idx[q * 6 + 5] = a + 3;
		}

		int numVerts[] = {10};
		int numIndices[] = {24};
		string materials[] = {MaterialOf(state)};
		Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!res || !res.GetResource())
		{
			if (!s_bWarned)
				Print(string.Format("MUD trench ghost: MeshObject.Create failed - material '%1' did not load?", MaterialOf(state)), LogLevel.ERROR);
			s_bWarned = true;
			return false;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		// indices first: UpdateVerts rebuilds normals from the triangle list
		mesh.UpdateIndices(0, idx);
		mesh.UpdateVerts(0, verts, uvs);

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
				return false;
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
		return true;
	}
}
