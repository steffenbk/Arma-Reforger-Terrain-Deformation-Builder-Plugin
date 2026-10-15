//! A destroyed building gathers every entity with physics in its box, parents it to itself, sinks
//! it with the ruin and deletes it (SCR_DestructibleBuildingComponent.DestroyInteriorInit /
//! AddEntityCallback). Our mud patches have physics and no parent, so a whole 8 m patch vanished
//! with the house (2026-10-08 16:45, user). Keep them out of that query.
modded class SCR_DestructibleBuildingComponent
{
	//------------------------------------------------------------------------------------------------
	override protected bool QueryFilterCallback(notnull IEntity entity)
	{
		if (MUD_GroundEntity.IsMudPatch(entity) || MUD_CoarseChunkEntity.Cast(entity) || MUD_FinePatchEntity.Cast(entity) || MUD_VoxelSubEntity.Cast(entity))
			return false;
		return super.QueryFilterCallback(entity);
	}
}
