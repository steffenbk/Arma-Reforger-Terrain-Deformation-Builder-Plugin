[EntityEditorProps(category: "Mud", description: "Deformable ground plane that IS the terrain - mesh + full collider, ruts and craters", color: "120 80 40 255")]
class MUD_GroundEntityClass : GenericEntityClass
{
}

//! Height field + per-vertex pass timers. Own object because a script class may
//! hold at most 256KB of members (proven 2026-10-07).
class MUD_GroundData
{
	float m_aH[16641];          // local height per grid vertex, 129 x 129
	float m_aRutTouch[16641];   // last time a wheel was on it (s)
	float m_aBermTouch[16641];  // last time a wheel ring was on it (s)
}

//! Undeformed surface per grid vertex (local Y): terrain under it + lift, or 0 when flat.
//! Separate from MUD_GroundData, which is already ~200KB of the 256KB member limit.
class MUD_GroundBase
{
	float m_aBase[16641];
	float m_aSmooth[16641];     // m_aH blurred for the collider, see UpdateSmooth
	float m_aFloor[16641];      // deepest m_aH allowed: stays m_fFloorMargin above the terrain (0 = no digging)
}

//! Scratch buffers for one 33 x 33 patch, shared by all patches
class MUD_GroundPatchBuffers
{
	vector m_aVerts[1089];
	float m_aUVs[2178];
	int m_aIndices[6144];
	vector m_aUnder[1089];      // groove layer: same shape, m_fRevealGap under the surface
	vector m_aShell[1089];     // bullet floor: collider verts lowered by m_fBulletSlab
	int m_aShellIndices[6144];  // m_aIndices with the winding reversed (faces down)
	float m_aBlurRows[1287];    // UpdateSmooth X pass: up to 33 cols x (33 + 2 * 3) rows
	int m_aCellSlot[1024];      // BuildPatchCollider: material slot per cell (index into the patch's m_aMatIdx)
	int m_aSplitIdx[6144];      // BuildPatchCollider: triangles of one material
	int m_aMeshIdx[6144];       // BuildPatchMesh on a patch with holes: hole cells collapsed to a point
}

//! Surface map: game material per 4 x 4 grid cells (1 m at 32 m), 32 x 32 samples.
//! Value = index into MUD_GroundEntity.m_aSurfMats.
class MUD_GroundSurface
{
	int m_aIdx[1024];
	float m_aSoft[1024];        // 0 = hard (no ruts), 1 = soft; soft samples next to hard ones get less (rut fades out)
}

//! Holes (buildings, MUD_CutBoxEntity): per cell, and per vertex the distance to the nearest hole
class MUD_GroundCut
{
	int m_aCell[16384];         // 128 x 128 cells, 1 = building (int: a fresh bool array read true everywhere, 16:42 log)
	float m_aDist[16641];       // per vertex, in grid steps; 0 = touches a hole cell
}

//! One 8 m piece: its own render mesh and its own static collider, so a rut
//! only rebuilds the pieces it touches.
class MUD_GroundPatch
{
	IEntity m_Ent;
	ref Resource m_MeshRes;     // keeps the current mesh alive
	Physics m_Phys;
	ResourceName m_GameMat;     // most common surface in the patch (bullet floor, debug)
	ref array<int> m_aMatIdx = {};  // surfaces with their own collider piece, most common first (max MAT_SLOTS)
	int m_iPx;
	int m_iPz;
	bool m_bHasCut;             // some cells of this patch are holes
	bool m_bMeshDirty;
	// grid rect whose heights changed since this patch's last smoothing (-1 = clean)
	int m_iSx0 = -1;
	int m_iSx1;
	int m_iSz0;
	int m_iSz1;
	bool m_bColDirty;
	float m_fMeshAt = -100;     // m_fTime of the last mesh / collider build
	float m_fColAt = -100;
	bool m_bTraceHidden;       // TRACEABLE cleared by MUD_GroundEntity.HideFromTraces, to restore
	// server: grid rect (clipped to this patch's verts) not yet sent to clients (-1 = nothing)
	int m_iNx0 = -1;
	int m_iNx1;
	int m_iNz0;
	int m_iNz1;
}

//! Flat deformable ground for a world WITHOUT terrain. Visual mesh and collider
//! share one 129 x 129 height grid (0.25 m at 32 m). Split into 4 x 4 patches,
//! each a spawned GenericEntity with a static mesh (flags 0, rebuilt on change)
//! and a static trimesh body on the Terrain layer preset (what shipped
//! PBR_MASTER_GridPlane uses), so vehicles, wheels, soldiers and bullets all hit it.
//! Wheels sink it a few cm per pass, capped. Explosions dig craters.
//! Multiplayer: the server alone deforms (wheels, explosions) and sends changed heights to
//! clients in mm every collider tick; joiners get the whole field via RplSave/RplLoad
//! (pattern of shipped SCR_MPDestructionManager). Needs the RplComponent on the prefab;
//! without one every machine deforms locally.
class MUD_GroundEntity : GenericEntity
{
	static const int GRID = 129;        // verts per side
	static const int PATCHES = 4;       // patches per side
	static const int PCELLS = 32;       // cells per patch side
	static const int PVERTS = 33;       // verts per patch side
	static const int NET_MIN = -8192;   // synced height range in mm (+-8 m, 14 bits)
	static const int NET_MAX = 8191;
	static const int SURF = 32;         // surface samples per side (one per 4 x 4 cells)
	static const int SPP = 8;           // surface samples per patch side
	static const int MAT_SLOTS = 5;     // max surfaces per patch collider; rarer ones join the most common

	[Attribute("32", UIWidgets.EditBox, "Edge length (m). Grid is fixed 128 cells, so 32 m = 0.25 m spacing", category: "Ground")]
	protected float m_fSize;

	[Attribute("{6EBF7ADA9FD582F7}Assets/Structures/Military/Fortifications/DirtMounds/Data/DirtCover_01.emat", UIWidgets.ResourcePickerThumbnail, "Material. UVs run 0-1 per 8 m patch. Must load or no mesh is built.", "emat", category: "Ground")]
	protected ResourceName m_Material;

	[Attribute("{6EBF7ADA9FD582F7}Assets/Structures/Military/Fortifications/DirtMounds/Data/DirtCover_01.emat", UIWidgets.ResourcePickerThumbnail, "Groove material: a second layer just under the surface that shows through where ruts/craters go deeper than Reveal Depth. Empty = off.", "emat", category: "Ground")]
	protected ResourceName m_RutMaterial;

	[Attribute("0.06", UIWidgets.EditBox, "Ruts/craters deeper than this (m) show the groove material", category: "Ground")]
	protected float m_fRevealDepth;

	[Attribute("0.1", UIWidgets.EditBox, "Depth band (m) over which the surface sinks under the groove layer - bigger = softer, wider edge", category: "Ground")]
	protected float m_fRevealBlend;

	[Attribute("0.03", UIWidgets.EditBox, "How far the groove layer sits under the surface (m). Too small = flicker at distance.", category: "Ground")]
	protected float m_fRevealGap;

	// "material/default" (as in shipped debug scripts) logs "Unknown resource type, missing extension!" per collider build
	[Attribute("{C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat", UIWidgets.ResourcePickerThumbnail, "Collider game material: grip, wheel dust, hit effects, sounds. Fallback when auto finds nothing.", "gamemat", category: "Ground")]
	protected ResourceName m_GameMaterial;

	[Attribute("asphalt,concrete", UIWidgets.EditBox, "Hard surfaces: a game material whose path contains one of these words (comma separated) gets no ruts and no shovel digging. Explosions still crater it.", category: "Ground")]
	protected string m_sHardWords;

	[Attribute("{C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat", UIWidgets.ResourcePickerThumbnail, "Game material a hard surface turns into where an explosion broke it (from then on it takes ruts)", "gamemat", category: "Ground")]
	protected ResourceName m_CraterMaterial;

	[Attribute("1", UIWidgets.CheckBox, "Collider takes the game material of the terrain surface under it, sampled every 4 grid cells (1 m at 32 m): grip, dust and sounds follow the terrain paint", category: "Ground")]
	protected bool m_bAutoGameMaterial;

	[Attribute("1", UIWidgets.CheckBox, "Start from the terrain shape under the grid instead of a flat plane. No terrain = flat.", category: "Terrain")]
	protected bool m_bFollowTerrain;

	// Pad profile over the terrain: raised middle, edges curving down under the terrain so no border shows.
	// Anything dug below the terrain is hidden and blocked by it, so the pad height is the visible dig room.
	[Attribute("0.4", UIWidgets.EditBox, "Middle of the pad sits this far above the terrain (m) - room for ruts to show", category: "Terrain")]
	protected float m_fPadHeight;

	[Attribute("0.15", UIWidgets.EditBox, "Edges end this far below the terrain (m), hiding the border", category: "Terrain")]
	protected float m_fEdgeSink;

	[Attribute("5", UIWidgets.EditBox, "Width of the band (m) where the pad curves from full height down under the terrain. Half the size = a dome.", category: "Terrain")]
	protected float m_fEdgeFalloff;

	[Attribute("0", UIWidgets.EditBox, "Bridge pits: terrain dips up to this wide (m) get a flat mud lid at rim height; the space down to the pit floor is extra dig room for craters and trenches. Sculpt the pits in the terrain editor. 0 = off (mud follows every dip).", category: "Terrain")]
	protected float m_fPitWidth;

	[Attribute("1", UIWidgets.CheckBox, "Leave a hole under every building (Building class: houses, sheds...) on the grid. MUD_CutBoxEntity boxes always cut.", category: "Cutouts")]
	protected bool m_bCutBuildings;

	[Attribute("-1", UIWidgets.EditBox, "Grow (+) or shrink (-) a building's locked area (m). Negative lets ruts and craters reach a little inside the walls, so the edge hides under the house.", category: "Cutouts")]
	protected float m_fCutMargin;

	[Attribute("1.5", UIWidgets.EditBox, "Cut Holes only: the mud eases down under the terrain over this distance (m) around a building.", category: "Cutouts")]
	protected float m_fCutFalloff;

	[Attribute("0", UIWidgets.CheckBox, "On: no mud at all under buildings (hole, real terrain shows). Off: the mud goes on under them - put the buildings on top with the Snap to Mud plugin (World Editor, Plugins menu).", category: "Cutouts")]
	protected bool m_bCutHoles;

	[Attribute("0.05", UIWidgets.EditBox, "How far a wheel sinks the ground per pass (m)", category: "Ruts")]
	protected float m_fRutStep;

	[Attribute("0.35", UIWidgets.EditBox, "Deepest any rut gets below the undeformed surface (m), however heavy the vehicle. Keep under Pad Height or the bottom hides under the terrain.", category: "Ruts")]
	protected float m_fRutMaxDepth;

	[Attribute("0.08", UIWidgets.EditBox, "Rut depth (m) = this + vehicle mass in tons x Rut Depth Per Ton, capped at Rut Max Depth", category: "Ruts")]
	protected float m_fRutBaseDepth;

	[Attribute("0.025", UIWidgets.EditBox, "Extra rut depth per ton of vehicle mass (m). Debug log prints each vehicle's mass and depth.", category: "Ruts")]
	protected float m_fRutDepthPerTon;

	// 0.25 m (= one grid step) made 1-vertex grooves; the collider blur (1.25 m) kept only ~1/3 of
	// their depth, so wheels rode above the visible rut. 0.5 m spans ~3 verts and survives the blur.
	[Attribute("0.5", UIWidgets.EditBox, "Rut radius around a wheel contact (m). Keep at least 2x the grid step or the smoothed collider loses most of the depth.", category: "Ruts")]
	protected float m_fRutRadius;

	[Attribute("0.8", UIWidgets.EditBox, "Share of raised ground (crater rim, berm) a wheel plows away per pass, 0-1. 0 = rims only sink one Rut Step per pass.", category: "Ruts")]
	protected float m_fRimCut;

	[Attribute("0.9", UIWidgets.EditBox, "Outer radius of the berm pushed up beside a rut (m)", category: "Ruts")]
	protected float m_fBermRadius;

	[Attribute("0.08", UIWidgets.EditBox, "Max berm height above the flat level (m)", category: "Ruts")]
	protected float m_fBermHeight;

	[Attribute("0.6", UIWidgets.EditBox, "A vertex must be free of wheels this long (s) before the next touch counts as a new pass. A parked wheel does not keep sinking.", category: "Ruts")]
	protected float m_fPassGap;

	[Attribute("2.5", UIWidgets.EditBox, "Crater radius (m)", category: "Craters")]
	protected float m_fCraterRadius;

	[Attribute("2", UIWidgets.EditBox, "Crater depth at the centre (m)", category: "Craters")]
	protected float m_fCraterDepth;

	[Attribute("0.3", UIWidgets.EditBox, "Crater rim height (m), rim reaches out to 1.5x radius", category: "Craters")]
	protected float m_fCraterRimHeight;

	[Attribute("4", UIWidgets.EditBox, "Nothing digs deeper than this below the flat level (m)", category: "Craters")]
	protected float m_fMaxDepth;

	[Attribute("0.05", UIWidgets.EditBox, "On terrain: ruts and craters stop this far above the real terrain (m), so it never shows through. Pad Height sets how deep they can go.", category: "Craters")]
	protected float m_fFloorMargin;

	[Attribute("0.45", UIWidgets.EditBox, "Shovel: radius of the hole one swing digs (m)", category: "Digging")]
	protected float m_fDigRadius;

	[Attribute("0.08", UIWidgets.EditBox, "Shovel: how deep one swing digs at the centre (m)", category: "Digging")]
	protected float m_fDigStep;

	[Attribute("1.5", UIWidgets.EditBox, "Shovel: deepest a trench goes below the flat level (m). On terrain the floor (Pad Height - Floor Margin) usually stops it first.", category: "Digging")]
	protected float m_fDigMaxDepth;

	[Attribute("1.1", UIWidgets.EditBox, "Shovel: outer radius of the dug-out soil pile around the hole (m)", category: "Digging")]
	protected float m_fSpoilRadius;

	[Attribute("0.3", UIWidgets.EditBox, "Shovel: max height of the soil pile above the flat level (m)", category: "Digging")]
	protected float m_fSpoilHeight;

	[Attribute("0.1", UIWidgets.EditBox, "Min seconds between mesh rebuilds of changed patches", category: "Rebuild")]
	protected float m_fMeshInterval;

	[Attribute("0.2", UIWidgets.EditBox, "Min seconds between collider rebuilds of changed patches", category: "Rebuild")]
	protected float m_fColliderInterval;

	[Attribute("2", UIWidgets.Slider, "Collider smoothing radius in grid steps (0 = off, 2 = 5x5 = 1.25 m at 0.25 m). Only the ruts/craters are smoothed, not the terrain shape; the visible mesh keeps full detail.", "0 3 1", category: "Rebuild")]
	protected int m_iColliderSmooth;

	[Attribute("1.5", UIWidgets.EditBox, "Bullet floor this far under the collider (m), facing down, FireGeo layer only (no effect on soldiers/vehicles). Closes the ground into a slab so bullets cannot pass through it. 0 = off.", category: "Rebuild")]
	protected float m_fBulletSlab;

	[Attribute("1", UIWidgets.CheckBox, "Print debug info (rebuild counts and cost)", category: "Rebuild")]
	protected bool m_bDebug;

	[Attribute("0", UIWidgets.EditBox, "DEBUG: stop all deformation this many seconds after start (0 = never). Make ruts, then drive in them frozen: jitter left = rut shape, jitter gone = rebuilds.", category: "Rebuild")]
	protected float m_fFreezeAfter;
	protected bool m_bFrozen;

	protected ref MUD_GroundData m_Data;
	protected ref MUD_GroundBase m_Base;
	protected ref MUD_GroundPatchBuffers m_Buf;
	protected ref MUD_GroundSurface m_Surf;
	protected ref MUD_GroundCut m_Cut;
	protected ref array<float> m_aRaw;          // terrain height per vertex (local Y); null = no terrain / flat
	protected ref array<float> m_aLid;          // m_aRaw with pits up to m_fPitWidth filled to the rim
	protected int m_iCutCells;
	protected int m_iCutObjects;
	protected bool m_bSetupDone;
	protected ref array<ResourceName> m_aSurfMats = {};
	protected ref array<bool> m_aMatHard = {};      // per m_aSurfMats entry
	protected ref array<string> m_aHardWords = {};
	protected float m_fSinceWheelLog;
	protected ref array<ref MUD_GroundPatch> m_aPatches = {};
	protected ref array<IEntity> m_aFound = {};
	protected ref Resource m_PreviewRes;

	protected float m_fStep;
	protected float m_fHalf;
	protected float m_fTime;
	protected float m_fSinceCol;        // net send timer (server)
	protected bool m_bRender;
	protected bool m_bGroove;       // render the groove layer (m_RutMaterial set and loads)

	// debug stats, printed every 10 s while something rebuilds
	protected int m_iMeshBuilds;
	protected int m_iColBuilds;
	protected int m_iColMs;
	protected int m_iMeshMs;
	protected int m_iSmoothMs;
	protected ref array<float> m_aSmoothW = {};
	protected float m_fSinceStats;
	protected int m_iNetMsgs;
	protected int m_iNetInts;

	protected RplComponent m_RplComponent;

	// every ground in play mode, for IsOnAnyGround and HideFromTraces
	protected static ref array<MUD_GroundEntity> s_aAll = {};

	//------------------------------------------------------------------------------------------------
	//! True if world position wp lies over the grid of any mud ground (XZ only, in its local frame)
	static bool IsOnAnyGround(vector wp)
	{
		foreach (MUD_GroundEntity g : s_aAll)
		{
			if (!g)
				continue;
			vector lp = g.CoordToLocal(wp);
			if (Math.AbsFloat(lp[0]) <= g.m_fHalf && Math.AbsFloat(lp[2]) <= g.m_fHalf)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True if e is one of the patch entities a mud ground spawned (MUD_BuildingDestructionFix)
	static bool IsMudPatch(IEntity e)
	{
		if (!e)
			return false;
		foreach (MUD_GroundEntity g : s_aAll)
		{
			if (!g)
				continue;
			foreach (MUD_GroundPatch p : g.m_aPatches)
			{
				if (p.m_Ent == e)
					return true;
			}
		}
		return false;
	}
	//------------------------------------------------------------------------------------------------
	//! Server: one shovel swing at world position wp, on every ground under it. False if none.
	static bool DigAt(vector wp)
	{
		bool any;
		foreach (MUD_GroundEntity g : s_aAll)
		{
			if (g && g.m_Data && !g.IsProxy() && g.Dig(g.CoordToLocal(wp)))
				any = true;
		}
		return any;
	}

	protected static int s_iTraceHideDepth;

	//------------------------------------------------------------------------------------------------
	//! Hide every mud patch from traces (hide = true) or bring them back (false). Calls nest.
	//! For the native vehicle get in/out check: its box starts exactly at the soldier's feet (stance
	//! shape offset Y 0, Character_Base.et ColliderStand), so standing on our mud it always touches
	//! it - real terrain is never tested there (VehicleActionObstructionTrace, flags ENTS|ANY_CONTACT).
	//! Also held across frames while someone gets in/out (MUD_VehicleAccess.HoldWhileMoving): every
	//! trace on this machine misses ALL mud grounds meanwhile.
	static void HideFromTraces(bool hide)
	{
		if (hide)
		{
			s_iTraceHideDepth++;
			if (s_iTraceHideDepth > 1)
				return;
		}
		else
		{
			if (s_iTraceHideDepth <= 0)
				return;
			s_iTraceHideDepth--;
			if (s_iTraceHideDepth > 0)
				return;
		}

		foreach (MUD_GroundEntity g : s_aAll)
		{
			if (!g)
				continue;
			foreach (MUD_GroundPatch p : g.m_aPatches)
			{
				if (!p || !p.m_Ent)
					continue;
				if (hide)
				{
					p.m_bTraceHidden = (p.m_Ent.GetFlags() & EntityFlags.TRACEABLE) != 0;
					if (p.m_bTraceHidden)
						p.m_Ent.ClearFlags(EntityFlags.TRACEABLE, false);
				}
				else if (p.m_bTraceHidden)
				{
					p.m_Ent.SetFlags(EntityFlags.TRACEABLE, false);
					p.m_bTraceHidden = false;
				}
			}
		}
		// plan 3: the world ground's fine patches lie under parked vehicles too
		MUD_WorldGroundEntity.SetTraceHidden(hide);
		// plan 3 task 1 row R6 FAILED (get in from the skin): hide the skin chunks too
		MUD_CoarseSkinEntity.SetTraceHidden(hide);
		// mud sheet colliders (2026-10-15: get in / out blocked on the mud; the native check ignores layers)
		MUD_MudSheet.SetTraceHidden(hide);
		// voxel bricks (2026-10-15: can't get in / out in a rut, they were never hidden)
		MUD_VoxelProbe.SetTraceHidden(hide);
	}

	//------------------------------------------------------------------------------------------------
	//! New play session on a map without MUD_GroundEntity (MUD_WorldGroundEntity.Setup): statics outlive
	//! the session, its call queue (MUD_VehicleAccess.Tick) does not
	static void ResetTraceHide()
	{
		s_iTraceHideDepth = 0;
		MUD_VehicleAccess.Reset();
	}

	protected ref array<int> m_aJipMm;          // field from RplLoad that arrived before EOnInit, mm per vertex

	//------------------------------------------------------------------------------------------------
	void MUD_GroundEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	void ~MUD_GroundEntity()
	{
		SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect().Remove(OnExplosion);
		if (s_aAll)
			s_aAll.RemoveItem(this);

		foreach (MUD_GroundPatch p : m_aPatches)
		{
			if (p && p.m_Ent)
				delete p.m_Ent;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected override void EOnInit(IEntity owner)
	{
		m_fStep = m_fSize / (GRID - 1);
		m_fHalf = m_fSize * 0.5;

		// edit mode: one flat quad so it can be seen and placed, no patches spawned
		if (!GetGame().InPlayMode())
		{
			ClearEventMask(EntityEvent.FRAME);
			BuildPreview();
			return;
		}
		// the rest runs on the first frame (Setup): the house search needs the rest of the world
		// loaded, and in EOnInit it found nothing (2026-10-08 16:02 log: "cut 0 objects")
	}

	//------------------------------------------------------------------------------------------------
	protected void Setup()
	{
		// dedicated server needs the collider (vehicles and soldiers are simulated there) but no mesh
		m_bRender = RplSession.Mode() != RplMode.Dedicated;
		m_bGroove = m_bRender && !m_RutMaterial.IsEmpty();
		m_RplComponent = RplComponent.Cast(FindComponent(RplComponent));

		m_Data = new MUD_GroundData();
		m_Base = new MUD_GroundBase();
		m_Buf = new MUD_GroundPatchBuffers();
		for (int i = 0; i < GRID * GRID; i++)
		{
			m_Data.m_aRutTouch[i] = -100;
			m_Data.m_aBermTouch[i] = -100;
			m_Base.m_aFloor[i] = -m_fMaxDepth;  // flat (no terrain); SampleTerrain sets the real floor
		}

		// holes first: SampleTerrain sinks the mud under the terrain around them
		int cutMs = CollectCuts();

		bool onTerrain;
		if (m_bFollowTerrain)
			onTerrain = SampleTerrain();

		m_Surf = new MUD_GroundSurface();
		int surfMs = SampleSurfaces();

		// collider blur weights = Pascal row 2r (r 2 -> 1 4 6 4 1); r 0 = plain copy
		int r = Math.ClampInt(m_iColliderSmooth, 0, 3);
		m_aSmoothW.Insert(1);
		for (int k = 1; k <= 2 * r; k++)
			m_aSmoothW.Insert(m_aSmoothW[k - 1] * (2 * r - k + 1) / k);

		BuildPatchIndices();
		SpawnPatches();

		if (m_aJipMm)
		{
			ApplyFieldMm(m_aJipMm);
			m_aJipMm = null;
		}

		SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect().Insert(OnExplosion);
		// first ground of a play session: drop hide state a previous session may have left (statics
		// outlive the session; its call queue, and with it MUD_VehicleAccess.Tick, does not)
		if (s_aAll.IsEmpty())
		{
			s_iTraceHideDepth = 0;
			MUD_VehicleAccess.Reset();
		}
		s_aAll.Insert(this);

		if (m_bDebug)
		{
			Print(string.Format("MUD ground: %1 m, step %2 m, %3 patches, render %4, on terrain %5, rpl %6, proxy %7", m_fSize, m_fStep, m_aPatches.Count(), m_bRender, onTerrain, m_RplComponent != null, IsProxy()));
			Print(string.Format("MUD ground: surface map %1 samples in %2 ms, %3 surfaces: %4", SURF * SURF, surfMs, m_aSurfMats.Count(), m_aSurfMats));
			Print(string.Format("MUD ground: cut %1 objects, %2 of %3 cells, %4 ms", m_iCutObjects, m_iCutCells, (GRID - 1) * (GRID - 1), cutMs));
			foreach (int i2, MUD_GroundPatch p : m_aPatches)
				Print(string.Format("MUD ground: patch %1 surfaces %2, main %3", i2, p.m_aMatIdx, p.m_GameMat));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fill m_Base from the terrain height under every grid vertex. False (base stays flat) when
	//! there is no terrain: GetSurfaceY then returns its -256 default (IDA, not tested in-engine),
	//! so anything at or below -255 counts as "no terrain".
	protected bool SampleTerrain()
	{
		if (!SampleLid())
		{
			for (int i = 0; i < GRID * GRID; i++)
			{
				m_Base.m_aBase[i] = 0;
				m_Base.m_aFloor[i] = -m_fMaxDepth;
			}
			return false;
		}

		for (int z = 0; z < GRID; z++)
		{
			for (int x = 0; x < GRID; x++)
			{
				int k = z * GRID + x;
				float lift = Lift(x, z);
				float pit = LidAt(x, z) - m_aRaw[k];   // how far the lid bridges over a pit here
				m_Base.m_aBase[k] = m_aRaw[k] + pit + lift;

				// down to the real terrain (pit floor included), minus the margin; the groove layer sits
				// m_fRevealGap under the dug surface, so leave room for it too - same on server and
				// clients (m_RutMaterial, not m_bGroove, which is off on a dedicated server)
				float room = lift + pit - m_fFloorMargin;
				if (!m_RutMaterial.IsEmpty())
					room -= m_fRevealGap;
				m_Base.m_aFloor[k] = Math.Min(-room, 0);
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Fill m_aRaw (terrain height, this entity's local Y, per grid vertex) and m_aLid (the same
	//! with every dip up to m_fPitWidth wide filled to its rim: a morphological closing, max then min
	//! over a square window). False when there is no terrain under the grid.
	protected bool SampleLid()
	{
		int t0 = System.GetTickCount();
		BaseWorld world = GetWorld();
		int count = GRID * GRID;
		m_aRaw = {};
		m_aRaw.Resize(count);
		m_aLid = {};
		m_aLid.Resize(count);
		for (int z = 0; z < GRID; z++)
		{
			for (int x = 0; x < GRID; x++)
			{
				vector wp = CoordToParent(Vector(x * m_fStep - m_fHalf, 0, z * m_fStep - m_fHalf));
				float sy = world.GetSurfaceY(wp[0], wp[2]);
				if (sy <= -255)
				{
					m_aRaw = null;
					m_aLid = null;
					return false;
				}
				wp[1] = sy;
				float h = CoordToLocal(wp)[1];
				m_aRaw[z * GRID + x] = h;
				m_aLid[z * GRID + x] = h;
			}
		}

		if (m_fPitWidth <= 0)
			return true;

		int rc = Math.Ceil(m_fPitWidth * 0.5 / m_fStep);   // int var first: a float straight into an int slot reads raw bits
		int r = Math.ClampInt(rc, 1, GRID);
		array<float> tmp = {};
		tmp.Resize(count);
		Morph(m_aLid, tmp, true, true, r);
		Morph(tmp, m_aLid, false, true, r);
		Morph(m_aLid, tmp, true, false, r);
		Morph(tmp, m_aLid, false, false, r);

		float deepest = 0;
		int bridged = 0;
		for (int i = 0; i < count; i++)
		{
			float fill = m_aLid[i] - m_aRaw[i];
			if (fill < 0)
			{
				m_aLid[i] = m_aRaw[i];
				fill = 0;
			}
			if (fill > 0.05)
				bridged++;
			deepest = Math.Max(deepest, fill);
		}
		if (m_bDebug)
			Print(string.Format("MUD ground: pit lid - window %1 m, %2 of %3 vertices bridged (> 5 cm), deepest %4 m, %5 ms", (2 * r + 1) * m_fStep, bridged, count, deepest, System.GetTickCount(t0)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One pass of a running max (takeMax) or min over a 2r+1 window, along X or Z rows of the grid,
	//! src -> dst. van Herk / Gil-Werman: 3 compares per vertex whatever the window size. Outside
	//! the grid counts as "no value" (window cut at the border).
	protected void Morph(array<float> src, array<float> dst, bool alongX, bool takeMax, int r)
	{
		int k = 2 * r + 1;
		int len = GRID + 2 * r;
		float pad = 1000000;
		if (takeMax)
			pad = -1000000;
		array<float> line = {};
		array<float> g = {};
		array<float> hh = {};
		line.Resize(len);
		g.Resize(len);
		hh.Resize(len);

		for (int row = 0; row < GRID; row++)
		{
			for (int i = 0; i < len; i++)
			{
				int o = i - r;
				float v = pad;
				if (o >= 0 && o < GRID)
				{
					if (alongX)
						v = src[row * GRID + o];
					else
						v = src[o * GRID + row];
				}
				line[i] = v;
			}

			// g: running op from the start of each k-block; hh: from the end of each k-block
			for (int i2 = 0; i2 < len; i2++)
			{
				float a = line[i2];
				if (i2 % k != 0)
				{
					if (takeMax)
						a = Math.Max(a, g[i2 - 1]);
					else
						a = Math.Min(a, g[i2 - 1]);
				}
				g[i2] = a;
			}
			for (int i3 = len - 1; i3 >= 0; i3--)
			{
				float b = line[i3];
				if (i3 + 1 < len && (i3 + 1) % k != 0)
				{
					if (takeMax)
						b = Math.Max(b, hh[i3 + 1]);
					else
						b = Math.Min(b, hh[i3 + 1]);
				}
				hh[i3] = b;
			}

			// window line[o2 .. o2 + 2r] = grid o2 - r .. o2 + r
			for (int o2 = 0; o2 < GRID; o2++)
			{
				float res;
				if (takeMax)
					res = Math.Max(hh[o2], g[o2 + 2 * r]);
				else
					res = Math.Min(hh[o2], g[o2 + 2 * r]);
				if (alongX)
					dst[row * GRID + o2] = res;
				else
					dst[o2 * GRID + row] = res;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Lid height (local Y) at grid vertex x, z, eased back to the raw terrain over the pad's edge
	//! band so the border still dives under the real terrain even where a pit reaches it
	protected float LidAt(int x, int z)
	{
		int k = z * GRID + x;
		float span = m_fPadHeight + m_fEdgeSink;
		float s = 1;
		if (span > 0.001)
			s = Math.Clamp((PadLift(x, z) + m_fEdgeSink) / span, 0, 1);
		return m_aRaw[k] + (m_aLid[k] - m_aRaw[k]) * s;
	}

	//------------------------------------------------------------------------------------------------
	//! Undeformed surface (local Y) at grid vertex x, z: lid + pad shape (holes ignored)
	protected float RestSurface(int x, int z)
	{
		return LidAt(x, z) + PadLift(x, z);
	}

	//------------------------------------------------------------------------------------------------
	//! Height above the terrain at grid vertex x, z: m_fPadHeight in the middle, -m_fEdgeSink at the
	//! border. Product of two smoothsteps (one per axis) so the corners round off without creases.
	protected float PadLift(int x, int z)
	{
		return PadLiftAt(x * m_fStep - m_fHalf, z * m_fStep - m_fHalf);
	}

	//------------------------------------------------------------------------------------------------
	//! PadLift at any local point (lx, lz), not only on vertices
	protected float PadLiftAt(float lx, float lz)
	{
		float ex = m_fHalf - Math.AbsFloat(lx);    // distance to the nearest X edge (m)
		float ez = m_fHalf - Math.AbsFloat(lz);
		float fall = Math.Max(m_fEdgeFalloff, 0.01);
		float sx = Math.Clamp(ex / fall, 0, 1);
		float sz = Math.Clamp(ez / fall, 0, 1);
		sx = sx * sx * (3 - 2 * sx);
		sz = sz * sz * (3 - 2 * sz);
		return -m_fEdgeSink + (m_fPadHeight + m_fEdgeSink) * sx * sz;
	}

	//------------------------------------------------------------------------------------------------
	//! Height above the terrain at grid vertex x, z: the pad shape. With holes, an ease down to
	//! -m_fEdgeSink around them (mud dives under the terrain at the wall instead of ending in a
	//! step). Without, buildings stand on the mud (Snap to Mud plugin) and the pad stays as it is.
	protected float Lift(int x, int z)
	{
		float lift = PadLift(x, z);
		if (!m_bCutHoles)
			return lift;
		float d = m_Cut.m_aDist[z * GRID + x] * m_fStep;
		float fall = Math.Max(m_fCutFalloff, 0.01);
		if (d >= fall)
			return lift;
		float s = d / fall;
		s = s * s * (3 - 2 * s);
		return Math.Min(lift, -m_fEdgeSink + (m_fPadHeight + m_fEdgeSink) * s);
	}

	//------------------------------------------------------------------------------------------------
	//! Mud surface height (world Y) at world point wp before any deformation - what the ground
	//! builds in play. False when wp is not over this ground. Works in edit mode (Snap to Mud plugin).
	//! Ignores holes (Cut Holes): there the building stands on the real terrain anyway.
	bool MudSurfaceY(vector wp, out float y)
	{
		vector lp = CoordToLocal(wp);
		if (Math.AbsFloat(lp[0]) > m_fHalf || Math.AbsFloat(lp[2]) > m_fHalf)
			return false;
		lp[1] = 0;
		if (!m_bFollowTerrain)
		{
			y = CoordToParent(lp)[1];
			return true;
		}
		if (!m_aLid)
			return false;   // no terrain under the grid

		// bilinear over the 4 grid vertices around lp, same surface the ground builds (lid + pad)
		float fx = (lp[0] + m_fHalf) / m_fStep;
		float fz = (lp[2] + m_fHalf) / m_fStep;
		int ix = Math.Floor(fx);
		int iz = Math.Floor(fz);
		ix = Math.ClampInt(ix, 0, GRID - 2);
		iz = Math.ClampInt(iz, 0, GRID - 2);
		float tx = Math.Clamp(fx - ix, 0, 1);
		float tz = Math.Clamp(fz - iz, 0, 1);
		float h0 = RestSurface(ix, iz) * (1 - tx) + RestSurface(ix + 1, iz) * tx;
		float h1 = RestSurface(ix, iz + 1) * (1 - tx) + RestSurface(ix + 1, iz + 1) * tx;
		lp[1] = h0 * (1 - tz) + h1 * tz;
		y = CoordToParent(lp)[1];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when grid vertex x, z is locked by a building (touches a cell under its footprint +
	//! Cut Margin): no ruts, craters or digging there. Outside it the mud deforms right up to the wall.
	protected bool AtCut(int x, int z)
	{
		return m_Cut.m_aDist[z * GRID + x] <= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Fill m_Cut from buildings (Building class, if m_bCutBuildings) and MUD_CutBoxEntity boxes
	//! touching the ground, then the per-vertex distance to the nearest hole. Read once at start:
	//! the same static world on server and clients gives the same holes. Returns the time taken (ms).
	protected int CollectCuts()
	{
		int t0 = System.GetTickCount();
		m_Cut = new MUD_GroundCut();
		for (int ic = 0; ic < 16384; ic++)
			m_Cut.m_aCell[ic] = 0;
		m_iCutCells = 0;
		m_iCutObjects = 0;

		// world AABB of the grid's 4 corners
		vector wmin, wmax;
		for (int i = 0; i < 4; i++)
		{
			float lx = m_fHalf;
			if (i == 1 || i == 3)
				lx = -m_fHalf;
			float lz = m_fHalf;
			if (i >= 2)
				lz = -m_fHalf;
			vector c = CoordToParent(Vector(lx, 0, lz));
			if (i == 0)
			{
				wmin = c;
				wmax = c;
				continue;
			}
			wmin[0] = Math.Min(wmin[0], c[0]);
			wmin[2] = Math.Min(wmin[2], c[2]);
			wmax[0] = Math.Max(wmax[0], c[0]);
			wmax[2] = Math.Max(wmax[2], c[2]);
		}
		wmin[1] = GetOrigin()[1] - 100;
		wmax[1] = GetOrigin()[1] + 100;

		m_aFound.Clear();
		GetWorld().QueryEntitiesByAABB(wmin, wmax, AddEntity);
		if (m_bDebug)
		{
			array<string> kinds = {};
			foreach (IEntity fe : m_aFound)
			{
				string kind = fe.ClassName();
				if (kinds.Find(kind) < 0 && kinds.Count() < 20)
					kinds.Insert(kind);
			}
			Print(string.Format("MUD ground: cut search found %1 entities, classes %2", m_aFound.Count(), kinds));
		}
		foreach (IEntity e : m_aFound)
		{
			vector bmin, bmax;
			MUD_CutBoxEntity box = MUD_CutBoxEntity.Cast(e);
			if (box)
			{
				box.GetBox(bmin, bmax);
			}
			else if (m_bCutBuildings && Building.Cast(e))
			{
				e.GetBounds(bmin, bmax);
				bmin[0] = bmin[0] - m_fCutMargin;
				bmin[2] = bmin[2] - m_fCutMargin;
				bmax[0] = bmax[0] + m_fCutMargin;
				bmax[2] = bmax[2] + m_fCutMargin;
			}
			else
			{
				continue;
			}

			// empty bounds cut nothing (every cell fails the box test) but still get a debug line
			int cells = CutFootprint(e, bmin, bmax);
			if (cells > 0)
				m_iCutObjects++;
			if (m_bDebug)
				Print(string.Format("MUD ground: cut %1 cells for %2 at local %3, bounds %4 .. %5", cells, e.ClassName(), CoordToLocal(e.GetOrigin()), bmin, bmax));
		}
		m_aFound.Clear();

		// chamfer distance (grid steps) from every vertex to the nearest vertex of a hole cell
		float big = 1000000;
		for (int v = 0; v < GRID * GRID; v++)
			m_Cut.m_aDist[v] = big;
		if (m_iCutCells == 0)
			return System.GetTickCount(t0);

		int cellsPerSide = GRID - 1;
		for (int cz = 0; cz < cellsPerSide; cz++)
		{
			for (int cx = 0; cx < cellsPerSide; cx++)
			{
				if (m_Cut.m_aCell[cz * cellsPerSide + cx] == 0)
					continue;
				int bl = cz * GRID + cx;
				m_Cut.m_aDist[bl] = 0;
				m_Cut.m_aDist[bl + 1] = 0;
				m_Cut.m_aDist[bl + GRID] = 0;
				m_Cut.m_aDist[bl + GRID + 1] = 0;
			}
		}

		float diag = 1.414;
		for (int z = 0; z < GRID; z++)
		{
			for (int x = 0; x < GRID; x++)
			{
				int k = z * GRID + x;
				float d = m_Cut.m_aDist[k];
				if (x > 0)
					d = Math.Min(d, m_Cut.m_aDist[k - 1] + 1);
				if (z > 0)
				{
					d = Math.Min(d, m_Cut.m_aDist[k - GRID] + 1);
					if (x > 0)
						d = Math.Min(d, m_Cut.m_aDist[k - GRID - 1] + diag);
					if (x < GRID - 1)
						d = Math.Min(d, m_Cut.m_aDist[k - GRID + 1] + diag);
				}
				m_Cut.m_aDist[k] = d;
			}
		}
		for (int z2 = GRID - 1; z2 >= 0; z2--)
		{
			for (int x2 = GRID - 1; x2 >= 0; x2--)
			{
				int k2 = z2 * GRID + x2;
				float d2 = m_Cut.m_aDist[k2];
				if (x2 < GRID - 1)
					d2 = Math.Min(d2, m_Cut.m_aDist[k2 + 1] + 1);
				if (z2 < GRID - 1)
				{
					d2 = Math.Min(d2, m_Cut.m_aDist[k2 + GRID] + 1);
					if (x2 < GRID - 1)
						d2 = Math.Min(d2, m_Cut.m_aDist[k2 + GRID + 1] + diag);
					if (x2 > 0)
						d2 = Math.Min(d2, m_Cut.m_aDist[k2 + GRID - 1] + diag);
				}
				m_Cut.m_aDist[k2] = d2;
			}
		}

		return System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	//! Mark every cell whose centre lies inside box [bmin..bmax] (XZ, local space of e). Returns the
	//! number of cells newly cut.
	protected int CutFootprint(IEntity e, vector bmin, vector bmax)
	{
		// world transform, not e.CoordToParent/CoordToLocal: those go to the PARENT's space, and
		// house parts (and houses in compositions) are children - every footprint came out off-grid
		// (2026-10-08 16:32 log: 0 cells for a 10 x 13 m house inside the ground)
		vector em[4];
		e.GetWorldTransform(em);

		// grid cell range covering the footprint's 4 corners
		float ax = 1000000;
		float az = 1000000;
		float bx = -1000000;
		float bz = -1000000;
		for (int i = 0; i < 4; i++)
		{
			float fx = bmin[0];
			if (i == 1 || i == 3)
				fx = bmax[0];
			float fz = bmin[2];
			if (i >= 2)
				fz = bmax[2];
			vector lp = CoordToLocal(ToWorld(em, Vector(fx, 0, fz)));
			ax = Math.Min(ax, lp[0]);
			az = Math.Min(az, lp[2]);
			bx = Math.Max(bx, lp[0]);
			bz = Math.Max(bz, lp[2]);
		}

		int last = GRID - 2;
		int cx0 = Math.Floor((ax + m_fHalf) / m_fStep);
		int cx1 = Math.Floor((bx + m_fHalf) / m_fStep);
		int cz0 = Math.Floor((az + m_fHalf) / m_fStep);
		int cz1 = Math.Floor((bz + m_fHalf) / m_fStep);
		if (cx1 < 0 || cz1 < 0 || cx0 > last || cz0 > last)
		{
			if (m_bDebug)
				Print(string.Format("MUD ground: footprint off grid - local x %1..%2 z %3..%4 -> cells x %5..%6 z %7..%8", ax, bx, az, bz, cx0, cx1, cz0, cz1));
			return 0;
		}
		cx0 = Math.ClampInt(cx0, 0, last);
		cx1 = Math.ClampInt(cx1, 0, last);
		cz0 = Math.ClampInt(cz0, 0, last);
		cz1 = Math.ClampInt(cz1, 0, last);

		if (m_bDebug)
		{
			int mx = (cx0 + cx1) / 2;
			int mz = (cz0 + cz1) / 2;
			vector mlp = Vector((mx + 0.5) * m_fStep - m_fHalf, 0, (mz + 0.5) * m_fStep - m_fHalf);
			Print(string.Format("MUD ground: footprint local x %1..%2 z %3..%4 -> cells x %5..%6 z %7..%8", ax, bx, az, bz, cx0, cx1, cz0, cz1));
			Print(string.Format("MUD ground: mid cell local %1 -> building local %2, already cut %3", mlp, ToBox(em, CoordToParent(mlp)), m_Cut.m_aCell[mz * (GRID - 1) + mx]));
		}

		int n;
		int tried;
		int wasCut;
		int outside;
		vector firstOut;
		for (int cz = cz0; cz <= cz1; cz++)
		{
			for (int cx = cx0; cx <= cx1; cx++)
			{
				tried++;
				int ci = cz * (GRID - 1) + cx;
				if (m_Cut.m_aCell[ci] != 0)
				{
					wasCut++;
					continue;
				}
				float clx = cx * m_fStep + 0.5 * m_fStep - m_fHalf;
				float clz = cz * m_fStep + 0.5 * m_fStep - m_fHalf;
				vector bp = ToBox(em, CoordToParent(Vector(clx, 0, clz)));
				float bpx = bp[0];
				float bpz = bp[2];
				bool inX = bpx >= bmin[0] && bpx <= bmax[0];
				bool inZ = bpz >= bmin[2] && bpz <= bmax[2];
				if (!inX || !inZ)
				{
					if (outside == 0)
						firstOut = bp;
					outside++;
					continue;
				}
				m_Cut.m_aCell[ci] = 1;
				n++;
			}
		}
		if (m_bDebug)
			Print(string.Format("MUD ground: cell test - tried %1, already cut %2, outside %3 (first %4), cut %5, box x %6..%7 z %8..%9", tried, wasCut, outside, firstOut, n, bmin[0], bmax[0], bmin[2], bmax[2]));
		m_iCutCells += n;
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! Point p in the space of world transform m -> world
	protected vector ToWorld(vector m[4], vector p)
	{
		return m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3];
	}

	//------------------------------------------------------------------------------------------------
	//! World point w -> the space of world transform m (rotation + scale, no shear)
	protected vector ToBox(vector m[4], vector w)
	{
		vector d = w - m[3];
		return Vector(vector.Dot(d, m[0]) / m[0].LengthSq(), vector.Dot(d, m[1]) / m[1].LengthSq(), vector.Dot(d, m[2]) / m[2].LengthSq());
	}

	//------------------------------------------------------------------------------------------------
	protected bool PatchHasCut(int px, int pz)
	{
		for (int cz = 0; cz < PCELLS; cz++)
		{
			for (int cx = 0; cx < PCELLS; cx++)
			{
				if (m_Cut.m_aCell[(pz * PCELLS + cz) * (GRID - 1) + px * PCELLS + cx] != 0)
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Fill m_Surf: one terrain trace per 4 x 4 cell block (its centre). Nothing hit or auto off ->
	//! m_GameMaterial. Returns the time taken (ms).
	protected int SampleSurfaces()
	{
		int t0 = System.GetTickCount();
		string words = m_sHardWords;
		words.ToLower();
		words.Split(",", m_aHardWords, true);

		float s = 4 * m_fStep;
		int hits = 0;
		for (int sz = 0; sz < SURF; sz++)
		{
			for (int sx = 0; sx < SURF; sx++)
			{
				ResourceName mat = m_GameMaterial;
				if (m_bAutoGameMaterial)
				{
					ResourceName under = SurfaceUnder(CoordToParent(Vector((sx + 0.5) * s - m_fHalf, 0, (sz + 0.5) * s - m_fHalf)));
					if (!under.IsEmpty())
					{
						mat = under;
						hits++;
					}
				}

				m_Surf.m_aIdx[sz * SURF + sx] = MatIndex(mat);
			}
		}
		UpdateSoft(0, SURF - 1, 0, SURF - 1);
		if (m_bDebug)
			Print(string.Format("MUD ground: surface traces - auto %1, %2 of %3 found a material, hard words %4, origin %5", m_bAutoGameMaterial, hits, SURF * SURF, m_aHardWords, GetOrigin()));
		return System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	//! Index of mat in m_aSurfMats, added (with its hard flag) if new
	protected int MatIndex(ResourceName mat)
	{
		int idx = m_aSurfMats.Find(mat);
		if (idx >= 0)
			return idx;

		string path = mat;
		path.ToLower();
		bool hard = false;
		foreach (string w : m_aHardWords)
		{
			if (path.Contains(w))
				hard = true;
		}
		m_aMatHard.Insert(hard);
		return m_aSurfMats.Insert(mat);
	}

	//------------------------------------------------------------------------------------------------
	//! Softness of samples [sx0..sx1] x [sz0..sz1]: hard = 0, else the share of soft samples in its
	//! 3 x 3 block, so a rut runs out over ~1 m toward a road instead of ending in a wall.
	protected void UpdateSoft(int sx0, int sx1, int sz0, int sz1)
	{
		int az = Math.ClampInt(sz0, 0, SURF - 1);
		int bz = Math.ClampInt(sz1, 0, SURF - 1);
		int ax = Math.ClampInt(sx0, 0, SURF - 1);
		int bx = Math.ClampInt(sx1, 0, SURF - 1);
		for (int sz = az; sz <= bz; sz++)
		{
			for (int sx = ax; sx <= bx; sx++)
			{
				if (m_aMatHard[m_Surf.m_aIdx[sz * SURF + sx]])
				{
					m_Surf.m_aSoft[sz * SURF + sx] = 0;
					continue;
				}

				float soft = 0;
				float all = 0;
				int nz0 = Math.ClampInt(sz - 1, 0, SURF - 1);
				int nz1 = Math.ClampInt(sz + 1, 0, SURF - 1);
				int nx0 = Math.ClampInt(sx - 1, 0, SURF - 1);
				int nx1 = Math.ClampInt(sx + 1, 0, SURF - 1);
				for (int nz = nz0; nz <= nz1; nz++)
				{
					for (int nx = nx0; nx <= nx1; nx++)
					{
						all++;
						if (!m_aMatHard[m_Surf.m_aIdx[nz * SURF + nx]])
							soft++;
					}
				}
				m_Surf.m_aSoft[sz * SURF + sx] = soft / all;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Softness under grid vertex x, z (the sample holding it)
	protected float VertexSoft(int x, int z)
	{
		// ClampInt, not Math.Min: Min returns a float, and a float used as an index is read as its
		// raw bits (crash 2026-10-08: index 1142685696 = 0x441B0000 = 620.0f)
		int sx = Math.ClampInt(x / 4, 0, SURF - 1);
		int sz = Math.ClampInt(z / 4, 0, SURF - 1);
		return m_Surf.m_aSoft[sz * SURF + sx];
	}

	//------------------------------------------------------------------------------------------------
	//! Hard samples of this patch that an explosion broke (any of its vertices moved > 3 cm) become
	//! m_CraterMaterial. Worked out from the heights alone, so server, clients and late joiners all
	//! agree without extra network data. Ruts never move hard vertices (softness 0), only craters do.
	protected void BreakHardSamples(MUD_GroundPatch p)
	{
		bool any = false;
		int crater = -1;
		for (int sz = p.m_iPz * SPP; sz < p.m_iPz * SPP + SPP; sz++)
		{
			for (int sx = p.m_iPx * SPP; sx < p.m_iPx * SPP + SPP; sx++)
			{
				if (!m_aMatHard[m_Surf.m_aIdx[sz * SURF + sx]])
					continue;

				// the vertices VertexSoft maps to this sample (the last sample also owns row/col 128)
				int x1 = sx * 4 + 3;
				if (sx == SURF - 1)
					x1 = GRID - 1;
				int z1 = sz * 4 + 3;
				if (sz == SURF - 1)
					z1 = GRID - 1;

				bool broken = false;
				for (int z = sz * 4; z <= z1 && !broken; z++)
				{
					for (int x = sx * 4; x <= x1; x++)
					{
						if (Math.AbsFloat(m_Data.m_aH[z * GRID + x]) > 0.03)
						{
							broken = true;
							break;
						}
					}
				}
				if (!broken)
					continue;

				if (crater < 0)
					crater = MatIndex(m_CraterMaterial);
				m_Surf.m_aIdx[sz * SURF + sx] = crater;
				any = true;
			}
		}

		if (!any)
			return;
		UpdateSoft(p.m_iPx * SPP - 1, p.m_iPx * SPP + SPP, p.m_iPz * SPP - 1, p.m_iPz * SPP + SPP);
		PatchMaterials(p);
	}

	//------------------------------------------------------------------------------------------------
	//! Pick the patch's collider surfaces: the MAT_SLOTS most common in its 8 x 8 samples
	protected void PatchMaterials(MUD_GroundPatch p)
	{
		array<int> counts = {};
		for (int m = 0; m < m_aSurfMats.Count(); m++)
			counts.Insert(0);

		for (int sz = 0; sz < SPP; sz++)
		{
			for (int sx = 0; sx < SPP; sx++)
			{
				int mi = m_Surf.m_aIdx[(p.m_iPz * SPP + sz) * SURF + p.m_iPx * SPP + sx];
				counts[mi] = counts[mi] + 1;    // dynamic array element: ++ is refused ("temporary value")
			}
		}

		p.m_aMatIdx.Clear();
		while (p.m_aMatIdx.Count() < MAT_SLOTS)
		{
			int best = -1;
			for (int m2 = 0; m2 < counts.Count(); m2++)
			{
				if (counts[m2] > 0 && (best < 0 || counts[m2] > counts[best]))
					best = m2;
			}
			if (best < 0)
				break;
			p.m_aMatIdx.Insert(best);
			counts[best] = 0;
		}
		p.m_GameMat = m_aSurfMats[p.m_aMatIdx[0]];
	}

	//------------------------------------------------------------------------------------------------
	//! Surface map entry under local position lp (debug)
	protected ResourceName SurfaceAt(vector lp)
	{
		float s = 4 * m_fStep;
		int sx = Math.ClampInt(Math.Floor((lp[0] + m_fHalf) / s), 0, SURF - 1);
		int sz = Math.ClampInt(Math.Floor((lp[2] + m_fHalf) / s), 0, SURF - 1);
		return m_aSurfMats[m_Surf.m_aIdx[sz * SURF + sx]];
	}

	//------------------------------------------------------------------------------------------------
	//! Static body from 1-6 geoms. CreateStaticEx takes a fixed-size array and no shipped script
	//! passes a variable count, so one literal array per count.
	protected Physics MakeStatic(IEntity ent, array<PhysicsGeom> g, array<ResourceName> m, array<int> l)
	{
		switch (g.Count())
		{
			case 1:
			{
				autoptr PhysicsGeomDef d1[] = {PhysicsGeomDef("", g[0], m[0], l[0])};
				return Physics.CreateStaticEx(ent, d1);
			}
			case 2:
			{
				autoptr PhysicsGeomDef d2[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1])};
				return Physics.CreateStaticEx(ent, d2);
			}
			case 3:
			{
				autoptr PhysicsGeomDef d3[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2])};
				return Physics.CreateStaticEx(ent, d3);
			}
			case 4:
			{
				autoptr PhysicsGeomDef d4[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2]), PhysicsGeomDef("", g[3], m[3], l[3])};
				return Physics.CreateStaticEx(ent, d4);
			}
			case 5:
			{
				autoptr PhysicsGeomDef d5[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2]), PhysicsGeomDef("", g[3], m[3], l[3]), PhysicsGeomDef("", g[4], m[4], l[4])};
				return Physics.CreateStaticEx(ent, d5);
			}
			case 6:
			{
				autoptr PhysicsGeomDef d6[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2]), PhysicsGeomDef("", g[3], m[3], l[3]), PhysicsGeomDef("", g[4], m[4], l[4]), PhysicsGeomDef("", g[5], m[5], l[5])};
				return Physics.CreateStaticEx(ent, d6);
			}
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Game material of the terrain surface under a world position (terrain only, like the shipped
	//! SCR_MineAnimationComponent trace). Empty when nothing was hit.
	protected ResourceName SurfaceUnder(vector wp)
	{
		TraceParam param = new TraceParam();
		param.Start = wp + vector.Up * 50;
		param.End = wp - vector.Up * 50;
		param.Flags = TraceFlags.WORLD;
		param.LayerMask = EPhysicsLayerPresets.Projectile;
		GetWorld().TraceMove(param, null);

		GameMaterial mat = param.SurfaceProps;
		if (!mat)
			return ResourceName.Empty;

		return mat.GetResourceName();
	}

	//------------------------------------------------------------------------------------------------
	//! Edit mode: the undeformed play surface (terrain + pad shape) as one 33 x 33 mesh with a
	//! collider, so objects can be dropped on top of it and Snap to Mud lands where play will be.
	protected void BuildPreview()
	{
		m_Buf = new MUD_GroundPatchBuffers();
		BuildPatchIndices();

		// every 4th grid vertex (128 / 32), from the same lid + pad shape the ground builds in play
		bool onTerrain = m_bFollowTerrain && SampleLid();
		float s = m_fSize / PCELLS;
		int every = (GRID - 1) / PCELLS;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				float h = 0;
				if (onTerrain)
					h = RestSurface(vx * every, vz * every);
				m_Buf.m_aVerts[vz * PVERTS + vx] = Vector(vx * s - m_fHalf, h, vz * s - m_fHalf);
			}
		}

		int numVerts[] = {1089};
		int numIndices[] = {6144};
		string materials[] = {m_Material};
		Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!res || !res.GetResource())
		{
			Print(string.Format("MUD ground: preview mesh failed - material '%1' did not load?", m_Material), LogLevel.ERROR);
			return;
		}

		MeshObject mesh = res.GetResource().ToMeshObject();
		mesh.UpdateIndices(0, m_Buf.m_aIndices);
		mesh.UpdateVerts(0, m_Buf.m_aVerts, m_Buf.m_aUVs);
		SetObject(mesh, "");
		m_PreviewRes = res;

		PhysicsGeom geom = PhysicsGeom.CreateTriMesh(m_Buf.m_aVerts, m_Buf.m_aIndices, 1089, 6144);
		if (geom)
		{
			array<PhysicsGeom> geoms = {geom};
			array<ResourceName> mats = {m_GameMaterial};
			array<int> layers = {EPhysicsLayerPresets.Terrain};
			MakeStatic(this, geoms, mats, layers);
		}
		m_Buf = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Same index pattern for every patch (local indexing), same winding as SCR_TerrainMeshEntity.
	//! The collider uses it too.
	protected void BuildPatchIndices()
	{
		int n;
		for (int z = 0; z < PCELLS; z++)
		{
			for (int x = 0; x < PCELLS; x++)
			{
				int bl = z * PVERTS + x;
				int br = bl + 1;
				int tl = bl + PVERTS;
				int tr = tl + 1;
				m_Buf.m_aIndices[n] = bl; n++;
				m_Buf.m_aIndices[n] = tl; n++;
				m_Buf.m_aIndices[n] = tr; n++;
				m_Buf.m_aIndices[n] = tr; n++;
				m_Buf.m_aIndices[n] = br; n++;
				m_Buf.m_aIndices[n] = bl; n++;
			}
		}

		for (int i = 0; i < 6144; i += 3)
		{
			m_Buf.m_aShellIndices[i] = m_Buf.m_aIndices[i];
			m_Buf.m_aShellIndices[i + 1] = m_Buf.m_aIndices[i + 2];
			m_Buf.m_aShellIndices[i + 2] = m_Buf.m_aIndices[i + 1];
		}

		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				m_Buf.m_aUVs[v * 2] = vx / 32.0;
				m_Buf.m_aUVs[v * 2 + 1] = 1 - vz / 32.0;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnPatches()
	{
		vector mat[4];
		GetWorldTransform(mat);
		float patchSize = PCELLS * m_fStep;

		for (int pz = 0; pz < PATCHES; pz++)
		{
			for (int px = 0; px < PATCHES; px++)
			{
				EntitySpawnParams params = new EntitySpawnParams();
				params.TransformMode = ETransformMode.WORLD;
				params.Transform[0] = mat[0];
				params.Transform[1] = mat[1];
				params.Transform[2] = mat[2];
				params.Transform[3] = CoordToParent(Vector(px * patchSize - m_fHalf, 0, pz * patchSize - m_fHalf));

				MUD_GroundPatch p = new MUD_GroundPatch();
				p.m_iPx = px;
				p.m_iPz = pz;
				p.m_bHasCut = m_bCutHoles && PatchHasCut(px, pz);
				PatchMaterials(p);

				p.m_Ent = GetGame().SpawnEntity(GenericEntity, GetWorld(), params);
				m_aPatches.Insert(p);
				if (!p.m_Ent)
				{
					Print("MUD ground: could not spawn patch entity", LogLevel.ERROR);
					continue;
				}

				if (m_bRender)
				{
					FillPatch(px, pz);
					BuildPatchMesh(p);
				}
				FillPatchSmooth(px, pz);
				BuildPatchCollider(p);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Copy one patch's heights into the shared vertex buffer, in patch-local coords
	protected void FillPatch(int px, int pz)
	{
		int gx0 = px * PCELLS;
		int gz0 = pz * PCELLS;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int g = (gz0 + vz) * GRID + gx0 + vx;
				float d = m_Data.m_aH[g];
				float h = m_Base.m_aBase[g] + d;
				int v = vz * PVERTS + vx;

				// groove reveal: the groove layer follows the surface m_fRevealGap below it; where the
				// ground is dug deeper than m_fRevealDepth the surface sinks 3x the gap, under the
				// groove layer, so it shows through. The edge is where the two cross (1/3 into the band).
				if (m_bGroove)
				{
					m_Buf.m_aUnder[v] = Vector(vx * m_fStep, h - m_fRevealGap, vz * m_fStep);
					float t = Math.Clamp((-d - m_fRevealDepth) / Math.Max(m_fRevealBlend, 0.01), 0, 1);
					h -= t * t * (3 - 2 * t) * m_fRevealGap * 3;
				}

				m_Buf.m_aVerts[v] = Vector(vx * m_fStep, h, vz * m_fStep);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Bring m_aSmooth up to date for this patch's own vertices only, from its pending change rect
	protected void SmoothPatch(MUD_GroundPatch p, int px, int pz)
	{
		if (p.m_iSx0 < 0)
			return;

		int r = m_aSmoothW.Count() / 2;
		int gx0 = px * PCELLS;
		int gz0 = pz * PCELLS;
		int ax = Math.Max(p.m_iSx0 - r, gx0);
		int bx = Math.Min(p.m_iSx1 + r, gx0 + PCELLS);
		int az = Math.Max(p.m_iSz0 - r, gz0);
		int bz = Math.Min(p.m_iSz1 + r, gz0 + PCELLS);
		if (ax <= bx && az <= bz)
			UpdateSmooth(ax, bx, az, bz);

		p.m_iSx0 = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Like FillPatch, but from the smoothed deformation m_aSmooth (kept up to date by UpdateSmooth),
	//! so the collider gets grooves the wheels roll through instead of one-vertex notches.
	protected void FillPatchSmooth(int px, int pz)
	{
		int gx0 = px * PCELLS;
		int gz0 = pz * PCELLS;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int g = (gz0 + vz) * GRID + gx0 + vx;
				float h = m_Base.m_aBase[g] + m_Base.m_aSmooth[g];
				m_Buf.m_aVerts[vz * PVERTS + vx] = Vector(vx * m_fStep, h, vz * m_fStep);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Re-blur m_aH into m_aSmooth for vertices [x0..x1] x [z0..z1] (caller adds the r margin).
	//! Binomial kernel (Pascal row 2r: 1 4 6 4 1 at r 2), deformation only - the terrain base is
	//! never blurred. Cost follows the size of the change, not the patch: a whole-patch blur per
	//! collider build cost ~7.5 ms each in script (measured 2026-10-08 08:12), and smoothing on
	//! every wheel press cost ~150 ms/s (08:17) - so it runs once per patch per collider rebuild.
	//! Separable (X pass into m_aBlurRows, then Z pass): 2 * (2r + 1) taps per vertex instead of
	//! (2r + 1)^2, same result - kernel and edge clamp are both per axis. The 2D version was still
	//! ~40 ms/s while driving (2026-10-08 13:46 log: smoothing 387-418 ms per 10 s).
	//! Caller keeps the rect inside one patch (at most 33 x 33).
	protected void UpdateSmooth(int x0, int x1, int z0, int z1)
	{
		int t0 = System.GetTickCount();
		int r = m_aSmoothW.Count() / 2;
		int cols = x1 - x0 + 1;
		int ra = z0 - r;
		int rows = z1 + r - ra + 1;
		if (cols * rows > 1287)
		{
			Print(string.Format("MUD ground: smooth rect %1 x %2 too big", cols, rows), LogLevel.ERROR);
			return;
		}

		float wsum = 0;
		foreach (float wt : m_aSmoothW)
			wsum += wt;
		float inv = 1 / (wsum * wsum);

		// X pass, rows z0 - r .. z1 + r (clamped to the grid like the old 2D loop)
		for (int z = ra; z <= z1 + r; z++)
		{
			int srow = Math.ClampInt(z, 0, GRID - 1) * GRID;
			int orow = (z - ra) * cols;
			for (int x = x0; x <= x1; x++)
			{
				float sum = 0;
				for (int kx = -r; kx <= r; kx++)
					sum += m_Data.m_aH[srow + Math.ClampInt(x + kx, 0, GRID - 1)] * m_aSmoothW[kx + r];
				m_Buf.m_aBlurRows[orow + x - x0] = sum;
			}
		}

		// Z pass
		for (int z2 = z0; z2 <= z1; z2++)
		{
			for (int x2 = x0; x2 <= x1; x2++)
			{
				float sum2 = 0;
				// hard ground keeps its own height: the blur would pull a rut next to it ~0.5 m onto the asphalt
				// same under a building: a rut at the wall must not blur in under the floor
				if (VertexSoft(x2, z2) <= 0 || AtCut(x2, z2))
				{
					m_Base.m_aSmooth[z2 * GRID + x2] = m_Data.m_aH[z2 * GRID + x2];
					continue;
				}
				for (int kz = -r; kz <= r; kz++)
					sum2 += m_Buf.m_aBlurRows[(z2 + kz - ra) * cols + x2 - x0] * m_aSmoothW[kz + r];
				m_Base.m_aSmooth[z2 * GRID + x2] = sum2 * inv;
			}
		}

		m_iSmoothMs += System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	//! Static mesh (flags 0): later UpdateVerts is not drawn, so every change is a fresh Create.
	//! DYNAMIC_VERTEXES would take updates but shades scaly (proven 2026-10-07).
	protected bool BuildPatchMesh(MUD_GroundPatch p)
	{
		int t0 = System.GetTickCount();
		Resource res;
		if (m_bGroove)
		{
			// mesh 0 = surface, mesh 1 = groove layer (see FillPatch)
			int numVerts2[] = {1089, 1089};
			int numIndices2[] = {6144, 6144};
			string materials2[] = {m_Material, m_RutMaterial};
			res = MeshObject.Create(2, numVerts2, numIndices2, materials2, 0);
			if (!res || !res.GetResource())
			{
				// Create fails if any material fails to load: drop the groove layer for good, and
				// rebuild this patch without it next time (its verts were sunk for the groove)
				Print(string.Format("MUD ground: groove material '%1' did not load - groove layer off", m_RutMaterial), LogLevel.ERROR);
				m_bGroove = false;
				p.m_bMeshDirty = true;
				return false;
			}
		}
		else
		{
			int numVerts[] = {1089};
			int numIndices[] = {6144};
			string materials[] = {m_Material};
			res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		}

		if (!res || !res.GetResource())
		{
			Print(string.Format("MUD ground: MeshObject.Create failed - material '%1' did not load?", m_Material), LogLevel.ERROR);
			return false;
		}

		MeshObject mesh = res.GetResource().ToMeshObject();
		// holes: same index count (Create fixed it), a hole cell's 6 indices all point at one vertex,
		// so its two triangles have no area and draw nothing
		if (p.m_bHasCut)
		{
			for (int cz = 0; cz < PCELLS; cz++)
			{
				for (int cx = 0; cx < PCELLS; cx++)
				{
					int src = (cz * PCELLS + cx) * 6;
					bool cut = m_Cut.m_aCell[(p.m_iPz * PCELLS + cz) * (GRID - 1) + p.m_iPx * PCELLS + cx] != 0;
					for (int j = 0; j < 6; j++)
					{
						if (cut)
							m_Buf.m_aMeshIdx[src + j] = m_Buf.m_aIndices[src];
						else
							m_Buf.m_aMeshIdx[src + j] = m_Buf.m_aIndices[src + j];
					}
				}
			}
		}

		// indices first: UpdateVerts rebuilds normals + tangents from the triangle list
		if (p.m_bHasCut)
			mesh.UpdateIndices(0, m_Buf.m_aMeshIdx);
		else
			mesh.UpdateIndices(0, m_Buf.m_aIndices);
		mesh.UpdateVerts(0, m_Buf.m_aVerts, m_Buf.m_aUVs);
		if (m_bGroove)
		{
			if (p.m_bHasCut)
				mesh.UpdateIndices(1, m_Buf.m_aMeshIdx);
			else
				mesh.UpdateIndices(1, m_Buf.m_aIndices);
			mesh.UpdateVerts(1, m_Buf.m_aUnder, m_Buf.m_aUVs);
		}
		p.m_Ent.SetObject(mesh, "");
		p.m_MeshRes = res;
		p.m_bMeshDirty = false;
		m_iMeshBuilds++;
		m_iMeshMs += System.GetTickCount(t0);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Old body goes and the new one comes in the same script call, so the physics step
	//! never sees the patch without ground. One trimesh per surface in the patch, each with its
	//! own game material, so a wheel gets the grip of the terrain paint it is on.
	protected void BuildPatchCollider(MUD_GroundPatch p)
	{
		int t0 = System.GetTickCount();

		// material slot of every cell; surfaces past MAT_SLOTS join slot 0 (the most common)
		int slots = p.m_aMatIdx.Count();
		for (int cz = 0; cz < PCELLS; cz++)
		{
			for (int cx = 0; cx < PCELLS; cx++)
			{
				int mi = m_Surf.m_aIdx[(p.m_iPz * SPP + cz / 4) * SURF + p.m_iPx * SPP + cx / 4];
				int slot = p.m_aMatIdx.Find(mi);
				if (slot < 0)
					slot = 0;
				// hole: in no piece
				if (p.m_bHasCut && m_Cut.m_aCell[(p.m_iPz * PCELLS + cz) * (GRID - 1) + p.m_iPx * PCELLS + cx] != 0)
					slot = -1;
				m_Buf.m_aCellSlot[cz * PCELLS + cx] = slot;
			}
		}

		array<PhysicsGeom> geoms = {};
		array<ResourceName> mats = {};
		array<int> layers = {};

		// Terrain preset (Terrain | Navmesh, 0x200080 in ArmaReforger.gproj): the Terrain row of the
		// interaction matrix covers VehicleCast (wheel casts), VehicleSimple, Character and Projectile
		// Bullet floor: the collider alone is an open sheet, and bullets passed through crater rims
		// (2026-10-08 dedicated test). A lowered, downward-facing copy on FireGeo (0x4000) closes it
		// into a slab for projectiles only.
		// + Foliage (0x40000): vehicle get in/out sweeps a box at the door and ignores bodies on
		// ExcludePhysicsLayer 0x40000 (CompartmentAccess_base.conf); without it our ground reads as
		// "Obstructed" (2026-10-08). Foliage row interacts only with Foliage, so nothing else changes.
		int layer = EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage;

		for (int k = 0; k < slots; k++)
		{
			int n = 0;
			for (int c = 0; c < PCELLS * PCELLS; c++)
			{
				if (m_Buf.m_aCellSlot[c] != k)
					continue;
				int src = c * 6;
				for (int j = 0; j < 6; j++)
				{
					m_Buf.m_aSplitIdx[n] = m_Buf.m_aIndices[src + j];
					n++;
				}
			}
			if (n == 0)
				continue;

			PhysicsGeom geom = PhysicsGeom.CreateTriMesh(m_Buf.m_aVerts, m_Buf.m_aSplitIdx, 1089, n);
			if (!geom)
			{
				Print("MUD ground: CreateTriMesh failed", LogLevel.ERROR);
				continue;
			}
			geoms.Insert(geom);
			mats.Insert(m_aSurfMats[p.m_aMatIdx[k]]);
			layers.Insert(layer);
		}
		if (geoms.IsEmpty())
			return;

		if (m_fBulletSlab > 0)
		{
			for (int i = 0; i < 1089; i++)
			{
				m_Buf.m_aShell[i] = m_Buf.m_aVerts[i] - vector.Up * m_fBulletSlab;
			}
			// shell without the hole cells (m_aSplitIdx is free again here)
			int sn = 0;
			for (int sc = 0; sc < PCELLS * PCELLS; sc++)
			{
				if (m_Buf.m_aCellSlot[sc] < 0)
					continue;
				for (int sj = 0; sj < 6; sj++)
				{
					m_Buf.m_aSplitIdx[sn] = m_Buf.m_aShellIndices[sc * 6 + sj];
					sn++;
				}
			}
			PhysicsGeom shell = PhysicsGeom.CreateTriMesh(m_Buf.m_aShell, m_Buf.m_aSplitIdx, 1089, sn);
			if (shell)
			{
				geoms.Insert(shell);
				mats.Insert(p.m_GameMat);
				layers.Insert(EPhysicsLayerPresets.FireGeo);
			}
		}

		if (p.m_Phys)
		{
			p.m_Phys.Destroy();
			p.m_Phys = null;
		}
		p.m_Phys = MakeStatic(p.m_Ent, geoms, mats, layers);
		if (!p.m_Phys)
			Print("MUD ground: CreateStaticEx failed", LogLevel.ERROR);

		p.m_bColDirty = false;
		m_iColBuilds++;
		m_iColMs += System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnExplosion(IEntity pHitEntity, inout vector outMat[3], IEntity damageSource, notnull Instigator instigator, string colliderName, float speed)
	{
		// clients get craters from the server
		if (!m_Data || IsProxy())
			return;

		// outMat[0] = hit position (as shipped SCR_FireSupportRequestedTaskEntity uses it)
		vector p = CoordToLocal(outMat[0]);
		float reach = m_fHalf + m_fCraterRadius * 1.5;
		if (Math.AbsFloat(p[0]) > reach || Math.AbsFloat(p[2]) > reach)
			return;

		Crater(p);
		if (m_bDebug)
			Print(string.Format("MUD ground: crater at local %1", p));
	}

	//------------------------------------------------------------------------------------------------
	//! Bowl m_fCraterDepth deep at the centre, raised rim out to 1.5x radius
	protected void Crater(vector p)
	{
		float outer = m_fCraterRadius * 1.5;
		int x0, x1, z0, z1;
		if (!GridRange(p, outer, x0, x1, z0, z1))
			return;

		// fit the bowl into the room above the terrain at the centre, so it stays a bowl instead of a
		// 2 m profile cut flat by the floor; the per-vertex floor below still guards the sides
		int cx = Math.ClampInt(Math.Round((p[0] + m_fHalf) / m_fStep), 0, GRID - 1);
		int cz = Math.ClampInt(Math.Round((p[2] + m_fHalf) / m_fStep), 0, GRID - 1);
		float depth = Math.Min(m_fCraterDepth, Math.Min(m_fMaxDepth, -m_Base.m_aFloor[cz * GRID + cx]));

		for (int z = z0; z <= z1; z++)
		{
			for (int x = x0; x <= x1; x++)
			{
				int idx = z * GRID + x;
				if (AtCut(x, z))
					continue;
				float dx = x * m_fStep - m_fHalf - p[0];
				float dz = z * m_fStep - m_fHalf - p[2];
				float d = Math.Sqrt(dx * dx + dz * dz);
				float h = m_Data.m_aH[idx];

				if (d < m_fCraterRadius)
				{
					float k = d / m_fCraterRadius;
					float y = Math.Max(-depth * (1 - k * k), m_Base.m_aFloor[idx]);
					if (y < h)
						m_Data.m_aH[idx] = y;
				}
				else if (d < outer)
				{
					float t = 1 - (d - m_fCraterRadius) / (outer - m_fCraterRadius);
					float rim = m_fCraterRimHeight * t * t;
					if (rim > h)
						m_Data.m_aH[idx] = rim;
				}
			}
		}

		MarkDirty(x0, x1, z0, z1);
	}

	//------------------------------------------------------------------------------------------------
	//! One shovel swing at local p: the hole sinks m_fDigStep at the centre, the soil goes on a low
	//! pile around it. A pile never lands in a dug vertex, so a trench dug swing by swing stays open.
	protected bool Dig(vector p)
	{
		int x0, x1, z0, z1;
		if (!GridRange(p, m_fSpoilRadius, x0, x1, z0, z1))
			return false;

		int cx = Math.ClampInt(Math.Round((p[0] + m_fHalf) / m_fStep), 0, GRID - 1);
		int cz = Math.ClampInt(Math.Round((p[2] + m_fHalf) / m_fStep), 0, GRID - 1);
		if (Math.AbsFloat(p[0]) > m_fHalf || Math.AbsFloat(p[2]) > m_fHalf)
			return false;
		// soldier is not on this ground (on a roof above it, ...): ignore
		int c = cz * GRID + cx;
		if (Math.AbsFloat(p[1] - m_Base.m_aBase[c] - m_Data.m_aH[c]) > 1.5)
			return false;
		// no shovel on asphalt / concrete (a crater there turns it to dirt first)
		if (VertexSoft(cx, cz) <= 0 || AtCut(cx, cz))
			return false;

		float spoilSpan = m_fSpoilRadius - m_fDigRadius;
		for (int z = z0; z <= z1; z++)
		{
			for (int x = x0; x <= x1; x++)
			{
				int idx = z * GRID + x;
				float dx = x * m_fStep - m_fHalf - p[0];
				float dz = z * m_fStep - m_fHalf - p[2];
				float d = Math.Sqrt(dx * dx + dz * dz);
				float h = m_Data.m_aH[idx];

				if (d < m_fDigRadius)
				{
					float k = d / m_fDigRadius;
					float floor = Math.Max(-Math.Min(m_fDigMaxDepth, m_fMaxDepth), m_Base.m_aFloor[idx]);
					// a pile in the way goes first, the way a wheel plows a berm
					float top = Math.Min(h, 0);
					float y = Math.Max(top - m_fDigStep * (1 - k * k * 0.5), floor);
					if (y < h)
						m_Data.m_aH[idx] = y;
				}
				else if (d < m_fSpoilRadius && spoilSpan > 0)
				{
					if (h < -0.001 || h >= m_fSpoilHeight)
						continue;
					float t = 1 - (d - m_fDigRadius) / spoilSpan;
					m_Data.m_aH[idx] = Math.Min(h + m_fDigStep * 0.6 * t, m_fSpoilHeight);
				}
			}
		}

		MarkDirty(x0, x1, z0, z1);
		if (m_bDebug)
			Print(string.Format("MUD ground: dig at local %1, centre now %2 m", p, m_Data.m_aH[c]));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Grid vertex range covering a circle around local p. False if fully off the grid.
	protected bool GridRange(vector p, float r, out int x0, out int x1, out int z0, out int z1)
	{
		x0 = Math.Floor((p[0] - r + m_fHalf) / m_fStep);
		x1 = Math.Ceil((p[0] + r + m_fHalf) / m_fStep);
		z0 = Math.Floor((p[2] - r + m_fHalf) / m_fStep);
		z1 = Math.Ceil((p[2] + r + m_fHalf) / m_fStep);
		if (x1 < 0 || z1 < 0 || x0 > GRID - 1 || z0 > GRID - 1)
			return false;

		x0 = Math.ClampInt(x0, 0, GRID - 1);
		x1 = Math.ClampInt(x1, 0, GRID - 1);
		z0 = Math.ClampInt(z0, 0, GRID - 1);
		z1 = Math.ClampInt(z1, 0, GRID - 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Flag every patch holding a vertex in the range. Edge verts belong to two patches.
	protected void MarkDirty(int x0, int x1, int z0, int z1)
	{
		int cx0 = x0;
		int cx1 = x1;
		int cz0 = z0;
		int cz1 = z1;

		// the blur reaches r verts further, which can cross into the next patch
		int r = m_aSmoothW.Count() / 2;
		x0 = Math.ClampInt(x0 - r, 0, GRID - 1);
		x1 = Math.ClampInt(x1 + r, 0, GRID - 1);
		z0 = Math.ClampInt(z0 - r, 0, GRID - 1);
		z1 = Math.ClampInt(z1 + r, 0, GRID - 1);

		int pxa = 0;
		if (x0 > 0)
			pxa = (x0 - 1) / PCELLS;
		int pza = 0;
		if (z0 > 0)
			pza = (z0 - 1) / PCELLS;
		int pxb = Math.ClampInt(x1 / PCELLS, 0, PATCHES - 1);
		int pzb = Math.ClampInt(z1 / PCELLS, 0, PATCHES - 1);

		// only the server sends; changes that came from the server are not sent on
		// unregistered item (e.g. prefab GUID missing from the resource DB) -> every Rpc logs an error, so skip
		bool net = m_RplComponent != null && !IsProxy() && RplSession.Mode() != RplMode.None && m_RplComponent.Id().IsValid();

		for (int pz = pza; pz <= pzb; pz++)
		{
			for (int px = pxa; px <= pxb; px++)
			{
				MUD_GroundPatch patch = m_aPatches[pz * PATCHES + px];
				patch.m_bMeshDirty = true;
				patch.m_bColDirty = true;
				if (patch.m_iSx0 < 0)
				{
					patch.m_iSx0 = cx0;
					patch.m_iSx1 = cx1;
					patch.m_iSz0 = cz0;
					patch.m_iSz1 = cz1;
				}
				else
				{
					patch.m_iSx0 = Math.Min(patch.m_iSx0, cx0);
					patch.m_iSx1 = Math.Max(patch.m_iSx1, cx1);
					patch.m_iSz0 = Math.Min(patch.m_iSz0, cz0);
					patch.m_iSz1 = Math.Max(patch.m_iSz1, cz1);
				}

				if (!net)
					continue;

				// changed verts inside this patch (the patch range above is widened by the blur)
				int nx0 = Math.Max(cx0, px * PCELLS);
				int nx1 = Math.Min(cx1, px * PCELLS + PCELLS);
				int nz0 = Math.Max(cz0, pz * PCELLS);
				int nz1 = Math.Min(cz1, pz * PCELLS + PCELLS);
				if (nx0 > nx1 || nz0 > nz1)
					continue;

				if (patch.m_iNx0 < 0)
				{
					patch.m_iNx0 = nx0;
					patch.m_iNx1 = nx1;
					patch.m_iNz0 = nz0;
					patch.m_iNz1 = nz1;
				}
				else
				{
					patch.m_iNx0 = Math.Min(patch.m_iNx0, nx0);
					patch.m_iNx1 = Math.Max(patch.m_iNx1, nx1);
					patch.m_iNz0 = Math.Min(patch.m_iNz0, nz0);
					patch.m_iNz1 = Math.Max(patch.m_iNz1, nz1);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True on a client: the server owns the ground shape
	protected bool IsProxy()
	{
		return m_RplComponent != null && m_RplComponent.IsProxy();
	}

	//------------------------------------------------------------------------------------------------
	protected int ToMm(float h)
	{
		return Math.ClampInt(Math.Round(h * 1000), NET_MIN, NET_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! Signed 16-bit half of a packed int: 0 = low, 1 = high
	protected int Unpack(int packed, int half)
	{
		int v = packed & 65535;
		if (half == 1)
			v = (packed >> 16) & 65535;
		if (v > 32767)
			v -= 65536;
		return v;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one message per patch with unsent changes - the rect's heights in mm, two per int
	protected void SendChanges()
	{
		foreach (MUD_GroundPatch p : m_aPatches)
		{
			if (p.m_iNx0 < 0)
				continue;

			// fresh array per message: Rpc may serialize later than this call
			array<int> pack = {};
			int pending = 0;
			bool odd = false;
			for (int z = p.m_iNz0; z <= p.m_iNz1; z++)
			{
				for (int x = p.m_iNx0; x <= p.m_iNx1; x++)
				{
					int mm = ToMm(m_Data.m_aH[z * GRID + x]) & 65535;
					if (!odd)
					{
						pending = mm;
					}
					else
					{
						pack.Insert(pending | (mm << 16));
					}
					odd = !odd;
				}
			}
			if (odd)
				pack.Insert(pending);

			Rpc(RpcDo_Heights, p.m_iNx0, p.m_iNx1, p.m_iNz0, p.m_iNz1, pack);
			m_iNetMsgs++;
			m_iNetInts += pack.Count();
			p.m_iNx0 = -1;
		}
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_Heights(int x0, int x1, int z0, int z1, array<int> packed)
	{
		if (!m_Data || !packed)
			return;
		if (x0 < 0 || z0 < 0 || x1 >= GRID || z1 >= GRID || x0 > x1 || z0 > z1)
			return;
		int n = (x1 - x0 + 1) * (z1 - z0 + 1);
		if (packed.Count() != (n + 1) / 2)
			return;

		int i = 0;
		for (int z = z0; z <= z1; z++)
		{
			for (int x = x0; x <= x1; x++)
			{
				m_Data.m_aH[z * GRID + x] = Unpack(packed[i / 2], i % 2) * 0.001;
				i++;
			}
		}

		MarkDirty(x0, x1, z0, z1);
	}

	//------------------------------------------------------------------------------------------------
	//! Whole field in mm (GRID * GRID) -> heights, every patch rebuilt
	protected void ApplyFieldMm(array<int> mm)
	{
		for (int i = 0; i < GRID * GRID; i++)
			m_Data.m_aH[i] = mm[i] * 0.001;
		MarkDirty(0, GRID - 1, 0, GRID - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Joining client gets the current field: per patch a flag, then its 33 x 33 heights if not flat
	override bool RplSave(ScriptBitWriter writer)
	{
		for (int pz = 0; pz < PATCHES; pz++)
		{
			for (int px = 0; px < PATCHES; px++)
			{
				bool has = false;
				if (m_Data)
				{
					for (int z = pz * PCELLS; z <= pz * PCELLS + PCELLS && !has; z++)
					{
						for (int x = px * PCELLS; x <= px * PCELLS + PCELLS; x++)
						{
							if (ToMm(m_Data.m_aH[z * GRID + x]) != 0)
							{
								has = true;
								break;
							}
						}
					}
				}

				writer.WriteBool(has);
				if (!has)
					continue;

				for (int z2 = pz * PCELLS; z2 <= pz * PCELLS + PCELLS; z2++)
				{
					for (int x2 = px * PCELLS; x2 <= px * PCELLS + PCELLS; x2++)
						writer.WriteIntRange(ToMm(m_Data.m_aH[z2 * GRID + x2]), NET_MIN, NET_MAX);
				}
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		array<int> mm = {};
		mm.Resize(GRID * GRID);
		for (int i = 0; i < GRID * GRID; i++)
			mm[i] = 0;

		bool any = false;
		for (int pz = 0; pz < PATCHES; pz++)
		{
			for (int px = 0; px < PATCHES; px++)
			{
				bool has = false;
				reader.ReadBool(has);
				if (!has)
					continue;

				any = true;
				for (int z = pz * PCELLS; z <= pz * PCELLS + PCELLS; z++)
				{
					for (int x = px * PCELLS; x <= px * PCELLS + PCELLS; x++)
					{
						int v = 0;
						reader.ReadIntRange(v, NET_MIN, NET_MAX);
						mm[z * GRID + x] = v;
					}
				}
			}
		}

		if (!any)
			return true;

		// before EOnInit there are no patches yet: keep it until then
		if (m_Data && m_aPatches.Count() > 0)
			ApplyFieldMm(mm);
		else
			m_aJipMm = mm;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddEntity(IEntity e)
	{
		m_aFound.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_Data)
		{
			if (!m_bSetupDone)
			{
				m_bSetupDone = true;
				Setup();
			}
			return;
		}

		BaseWorld world = GetWorld();
		m_fTime += timeSlice;

		if (m_fFreezeAfter > 0 && m_fTime >= m_fFreezeAfter)
		{
			if (!m_bFrozen)
				Print(string.Format("MUD ground: FROZEN at %1 s - no more deformation", m_fTime), LogLevel.WARNING);
			m_bFrozen = true;
			return;
		}

		// clients get ruts from the server
		m_aFound.Clear();
		bool proxy = IsProxy();
		if (!proxy)
			world.QueryEntitiesBySphere(GetOrigin(), m_fHalf * 1.42 + 6, AddEntity);
		m_fSinceWheelLog += timeSlice;
		bool wheelLog = m_bDebug && m_fSinceWheelLog >= 1;
		foreach (IEntity ent : m_aFound)
		{
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(ent.FindComponent(VehicleWheeledSimulation));
			if (!sim || !sim.IsValid())
				continue;

			// heavier vehicle, deeper rut
			float mass = 0;
			Physics phys = ent.GetPhysics();
			if (phys)
				mass = phys.GetMass();
			float depth = Math.Min(m_fRutBaseDepth + mass * 0.001 * m_fRutDepthPerTon, m_fRutMaxDepth);

			int count = sim.WheelCount();
			for (int w = 0; w < count; w++)
			{
				if (!sim.WheelHasContact(w))
					continue;
				vector lp = CoordToLocal(sim.WheelGetContactPosition(w));
				Press(lp, depth);

				// test: the material the wheel really touches should match our surface map under it
				if (wheelLog && w == 0 && Math.AbsFloat(lp[0]) <= m_fHalf && Math.AbsFloat(lp[2]) <= m_fHalf)
				{
					GameMaterial touch = sim.WheelGetContactMaterial(w);
					ResourceName touchName;
					if (touch)
						touchName = touch.GetResourceName();
					Print(string.Format("MUD wheel: touches %1 | map %2 | mass %3 kg, rut depth %4 m", touchName, SurfaceAt(lp), mass, depth));
					m_fSinceWheelLog = 0;
				}
			}
		}

		// At most one mesh and one collider per frame, the longest-waiting patch first; each patch
		// still waits its interval between builds. Building every dirty patch on one tick froze a
		// frame for ~15 ms every 0.2 s while a truck drove across 3 patches (2026-10-08 13:46 log:
		// per 10 s 280 meshes 184 ms, 160 colliders 240 ms, smoothing 387 ms).
		int meshI = -1;
		int colI = -1;
		for (int i = 0; i < m_aPatches.Count(); i++)
		{
			MUD_GroundPatch p = m_aPatches[i];
			if (!p.m_Ent)
				continue;

			if (m_bRender && p.m_bMeshDirty && m_fTime - p.m_fMeshAt >= m_fMeshInterval)
			{
				if (meshI < 0 || p.m_fMeshAt < m_aPatches[meshI].m_fMeshAt)
					meshI = i;
			}
			if (p.m_bColDirty && m_fTime - p.m_fColAt >= m_fColliderInterval)
			{
				if (colI < 0 || p.m_fColAt < m_aPatches[colI].m_fColAt)
					colI = i;
			}
		}

		if (meshI >= 0)
		{
			MUD_GroundPatch mp = m_aPatches[meshI];
			FillPatch(meshI % PATCHES, meshI / PATCHES);
			BuildPatchMesh(mp);
			mp.m_fMeshAt = m_fTime;
		}
		if (colI >= 0)
		{
			MUD_GroundPatch cp = m_aPatches[colI];
			BreakHardSamples(cp);   // first: smoothing and the collider pieces read the surface map
			SmoothPatch(cp, colI % PATCHES, colI / PATCHES);
			FillPatchSmooth(colI % PATCHES, colI / PATCHES);
			BuildPatchCollider(cp);
			cp.m_fColAt = m_fTime;
		}

		m_fSinceCol += timeSlice;
		if (m_fSinceCol >= m_fColliderInterval)
		{
			m_fSinceCol = 0;
			if (!proxy)
				SendChanges();
		}

		if (m_bDebug)
		{
			m_fSinceStats += timeSlice;
			if (m_fSinceStats >= 10)
			{
				if (m_iMeshBuilds > 0 || m_iColBuilds > 0 || m_iNetMsgs > 0)
					Print(string.Format("MUD ground: last 10 s - %1 patch meshes (%2 ms), %3 patch colliders (%4 ms), smoothing %5 ms, net %6 msgs %7 ints", m_iMeshBuilds, m_iMeshMs, m_iColBuilds, m_iColMs, m_iSmoothMs, m_iNetMsgs, m_iNetInts));
				m_iNetMsgs = 0;
				m_iNetInts = 0;
				m_fSinceStats = 0;
				m_iMeshBuilds = 0;
				m_iColBuilds = 0;
				m_iColMs = 0;
				m_iMeshMs = 0;
				m_iSmoothMs = 0;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! p = wheel contact in this entity's local space. A vertex sinks by m_fRutStep once per pass:
	//! only when no wheel touched it for m_fPassGap seconds. Every touch restarts the timer,
	//! so a parked or slow-rolling wheel does not drill down. depth = this vehicle's rut depth (mass).
	//! Hard ground (softness 0) takes no rut and no berm; next to it both shrink with the softness.
	protected void Press(vector p, float depth)
	{
		int x0, x1, z0, z1;
		if (!GridRange(p, m_fBermRadius, x0, x1, z0, z1))
			return;

		// contact not on this ground (vehicle above it on something else): ignore
		int cx = Math.ClampInt(Math.Round((p[0] + m_fHalf) / m_fStep), 0, GRID - 1);
		int cz = Math.ClampInt(Math.Round((p[2] + m_fHalf) / m_fStep), 0, GRID - 1);
		int c = cz * GRID + cx;
		if (Math.AbsFloat(p[1] - m_Base.m_aBase[c] - m_Data.m_aH[c]) > 0.5)
			return;

		bool changed;
		float bermSpan = m_fBermRadius - m_fRutRadius;
		for (int z = z0; z <= z1; z++)
		{
			for (int x = x0; x <= x1; x++)
			{
				int idx = z * GRID + x;
				float dx = x * m_fStep - m_fHalf - p[0];
				float dz = z * m_fStep - m_fHalf - p[2];
				float d = Math.Sqrt(dx * dx + dz * dz);
				float h = m_Data.m_aH[idx];
				float soft = VertexSoft(x, z);
				if (soft <= 0 || AtCut(x, z))
					continue;

				if (d < m_fRutRadius)
				{
					bool newPass = m_fTime - m_Data.m_aRutTouch[idx] > m_fPassGap;
					m_Data.m_aRutTouch[idx] = m_fTime;
					float cap = depth * soft;
					if (!newPass || h <= -cap)
						continue;

					float k = d / m_fRutRadius;
					float fall = 1 - k * k * 0.5;
					// raised loose soil (crater rim, berm) gets plowed: most of it goes in one pass
					float top = h;
					if (h > 0)
						top = h - h * m_fRimCut * fall;
					float y = Math.Max(top - m_fRutStep * fall, Math.Max(-cap, m_Base.m_aFloor[idx]));
					if (y < h)
					{
						m_Data.m_aH[idx] = y;
						changed = true;
					}
				}
				else if (d < m_fBermRadius)
				{
					bool newBerm = m_fTime - m_Data.m_aBermTouch[idx] > m_fPassGap;
					m_Data.m_aBermTouch[idx] = m_fTime;
					// ruts win over berms: never push up a vertex that is already below flat
					float bermTop = m_fBermHeight * soft;
					if (!newBerm || h < -0.001 || h >= bermTop)
						continue;

					float t = 1 - (d - m_fRutRadius) / bermSpan;
					float y = Math.Min(h + m_fRutStep * 0.5 * t, bermTop);
					if (y > h)
					{
						m_Data.m_aH[idx] = y;
						changed = true;
					}
				}
			}
		}

		if (changed)
			MarkDirty(x0, x1, z0, z1);
	}
}
