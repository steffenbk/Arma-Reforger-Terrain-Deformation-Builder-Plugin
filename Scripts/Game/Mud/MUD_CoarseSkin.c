[EntityEditorProps(category: "Mud", description: "Mud plan 2: fills every terrain hole cell of the whole map with the exact 2 m heightmap triangles (+1 mm). Collider on every machine, terrain-look mesh on clients. Place by class anywhere, one per world.", color: "0 200 255 255")]
class MUD_CoarseSkinEntityClass : GenericEntityClass
{
}

//! One 64 m skin chunk. Own class so MUD_BuildingDestructionFix keeps it out of a ruin's sink-and-delete query.
class MUD_CoarseChunkEntityClass : GenericEntityClass
{
}

class MUD_CoarseChunkEntity : GenericEntity
{
}

//! Scratch buffers for one chunk (static arrays live in a helper class: script class members max 256 KB)
class MUD_CoarseSkinBuffers
{
	vector m_aVerts[1089];
	float m_aUVs[2178];
	int m_aIndices[6144];       // mesh: hole cells real, the rest zero-area (all 6 on one vertex)
	int m_aColIdx[6144];        // collider: hole cells and sleeping covered cells real
	int m_aSplitIdx[6144];      // collider: hole triangles of one material
	int m_aCellSlot[1024];      // material slot of each cell, -1 = not a hole
}

//! One skin chunk with holes, kept after the build for the fine patches (plan 3): ~8.4 KB each.
class MUD_SkinChunk
{
	float m_aH[1089];           // vertex heights incl. m_fOffset, 33 x 33, bl = z * 33 + x
	int m_aCell[1024];          // -1 = not a hole, else slot | CELL_TLBR | CELL_COVER | CELL_SLEEP | CELL_VOX
	ref array<ResourceName> m_aMats = {};
	vector m_vOrigin;
	IEntity m_Ent;
	ref Resource m_MeshRes;
	Physics m_Phys;
	ref array<PhysicsGeom> m_aGeoms = {};   // geoms of m_Phys: Physics.Destroy() leaves them, DropBody frees
	int m_iCovered;

	//------------------------------------------------------------------------------------------------
	//! Old body, then its geoms (measured 2026-10-09 Exp 192624: ~270 KB kept per rebuild otherwise)
	void DropBody()
	{
		if (m_Phys)
		{
			m_Phys.Destroy();
			m_Phys = null;
		}
		foreach (PhysicsGeom g : m_aGeoms)
		{
			if (g)
				g.Destroy();
		}
		m_aGeoms.Clear();
	}
}

//! Coarse skin (mud plan 2, task 3; spec 5.1). Tools/mud_holes.py holes the map everywhere except
//! roads and water; this entity puts the ground back in every hole cell, so the map plays as before.
//! Plan 3's fine deformable patches will replace skin cells where the ground moves.
//! Per 64 m chunk (32 x 32 cells at 2 m): hole cell = GetSurfaceY < -255 at its centre (-256 in a
//! hole, proven 2026-10-08); heights from a WORLD trace (still hits the heightmap inside holes);
//! one extra trace per hole cell picks its diagonal and its game material. A chunk without holes
//! costs one GetSurfaceY per cell and nothing else.
//! Same skin on every machine from the same terrain: no replication. Dedicated server: colliders only.
class MUD_CoarseSkinEntity : GenericEntity
{
	static const int CELLS = 32;
	static const int VERTS = 33;
	static const int MAT_SLOTS = 6;     // MakeStatic takes 1-6 geoms
	static const int CELL_SLOT = 15;    // m_aCell bits 0-3: collider slot (index into m_aMats)
	static const int CELL_TLBR = 16;    // bit 4: shared edge tl-br (else bl-tr)
	static const int CELL_COVER = 32;   // bit 5: a fine patch covers the cell (plan 3)
	static const int CELL_SLEEP = 64;   // bit 6: the covering patch sleeps (no collider): mesh still hidden, skin collider on
	static const int CELL_VOX = 128;    // bit 7: TEMP probe 2026-10-13 (MUD_VoxelProbe overlay): a live voxel tile carries the
	                                    // cell - no skin mesh, no skin collider, whatever COVER / SLEEP say

	[Attribute("2", UIWidgets.EditBox, "Heightmap cell size (m). A chunk is 32 x 32 cells.", category: "Skin")]
	protected float m_fCell;

	[Attribute("{D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat", UIWidgets.ResourcePickerThumbnail, "Mesh material (terrain look)", "emat", category: "Skin")]
	protected ResourceName m_Material;

	[Attribute("{C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat", UIWidgets.ResourcePickerThumbnail, "Collider game material where the trace finds none", "gamemat", category: "Skin")]
	protected ResourceName m_GameMaterial;

	[Attribute("0.001", UIWidgets.EditBox, "Height above the heightmap (m). Grass clutter shows on our ground at +1 mm (2026-10-08).", category: "Skin")]
	protected float m_fOffset;

	[Attribute("1", UIWidgets.CheckBox, "Build the whole skin in EOnInit, before the first physics step: one freeze at load, nothing placed falls through. Off = spread over frames (Budget).", category: "Build")]
	protected bool m_bBuildAtInit;

	[Attribute("$Testingplanvechmsh:Data/Mud/mud_heights.c", UIWidgets.EditBox, "Baked heights file from Tools/mud_holes.py bake. ARLAND ONLY: blocks are keyed by chunk, another map would load wrong heights. Binary, but named .c: game FileIO opens only some extensions (.c .conf .et yes; .bin .json .txt .ttile .ent no; probe 2026-10-10 Exp 192624). Empty = trace every vertex.", category: "Build")]
	protected string m_sBaked;

	[Attribute("8", UIWidgets.EditBox, "Spread build: work per frame (ms), at least one chunk per frame", category: "Build")]
	protected int m_iBudgetMs;

	[Attribute("0", UIWidgets.CheckBox, "Colliders only, no meshes (what a dedicated server builds)", category: "Build")]
	protected bool m_bNoRender;

	[Attribute("0", UIWidgets.EditBox, "After the build: hole cells checked by a trace against the skin (0 = no check)", category: "Check")]
	protected int m_iCheckCells;

	protected ref MUD_CoarseSkinBuffers m_Buf;
	protected ref array<vector> m_aTodo = {};       // chunk origins (world XZ) not built yet
	protected ref array<IEntity> m_aChunks = {};
	protected ref array<ref MUD_SkinChunk> m_aRec = {};   // per chunk (z * m_iChunksX + x), null = no holes
	protected int m_iChunksX;
	protected static MUD_CoarseSkinEntity s_Inst;
	protected ref TraceParam m_Trace;
	protected bool m_bRender;
	protected bool m_bStarted;
	protected bool m_bDone;
	protected float m_fSinceDone;
	protected bool m_bLateChecked;
	protected bool m_bLastHit;
	protected float m_fChunk;
	protected float m_fX0;
	protected float m_fZ0;
	protected float m_fXMax;
	protected float m_fZMax;
	protected float m_fTop;
	protected float m_fBottom;
	protected int m_iNCX;               // cells across the terrain
	protected int m_iNCZ;
	protected int m_iTotal;
	protected int m_iEmpty;
	protected int m_iHoleCells;
	protected int m_iMiss;
	protected int m_iNonTerrain;
	protected ref array<string> m_aNonTerrain = {};
	protected int m_iRetraced;
	protected int m_iRoadKept;          // road hit with no terrain found under it: road height kept
	protected float m_fLastGood;        // last traced terrain height, the fallback on a miss
	protected float m_fRetraceSum;
	protected float m_fRetraceMax;
	protected int m_iJoined;
	protected int m_iFrames;
	protected int m_iWorstMs;
	protected int m_iWorkMs;
	protected int m_iHeightMs;
	protected int m_iMeshMs;
	protected int m_iColMs;
	protected int m_iStartTick;
	protected int m_iMemStartKB;
	protected ref map<int, ref array<float>> m_mBaked;     // chunk index -> 1089 heights, freed after the build
	protected int m_iBakedChunks;
	protected int m_iTracedChunks;

	//------------------------------------------------------------------------------------------------
	void MUD_CoarseSkinEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	void ~MUD_CoarseSkinEntity()
	{
		if (s_Inst == this)
			s_Inst = null;
		// Same order as MUD_WorldGroundEntity's dtor (body, geoms, entity); chunks the world already deleted keep
		// their geoms - freeing them after the entity went corrupted the heap on play stop (2026-10-09 12:56)
		int left;
		foreach (MUD_SkinChunk rec : m_aRec)
		{
			if (!rec)
				continue;
			rec.m_Phys = null;
			if (rec.m_Ent)
			{
				Physics ph = rec.m_Ent.GetPhysics();
				if (ph)
					ph.Destroy();
				foreach (PhysicsGeom g : rec.m_aGeoms)
				{
					if (g)
						g.Destroy();
				}
				delete rec.m_Ent;
			}
			else
			{
				left += rec.m_aGeoms.Count();
			}
			rec.m_aGeoms.Clear();
		}
		foreach (IEntity e : m_aChunks)
		{
			if (e)
				delete e;
		}
		if (left > 0)
			Print(string.Format("MUD coarse: teardown left %1 geoms (their chunk entities were already gone)", left), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame().InPlayMode())
			return;     // never in the World Editor's edit mode
		if (!Start())
			return;     // no terrain yet: EOnFrame retries
		if (m_bBuildAtInit)
			BuildAll("init");
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (m_bDone)
		{
			// second check 5 s later: the one in Finish() runs in the build frame (init check hit 940 / 2000,
			// 2026-10-09 00:05) - are new static bodies traceable only after a physics step?
			m_fSinceDone += timeSlice;
			if (!m_bLateChecked && m_iCheckCells > 0 && m_fSinceDone >= 5)
			{
				m_bLateChecked = true;
				Check("late");
			}
			return;
		}

		if (!m_bStarted)
		{
			if (!GetGame().InPlayMode() || !Start())
				return;
			if (m_bBuildAtInit)
			{
				BuildAll("first frame");
				return;
			}
		}

		m_iFrames++;
		int t0 = System.GetTickCount();
		while (!m_aTodo.IsEmpty())
		{
			BuildNext();
			if (System.GetTickCount(t0) >= m_iBudgetMs)
				break;
		}
		int ms = System.GetTickCount(t0);
		m_iWorkMs += ms;
		if (ms > m_iWorstMs)
			m_iWorstMs = ms;

		if (m_aTodo.IsEmpty())
			Finish("frames");
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildAll(string how)
	{
		int t0 = System.GetTickCount();
		while (!m_aTodo.IsEmpty())
		{
			BuildNext();
		}
		m_iWorkMs = System.GetTickCount(t0);
		m_iWorstMs = m_iWorkMs;
		m_iFrames = 1;
		Finish(how);
	}

	//------------------------------------------------------------------------------------------------
	//! List the chunks. False when there is no terrain yet.
	protected bool Start()
	{
		vector pos = GetOrigin();
		GenericTerrainEntity terrain;
		GenericWorldEntity worldEnt = GetGame().GetWorldEntity();
		if (worldEnt)
			terrain = worldEnt.GetTerrain(pos[0], pos[2]);
		if (!terrain)
			return false;

		m_bStarted = true;
		m_bRender = RplSession.Mode() != RplMode.Dedicated && !m_bNoRender;
		m_fChunk = CELLS * m_fCell;
		m_Buf = new MUD_CoarseSkinBuffers();
		m_Trace = new TraceParam();
		BuildUVs();

		vector mins, maxs;
		terrain.GetTerrainBoundBox(mins, maxs);
		m_fX0 = mins[0];
		m_fZ0 = mins[2];
		// samples stay 1 cm inside the far edges (a WORLD trace exactly on x = 4096 missed, 2026-10-08)
		m_fXMax = maxs[0] - 0.01;
		m_fZMax = maxs[2] - 0.01;
		m_fTop = maxs[1] + 10;
		m_fBottom = mins[1] - 10;
		m_iNCX = Math.Round((maxs[0] - mins[0]) / m_fCell);
		m_iNCZ = Math.Round((maxs[2] - mins[2]) / m_fCell);
		int nx = Math.Ceil(m_iNCX / 32.0);
		int nz = Math.Ceil(m_iNCZ / 32.0);
		m_iChunksX = nx;
		m_aRec.Clear();
		m_aRec.Resize(nx * nz);     // all null
		s_Inst = this;
		for (int cz = 0; cz < nz; cz++)
		{
			for (int cx = 0; cx < nx; cx++)
			{
				m_aTodo.Insert(Vector(m_fX0 + cx * m_fChunk, 0, m_fZ0 + cz * m_fChunk));
			}
		}
		m_iTotal = m_aTodo.Count();
		m_iStartTick = System.GetTickCount();
		m_iMemStartKB = System.MemoryAllocationKB();
		Print(string.Format("MUD coarse: terrain %1 .. %2, %3 x %4 chunks of %5 m, render %6, build at init %7, budget %8 ms",
			mins, maxs, nx, nz, m_fChunk, m_bRender, m_bBuildAtInit, m_iBudgetMs));
		LoadBaked();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Baked vertex heights (MUDH file, Tools/mud_holes.py bake): magic 'MUDH' read as int 1212437837,
	//! version 1, count, 32, then per block bx, bz, 1089 floats (no offset). Any fault: none kept, trace.
	protected void LoadBaked()
	{
		if (m_sBaked.IsEmpty())
			return;
		int t0 = System.GetTickCount();
		FileHandle f = FileIO.OpenFile(m_sBaked, FileMode.READ);
		if (!f)
		{
			Print(string.Format("MUD coarse: baked heights '%1' did not open - tracing", m_sBaked), LogLevel.ERROR);
			return;
		}
		int magic;
		int ver;
		int n;
		int cells;
		f.Read(magic, 4);
		f.Read(ver, 4);
		f.Read(n, 4);
		f.Read(cells, 4);
		int nz = m_aRec.Count() / m_iChunksX;
		if (magic != 1212437837 || ver != 1 || cells != CELLS || n < 0 || n > m_aRec.Count()
			|| f.GetLength() != 16 + n * (8 + 4 * VERTS * VERTS))
		{
			Print(string.Format("MUD coarse: baked heights '%1' bad header or length (magic %2 version %3 count %4 cells %5, %6 bytes) - tracing",
				m_sBaked, magic, ver, n, cells, f.GetLength()), LogLevel.ERROR);
			f.Close();
			return;
		}
		map<int, ref array<float>> baked = new map<int, ref array<float>>();
		for (int i = 0; i < n; i++)
		{
			int bx;
			int bz;
			f.Read(bx, 4);
			f.Read(bz, 4);
			int bi = bz * m_iChunksX + bx;
			if (bx < 0 || bz < 0 || bx >= m_iChunksX || bz >= nz || baked.Contains(bi))
			{
				Print(string.Format("MUD coarse: baked heights '%1' block %2 %3 outside the %4 x %5 chunks or twice - tracing",
					m_sBaked, bx, bz, m_iChunksX, nz), LogLevel.ERROR);
				f.Close();
				return;
			}
			array<float> h = {};
			h.Resize(VERTS * VERTS);
			f.ReadArray(h, 4, VERTS * VERTS);
			baked.Insert(bi, h);
		}
		bool whole = f.GetPos() == f.GetLength();
		f.Close();
		if (!whole)
		{
			Print(string.Format("MUD coarse: baked heights '%1' read stopped short of the file end - tracing", m_sBaked), LogLevel.ERROR);
			return;
		}
		// stale-file guard (the check below the build is off by default): one centre vertex in every n/64-th block (~64)
		// against the heightmap. A bake from another map or an older terrain is dropped, not built.
		int spots;
		float worst;
		int step = Math.Max(1, n / 64);
		for (int s = 0; s < baked.Count(); s += step)
		{
			int key = baked.GetKey(s);
			int sz = key / m_iChunksX;
			int sx = key - sz * m_iChunksX;
			float wx = Math.Min(m_fX0 + sx * m_fChunk + 16 * m_fCell, m_fXMax);
			float wz = Math.Min(m_fZ0 + sz * m_fChunk + 16 * m_fCell, m_fZMax);
			float err = Math.AbsFloat(baked.GetElement(s)[16 * VERTS + 16] - MUD_Surface.Get().HeightmapY(GetWorld(), wx, wz));
			worst = Math.Max(worst, err);
			spots++;
		}
		if (worst > 0.05)
		{
			Print(string.Format("MUD coarse: baked heights '%1' do not match this terrain (%2 spots, worst %3 m) - re-run mud_holes.py fit + bake; tracing",
				m_sBaked, spots, worst), LogLevel.ERROR);
			return;
		}
		m_mBaked = baked;
		Print(string.Format("MUD coarse: baked heights %1 blocks from '%2' (%3 ms), spot check %4 vertices worst %5 m",
			n, m_sBaked, System.GetTickCount(t0), spots, worst));
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildUVs()
	{
		for (int vz = 0; vz < VERTS; vz++)
		{
			for (int vx = 0; vx < VERTS; vx++)
			{
				int v = vz * VERTS + vx;
				m_Buf.m_aUVs[v * 2] = vx / 32.0;
				m_Buf.m_aUVs[v * 2 + 1] = 1 - vz / 32.0;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Heightmap height at a world XZ by WORLD trace (also inside holes). m_Trace keeps the hit
	//! (SurfaceProps) and m_bLastHit says whether there was one.
	protected float HeightAt(float x, float z)
	{
		x = Math.Min(x, m_fXMax);
		z = Math.Min(z, m_fZMax);
		m_Trace.Start = Vector(x, m_fTop, z);
		m_Trace.End = Vector(x, m_fBottom, z);
		m_Trace.Flags = TraceFlags.WORLD;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_Trace.TraceEnt = null;
		// WORLD traces also hit roads (TraceEnt RoadEntity, probe 2026-10-08 23:48): accept only the terrain,
		// and count what still gets through in case the filter does not apply to WORLD geometry
		float frac = GetWorld().TraceMove(m_Trace, TerrainOnly);
		m_bLastHit = frac < 1;
		if (!m_bLastHit)
		{
			// GetSurfaceY is -256 exactly in a hole (where every skin sample is): fall back to the last traced
			// height instead, a neighbour sample in the build loops. Counted in "trace misses".
			m_iMiss++;
			float s = GetWorld().GetSurfaceY(x, z);
			if (s > -255)
				return s;
			return m_fLastGood;
		}
		float y = m_fTop + (m_fBottom - m_fTop) * frac;
		// the filter does not stop WORLD road geometry: the spread build hit RoadEntity 90663 times (init build 0,
		// roads not there yet; 2026-10-09 09:23). Trace again from just under the hit until it is the terrain.
		float yFirst = y;
		for (int r = 0; r < 4 && m_Trace.TraceEnt && !GenericTerrainEntity.Cast(m_Trace.TraceEnt); r++)
		{
			m_Trace.Start = Vector(x, y - 0.01, z);
			m_Trace.TraceEnt = null;
			frac = GetWorld().TraceMove(m_Trace, TerrainOnly);
			if (frac >= 1)
			{
				m_Trace.TraceEnt = null;
				y = yFirst;   // nothing under the road: keep the first hit
				m_iRoadKept++;
				break;
			}
			y = m_Trace.Start[1] + (m_fBottom - m_Trace.Start[1]) * frac;
		}
		if (y != yFirst)
		{
			m_iRetraced++;
			m_fRetraceSum += yFirst - y;
			m_fRetraceMax = Math.Max(m_fRetraceMax, yFirst - y);
		}
		if (m_Trace.TraceEnt && !GenericTerrainEntity.Cast(m_Trace.TraceEnt))
		{
			m_iNonTerrain++;
			string cls = m_Trace.TraceEnt.ClassName();
			if (m_aNonTerrain.Count() < 5 && !m_aNonTerrain.Contains(cls))
				m_aNonTerrain.Insert(cls);
		}
		m_fLastGood = y;
		return y;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TerrainOnly(notnull IEntity e)
	{
		return GenericTerrainEntity.Cast(e) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildNext()
	{
		int last = m_aTodo.Count() - 1;
		BuildChunk(m_aTodo[last]);
		m_aTodo.Remove(last);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildChunk(vector o)
	{
		// TEMP probe 2026-10-13 (bricks): a chunk sampled anew loses its CELL_VOX bits (voxel cells would show twice)
		if (MUD_VoxelProbe.Started())
			Print(string.Format("MUD skin: chunk at %1 built after the voxel probe started - its voxel cells are lost", o), LogLevel.WARNING);
		MUD_SkinChunk rec = SampleChunk(o);
		if (!rec)
			return;
		int ix = Math.Round((o[0] - m_fX0) / m_fChunk);
		int iz = Math.Round((o[2] - m_fZ0) / m_fChunk);
		m_aRec[iz * m_iChunksX + ix] = rec;
		BuildGeometry(rec);
	}

	//------------------------------------------------------------------------------------------------
	//! Heights, hole cells, diagonals and game materials of the chunk at world XZ o; null = no holes.
	//! Same traces in the same order as plan 2 (the miss fallback m_fLastGood depends on the order).
	protected MUD_SkinChunk SampleChunk(vector o)
	{
		BaseWorld world = GetWorld();
		int t0 = System.GetTickCount();
		MUD_SkinChunk rec = new MUD_SkinChunk();
		rec.m_vOrigin = o;

		int holes = 0;
		// a map whose cell count is not a multiple of 32 has a last chunk reaching past the terrain, where
		// GetSurfaceY is -256 too (no terrain answers): those cells are never holes
		int ox = Math.Round((o[0] - m_fX0) / m_fCell);
		int oz = Math.Round((o[2] - m_fZ0) / m_fCell);
		for (int hz = 0; hz < CELLS; hz++)
		{
			for (int hx = 0; hx < CELLS; hx++)
			{
				int hc = hz * CELLS + hx;
				rec.m_aCell[hc] = -1;
				if (ox + hx >= m_iNCX || oz + hz >= m_iNCZ)
					continue;
				if (world.GetSurfaceY(o[0] + (hx + 0.5) * m_fCell, o[2] + (hz + 0.5) * m_fCell) < -255)
				{
					rec.m_aCell[hc] = 0;
					holes++;
				}
			}
		}
		if (holes == 0)
		{
			m_iEmpty++;
			m_iHeightMs += System.GetTickCount(t0);
			return null;
		}

		// baked vertex heights when the file has this chunk (plan 3 task 11), else one trace per vertex
		array<float> baked;
		if (m_mBaked)
		{
			int bix = Math.Round((o[0] - m_fX0) / m_fChunk);    // int first: never a float map key
			int biz = Math.Round((o[2] - m_fZ0) / m_fChunk);
			baked = m_mBaked.Get(biz * m_iChunksX + bix);
		}
		if (baked)
			m_iBakedChunks++;
		else
			m_iTracedChunks++;
		for (int vz = 0; vz < VERTS; vz++)
		{
			for (int vx = 0; vx < VERTS; vx++)
			{
				if (baked)
					rec.m_aH[vz * VERTS + vx] = baked[vz * VERTS + vx] + m_fOffset;
				else
					rec.m_aH[vz * VERTS + vx] = HeightAt(o[0] + vx * m_fCell, o[2] + vz * m_fCell) + m_fOffset;
			}
		}

		// per hole cell: diagonal whose midpoint is nearest the traced centre, and the centre's gamemat
		for (int z = 0; z < CELLS; z++)
		{
			for (int x = 0; x < CELLS; x++)
			{
				int c = z * CELLS + x;
				if (rec.m_aCell[c] < 0)
					continue;

				float hm = HeightAt(o[0] + (x + 0.5) * m_fCell, o[2] + (z + 0.5) * m_fCell) + m_fOffset;
				ResourceName mat;
				if (m_bLastHit)
				{
					GameMaterial gm = m_Trace.SurfaceProps;
					if (gm)
						mat = gm.GetResourceName();
				}
				if (mat.IsEmpty())
					mat = m_GameMaterial;
				int slot = rec.m_aMats.Find(mat);
				if (slot < 0)
				{
					if (rec.m_aMats.Count() < MAT_SLOTS)
					{
						slot = rec.m_aMats.Count();
						rec.m_aMats.Insert(mat);
					}
					else
					{
						slot = 0;   // more than 6 surfaces in one chunk: the rest join slot 0
						m_iJoined++;
					}
				}

				int bl = z * VERTS + x;
				float dA = Math.AbsFloat((rec.m_aH[bl] + rec.m_aH[bl + VERTS + 1]) * 0.5 - hm);
				float dB = Math.AbsFloat((rec.m_aH[bl + 1] + rec.m_aH[bl + VERTS]) * 0.5 - hm);
				int cell = slot;
				if (dB < dA - 0.001)
					cell = cell | CELL_TLBR;    // shared edge tl-br (99.93 % of non-flat Arland cells, 2026-10-08)
				rec.m_aCell[c] = cell;
			}
		}
		m_iHoleCells += holes;
		m_iHeightMs += System.GetTickCount(t0);
		return rec;
	}

	//------------------------------------------------------------------------------------------------
	//! Mesh and collider of one chunk: its hole cells that no fine patch covers. Spawns the chunk entity
	//! on first use; on a rebuild the old body is destroyed and the new one made in this same call.
	protected void BuildGeometry(MUD_SkinChunk rec)
	{
		if (!rec.m_Ent)
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = rec.m_vOrigin;
			rec.m_Ent = GetGame().SpawnEntity(MUD_CoarseChunkEntity, GetWorld(), params);
			if (!rec.m_Ent)
			{
				Print("MUD coarse: could not spawn chunk entity", LogLevel.ERROR);
				return;
			}
			m_aChunks.Insert(rec.m_Ent);
		}

		for (int vz = 0; vz < VERTS; vz++)
		{
			for (int vx = 0; vx < VERTS; vx++)
			{
				int v = vz * VERTS + vx;
				m_Buf.m_aVerts[v] = Vector(vx * m_fCell, rec.m_aH[v], vz * m_fCell);
			}
		}

		int n;
		for (int z = 0; z < CELLS; z++)
		{
			for (int x = 0; x < CELLS; x++)
			{
				int c = z * CELLS + x;
				int bl = z * VERTS + x;
				int br = bl + 1;
				int tl = bl + VERTS;
				int tr = tl + 1;
				int cell = rec.m_aCell[c];
				if (cell < 0)
				{
					// kept terrain: zero-area cell keeps the index count fixed
					for (int j = 0; j < 6; j++)
					{
						m_Buf.m_aIndices[n] = bl;
						m_Buf.m_aColIdx[n] = bl;
						n++;
					}
					m_Buf.m_aCellSlot[c] = -1;
					continue;
				}
				if ((cell & CELL_TLBR) != 0)
				{
					m_Buf.m_aColIdx[n] = bl;
					m_Buf.m_aColIdx[n + 1] = tl;
					m_Buf.m_aColIdx[n + 2] = br;
					m_Buf.m_aColIdx[n + 3] = tl;
					m_Buf.m_aColIdx[n + 4] = tr;
					m_Buf.m_aColIdx[n + 5] = br;
				}
				else
				{
					// shared edge bl-tr (also flat cells: both diagonals are the same there)
					m_Buf.m_aColIdx[n] = bl;
					m_Buf.m_aColIdx[n + 1] = tl;
					m_Buf.m_aColIdx[n + 2] = tr;
					m_Buf.m_aColIdx[n + 3] = tr;
					m_Buf.m_aColIdx[n + 4] = br;
					m_Buf.m_aColIdx[n + 5] = bl;
				}
				bool covered = (cell & CELL_COVER) != 0;
				bool vox = (cell & CELL_VOX) != 0;
				// a fine patch shows here: no skin mesh; its collider too unless that patch sleeps. A voxel tile: neither.
				if (vox || (covered && (cell & CELL_SLEEP) == 0))
					m_Buf.m_aCellSlot[c] = -1;
				else
					m_Buf.m_aCellSlot[c] = cell & CELL_SLOT;
				for (int k6 = 0; k6 < 6; k6++)
				{
					if (covered || vox)
						m_Buf.m_aIndices[n] = bl;
					else
						m_Buf.m_aIndices[n] = m_Buf.m_aColIdx[n];
					n++;
				}
			}
		}

		if (m_bRender)
		{
			int t1 = System.GetTickCount();
			int numVerts[] = {1089};
			int numIndices[] = {6144};
			string materials[] = {m_Material};
			Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
			if (res && res.GetResource())
			{
				MeshObject mesh = res.GetResource().ToMeshObject();
				// indices first: UpdateVerts rebuilds normals + tangents from the triangle list
				mesh.UpdateIndices(0, m_Buf.m_aIndices);
				mesh.UpdateVerts(0, m_Buf.m_aVerts, m_Buf.m_aUVs);
				rec.m_Ent.SetObject(mesh, "");
				rec.m_MeshRes = res;
			}
			else
			{
				Print(string.Format("MUD coarse: MeshObject.Create failed - material '%1' did not load?", m_Material), LogLevel.ERROR);
				m_bRender = false;
			}
			m_iMeshMs += System.GetTickCount(t1);
		}
		else if (!rec.m_MeshRes)
		{
			// dedicated server: no mesh = point bounds = traces skip the chunk (MUD_BoundsMesh.c)
			float yLo = rec.m_aH[0];
			float yHi = yLo;
			for (int ib = 0; ib < 1089; ib++)
			{
				float hb = rec.m_aH[ib];
				if (hb < yLo)
					yLo = hb;
				if (hb > yHi)
					yHi = hb;
			}
			float side = CELLS * m_fCell;
			rec.m_MeshRes = MUD_BoundsMesh.Make(rec.m_Ent, Vector(0, yLo - 1, 0), Vector(side, yHi + 1, side), m_Material);
			if (!rec.m_MeshRes)
				Print(string.Format("MUD coarse: bounds mesh failed - material '%1'", m_Material), LogLevel.ERROR);
		}

		int t2 = System.GetTickCount();
		array<PhysicsGeom> geoms = {};
		array<ResourceName> mats = {};
		array<int> layers = {};
		// same layers as the ground patches: Terrain preset + Foliage (vehicle get in/out sweep skips it)
		int layer = EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage;
		for (int k = 0; k < rec.m_aMats.Count(); k++)
		{
			int sn = 0;
			for (int sc = 0; sc < CELLS * CELLS; sc++)
			{
				if (m_Buf.m_aCellSlot[sc] != k)
					continue;
				for (int sj = 0; sj < 6; sj++)
				{
					m_Buf.m_aSplitIdx[sn] = m_Buf.m_aColIdx[sc * 6 + sj];
					sn++;
				}
			}
			if (sn == 0)
				continue;
			PhysicsGeom geom = PhysicsGeom.CreateTriMesh(m_Buf.m_aVerts, m_Buf.m_aSplitIdx, 1089, sn);
			if (!geom)
			{
				Print("MUD coarse: CreateTriMesh failed", LogLevel.ERROR);
				continue;
			}
			geoms.Insert(geom);
			mats.Insert(rec.m_aMats[k]);
			layers.Insert(layer);
		}
		rec.DropBody();
		if (!geoms.IsEmpty())
		{
			rec.m_Phys = MakeStatic(rec.m_Ent, geoms, mats, layers);
			if (rec.m_Phys)
			{
				rec.m_aGeoms.Copy(geoms);
			}
			else
			{
				Print("MUD coarse: CreateStaticEx failed", LogLevel.ERROR);
				foreach (PhysicsGeom g : geoms)
					g.Destroy();
			}
		}
		// the chunk got its mesh after it was spawned: Update() refreshes its bounds in the scene, else the renderer
		// keeps the empty spawn-time bounds and culls the chunk when its corner is off screen (user 2026-10-09 00:17;
		// shipped SCR_DestructionBaseHandler does SetObject then Update)
		rec.m_Ent.Update();
		m_iColMs += System.GetTickCount(t2);
	}

	//------------------------------------------------------------------------------------------------
	static MUD_CoarseSkinEntity GetInstance()
	{
		return s_Inst;
	}

	//------------------------------------------------------------------------------------------------
	bool IsReady()
	{
		return m_bDone;
	}

	//------------------------------------------------------------------------------------------------
	float GetCell()
	{
		return m_fCell;
	}

	//------------------------------------------------------------------------------------------------
	void GetGrid(out float x0, out float z0, out int ncx, out int ncz)
	{
		x0 = m_fX0;
		z0 = m_fZ0;
		ncx = m_iNCX;
		ncz = m_iNCZ;
	}

	//------------------------------------------------------------------------------------------------
	//! Record holding global cell (cx, cz) and the cell's index in it; null outside the map or no holes
	protected MUD_SkinChunk RecOf(int cx, int cz, out int c)
	{
		if (cx < 0 || cz < 0 || cx >= m_iNCX || cz >= m_iNCZ)
			return null;
		int ix = cx / CELLS;
		int iz = cz / CELLS;
		c = (cz - iz * CELLS) * CELLS + cx - ix * CELLS;
		return m_aRec[iz * m_iChunksX + ix];
	}

	//------------------------------------------------------------------------------------------------
	bool IsHoleCell(int cx, int cz)
	{
		int c;
		MUD_SkinChunk rec = RecOf(cx, cz, c);
		return rec && rec.m_aCell[c] >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Corner heights (incl. offset), diagonal and game material of hole cell (cx, cz); false if not a hole
	bool CellShape(int cx, int cz, out float h00, out float h10, out float h01, out float h11, out bool tlbr, out ResourceName mat)
	{
		int c;
		MUD_SkinChunk rec = RecOf(cx, cz, c);
		if (!rec || rec.m_aCell[c] < 0)
			return false;
		int lx = c % CELLS;
		int lz = c / CELLS;
		int bl = lz * VERTS + lx;
		h00 = rec.m_aH[bl];
		h10 = rec.m_aH[bl + 1];
		h01 = rec.m_aH[bl + VERTS];
		h11 = rec.m_aH[bl + VERTS + 1];
		tlbr = (rec.m_aCell[c] & CELL_TLBR) != 0;
		mat = rec.m_aMats[rec.m_aCell[c] & CELL_SLOT];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Cover (a fine patch took over) or uncover n x n hole cells from (cx0, cz0); rebuilds every chunk
	//! touched, now. Returns the number of cells that changed.
	int SetCovered(int cx0, int cz0, int n, bool covered)
	{
		array<MUD_SkinChunk> touched = {};
		int changed;
		for (int cz = cz0; cz < cz0 + n; cz++)
		{
			for (int cx = cx0; cx < cx0 + n; cx++)
			{
				int c;
				MUD_SkinChunk rec = RecOf(cx, cz, c);
				if (!rec || rec.m_aCell[c] < 0)
					continue;
				bool was = (rec.m_aCell[c] & CELL_COVER) != 0;
				if (was == covered)
					continue;
				if (covered)
				{
					rec.m_aCell[c] = rec.m_aCell[c] | CELL_COVER;
					rec.m_iCovered++;
				}
				else
				{
					rec.m_aCell[c] = rec.m_aCell[c] & ~CELL_COVER & ~CELL_SLEEP;
					rec.m_iCovered--;
				}
				changed++;
				if (!touched.Contains(rec))
					touched.Insert(rec);
			}
		}
		foreach (MUD_SkinChunk t : touched)
			BuildGeometry(t);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Sleep (the covering fine patch dropped its collider: the skin collider comes back, its mesh stays
	//! hidden) or wake n x n covered hole cells from (cx0, cz0); rebuilds every chunk touched, now.
	//! Returns the number of cells that changed.
	int SetSleep(int cx0, int cz0, int n, bool asleep)
	{
		array<MUD_SkinChunk> touched = {};
		int changed;
		for (int cz = cz0; cz < cz0 + n; cz++)
		{
			for (int cx = cx0; cx < cx0 + n; cx++)
			{
				int c;
				MUD_SkinChunk rec = RecOf(cx, cz, c);
				if (!rec || rec.m_aCell[c] < 0 || (rec.m_aCell[c] & CELL_COVER) == 0)
					continue;
				bool was = (rec.m_aCell[c] & CELL_SLEEP) != 0;
				if (was == asleep)
					continue;
				if (asleep)
					rec.m_aCell[c] = rec.m_aCell[c] | CELL_SLEEP;
				else
					rec.m_aCell[c] = rec.m_aCell[c] & ~CELL_SLEEP;
				changed++;
				if (!touched.Contains(rec))
					touched.Insert(rec);
			}
		}
		foreach (MUD_SkinChunk t : touched)
			BuildGeometry(t);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP probe 2026-10-13 (MUD_VoxelProbe ground test): cover / uncover each listed hole cell (cxs[n], czs[n]) as
	//! covered[n]; every touched chunk is rebuilt once, now. Returns the number of cells that changed.
	int SetCoveredCells(array<int> cxs, array<int> czs, array<bool> covered)
	{
		array<MUD_SkinChunk> touched = {};
		int changed;
		for (int n = 0; n < cxs.Count(); n++)
		{
			int c;
			MUD_SkinChunk rec = RecOf(cxs[n], czs[n], c);
			if (!rec || rec.m_aCell[c] < 0)
				continue;
			bool was = (rec.m_aCell[c] & CELL_COVER) != 0;
			bool want = covered[n];
			if (was == want)
				continue;
			if (want)
			{
				rec.m_aCell[c] = rec.m_aCell[c] | CELL_COVER;
				rec.m_iCovered++;
			}
			else
			{
				rec.m_aCell[c] = rec.m_aCell[c] & ~CELL_COVER & ~CELL_SLEEP;
				rec.m_iCovered--;
			}
			changed++;
			if (!touched.Contains(rec))
				touched.Insert(rec);
		}
		foreach (MUD_SkinChunk t : touched)
			BuildGeometry(t);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP probe 2026-10-13 (MUD_VoxelProbe overlay): a live voxel tile carries hole cell (cx, cz)
	bool IsVoxCell(int cx, int cz)
	{
		int c;
		MUD_SkinChunk rec = RecOf(cx, cz, c);
		return rec && rec.m_aCell[c] >= 0 && (rec.m_aCell[c] & CELL_VOX) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP probe 2026-10-13 (MUD_VoxelProbe overlay): set / clear CELL_VOX of each listed hole cell (cxs[n], czs[n]) as
	//! vox[n]; every touched chunk is rebuilt once, now. Returns the number of cells that changed.
	int SetVoxCells(array<int> cxs, array<int> czs, array<bool> vox)
	{
		array<MUD_SkinChunk> touched = {};
		int changed;
		for (int n = 0; n < cxs.Count(); n++)
		{
			int c;
			MUD_SkinChunk rec = RecOf(cxs[n], czs[n], c);
			if (!rec || rec.m_aCell[c] < 0)
				continue;
			bool was = (rec.m_aCell[c] & CELL_VOX) != 0;
			if (was == vox[n])
				continue;
			if (vox[n])
				rec.m_aCell[c] = rec.m_aCell[c] | CELL_VOX;
			else
				rec.m_aCell[c] = rec.m_aCell[c] & ~CELL_VOX;
			changed++;
			if (!touched.Contains(rec))
				touched.Insert(rec);
		}
		foreach (MUD_SkinChunk t : touched)
			BuildGeometry(t);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! TRACEABLE off (hide) or back on for every chunk (vehicle get in/out, only if plan 3 task 1 R6 failed)
	static void SetTraceHidden(bool hide)
	{
		if (!s_Inst)
			return;
		foreach (IEntity e : s_Inst.m_aChunks)
		{
			if (!e)
				continue;
			if (hide)
				e.ClearFlags(EntityFlags.TRACEABLE, false);
			else
				e.SetFlags(EntityFlags.TRACEABLE, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Static body from 1-6 geoms. CreateStaticEx takes a fixed-size array and no shipped script
	//! passes a variable count, so one literal array per count (copy of MUD_GroundEntity.MakeStatic).
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
	//! 5 s after the skin is built the world ground should be ready. A world save once dropped it from
	//! the layer (2026-10-10 21:44) and nothing deformed, with no error anywhere.
	protected static void CheckWorldGround()
	{
		if (!s_Inst)
			return;     // play already stopped
		if (!MUD_WorldGroundEntity.GetInstance())
			Print("MUD coarse: NO MUD world ground running 5 s after the skin was built - nothing will deform. Is Prefabs/Mud/MudWorldGround.et in the world layer?", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish(string how)
	{
		m_bDone = true;
		if (GetGame().InPlayMode())
			GetGame().GetCallqueue().CallLater(CheckWorldGround, 5000);
		int built = m_iTotal - m_iEmpty;
		Print(string.Format("MUD coarse: built %1 chunks with holes + %2 without = %3 | hole cells %4, tris %5 | via %6",
			built, m_iEmpty, m_iTotal, m_iHoleCells, m_iHoleCells * 2, how));
		Print(string.Format("MUD coarse: work %1 ms (heights %2, meshes %3, colliders %4), wall %5 ms, %6 frames, worst frame %7 ms",
			m_iWorkMs, m_iHeightMs, m_iMeshMs, m_iColMs, System.GetTickCount(m_iStartTick), m_iFrames, m_iWorstMs));
		Print(string.Format("MUD coarse: trace misses %1, cells joined to slot 0 (> 6 surfaces) %2, mem +%3 KB",
			m_iMiss, m_iJoined, System.MemoryAllocationKB() - m_iMemStartKB));
		string names;
		foreach (string n : m_aNonTerrain)
		{
			names += " " + n;
		}
		Print(string.Format("MUD coarse: height traces on non-terrain %1 (classes:%2)", m_iNonTerrain, names));
		float meanRe;
		if (m_iRetraced > 0)
			meanRe = m_fRetraceSum / m_iRetraced;
		Print(string.Format("MUD coarse: retraced under a road %1, first hit above terrain by mean %2 m, max %3 m; road height kept (no terrain under it) %4",
			m_iRetraced, meanRe, m_fRetraceMax, m_iRoadKept));
		if (m_sBaked != "")
			Print(string.Format("MUD coarse: chunks from baked heights %1, traced (no baked block) %2", m_iBakedChunks, m_iTracedChunks));
		m_mBaked = null;    // ~7.4 MB, not needed after the build
		if (m_iCheckCells > 0)
			Check("build");
	}

	//------------------------------------------------------------------------------------------------
	protected bool SkinOnly(notnull IEntity e)
	{
		return MUD_CoarseChunkEntity.Cast(e) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Trace the skin at hole cell centres spread over the map (odd stride over a power-of-two
	//! cell count visits every cell once) and compare with the heightmap height there.
	protected void Check(string when)
	{
		BaseWorld world = GetWorld();
		int total = m_iNCX * m_iNCZ;
		int tested;
		int hit;
		int within;
		float worst;
		TraceParam p = new TraceParam();
		int idx = 0;
		for (int k = 0; k < total && tested < m_iCheckCells; k++)
		{
			idx = (idx + 104729) % total;   // stays below 2^31 (k * 104729 would not)
			// int maths first: inside a float expression Enforce has no '%' and '/' would not truncate
			int icx = idx % m_iNCX;
			int icz = idx / m_iNCX;
			float x = m_fX0 + (icx + 0.5) * m_fCell;
			float z = m_fZ0 + (icz + 0.5) * m_fCell;
			if (world.GetSurfaceY(x, z) > -255)
				continue;   // not a hole: real terrain
			tested++;

			float hW = MUD_Surface.Get().HeightmapY(world, x, z) + m_fOffset;   // own trace, not the build's HeightAt
			p.Start = Vector(x, m_fTop, z);
			p.End = Vector(x, m_fBottom, z);
			// ENTS | WORLD: an ENTS-only trace saw the skin in only 940 of 2000 cells - the engine files part of
			// the static chunk bodies as WORLD geometry (they are there: ENTS|WORLD hit the missed cells at skin
			// height, 2026-10-09 00:12). The hit must be a skin chunk, not the heightmap 1 mm below.
			p.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
			p.LayerMask = EPhysicsLayerPresets.Projectile;
			p.TraceEnt = null;
			float frac = world.TraceMove(p, SkinOnly);
			if (frac >= 1 || !MUD_CoarseChunkEntity.Cast(p.TraceEnt))
				continue;
			hit++;
			float err = Math.AbsFloat(m_fTop + (m_fBottom - m_fTop) * frac - hW);
			if (err < 0.05)
				within++;
			if (err > worst)
				worst = err;
		}
		Print(string.Format("MUD coarse check (%1): %2 hole cells, skin hit %3, within 5 cm %4, worst %5 m", when, tested, hit, within, worst));
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(float timeSlice)
	{
		Shape.CreateSphere(ARGB(255, 0, 200, 255), ShapeFlags.WIREFRAME | ShapeFlags.ONCE, GetOrigin(), 2);
		super._WB_AfterWorldUpdate(timeSlice);
	}
#endif
}
