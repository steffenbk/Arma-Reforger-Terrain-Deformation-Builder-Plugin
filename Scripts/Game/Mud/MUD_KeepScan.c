//! RUN 2 (2026-10-17 ~22:40, user: 'you cut away a lot of asphalt'): painted asphalt / concrete TERRAIN (not a
//! RoadEntity) was never in any mask - only props' pins kept bits of it. Now every cell, holes included, whose terrain
//! gamemat at a corner or the centre contains a HARD_WORDS word is kept (a WORLD trace still reads the hidden terrain
//! on a hole: y + gamemat, mud-gotchas 2026-10-08 19:58 / 2026-10-17 16:10). Log: 'MUD keepscan: mat ...' histogram.
//! TEMP tool 2026-10-17 (pin cut-out, TEST_KIT, SP / WB host only): writes a NEW keep mask + pin list for
//! Tools/mud_holes.py, on our HOLED map (unlike the archived MUD_HoleMask, which needed vanilla tiles).
//! User rule 2026-10-17: real terrain stays only under asphalt, concrete, buildings and real roads.
//! - Mask ($logs:mud_hole_mask2.txt, MUD_HoleMask format): every cell that is NOT a hole today (holes stay holes) is
//!   sampled again at its 4 corners + centre: kept when a road that is NOT a trail, water or a MUD_CutBoxEntity
//!   box is there. Trails = RoadEntity origins flagged 1 in $profile:mud_roads.txt (MUD_RoadListPlugin, World
//!   Editor); a road the list does not know counts as a real road (safe side) and is counted. Water uses the undug
//!   ground (MUD_WorldGroundEntity.UndugY = heightmap on kept cells, skin on holes), not GetSurfaceY (-256 on holes).
//! - Pins ($logs:mud_pins2.txt, MUD_RelYPins format): the cell under every RELATIVE_Y root that keeps real terrain =
//!   Building, SCR_PowerPole (poles: cables may be built from pole heights - not checked, kept on the safe side).
//!   Every other RELATIVE_Y root on a new hole is lifted at load by MUD_PinLift.
//! WHOLE-MAP CUT Task 2 (2026-10-22, plan docs/superpowers/plans/2026-10-18-whole-map-cut-plan.md, user asked for
//! craters on roads): KEEP_ROADS / KEEP_HARD false drop roads / asphalt + concrete terrain from the mask (writes
//! MASK_CUT / PINS_CUT, the files for Task 8); MAT_REPORT logs every armor cell (kept for a road or hard terrain)
//! by its centre gamemat ('MUD keepscan: armor road|terrain <mat> cells <n>') and the 64 m skin chunks that would
//! hold more than MUD_CoarseSkinEntity.MAT_SLOTS surfaces after the cut. Both true = today's mask (no change).
//! Runs once, START_S after the world ground is ready (logs a warning when $profile:mud_roads.txt is missing).
//! BUILD PLUGIN (stage 4, 2026-10-23): no longer TEMP - it is the scan pass of World Editor 'MUD Ground: Build'
//! (REQUEST below; runs without the test kit then). The cell is the skin's (was a fixed 2 m).
class MUD_KeepScan
{
	static const bool ON = true;     // stays on in release: Tick runs only with TEST_KIT or a Build request
	static const float START_S = 5;
	static const int BUDGET_MS = 30;
	static const float WATER_LIFT = 0.05;   // m above the ground (as MUD_HoleMask)
	static const float MATCH = 0.1;         // m: road origin vs list origin (XZ)
	static const string ROADS = "$profile:mud_roads.txt";
	static const string MASK = "$logs:mud_hole_mask2.txt";
	static const string PINS = "$logs:mud_pins2.txt";
	static const bool KEEP_ROADS = true;    // Task 2 / 8: false = road cells become holes (armored ground, plan Task 3-7)
	static const bool KEEP_HARD = true;     // false = painted asphalt / concrete terrain cells become holes
	static const bool MAT_REPORT = true;
	static const string MASK_CUT = "$logs:mud_hole_mask4.txt";
	static const string PINS_CUT = "$logs:mud_pins4.txt";
	//! BUILD PLUGIN stage 4 (2026-10-23): World Editor 'MUD Ground: Build' writes this file (MUD_GroundBuilder.
	//! WriteRequest); while it names the world being played, the scan runs WITHOUT the test kit and writes the mask +
	//! pins to the paths it gives. The plugin deletes it when it takes the result. Lines:
	//!   MUD build request world <world file>   /   mask <path>   /   pins <path>   /   MUD build request done
	static const string REQUEST = "$profile:MUD_Build/request.txt";

	protected static ref MUD_KeepScan s_Inst;
	protected static BaseWorld s_World;
	protected static int s_iRequest;            // 0 not read for this world, 1 none / other world, 2 asked for this world
	protected static string s_sReqMask;
	protected static string s_sReqPins;
	protected float m_fCell = 2;                // m, the skin's cell (Begin); 2 on Arland
	protected float m_fT0 = -1;
	protected int m_iState;     // 0 wait, 1 scan, 2 done / failed
	protected MUD_CoarseSkinEntity m_Skin;
	protected float m_fX0;
	protected float m_fZ0;
	protected float m_fXMax;
	protected float m_fZMax;
	protected float m_fTop;
	protected float m_fBottom;
	protected int m_iNX;
	protected int m_iNZ;
	protected int m_iRow;
	protected ref array<int> m_aPrev = {};
	protected ref array<int> m_aNext = {};
	protected ref array<vector> m_aListPos = {};
	protected ref array<int> m_aListTrail = {};
	protected ref array<IEntity> m_aSeen = {};     // runtime roads met so far
	protected ref array<int> m_aSeenTrail = {};    // 1 trail, 0 road, -1 not in the list (= road)
	protected IEntity m_LastRoad;
	protected bool m_bLastTrail;
	protected ref array<MUD_CutBoxEntity> m_aBoxes = {};
	protected ref array<IEntity> m_aFound = {};
	protected ref TraceParam m_Trace = new TraceParam();
	protected ref FileHandle m_File;
	protected int m_iKeep;
	protected int m_iRuns;
	protected int m_iRoad;
	protected int m_iWater;
	protected int m_iBox;
	protected int m_iUndugMiss;
	protected int m_iTrailHits;
	protected int m_iWorkMs;

	static const int ROAD = 1;
	static const int WATER = 2;
	static const int BOX = 4;
	static const int HARD = 8;
	static const string HARD_WORDS = "asphalt,concrete";   // as MUD_WorldGround m_sHardWords
	protected ref array<string> m_aHardWords = {};
	protected ref map<string, int> m_mMatHole = new map<string, int>();   // centre gamemat of hole cells
	protected ref map<string, int> m_mMatKept = new map<string, int>();   // centre gamemat of kept cells
	protected string m_sLastMat;
	protected int m_iHard;
	protected int m_iHardOnHole;
	protected ref map<string, int> m_mArmorRoad = new map<string, int>();      // MAT_REPORT: armor cell centre gamemat
	protected ref map<string, int> m_mArmorTerrain = new map<string, int>();
	protected ref map<int, ref map<string, int>> m_mChunkMats = new map<int, ref map<string, int>>();   // after the cut
	protected ref map<int, int> m_mChunkArmor = new map<int, int>();
	protected int m_iArmor;
	protected string m_sMask;
	protected string m_sPins;

	//------------------------------------------------------------------------------------------------
	static void Tick(float time)
	{
		if (!ON || System.IsConsoleApp() || RplSession.Mode() == RplMode.Client)
			return;
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		if (!s_Inst || s_World != world)
		{
			s_Inst = new MUD_KeepScan();
			s_World = world;
			s_iRequest = 0;
		}
		if (s_iRequest == 0)
			ReadRequest();
		if (!MUD_TestKit.TEST_KIT && s_iRequest != 2)
			return;
		s_Inst.Step(world, time);
	}

	//------------------------------------------------------------------------------------------------
	//! World path without a {GUID} or $addon: prefix, lower case, forward slashes
	static string PlainPath(string path)
	{
		path.Replace("\\", "/");
		int close = path.IndexOf("}");
		if (path.StartsWith("{") && close > 0)
			path = path.Substring(close + 1, path.Length() - close - 1);
		int colon = path.IndexOf(":");
		if (path.StartsWith("$") && colon > 0)
			path = path.Substring(colon + 1, path.Length() - colon - 1);
		while (path.StartsWith("/"))
			path = path.Substring(1, path.Length() - 1);
		path.ToLower();
		return path;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads REQUEST once per world: s_iRequest 2 when it names this world (and is complete), else 1
	protected static void ReadRequest()
	{
		s_iRequest = 1;
		FileHandle f = FileIO.OpenFile(REQUEST, FileMode.READ);
		if (!f)
			return;
		string reqWorld;
		string mask;
		string pins;
		bool done;
		string line;
		while (f.ReadLine(line) >= 0)
		{
			line = line.Trim();
			if (line.StartsWith("MUD build request world "))
				reqWorld = line.Substring(24, line.Length() - 24).Trim();
			else if (line.StartsWith("MUD build request mask "))
				mask = line.Substring(23, line.Length() - 23).Trim();
			else if (line.StartsWith("MUD build request pins "))
				pins = line.Substring(23, line.Length() - 23).Trim();
			else if (line == "MUD build request done")
				done = true;
		}
		f.Close();
		string here = PlainPath(GetGame().GetWorldFile());
		string asked = PlainPath(reqWorld);
		if (!done || mask.IsEmpty() || pins.IsEmpty() || asked.IsEmpty())
		{
			Print("MUD keepscan: build request " + REQUEST + " is incomplete - no build scan", LogLevel.WARNING);
			return;
		}
		if (here != asked && !here.EndsWith(asked) && !asked.EndsWith(here))
		{
			Print(string.Format("MUD keepscan: build request is for '%1', this world is '%2' - no build scan", asked, here), LogLevel.WARNING);
			return;
		}
		s_sReqMask = mask;
		s_sReqPins = pins;
		s_iRequest = 2;
		Print(string.Format("MUD keepscan: BUILD SCAN requested by the World Editor plugin for '%1' -> %2, %3", here, mask, pins), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void Step(BaseWorld world, float time)
	{
		if (m_iState == 2)
			return;
		if (m_fT0 < 0)
			m_fT0 = time;
		if (m_iState == 0)
		{
			if (time - m_fT0 >= START_S)
				Begin(world);
			return;
		}
		int t0 = System.GetTickCount();
		while (m_iRow < m_iNZ)
		{
			ScanRow(world, m_iRow);
			m_iRow++;
			if (m_iRow % 256 == 0)
			{
				Print(string.Format("MUD keepscan: row %1 / %2, keep %3", m_iRow, m_iNZ, m_iKeep), LogLevel.NORMAL);
				if (s_iRequest == 2)     // build plugin step 3: on-screen progress
					SCR_HintManagerComponent.ShowCustomHint(string.Format("Scanning the map: %1 percent\nLeave Play running.", Math.Round(100 * m_iRow / m_iNZ)), "Mud ground: step 3", 10, true);
			}
			if (System.GetTickCount(t0) >= BUDGET_MS)
				break;
		}
		m_iWorkMs += System.GetTickCount(t0);
		if (m_iRow < m_iNZ)
			return;
		m_iState = 2;
		string done = string.Format("MUD mask done keep %1 runs %2 road %3 water %4 box %5 ms %6", m_iKeep, m_iRuns, m_iRoad, m_iWater, m_iBox, m_iWorkMs);
		Print(string.Format("MUD keepscan: hard (asphalt / concrete terrain) cells %1, of them holes today %2", m_iHard, m_iHardOnHole), LogLevel.NORMAL);
		foreach (string mn, int mc : m_mMatHole)
			Print(string.Format("MUD keepscan: mat hole %1 cells %2", mn, mc), LogLevel.NORMAL);
		foreach (string kn, int kc : m_mMatKept)
			Print(string.Format("MUD keepscan: mat kept %1 cells %2", kn, kc), LogLevel.NORMAL);
		if (MAT_REPORT)
			ArmorReport();
		m_File.WriteLine(done);
		m_File.Close();
		m_File = null;
		Print(done + " -> " + m_sMask, LogLevel.NORMAL);
		int trail;
		int road;
		int unknown;
		foreach (int s : m_aSeenTrail)
		{
			if (s == 1)
				trail++;
			else if (s == 0)
				road++;
			else
				unknown++;
		}
		Print(string.Format("MUD keepscan: roads met %1 = trail %2 + road %3 + not in list %4 (kept as road); trail hits skipped %5, undug misses %6",
			m_aSeen.Count(), trail, road, unknown, m_iTrailHits, m_iUndugMiss), LogLevel.NORMAL);
		if (unknown > 0)
			Print("MUD keepscan: some roads are not in " + ROADS + " - wrong world open when the plugin ran?", LogLevel.WARNING);
		if (s_iRequest == 2)
		{
			Print("MUD keepscan: BUILD SCAN DONE - stop Play, then World Editor > Plugins > MUD Ground 4: Cut terrain", LogLevel.NORMAL);
			if (GetGame().GetPlayerController())
				SCR_HintManagerComponent.ShowCustomHint("Scan done (100 %).\nStop Play, then run Plugins > MUD Ground 4: Cut terrain (or Next step).", "Mud ground", 30, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool LoadRoads()
	{
		FileHandle f = FileIO.OpenFile(ROADS, FileMode.READ);
		if (!f)
		{
			Print("MUD keepscan: cannot open " + ROADS + " - run World Editor > Plugins > MUD List Roads first", LogLevel.WARNING);
			return false;
		}
		string line;
		int done = -1;
		while (f.ReadLine(line) >= 0)
		{
			array<string> w = {};
			line.Split(" ", w, true);
			if (w.Count() >= 6 && w[0] == "MUD" && w[1] == "road")
			{
				m_aListTrail.Insert(w[2].ToInt());
				m_aListPos.Insert(Vector(w[3].ToFloat(), w[4].ToFloat(), w[5].ToFloat()));
			}
			else if (w.Count() >= 5 && w[0] == "MUD" && w[1] == "roads" && w[2] == "done")
				done = w[4].ToInt();
		}
		f.Close();
		if (done != m_aListPos.Count())
		{
			Print(string.Format("MUD keepscan: %1 has %2 roads, done line says %3 - refused", ROADS, m_aListPos.Count(), done), LogLevel.ERROR);
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Begin(BaseWorld world)
	{
		m_iState = 2;
		m_sMask = MASK;
		m_sPins = PINS;
		if (!KEEP_ROADS || !KEEP_HARD)
		{
			m_sMask = MASK_CUT;
			m_sPins = PINS_CUT;
		}
		if (s_iRequest == 2)
		{
			m_sMask = s_sReqMask;
			m_sPins = s_sReqPins;
		}
		Print(string.Format("MUD keepscan: keep roads %1, keep hard terrain %2, mat report %3", KEEP_ROADS, KEEP_HARD, MAT_REPORT), LogLevel.NORMAL);
		// WB 2026-10-17 22:14: no keepscan line at all with the file there (inference: FileExists on $profile was false) - just open it
		Print("MUD keepscan: start, reading " + ROADS, LogLevel.NORMAL);
		array<string> words = {};
		HARD_WORDS.Split(",", words, true);
		foreach (string hw : words)
			m_aHardWords.Insert(hw.Trim());
		m_Skin = MUD_CoarseSkinEntity.GetInstance();
		if (!m_Skin || !m_Skin.IsReady() || !LoadRoads())
		{
			Print("MUD keepscan: no skin or road list - not run", LogLevel.WARNING);
			return;
		}
		m_fCell = m_Skin.GetCell();
		int ncx;
		int ncz;
		m_Skin.GetGrid(m_fX0, m_fZ0, ncx, ncz);
		m_iNX = ncx;
		m_iNZ = ncz;
		m_fXMax = m_fX0 + m_iNX * m_fCell - 0.01;   // 1 cm inside the far edges (MUD_HoleMask, 2026-10-08)
		m_fZMax = m_fZ0 + m_iNZ * m_fCell - 0.01;
		m_fTop = 1000;
		m_fBottom = -300;
		vector mins = Vector(m_fX0, -300, m_fZ0);
		vector maxs = Vector(m_fX0 + m_iNX * m_fCell, 1500, m_fZ0 + m_iNZ * m_fCell);
		world.QueryEntitiesByAABB(mins, maxs, AddBox);
		if (!WritePins(world, mins, maxs))
			return;

		m_File = FileIO.OpenFile(m_sMask, FileMode.WRITE);
		if (!m_File)
		{
			Print("MUD keepscan: cannot open " + m_sMask, LogLevel.ERROR);
			return;
		}
		m_File.WriteLine("MUD mask 1");
		m_File.WriteLine(string.Format("MUD mask box %1 %2 cells %3 %4 cell %5", m_fX0, m_fZ0, m_iNX, m_iNZ, m_fCell));
		Print(string.Format("MUD keepscan: %1 listed roads (%2 trails), %3 keep boxes, grid %4 x %5, writing %6",
			m_aListPos.Count(), m_aListTrail.Count() - CountRoads(), m_aBoxes.Count(), m_iNX, m_iNZ, m_sMask), LogLevel.NORMAL);
		for (int i = 0; i <= m_iNX; i++)
		{
			m_aPrev.Insert(0);
			m_aNext.Insert(0);
		}
		SampleVertexRow(world, 0, m_aPrev);
		m_iState = 1;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountRoads()
	{
		int n;
		foreach (int t : m_aListTrail)
		{
			if (t == 0)
				n++;
		}
		return n;
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddBox(IEntity e)
	{
		MUD_CutBoxEntity b = MUD_CutBoxEntity.Cast(e);
		if (b)
			m_aBoxes.Insert(b);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddRelY(IEntity e)
	{
		if (!e.GetParent() && (e.GetFlags() & EntityFlags.RELATIVE_Y))
			m_aFound.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool WritePins(BaseWorld world, vector mins, vector maxs)
	{
		m_aFound.Clear();
		world.QueryEntitiesByAABB(mins, maxs, AddRelY);
		FileHandle f = FileIO.OpenFile(m_sPins, FileMode.WRITE);
		if (!f)
		{
			Print("MUD keepscan: cannot open " + m_sPins, LogLevel.ERROR);
			return false;
		}
		f.WriteLine(string.Format("MUD pins box %1 %2 cells %3 %4 cell %5", m_fX0, m_fZ0, m_iNX, m_iNZ, m_fCell));
		set<int> seen = new set<int>();
		int keepers;
		int outside;
		foreach (IEntity e : m_aFound)
		{
			if (!MUD_PinLift.KeepsTerrain(e))
				continue;
			keepers++;
			vector o = e.GetOrigin();
			int cx = Math.Floor((o[0] - m_fX0) / m_fCell);
			int cz = Math.Floor((o[2] - m_fZ0) / m_fCell);
			if (cx < 0 || cz < 0 || cx >= m_iNX || cz >= m_iNZ)
			{
				outside++;
				continue;
			}
			int idx = cz * m_iNX + cx;
			if (seen.Contains(idx))
				continue;
			seen.Insert(idx);
			f.WriteLine(string.Format("MUD pin z %1 x %2", cz, cx));
		}
		string done = string.Format("MUD pins done pins %1 roots %2", seen.Count(), keepers);
		f.WriteLine(done);
		f.Close();
		Print(string.Format("%1 (of %2 RELATIVE_Y roots, outside %3) -> %4", done, m_aFound.Count(), outside, m_sPins), LogLevel.NORMAL);
		m_aFound.Clear();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! 1 trail, 0 listed road, -1 not in the list
	protected int Classify(IEntity e)
	{
		int i = m_aSeen.Find(e);
		if (i >= 0)
			return m_aSeenTrail[i];
		vector o = e.GetOrigin();
		int s = -1;
		for (int k = 0; k < m_aListPos.Count(); k++)
		{
			if (vector.DistanceSqXZ(m_aListPos[k], o) <= MATCH * MATCH)    // XZ: y may differ by RELATIVE_Y
			{
				s = m_aListTrail[k];
				break;
			}
		}
		m_aSeen.Insert(e);
		m_aSeenTrail.Insert(s);
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! Trace filter: real roads only (trails are passed through, the trace goes on below them)
	protected bool RealRoad(notnull IEntity e)
	{
		if (e.Type() != RoadEntity)
			return false;
		if (e != m_LastRoad)
		{
			m_LastRoad = e;
			m_bLastTrail = Classify(e) == 1;
		}
		if (m_bLastTrail)
			m_iTrailHits++;
		return !m_bLastTrail;
	}

	//------------------------------------------------------------------------------------------------
	protected int Sample(BaseWorld world, float x, float z)
	{
		x = Math.Min(x, m_fXMax);
		z = Math.Min(z, m_fZMax);
		float h;
		if (!MUD_WorldGroundEntity.UndugY(x, z, h))
		{
			m_iUndugMiss++;
			h = world.GetSurfaceY(x, z);
		}
		int f = 0;
		if (h > -255 && ChimeraWorldUtils.TryGetWaterSurfaceSimple(world, Vector(x, h + WATER_LIFT, z)))
			f |= WATER;
		m_Trace.Start = Vector(x, m_fTop, z);
		m_Trace.End = Vector(x, m_fBottom, z);
		m_Trace.Flags = TraceFlags.WORLD;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_Trace.TraceEnt = null;
		if (world.TraceMove(m_Trace, RealRoad) < 1 && m_Trace.TraceEnt && m_Trace.TraceEnt.Type() == RoadEntity && Classify(m_Trace.TraceEnt) != 1)
			f |= ROAD;
		// terrain surface (no filter: first WORLD hit = road mesh or the terrain, hidden or not)
		m_Trace.Start = Vector(x, m_fTop, z);
		m_Trace.End = Vector(x, m_fBottom, z);
		m_Trace.TraceEnt = null;
		m_Trace.SurfaceProps = null;
		m_sLastMat = "(none)";
		if (world.TraceMove(m_Trace, null) < 1 && m_Trace.SurfaceProps)
		{
			string mat = m_Trace.SurfaceProps.GetResourceName();
			m_sLastMat = mat;
			mat.ToLower();
			foreach (string w : m_aHardWords)
			{
				if (mat.Contains(w))
				{
					f |= HARD;
					break;
				}
			}
		}
		return f;
	}

	//------------------------------------------------------------------------------------------------
	protected void SampleVertexRow(BaseWorld world, int vz, array<int> flags)
	{
		float z = m_fZ0 + vz * m_fCell;
		for (int vx = 0; vx <= m_iNX; vx++)
			flags[vx] = Sample(world, m_fX0 + vx * m_fCell, z);
	}

	//------------------------------------------------------------------------------------------------
	protected bool InBox(float x, float z)
	{
		foreach (MUD_CutBoxEntity b : m_aBoxes)
		{
			if (!b)
				continue;
			vector mins, maxs;
			b.GetBox(mins, maxs);
			vector l = b.CoordToLocal(Vector(x, b.GetOrigin()[1], z));
			if (l[0] >= mins[0] && l[0] <= maxs[0] && l[2] >= mins[2] && l[2] <= maxs[2])
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void ScanRow(BaseWorld world, int cz)
	{
		SampleVertexRow(world, cz + 1, m_aNext);
		float zc = m_fZ0 + (cz + 0.5) * m_fCell;
		int start = -1;
		for (int cx = 0; cx < m_iNX; cx++)
		{
			float xc = m_fX0 + (cx + 0.5) * m_fCell;
			int f = Sample(world, xc, zc) | m_aPrev[cx] | m_aPrev[cx + 1] | m_aNext[cx] | m_aNext[cx + 1];
			bool hole = m_Skin.IsHoleCell(cx, cz);
			if (hole)
			{
				m_mMatHole.Set(m_sLastMat, m_mMatHole.Get(m_sLastMat) + 1);
				f &= HARD;      // holes today stay holes unless the terrain there is asphalt / concrete
			}
			else
			{
				m_mMatKept.Set(m_sLastMat, m_mMatKept.Get(m_sLastMat) + 1);
				if (!m_aBoxes.IsEmpty() && InBox(xc, zc))
					f |= BOX;
			}
			if (MAT_REPORT)
				ArmorNote(cx, cz, f, hole);
			if (!KEEP_ROADS)
				f &= ~ROAD;
			if (!KEEP_HARD)
				f &= ~HARD;
			if (f != 0)
			{
				m_iKeep++;
				if (f & HARD)
				{
					m_iHard++;
					if (hole)
						m_iHardOnHole++;
				}
				if (f & ROAD)
					m_iRoad++;
				if (f & WATER)
					m_iWater++;
				if (f & BOX)
					m_iBox++;
				if (start < 0)
					start = cx;
				continue;
			}
			if (start >= 0)
			{
				m_iRuns++;
				m_File.WriteLine(string.Format("MUD mask keep z %1 x %2 %3", cz, start, cx - 1));
				start = -1;
			}
		}
		if (start >= 0)
		{
			m_iRuns++;
			m_File.WriteLine(string.Format("MUD mask keep z %1 x %2 %3", cz, start, m_iNX - 1));
		}
		m_aPrev.Copy(m_aNext);
	}

	//------------------------------------------------------------------------------------------------
	//! MAT_REPORT: cell cx, cz with keep flags f (centre gamemat in m_sLastMat). Armor = kept for a road or hard
	//! terrain (a hole today with hard terrain counts too: the mask keeps it). Every cell that is a hole after the
	//! cut (holes today + armor cells) adds its gamemat to its 64 m skin chunk's surface count.
	protected void ArmorNote(int cx, int cz, int f, bool hole)
	{
		int ck = (cz / MUD_CoarseSkinEntity.CELLS) * 10000 + cx / MUD_CoarseSkinEntity.CELLS;
		bool armor = (f & (ROAD | HARD)) != 0 && (f & (WATER | BOX)) == 0;
		if (armor)
		{
			m_iArmor++;
			if (f & ROAD)
				m_mArmorRoad.Set(m_sLastMat, m_mArmorRoad.Get(m_sLastMat) + 1);
			else
				m_mArmorTerrain.Set(m_sLastMat, m_mArmorTerrain.Get(m_sLastMat) + 1);
			m_mChunkArmor.Set(ck, m_mChunkArmor.Get(ck) + 1);
		}
		if (!hole && !armor)
			return;
		map<string, int> mats = m_mChunkMats.Get(ck);
		if (!mats)
		{
			mats = new map<string, int>();
			m_mChunkMats.Set(ck, mats);
		}
		mats.Set(m_sLastMat, mats.Get(m_sLastMat) + 1);
	}

	//------------------------------------------------------------------------------------------------
	//! MAT_REPORT lines; the road + terrain buckets sum to the armor count (checked in the last line)
	protected void ArmorReport()
	{
		int sum;
		foreach (string rn, int rc : m_mArmorRoad)
		{
			Print(string.Format("MUD keepscan: armor road %1 cells %2", rn, rc), LogLevel.NORMAL);
			sum += rc;
		}
		foreach (string tn, int tc : m_mArmorTerrain)
		{
			Print(string.Format("MUD keepscan: armor terrain %1 cells %2", tn, tc), LogLevel.NORMAL);
			sum += tc;
		}
		int over;
		int overArmor;
		foreach (int ck, map<string, int> mats : m_mChunkMats)
		{
			if (mats.Count() <= MUD_CoarseSkinEntity.MAT_SLOTS)
				continue;
			over++;
			overArmor += m_mChunkArmor.Get(ck);
			Print(string.Format("MUD keepscan: chunk %1 x %2 holds %3 surfaces after the cut (armor cells %4)",
				ck % 10000, ck / 10000, mats.Count(), m_mChunkArmor.Get(ck)), LogLevel.NORMAL);
		}
		Print(string.Format("MUD keepscan: armor cells %1 (buckets sum %2), chunks over %3 surfaces %4 (armor cells in them %5)",
			m_iArmor, sum, MUD_CoarseSkinEntity.MAT_SLOTS, over, overArmor), LogLevel.NORMAL);
	}
}
