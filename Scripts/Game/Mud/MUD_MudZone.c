[EntityEditorProps(category: "Mud", description: "Mud zone: the visual mud layer (ruts, ridges, drag) is built over this box at map load. Axis-aligned, rotation ignored.", color: "120 80 40 255")]
class MUD_MudZoneEntityClass : GenericEntityClass
{
}

//! Placed mud area (plan docs/superpowers/plans/old/2026-10-14-mud-zones-plan.md). MUD_MudSheet builds every 3 m tile
//! whose middle is inside a zone at load and only draws mud there.
class MUD_MudZoneEntity : GenericEntity
{
	[Attribute("64", UIWidgets.EditBox, "Zone size along world X (m)", category: "Mud zone")]
	protected float m_fSizeX;

	[Attribute("64", UIWidgets.EditBox, "Zone size along world Z (m)", category: "Mud zone")]
	protected float m_fSizeZ;

	protected static ref array<MUD_MudZoneEntity> s_aAll = {};
	// H3 TEST (2026-10-18, user: 'a test to cover all offroad terrain'; v200 = before): the sheet covers EVERY tile whose
	// middle is on holed ground (= offroad: real terrain stays only under asphalt / concrete, buildings, poles, roads -
	// project TERRAIN RULE), placed zones or not. The hole map is baked terrain = the same answer on every machine
	// (synced layer, ops). false = placed zones only, as before.
	static const bool ALL_OFFROAD = true;
	protected static ref map<int, int> s_mOffroad = new map<int, int>();    // ALL_OFFROAD: tile key -> 1 holed middle
	protected static int s_iOffroadAsked;                                    // log: tiles looked up / holed
	protected static int s_iOffroadHoled;

	//------------------------------------------------------------------------------------------------
	void MUD_MudZoneEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	void ~MUD_MudZoneEntity()
	{
		if (s_aAll)
			s_aAll.RemoveItem(this);
	}

	//------------------------------------------------------------------------------------------------
	override protected void EOnInit(IEntity owner)
	{
		if (s_aAll.Find(this) < 0)
			s_aAll.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Zones of the running game world (the editor world's own copies are skipped)
	static void GetInWorld(notnull array<MUD_MudZoneEntity> outZones)
	{
		outZones.Clear();
		BaseWorld w = GetGame().GetWorld();
		foreach (MUD_MudZoneEntity z : s_aAll)
		{
			if (z && z.GetWorld() == w)
				outZones.Insert(z);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True where the mud sheet has a zone tile (MUD_MudSheet builds every TILE_M tile whose middle is in a zone), so
	//! the sheet's own top look shows there and the voxels under it need no grass (user 2026-10-15)
	static bool SheetCovers(float x, float z)
	{
		if (!MUD_MudSheet.ENABLED || !MUD_MudSheet.SYNC)
			return false;     // MUD LAB sheet local (SYNC false): the sheet never changes rut behaviour
		float t = MUD_MudSheet.TILE_M;
		return TileCovered(Math.Floor(x / t), Math.Floor(z / t));
	}

	//------------------------------------------------------------------------------------------------
	//! Is world sheet tile tx, tz a mud tile? Its middle inside a placed zone, or (ALL_OFFROAD) on holed ground.
	//! MUD_MudSheet.TileInZone and MUD_MudSync.ZoneTile ask this, so sheet and synced layer agree.
	static bool TileCovered(int tx, int tz)
	{
		BaseWorld w = GetGame().GetWorld();
		if (!w)
			return false;
		float cx = (tx * MUD_MudSheet.TQ + MUD_MudSheet.TQ / 2) * MUD_MudSheet.CELL;
		float cz = (tz * MUD_MudSheet.TQ + MUD_MudSheet.TQ / 2) * MUD_MudSheet.CELL;
		foreach (MUD_MudZoneEntity zone : s_aAll)
		{
			if (zone && zone.GetWorld() == w && zone.Contains(cx, cz))
				return true;
		}
		if (!ALL_OFFROAD)
			return false;
		int key = MUD_MudSheet.TrailKey(tx, tz);
		int known;
		if (s_mOffroad.Find(key, known))
			return known != 0;
		int holed = 0;
		if (MUD_Surface.IsHole(w, cx, cz))
			holed = 1;
		s_mOffroad.Set(key, holed);
		s_iOffroadAsked++;
		s_iOffroadHoled += holed;
		return holed != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! ALL_OFFROAD: any box may hold mud tiles (the per-tile TileCovered decides). Else the caller checks its zones.
	static bool AnyBox()
	{
		return ALL_OFFROAD;
	}

	//------------------------------------------------------------------------------------------------
	//! Log: tiles looked up so far / holed ones (ALL_OFFROAD cache size)
	static string OffroadStats()
	{
		return string.Format("all offroad %1: tiles looked up %2, holed %3", ALL_OFFROAD, s_iOffroadAsked, s_iOffroadHoled);
	}

	//------------------------------------------------------------------------------------------------
	bool Contains(float x, float z)
	{
		vector o = GetOrigin();
		return Math.AbsFloat(x - o[0]) <= m_fSizeX * 0.5 && Math.AbsFloat(z - o[2]) <= m_fSizeZ * 0.5;
	}

	//------------------------------------------------------------------------------------------------
	void GetRect(out float x0, out float z0, out float x1, out float z1)
	{
		vector o = GetOrigin();
		x0 = o[0] - m_fSizeX * 0.5;
		x1 = o[0] + m_fSizeX * 0.5;
		z0 = o[2] - m_fSizeZ * 0.5;
		z1 = o[2] + m_fSizeZ * 0.5;
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(float timeSlice)
	{
		// world-axis box (the zone ignores rotation), 2 m high so it shows on the ground
		vector o = GetOrigin();
		vector mins = o + Vector(-m_fSizeX * 0.5, -1, -m_fSizeZ * 0.5);
		vector maxs = o + Vector(m_fSizeX * 0.5, 1, m_fSizeZ * 0.5);
		Shape.Create(ShapeType.BBOX, ARGB(255, 140, 90, 40), ShapeFlags.WIREFRAME | ShapeFlags.ONCE, mins, maxs);
		super._WB_AfterWorldUpdate(timeSlice);
	}
#endif
}
