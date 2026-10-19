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
	float m_aEdgeLock[1089];    // m_aLock without buildings (kept terrain border only): the rut floor, same on every machine
	float m_aColH[1089];        // world Y of each vertex in the live collider (P1a wheel hold)
	// press / ahead / scrape arrays (patch memory Stage 1, 2026-10-16): made on the first write, null = all default
	// (~39 KB per patch saved where no wheel press, ahead hide, scrape or shown deformation ever touched it)
	ref MUD_PatchPress m_Press;
	bool m_bColValid;          // m_aColH is the live body (false: no body yet, asleep, or it failed)
	int m_aCell[16];            // per skin cell (4 x 4): -1 = kept terrain, else slot | TLBR
	int m_iRutMud;              // Build 3 Stage 4: rut mud cell mask of the last grid collider (-1 = no slot free; debug line on change)
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
	float m_fQueuedAt;          // m_fTime it entered m_aDirty (test kit 10 s line: oldest dirty age)
	bool m_bDeformed;           // some vertex moved: kept for the match, sent to joiners
	bool m_bFloor;              // RUT FIELD: a rut floor tile reaches it - may sleep, freed out of range (FLOOR_FREE); NOT sent / hashed
	bool m_bFloorWait;          // RUT FIELD settle (MUD_WorldGroundEntity.FLOOR_SETTLE): only floor dirt, held until quiet
	float m_fFloorFirstAt;      // m_fTime of the first held floor dirt (max wait)
	float m_fFloorDirtAt;       // m_fTime of the newest floor dirt (quiet)
	bool m_bLookDirty;          // client look ruts (MUD_LookRuts): mesh-only dirt, may build while the floor settle holds
	float m_fLookSince = -1;    // TEMP delay meter 2026-10-18: m_fTime the oldest undrawn look segment's wheel passed (-1 none)
	float m_fLookSpeed;         // TEMP delay meter: vehicle speed (m/s) of that cut
	float m_fLookCutAt = -1;    // TEMP split delay meter: m_fTime of the first look mark since the last mesh build (-1 none)
	bool m_bSheetHid;           // LAZY RUT MESH (MUD_MudSheet.LAZY_RUT_MESH): mesh hidden under the mud sheet, rebuilds held
	bool m_bSheetCov;           // ... last answer of MUD_MudSheet.Covers for this patch (MUD_WorldGroundEntity.SheetCovered)
	float m_fSheetAt = -100;    // ... m_fTime of that answer
	int m_iSheetGen = -1;       // ... MUD_MudSheet.CoverGen at that answer
	float m_fFieldSeenAt;       // RUT FIELD: m_fTime it was last in MUD_WorldGroundEntity.FieldDue's range (keeps a floor-only patch)
	bool m_bTraceHidden;       // TRACEABLE cleared by MUD_WorldGroundEntity.SetTraceHidden, to restore
	int m_iBuild;               // build steps left (MUD_WorldGroundEntity.BUILD_*): 0 = live, skin covered
	int m_iBuildFails;          // collider steps that made no body
	bool m_bAsleep;             // deformed, no vehicle near: no collider (skin collider under it), mesh kept
	float m_fMeshAt = -100;
	float m_fColAt = -100;
	float m_fColBuiltAt = -100; // PAPER TEST 3: m_fTime of the last collider body actually built (m_fColAt also moves on a skip)
	float m_fMeshBuiltAt = -100; // PERF IDEA 2: m_fTime of the last band mesh actually built (m_fMeshAt also moves on a skip;
	                             // -100 after a grid mesh = no skip against it)
	ref array<float> m_aMeshY;   // PERF IDEA 2: band mesher input Y per vertex of that build (made at the first band mesh:
	                             // a DS never has one)
	float m_fMeshHoldTo = -100;  // PERF IDEA 2 fix: a skipped patch is not picked by the mesh queue before this m_fTime
	// REBUILD 5 (2026-10-19, v240 = before; MUD_WorldGroundEntity RUT_PACE / SAME_SKIP / PARTIAL_IN)
	bool m_bRutMesh;             // option 1: the mesh dirt came from the rut floor alone (MarkFloorDirty) - RUT_MESH_EVERY pace
	bool m_bPaceHeld;            // option 1 debug: the old pace would have built it already
	ref array<float> m_aMeshD;   // option 2 / 3: band mesher input D of the last band mesh (with m_aMeshY)
	ref array<int> m_aMeshSkin;  // option 2 / 3: band mesher m_aSkin of that build
	int m_iMBx0 = -1;            // option 3: patch-local vertex box whose mesh input changed since that build (-1 = none)
	int m_iMBx1;
	int m_iMBz0;
	int m_iMBz1;
	bool m_bMBoxAll = true;      // option 3: a change with no box since that build - the next one reads every vertex
	bool m_bLastHideOwed;        // option 3: that build left hidden changes owed
	bool m_bLastRfOn;            // option 3: that build read the rut floor / look ruts
	float m_fHideUntil = -100;   // option 3: m_fTime until which a vertex may still be hidden (ahead ruts, HIDE_HOLD)
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
	ref MUD_IdxSet m_Idx;        // its layout's index lists (TEMP probe 2026-10-13, bricks: VOX layouts vary, the IDX_SETS ring
	                            // alone thrashed); cleared when it sleeps (MUD_WorldGroundEntity.SleepPatch) to bound memory

	//------------------------------------------------------------------------------------------------
	//! The press arrays, made now if missing (writers)
	MUD_PatchPress EnsurePress()
	{
		if (!m_Press)
			m_Press = new MUD_PatchPress();
		return m_Press;
	}

	// readers: null = the defaults MUD_PatchPress starts with
	float RutTouch(int v)
	{
		if (!m_Press)
			return -100;
		return m_Press.m_aRutTouch[v];
	}

	float BermTouch(int v)
	{
		if (!m_Press)
			return -100;
		return m_Press.m_aBermTouch[v];
	}

	float Show(int v)
	{
		if (!m_Press)
			return 0;
		return m_Press.m_aShow[v];
	}

	//! Mesh builds write every vertex: a 0 into a missing holder changes nothing, so it is made only for a non-0
	void SetShow(int v, float d)
	{
		if (!m_Press && d == 0)
			return;
		EnsurePress().m_aShow[v] = d;
	}

	float HideAt(int v)
	{
		if (!m_Press)
			return -100;
		return m_Press.m_aHideAt[v];
	}

	float ScrapeTouch(int v)
	{
		if (!m_Press)
			return -100;
		return m_Press.m_aScrapeTouch[v];
	}

	float RutFrom(int v)
	{
		if (!m_Press)
			return 0;
		return m_Press.m_aRutFrom[v];
	}

	float BermFrom(int v)
	{
		if (!m_Press)
			return 0;
		return m_Press.m_aBermFrom[v];
	}

	float PendOrig(int v)
	{
		if (!m_Press)
			return 0;
		return m_Press.m_aPendOrig[v];
	}

	float PendAt(int v)
	{
		if (!m_Press)
			return -1;
		return m_Press.m_aPendAt[v];
	}

	void ClearPend(int v)
	{
		if (m_Press)
			m_Press.m_aPendAt[v] = -1;
	}
}

//! MUD_FinePatch.m_Press: per-vertex arrays only the heightfield wheel press, ahead ruts / undo and body scrape use.
//! Defaults as MUD_WorldGroundEntity.InitPatch set them before 2026-10-16.
class MUD_PatchPress
{
	float m_aRutTouch[1089];    // last m_fTime a wheel was on it
	float m_aBermTouch[1089];
	float m_aShow[1089];        // ahead ruts (TEST 2026-10-11): deformation the mesh shows while the vertex is hidden
	float m_aHideAt[1089];      // ahead ruts: last m_fTime the vertex was in a wheel's hide corridor
	float m_aScrapeTouch[1089]; // body scrape: last m_fTime an underside point was over it (one cut step per pass)
	float m_aRutFrom[1089];     // m_aH when the current rut pass began (a moving press digs the full profile in one pass)
	float m_aBermFrom[1089];    // m_aH when the current berm pass began
	float m_aPendOrig[1089];    // ahead undo: m_aH before the first not-yet-reached ahead press (canonical At() copy only)
	float m_aPendAt[1089];      // ahead undo: m_fTime of the newest pending ahead press on it, -1 = none

	//------------------------------------------------------------------------------------------------
	void MUD_PatchPress()
	{
		for (int v = 0; v < 1089; v++)
		{
			m_aRutTouch[v] = -100;
			m_aBermTouch[v] = -100;
			m_aHideAt[v] = -100;
			m_aScrapeTouch[v] = -100;
			m_aPendAt[v] = -1;
			m_aShow[v] = 0;
			m_aRutFrom[v] = 0;
			m_aBermFrom[v] = 0;
			m_aPendOrig[v] = 0;
		}
	}
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
	ref MUD_IdxList m_RutRest = new MUD_IdxList();  // Build 3 Stage 4: one slot list without its rut mud cells
	ref MUD_IdxList m_RutMud = new MUD_IdxList();   // Build 3 Stage 4: rut mud cells of every slot (one collider piece)
	float m_aBlur[1521];        // smoothing window, up to 39 x 39
	float m_aBlurRows[1287];    // smoothing X pass, up to 33 x 39
}
