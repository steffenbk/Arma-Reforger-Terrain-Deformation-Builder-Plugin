//! Mud plan 3: entity of one 8 m fine patch (and of its dirt wall, task 8). Own class so
//! MUD_BuildingDestructionFix keeps it out of a ruin's sink-and-delete query and MUD_Surface.IsOurs knows it.
class MUD_FinePatchEntityClass : GenericEntityClass
{
}

class MUD_FinePatchEntity : GenericEntity
{
}

//! Edge Mode (spec 4.3): what the ground does where a hole meets kept terrain (road, water)
enum MUD_EEdgeMode
{
	LOCKED,         // edge verts never move, digging fades in over Edge Fade
	DIRT_WALL       // dig to the edge, a wall fills the gap (task 8)
}

//! One 8 m piece of deformable ground over 4 x 4 skin cells: 33 x 33 verts of 0.25 m, ~35 KB.
//! World Y of vertex v = m_aBase[v] + m_aH[v]. A vertex on a patch edge is stored in every patch
//! holding it; MUD_WorldGroundEntity.SetH writes all copies, so they never differ.
class MUD_FinePatch
{
	float m_aBase[1089];        // undeformed world Y: the skin triangles (incl. the skin's +1 mm)
	float m_aH[1089];           // deformation (m): < 0 dug, > 0 berm / rim / spoil
	float m_aSmooth[1089];      // m_aH blurred for the collider
	float m_aFloor[1089];       // deepest m_aH allowed (negative): Max Depth or a depth box
	float m_aLock[1089];        // 0 = never moves (kept terrain border, building), 1 = free
	float m_aRutTouch[1089];    // last m_fTime a wheel was on it
	float m_aBermTouch[1089];
	float m_aColH[1089];        // world Y of each vertex in the live collider (P1a wheel hold)
	bool m_bColValid;           // m_aColH is the live body (false: no body yet, asleep, or it failed)
	int m_aCell[16];            // per skin cell (4 x 4): -1 = kept terrain, else slot | TLBR
	float m_aCellSoft[16];      // 0 = hard (no ruts, no shovel) .. 1 = soft
	int m_aCellHard[16];        // 1 = hard game material (int: a fresh bool array reads true)
	ref array<ResourceName> m_aMats = {};   // collider surfaces, at most MUD_WorldGroundEntity.MAT_SLOTS
	int m_iPx;
	int m_iPz;
	int m_iKey;                 // m_iPz * m_iNPX + m_iPx
	IEntity m_Ent;
	ref Resource m_MeshRes;
	Physics m_Phys;
	ref array<PhysicsGeom> m_aGeoms = {};   // geoms of m_Phys: Physics.Destroy() leaves them, MUD_WorldGroundEntity.DropBody frees
	bool m_bMeshDirty;
	bool m_bColDirty;
	bool m_bQueued;             // key is in MUD_WorldGroundEntity.m_aDirty
	bool m_bDeformed;           // some vertex moved: kept for the match, sent to joiners
	bool m_bTraceHidden;        // TRACEABLE cleared by MUD_WorldGroundEntity.SetTraceHidden, to restore
	int m_iBuild;               // build steps left (MUD_WorldGroundEntity.BUILD_*): 0 = live, skin covered
	int m_iBuildFails;          // collider steps that made no body
	bool m_bAsleep;             // deformed, no vehicle near: no collider (skin collider under it), mesh kept
	float m_fMeshAt = -100;
	float m_fColAt = -100;
	float m_fColChangeAt = -100;  // TEMP probe 2026-10-11: m_fTime a collider rebuild last moved a vertex
	float m_fNearAt;            // m_fTime a vehicle was last near
	// patch-local vertex rect whose m_aSmooth is stale (-1 = clean)
	int m_iSx0 = -1;
	int m_iSx1;
	int m_iSz0;
	int m_iSz1;
	// server: patch-local vertex rect not yet sent to clients (-1 = nothing), task 9
	int m_iNx0 = -1;
	int m_iNx1;
	int m_iNz0;
	int m_iNz1;
	bool m_bNetQueued;
}

//! One triangle list, first m_iN entries used
class MUD_IdxList
{
	int m_aIdx[6144];
	int m_iN;
}

//! Every index list of one patch layout. They depend on MUD_FinePatch.m_aCell only, so patches with
//! the same 16 cells share one (cached in MUD_WorldGroundEntity.IdxSet). FillIndices costs ~1.4 ms
//! of script per call (measured 2026-10-09 Exp 192624, 50 calls 68 ms) and ran twice per patch build.
class MUD_IdxSet
{
	int m_aCell[16];
	ref MUD_IdxList m_Mesh = new MUD_IdxList();         // all cells, kept-terrain cells collapsed to a point
	ref array<ref MUD_IdxList> m_aSlot = {};            // collider triangles of each slot (index = slot)
	ref MUD_IdxList m_Shell = new MUD_IdxList();        // bullet slab: non-kept cells, winding reversed
	ref MUD_IdxList m_Plate = new MUD_IdxList();        // safety plate: non-kept cells
}

//! The skin cells around one patch, read once per new patch (MUD_WorldGroundEntity.FillCellCache). BaseAt +
//! EdgeLock asked the skin up to ~8700 times per patch: ~4.7 ms of the ~5 ms setup (measured 2026-10-09 13:40
//! Exp 192624: base 179 + lock 223 ms for 86 patches). Index = (cz - m_iZ0) * m_iNX + cx - m_iX0.
class MUD_CellCache
{
	int m_iX0;
	int m_iZ0;
	int m_iNX;                  // 0 = not filled (region too big), callers use the skin directly
	int m_iNZ;
	bool m_bAnyKept;            // some cell is kept terrain (or off the map)
	int m_aHole[256];           // 1 = hole cell (a skin cell), 0 = kept
	int m_aTlbr[256];
	float m_aH00[256];
	float m_aH10[256];
	float m_aH01[256];
	float m_aH11[256];
}

//! Scratch buffers shared by all patches (script class members max 256 KB: ~146 KB here)
class MUD_FineBuffers
{
	vector m_aVerts[1089];
	vector m_aUnder[1089];      // groove layer, m_fRevealGap under the surface
	vector m_aShell[1089];      // bullet slab: collider verts lowered by m_fBulletSlab, facing down
	vector m_aPlate[1089];      // safety plate: base + floor - m_fPlateGap
	float m_aUVs[2178];         // surface: same UVs as the skin chunk under it (0..1 per 64 m chunk)
	float m_aGrooveUVs[2178];   // groove layer: 0..1 per patch
	int m_aIndices[6144];       // this patch's triangles, diagonal per skin cell
	int m_aMeshIdx[6144];       // m_aIndices with kept-terrain cells collapsed to a point
	int m_aShellIdx[6144];      // m_aIndices with the winding reversed
	int m_aCellSlot[1024];      // collider slot per fine cell, -1 = kept terrain
	float m_aBlur[1521];        // smoothing window, up to 39 x 39
	float m_aBlurRows[1287];    // smoothing X pass, up to 33 x 39
}
