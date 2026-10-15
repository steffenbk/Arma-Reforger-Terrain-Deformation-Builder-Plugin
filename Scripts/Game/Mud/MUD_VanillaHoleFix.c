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
		if (!entities)
			return;
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
