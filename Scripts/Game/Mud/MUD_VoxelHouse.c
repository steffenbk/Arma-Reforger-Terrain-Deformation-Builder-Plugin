//! TEMP probe 2026-10-13: VOXEL HOUSE (SP only, user side test "what if we could voxel a house?").
//! On foot, J while looking at a building (within PICK_RANGE): its collider is sampled into a voxel grid in the
//! building's own frame (CELL 0.25 m, a sample is solid when a sphere of r SAMPLE_R there touches the building's
//! Projectile-layer geometry, traced with Include = the building only), a few sub-blocks per frame. Then surface nets
//! (as MUD_VoxelProbe) build the mesh + a static trimesh collider per 8^3 sub-block, hidden; once all are built the
//! real building shell is hidden and its collision turned off (layer 0, mud-gotchas 2026-10-09) in the same frame the
//! voxel house shows. Its children (doors, windows, furniture) stay. J again while looking at the voxel house blasts a
//! sphere hole (CLICK_R); real explosions near it carve one too (MUD_WorldGroundEntity explosion hook). A blast
//! rebuilds its sub-blocks in that frame and removes the small children its sphere reaches (no floating pictures).
//! One house per play session. Look: MUD_VoxelPlaster.emat (vanilla plaster), UVs 0..1 per WALL_TILE m (script-mesh
//! UVs are clamped to 0..1). Logs 'MUD voxel house:'.
//! USE_PANELS (user 2026-10-13 "try 3"): flat walls / floors come out of the grid as wall panels (MUD_WallPanels.c,
//! crisp slabs with holes cut through); only what is left (roof slopes, stairs, odd bits) stays voxels.
//! Remove with: the MUD_VoxelHouse lines in MUD_WorldGround.c (Tick, Clear, explosion hook) and the MUD_VoxelHouse
//! action in Configs/System/chimeraInputCommon.conf.
class MUD_VoxelHouseSub
{
	IEntity m_Ent;
	ref Resource m_Res;
	ref array<PhysicsGeom> m_aGeoms = {};
	int m_i0;
	int m_j0;
	int m_k0;
	bool m_bQueued;
}

class MUD_VoxelHouse
{
	static const bool ENABLED = true;     // on for release (user 2026-10-15: tested, works)
	static const bool USE_PANELS = true;    // false = all voxels (option 4)
	static const string CONTEXT = "MUD_VoxelTunnelContext";
	static const string ACTION = "MUD_VoxelHouse";
	static const float CELL = 0.25;
	static const float SAMPLE_R = 0.14;     // > CELL / 2: any wall plane is within reach of one sample row
	static const int SUB = 8;
	static const int PAD = 2;               // air cells round the building bounds (closed mesh)
	static const int MAX_N = 192;           // cells per axis (48 m)
	static const float MIN_SIZE = 2;        // smallest building side (m)
	static const float PICK_RANGE = 60;
	static const float CLICK_R = 1.5;       // J on the voxel house: hole radius
	static const int VOX_MS = 8;            // voxelizing per frame
	static const int BUILD_MS = 6;          // first build per frame
	static const float WALL_TILE = 4;       // UVs 0..1 per this many metres (tiling in the emat)
	static const int MAX_V = 8000;          // as MUD_VoxelVerts
	static const int MAX_I = 24000;         // as MUD_VoxelIdx
	static const ResourceName MAT = "{4D5548504C410001}Assets/Mud/Data/MUD_VoxelPlaster.emat";   // vanilla plaster textures, tiling 2 per WALL_TILE (user: plaster look, 2026-10-13)
	static const float CHILD_MAX = 4;       // a child (door, furniture, lamp) at most this big goes with a hole that reaches it
	static const ResourceName GAMEMAT = "{C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat";

	static const int S_NONE = 0;
	static const int S_VOX = 1;
	static const int S_BUILD = 2;
	static const int S_LIVE = 3;

	protected static ref MUD_VoxelHouse s_Inst;

	protected IEntity m_House;
	protected string m_sName;
	protected ref array<IEntity> m_aOff = {};   // the building + children, hidden / collision off once live
	protected vector m_aMat[4];                 // building world transform
	protected vector m_vOrg;                    // grid corner, building local
	protected int m_iNX;                        // cells per axis (multiples of SUB)
	protected int m_iNY;
	protected int m_iNZ;
	protected int m_iSX;                        // samples per axis (cells + 1)
	protected int m_iSY;
	protected int m_iSZ;
	protected ref array<float> m_aD = {};       // > 0 solid; id = (k * m_iSY + j) * m_iSX + i
	protected ref array<ref MUD_VoxelHouseSub> m_aSubs = {};
	protected int m_iState;
	protected int m_iNext;                      // next sub-block to voxelize / build
	protected int m_iT0;
	protected int m_iTraces;
	protected int m_iSolid;
	protected int m_iCoarseHit;
	protected int m_iVoxMs;
	protected int m_iVoxFrames;
	protected int m_iBuildMs;
	protected int m_iBuildFrames;
	protected int m_iVerts;
	protected int m_iTris;
	protected int m_iBlasts;
	protected ref TraceSphere m_Trace;
	protected ref MUD_HousePanels m_Panels;

	// mesh build (as MUD_VoxelProbe)
	protected ref MUD_VoxelVerts m_Vb = new MUD_VoxelVerts();
	protected ref MUD_VoxelIdx m_Ib = new MUD_VoxelIdx();
	protected ref MUD_VoxelCells m_Cb = new MUD_VoxelCells();
	protected int m_aOffS[8];                   // cube corner q (bit 1 +x, 2 +y, 4 +z) -> sample id offset
	protected vector m_aOffV[8];
	protected int m_aEa[12];
	protected int m_aEb[12];
	protected MUD_VoxelHouseSub m_Sub;
	protected vector m_vSubOff;                 // sub-block corner, grid local
	protected int m_iStamp;
	protected int m_iV;
	protected int m_iI;
	protected bool m_bOverflow;

	//------------------------------------------------------------------------------------------------
	//! MUD_WorldGroundEntity.EOnFrame
	static void Tick()
	{
		if (!ENABLED || System.IsConsoleApp() || RplSession.Mode() != RplMode.None)
			return;     // SP test only
		Input();
		if (s_Inst)
			s_Inst.Step();
	}

	//------------------------------------------------------------------------------------------------
	//! Play stop (~MUD_WorldGroundEntity)
	static void Clear()
	{
		if (s_Inst)
			s_Inst.Free();
		s_Inst = null;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_WorldGroundEntity explosion hook: a blast of kg TNT at wp carves the live voxel house when it reaches it
	static void Blast(vector wp, float kg)
	{
		if (!ENABLED || !s_Inst || s_Inst.m_iState != S_LIVE || RplSession.Mode() != RplMode.None)
			return;
		float r = Math.Clamp(0.6 + 0.9 * Math.Pow(Math.Max(kg, 0.01), 1.0 / 3.0), 0.8, 4);
		s_Inst.Carve(wp, r, string.Format("explosion %1 kg", kg));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Input()
	{
		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!player || player.IsInVehicle())
			return;
		InputManager input = GetGame().GetInputManager();
		CameraManager cameras = GetGame().GetCameraManager();
		if (!input || !cameras || !cameras.CurrentCamera())
			return;
		input.ActivateContext(CONTEXT);
		if (!input.GetActionTriggered(ACTION))
			return;

		vector cam[4];
		cameras.CurrentCamera().GetTransform(cam);
		TraceParam tr = new TraceParam();
		tr.Start = cam[3];
		tr.End = cam[3] + cam[2] * PICK_RANGE;
		tr.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		tr.LayerMask = EPhysicsLayerPresets.Projectile;
		tr.Exclude = player;
		float f = GetGame().GetWorld().TraceMove(tr, null);
		if (f >= 1 || !tr.TraceEnt)
		{
			Print("MUD voxel house: J - nothing in front of you (or only terrain)", LogLevel.NORMAL);
			return;
		}
		vector hit = tr.Start + (tr.End - tr.Start) * f;
		if (s_Inst && s_Inst.Owns(tr.TraceEnt))
		{
			s_Inst.Carve(hit, CLICK_R, "J");
			return;
		}
		if (s_Inst)
		{
			Print(string.Format("MUD voxel house: J on %1 - one house per play session (%2 is taken)", tr.TraceEnt.GetRootParent(), s_Inst.m_sName), LogLevel.NORMAL);
			return;
		}
		s_Inst = new MUD_VoxelHouse();
		if (!s_Inst.Start(tr.TraceEnt.GetRootParent()))
			s_Inst = null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Owns(IEntity e)
	{
		if (m_Panels && m_Panels.Owns(e))
			return true;
		foreach (MUD_VoxelHouseSub s : m_aSubs)
		{
			if (s.m_Ent && s.m_Ent == e)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Checks the building, sets up the grid. False = not a building we can do (logged).
	protected bool Start(IEntity house)
	{
		m_sName = string.Format("%1", house);
		Physics ph = house.GetPhysics();
		if (!ph || ph.IsDynamic() || Vehicle.Cast(house) || ChimeraCharacter.Cast(house) || GenericTerrainEntity.Cast(house))
		{
			Print(string.Format("MUD voxel house: %1 is not a static building (physics %2) - not done", m_sName, ph), LogLevel.NORMAL);
			return false;
		}
		// a model from the game (.xob), not one of our script meshes (ground patches, skin, voxels)
		string model;
		if (house.GetVObject())
			model = house.GetVObject().GetResourceName();
		if (!model.Contains(".xob"))
		{
			Print(string.Format("MUD voxel house: %1 has no .xob model ('%2') - not done (our ground / script mesh?)", m_sName, model), LogLevel.NORMAL);
			return false;
		}
		m_sName = m_sName + " " + FilePath.StripPath(model);
		vector mins;
		vector maxs;
		house.GetBounds(mins, maxs);
		vector size = maxs - mins;
		if (size[0] < MIN_SIZE || size[2] < MIN_SIZE || size[1] < MIN_SIZE)
		{
			Print(string.Format("MUD voxel house: %1 too small (%2 m) - not done", m_sName, size), LogLevel.NORMAL);
			return false;
		}
		m_iNX = RoundSub(Math.Ceil(size[0] / CELL) + 2 * PAD);
		m_iNY = RoundSub(Math.Ceil(size[1] / CELL) + 2 * PAD);
		m_iNZ = RoundSub(Math.Ceil(size[2] / CELL) + 2 * PAD);
		if (m_iNX > MAX_N || m_iNY > MAX_N || m_iNZ > MAX_N)
		{
			Print(string.Format("MUD voxel house: %1 too big (%2 m, max %3 m per side) - not done", m_sName, size, MAX_N * CELL - 2 * PAD * CELL), LogLevel.NORMAL);
			return false;
		}
		m_House = house;
		house.GetWorldTransform(m_aMat);
		// centred on the bounds
		vector mid = (mins + maxs) * 0.5;
		m_vOrg = mid - Vector(m_iNX * CELL * 0.5, m_iNY * CELL * 0.5, m_iNZ * CELL * 0.5);
		m_iSX = m_iNX + 1;
		m_iSY = m_iNY + 1;
		m_iSZ = m_iNZ + 1;
		m_aD.Resize(m_iSX * m_iSY * m_iSZ);
		for (int n = 0; n < m_aD.Count(); n++)
			m_aD[n] = -1;
		for (int bk = 0; bk < m_iNZ / SUB; bk++)
		{
			for (int bj = 0; bj < m_iNY / SUB; bj++)
			{
				for (int bi = 0; bi < m_iNX / SUB; bi++)
				{
					MUD_VoxelHouseSub sub = new MUD_VoxelHouseSub();
					sub.m_i0 = bi * SUB;
					sub.m_j0 = bj * SUB;
					sub.m_k0 = bk * SUB;
					m_aSubs.Insert(sub);
				}
			}
		}
		m_Trace = new TraceSphere();
		m_Trace.Radius = SAMPLE_R;
		m_Trace.Flags = TraceFlags.ENTS;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_Trace.Include = house;

		int ea[] = {0, 2, 4, 6, 0, 1, 4, 5, 0, 1, 2, 3};
		int eb[] = {1, 3, 5, 7, 2, 3, 6, 7, 4, 5, 6, 7};
		for (int e = 0; e < 12; e++)
		{
			m_aEa[e] = ea[e];
			m_aEb[e] = eb[e];
		}
		int sxy = m_iSX * m_iSY;
		for (int q = 0; q < 8; q++)
		{
			int bx = q & 1;
			int by = (q >> 1) & 1;
			int bz = (q >> 2) & 1;
			m_aOffV[q] = Vector(bx * CELL, by * CELL, bz * CELL);
			m_aOffS[q] = bx + by * m_iSX + bz * sxy;
		}

		m_aOff.Insert(house);
		Collect(house.GetChildren());
		m_iState = S_VOX;
		m_iT0 = System.GetTickCount();
		Print(string.Format("MUD voxel house: %1 at %2 - bounds %3 m, grid %4 x %5 x %6 cells of %7 m (%8 samples, %9 sub-blocks), voxelizing",
			m_sName, house.GetOrigin(), size, m_iNX, m_iNY, m_iNZ, CELL, m_aD.Count(), m_aSubs.Count()) + string.Format(", %1 child entities", m_aOff.Count() - 1), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static int RoundSub(float n)
	{
		int c = n;
		return ((c + SUB - 1) / SUB) * SUB;
	}

	//------------------------------------------------------------------------------------------------
	protected void Collect(IEntity child)
	{
		while (child)
		{
			m_aOff.Insert(child);
			Collect(child.GetChildren());
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected vector ToWorld(vector local)
	{
		return m_aMat[3] + m_aMat[0] * local[0] + m_aMat[1] * local[1] + m_aMat[2] * local[2];
	}

	//------------------------------------------------------------------------------------------------
	protected bool Touches(vector local, float r)
	{
		m_Trace.Start = ToWorld(local);
		m_Trace.End = m_Trace.Start;
		m_Trace.Radius = r;
		m_iTraces++;
		return GetGame().GetWorld().TracePosition(m_Trace, null) < 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void Step()
	{
		int t0 = System.GetTickCount();
		if (m_iState == S_VOX)
		{
			while (m_iNext < m_aSubs.Count() && System.GetTickCount(t0) < VOX_MS)
			{
				VoxSub(m_aSubs[m_iNext]);
				m_iNext++;
			}
			m_iVoxMs += System.GetTickCount(t0);
			m_iVoxFrames++;
			if (m_iNext >= m_aSubs.Count())
			{
				Print(string.Format("MUD voxel house: voxelized - %1 solid samples, %2 of %3 sub-blocks touch the building, %4 traces, %5 ms over %6 frames",
					m_iSolid, m_iCoarseHit, m_aSubs.Count(), m_iTraces, m_iVoxMs, m_iVoxFrames), LogLevel.NORMAL);
				if (m_iSolid == 0)
				{
					Print("MUD voxel house: NO solid sample - the sphere trace never touched the building (layer / Include?) - stopped", LogLevel.WARNING);
					m_iState = S_NONE;
					return;
				}
				if (USE_PANELS)
					MakePanels();
				m_iNext = 0;
				m_iState = S_BUILD;
			}
			return;
		}
		if (m_iState == S_BUILD)
		{
			while (m_iNext < m_aSubs.Count() && System.GetTickCount(t0) < BUILD_MS)
			{
				BuildSub(m_aSubs[m_iNext], true);
				m_iNext++;
			}
			m_iBuildMs += System.GetTickCount(t0);
			m_iBuildFrames++;
			if (m_iNext >= m_aSubs.Count())
			{
				if (m_Panels)
				{
					int pms = m_Panels.BuildAll(true);
					Print(string.Format("MUD voxel house: panels built - %1 chunks, %2 ms", m_Panels.m_iChunks, pms), LogLevel.NORMAL);
				}
				GoLive();
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Samples owned by this sub-block (its 8^3 base samples, plus the far face on the grid's last sub-blocks).
	//! One coarse sphere first: no touch = all air.
	protected void VoxSub(MUD_VoxelHouseSub sub)
	{
		int i1 = sub.m_i0 + SUB;
		int j1 = sub.m_j0 + SUB;
		int k1 = sub.m_k0 + SUB;
		if (i1 < m_iNX)
			i1--;
		if (j1 < m_iNY)
			j1--;
		if (k1 < m_iNZ)
			k1--;
		vector lo = m_vOrg + Vector(sub.m_i0 * CELL, sub.m_j0 * CELL, sub.m_k0 * CELL);
		vector hi = m_vOrg + Vector(i1 * CELL, j1 * CELL, k1 * CELL);
		vector c = (lo + hi) * 0.5;
		if (!Touches(c, vector.Distance(lo, hi) * 0.5 + SAMPLE_R + 0.05))
			return;
		m_iCoarseHit++;
		for (int k = sub.m_k0; k <= k1; k++)
		{
			for (int j = sub.m_j0; j <= j1; j++)
			{
				for (int i = sub.m_i0; i <= i1; i++)
				{
					// grid border stays air: the mesh is closed
					if (i == 0 || j == 0 || k == 0 || i == m_iNX || j == m_iNY || k == m_iNZ)
						continue;
					if (Touches(m_vOrg + Vector(i * CELL, j * CELL, k * CELL), SAMPLE_R))
					{
						m_aD[(k * m_iSY + j) * m_iSX + i] = 1;
						m_iSolid++;
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Flat slabs out of the grid into wall panels (rays at the real collider: before GoLive turns it off)
	protected void MakePanels()
	{
		int t0 = System.GetTickCount();
		m_Panels = new MUD_HousePanels();
		m_Panels.Init(m_House, m_aMat[0], m_aMat[1], m_aMat[2], m_aMat[3], MAT, GAMEMAT);
		int n = m_Panels.Extract(m_aD, m_iSX, m_iSY, m_iSZ, m_vOrg, CELL);
		int ms = System.GetTickCount(t0);
		int axis[3] = {0, 0, 0};
		foreach (MUD_WallPanel p : m_Panels.m_aPanels)
			axis[p.m_iT] = axis[p.m_iT] + 1;
		Print(string.Format("MUD voxel house: panels - %1 boxes, %2 panels (thin x %3, y %4, z %5), %6 faces from rays, %7 of %8 solid samples moved to panels, %9 left as voxels",
			m_Panels.m_iBoxes, n, axis[0], axis[1], axis[2], m_Panels.m_iRayed, m_Panels.m_iMoved, m_iSolid, m_iSolid - m_Panels.m_iMoved) + string.Format(", %1 ms", ms), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Every sub-block built: the voxel house in, the real one out, one frame
	protected void GoLive()
	{
		foreach (MUD_VoxelHouseSub s : m_aSubs)
		{
			if (s.m_Ent)
				s.m_Ent.SetFlags(EntityFlags.VISIBLE, false);
		}
		if (m_Panels)
			m_Panels.Show();
		// only the building shell: doors, windows, furniture (children, not in the grid) stay (user 2026-10-13: doors
		// were lost when every child was hidden)
		int geoms = Remove(m_House);
		m_iState = S_LIVE;
		int built;
		foreach (MUD_VoxelHouseSub s2 : m_aSubs)
		{
			if (s2.m_Ent)
				built++;
		}
		Print(string.Format("MUD voxel house: LIVE %1 ms after J - %2 sub-blocks with a mesh, %3 verts, %4 tris, build %5 ms over %6 frames | real shell hidden, %7 geoms collision off, %8 children kept. J on it = hole r %9 m",
			System.GetTickCount(m_iT0), built, m_iVerts, m_iTris, m_iBuildMs, m_iBuildFrames, geoms, m_aOff.Count() - 1, CLICK_R), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Hidden + collision off (layer 0, mud-gotchas 2026-10-09). Returns the geoms switched off.
	protected int Remove(IEntity e)
	{
		e.ClearFlags(EntityFlags.VISIBLE, false);
		Physics ph = e.GetPhysics();
		if (!ph)
			return 0;
		ph.SetInteractionLayer(0);
		int n = ph.GetNumGeoms();
		for (int g = 0; g < n; g++)
			ph.SetGeomInteractionLayer(g, 0);
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! A hole at wp, r: kept children (m_aOff past the shell) whose world box it reaches go too, with their own
	//! children (m_aOff lists them after their parent). Only small ones (CHILD_MAX): a big child is building structure.
	protected int RemoveChildren(vector wp, float r)
	{
		int gone;
		for (int n = 1; n < m_aOff.Count(); n++)
		{
			IEntity e = m_aOff[n];
			if (!e || (e.GetFlags() & EntityFlags.VISIBLE) == 0)
				continue;
			vector lo;
			vector hi;
			e.GetWorldBounds(lo, hi);
			vector size = hi - lo;
			if (size[0] > CHILD_MAX || size[1] > CHILD_MAX || size[2] > CHILD_MAX)
				continue;
			// nearest point of the box to the blast
			vector p = Vector(Math.Clamp(wp[0], lo[0], hi[0]), Math.Clamp(wp[1], lo[1], hi[1]), Math.Clamp(wp[2], lo[2], hi[2]));
			if (vector.Distance(p, wp) > r)
				continue;
			Remove(e);
			gone++;
			// its own children
			IEntity c = e.GetChildren();
			while (c)
			{
				RemoveTree(c);
				c = c.GetSibling();
			}
		}
		return gone;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveTree(IEntity e)
	{
		Remove(e);
		IEntity c = e.GetChildren();
		while (c)
		{
			RemoveTree(c);
			c = c.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sphere hole of radius r at world wp; the touched sub-blocks rebuild this frame
	protected void Carve(vector wp, float r, string why)
	{
		if (m_iState != S_LIVE)
			return;
		int t0 = System.GetTickCount();
		// grid local: building local (world -> local through the transform's axes) minus the corner
		vector d = wp - m_aMat[3];
		vector loc = Vector(vector.Dot(d, m_aMat[0]) / m_aMat[0].LengthSq(), vector.Dot(d, m_aMat[1]) / m_aMat[1].LengthSq(), vector.Dot(d, m_aMat[2]) / m_aMat[2].LengthSq());
		vector c = loc - m_vOrg;
		int holes;
		int chunks;
		if (m_Panels)
			chunks = m_Panels.Carve(loc, r, holes);
		int i0 = Math.Max(1, Math.Floor((c[0] - r) / CELL));
		int i1 = Math.Min(m_iNX - 1, Math.Ceil((c[0] + r) / CELL));
		int j0 = Math.Max(1, Math.Floor((c[1] - r) / CELL));
		int j1 = Math.Min(m_iNY - 1, Math.Ceil((c[1] + r) / CELL));
		int k0 = Math.Max(1, Math.Floor((c[2] - r) / CELL));
		int k1 = Math.Min(m_iNZ - 1, Math.Ceil((c[2] + r) / CELL));
		int removed;
		for (int k = k0; k <= k1; k++)
		{
			for (int j = j0; j <= j1; j++)
			{
				for (int i = i0; i <= i1; i++)
				{
					int id = (k * m_iSY + j) * m_iSX + i;
					float dist = vector.Distance(c, Vector(i * CELL, j * CELL, k * CELL));
					float v = (dist - r) / CELL;
					if (v >= m_aD[id])
						continue;
					if (m_aD[id] > 0 && v <= 0)
						removed++;
					if (v > -0.005 && v < 0.005)
						v = -0.005;
					m_aD[id] = v;
				}
			}
		}
		if (removed == 0 && holes == 0)
		{
			Print(string.Format("MUD voxel house: %1 at %2 r %3 - nothing solid in reach", why, wp, r), LogLevel.NORMAL);
			return;
		}
		// sub-blocks whose cells touch the changed samples (sample i is a corner of cells i - 1 and i)
		int n;
		foreach (MUD_VoxelHouseSub s : m_aSubs)
		{
			if (removed == 0)
				break;
			if (s.m_i0 > i1 || s.m_i0 + SUB < i0 || s.m_j0 > j1 || s.m_j0 + SUB < j0 || s.m_k0 > k1 || s.m_k0 + SUB < k0)
				continue;
			BuildSub(s, false);
			n++;
		}
		int kids = RemoveChildren(wp, r);
		m_iBlasts++;
		Print(string.Format("MUD voxel house: hole %1 (%2) at %3 r %4 m - %5 panel holes (%6 chunks rebuilt), %7 voxel samples cleared (%8 sub-blocks rebuilt), %9 children gone with it",
			m_iBlasts, why, wp, r, holes, chunks, removed, n, kids) + string.Format(", %1 ms", System.GetTickCount(t0)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Surface nets over the sub-block (as MUD_VoxelProbe.BuildSub): one quad per sign-changing sample edge whose base
	//! sample is in it. Mesh + collider replace the old ones (hidden until GoLive on the first build).
	protected void BuildSub(MUD_VoxelHouseSub sub, bool first)
	{
		m_Sub = sub;
		m_iStamp++;
		m_iV = 0;
		m_iI = 0;
		m_bOverflow = false;
		m_vSubOff = Vector(sub.m_i0 * CELL, sub.m_j0 * CELL, sub.m_k0 * CELL);
		int sx = m_iSX;
		int sxy = m_iSX * m_iSY;
		for (int k = sub.m_k0; k < sub.m_k0 + SUB; k++)
		{
			for (int j = sub.m_j0; j < sub.m_j0 + SUB; j++)
			{
				for (int i = sub.m_i0; i < sub.m_i0 + SUB; i++)
				{
					int p = (k * m_iSY + j) * sx + i;
					bool solid = m_aD[p] > 0;
					int lc = (k - sub.m_k0 + 1) * 81 + (j - sub.m_j0 + 1) * 9 + (i - sub.m_i0 + 1);
					if (j >= 1 && k >= 1 && solid != (m_aD[p + 1] > 0))
						Quad(lc - 81 - 9, lc - 81, lc, lc - 9, Vector(1, 0, 0), solid);
					if (i >= 1 && k >= 1 && solid != (m_aD[p + sx] > 0))
						Quad(lc - 81 - 1, lc - 81, lc, lc - 1, Vector(0, 1, 0), solid);
					if (i >= 1 && j >= 1 && solid != (m_aD[p + sxy] > 0))
						Quad(lc - 9 - 1, lc - 9, lc, lc - 1, Vector(0, 0, 1), solid);
				}
			}
		}
		if (m_bOverflow)
		{
			Print(string.Format("MUD voxel house: sub-block %1 overflow (%2 verts) - not built", m_vSubOff, m_iV), LogLevel.ERROR);
			return;
		}
		DropSub(sub);
		if (m_iI == 0)
			return;
		m_iVerts += m_iV;
		m_iTris += m_iI / 3;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = m_aMat[0];
		params.Transform[1] = m_aMat[1];
		params.Transform[2] = m_aMat[2];
		params.Transform[3] = ToWorld(m_vOrg + m_vSubOff);
		IEntity ent = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
		if (!ent)
			return;
		if (first)
			ent.ClearFlags(EntityFlags.VISIBLE, false);
		int nv[] = {m_iV};
		int ni[] = {m_iI};
		string mats[] = {MAT};
		Resource res = MeshObject.Create(1, nv, ni, mats, 0);
		if (!res || !res.GetResource())
		{
			Print("MUD voxel house: MeshObject.Create failed", LogLevel.ERROR);
			delete ent;
			return;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		mesh.UpdateIndices(0, m_Ib.m_aI);   // indices first: UpdateVerts rebuilds normals from them
		mesh.UpdateVerts(0, m_Vb.m_aV, m_Vb.m_aUV);
		ent.SetObject(mesh, "");
		ent.Update();
		sub.m_Ent = ent;
		sub.m_Res = res;

		PhysicsGeom g = PhysicsGeom.CreateTriMesh(m_Vb.m_aV, m_Ib.m_aI, m_iV, m_iI);
		if (!g)
		{
			Print("MUD voxel house: collider geom failed", LogLevel.ERROR);
			return;
		}
		int layer = EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage;   // as the voxel ground (walkable, proven)
		autoptr PhysicsGeomDef defs[] = {PhysicsGeomDef("", g, GAMEMAT, layer)};
		Physics phys = Physics.CreateStaticEx(ent, defs);
		sub.m_aGeoms.Insert(g);
		if (!phys)
			Print("MUD voxel house: collider failed", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Old body, geoms, entity of a sub-block (in that order: mud-gotchas heap corruption 12:56). The body only
	//! through the live entity: an entity already gone (play stop) leaves body + geoms alone - destroying a kept
	//! Physics pointer there crashed (access violation in DropSub, play stop 2026-10-13 19:19).
	protected void DropSub(MUD_VoxelHouseSub sub)
	{
		if (sub.m_Ent)
		{
			Physics ph = sub.m_Ent.GetPhysics();
			if (ph)
				ph.Destroy();
			foreach (PhysicsGeom g : sub.m_aGeoms)
			{
				if (g)
					g.Destroy();
			}
			delete sub.m_Ent;
		}
		sub.m_aGeoms.Clear();
		sub.m_Ent = null;
		sub.m_Res = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Free()
	{
		foreach (MUD_VoxelHouseSub s : m_aSubs)
			DropSub(s);
		m_aSubs.Clear();
		if (m_Panels)
			m_Panels.Free();
		m_Panels = null;
	}

	//------------------------------------------------------------------------------------------------
	//! As MUD_VoxelProbe.Quad: faces the air, one UV projection from its normal (building axes), one tile from its middle
	protected void Quad(int c0, int c1, int c2, int c3, vector axis, bool solid)
	{
		if (m_bOverflow)
			return;
		vector p0 = CellPos(c0);
		vector p1 = CellPos(c1);
		vector p2 = CellPos(c2);
		vector p3 = CellPos(c3);
		vector air = axis;
		if (!solid)
			air = axis * -1;
		vector n = Cross(p2 - p0, p3 - p1);
		if (vector.Dot(n, air) < 0)
		{
			int ct = c1;
			c1 = c3;
			c3 = ct;
			vector pt = p1;
			p1 = p3;
			p3 = pt;
		}
		float ax = Math.AbsFloat(n[0]);
		float ay = Math.AbsFloat(n[1]);
		float az = Math.AbsFloat(n[2]);
		int kind = 2;
		if (ay >= ax && ay >= az)
			kind = 0;
		else if (ax >= az)
			kind = 1;
		vector mid = (p0 + p1 + p2 + p3) * 0.25;
		float ma;
		float mb;
		KindAB(kind, mid, ma, mb);
		int ta = Math.Floor(ma / WALL_TILE);
		int tb = Math.Floor(mb / WALL_TILE);
		int par = (ta & 1) + 2 * (tb & 1);
		int v0 = CellV(c0, kind, par, ta, tb);
		int v1 = CellV(c1, kind, par, ta, tb);
		int v2 = CellV(c2, kind, par, ta, tb);
		int v3 = CellV(c3, kind, par, ta, tb);
		if (vector.DistanceSq(p0, p2) <= vector.DistanceSq(p1, p3))
		{
			Tri(v0, v1, v2);
			Tri(v0, v2, v3);
		}
		else
		{
			Tri(v0, v1, v3);
			Tri(v1, v2, v3);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! UV plane of a kind (0 top-down, 1 along x, 2 along z), grid-local metres
	protected void KindAB(int kind, vector p, out float a, out float b)
	{
		if (kind == 0)
		{
			a = p[0];
			b = p[2];
		}
		else if (kind == 1)
		{
			a = p[2];
			b = p[1];
		}
		else
		{
			a = p[0];
			b = p[1];
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Surface-net vertex of local cell lc (grid local): mean of the crossings on its 12 edges
	protected vector CellPos(int lc)
	{
		if (m_Cb.m_aCs[lc] == m_iStamp)
			return m_Cb.m_aCp[lc];
		int li = lc % 9;
		int lj = (lc / 9) % 9;
		int lk = lc / 81;
		int ci = m_Sub.m_i0 - 1 + li;
		int cj = m_Sub.m_j0 - 1 + lj;
		int ck = m_Sub.m_k0 - 1 + lk;
		int base = (ck * m_iSY + cj) * m_iSX + ci;
		vector bp = Vector(ci * CELL, cj * CELL, ck * CELL);
		vector sum = vector.Zero;
		int cnt = 0;
		for (int e = 0; e < 12; e++)
		{
			int qa = m_aEa[e];
			int qb = m_aEb[e];
			float da = m_aD[base + m_aOffS[qa]];
			float db = m_aD[base + m_aOffS[qb]];
			if ((da > 0) == (db > 0))
				continue;
			float t = da / (da - db);
			vector pa = bp + m_aOffV[qa];
			sum = sum + pa + (bp + m_aOffV[qb] - pa) * t;
			cnt++;
		}
		vector pos = bp + Vector(CELL * 0.5, CELL * 0.5, CELL * 0.5);
		if (cnt > 0)
			pos = sum * (1.0 / cnt);
		m_Cb.m_aCp[lc] = pos;
		m_Cb.m_aCs[lc] = m_iStamp;
		return pos;
	}

	//------------------------------------------------------------------------------------------------
	//! Vertex of local cell lc for UV kind + tile parity, sub-block local; UVs 0..1 inside the tile
	protected int CellV(int lc, int kind, int par, int ta, int tb)
	{
		int key = lc * 16 + kind * 4 + par;
		int val = m_Cb.m_aCv[key];
		if (val / 8192 == m_iStamp)
			return val % 8192;
		if (m_iV >= MAX_V)
		{
			m_bOverflow = true;
			return 0;
		}
		vector p = CellPos(lc);
		int v = m_iV;
		m_Vb.m_aV[v] = p - m_vSubOff;
		float a;
		float b;
		KindAB(kind, p, a, b);
		m_Vb.m_aUV[v * 2] = (a - ta * WALL_TILE) / WALL_TILE;
		m_Vb.m_aUV[v * 2 + 1] = 1 - (b - tb * WALL_TILE) / WALL_TILE;
		m_iV++;
		m_Cb.m_aCv[key] = m_iStamp * 8192 + v;
		return v;
	}

	//------------------------------------------------------------------------------------------------
	protected void Tri(int a, int b, int c)
	{
		if (m_bOverflow)
			return;
		if (m_iI + 3 > MAX_I)
		{
			m_bOverflow = true;
			return;
		}
		if (a == b || b == c || a == c)
			return;
		vector pa = m_Vb.m_aV[a];
		vector n = Cross(m_Vb.m_aV[b] - pa, m_Vb.m_aV[c] - pa);
		if (n.LengthSq() < 0.000000000001)
			return;
		m_Ib.m_aI[m_iI] = a;
		m_Ib.m_aI[m_iI + 1] = b;
		m_Ib.m_aI[m_iI + 2] = c;
		m_iI += 3;
	}

	//------------------------------------------------------------------------------------------------
	protected static vector Cross(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}
}
