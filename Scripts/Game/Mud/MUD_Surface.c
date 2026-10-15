//! Heights on the holed map (mud plan 3). GetSurfaceY is -256 on a hole cell; nothing here returns that.
class MUD_Surface
{
	protected static const float TOP = 400;
	protected static const float BOTTOM = -300;

	protected static ref MUD_Surface s_Inst;
	protected ref TraceParam m_Trace = new TraceParam();
	protected float m_fLastGood;

	//! Trace found no terrain on a hole cell (last good height returned instead)
	int m_iMiss;
	//! Still on a non-terrain WORLD hit after 4 retraces (its height returned)
	int m_iNonTerrain;

	//------------------------------------------------------------------------------------------------
	static MUD_Surface Get()
	{
		if (!s_Inst)
			s_Inst = new MUD_Surface();
		return s_Inst;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsHole(notnull BaseWorld world, float x, float z)
	{
		return world.GetSurfaceY(x, z) < -255;
	}

	//------------------------------------------------------------------------------------------------
	//! GetSurfaceY that never returns -256: on a hole cell our ground top (Y) instead. Kept terrain costs
	//! nothing extra. For vanilla code that reads GetSurfaceY (GM camera, GM placing, MUD_VanillaHoleFix.c).
	static float SafeY(notnull BaseWorld world, float x, float z)
	{
		float s = world.GetSurfaceY(x, z);
		if (s > -255)
			return s;
		return Get().Y(world, x, z);
	}

	//------------------------------------------------------------------------------------------------
	//! Our ground entities: skin chunks (and fine patches, Task 4; voxel bricks, TEMP probe 2026-10-13)
	static bool IsOurs(IEntity e)
	{
		return MUD_CoarseChunkEntity.Cast(e) != null || MUD_FinePatchEntity.Cast(e) != null || MUD_VoxelSubEntity.Cast(e) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Heightmap height: WORLD trace (hits the heightmap inside holes too), terrain only, retraced from
	//! 1 cm under a road (the filter does not stop WORLD road geometry, plan 2). GetSurfaceY if nothing hit
	//! on kept terrain; on a hole cell with no hit, the last good height (counted in m_iMiss), like
	//! MUD_CoarseSkinEntity.HeightAt.
	float HeightmapY(notnull BaseWorld world, float x, float z)
	{
		m_Trace.Start = Vector(x, TOP, z);
		m_Trace.End = Vector(x, BOTTOM, z);
		m_Trace.Flags = TraceFlags.WORLD;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_Trace.TraceEnt = null;
		float frac = world.TraceMove(m_Trace, TerrainOnly);
		if (frac >= 1)
		{
			float s = world.GetSurfaceY(x, z);
			if (s > -255)
				return s;
			m_iMiss++;
			return m_fLastGood;
		}
		float y = TOP + (BOTTOM - TOP) * frac;
		for (int r = 0; r < 4 && m_Trace.TraceEnt && !GenericTerrainEntity.Cast(m_Trace.TraceEnt); r++)
		{
			m_Trace.Start = Vector(x, y - 0.01, z);
			m_Trace.TraceEnt = null;
			frac = world.TraceMove(m_Trace, TerrainOnly);
			if (frac >= 1)
				break;
			y = m_Trace.Start[1] + (BOTTOM - m_Trace.Start[1]) * frac;
		}
		if (m_Trace.TraceEnt && !GenericTerrainEntity.Cast(m_Trace.TraceEnt))
			m_iNonTerrain++;
		m_fLastGood = y;
		return y;
	}

	//------------------------------------------------------------------------------------------------
	//! Top of our ground at x, z (skin chunk or fine patch, by ENTS | WORLD trace); the heightmap height
	//! where the first hit is something else (kept terrain, a road, a WORLD static)
	float Y(notnull BaseWorld world, float x, float z)
	{
		// Fine patch first: in a trench a vertical trace hit something of ours 1.2-1.5 m above the dug floor
		// (lightning probe 2026-10-12 21:48, floor = MUD_FinePatchEntity), so the GM camera stopped early.
		m_Trace.Start = Vector(x, TOP, z);
		m_Trace.End = Vector(x, BOTTOM, z);
		m_Trace.Flags = TraceFlags.ENTS;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_Trace.TraceEnt = null;
		float fineFrac = world.TraceMove(m_Trace, FineOnly);
		if (fineFrac < 1 && MUD_FinePatchEntity.Cast(m_Trace.TraceEnt))
			return TOP + (BOTTOM - TOP) * fineFrac;

		m_Trace.Start = Vector(x, TOP, z);
		m_Trace.End = Vector(x, BOTTOM, z);
		m_Trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_Trace.TraceEnt = null;
		float frac = world.TraceMove(m_Trace, OurGround);
		if (frac < 1 && IsOurs(m_Trace.TraceEnt))
			return TOP + (BOTTOM - TOP) * frac;
		return HeightmapY(world, x, z);
	}

	//------------------------------------------------------------------------------------------------
	protected bool OurGround(notnull IEntity e)
	{
		return IsOurs(e);
	}

	//------------------------------------------------------------------------------------------------
	protected bool FineOnly(notnull IEntity e)
	{
		return MUD_FinePatchEntity.Cast(e) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TerrainOnly(notnull IEntity e)
	{
		return GenericTerrainEntity.Cast(e) != null;
	}
}
