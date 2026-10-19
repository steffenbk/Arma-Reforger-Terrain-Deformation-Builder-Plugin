//! TEMP frame cost debugger (2026-10-18, user 'a more in-depth debugger for these issues'; research/2026-10-18-mud-zone-rut-perf.md).
//! Every MUD system that runs per frame from MUD_WorldGroundEntity.EOnFrame (and the mud sheet's own parts) adds its ms
//! here (System.GetTickCount: 1 ms steps - read the averages over many frames, not one frame). Once a frame, EndFrame
//! takes the engine's own split (System.GetProfileData: frame / render / GPU / simulation ms) and every 5 s prints:
//!   'MUD prof 5 s'      - fps, engine frame avg / max, render, GPU, simulation, and the whole MUD script share
//!   'MUD prof parts'    - per part: avg ms per frame / worst ms in one frame
//!   'MUD prof spikes'   - frames over 33 ms and how many of them MUD made (MUD >= half the frame)
//!   'MUD prof worst'    - the worst frame's split
//! CSV = true also writes one row per frame to $profile:MUD_prof_<tick>.csv (path in the first 'MUD prof' line).
//! Remove: this file + its calls (grep MUD_Prof).
class MUD_Prof
{
	static const bool ON = true;
	static const bool CSV = true;
	static const float EVERY_S = 5;
	static const int SPIKE_MS = 33;

	// parts (keep NAMES in the same order)
	static const int VOX = 0;           // MUD_VoxelProbe.Tick (ops in, go-live, bricks)
	static const int KEEPSCAN = 1;      // MUD_KeepScan.Tick
	static const int SHEET_SYNC = 2;    // MUD_MudSync.Drain (sync layer op steps)
	static const int SHEET = 3;         // MUD_MudSheet.Frame (whole; 10-14 are inside it)
	static const int HOUSE = 4;         // MUD_VoxelHouse.Tick
	static const int WHEELS = 5;        // MUD_WorldGroundEntity.Wheels
	static const int LOOK = 6;          // MUD_LookRuts.Tick
	static const int AHEAD = 7;         // AheadView + Ahead
	static const int REBUILD = 8;       // patch Rebuild + nav refresh
	static const int FREE_NET = 9;      // FreeIdle + net send / JIP
	static const int SH_FOLLOW = 10;    // inside SHEET: window follow
	static const int SH_WHEELS = 11;    // inside SHEET: wheel stamps
	static const int SH_GROUNDS = 12;   // inside SHEET: ground traces
	static const int SH_UPLOAD = 13;    // inside SHEET: live slot mesh builds
	static const int SH_STORED = 14;    // inside SHEET: stored tile rebuilds (sync restore)
	static const int SH_PRE = 15;       // inside SHEET: StepNow / ApplyPending (ONE GRID op hold)
	static const int SH_ZONES = 16;     // inside SHEET: BuildZones (camera radius tile builds, RAD_BUILD_MS)
	static const int SH_JOBS = 17;      // inside SHEET: GroundJobs (voxel change redo)
	static const int SH_EXPIRE = 18;    // inside SHEET: ExpirePreds (own stamps no op took)
	static const int W_TRACK = 19;      // inside WHEELS: MUD_VoxelProbe.RutWheel (all wheels)
	static const int W_AHEAD = 20;      // inside WHEELS: RutAhead + RutTail
	static const int W_FLUSH = 21;      // inside WHEELS: RutFlush (rut ops made / sent)
	static const int W_OTHER = 22;      // inside WHEELS: the rest (hold, press, contacts)
	static const int RB_INDEX = 23;     // inside REBUILD: index list builds
	static const int RB_MESH = 24;      // inside REBUILD: BuildMesh
	static const int RB_SMOOTH = 25;    // inside REBUILD: BeforeCollider + SmoothPatch
	static const int RB_COLLIDER = 26;  // inside REBUILD: BuildCollider
	static const int RB_NAV = 27;       // inside REBUILD: nav refresh
	static const int OP_ALL = 28;       // MUD_VoxelProbe.Submit whole (wherever it runs; ops in come inside VOX)
	static const int OP_FLUSH = 29;     // inside OP_ALL: MUD_MudSync.Flush (open sync op work ends)
	static const int OP_PRE = 30;       // inside OP_ALL: Prepare + sheet TakeOp + columns / rows before the carve
	static const int OP_CARVE = 31;     // inside OP_ALL: ApplyOp (CarveFloor for ruts)
	static const int OP_SMOOTH = 32;    // inside OP_ALL: SmoothBox + ClearFloor
	static const int OP_DIRTY = 33;     // inside OP_ALL: MUD_WorldGroundEntity.FloorDirty (patch mark + FloorFinish)
	static const int OP_REGROUND = 34;  // inside OP_ALL: MUD_MudSheet.FloorChanged (sheet ground re-read)
	static const int OP_SYNC = 35;      // inside OP_ALL: MUD_MudSync.RutOp / ClearOp
	static const int OP_POST = 36;      // inside OP_ALL: the rest (DispOpDone, QueueBox, brick queue, log; also everything of an op that returns early)
	static const int C_SEG = 37;        // inside OP_CARVE: CarveFloor segment loop (best profile per sample)
	static const int C_WRITE = 38;      // inside OP_CARVE: CarveFloor floor writes (FloorBase, tiles, hash)
	static const int C_RELAX = 39;      // inside OP_CARVE: RelaxFloor
	static const int C_RIDGE = 40;      // inside OP_CARVE: GrowRidges
	static const int Z_GROUND = 41;     // inside SH_ZONES: BuildZoneTile ground reads (GroundDown x 121)
	static const int Z_DATA = 42;       // inside SH_ZONES: BuildZoneTile mud data (sync layer reads + FillTile)
	static const int Z_SPAWN = 43;      // inside SH_ZONES: BuildZoneTile SpawnTile
	static const int Z_MESH = 44;       // inside SH_ZONES: BuildZoneTile BuildTile (mesh + upload)
	static const int T_SETUP = 45;     // inside every BuildTileMesh: arrays + vertex heights + gap probe
	static const int T_QUADS = 46;     // inside every BuildTileMesh: the quad loop (cuts, dirt / top split)
	static const int T_SKIRT = 47;     // inside every BuildTileMesh: the side skirt
	static const int T_ENGINE = 48;    // inside every BuildTileMesh: MeshObject.Create + UpdateIndices / Verts + SetObject
	static const int N = 49;
	static const int TOP = 10;          // parts 0..TOP-1 add up to the MUD total (10-18 inside SHEET, 19-22 inside WHEELS, 23-27 inside REBUILD)
	static const string NAMES = "vox,keepscan,sheet sync,sheet,house,wheels,look ruts,ahead,rebuild,free+net,sh follow,sh wheels,sh grounds,sh upload,sh stored,sh pre,sh zones,sh jobs,sh expire,w track,w ahead,w flush,w other,rb index,rb mesh,rb smooth,rb collider,rb nav,op all,op flush,op pre,op carve,op smooth,op dirty,op reground,op sync,op post,c seg,c write,c relax,c ridge,z ground,z data,z spawn,z mesh,t setup,t quads,t skirt,t engine";

	protected static ref array<int> s_aCur;     // this frame
	protected static ref array<int> s_aSum;     // this 5 s
	protected static ref array<int> s_aMax;
	protected static ref array<int> s_aWorst;   // split of the worst frame
	protected static ref array<string> s_aNames;
	protected static int s_iFrames;
	protected static float s_fT;
	protected static float s_fDtSum;
	protected static float s_fDtMax;
	protected static float s_fWorstDt;
	protected static int s_iWorstMud;
	protected static int s_iMudSum;
	protected static int s_iMudMax;
	protected static float s_fEngFrame;
	protected static float s_fEngFrameMax;
	protected static float s_fEngRender;
	protected static float s_fEngGpu;
	protected static float s_fEngSim;
	protected static int s_iSpikes;
	protected static int s_iSpikesMud;
	protected static ref FileHandle s_File;
	protected static string s_sPath;
	protected static int s_iRow;

	//------------------------------------------------------------------------------------------------
	protected static void Init()
	{
		s_aCur = {};
		s_aSum = {};
		s_aMax = {};
		s_aWorst = {};
		for (int i = 0; i < N; i++)
		{
			s_aCur.Insert(0);
			s_aSum.Insert(0);
			s_aMax.Insert(0);
			s_aWorst.Insert(0);
		}
		s_aNames = {};
		NAMES.Split(",", s_aNames, false);
		if (CSV)
		{
			string path = string.Format("$profile:MUD_prof_%1.csv", System.GetTickCount());
			s_sPath = path;
			s_File = FileIO.OpenFile(path, FileMode.WRITE);
			string head = "row,dt_ms,eng_frame_ms,eng_render_ms,eng_gpu_ms,eng_sim_ms,mud_ms";
			foreach (string nm : s_aNames)
				head += "," + nm;
			if (s_File)
				s_File.WriteLine(head);
			Print(string.Format("MUD prof: on, per-frame CSV %1 (open %2)", path, s_File != null), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! render A/B (MUD_MudSheet.RenderAB): its phase shows in the 5 s line; -1 = no A/B
	protected static int s_iAbPhase = -1;
	static string AbWord(int phase)
	{
		s_iAbPhase = phase;
		if (phase == 1)
			return "HIDDEN";
		return "shown";
	}

	//------------------------------------------------------------------------------------------------
	//! part's ms this frame (System.GetTickCount(t0) of the caller)
	static void Add(int part, int ms)
	{
		if (!ON)
			return;
		if (!s_aCur)
			Init();
		s_aCur.Set(part, s_aCur[part] + ms);
	}

	//------------------------------------------------------------------------------------------------
	//! Once a frame, end of MUD_WorldGroundEntity.EOnFrame: dt = timeSlice (s), mudMs = that EOnFrame's whole ms
	static void EndFrame(float dt, int mudMs)
	{
		if (!ON)
			return;
		if (!s_aCur)
			Init();
		float eFrame = 0;
		float eRender = 0;
		float eGpu = 0;
		float eSim = 0;
		ProfileData pd = System.GetProfileData();
		if (pd)
		{
			eFrame = pd.FrameTimeMs;
			eRender = pd.RenderTimeMs;
			eGpu = pd.GPUTimeMs;
			eSim = pd.SimulationTimeMs;
		}
		float dtMs = dt * 1000;
		s_iFrames++;
		s_fDtSum += dtMs;
		s_fDtMax = Math.Max(s_fDtMax, dtMs);
		s_iMudSum += mudMs;
		s_iMudMax = Math.Max(s_iMudMax, mudMs);
		s_fEngFrame += eFrame;
		s_fEngFrameMax = Math.Max(s_fEngFrameMax, eFrame);
		s_fEngRender += eRender;
		s_fEngGpu += eGpu;
		s_fEngSim += eSim;
		if (dtMs > SPIKE_MS)
		{
			s_iSpikes++;
			if (mudMs * 2 >= dtMs)
				s_iSpikesMud++;
		}
		bool worst = dtMs > s_fWorstDt;
		if (worst)
		{
			s_fWorstDt = dtMs;
			s_iWorstMud = mudMs;
		}
		string row = "";
		if (s_File)
			row = string.Format("%1,%2,%3,%4,%5,%6,%7", s_iRow, dtMs, eFrame, eRender, eGpu, eSim, mudMs);
		for (int i = 0; i < N; i++)
		{
			int v = s_aCur[i];
			s_aSum.Set(i, s_aSum[i] + v);
			if (v > s_aMax[i])
				s_aMax.Set(i, v);
			if (worst)
				s_aWorst.Set(i, v);
			if (s_File)
				row += "," + v.ToString();
			s_aCur.Set(i, 0);
		}
		if (s_File)
		{
			s_File.WriteLine(row);
			s_iRow++;
		}
		s_fT += dt;
		if (s_fT >= EVERY_S)
			Report();
	}

	//------------------------------------------------------------------------------------------------
	protected static string AbTag()
	{
		if (s_iAbPhase < 0)
			return "";
		if (s_iAbPhase == 1)
			return " | render A/B: sheet meshes HIDDEN";
		return " | render A/B: sheet meshes shown";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Report()
	{
		float n = Math.Max(1, s_iFrames);
		float fps = s_iFrames / Math.Max(0.001, s_fT);
		float mudAvg = s_iMudSum / n;
		float dtAvg = s_fDtSum / n;
		float share = 0;
		if (dtAvg > 0)
			share = 100 * mudAvg / dtAvg;
		Print(string.Format("MUD prof 5 s: %1 frames, fps %2, frame avg %3 ms max %4 | engine frame avg %5 max %6, render %7, GPU %8, simulation %9",
			s_iFrames, fps, dtAvg, s_fDtMax, s_fEngFrame / n, s_fEngFrameMax, s_fEngRender / n, s_fEngGpu / n, s_fEngSim / n)
			+ string.Format(" | MUD script avg %1 ms/frame (%2 pct of the frame), max %3 ms", mudAvg, share, s_iMudMax) + AbTag(), LogLevel.NORMAL);
		string parts = "MUD prof parts (avg ms/frame / worst ms):";
		for (int i = 0; i < N; i++)
		{
			if (s_aSum[i] == 0 && s_aMax[i] == 0)
				continue;
			float avg = s_aSum[i] / n;
			parts += string.Format(" %1 %2/%3,", s_aNames[i], avg.ToString(-1, 2), s_aMax[i]);
		}
		Print(string.Format("%1", parts), LogLevel.NORMAL);
		Print(string.Format("MUD prof spikes: %1 frames over %2 ms, %3 of them MUD >= half the frame", s_iSpikes, SPIKE_MS, s_iSpikesMud), LogLevel.NORMAL);
		string w = string.Format("MUD prof worst frame: %1 ms, MUD %2 ms:", s_fWorstDt, s_iWorstMud);
		for (int k = 0; k < N; k++)
		{
			if (s_aWorst[k] > 0)
				w += string.Format(" %1 %2,", s_aNames[k], s_aWorst[k]);
		}
		Print(string.Format("%1", w), LogLevel.NORMAL);
		if (s_File)
		{
			// closed and opened again every report: rows reach the disk even when play stops without a close
			s_File.Close();
			s_File = FileIO.OpenFile(s_sPath, FileMode.APPEND);
		}
		s_iFrames = 0;
		s_fT = 0;
		s_fDtSum = 0;
		s_fDtMax = 0;
		s_fWorstDt = 0;
		s_iWorstMud = 0;
		s_iMudSum = 0;
		s_iMudMax = 0;
		s_fEngFrame = 0;
		s_fEngFrameMax = 0;
		s_fEngRender = 0;
		s_fEngGpu = 0;
		s_fEngSim = 0;
		s_iSpikes = 0;
		s_iSpikesMud = 0;
		for (int j = 0; j < N; j++)
		{
			s_aSum.Set(j, 0);
			s_aMax.Set(j, 0);
			s_aWorst.Set(j, 0);
		}
	}
}
