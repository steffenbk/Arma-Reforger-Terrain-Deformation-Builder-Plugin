MUD_VoxelTop.emat probe (2026-10-17): child of default_MatTerrainObj1 with CastShadow 1 + DisableInteriorProbes 0, used as MUD_VoxelProbe.MAT_TOP.
Result WB: no effect on tunnel sun spots (voxel tops are not drawn over uncarved cells; the heightfield is the surface there).
Also tried CastShadow 1 in the shared default_MatTerrainObj1.emat: no effect -> MatTerrainObject casts no sun shadow (or ignores CastShadow). Both reverted.
Call site: MUD_VoxelProbe.c MAT_TOP const (reverted to {D6FF1C7CCAA00596}).
