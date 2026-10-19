[ComponentEditorProps(category: "Mud", description: "Fake crater on kept ground (roads, asphalt, concrete): scorch decal + concrete chunks. Spawned by the server; needs an RplComponent.")]
class MUD_RoadCraterComponentClass : ScriptComponentClass
{
}

//! Road crater (option A, user 2026-10-18): our ground can't dig kept terrain (roads, asphalt / concrete gamemat,
//! building feet - terrain can't be holed at runtime), so a blast there leaves a scorch decal and a ring of concrete
//! chunks instead. The server spawns one replicated MudRoadCrater.et per blast (radius + seed as RplProps, as
//! MUD_TrenchSite); every machine lays the same pieces LOCALLY from the seed (RandomGenerator), so the chunk colliders
//! match on server and clients. Dedicated server: chunks only (vehicle physics), no decal.
class MUD_RoadCraterComponent : ScriptComponent
{
	static const bool ENABLED = false;    // user 2026-10-18: no debris - wants real deep craters on roads (option B, MUD_RoadHideProbe first)
	static const ResourceName PREFAB = "{4D5552524F414430}Prefabs/Mud/MudRoadCrater.et";
	static const ResourceName DECAL = "{4D5552524F414440}Prefabs/Mud/MudRoadCraterDecal.et";
	static const int MAX = 150;             // server-wide; the oldest goes first
	static const float RADIUS_SHARE = 1.2;  // chunk ring radius / crater depth this charge would dig on soft ground
	static const float DECAL_SHARE = 2.4;   // decal size / ring radius
	static const float CHUNK_SCALE_MIN = 0.25;  // chunk size (vanilla DebrisPile_Concrete_01 pieces), x radius / 1.5
	static const float CHUNK_SCALE_MAX = 0.55;

	[RplProp(onRplName: "OnRpl")]
	protected float m_fRadius;
	[RplProp(onRplName: "OnRpl")]
	protected int m_iSeed;

	protected bool m_bBuilt;
	protected ref array<IEntity> m_aPieces = {};

	protected static ref array<IEntity> s_aServer = {};     // server: oldest first

	//------------------------------------------------------------------------------------------------
	//! Chunk prefab i (0-3)
	protected static ResourceName Chunk(int i)
	{
		if (i == 1)
			return "{4D5552524F414252}Prefabs/Mud/MudRoadChunk_V2.et";
		if (i == 2)
			return "{4D5552524F414253}Prefabs/Mud/MudRoadChunk_V3.et";
		if (i == 3)
			return "{4D5552524F414254}Prefabs/Mud/MudRoadChunk_V4.et";
		return "{4D5552524F414251}Prefabs/Mud/MudRoadChunk_V1.et";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a blast at wp on kept ground; depth = what this charge digs on soft ground (MUD_WorldGround FullDepth)
	static void Spawn(vector wp, float depth, bool dbg)
	{
		if (!ENABLED || !Replication.IsServer())
			return;
		Resource res = Resource.Load(PREFAB);
		if (!res || !res.IsValid())
		{
			Print("MUD road crater: prefab will not load: " + PREFAB, LogLevel.WARNING);
			return;
		}
		BaseWorld world = GetGame().GetWorld();
		vector origin = wp;
		origin[1] = world.GetSurfaceY(wp[0], wp[2]);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = origin;
		IEntity ent = GetGame().SpawnEntityPrefab(res, world, params);
		if (!ent)
			return;
		MUD_RoadCraterComponent rc = MUD_RoadCraterComponent.Cast(ent.FindComponent(MUD_RoadCraterComponent));
		if (!rc)
		{
			RplComponent.DeleteRplEntity(ent, false);
			return;
		}
		rc.Set(Math.Max(0.6, depth * RADIUS_SHARE), Math.RandomInt(1, 1000000));
		s_aServer.RemoveItem(null);
		s_aServer.Insert(ent);
		while (s_aServer.Count() > MAX)
		{
			IEntity old = s_aServer[0];
			s_aServer.RemoveOrdered(0);
			if (old)
				RplComponent.DeleteRplEntity(old, false);
		}
		if (dbg)
			Print(string.Format("MUD road crater: at %1, radius %2 m (depth %3), %4 live", origin, rc.m_fRadius, depth, s_aServer.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server, right after spawn
	protected void Set(float radius, int seed)
	{
		m_fRadius = radius;
		m_iSeed = seed;
		Replication.BumpMe();
		Build();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		// a joining client may get the props before init without the onRpl call
		Build();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRpl()
	{
		Build();
	}

	//------------------------------------------------------------------------------------------------
	//! Lays the decal + chunks once both props are in. The same seed gives the same pieces on every machine.
	protected void Build()
	{
		if (m_bBuilt || m_fRadius <= 0 || m_iSeed == 0)
			return;
		IEntity owner = GetOwner();
		if (!owner)
			return;
		// chunks go only on kept cells (static map data): wait for the skin, else a JIP client could lay a different set
		MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
		if (!skin || !skin.IsReady())
		{
			GetGame().GetCallqueue().CallLater(Build, 500);
			return;
		}
		m_bBuilt = true;
		BaseWorld world = owner.GetWorld();
		vector origin = owner.GetOrigin();
		RandomGenerator rng = new RandomGenerator();
		rng.SetSeed(m_iSeed);
		float yaw = rng.RandFloatXY(0, 360);
		if (RplSession.Mode() != RplMode.Dedicated)
			Place(world, DECAL, origin, yaw, 0, m_fRadius * DECAL_SHARE);
		int n = Math.Round(4 + m_fRadius * 2);
		n = Math.ClampInt(n, 4, 10);
		float size = Math.Clamp(m_fRadius / 1.5, 0.6, 1.6);
		for (int i = 0; i < n; i++)
		{
			float a = rng.RandFloatXY(0, Math.PI2);
			float r = m_fRadius * rng.RandFloatXY(0.45, 1.15);
			vector p = origin + Vector(Math.Cos(a) * r, 0, Math.Sin(a) * r);
			p[1] = world.GetSurfaceY(p[0], p[2]) - 0.05;
			int v = rng.RandInt(0, 4);
			float sc = rng.RandFloatXY(CHUNK_SCALE_MIN, CHUNK_SCALE_MAX) * size;
			float tilt = rng.RandFloatXY(-15, 15);
			float turn = rng.RandFloatXY(0, 360);
			// off the road (a hole cell = our ground, maybe dug): no chunk - every draw above is still made, same order
			if (KeptAt(skin, p[0], p[2]))
				Place(world, Chunk(v), p, turn, tilt, sc);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! (x, z) is on kept terrain (not a skin hole cell): the real terrain is the ground there
	protected static bool KeptAt(MUD_CoarseSkinEntity skin, float x, float z)
	{
		float x0;
		float z0;
		int ncx;
		int ncz;
		skin.GetGrid(x0, z0, ncx, ncz);
		float cell = skin.GetCell();
		int cx = Math.Floor((x - x0) / cell);
		int cz = Math.Floor((z - z0) / cell);
		return !skin.IsHoleCell(cx, cz);
	}

	//------------------------------------------------------------------------------------------------
	protected void Place(BaseWorld world, ResourceName prefab, vector pos, float yaw, float tilt, float scale)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(yaw, tilt, 0), params.Transform);
		params.Transform[3] = pos;
		params.Scale = scale;
		IEntity e = GetGame().SpawnEntityPrefabLocal(res, world, params);
		if (e)
			m_aPieces.Insert(e);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Build);
		foreach (IEntity e : m_aPieces)
		{
			if (e)
				SCR_EntityHelper.DeleteEntityAndChildren(e);
		}
		m_aPieces.Clear();
		super.OnDelete(owner);
	}
}
