[EntityEditorProps(category: "Mud", description: "Mud zone: the visual mud layer (ruts, ridges, drag) is built over this box at map load. Axis-aligned, rotation ignored.", color: "120 80 40 255")]
class MUD_MudZoneEntityClass : GenericEntityClass
{
}

//! Placed mud area (plan docs/superpowers/plans/2026-10-14-mud-zones-plan.md). MUD_MudSheet builds every 3 m tile
//! whose middle is inside a zone at load and only draws mud there.
class MUD_MudZoneEntity : GenericEntity
{
	[Attribute("64", UIWidgets.EditBox, "Zone size along world X (m)", category: "Mud zone")]
	protected float m_fSizeX;

	[Attribute("64", UIWidgets.EditBox, "Zone size along world Z (m)", category: "Mud zone")]
	protected float m_fSizeZ;

	protected static ref array<MUD_MudZoneEntity> s_aAll = {};

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
		if (!MUD_MudSheet.ENABLED || s_aAll.IsEmpty())
			return false;
		float t = MUD_MudSheet.TILE_M;
		float mx = (Math.Floor(x / t) + 0.5) * t;
		float mz = (Math.Floor(z / t) + 0.5) * t;
		BaseWorld w = GetGame().GetWorld();
		foreach (MUD_MudZoneEntity zone : s_aAll)
		{
			if (zone && zone.GetWorld() == w && zone.Contains(mx, mz))
				return true;
		}
		return false;
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
