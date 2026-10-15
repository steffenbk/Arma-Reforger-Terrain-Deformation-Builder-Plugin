#ifdef WORKBENCH
//! Lift the selected entities onto the mud surface, for maps that already exist: select the
//! houses, props, ... standing where a MUD_GroundEntity goes and run this. Same idea as the
//! shipped SCR_SnapAndOrientToTerrainPlugin (copied from it), but to the surface the mud builds in
//! play (terrain + pad shape, MUD_GroundEntity.MudSurfaceY). Keeps rotation. Undo works.
[WorkbenchPluginAttribute(
	name: "Snap to Mud",
	description: "Move the selected entities up onto the MUD ground surface under them",
	shortcut: "Ctrl+Alt+PageDown",
	wbModules: { "WorldEditor" },
	awesomeFontCode: 0xF2D1)]
class MUD_SnapToMudPlugin : WorkbenchPlugin
{
	protected ref array<IEntity> m_aFound = {};

	//------------------------------------------------------------------------------------------------
	protected bool AddEntity(IEntity e)
	{
		m_aFound.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Highest mud surface (world Y) over world point wp. False when no mud ground covers it.
	protected bool MudY(BaseWorld world, vector wp, out float y)
	{
		m_aFound.Clear();
		world.QueryEntitiesByAABB(wp - Vector(0.05, 200, 0.05), wp + Vector(0.05, 200, 0.05), AddEntity);
		bool found;
		foreach (IEntity e : m_aFound)
		{
			MUD_GroundEntity ground = MUD_GroundEntity.Cast(e);
			float gy;
			if (!ground || !ground.MudSurfaceY(wp, gy))
				continue;
			if (!found || gy > y)
				y = gy;
			found = true;
		}
		m_aFound.Clear();
		return found;
	}

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor)
			return;

		WorldEditorAPI api = worldEditor.GetApi();
		if (!api)
			return;

		BaseWorld world = api.GetWorld();
		if (!world)
			return;

		int moved, skipped;
		api.BeginEntityAction();

		vector transform[4], parentTransform[4];
		EntityFlags flags;
		for (int i, count = api.GetSelectedEntitiesCount(); i < count; i++)
		{
			IEntitySource src = api.GetSelectedEntity(i);
			IEntity entity = api.SourceToEntity(src);
			if (!entity || MUD_GroundEntity.Cast(entity))
				continue;

			entity.GetWorldTransform(transform);
			float y;
			if (!MudY(world, transform[3], y))
			{
				skipped++;
				continue;
			}
			vector wpos = transform[3];
			wpos[1] = y;
			transform[3] = wpos;

			IEntity parent = api.SourceToEntity(src.GetParent());
			if (parent)
			{
				parent.GetWorldTransform(parentTransform);
				Math3D.MatrixInvMultiply4(parentTransform, transform, transform);
			}

			vector pos = transform[3];
			// RELATIVE_Y: coords Y is the height above the terrain, not sea level
			src.Get("Flags", flags);
			if (flags & EntityFlags.RELATIVE_Y)
				pos[1] = y - world.GetSurfaceY(wpos[0], wpos[2]);

			api.SetVariableValue(src, null, "coords", string.Format("%1 %2 %3", pos[0], pos[1], pos[2]));
			moved++;
		}

		api.EndEntityAction();
		Print(string.Format("MUD snap: %1 moved onto the mud, %2 not over a mud ground", moved, skipped));
	}
}
#endif // WORKBENCH
