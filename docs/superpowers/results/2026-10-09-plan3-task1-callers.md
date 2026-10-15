# Shipped hole-height callers (pak experimental)

Regex: `\b(GetSurfaceY|GetTerrainY|GetHeightAboveTerrain)\s*\(` on code with `//` comments stripped, all `scripts/**/*.c`.

Calls 109 in 78 files; by api: {'GetSurfaceY': 89, 'GetHeightAboveTerrain': 7, 'GetTerrainY': 13}; sum 109 (checksum = calls).

| file | line | api | function | code |
|---|---|---|---|---|
| scripts/Game/AI/ScriptedNodes/Aiming/SCR_AIGetAimCompensation.c | 168 | GetSurfaceY | CorrectForSurface | `float targetElev = targetPos[1] - world.GetSurfaceY(targetPos[0], targetPos[2]);` |
| scripts/Game/AI/ScriptedNodes/Aiming/SCR_AIGetAimCompensation.c | 174 | GetSurfaceY | CorrectForSurface | `float aimPosSurfY = world.GetSurfaceY(aimPos[0], aimPos[2]);` |
| scripts/Game/AI/ScriptedNodes/Utils/SCR_AIGetPosForward.c | 52 | GetSurfaceY | EOnTaskSimulate | `forwardVector[1] = m_Target.GetWorld().GetSurfaceY(forwardVector[0], forwardVector[2]);` |
| scripts/Game/AI/ScriptedNodes/Utils/SCR_AIGetPosForwardToTarget.c | 72 | GetSurfaceY | EOnTaskSimulate | `forwardVector[1] = controlledEntity.GetWorld().GetSurfaceY(forwardVector[0], forwardVector[2]);` |
| scripts/Game/AI/Utils/SCR_AISuppressionVolume.c | 118 | GetSurfaceY | GetWorldYRange | `minWorldY = Math.Max(minY, world.GetSurfaceY(position[0], position[2]) + MIN_SURFACE_Y);` |
| scripts/Game/Building/SCR_CampaignBuildingLayoutComponent.c | 493 | GetSurfaceY | SnapEntityToTerrain | `transform[3][1] = world.GetSurfaceY(transform[3][0], transform[3][2]);` |
| scripts/Game/Camera/Components/SCR_ATLManualCameraComponent.c | 30 | GetSurfaceY | EOnCameraFrame | `float height = Math.AbsFloat(pos[1] - Math.Max(param.world.GetSurfaceY(pos[0], pos[2]), 0));` |
| scripts/Game/Camera/Components/SCR_ATLManualCameraComponent.c | 38 | GetSurfaceY | EOnCameraFrame | `float heightOrig = Math.AbsFloat(posOrig[1] - Math.Max(param.world.GetSurfaceY(posOrig[0], posOrig[2]), 0));` |
| scripts/Game/Camera/Components/SCR_LightManualCameraComponent.c | 195 | GetHeightAboveTerrain | CameraLightIntensityUpdate | `float lightIntensity = Math.Lerp(m_fCameraLightIntesityMin, m_fCameraLightIntesityMax, LegacyCurve.Curve(ECurveType.CurveProperty2D, Math.Clamp(SCR_TerrainHelper.GetHeightAboveTerrain(cameraTransform[3], m_World, true) / m_fCameraLightIntensityMaxHeight, 0, 1), m_cCameraLightIntensityCurve)[1]);` |
| scripts/Game/Camera/Components/SCR_LightManualCameraComponent.c | 234 | GetHeightAboveTerrain | PointingLightTransformUpdate | `float cameraHeightAboveTerrain = SCR_TerrainHelper.GetHeightAboveTerrain(cameraTransform[3], m_World, true);` |
| scripts/Game/Camera/Components/SCR_OrbitingManualCameraComponent.c | 103 | GetSurfaceY | EOnCameraFrame | `m_vTargetHeightATL = m_vTarget[1] - m_World.GetSurfaceY(m_vTarget[0], m_vTarget[2]);` |
| scripts/Game/Camera/Components/SCR_OrbitingManualCameraComponent.c | 156 | GetSurfaceY | EOnCameraFrame | `target[1] = m_World.GetSurfaceY(target[0], target[2]) + m_vTargetHeightATL;` |
| scripts/Game/Camera/Components/SCR_OrbitingManualCameraComponent.c | 166 | GetSurfaceY | EOnCameraFrame | `if (posWorld[1] < m_World.GetSurfaceY(posWorld[0], posWorld[2]))` |
| scripts/Game/Camera/Components/SCR_SpeedBoostUpManualCameraComponent.c | 44 | GetSurfaceY | EOnCameraFrame | `float surfaceY = param.world.GetSurfaceY(pos[0], pos[2]);` |
| scripts/Game/Camera/Components/SCR_SpeedByHeightManualCameraComponent.c | 28 | GetSurfaceY | EOnCameraFrame | `float surfaceY = param.world.GetSurfaceY(pos[0], pos[2]);` |
| scripts/Game/Camera/Components/SCR_TeleportToCursorManualCameraComponent.c | 157 | GetSurfaceY | EOnCameraFrame | `float height = pos[1] - param.world.GetSurfaceY(pos[0], pos[2]);` |
| scripts/Game/Camera/Components/SCR_TerrainCollisionManualCameraComponent.c | 14 | GetSurfaceY | EOnCameraFrame | `pos[1] = Math.Max(pos[1], param.world.GetSurfaceY(pos[0], pos[2]) + m_fMinHeight);` |
| scripts/Game/Campaign/SCR_CampaignFeedbackComponent.c | 691 | GetHeightAboveTerrain | IsPlayerCapturing | `if (!player.IsInVehicle() \|\| SCR_TerrainHelper.GetHeightAboveTerrain(playerPos) <= seizingComp.GetMaximumAltitude())` |
| scripts/Game/Commanding/Commands/SCR_ArtilleryWaypointGroupCommand.c | 71 | GetTerrainY | SpoofTargetPosition | `targetPosition[1] = SCR_TerrainHelper.GetTerrainY(targetPosition, noUnderwater: true);` |
| scripts/Game/Commanding/SCR_PlayerControllerCommandingComponent.c | 295 | GetSurfaceY | OnMapCommandPerformed | `float height = GetGame().GetWorld().GetSurfaceY(worldPos[0], worldPos[1]);` |
| scripts/Game/Components/AmbientSoundsComponent/SCR_SoundGroup.c | 133 | GetSurfaceY | GetSoundTypeAndPosition | `float surfaceHeight = randomPositionaSounds.GetWorld().GetSurfaceY(position[0], position[2]);` |
| scripts/Game/Components/AmbientSoundsComponent/SCR_SoundGroup.c | 193 | GetSurfaceY | GetSoundTypeAndPosition | `float surfaceHeight = world.GetSurfaceY(position[0], position[2]);` |
| scripts/Game/Components/AreaMesh/SCR_BaseAreaMeshComponent.c | 213 | GetSurfaceY | GenerateAreaMesh | `worldPos[1] = Math.Max(world.GetSurfaceY(worldPos[0], worldPos[2]), 0);` |
| scripts/Game/Components/EffectModules/SCR_EffectModuleChildComponent.c | 55 | GetTerrainY | OnParentEffectsModuleUpdate | `position[1] = SCR_TerrainHelper.GetTerrainY(position);` |
| scripts/Game/Components/EffectModules/SCR_EffectModuleComponent.c | 104 | GetSurfaceY | OnTransformChanged | `pos[1] = GetOwner().GetWorld().GetSurfaceY(pos[0], pos[2]);` |
| scripts/Game/Components/EffectModules/SCR_EffectModuleComponent.c | 105 | GetTerrainY | OnTransformChanged | `pos[1] = SCR_TerrainHelper.GetTerrainY(pos, GetOwner().GetWorld(), true);` |
| scripts/Game/Components/SCR_FastTravelComponent.c | 322 | GetTerrainY | CalculateDestinationVector | `newPlayerOrigin[1] = SCR_TerrainHelper.GetTerrainY(newPlayerOrigin);` |
| scripts/Game/Components/SCR_RandomPositionalInsects.c | 346 | GetSurfaceY | GetSoundMapTypeFromTerrain | `float surfaceHeight = world.GetSurfaceY(worldPos[0], worldPos[2]);` |
| scripts/Game/Components/Sandbox/SCR_ResourcePlayerControllerInventoryComponent.c | 976 | GetSurfaceY | FindSuitablePosition | `position[1] = GetGame().GetWorld().GetSurfaceY(param.End[0], param.End[2]);` |
| scripts/Game/Compositions/SCR_SiteSlotEntity.c | 176 | GetSurfaceY | _WB_SnapToTerrain | `pos[1] = GetWorld().GetSurfaceY(pos[0], pos[2]);` |
| scripts/Game/Compositions/SCR_SlotCompositionComponent.c | 107 | GetHeightAboveTerrain | SetChildTransform | `height = SCR_TerrainHelper.GetHeightAboveTerrain(owner.GetOrigin(), world);` |
| scripts/Game/Destruction/Building/SCR_DestructibleBuildingComponent.c | 341 | GetTerrainY | SnapToTerrain | `float y = SCR_TerrainHelper.GetTerrainY(origin, owner.GetWorld());` |
| scripts/Game/Destruction/SCR_ParticleSpawnable.c | 87 | GetTerrainY | SpawnAsChild | `position[1] = SCR_TerrainHelper.GetTerrainY(position, owner.GetWorld());` |
| scripts/Game/Destruction/SCR_ParticleSpawnable.c | 128 | GetTerrainY | Spawn | `position[1] = SCR_TerrainHelper.GetTerrainY(position, owner.GetWorld());` |
| scripts/Game/Destruction/SCR_PrefabSpawnable.c | 146 | GetSurfaceY | Spawn | `float terrainYMins = GetGame().GetWorld().GetSurfaceY(mins[0], mins[2]);` |
| scripts/Game/Destruction/SCR_PrefabSpawnable.c | 147 | GetSurfaceY | Spawn | `float terrainYMaxs = GetGame().GetWorld().GetSurfaceY(maxs[0], maxs[2]);` |
| scripts/Game/Editor/Components/EditableEntity/SCR_EditableCharacterComponent.c | 524 | GetSurfaceY | SetTransform | `transform[3][1] = Math.Max(transform[3][1], GetOwner().GetWorld().GetSurfaceY(transform[3][0], transform[3][2]));` |
| scripts/Game/Editor/Components/EditableEntity/SCR_EditableEntityComponent.c | 659 | GetSurfaceY | SetTransform | `float terrainY = m_Owner.GetWorld().GetSurfaceY(position[0], position[2]);` |
| scripts/Game/Editor/Components/Editor/SCR_CameraEditorComponent.c | 115 | GetSurfaceY | InitCameraTransform | `center[1] = world.GetSurfaceY(center[0], center[2]);` |
| scripts/Game/Editor/Components/Editor/SCR_CampaignBuildingPlacingObstructionEditorComponent.c | 211 | GetSurfaceY | GetAllEntitiesToEvaluate | `float surfaceY = world.GetSurfaceY(entityCenter[0], entityCenter[2]);` |
| scripts/Game/Editor/Components/Editor/SCR_CampaignBuildingPlacingObstructionEditorComponent.c | 531 | GetSurfaceY | TraceEntityOnPosition | `position[1] = Math.Max(trace.Start[1] - traceCoef * m_vTraceOffset[1] + 0.01, world.GetSurfaceY(position[0], position[2]) + HEIGHT_ABOVE_GROUND_BUFFER);` |
| scripts/Game/Editor/Components/Editor/SCR_PlacingEditorComponent.c | 763 | GetSurfaceY | CanCreateEntity | `float surfaceY = GetGame().GetWorld().GetSurfaceY(x, z);` |
| scripts/Game/Editor/Components/Editor/SCR_PreviewEntityEditorComponent.c | 304 | GetTerrainY | SetPreviewHeight | `m_fHeightTerrain = pos[1] - SCR_TerrainHelper.GetTerrainY(pos, m_World, !m_bIsUnderwater, trace);` |
| scripts/Game/Editor/Components/Editor/SCR_PreviewEntityEditorComponent.c | 449 | GetTerrainY | GetSnapBaseHeight | `return SCR_TerrainHelper.GetTerrainY(pos, m_World, !m_bIsUnderwater, trace) + m_aLocalOffset[3][1] + VERTICAL_TRACE_OFFSET; //--- Offset a bit to make sure the position is not under surface` |
| scripts/Game/Editor/Containers/EditableEntityFilters/SCR_FriendlyPlayerEditableEntityFilter.c | 52 | GetSurfaceY | CacheCameraPos | `float heightATL = m_vCameraPos[1] - Math.Max(m_World.GetSurfaceY(m_vCameraPos[0], m_vCameraPos[2]), 0);` |
| scripts/Game/Editor/Containers/EditableEntityFilters/SCR_RenderedEditableEntityFilter.c | 51 | GetSurfaceY | CacheCameraPos | `float heightATL = m_vCameraPos[1] - Math.Max(m_World.GetSurfaceY(m_vCameraPos[0], m_vCameraPos[2]), 0);` |
| scripts/Game/Editor/Entities/SCR_VirtualAreaEntity.c | 183 | GetSurfaceY | Generate | `entitySurfaceY = Math.Max(GetWorld().GetSurfaceY(transform[3][0], transform[3][2]), 0);` |
| scripts/Game/Editor/Entities/SCR_VirtualAreaEntity.c | 187 | GetSurfaceY | Generate | `pos[1] = Math.Max(GetWorld().GetSurfaceY(worldPos[0], worldPos[2]), 0) - entitySurfaceY;` |
| scripts/Game/Editor/UI/Components/Attributes/SCR_PositionEditorAttributeUIComponent.c | 185 | GetSurfaceY | OnChangeInternal | `yValue = m_World.GetSurfaceY(xValue, zValue);` |
| scripts/Game/Editor/UI/Components/Common/SCR_CursorEditorUIComponent.c | 159 | GetSurfaceY | GetCursorWorldPos | `worldPos[1] = world.GetSurfaceY(worldPos[0], worldPos[2]);` |
| scripts/Game/Editor/UI/Components/Common/SCR_PreviewEntityEditorUIComponent.c | 361 | GetSurfaceY | MoveHorizontalTowardsCursor | `pos[1] = Math.Max(pos[1], GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]));` |
| scripts/Game/Entities/Preview/SCR_RefPreviewEntity.c | 186 | GetTerrainY | UpdateReference | `currentHeight = currentPos[1] - SCR_TerrainHelper.GetTerrainY(currentPos, GetWorld(), !SCR_Enum.HasFlag(m_Flags, EPreviewEntityFlag.UNDERWATER), trace);` |
| scripts/Game/Entities/Preview/SCR_RefPreviewEntity.c | 189 | GetTerrainY | UpdateReference | `float terrainY = SCR_TerrainHelper.GetTerrainY(transform[3], GetWorld(), !isUnderwater, trace);` |
| scripts/Game/Entities/SCR_AIGroup.c | 1677 | GetSurfaceY | SpawnGroupMember | `float surfaceY = world.GetSurfaceY(pos[0], pos[2]);` |
| scripts/Game/Entities/SCR_AIGroup.c | 1727 | GetSurfaceY | SpawnGroupMember | `float groundHeight = world.GetSurfaceY(pos[0], pos[2]);` |
| scripts/Game/Entities/SCR_FPS_Autotest.c | 205 | GetSurfaceY | EOnInit | `float groundY = m_camera.GetWorld().GetSurfaceY(m_x, m_z);` |
| scripts/Game/Entities/SCR_FPS_Autotest.c | 418 | GetSurfaceY | EOnFrame | `float groundY = m_camera.GetWorld().GetSurfaceY(m_x, m_z);` |
| scripts/Game/Entities/SCR_TerrainMeshEntity.c | 141 | GetSurfaceY | _WB_OnContextMenu | `posZ = Math.Max(world.GetSurfaceY(posX, posY), 0) / heightMax;` |
| scripts/Game/Entities/debris/DebrisSmallEntity.c | 290 | GetSurfaceY | SpawnDebris | `float terrainYMins = GetGame().GetWorld().GetSurfaceY(mins[0], mins[2]);` |
| scripts/Game/Entities/debris/DebrisSmallEntity.c | 291 | GetSurfaceY | SpawnDebris | `float terrainYMaxs = GetGame().GetWorld().GetSurfaceY(maxs[0], maxs[2]);` |
| scripts/Game/FactionCommander/Components/SCR_FactionCommanderPlayerComponent.c | 745 | GetSurfaceY | OnCommandPerformed | `position[1] = GetGame().GetWorld().GetSurfaceY(worldPos[0], worldPos[1]);` |
| scripts/Game/FactionCommander/MapCommandCursors/SCR_EstablishBaseMapCommandCursor.c | 243 | GetSurfaceY | CanExecuteCommand | `if (m_World && m_bIsOcean && m_World.GetSurfaceY(position[0], position[2]) < m_fOceanBaseHeight)` |
| scripts/Game/GameMode/Components/SCR_SeizingComponent.c | 302 | GetHeightAboveTerrain | EvaluateEntityFaction | `if (char.IsInVehicle() && SCR_TerrainHelper.GetHeightAboveTerrain(char.GetOrigin()) > m_iMaximumAltitude)` |
| scripts/Game/GameMode/PrefabsSpawning/SCR_PrefabSpawnPoint.c | 12 | GetSurfaceY |  | `[Attribute("0", UIWidgets.CheckBox, "Should be SP snaped in script to ground? -- GetWorld().GetSurfaceY()", "", ParamEnumArray.FromEnum(EPrefabSpawnType) )]` |
| scripts/Game/GameMode/PrefabsSpawning/SCR_PrefabsSpawner.c | 84 | GetSurfaceY | SpawnPrefabCategory | `position[1] = prefabSpawnPoint.GetWorld().GetSurfaceY(position[0], position[2]);` |
| scripts/Game/GameMode/Respawn/SCR_RandomSpawnManagerComponent.c | 153 | GetSurfaceY | InitializeGrid | `if (m_World.GetSurfaceY(cellCenter[0], cellCenter[2]) <= WATER_LEVEL_THRESHOLD)` |
| scripts/Game/GameMode/Respawn/SCR_RandomSpawnManagerComponent.c | 287 | GetSurfaceY | TryGenerateSinglePoint | `surfaceY = m_World.GetSurfaceY(testPos[0], testPos[2]);` |
| scripts/Game/Generators/Common/SCR_ObstacleDetector.c | 228 | GetSurfaceY | HasObstacle | `float terrainY = world.GetSurfaceY(worldPos[0], worldPos[2]);` |
| scripts/Game/Generators/Common/SCR_ParallelShapeHelper.c | 176 | GetSurfaceY | SnapRelativePointsToGround | `worldPos[1] = world.GetSurfaceY(worldPos[0], worldPos[2]);` |
| scripts/Game/Generators/Common/SCR_RandomisationEditorData.c | 40 | GetSurfaceY | RandomiseExt | `float ySurface = world.GetSurfaceY(worldPos[0], worldPos[2]);` |
| scripts/Game/Generators/ForestGeneratorEntity.c | 939 | GetSurfaceY | GenerateEntities | `worldPos[1] = world.GetSurfaceY(worldPos[0], worldPos[2]);							// snaps to ground here` |
| scripts/Game/Generators/SCR_LineGeneratorBaseEntity.c | 77 | GetSurfaceY | ResetShapeNextPointHelper | `tempVector[1] = world.GetSurfaceY(tempVector[0], tempVector[2]) + m_vShapeOffset[1];` |
| scripts/Game/Generators/SCR_LineGeneratorBaseEntity.c | 91 | GetSurfaceY | ResetShapeNextPointHelper | `tempVector[1] = world.GetSurfaceY(tempVector[0], tempVector[2]) + m_vShapeOffset[1];` |
| scripts/Game/Generators/SCR_RoadGeneratorEntity.c | 85 | GetSurfaceY | CheckSlopes | `prevPoint[1] = world.GetSurfaceY(prevPoint[0], prevPoint[2]);` |
| scripts/Game/Generators/SCR_RoadGeneratorEntity.c | 90 | GetSurfaceY | CheckSlopes | `currPoint[1] = world.GetSurfaceY(currPoint[0], currPoint[2]);` |
| scripts/Game/Generators/SCR_RoadGeneratorEntity.c | 127 | GetSurfaceY | CheckSlopes | `sidePoint[1] = world.GetSurfaceY(sidePoint[0], sidePoint[2]);` |
| scripts/Game/Generators/SCR_RoadGeneratorEntity.c | 150 | GetSurfaceY | CheckSlopes | `leftRightPoints[i][1] = world.GetSurfaceY(sidePoint[0], sidePoint[2]);` |
| scripts/Game/Generators/SCR_ShapeAnalyserEntity.c | 257 | GetSurfaceY | PrintDebugInfo | `float tmp = pointRay.m_vPosition[1] - GetWorld().GetSurfaceY(pointRay.m_vPosition[0], pointRay.m_vPosition[2]);` |
| scripts/Game/Generators/WallGeneratorEntity.c | 698 | GetSurfaceY | CreateWallEntity | `absPos[1] = worldEditorAPI.GetWorld().GetSurfaceY(absPos[0], absPos[2]) + m_vShapeOffset[1];` |
| scripts/Game/Global/Functions.c | 1987 | GetSurfaceY | IsPositionWithinTerrainBounds | `const float surfaceY = game.GetWorld().GetSurfaceY(x, z);` |
| scripts/Game/Global/SCR_WorldTools.c | 116 | GetSurfaceY | FindEmptyTerrainPosition | `outPosition[1] = Math.Max(trace.Start[1] - traceCoef * traceOffset[1] + 0.01, world.GetSurfaceY(outPosition[0], outPosition[2]));` |
| scripts/Game/Global/SCR_WorldTools.c | 195 | GetSurfaceY | FindAllEmptyTerrainPositions | `position[1] = Math.Max(trace.Start[1] - traceCoef * traceOffset[1] + 0.01, world.GetSurfaceY(position[0], position[2]));` |
| scripts/Game/Helpers/SCR_EmptyPositionHelper.c | 108 | GetSurfaceY | TryFindNearbyFloorPosition | `float terrainHeight = world.GetSurfaceY(point[0], point[2]);` |
| scripts/Game/Helpers/SCR_EmptyPositionHelper.c | 168 | GetSurfaceY | TryGetValidFloorPosition | `float surfaceHeightWithBuffer = world.GetSurfaceY(position[0], position[2]) + traceHeight + TRACE_EPSILON;` |
| scripts/Game/Helpers/SCR_EmptyPositionHelper.c | 313 | GetSurfaceY | RaiseToTerrain | `float oceanHeightAtPos = world.GetSurfaceY(position[0], position[2]);` |
| scripts/Game/Helpers/SCR_EmptyPositionHelper.c | 325 | GetSurfaceY | GetMinYOnTerrainOrOcean | `float terrainMin = world.GetSurfaceY(position[0], position[2]) - TRACE_EPSILON;` |
| scripts/Game/Helpers/SCR_TerrainHelper.c | 36 | GetSurfaceY | GetTerrainY | `surfaceY = world.GetSurfaceY(pos[0], pos[2]);` |
| scripts/Game/Helpers/SCR_TerrainHelper.c | 55 | GetTerrainY | GetHeightAboveTerrain | `return pos[1] - GetTerrainY(pos, world, noUnderwater, trace);` |
| scripts/Game/Helpers/SCR_TerrainHelper.c | 79 | GetSurfaceY | GetTerrainNormal | `pos[1] = Math.Max(pos[1], world.GetSurfaceY(pos[0], pos[2]) + 0.01);` |
| scripts/Game/Helpers/SCR_TerrainHelper.c | 103 | GetSurfaceY | GetTerrainNormal | `float surfaceY = world.GetSurfaceY(pos[0], pos[2]);` |
| scripts/Game/Helpers/SCR_TerrainHelper.c | 176 | GetSurfaceY | SnapToGeometry | `currentPosition[1] = world.GetSurfaceY(currentPosition[0], currentPosition[2]);` |
| scripts/Game/Helpers/SCR_TerrainHelper.c | 177 | GetTerrainY | SnapToGeometry | `currentPosition[1] = SCR_TerrainHelper.GetTerrainY(currentPosition, world, true);` |
| scripts/Game/HitZone/SCR_FlammableHitZone.c | 628 | GetSurfaceY | StartDestructionGroundFire | `float surfaceY = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);` |
| scripts/Game/Map/ComponentsUI/SCR_MapRadialUI.c | 83 | GetSurfaceY | GetMenuWorldPosition | `m_vMenuWorldPos[1] = GetGame().GetWorld().GetSurfaceY(m_vMenuWorldPos[0], m_vMenuWorldPos[2]);` |
| scripts/Game/Map/Modules/SCR_MapCursorModule.c | 1177 | GetSurfaceY | UpdateCrosshairUI | `float heightASL = Math.Round(GetGame().GetWorld().GetSurfaceY(wX, wY) / m_iHeightPrecision) * m_iHeightPrecision;` |
| scripts/Game/ScenarioFramework/Actions/SCR_ScenarioFrameworkActionQRFDispacher.c | 328 | GetSurfaceY | StartQRFProcedure | `wpPosition[1] = GetGame().GetWorld().GetSurfaceY(wpPosition[0], wpPosition[2]);` |
| scripts/Game/ScenarioFramework/Components/SCR_ScenarioFrameworkSlotAI.c | 354 | GetSurfaceY | InitGroupComponents | `float surfaceY = world.GetSurfaceY(formationPosition[0], formationPosition[2]);` |
| scripts/Game/ScenarioFramework/Components/SCR_ScenarioFrameworkSlotTaskAI.c | 326 | GetSurfaceY | InitGroupComponents | `float surfaceY = world.GetSurfaceY(formationPosition[0], formationPosition[2]);` |
| scripts/Game/ScenarioFramework/Entities/Triggers/TriggerConditions/SCR_CustomTriggerConditionsAltitude.c | 63 | GetHeightAboveTerrain | Init | `aglHeight = SCR_TerrainHelper.GetHeightAboveTerrain(ent.GetOrigin());` |
| scripts/Game/Systems/SectorSpawn/SCR_SectorSpawn.c | 107 | GetSurfaceY | SpawnPreview | `position[1] = prefabSpawnPoint.GetWorld().GetSurfaceY(position[0], position[2]);` |
| scripts/Game/TaskSolver/SCR_ResupplyTaskSolver.c | 590 | GetSurfaceY | SpawnWaypoint | `float surfaceY = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);` |
| scripts/Game/UserActions/SCR_CampaignDeployMobileAssemblyUserAction.c | 130 | GetHeightAboveTerrain | CanBePerformedScript | `if (SCR_TerrainHelper.GetHeightAboveTerrain(pos) > MAX_TERRAIN_HEIGHT)` |
| scripts/WorkbenchGame/WorldEditor/GameModeSetup/GameModeSetupConfig.c | 140 | GetSurfaceY | GenerateWorld | `worldCenter[1] = world.GetSurfaceY(worldCenter[0], worldCenter[2]);` |
| scripts/WorkbenchGame/WorldEditor/ObjectBrush/SCR_ObjectBrushTool.c | 265 | GetSurfaceY | CreateObjects | `float terrainY = world.GetSurfaceY(point[0], point[2]);` |
| scripts/WorkbenchGame/WorldEditor/SCR_AutoSpawnerTool.c | 277 | GetSurfaceY | PlaceSelection | `float height = m_API.GetWorld().GetSurfaceY(pos[0], pos[2]);` |
| scripts/WorkbenchGame/WorldEditor/SCR_EntityReplacerTool.c | 191 | GetSurfaceY | CreateReplacements | `terrainY = baseWorld.GetSurfaceY(worldPos[0], worldPos[2]);` |
| scripts/WorkbenchGame/WorldEditor/SCR_FloatersFinderPlugin.c | 402 | GetSurfaceY | FilterEntities | `tempTerrainY = baseWorld.GetSurfaceY(tempPos[0], tempPos[2]);` |
| scripts/WorkbenchGame/WorldEditor/SCR_NormalAlignedFloatersFinderPlugin.c | 189 | GetSurfaceY | GetSusEntitySources | `float surfaceY = baseWorld.GetSurfaceY(entityPos[0], entityPos[2]);` |
| scripts/WorkbenchGame/WorldEditor/SCR_NormalAlignedFloatersFinderPlugin.c | 227 | GetSurfaceY | GetSusEntitySources | `diffY = corners[j][1] - baseWorld.GetSurfaceY(corners[j][0], corners[j][2]);` |

# Prefabs with RELATIVE_Y (0x80000) on a Flags line

Prefabs scanned 15457, with RELATIVE_Y 0.

