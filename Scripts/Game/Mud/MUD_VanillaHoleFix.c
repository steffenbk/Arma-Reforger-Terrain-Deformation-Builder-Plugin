//! Vanilla code that reads BaseWorld.GetSurfaceY gets -256 on our terrain hole cells (mud-gotchas 2026-10-08).
//! 2026-10-12 user: GM-placed objects spawn under the map, the GM camera passes through the ground, and the GM
//! camera shoots up when it crosses kept terrain (its ATL height jumps from "above sea" to "above terrain").
//! These copies of the vanilla methods (Exp 192624 pak) differ only in MUD_Surface.SafeY for GetSurfaceY.
//! The camera classes are prefab containers: a modded one WITHOUT [BaseContainerProps] is an 'Unknown class' to
//! ManualCameraBase.et and the component is dropped (log 2026-10-12 21:18 - camera lost ground stop and ATL).

//------------------------------------------------------------------------------------------------
modded class SCR_TerrainHelper
{
	// MEASURED 2026-10-12 21:40 (lightning probe): SnapToTerrain on our ground gave y 0 (= vanilla GetTerrainNormal
	// path, -256 clamped to sea) while SafeY was 38.5, but outside calls to SCR_TerrainHelper.GetTerrainY get ours.
	// Inference: vanilla statics calling each other unqualified keep the vanilla version. So every chain is
	// copied here and calls the next step qualified (re-check with the lightning probe).

	//------------------------------------------------------------------------------------------------
	override static float GetHeightAboveTerrain(vector pos, BaseWorld world = null, bool noUnderwater = false, TraceParam trace = null)
	{
		return pos[1] - SCR_TerrainHelper.GetTerrainY(pos, world, noUnderwater, trace);
	}

	//------------------------------------------------------------------------------------------------
	override static bool GetTerrainBasis(vector pos, out vector result[4], BaseWorld world = null, bool noUnderwater = false, TraceParam trace = null)
	{
		vector normal = SCR_TerrainHelper.GetTerrainNormal(pos, world, noUnderwater, trace);
		if (normal == vector.Zero)
			return false;

		vector perpend = normal.Perpend();
		Math3D.DirectionAndUpMatrix(perpend, normal, result);

		vector basis[4];
		Math3D.AnglesToMatrix({ -perpend.VectorToAngles()[0], 0, 0 }, basis);
		Math3D.MatrixMultiply3(result, basis, result);

		result[3] = pos;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override static bool SnapToTerrain(out vector transform[4], BaseWorld world = null, bool noUnderwater = false, TraceParam trace = null)
	{
		vector surfaceBasis[4];
		if (!SCR_TerrainHelper.GetTerrainBasis(transform[3], surfaceBasis, world, noUnderwater, trace))
			return false;

		transform[3] = surfaceBasis[3];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override static bool OrientToTerrain(out vector transform[4], BaseWorld world = null, bool noUnderwater = false, TraceParam trace = null)
	{
		vector surfaceBasis[4];
		if (!SCR_TerrainHelper.GetTerrainBasis(transform[3], surfaceBasis, world, noUnderwater, trace))
			return false;

		Math3D.MatrixMultiply3(surfaceBasis, transform, transform);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override static bool SnapAndOrientToTerrain(out vector transform[4], BaseWorld world = null, bool noUnderwater = false, TraceParam trace = null)
	{
		vector surfaceBasis[4];
		if (!SCR_TerrainHelper.GetTerrainBasis(transform[3], surfaceBasis, world, noUnderwater, trace))
			return false;

		transform[3] = surfaceBasis[3];
		Math3D.MatrixMultiply3(surfaceBasis, transform, transform);
		return true;
	}
	//------------------------------------------------------------------------------------------------
	override static float GetTerrainY(vector pos, BaseWorld world = null, bool noUnderwater = false, TraceParam trace = null)
	{
		if (!world)
		{
			world = GetGame().GetWorld();
			if (!world)
				return 0;
		}

		float surfaceY;
		if (trace)
		{
			trace.Start = pos;
			trace.End = { pos[0], pos[1] - MAX_TRACE_LENGTH, pos[2] };

			if (trace.Flags == 0)
				trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;

			float traceCoef = world.TraceMove(trace, null);
			if (traceCoef < 1)
				surfaceY = trace.Start[1] - (trace.Start[1] - trace.End[1]) * traceCoef;
		}

		if (surfaceY == 0)
			surfaceY = MUD_Surface.SafeY(world, pos[0], pos[2]);

		if (noUnderwater && surfaceY < 0)
			surfaceY = 0;

		return surfaceY;
	}

	//------------------------------------------------------------------------------------------------
	override static vector GetTerrainNormal(inout vector pos, BaseWorld world = null, bool noUnderwater = false, TraceParam trace = null)
	{
		if (!world)
		{
			world = GetGame().GetWorld();
			if (!world)
				return vector.Zero;
		}

		if (trace)
		{
			pos[1] = Math.Max(pos[1], MUD_Surface.SafeY(world, pos[0], pos[2]) + 0.01);

			trace.Start = pos;
			trace.End = { pos[0], pos[1] - MAX_TRACE_LENGTH, pos[2] };

			if (trace.Flags == 0)
				trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;

			float traceCoef = world.TraceMove(trace, null);
			if (traceCoef < 1)
			{
				pos[1] = trace.Start[1] - (trace.Start[1] - trace.End[1]) * traceCoef;
				if (noUnderwater && pos[1] < world.GetOceanBaseHeight())
				{
					pos[1] = Math.Max(pos[1], world.GetOceanBaseHeight());
					return vector.Up;
				}
				return trace.TraceNorm;
			}
			return vector.Up;
		}

		float surfaceY = MUD_Surface.SafeY(world, pos[0], pos[2]);
		if (noUnderwater && surfaceY < world.GetOceanBaseHeight())
		{
			pos[1] = Math.Max(surfaceY, world.GetOceanBaseHeight());
			return vector.Up;
		}

		pos[1] = surfaceY;
		TraceParam traceRef = new TraceParam();
		traceRef.Start = pos + vector.Up;
		traceRef.End = pos - vector.Up;
		traceRef.Flags = TraceFlags.WORLD | TraceFlags.ENTS;    // vanilla WORLD only: our ground is entities
		world.TraceMove(traceRef, null);
		if (traceRef.TraceNorm == vector.Zero)
			return vector.Up;
		return traceRef.TraceNorm;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseManualCameraComponentTitle()]
modded class SCR_TerrainCollisionManualCameraComponent
{
	//------------------------------------------------------------------------------------------------
	override void EOnCameraFrame(SCR_ManualCameraParam param)
	{
		vector pos = CoordFromCamera(param.transform[3]);
		pos[1] = Math.Max(pos[1], MUD_Surface.SafeY(param.world, pos[0], pos[2]) + m_fMinHeight);
		param.transform[3] = CoordToCamera(pos);
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseManualCameraComponentTitle()]
modded class SCR_ATLManualCameraComponent
{
	//------------------------------------------------------------------------------------------------
	override void EOnCameraFrame(SCR_ManualCameraParam param)
	{
		if (!param.isDirty)
			return;

		float vertical = m_InputManager.GetActionValue("ManualCameraMoveVertical");
		if (vertical != 0.0)
			return;

		vector pos = param.transform[3];
		float height = Math.AbsFloat(pos[1] - Math.Max(MUD_Surface.SafeY(param.world, pos[0], pos[2]), 0));

		if (height > m_fATLMaxHeight)
			return;

		vector posOrig = param.transformOriginal[3];
		float heightOrig = Math.AbsFloat(posOrig[1] - Math.Max(MUD_Surface.SafeY(param.world, posOrig[0], posOrig[2]), 0));

		float progress = Math.Clamp(Math.InverseLerp(m_fATLMinHeight, m_fATLMaxHeight, height), 0.0, 1.0);
		float heightInterpolated = Math.Lerp(heightOrig - height, 0, progress);

		float inputDiff = 0;
		if (param.isManualInput)
			inputDiff = pos[1] - posOrig[1];

		pos[1] = pos[1] + heightInterpolated + inputDiff;
		param.transform[3] = pos;
	}
}

//------------------------------------------------------------------------------------------------
//! Camera speed by height: on a hole cell vanilla measured from sea level (camera sped up over holes)
[BaseContainerProps(), SCR_BaseManualCameraComponentTitle()]
modded class SCR_SpeedByHeightManualCameraComponent
{
	//------------------------------------------------------------------------------------------------
	override void EOnCameraFrame(SCR_ManualCameraParam param)
	{
		vector pos = CoordFromCamera(param.transform[3]);
		float surfaceY = Math.Max(MUD_Surface.SafeY(param.world, pos[0], pos[2]), 0);    // vanilla #else branch
		float height = Math.AbsFloat(pos[1] - surfaceY);
		if (pos[1] < 0)
			height = m_fUnderwaterSpeedHeight;

		param.multiplier *= Math.Max(height * m_fSpeedCoef, m_fMinSpeed);
	}
}

//------------------------------------------------------------------------------------------------
//! 2026-10-12 user: a GM-placed vehicle or heavy object hangs in the air until someone drives it.
//! Inference (not measured): it is spawned with a sleeping body a little above our ground and nothing wakes
//! it. Wake every dynamic body the GM places, one frame after spawn (after the vanilla setup).
modded class SCR_PlacingEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void OnEntityCreatedServer(array<SCR_EditableEntityComponent> entities)
	{
		super.OnEntityCreatedServer(entities);
		MUD_QueueWake(entities);
	}

	//------------------------------------------------------------------------------------------------
	//! User WB 2026-10-24: the GM mortar placement (no vertical adjust, TERRAIN mode) can only be placed near roads,
	//! or after lifting it. Vanilla CanCreateEntity (Exp pak read 2026-10-24) blocks placing when the height is more
	//! than 2 m off the raw GetSurfaceY - -256 on a hole cell, so only kept terrain (roads) passed. Same method, SafeY.
	override bool CanCreateEntity(out ENotification outNotification = -1, inout SCR_EPreviewState previewStateToShow = SCR_EPreviewState.PLACEABLE, SCR_EditorPreviewParams params = null, int prefabID = -1)
	{
		if (m_bBlockPlacing)
			return false;

		if (!params)
			return true;

		vector mat[4];
		params.GetWorldTransform(mat);
		if (!SCR_Global.IsPositionWithinTerrainBounds(mat[3]))
		{
			previewStateToShow = SCR_EPreviewState.BLOCKED;
			outNotification = ENotification.EDITOR_TRANSFORMING_INCORRECT_POSITION;
			return false; // dont allow for spawning of objects outside of the terrain
		}

		if (m_PreviewManager && !m_PreviewManager.GetCanAdjustVerticalPosition())
		{
			float x = mat[3][0];
			float y = mat[3][1];
			float z = mat[3][2];
			float surfaceY = MUD_Surface.SafeY(GetGame().GetWorld(), x, z);

			switch (m_PreviewManager.GetVerticalMode())
			{
				case EEditorTransformVertical.TERRAIN:
					if (!float.AlmostEqual(surfaceY, y, MAX_HEIGHT_ABOVE_TERRAIN))
					{
						previewStateToShow = SCR_EPreviewState.BLOCKED;
						outNotification = ENotification.EDITOR_TRANSFORMING_INCORRECT_POSITION;
						return false; // dont allow for placing mid air
					}

					break;

				case EEditorTransformVertical.GEOMETRY:
					TraceParam param = new TraceParam();
					param.Start = mat[3] + vector.Up * 0.5; // to avoid missing the entity on which it is placed when trace would start inside the collider
					param.End = param.Start - vector.Up * MAX_HEIGHT_ABOVE_TERRAIN;
					param.Flags = TraceFlags.ENTS | TraceFlags.WORLD | TraceFlags.OCEAN;
					param.Exclude = m_PreviewManager.GetPreviewEntity();
					float distPercentage = GetGame().GetWorld().TraceMove(param);
					if (distPercentage == 1 && param.TraceEnt == null)
					{
						previewStateToShow = SCR_EPreviewState.BLOCKED;
						outNotification = ENotification.EDITOR_TRANSFORMING_INCORRECT_POSITION;
						return false; // dont allow for placing mid air
					}

					break;
			}
		}

		// only client has proper data setup for validating if this prefab is available, thus server uses default attribute value for prefabID
		if (prefabID < 0)
			return true;

		if (params.m_PlacingFlags & EEditorPlacingFlags.PLACING_ACTION)
			return IsActionValid(prefabID, params);

		SCR_ContentBrowserEditorComponent editorBrowser = SCR_ContentBrowserEditorComponent.Cast(m_Owner.FindComponent(SCR_ContentBrowserEditorComponent));
		if (!editorBrowser)
			return false;

		return editorBrowser.IsPrefabIDAvailable(prefabID);
	}

	//------------------------------------------------------------------------------------------------
	//! User WB 2026-10-24: GM mortar placements (SlotFlatSmall/MortarPlacement_S_*) still spawned at y ~ -38 over
	//! ~+38 ground on hole cells (log 00:36-00:40, 3 of 5), and NOT through SCR_SlotCompositionComponent (its
	//! TEST_KIT line never printed for them) - source UNRESOLVED. So every GM-placed root on a hole cell more than
	//! 0.5 m under our ground is put on it, server side, via the editable entity (which also sends it to clients).
	//! Known limit: an item placed inside a voxel tunnel is pushed up to the surface (SafeY = top of our ground).
	protected static void MUD_LiftPlaced(array<SCR_EditableEntityComponent> entities)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || !entities)
			return;
		foreach (SCR_EditableEntityComponent e : entities)
		{
			if (!e || !e.GetOwner() || e.GetOwner().GetParent())
				continue;
			vector transform[4];
			e.GetOwner().GetWorldTransform(transform);
			vector o = transform[3];
			if (!MUD_Surface.IsHole(world, o[0], o[2]))
				continue;
			float g = MUD_Surface.SafeY(world, o[0], o[2]);
			if (o[1] > g - 0.5)
				continue;
			if (MUD_TestKit.TEST_KIT)
				Print(string.Format("MUD hole fix: GM-placed %1 at %2 was %3 m under our ground - put on it", e.GetOwner().ClassName(), o, g - o[1]), LogLevel.NORMAL);
			transform[3][1] = g;
			e.SetTransform(transform);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Wake the placed entities' dynamic bodies one frame later (also called by the Conflict building class)
	protected void MUD_QueueWake(array<SCR_EditableEntityComponent> entities)
	{
		if (!entities)
			return;
		MUD_LiftPlaced(entities);     // 2026-10-24: before the wake, same frame as the vanilla setup ran
		GetGame().GetCallqueue().CallLater(MUD_LiftPlaced, 100, false, entities);     // and again once EOnInit-time moves are done
		array<IEntity> owners = {};
		foreach (SCR_EditableEntityComponent e : entities)
		{
			if (e && e.GetOwner())
				owners.Insert(e.GetOwner());
		}
		if (!owners.IsEmpty())
			GetGame().GetCallqueue().CallLater(MUD_WakePlaced, 0, false, owners);
	}

	//------------------------------------------------------------------------------------------------
	protected static void MUD_WakePlaced(array<IEntity> owners)
	{
		foreach (IEntity o : owners)
		{
			if (!o)
				continue;
			Physics phys = o.GetPhysics();
			if (phys && phys.IsDynamic())
				phys.SetActive(ActiveState.ACTIVE);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Audit A13 (2026-10-22): Conflict building mode overrides OnEntityCreatedServer WITHOUT super (vanilla
//! SCR_CampaignBuildingPlacingEditorComponent.c, read from the game pak 2026-10-22), so the hook above never runs there
modded class SCR_CampaignBuildingPlacingEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void OnEntityCreatedServer(array<SCR_EditableEntityComponent> entities)
	{
		super.OnEntityCreatedServer(entities);
		MUD_QueueWake(entities);
	}
}

//------------------------------------------------------------------------------------------------
//! User DS 2026-10-22: after a GM MOVE the moved vehicle (with its crew) and other moved objects hang in the air,
//! frozen, until someone drives them (inference: the teleport leaves the body asleep). Vanilla ConfirmEditingServer
//! (SCR_TransformingEditorComponent.c, Exp 1.9 pak, read 2026-10-22) applies the move, then calls Clean(), which
//! clears m_aEditedEntities - so the moved bodies are woken from Clean, server only, one frame and 0.5 s later.
//! A cancelled move also passes Clean: waking those is harmless.
modded class SCR_TransformingEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void Clean()
	{
		if (m_aEditedEntities && !m_aEditedEntities.IsEmpty() && Replication.IsServer())
		{
			array<IEntity> owners = {};
			foreach (SCR_EditableEntityComponent e : m_aEditedEntities)
			{
				if (e && e.GetOwner())
					owners.Insert(e.GetOwner());
			}
			if (!owners.IsEmpty())
			{
				GetGame().GetCallqueue().CallLater(MUD_WakeMoved, 0, false, owners);
				GetGame().GetCallqueue().CallLater(MUD_WakeMoved, 500, false, owners);
			}
		}
		super.Clean();
	}

	//------------------------------------------------------------------------------------------------
	protected static void MUD_WakeMoved(array<IEntity> owners)
	{
		foreach (IEntity o : owners)
		{
			if (!o)
				continue;
			Physics phys = o.GetPhysics();
			if (phys && phys.IsDynamic())
				phys.SetActive(ActiveState.ACTIVE);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Vanilla bug, not ours: stopping play while GM is OPEN closes the editor after the faction delegates are
//! gone -> NULL at SCR_HighlightedEditorComponent.c:415 (seen 2026-10-11 18:06 before any of these fixes).
//! Same method with a null check.
modded class SCR_HighlightedEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override void EOnEditorCloseServer()
	{
		if (m_DelegateFactionManager)
		{
			map<Faction, SCR_EditableFactionComponent> factionMap = new map<Faction, SCR_EditableFactionComponent>;
			m_DelegateFactionManager.GetFactionDelegates(factionMap);

			foreach (Faction faction, SCR_EditableFactionComponent editableFaction : factionMap)
			{
				if (!editableFaction)
					continue;
				editableFaction.GetOnSpawnPointCountChanged().Remove(OnFactionSpawnPointsUpdated);
				editableFaction.GetOnTaskCountChanged().Remove(OnFactionTasksUpdated);
			}
		}

		m_HighlightedEntities.Clear();
	}
}

//------------------------------------------------------------------------------------------------
//! User 2026-10-23: built fortifications (sandbag towers, bunkers ... = BuildableComposition_Base with a Conflict
//! building layout) always end up under the map. Vanilla SnapEntityToTerrain sets every layout child to the raw
//! GetSurfaceY = -256 on a hole cell (SCR_CampaignBuildingLayoutComponent.c, Exp pak read 2026-10-23).
//! Same method, SafeY for GetSurfaceY.
modded class SCR_CampaignBuildingLayoutComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void SnapEntityToTerrain(IEntity entity)
	{
		if (!entity)
			return;

		vector transform[4];
		Math3D.MatrixIdentity4(transform);

		transform[3] = entity.GetOrigin();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		transform[3][1] = MUD_Surface.SafeY(world, transform[3][0], transform[3][2]);
		if (MUD_TestKit.TEST_KIT && MUD_Surface.IsHole(world, transform[3][0], transform[3][2]))
			Print(string.Format("MUD hole fix: layout child %1 snapped to %2 (vanilla -256)", entity.ClassName(), transform[3]), LogLevel.NORMAL);
		entity.SetTransform(transform);
	}
}

//------------------------------------------------------------------------------------------------
//! User WB 2026-10-24: the GM mortar placement GHOST sits away from the cursor (as the old Zeus lightning did).
//! Vanilla GetPreviewPosAboveTerrain (SCR_PreviewEntityEditorUIComponent, Exp pak read 2026-10-24) starts the cursor
//! ray heightATL lower and adds it back, so a wrong height above terrain moves the hit point along the ray, off the
//! cursor. Inference: on hole cells the stored height is the old under-ground value (-38 / -76). The value itself is
//! private (m_fHeightTerrain), so the getter is clamped: an entity that cannot adjust its height (vanilla
//! CanCreateEntity blocks it beyond MAX_HEIGHT_ABOVE_TERRAIN 2 m anyway) never reports more than 2 m off the ground.
modded class SCR_PreviewEntityEditorComponent
{
	protected static int s_iMudHatPrintAt;

	//------------------------------------------------------------------------------------------------
	override float GetPreviewHeightAboveTerrain()
	{
		float h = super.GetPreviewHeightAboveTerrain();
		if (GetCanAdjustVerticalPosition() || Math.AbsFloat(h) <= 2)
			return h;
		if (MUD_TestKit.TEST_KIT && System.GetTickCount() - s_iMudHatPrintAt > 1000)
		{
			s_iMudHatPrintAt = System.GetTickCount();
			Print(string.Format("MUD hole fix: preview height above terrain %1 m clamped to 0 (no vertical adjust)", h), LogLevel.NORMAL);
		}
		return 0;
	}
}

//------------------------------------------------------------------------------------------------
//! User WB 2026-10-24: after the layout fix only the GM mortar placement (Slotted/SlotFlatSmall/MortarPlacement_S_*)
//! still went under the map. Log 2026-10-24 00:29: 'spawned E_MortarPlacement_S_FIA_01.et prefab at <948.9,-37.94,2337>'
//! and '<1178.6,-38.63,2639.6>' (one on kept terrain was fine at y 39). Vanilla SetChildTransform (Exp pak read
//! 2026-10-24) keeps the root's height ABOVE TERRAIN as it was spawned ("maintain existing height ATL"), so a root
//! spawned under our ground stays there. Where the under-ground spawn height comes from (a buried site slot or the
//! placing transform) is UNRESOLVED - the TEST_KIT lines below print it. Same method; on a hole cell a root more
//! than 0.5 m under our ground is put on it (height 0).
modded class SCR_SlotCompositionComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void SetChildTransform(IEntity owner, BaseWorld world)
	{
		//--- Find entity's height
		float height = 0;
		if (owner.GetParent())
		{
			//--- Use original local height (used only during initial transformation)
			height = owner.GetLocalTransformAxis(3)[1];
		}
		else
		{
			//--- Maintain existing height ATL
			height = SCR_TerrainHelper.GetHeightAboveTerrain(owner.GetOrigin(), world);
			vector o = owner.GetOrigin();
			if (MUD_Surface.IsHole(world, o[0], o[2]))
			{
				if (MUD_TestKit.TEST_KIT)
					Print(string.Format("MUD hole fix: slot composition %1 root at %2, our ground %3, height ATL %4", owner.ClassName(), o, MUD_Surface.SafeY(world, o[0], o[2]), height), LogLevel.NORMAL);
				if (height < -0.5)
					height = 0;     // MUD: a placed composition never starts under our ground
			}
		}

		//--- Apply transformation
		vector transform[4];
		owner.GetWorldTransform(transform);
		if (owner == m_Owner)
			SCR_TerrainHelper.SnapToTerrain(transform, world); //--- Don't rotate root entity. Its transformation is broadcasted to clients, who would then think the rotated state is default
		else
			SCR_TerrainHelper.SnapAndOrientToTerrain(transform, world);

		transform[3] = transform[3] + Vector(0, height, 0);

		//If entity should be horizontally aligned rather then aligned to terrain
		SCR_HorizontalAlignComponent horizontalAlignComponent = SCR_HorizontalAlignComponent.Cast(owner.FindComponent(SCR_HorizontalAlignComponent));
		if (horizontalAlignComponent)
		{
			vector angles = Math3D.MatrixToAngles(transform);
			angles[1] = 0;
			angles[2] = 0;
			Math3D.AnglesToMatrix(angles, transform);
		}

#ifdef WORKBENCH
		//--- When in World Editor, apply changes to attributes directly
		if (SCR_Global.IsEditMode(owner))
		{
			WorldEditorAPI api = GenericEntity.Cast(owner)._WB_GetEditorAPI();
			IEntity parent = owner.GetParent();
			vector pos = transform[3];
			if (parent)
			{
				vector parentTransform[3];
				parent.GetWorldTransform(parentTransform);
				Math3D.MatrixInvMultiply3(parentTransform, transform, transform);
				pos = parent.CoordToLocal(pos);
			}
			vector angles = Math3D.MatrixToAngles(transform);
			IEntitySource ownerSrc = api.EntityToSource(owner);
			api.SetVariableValue(ownerSrc, null, "coords", pos.ToString(false));
			api.SetVariableValue(ownerSrc, null, "angles", string.Format("%1 %2 %3", angles[1], angles[0], angles[2]));
		}
		else
		{
			owner.SetWorldTransform(transform);
		}
#else
		owner.SetWorldTransform(transform);
#endif

		//--- Process children recursively
		SCR_SlotCompositionComponent composition = SCR_SlotCompositionComponent.Cast(owner.FindComponent(SCR_SlotCompositionComponent));
		if (composition && composition.CanOrientChildrenToTerrain())
		{
			IEntity child = owner.GetChildren();
			while (child)
			{
				SetChildTransform(child, world);
				child = child.GetSibling();
			}
		}
	}
}

//------------------------------------------------------------------------------------------------
//! 2026-10-24 TEST_KIT only: where a slot spawn starts (a site slot on a hole cell may be buried - unresolved)
modded class SCR_SiteSlotEntity
{
	//------------------------------------------------------------------------------------------------
	override IEntity SpawnEntityInSlot(Resource resource, float rotation = -1, EEditorTransformVertical verticalMode = EEditorTransformVertical.TERRAIN)
	{
		if (MUD_TestKit.TEST_KIT)
		{
			vector o = GetOrigin();
			Print(string.Format("MUD hole fix: site slot %1 at %2, hole %3, our ground %4, vertical mode %5", GetName(), o, MUD_Surface.IsHole(GetWorld(), o[0], o[2]), MUD_Surface.SafeY(GetWorld(), o[0], o[2]), verticalMode), LogLevel.NORMAL);
		}
		return super.SpawnEntityInSlot(resource, rotation, verticalMode);
	}
}

//------------------------------------------------------------------------------------------------
//! User 2026-10-23: AI spawn under the map (and sometimes high in the air). Vanilla SpawnGroupMember reads the raw
//! GetSurfaceY twice (SCR_AIGroup.c, Exp pak read 2026-10-23): snapToTerrain puts the member at -256 on a hole
//! cell, so the navmesh search ("10 10 10" round that point) finds nothing and the member spawns at -256; the
//! "not under the ground" clamp after the navmesh snap is -256 too. Same method, SafeY for both reads.
//! The high-in-the-air case is UNRESOLVED: the TEST_KIT line prints the spawn height vs our ground on hole cells.
modded class SCR_AIGroup
{
	//------------------------------------------------------------------------------------------------
	override protected bool SpawnGroupMember(bool snapToTerrain, int index, ResourceName res, bool editMode, bool isLast)
	{
		BaseWorld world = GetWorld();
		AIFormationDefinition formationDefinition;
		AIFormationComponent formationComponent = AIFormationComponent.Cast(this.FindComponent(AIFormationComponent));
		if (formationComponent)
			formationDefinition = formationComponent.GetFormation();
		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;
		GetWorldTransform(spawnParams.Transform);
		vector pos = spawnParams.Transform[3];

		if (formationDefinition)
			pos = CoordToParent(formationDefinition.GetOffsetPosition(index));
		else
			pos = CoordToParent(Vector(index, 0, 0));

		if (snapToTerrain)
		{
			float surfaceY = MUD_Surface.SafeY(world, pos[0], pos[2]);
			pos[1] = surfaceY;
		}


		//Snap to the nearest navmesh point
		AIPathfindingComponent pathFindindingComponent = AIPathfindingComponent.Cast(this.FindComponent(AIPathfindingComponent));

		if (!editMode)
		{
			NavmeshWorldComponent navmesh = pathFindindingComponent.GetNavmeshComponent();
			if (navmesh)
			{
				if (navmesh.IsTileRequested(pos))
				{
					m_iNavmeshStallFrames++;
					if (m_iNavmeshStallFrames < NAVMESH_STALL_LIMIT)
					{
						if (DBG_AI_LOGGING)
							Print(string.Format("[DBG_AI] SpawnGroupMember fail: navmesh tile REQUESTED at %1 stall=%2/%3 (group %4 slot %5)", pos, m_iNavmeshStallFrames, NAVMESH_STALL_LIMIT, this, index), LogLevel.WARNING);
						return false;
					}
					// Tile was requested but never loaded - no navmesh data at this position.
					// Give up waiting and let the spawn proceed.
					if (DBG_AI_LOGGING)
						Print(string.Format("[DBG_AI] SpawnGroupMember: navmesh stall limit reached at %1, proceeding without loaded tile (group %2 slot %3)", pos, this, index), LogLevel.WARNING);
				}
				else if (!navmesh.IsTileLoaded(pos))
				{
					// Call LoadTileIn to start async tile loading and count the attempt.
					// If the tile never transitions to IsTileRequested (e.g. navmesh is
					// pre-baked or not streamed on this server), LoadTileIn is a no-op and
					// IsTileRequested stays false indefinitely. Count these attempts with the
					// same stall counter so the give-up path fires after NAVMESH_STALL_LIMIT
					// calls, preventing an infinite retry loop on servers without tile streaming.
					m_iNavmeshStallFrames++;
					if (DBG_AI_LOGGING)
						Print(string.Format("[DBG_AI] SpawnGroupMember fail: navmesh tile NOT LOADED at %1 - LoadTileIn issued stall=%2/%3 (group %4 slot %5)", pos, m_iNavmeshStallFrames, NAVMESH_STALL_LIMIT, this, index), LogLevel.WARNING);
					navmesh.LoadTileIn(pos);
					if (m_iNavmeshStallFrames < NAVMESH_STALL_LIMIT)
						return false;
					// Stall limit reached without the tile loading - proceed without navmesh snap.
					if (DBG_AI_LOGGING)
						Print(string.Format("[DBG_AI] SpawnGroupMember: LoadTileIn stall limit reached at %1, proceeding without loaded tile (group %2 slot %3)", pos, this, index), LogLevel.WARNING);
				}
			}
		}

		bool onNav = false;     // MUD: for the TEST_KIT line
		if (pathFindindingComponent && pathFindindingComponent.GetClosestPositionOnNavmesh(pos, "10 10 10", pos))
		{
			onNav = true;
			float groundHeight = MUD_Surface.SafeY(world, pos[0], pos[2]);
			if (pos[1] < groundHeight)
				pos[1] = groundHeight;
			vector outWaterSurfacePoint;
			EWaterSurfaceType waterSurfaceType;
			vector transformWS[4];
			vector obbExtents;
			if (ChimeraWorldUtils.TryGetWaterSurface(GetWorld(), pos, outWaterSurfacePoint, waterSurfaceType, transformWS, obbExtents))
			{
				pos = outWaterSurfacePoint;
			}
		}

		if (MUD_TestKit.TEST_KIT && MUD_Surface.IsHole(world, pos[0], pos[2]))
			Print(string.Format("MUD hole fix: AI member %1 at %2, our ground %3 (snap %4, edit %5, navmesh %6)",
				index, pos, MUD_Surface.SafeY(world, pos[0], pos[2]), snapToTerrain, editMode, onNav), LogLevel.NORMAL);

		spawnParams.Transform[3] = pos;

		IEntity member = GetGame().SpawnEntityPrefabEx(res, true, world, spawnParams);
		if (!member)
			return true;

		// Move in to vehicle
		SCR_EditableEntityComponent editableEntity = SCR_EditableEntityComponent.Cast(member.FindComponent(SCR_EditableEntityComponent));


		if (editMode)
			m_aSceneGroupUnitInstances.Insert(member);

		// Even same null-check is above, in some situations, member can get deleted and it would result in VME
		if (!member)
			return true;

		AddAIEntityToGroup(member);

		FactionAffiliationComponent factionAffiliation = FactionAffiliationComponent.Cast(member.FindComponent(FactionAffiliationComponent));

		if (factionAffiliation)
			factionAffiliation.SetAffiliatedFactionByKey(m_faction);

		if (isLast)
			InvokeEventOnInit();
		return true;
	}
}
