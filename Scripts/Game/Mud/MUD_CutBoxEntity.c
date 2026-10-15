[EntityEditorProps(category: "Mud", description: "Box where mud grounds leave a hole, so the real terrain shows. Only its XZ footprint counts (rotation too). Read once when the ground starts.", color: "255 80 0 255")]
class MUD_CutBoxEntityClass : GenericEntityClass
{
}

//! Hand-placed hole in every MUD_GroundEntity under it, for anything the automatic building cut
//! misses (MUD_GroundEntity.CollectCuts finds it by class).
class MUD_CutBoxEntity : GenericEntity
{
	[Attribute("4 3 4", UIWidgets.Coords, "Box size (m): X width, Y height (editor view only), Z length. Centred on the entity.")]
	protected vector m_vSize;

	//------------------------------------------------------------------------------------------------
	//! Box in this entity's local space
	void GetBox(out vector mins, out vector maxs)
	{
		mins = m_vSize * -0.5;
		maxs = m_vSize * 0.5;
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
		vector mins, maxs;
		GetBox(mins, maxs);
		Shape box = Shape.Create(ShapeType.BBOX, ARGB(255, 255, 80, 0), ShapeFlags.WIREFRAME | ShapeFlags.ONCE, mins, maxs);
		vector mat[4];
		GetWorldTransform(mat);
		box.SetMatrix(mat);

		super._WB_AfterWorldUpdate(timeSlice);
	}
#endif
}
