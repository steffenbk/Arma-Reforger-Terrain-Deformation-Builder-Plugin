[EntityEditorProps(category: "Mud", description: "Mud plan 3: inside this box (XZ footprint, rotation counts) the world ground digs down to Max Depth instead of the ground's own Max Depth. Overlapping boxes: the deepest wins. Read when a fine patch is made.", color: "80 160 255 255")]
class MUD_DepthBoxEntityClass : GenericEntityClass
{
}

class MUD_DepthBoxEntity : GenericEntity
{
	[Attribute("8 4 8", UIWidgets.Coords, "Box size (m): X width, Y height (editor view only), Z length. Centred on the entity.")]
	protected vector m_vSize;

	[Attribute("8", UIWidgets.EditBox, "Max Depth inside the box (m below the undeformed surface)")]
	protected float m_fMaxDepth;

	protected static ref array<MUD_DepthBoxEntity> s_aAll = {};   // weak: a deleted box reads null

	//------------------------------------------------------------------------------------------------
	void MUD_DepthBoxEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame().InPlayMode())
			return;
		for (int i = s_aAll.Count() - 1; i >= 0; i--)
		{
			if (!s_aAll[i])
				s_aAll.Remove(i);
		}
		s_aAll.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Max Depth at world wp: the deepest box holding wp, else def
	static float DepthAt(vector wp, float def)
	{
		float best = -1;
		foreach (MUD_DepthBoxEntity b : s_aAll)
		{
			if (!b)
				continue;
			vector lp = b.CoordToLocal(wp);
			if (Math.AbsFloat(lp[0]) > b.m_vSize[0] * 0.5 || Math.AbsFloat(lp[2]) > b.m_vSize[2] * 0.5)
				continue;
			if (b.m_fMaxDepth > best)
				best = b.m_fMaxDepth;
		}
		if (best < 0)
			return def;
		return best;
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
		vector mins = m_vSize * -0.5;
		vector maxs = m_vSize * 0.5;
		Shape box = Shape.Create(ShapeType.BBOX, ARGB(255, 80, 160, 255), ShapeFlags.WIREFRAME | ShapeFlags.ONCE, mins, maxs);
		vector mat[4];
		GetWorldTransform(mat);
		box.SetMatrix(mat);
		super._WB_AfterWorldUpdate(timeSlice);
	}
#endif
}
