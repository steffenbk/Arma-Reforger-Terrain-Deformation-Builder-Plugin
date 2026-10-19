//! MUD OVERLAY v3, option A (plan docs/superpowers/plans/2026-10-19-mud-overlay-v3.md, build 2026-10-19; checkpoint
//! v247 = before): the mud sheet's look as a client-only mesh on top of the untouched voxel ruts. No sync layer, no
//! hook in the rut code: it only READS MUD_VoxelProbe.FieldFloorAt / ColumnAt, MUD_WorldGroundEntity.DugUndug, the
//! wheel contacts of the vehicles this client sees (MUD_Wheels.GetAll) and MUD_VoxelProbe.OpCount (a hole check runs
//! the frame an op lands). Stamp / LimitDrag / tile mesh pieces are copied from MUD_MudSheet (its state is not used).
//! Per 0.15 m cell: mud d (THICK = untouched, less = pressed, more = pushed up) and texture drag u / v.
//! Drawn height per vertex: h = max(undug + (d - THICK), dug) - a pressed groove lands ON the felt rut floor, the
//! felt rut outside the tyre is filled back up into soft walls, ridges stand above the ground. Quads with all four
//! corners on the felt floor, quads with no stamped corner and quads touching a hole are not drawn.
//! Off (ENABLED false) = Tick returns at once, nothing made. Called from MUD_WorldGroundEntity's frame (render
//! machines only). TEST_KIT numpad 3 cycles off -> overlay presets -> pressed lift presets -> off (MUD_WorldGround
//! PRESS_LIFT); the two never run together.
class MUD_MudOverlay
{
	static bool ENABLED = false;               // default off; the TEST_KIT key turns it on
	static int s_iPreset = 1;                  // 1 = live stamps + re-centre + fallback, 2 = fallback only (late-join view)
	static const int PRESETS = 2;

	// grid (as MUD_MudSheet: 0.15 m cells, 3 m tiles, 21 x 21 vertices sharing their edges)
	static const float CELL = 0.15;
	static const int TQ = 20;
	static const int TV = 21;
	static const float TILE_M = 3;
	static const float SIZE = 12;
	static const int UV_TILES = 4;
	static const int PAD = 3;                  // hole cells read past each tile side (taper)
	static const int PW = 27;                  // TV + 2 * PAD

	// shape: MUD_MudSheet values (BOX + SOFT + GRADUAL, ridges + bow wave, drag + LimitDrag)
	static float THICK = 0.15;
	static float HALF_PER_R = 0.38;
	static float HALF_MIN = 0.11;
	static float HALF_MAX = 0.28;
	static float HALF_REF = 0.18;              // = MUD_MudSheet.HALF (band scales with half / HALF)
	static float BAND = 0.32;
	static float NOSE = 0.2;
	static float FRONT_LEN = 0.22;
	static float RIDGE_TOP = 0.35;
	static float W_FIRST = 0.55;
	static float SOFT_WALL = 0.35;
	static float DIRT_NOISE = 0.04;
	static float DRAG_TOUCH = 0.03;
	static float H0 = 0.05;
	static float H_SPEED = 0.012;
	static float H_SINK = 0.8;
	static float HMAX = 0.28;
	static float DRAG = 0.5;
	static float DRAG_MAX = 0.3;
	static float DRAG_LIM = 0.06;
	static float PRESS_STEP = 0.05;
	static float PASS_GAP = 4;
	static float TOUCH = 0.01;
	// height rule / holes (plan)
	static float EPS_DRAW = 0.005;             // a vertex this close over the felt floor counts as on it
	static float FILM = 0.005;                 // pass 1 (wear, no cut): pressed cells drawn this far over the ground, dirt
	static float FILM_CUT = 0.005;             // ... while the felt floor there is less than this under the undug ground
	static float MOVED = 0.08;                 // dug this far under undug and no stamped rut explains it = hole
	static float TAPER_CELLS = 3;              // ridges fall to the ground over this many cells toward a hole
	// re-centre (plan step 2)
	static float RECENTRE_R = 0.3;
	static float RECENTRE_STEP = 0.05;
	static float RECENTRE_AGE = 0.3;           // s a stamp waits before its floor is read
	static float RECENTRE_GIVEUP = 8;          // s: no cut under it by then (wear pass) = left where it is
	static float RECENTRE_CUT = 0.005;         // m: a floor this deep counts as cut
	// fallback (plan step 3)
	static float FALL_R = 70;                  // m round the camera that is scanned for unwatched ruts
	static float FALL_CUT_MIN = 0.01;
	static float FALL_LINK_MIN = 0.3;          // m between two centre points linked into one segment
	static float FALL_LINK_MAX = 0.55;
	static float FALL_LIVE_NEAR = 0.4;         // a centre point this close to a live stamp is watched ground: none
	static float FALL_HALF = 0.21;             // tyre half (Ural radius ~0.55 x HALF_PER_R)
	static float FALL_PASS_CUT = 0.03;         // cut per pass after the wear pass (RUT WEAR v234)
	static int FALL_PER_FRAME = 3;             // tiles checked per frame (8 lagged, WB 2026-10-20)
	// tiles and pacing (plan "Tiles and cost")
	static float BUILD_R = 75;                 // m: tiles with mud get a mesh within this
	static float WHEEL_EVERY = 0.1;            // s between rebuilds of a tile a live stamp hit (never held)
	static float OTHER_EVERY = 0.25;           // s between other rebuilds of one tile
	static int OTHER_MS = 3;                   // ms of other builds per frame (at least one)
	static float POLL_EVERY = 0.25;            // s between floor polls of built tiles
	static float OP_POLL = 0.1;                // s: an op landed - the next poll comes within this
	static float REPLAY_QUIET = 1;             // s with no live stamp in a tile before it replays (re-centre / fallback)
	static float REPLAY_EVERY = 2;             // s between replays of one tile
	static int REPLAY_MS = 2;                  // ms of replays per frame (at least one)
	static float POLL_R = 45;
	static int MESH_CAP = 500;                 // most tiles with a mesh (farthest go first)
	static int DATA_CAP = 4000;                // most tiles with data (farthest go first)
	static const float LOG_S = 10;
	static const ResourceName MAT = "{6EBF7ADA9FD582F7}Assets/Structures/Military/Fortifications/DirtMounds/Data/DirtCover_01.emat";   // = MUD_MudSheet.MAT
	static const ResourceName MAT_TOP = "{D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat";   // = MUD_MudSheet.MAT_TOP

	protected static ref MUD_MudOverlay s_Inst;
	protected static int s_iCycle;             // TEST_KIT key: 0 off, 1..PRESETS overlay, then pressed lift presets
	protected static float s_fTickLast = -1;
	protected static float s_fShownAt = -100;

	protected ref map<int, ref MUD_OvTile> m_mTiles = new map<int, ref MUD_OvTile>();
	protected ref array<ref MUD_OvVeh> m_aVeh = {};
	protected ref array<ref MUD_OvStamp> m_aPendRe = {};     // live stamps whose floor is not read yet
	protected ref MUD_OvPass m_Live = new MUD_OvPass();
	protected ref map<int, int> m_mFallSig = new map<int, int>();   // tile key -> floor signature of its last fallback scan
	protected ref array<int> m_aFallQ = {};    // tile keys still to scan this sweep
	protected ref array<int> m_aHoleNow = {};  // tile keys to rebuild this frame (a hole came)
	protected float m_fLast = -1;
	protected float m_fNextPoll;
	protected float m_fNextRe;
	protected float m_fNextKeep;
	protected int m_iOps = -2;
	protected int m_iNextVeh;
	protected vector m_vCam;
	// 10 s log
	protected float m_fLogAt = -1;
	protected int m_iBuilds;
	protected int m_iRecentred;                // stamps shifted (held now)
	protected int m_iHoleBuilds;
	protected int m_iMsStamp;                  // TEMP lag hunt: ms per part, 10 s
	protected int m_iMsFall;
	protected int m_iMsPoll;
	protected int m_iMsBuild;
	protected int m_iMsGround;
	protected int m_iMsReplay;
	// scratch (one build)
	protected int m_aHole[729];                // PW x PW: 1 = hole vertex
	protected float m_aDist[729];              // cells to the nearest hole vertex
	protected float m_aU[441];                 // undug
	protected float m_aG[441];                 // dug (felt floor)
	protected MUD_OvTile m_Cl;                 // last tile CellTile found (cache)
	protected int m_iCl;                       // cell index in it

	//------------------------------------------------------------------------------------------------
	static int Key(int tx, int tz)
	{
		return tx * 20000 + tz;
	}

	//------------------------------------------------------------------------------------------------
	static int FloorDiv(int a, int b)
	{
		if (a >= 0)
			return a / b;
		return -((-a + b - 1) / b);
	}

	//------------------------------------------------------------------------------------------------
	//! Called every frame by MUD_WorldGroundEntity (render machines, ground ready)
	static void Tick(float time)
	{
		// a new play keeps the statics: both designs back to off (defaults)
		if (time < s_fTickLast)
		{
			if (ENABLED)
				SetPreset(0, "new play");
			if (MUD_WorldGroundEntity.PRESS_LIFT)
				MUD_WorldGroundEntity.SetPressLift(0, "new play");
			s_iCycle = 0;
		}
		s_fTickLast = time;
		if (MUD_TestKit.TEST_KIT && Debug.KeyState(KeyCode.KC_NUMPAD3))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD3);
			Cycle();
			s_fShownAt = time;
		}
		if (MUD_TestKit.TEST_KIT)
			ShowMode(time);
		MUD_WorldGroundEntity.PressLog(time);     // pressed lift 10 s log (returns at once while it is off)
		if (!ENABLED)
			return;
		if (!s_Inst || time < s_Inst.m_fLast)
			s_Inst = new MUD_MudOverlay();     // first frame, or a new play
		s_Inst.Frame(time);
	}

	//------------------------------------------------------------------------------------------------
	//! TEST_KIT: which mud look is on, top left (while one is on; 'off' for 3 s after the key)
	protected static void ShowMode(float time)
	{
		string txt = "";
		if (ENABLED && s_iPreset == 1)
			txt = "DESIGN 1 mud overlay - preset 1: live stamps + re-centre + fallback";
		else if (ENABLED)
			txt = "DESIGN 1 mud overlay - preset 2: fallback only (late-join view)";
		else if (MUD_WorldGroundEntity.PRESS_LIFT)
			txt = string.Format("DESIGN 2 pressed lift - preset %1: lift %2 m, press %3 m, wear press %4", MUD_WorldGroundEntity.s_iPressPreset, MUD_WorldGroundEntity.s_fPressLiftM, MUD_WorldGroundEntity.s_fPressM, MUD_WorldGroundEntity.s_fWearPress);
		else if (time - s_fShownAt < 3)
			txt = "mud look OFF (today's ground)";
		if (txt == "")
			return;
		DbgUI.Begin("MUD look (numpad 3)", 20, 120);
		DbgUI.Text(string.Format("step %1 of %2: %3", s_iCycle, PRESETS + MUD_WorldGroundEntity.PRESS_PRESETS, txt));
		DbgUI.End();
	}

	//------------------------------------------------------------------------------------------------
	//! TEST_KIT numpad 3: off -> overlay presets -> pressed lift presets -> off
	protected static void Cycle()
	{
		s_iCycle++;
		if (s_iCycle > PRESETS + MUD_WorldGroundEntity.PRESS_PRESETS)
			s_iCycle = 0;
		if (s_iCycle == 0)
		{
			SetPreset(0, "key");
			MUD_WorldGroundEntity.SetPressLift(0, "key");
		}
		else if (s_iCycle <= PRESETS)
		{
			SetPreset(s_iCycle, "key");
		}
		else
		{
			MUD_WorldGroundEntity.SetPressLift(s_iCycle - PRESETS, "key");
		}
		Print(string.Format("MUD mud look cycle: step %1 = overlay %2 preset %3 | pressed lift %4 preset %5", s_iCycle, ENABLED, s_iPreset, MUD_WorldGroundEntity.PRESS_LIFT, MUD_WorldGroundEntity.s_iPressPreset), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Overlay preset p (0 = off); on turns the pressed lift off (the two never run together). Every tile is dropped
	//! and made again from what this client sees / the rut field (= rebuilds what is in view).
	static void SetPreset(int p, string why)
	{
		if (p > 0 && MUD_WorldGroundEntity.PRESS_LIFT)
		{
			Print("MUD mud overlay: on -> pressed lift turned off (the two never run together)", LogLevel.NORMAL);
			MUD_WorldGroundEntity.SetPressLift(0, "overlay on");
		}
		bool keepLive = ENABLED && p == 1 && s_iPreset == 1;
		if (s_Inst && !keepLive)
		{
			s_Inst.DropAll();
			s_Inst = null;
		}
		ENABLED = p > 0;
		if (p > 0)
			s_iPreset = p;
		Print(string.Format("MUD mud overlay: preset %1 (%2), on %3", p, why, ENABLED), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void DropAll()
	{
		foreach (int k, MUD_OvTile t : m_mTiles)
			DropMesh(t);
		m_Cl = null;
		m_mTiles.Clear();
		m_aPendRe.Clear();
		m_mFallSig.Clear();
		m_aFallQ.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void DropMesh(MUD_OvTile t)
	{
		if (t.m_Ent)
			SCR_EntityHelper.DeleteEntityAndChildren(t.m_Ent);
		t.m_Ent = null;
		t.m_Res = null;
		t.m_bMesh = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void Frame(float time)
	{
		m_fLast = time;
		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		m_vCam = cam[3];
		int tP = System.GetTickCount();
		if (s_iPreset == 1)
		{
			foreach (Vehicle veh : MUD_Wheels.GetAll())
			{
				if (veh)
					Wheels(veh, time);
			}
			if (time >= m_fNextRe)
			{
				m_fNextRe = time + 0.25;
				Recentre(time);
			}
		}
		m_iMsStamp += System.GetTickCount(tP);
		tP = System.GetTickCount();
		FallScan();
		m_iMsFall += System.GetTickCount(tP);
		tP = System.GetTickCount();
		// a landed op may have made a hole: every built tile near is checked this frame and the next two
		// LAG FIX (2026-10-20): an op brings the next poll forward to OP_POLL s (was: every built tile, 3 frames)
		int ops = MUD_VoxelProbe.OpCount();
		if (ops != m_iOps)
		{
			m_iOps = ops;
			m_fNextPoll = Math.Min(m_fNextPoll, time + OP_POLL);
		}
		if (time >= m_fNextPoll)
		{
			Poll(false);
			m_fNextPoll = time + POLL_EVERY;
		}
		if (time >= m_fNextKeep)
		{
			m_fNextKeep = time + 0.25;
			Keep();
		}
		m_iMsPoll += System.GetTickCount(tP);
		tP = System.GetTickCount();
		Builds(time);
		m_iMsBuild += System.GetTickCount(tP);
		Log(time);
	}

	//------------------------------------------------------------------------------------------------
	protected void Log(float time)
	{
		if (m_fLogAt < 0)
			m_fLogAt = time;
		if (time - m_fLogAt < LOG_S)
			return;
		m_fLogAt = time;
		int built = 0;
		int live = 0;
		int fall = 0;
		foreach (int k, MUD_OvTile t : m_mTiles)
		{
			if (t.m_bMesh)
				built++;
			foreach (MUD_OvStamp s : t.m_aStamps)
			{
				if (s.m_iOwner != k)
					continue;      // counted once, in its owner tile
				if (s.m_bFall)
					fall++;
				else
					live++;
			}
		}
		int tiles = m_mTiles.Count();
		Print(string.Format("MUD mud overlay: tiles %1 (built %2, kept %3), stamps live %4 / re-centred %5 / fallback %6, builds %7, preset %8", tiles, built, tiles - built, live, m_iRecentred, fall, m_iBuilds, s_iPreset), LogLevel.NORMAL);
		// TEMP lag hunt (WB 2026-10-20, where the time goes - not a perf number): ms per part over the 10 s
		Print(string.Format("MUD mud overlay parts ms / 10 s: stamps + re-centre %1, fallback %2, poll + keep %3, builds %4 (ground reads %5, replays %6)", m_iMsStamp, m_iMsFall, m_iMsPoll, m_iMsBuild, m_iMsGround, m_iMsReplay), LogLevel.NORMAL);
		m_iMsStamp = 0;
		m_iMsFall = 0;
		m_iMsPoll = 0;
		m_iMsBuild = 0;
		m_iMsGround = 0;
		m_iMsReplay = 0;
		if (m_iHoleBuilds > 0)
			Print(string.Format("MUD mud overlay: %1 builds in a hole frame", m_iHoleBuilds), LogLevel.NORMAL);
		m_iBuilds = 0;
		m_iHoleBuilds = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected MUD_OvTile GetTile(int tx, int tz, bool create)
	{
		int key = Key(tx, tz);
		MUD_OvTile t = m_mTiles.Get(key);
		if (t || !create)
			return t;
		t = new MUD_OvTile();
		t.Init(tx, tz, THICK);
		m_mTiles.Set(key, t);
		return t;
	}

	//------------------------------------------------------------------------------------------------
	//! World cell gi, gk -> its tile (m_Cl) and index (m_iCl); false = no tile there (create false)
	protected bool CellTile(int gi, int gk, bool create)
	{
		int tx = FloorDiv(gi, TQ);
		int tz = FloorDiv(gk, TQ);
		if (!m_Cl || m_Cl.m_iTx != tx || m_Cl.m_iTz != tz)
			m_Cl = GetTile(tx, tz, create);
		if (!m_Cl)
			return false;
		m_iCl = (gk - tz * TQ) * TQ + gi - tx * TQ;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected float CellD(int gi, int gk)
	{
		if (!CellTile(gi, gk, false))
			return THICK;
		return m_Cl.m_aD[m_iCl];
	}

	//------------------------------------------------------------------------------------------------
	//! GRADUAL: world cell -> pass map key (as MUD_MudSheet.PassKey)
	protected static int PassKey(int gi, int gk)
	{
		return (gi & 4095) | ((gk & 4095) << 12);
	}

	//------------------------------------------------------------------------------------------------
	//! Live stamps (as MUD_MudSheet.Wheels): one per wheel per 0.15 m of travel, on our soft ground only
	protected void Wheels(Vehicle veh, float time)
	{
		VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
		if (!sim)
			return;
		MUD_OvVeh mv;
		foreach (MUD_OvVeh m : m_aVeh)
		{
			if (m.m_Veh == veh)
				mv = m;
		}
		if (!mv)
		{
			mv = new MUD_OvVeh();
			mv.m_Veh = veh;
			m_iNextVeh++;
			mv.m_iId = m_iNextVeh;
			m_aVeh.Insert(mv);
		}
		int count = sim.WheelCount();
		while (mv.m_aLast.Count() < count)
		{
			mv.m_aLast.Insert(vector.Zero);
			mv.m_aTime.Insert(0);
			mv.m_aHas.Insert(0);
		}
		for (int w = 0; w < count; w++)
		{
			if (!sim.WheelHasContact(w))
			{
				mv.m_aHas[w] = 0;
				continue;
			}
			// armor rule: roads, kept terrain (asphalt / concrete), bricks get no mud - only our patches / skin
			IEntity ce = sim.WheelGetContactEntity(w);
			if (!MUD_FinePatchEntity.Cast(ce) && !MUD_CoarseChunkEntity.Cast(ce))
			{
				mv.m_aHas[w] = 0;
				continue;
			}
			vector cp = sim.WheelGetContactPosition(w);
			if (mv.m_aHas[w] != 0)
			{
				vector lp = mv.m_aLast[w];
				float len = Math.Sqrt((cp[0] - lp[0]) * (cp[0] - lp[0]) + (cp[2] - lp[2]) * (cp[2] - lp[2]));
				if (len < CELL)
					continue;
				float dt = time - mv.m_aTime[w];
				if (len < 3 && dt > 0)
					LiveStamp(lp, cp, len / dt, time, mv.m_iId, TyreHalf(sim.WheelGetRadius(w)));
			}
			mv.m_aLast[w] = cp;
			mv.m_aTime[w] = time;
			mv.m_aHas[w] = 1;
		}
	}

	//------------------------------------------------------------------------------------------------
	static float TyreHalf(float radius)
	{
		return Math.Clamp(radius * HALF_PER_R, HALF_MIN, HALF_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! One live tyre step: ridge height from speed + sink (ground beside the tyre vs its contact), stamped now and kept
	//! in every tile it reaches (re-centre / fallback replays)
	protected void LiveStamp(vector a, vector b, float speed, float time, int vehId, float half)
	{
		vector dir = Vector(b[0] - a[0], 0, b[2] - a[2]).Normalized();
		vector side = Vector(dir[2], 0, -dir[0]);
		float reach = half + BAND * half / HALF_REF;
		float sum = 0;
		int n = 0;
		for (int s = -1; s <= 1; s += 2)
		{
			vector q = b + side * (s * (reach + CELL));
			float dug;
			float undug;
			if (MUD_WorldGroundEntity.DugUndug(q[0], q[2], dug, undug))
			{
				sum += dug;
				n++;
			}
		}
		float sink = 0;
		if (n > 0)
			sink = Math.Max(0, sum / n - b[1]);
		MUD_OvStamp st = new MUD_OvStamp();
		st.m_vA = a;
		st.m_vB = b;
		st.m_fH = Math.Min(HMAX, H0 + H_SPEED * speed + H_SINK * sink);
		st.m_fHalf = half;
		st.m_fT = time;
		st.m_iVeh = vehId;
		Attach(st);
		Stamp(st, m_Live, null);
		m_aPendRe.Insert(st);
	}

	//------------------------------------------------------------------------------------------------
	//! Stamp box (world cells) of s: i0..i1, k0..k1 (members m_iB*)
	protected int m_iBi0;
	protected int m_iBi1;
	protected int m_iBk0;
	protected int m_iBk1;
	protected void StampBox(MUD_OvStamp s)
	{
		float half = s.m_fHalf;
		float reach = half + BAND * half / HALF_REF;
		vector dir = Vector(s.m_vB[0] - s.m_vA[0], 0, s.m_vB[2] - s.m_vA[2]);
		float len = dir.Length();
		if (len < 0.001)
			len = 0.001;
		dir = dir * (1 / len);
		float far = len + NOSE + FRONT_LEN;
		float ex = s.m_vA[0] + dir[0] * far;
		float ez = s.m_vA[2] + dir[2] * far;
		m_iBi0 = Math.Floor((Math.Min(s.m_vA[0], ex) - reach) / CELL);
		m_iBi1 = Math.Ceil((Math.Max(s.m_vA[0], ex) + reach) / CELL);
		m_iBk0 = Math.Floor((Math.Min(s.m_vA[2], ez) - reach) / CELL);
		m_iBk1 = Math.Ceil((Math.Max(s.m_vA[2], ez) + reach) / CELL);
	}

	//------------------------------------------------------------------------------------------------
	//! s goes into every tile its box reaches (made where missing); the first one is its owner
	protected void Attach(MUD_OvStamp s)
	{
		StampBox(s);
		int tx0 = FloorDiv(m_iBi0, TQ);
		int tx1 = FloorDiv(m_iBi1, TQ);
		int tz0 = FloorDiv(m_iBk0, TQ);
		int tz1 = FloorDiv(m_iBk1, TQ);
		s.m_aKeys.Clear();
		for (int tz = tz0; tz <= tz1; tz++)
		{
			for (int tx = tx0; tx <= tx1; tx++)
			{
				MUD_OvTile t = GetTile(tx, tz, true);
				t.m_aStamps.Insert(s);
				s.m_aKeys.Insert(Key(tx, tz));
				if (!s.m_bFall)
					t.m_bLive = true;
			}
		}
		if (s.m_iOwner == 0 && !s.m_aKeys.IsEmpty())
			s.m_iOwner = s.m_aKeys[0];
	}

	//------------------------------------------------------------------------------------------------
	//! One tyre step (MUD_MudSheet.Stamp maths: BOX + SOFT + GRADUAL, ridge + push ahead, drag). clip = only that
	//! tile's cells (replay), null = every tile (live). Every reached tile is marked dirty (live = wheel pace).
	protected void Stamp(MUD_OvStamp s, MUD_OvPass ctx, MUD_OvTile clip)
	{
		vector a = s.m_vA;
		vector b = s.m_vB;
		float time = s.m_fT;
		float half = s.m_fHalf;
		ctx.Prune(time, PASS_GAP);
		// WIDEN: how pressed the mud at the contact was when this pass began
		int wi = Math.Round(b[0] / CELL);
		int wk = Math.Round(b[2] / CELL);
		float wd = CellD(wi, wk);
		int wpk = PassKey(wi, wk);
		if (ctx.m_mT.Contains(wpk) && ctx.m_mV.Get(wpk) == s.m_iVeh && time - ctx.m_mT.Get(wpk) <= PASS_GAP)
			wd = ctx.m_mD.Get(wpk);
		float pressed = Math.Clamp((THICK - wd) / THICK, 0, 1);
		half = half * (W_FIRST + (1 - W_FIRST) * pressed);
		float band = BAND * half / HALF_REF;
		vector dir = Vector(b[0] - a[0], 0, b[2] - a[2]);
		float len = dir.Length();
		if (len < 0.001)
			return;
		dir = dir * (1 / len);
		vector side = Vector(dir[2], 0, -dir[0]);
		float reach = half + band;
		float nose = len + NOSE;
		float far = nose + FRONT_LEN;
		float ex = a[0] + dir[0] * far;
		float ez = a[2] + dir[2] * far;
		int i0 = Math.Floor((Math.Min(a[0], ex) - reach) / CELL);
		int i1 = Math.Ceil((Math.Max(a[0], ex) + reach) / CELL);
		int k0 = Math.Floor((Math.Min(a[2], ez) - reach) / CELL);
		int k1 = Math.Ceil((Math.Max(a[2], ez) + reach) / CELL);
		if (clip)
		{
			i0 = Math.Max(i0, clip.m_iTx * TQ);
			i1 = Math.Min(i1, clip.m_iTx * TQ + TQ - 1);
			k0 = Math.Max(k0, clip.m_iTz * TQ);
			k1 = Math.Min(k1, clip.m_iTz * TQ + TQ - 1);
		}
		float h = s.m_fH;
		float drag = DRAG * len;
		for (int k = k0; k <= k1; k++)
		{
			for (int i = i0; i <= i1; i++)
			{
				vector p = Vector(i * CELL - a[0], 0, k * CELL - a[2]);
				float t = vector.Dot(p, dir);
				if (t < 0 || t > far)
					continue;
				float lat = Math.AbsFloat(vector.Dot(p, side));
				if (lat >= reach || (t > nose && lat >= half))
					continue;
				if (!CellTile(i, k, clip == null))
					continue;
				MUD_OvTile ct = m_Cl;
				int v = m_iCl;
				float d0 = ct.m_aD[v];
				int pk = PassKey(i, k);
				if (!ctx.m_mT.Contains(pk) || ctx.m_mV.Get(pk) != s.m_iVeh || time - ctx.m_mT.Get(pk) > PASS_GAP)
					ctx.m_mD.Set(pk, d0);     // a new pass starts here
				else
					d0 = ctx.m_mD.Get(pk);
				ctx.m_mT.Set(pk, time);
				ctx.m_mV.Set(pk, s.m_iVeh);
				float target = 0;
				float du = 0;
				float dv = 0;
				if (t <= nose && lat < half)
				{
					// PRESS (SOFT_WALL: full in the middle, none at the side), texture dragged along
					float pf = 1;
					float inW = half * SOFT_WALL;
					float fromSide = half - lat;
					if (fromSide < inW)
						pf = 0.5 * (1 - Math.Cos(Math.PI * fromSide / inW));
					ct.m_aD[v] = Math.Min(ct.m_aD[v], Math.Max(0, Math.Min(d0, THICK) - PRESS_STEP * pf));
					du = dir[0] * drag;
					dv = dir[2] * drag;
				}
				else
				{
					float w = 1;
					vector pushDir = dir;
					if (t > nose)
					{
						// push ahead: tyre wide, highest at the tyre front, gone at FRONT_LEN
						w = Math.Cos((t - nose) / FRONT_LEN * Math.PI * 0.5);
					}
					else
					{
						// side ridge: round shoulder up from the wall, flat top, falls off to the band's outer edge
						float x = (lat - half) / band;
						if (x > RIDGE_TOP)
							w = 0.5 * (1 + Math.Cos(Math.PI * (x - RIDGE_TOP) / (1 - RIDGE_TOP)));
						else
							w = Math.Sin(0.5 * Math.PI * x / RIDGE_TOP);
						pushDir = side;
						if (vector.Dot(p, side) < 0)
							pushDir = -side;
					}
					target = THICK + h * w;
					target = Math.Min(target, Math.Max(d0, THICK) + h * w * PRESS_STEP / THICK);
					du = pushDir[0] * drag * 0.5 * w;
					dv = pushDir[2] * drag * 0.5 * w;
				}
				if (target > ct.m_aD[v])
					ct.m_aD[v] = target;
				float ou = ct.m_aOu[v] + du;
				float ov = ct.m_aOv[v] + dv;
				float ol = Math.Sqrt(ou * ou + ov * ov);
				if (ol > DRAG_MAX)
				{
					ou *= DRAG_MAX / ol;
					ov *= DRAG_MAX / ol;
				}
				ct.m_aOu[v] = ou;
				ct.m_aOv[v] = ov;
				ct.m_aSt[v] = 1;
				if (!clip)
				{
					ct.m_iDirty = 2;     // live: wheel pace
					ct.m_fLiveAt = time;
				}
			}
		}
		if (DRAG_LIM > 0)
			LimitDrag(i0, i1, k0, k1);
	}

	//------------------------------------------------------------------------------------------------
	//! UV STRETCH (MUD_MudSheet.LimitDrag): each vertex's drag within DRAG_LIM of its 4 neighbours, two sweeps
	protected void LimitDrag(int i0, int i1, int k0, int k1)
	{
		for (int sweep = 0; sweep < 2; sweep++)
		{
			for (int kk = k0; kk <= k1; kk++)
			{
				int k = kk;
				if (sweep == 1)
					k = k0 + k1 - kk;
				for (int ii = i0; ii <= i1; ii++)
				{
					int i = ii;
					if (sweep == 1)
						i = i0 + i1 - ii;
					if (!CellTile(i, k, false))
						continue;
					MUD_OvTile ct = m_Cl;
					int v = m_iCl;
					float ou = ct.m_aOu[v];
					float ov = ct.m_aOv[v];
					for (int q = 0; q < 4; q++)
					{
						int ni = i;
						int nk = k;
						if (q == 0)
							ni = i - 1;
						else if (q == 1)
							ni = i + 1;
						else if (q == 2)
							nk = k - 1;
						else
							nk = k + 1;
						if (!CellTile(ni, nk, false))
							continue;
						ou = Math.Clamp(ou, m_Cl.m_aOu[m_iCl] - DRAG_LIM, m_Cl.m_aOu[m_iCl] + DRAG_LIM);
						ov = Math.Clamp(ov, m_Cl.m_aOv[m_iCl] - DRAG_LIM, m_Cl.m_aOv[m_iCl] + DRAG_LIM);
					}
					ct.m_aOu[v] = ou;
					ct.m_aOv[v] = ov;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Tile t's field again from its stamps (fallback ones first, then live in their order), fresh pass maps
	protected void Replay(MUD_OvTile t)
	{
		int tR = System.GetTickCount();
		t.Init(t.m_iTx, t.m_iTz, THICK);
		MUD_OvPass ctx = new MUD_OvPass();
		for (int f = 0; f < 2; f++)
		{
			foreach (MUD_OvStamp s : t.m_aStamps)
			{
				if (s.m_bFall == (f == 0))
					Stamp(s, ctx, t);
			}
		}
		t.m_bReplay = false;
		if (t.m_iDirty == 0)
			t.m_iDirty = 1;
		m_iMsReplay += System.GetTickCount(tR);
	}

	//------------------------------------------------------------------------------------------------
	//! Plan step 2: a live stamp whose felt floor is cut moves sideways onto the deepest floor sample within
	//! RECENTRE_R (network smoothing offset); its tiles replay. Pass-1 stamps (no cut by RECENTRE_GIVEUP) stay.
	protected void Recentre(float time)
	{
		array<ref MUD_OvStamp> left = {};
		foreach (MUD_OvStamp s : m_aPendRe)
		{
			float age = time - s.m_fT;
			if (age < RECENTRE_AGE)
			{
				left.Insert(s);
				continue;
			}
			vector m = (s.m_vA + s.m_vB) * 0.5;
			float dug;
			float undug;
			if (!MUD_WorldGroundEntity.DugUndug(m[0], m[2], dug, undug))
				continue;
			vector dir = Vector(s.m_vB[0] - s.m_vA[0], 0, s.m_vB[2] - s.m_vA[2]).Normalized();
			vector side = Vector(dir[2], 0, -dir[0]);
			float best = undug - RECENTRE_CUT;
			float bestOff = 0;
			bool found = false;
			for (float off = -RECENTRE_R; off <= RECENTRE_R + 0.001; off += RECENTRE_STEP)
			{
				vector q = m + side * off;
				float f = MUD_VoxelProbe.FieldFloorAt(q[0], q[2], undug);
				if (f < best || (found && f == best && Math.AbsFloat(off) < Math.AbsFloat(bestOff)))
				{
					best = f;
					bestOff = off;
					found = true;
				}
			}
			if (!found)
			{
				if (age < RECENTRE_GIVEUP)
					left.Insert(s);
				continue;
			}
			if (Math.AbsFloat(bestOff) > 0.001)
			{
				s.m_vA = s.m_vA + side * bestOff;
				s.m_vB = s.m_vB + side * bestOff;
				m_iRecentred++;
				ReAttach(s);
			}
		}
		m_aPendRe = left;
	}

	//------------------------------------------------------------------------------------------------
	//! s moved: out of its old tiles, into the new ones, all of them replay
	protected void ReAttach(MUD_OvStamp s)
	{
		foreach (int ok : s.m_aKeys)
		{
			MUD_OvTile ot = m_mTiles.Get(ok);
			if (!ot)
				continue;
			ot.m_aStamps.RemoveItemOrdered(s);
			ot.m_bReplay = true;
		}
		int owner = s.m_iOwner;
		Attach(s);
		s.m_iOwner = owner;
		foreach (int nk : s.m_aKeys)
		{
			MUD_OvTile nt = m_mTiles.Get(nk);
			if (nt)
				nt.m_bReplay = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Plan step 3: tiles round the camera, a few a frame; a rut floor nobody here watched gets fallback stamps along
	//! its centre lines (made again when the floor there changes)
	protected void FallScan()
	{
		if (m_aFallQ.IsEmpty())
		{
			int r = Math.Ceil(FALL_R / TILE_M);
			int cx = Math.Floor(m_vCam[0] / TILE_M);
			int cz = Math.Floor(m_vCam[2] / TILE_M);
			for (int dz = -r; dz <= r; dz++)
			{
				for (int dx = -r; dx <= r; dx++)
				{
					if (dx * dx + dz * dz <= r * r)
						m_aFallQ.Insert(Key(cx + dx, cz + dz));
				}
			}
		}
		for (int n = 0; n < FALL_PER_FRAME && !m_aFallQ.IsEmpty(); n++)
		{
			int key = m_aFallQ[m_aFallQ.Count() - 1];
			m_aFallQ.Remove(m_aFallQ.Count() - 1);
			int tx = Math.Round(key / 20000.0);
			int tz = key - tx * 20000;
			FallTile(tx, tz, key);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Coarse floor signature of tile tx, tz (0.5 m samples, FLOOR_NONE corners = no floor); 0 = no floor at all
	protected int FloorSig(int tx, int tz)
	{
		int sig = 0;
		for (int sz = 0; sz < 6; sz++)
		{
			for (int sx = 0; sx < 6; sx++)
			{
				float x = tx * TILE_M + (sx + 0.5) * 0.5;
				float z = tz * TILE_M + (sz + 0.5) * 0.5;
				float f = MUD_VoxelProbe.FieldFloorAt(x, z, MUD_VoxelProbe.FLOOR_NONE);
				if (f >= MUD_VoxelProbe.FLOOR_NONE * 0.5)
					continue;
				int cm = Math.Round(f * 100);
				sig = sig + cm * (sx + 7 * sz + 1) + 1;
			}
		}
		return sig;
	}

	//------------------------------------------------------------------------------------------------
	protected void FallTile(int tx, int tz, int key)
	{
		int sig = FloorSig(tx, tz);
		int old = 0;
		if (m_mFallSig.Contains(key))
			old = m_mFallSig.Get(key);
		if (sig == old)
			return;
		m_mFallSig.Set(key, sig);
		MUD_OvTile t = m_mTiles.Get(key);
		// old fallback stamps this tile owns go
		if (t)
			DropFall(t, key);
		if (sig == 0)
			return;
		// 0.25 m samples over the tile + 0.5 m pad: cut = undug - floor
		array<float> cut = {};
		cut.Resize(289);
		for (int sz = 0; sz < 17; sz++)
		{
			for (int sx = 0; sx < 17; sx++)
			{
				float x = tx * TILE_M - 0.5 + sx * 0.25;
				float z = tz * TILE_M - 0.5 + sz * 0.25;
				cut[sz * 17 + sx] = 0;
				float dug;
				float undug;
				if (!MUD_WorldGroundEntity.DugUndug(x, z, dug, undug) || MUD_VoxelProbe.ColumnAt(x, z))
					continue;
				float f = MUD_VoxelProbe.FieldFloorAt(x, z, undug);
				if (f < undug)
					cut[sz * 17 + sx] = undug - f;
			}
		}
		// centre points: inside the tile, cut >= FALL_CUT_MIN, a cut maximum across x or z (+-2 samples)
		array<vector> pts = {};
		array<float> pcut = {};
		for (int pz = 2; pz < 14; pz++)
		{
			for (int px = 2; px < 14; px++)
			{
				int c = pz * 17 + px;
				float cc = cut[c];
				if (cc < FALL_CUT_MIN)
					continue;
				bool mx = cc >= cut[c - 1] && cc >= cut[c + 1] && cc >= cut[c - 2] && cc >= cut[c + 2];
				bool mz = cc >= cut[c - 17] && cc >= cut[c + 17] && cc >= cut[c - 34] && cc >= cut[c + 34];
				if (!mx && !mz)
					continue;
				vector pp = Vector(tx * TILE_M - 0.5 + px * 0.25, 0, tz * TILE_M - 0.5 + pz * 0.25);
				if (t && NearLive(t, pp))
					continue;     // watched ground: the live stamps draw it
				pts.Insert(pp);
				pcut.Insert(cc);
			}
		}
		FallLink(pts, pcut, key);
	}

	//------------------------------------------------------------------------------------------------
	//! Centre points linked into segments FALL_LINK_MIN..MAX apart; each segment = one stamp per pass its cut says
	protected void FallLink(array<vector> pts, array<float> pcut, int key)
	{
		int n = pts.Count();
		for (int a = 0; a < n; a++)
		{
			for (int b = a + 1; b < n; b++)
			{
				float dx = pts[b][0] - pts[a][0];
				float dz = pts[b][2] - pts[a][2];
				float dd = Math.Sqrt(dx * dx + dz * dz);
				if (dd < FALL_LINK_MIN || dd > FALL_LINK_MAX)
					continue;
				float c = 0.5 * (pcut[a] + pcut[b]);
				int passes = 1 + Math.Round(c / FALL_PASS_CUT);
				passes = Math.ClampInt(passes, 1, 4);
				for (int ps = 1; ps <= passes; ps++)
				{
					MUD_OvStamp st = new MUD_OvStamp();
					st.m_vA = pts[a];
					st.m_vB = pts[b];
					st.m_fH = Math.Min(HMAX, H0 + H_SINK * c);
					st.m_fHalf = FALL_HALF;
					st.m_fT = ps * (PASS_GAP + 1);       // replay time: each pass its own
					st.m_iVeh = -ps;
					st.m_bFall = true;
					st.m_iOwner = key;
					Attach(st);
					foreach (int sk : st.m_aKeys)
					{
						MUD_OvTile tt = m_mTiles.Get(sk);
						if (tt)
							tt.m_bReplay = true;
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool NearLive(MUD_OvTile t, vector p)
	{
		float r2 = FALL_LIVE_NEAR * FALL_LIVE_NEAR;
		foreach (MUD_OvStamp s : t.m_aStamps)
		{
			if (s.m_bFall)
				continue;
			vector m = (s.m_vA + s.m_vB) * 0.5;
			float dx = m[0] - p[0];
			float dz = m[2] - p[2];
			if (dx * dx + dz * dz < r2)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Fallback stamps owned by tile key go from every tile holding them (those replay)
	protected void DropFall(MUD_OvTile t, int key)
	{
		array<ref MUD_OvStamp> gone = {};
		foreach (MUD_OvStamp s : t.m_aStamps)
		{
			if (s.m_bFall && s.m_iOwner == key)
				gone.Insert(s);
		}
		foreach (MUD_OvStamp g : gone)
		{
			foreach (int gk : g.m_aKeys)
			{
				MUD_OvTile gt = m_mTiles.Get(gk);
				if (!gt)
					continue;
				gt.m_aStamps.RemoveItemOrdered(g);
				gt.m_bReplay = true;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Plan step 6: is the vertex at x, z a hole (voxel column, unknown ground, or dug more than MOVED under undug
	//! that no stamped rut floor explains)? Also gives undug / dug.
	protected bool HoleAt(float x, float z, int gi, int gk, out float dug, out float undug)
	{
		dug = 0;
		undug = 0;
		if (MUD_VoxelProbe.ColumnAt(x, z))
			return true;
		if (!MUD_WorldGroundEntity.DugUndug(x, z, dug, undug))
			return true;
		if (undug - dug <= MOVED)
			return false;
		float rf = MUD_VoxelProbe.FieldFloorAt(x, z, undug);
		bool rut = rf < MUD_VoxelProbe.FLOOR_NONE * 0.5 && rf <= dug + 0.01;
		if (!rut)
			return true;
		if (!CellTile(gi, gk, false))
			return true;
		return m_Cl.m_aSt[m_iCl] == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Coarse ground signature of a built tile (36 vertices): felt floor in cm, holes x 7919
	protected int GroundSig(MUD_OvTile t, out int holes)
	{
		int sig = 0;
		holes = 0;
		for (int sz = 0; sz <= 5; sz++)
		{
			for (int sx = 0; sx <= 5; sx++)
			{
				int gi = t.m_iTx * TQ + sx * 4;
				int gk = t.m_iTz * TQ + sz * 4;
				float dug;
				float undug;
				if (HoleAt(gi * CELL, gk * CELL, gi, gk, dug, undug))
				{
					holes++;
					continue;
				}
				int cm = Math.Round(dug * 100);
				sig = sig + cm * (sx + 7 * sz + 1);
			}
		}
		return sig + holes * 7919;
	}

	//------------------------------------------------------------------------------------------------
	//! Floor poll (plan: no hook): built tiles near the camera whose felt floor changed rebuild; a new / gone hole
	//! rebuilds the tile and its neighbours this frame (all = every built tile, the frames after an op)
	protected void Poll(bool all)
	{
		float r2 = POLL_R * POLL_R;
		if (all)
			r2 = BUILD_R * BUILD_R;
		foreach (int k, MUD_OvTile t : m_mTiles)
		{
			if (!t.m_bMesh)
				continue;
			float ux = (t.m_iTx + 0.5) * TILE_M - m_vCam[0];
			float uz = (t.m_iTz + 0.5) * TILE_M - m_vCam[2];
			if (ux * ux + uz * uz > r2)
				continue;
			int holes;
			int sig = GroundSig(t, holes);
			if (sig == t.m_iSig)
				continue;
			bool holeChg = holes != t.m_iHoles;
			t.m_iSig = sig;
			t.m_iHoles = holes;
			if (!holeChg)
			{
				if (t.m_iDirty == 0)
					t.m_iDirty = 1;
				continue;
			}
			for (int dz = -1; dz <= 1; dz++)
			{
				for (int dx = -1; dx <= 1; dx++)
				{
					int nk = Key(t.m_iTx + dx, t.m_iTz + dz);
					MUD_OvTile nt = m_mTiles.Get(nk);
					if (nt && nt.m_bMesh && !m_aHoleNow.Contains(nk))
						m_aHoleNow.Insert(nk);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Mesh on approach, drop past the caps (farthest first)
	protected void Keep()
	{
		float r2 = BUILD_R * BUILD_R;
		int meshes = 0;
		array<int> keys = {};
		array<float> dist = {};
		foreach (int k, MUD_OvTile t : m_mTiles)
		{
			float ux = (t.m_iTx + 0.5) * TILE_M - m_vCam[0];
			float uz = (t.m_iTz + 0.5) * TILE_M - m_vCam[2];
			float d2 = ux * ux + uz * uz;
			t.m_fCam2 = d2;
			if (t.m_bMesh)
				meshes++;
			else if (d2 <= r2 && t.m_iDirty == 0 && !t.m_aStamps.IsEmpty())
				t.m_iDirty = 1;
			keys.Insert(k);
			dist.Insert(d2);
		}
		int over = m_mTiles.Count() - DATA_CAP;
		int meshOver = meshes - MESH_CAP;
		while ((over > 0 || meshOver > 0) && !keys.IsEmpty())
		{
			int far = 0;
			for (int j = 1; j < keys.Count(); j++)
			{
				if (dist[j] > dist[far])
					far = j;
			}
			MUD_OvTile ft = m_mTiles.Get(keys[far]);
			if (ft && ft.m_fCam2 <= r2)
				break;     // the rest are in range
			if (ft)
			{
				if (ft.m_bMesh)
				{
					DropMesh(ft);
					meshOver--;
				}
				if (over > 0)
				{
					m_Cl = null;
					m_mTiles.Remove(keys[far]);
					over--;
				}
			}
			keys.Remove(far);
			dist.Remove(far);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Plan pacing: hole tiles now; wheel tiles every WHEEL_EVERY, never held; others every OTHER_EVERY within
	//! OTHER_MS a frame. Replays first (re-centre / fallback changed their stamps).
	protected void Builds(float time)
	{
		foreach (int hk : m_aHoleNow)
		{
			MUD_OvTile ht = m_mTiles.Get(hk);
			if (!ht)
				continue;
			if (ht.m_bReplay)
				Replay(ht);
			Build(ht, time);
			m_iHoleBuilds++;
		}
		m_aHoleNow.Clear();
		float r2 = BUILD_R * BUILD_R;
		int t0 = System.GetTickCount();
		int others = 0;
		int replays = 0;
		foreach (int k, MUD_OvTile t : m_mTiles)
		{
			// LAG FIX 2 (WB 2026-10-20: replays 9.7 s of 10 s): a tile replays only once no wheel stamped it for
			// REPLAY_QUIET s, at most every REPLAY_EVERY s, within REPLAY_MS a frame (at least one)
			if (t.m_bReplay && time - t.m_fLiveAt >= REPLAY_QUIET && time - t.m_fReplayAt >= REPLAY_EVERY)
			{
				if (replays == 0 || System.GetTickCount(t0) < REPLAY_MS)
				{
					Replay(t);
					t.m_fReplayAt = time;
					replays++;
				}
			}
			if (t.m_iDirty == 0)
				continue;
			float ux = (t.m_iTx + 0.5) * TILE_M - m_vCam[0];
			float uz = (t.m_iTz + 0.5) * TILE_M - m_vCam[2];
			if (ux * ux + uz * uz > r2 && !t.m_bMesh)
				continue;      // far: built on approach
			float since = time - t.m_fBuiltAt;
			if (t.m_iDirty == 2)
			{
				if (since >= WHEEL_EVERY)
					Build(t, time);
				continue;
			}
			if (since < OTHER_EVERY)
				continue;
			if (others > 0 && System.GetTickCount(t0) >= OTHER_MS)
				continue;
			Build(t, time);
			others++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Hole flags of the padded vertex grid (PW x PW) + undug / dug of the tile's own vertices, then the chamfer
	//! distance to the nearest hole. True = some hole in reach.
	protected int m_aNeed[441];        // 1 = own vertex within one of a stamped one (only those can be in a drawn quad)
	protected int m_iNeedN;
	protected float m_fOrgY;
	protected bool Grounds(MUD_OvTile t)
	{
		// LAG FIX (WB look run 2026-10-20): ground reads only where a quad can be drawn; the pad ring = voxel columns only
		m_iNeedN = 0;
		for (int n = 0; n < TV * TV; n++)
		{
			m_aNeed[n] = 0;
		}
		for (int sv = 0; sv < TV * TV; sv++)
		{
			int svk = sv / TV;
			int svi = sv - svk * TV;
			VertData(t, svi, svk);
			if (m_iVs == 0)
				continue;
			for (int nk = Math.Max(0, svk - 1); nk <= Math.Min(TV - 1, svk + 1); nk++)
			{
				for (int ni = Math.Max(0, svi - 1); ni <= Math.Min(TV - 1, svi + 1); ni++)
				{
					int nl = nk * TV + ni;
					m_aNeed[nl] = 1;
				}
			}
		}
		if (!t.m_G)
			t.m_G = new MUD_OvGround();
		MUD_OvGround cg = t.m_G;
		int ops = MUD_VoxelProbe.OpCount();
		if (cg.m_iOps != ops)
		{
			cg.m_iOps = ops;
			for (int ck = 0; ck < TV * TV; ck++)
			{
				cg.m_aKnown[ck] = 0;
			}
			for (int cp = 0; cp < PW * PW; cp++)
			{
				cg.m_aPad[cp] = -1;
			}
		}
		int gi0 = t.m_iTx * TQ - PAD;
		int gk0 = t.m_iTz * TQ - PAD;
		bool any = false;
		bool org = false;
		for (int pk = 0; pk < PW; pk++)
		{
			for (int pi = 0; pi < PW; pi++)
			{
				int gi = gi0 + pi;
				int gk = gk0 + pk;
				int l = pk * PW + pi;
				m_aHole[l] = 0;
				int li = pi - PAD;
				int lk = pk - PAD;
				if (li < 0 || lk < 0 || li >= TV || lk >= TV)
				{
					if (cg.m_aPad[l] < 0)
					{
						cg.m_aPad[l] = 0;
						if (MUD_VoxelProbe.ColumnAt(gi * CELL, gk * CELL))
							cg.m_aPad[l] = 1;
					}
					if (cg.m_aPad[l] == 1)
					{
						m_aHole[l] = 1;
						any = true;
					}
					continue;
				}
				int ol = lk * TV + li;
				m_aU[ol] = 0;
				m_aG[ol] = 0;
				if (m_aNeed[ol] == 0)
					continue;
				m_iNeedN++;
				if (cg.m_aKnown[ol] == 0)
				{
					float rDug;
					float rUndug;
					cg.m_aHole[ol] = 0;
					if (HoleAt(gi * CELL, gk * CELL, gi, gk, rDug, rUndug))
						cg.m_aHole[ol] = 1;
					cg.m_aU[ol] = rUndug;
					cg.m_aG[ol] = rDug;
					cg.m_aKnown[ol] = 1;
				}
				if (cg.m_aHole[ol] != 0)
				{
					m_aHole[l] = 1;
					any = true;
				}
				float undug = cg.m_aU[ol];
				m_aU[ol] = undug;
				m_aG[ol] = cg.m_aG[ol];
				if (!org && undug != 0)
				{
					m_fOrgY = undug;
					org = true;
				}
			}
		}
		if (t.m_Ent)
			m_fOrgY = t.m_Ent.GetOrigin()[1];     // the entity stays where it was spawned
		for (int c = 0; c < PW * PW; c++)
		{
			m_aDist[c] = 100;
			if (m_aHole[c] != 0)
				m_aDist[c] = 0;
		}
		if (!any)
			return false;
		// two chamfer sweeps (1 / 1.4)
		for (int sw = 0; sw < 2; sw++)
		{
			for (int kk = 0; kk < PW; kk++)
			{
				int k = kk;
				if (sw == 1)
					k = PW - 1 - kk;
				for (int ii = 0; ii < PW; ii++)
				{
					int i = ii;
					if (sw == 1)
						i = PW - 1 - ii;
					int s = 1;
					if (sw == 1)
						s = -1;
					int l2 = k * PW + i;
					float best = m_aDist[l2];
					if (i - s >= 0 && i - s < PW)
						best = Math.Min(best, m_aDist[l2 - s] + 1);
					if (k - s >= 0 && k - s < PW)
					{
						best = Math.Min(best, m_aDist[l2 - s * PW] + 1);
						if (i - s >= 0 && i - s < PW)
							best = Math.Min(best, m_aDist[l2 - s * PW - s] + 1.4);
						if (i + s >= 0 && i + s < PW)
							best = Math.Min(best, m_aDist[l2 - s * PW + s] + 1.4);
					}
					m_aDist[l2] = best;
				}
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Cell value of tile vertex li, lk (0..TQ; TQ reads the east / north neighbour): d, drag, stamped
	protected float m_fVd;
	protected float m_fVu;
	protected float m_fVv;
	protected int m_iVs;
	protected void VertData(MUD_OvTile t, int li, int lk)
	{
		m_fVd = THICK;
		m_fVu = 0;
		m_fVv = 0;
		m_iVs = 0;
		if (!CellTile(t.m_iTx * TQ + li, t.m_iTz * TQ + lk, false))
			return;
		m_fVd = m_Cl.m_aD[m_iCl];
		m_fVu = m_Cl.m_aOu[m_iCl];
		m_fVv = m_Cl.m_aOv[m_iCl];
		m_iVs = m_Cl.m_aSt[m_iCl];
	}

	//------------------------------------------------------------------------------------------------
	//! Per vertex of tile t: drawn height (m_aVy), flags (m_aVf: 1 drawn = over the felt floor or film, 2 dirt,
	//! 4 stamped, 8 hole) and uv (m_aSuv). Height rule h = max(undug + (d - THICK), dug), ridges tapered to holes.
	protected float m_aVy[441];
	protected int m_aVf[441];
	protected float m_aSuv[882];
	protected void Verts(MUD_OvTile t, bool anyHole)
	{
		int um = ((t.m_iTx % UV_TILES) + UV_TILES) % UV_TILES;
		int vm = ((t.m_iTz % UV_TILES) + UV_TILES) % UV_TILES;
		for (int l = 0; l < TV * TV; l++)
		{
			int li = l % TV;
			int lk = l / TV;
			VertData(t, li, lk);
			int fl = 0;
			if (m_iVs != 0)
				fl = fl | 4;
			int hl = (lk + PAD) * PW + li + PAD;
			m_aSuv[l * 2] = Math.Clamp(((um * TQ + li) * CELL - m_fVu) / SIZE, 0, 1);
			m_aSuv[l * 2 + 1] = 1 - Math.Clamp(((vm * TQ + lk) * CELL - m_fVv) / SIZE, 0, 1);
			if (m_aHole[hl] != 0)
			{
				m_aVf[l] = fl | 8;
				m_aVy[l] = m_aG[l];
				continue;
			}
			float ex = m_fVd - THICK;
			if (anyHole && ex > 0)
			{
				float tf = Math.Clamp(m_aDist[hl] / TAPER_CELLS, 0, 1);
				ex = ex * Math.Sin(0.5 * Math.PI * tf);     // sine shoulder toward the hole (MUD_MudSheet TAPER_CURVE)
			}
			float u = m_aU[l];
			float g = m_aG[l];
			float h = u + ex;
			bool film = m_iVs != 0 && m_fVd < THICK - TOUCH && u - g < FILM_CUT;
			if (film)
				h = Math.Max(h, u + FILM);
			if (h < g)
				h = g;
			m_aVy[l] = h;
			if (film || h > g + EPS_DRAW)
				fl = fl | 1;
			// dirt score (MUD_MudSheet.DirtQuad per vertex): changed more than TOUCH + noise, or dragged, or film
			int hsh = (t.m_iTx * TQ + li) * 73856093 + (t.m_iTz * TQ + lk) * 19349663;
			hsh = hsh + hsh / 8192;
			int hm = hsh % 1024;
			if (hm < 0)
				hm += 1024;
			float nz = hm / 1023.0;
			float dl = DRAG_TOUCH + DRAG_TOUCH * nz;
			if (film || Math.AbsFloat(m_fVd - THICK) > TOUCH + DIRT_NOISE * nz || m_fVu * m_fVu + m_fVv * m_fVv > dl * dl)
				fl = fl | 2;
			m_aVf[l] = fl;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One fresh mesh for tile t (MUD_MudSheet.BuildTileMesh pieces: two sections, MAT_TOP untouched look and MAT
	//! dirt, distinct emats; indices before verts; every index < the vertex count; local static buffers)
	protected void Build(MUD_OvTile t, float time)
	{
		t.m_fBuiltAt = time;
		t.m_iDirty = 0;
		m_iBuilds++;
		int tG = System.GetTickCount();
		bool anyHole = Grounds(t);
		m_iMsGround += System.GetTickCount(tG);
		if (m_iNeedN == 0)
		{
			if (t.m_Ent)
				t.m_Ent.ClearFlags(EntityFlags.VISIBLE, false);
			t.m_bMesh = true;
			return;
		}
		Verts(t, anyHole);
		float orgY = m_fOrgY;
		vector tv[441];
		float tuv[882];
		int tix[2400];
		int tixTop[2400];
		int tmap[441];
		for (int l = 0; l < TV * TV; l++)
		{
			tmap[l] = -1;
		}
		int nv = 0;
		int ni = 0;
		int niTop = 0;
		for (int qk = 0; qk < TQ; qk++)
		{
			for (int qi = 0; qi < TQ; qi++)
			{
				int la = qk * TV + qi;
				int lb = la + 1;
				int lc = la + TV;
				int ld = lc + 1;
				int all = m_aVf[la] | m_aVf[lb] | m_aVf[lc] | m_aVf[ld];
				if ((all & 8) != 0 || (all & 4) == 0 || (all & 1) == 0)
					continue;     // touches a hole / nothing stamped / all four on the felt floor
				for (int c = 0; c < 4; c++)
				{
					int lv = la;
					if (c == 1)
						lv = lb;
					else if (c == 2)
						lv = lc;
					else if (c == 3)
						lv = ld;
					if (tmap[lv] >= 0)
						continue;
					int ci = lv % TV;
					int ck = lv / TV;
					tmap[lv] = nv;
					tv[nv] = Vector(ci * CELL, m_aVy[lv] - orgY, ck * CELL);
					tuv[nv * 2] = m_aSuv[lv * 2];
					tuv[nv * 2 + 1] = m_aSuv[lv * 2 + 1];
					nv++;
				}
				int ia = tmap[la];
				int ib = tmap[lb];
				int ic = tmap[lc];
				int id = tmap[ld];
				if ((all & 2) == 0)
				{
					tixTop[niTop] = ia;
					tixTop[niTop + 1] = ic;
					tixTop[niTop + 2] = id;
					tixTop[niTop + 3] = ia;
					tixTop[niTop + 4] = id;
					tixTop[niTop + 5] = ib;
					niTop += 6;
				}
				else
				{
					tix[ni] = ia;
					tix[ni + 1] = ic;
					tix[ni + 2] = id;
					tix[ni + 3] = ia;
					tix[ni + 4] = id;
					tix[ni + 5] = ib;
					ni += 6;
				}
			}
		}
		t.m_bMesh = true;     // counts as built (a tile with nothing to draw keeps its hidden entity)
		if (!t.m_Ent)
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[0] = vector.Right;
			params.Transform[1] = vector.Up;
			params.Transform[2] = vector.Forward;
			params.Transform[3] = Vector(t.m_iTx * TILE_M, orgY, t.m_iTz * TILE_M);
			t.m_Ent = GetGame().SpawnEntity(MUD_MudOverlayEntity, GetGame().GetWorld(), params);
			if (!t.m_Ent)
				return;
		}
		if (ni + niTop == 0)
		{
			t.m_Ent.ClearFlags(EntityFlags.VISIBLE, false);     // no SetObject(null) (MUD_MudSheet: crash after 16 of those)
			return;
		}
		t.m_Ent.SetFlags(EntityFlags.VISIBLE, false);
		Resource res;
		if (ni > 0 && niTop > 0)
		{
			int nvs2[] = {nv, nv};
			int nis2[] = {niTop, ni};
			string mats2[] = {MAT_TOP, MAT};
			res = MeshObject.Create(2, nvs2, nis2, mats2, 0);
		}
		else
		{
			int nvs[] = {nv};
			int nis[] = {ni + niTop};
			string mats[] = {MAT};
			if (niTop > 0)
				mats[0] = MAT_TOP;
			res = MeshObject.Create(1, nvs, nis, mats, 0);
		}
		if (!res || !res.GetResource())
		{
			Print("MUD mud overlay: MeshObject.Create failed", LogLevel.ERROR);
			return;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		// indices first: UpdateVerts rebuilds normals from the triangle list
		if (ni > 0 && niTop > 0)
		{
			mesh.UpdateIndices(0, tixTop);
			mesh.UpdateVerts(0, tv, tuv);
			mesh.UpdateIndices(1, tix);
			mesh.UpdateVerts(1, tv, tuv);
		}
		else if (niTop > 0)
		{
			mesh.UpdateIndices(0, tixTop);
			mesh.UpdateVerts(0, tv, tuv);
		}
		else
		{
			mesh.UpdateIndices(0, tix);
			mesh.UpdateVerts(0, tv, tuv);
		}
		t.m_Ent.SetObject(mesh, "");
		t.m_Ent.Update();
		t.m_Res = res;
	}
}

//! Entity of one overlay tile: own class (MUD_Surface.IsOurs must never take it for our ground), no collider
class MUD_MudOverlayEntityClass : GenericEntityClass
{
}

class MUD_MudOverlayEntity : GenericEntity
{
}

//! One overlay tile: 20 x 20 own cells (gi = tx * 20 + 0..19), the vertex row / column 20 is the neighbour's
class MUD_OvTile
{
	int m_iTx;
	int m_iTz;
	float m_aD[400];
	float m_aOu[400];
	float m_aOv[400];
	int m_aSt[400];             // 1 = a stamp reached the cell
	ref array<ref MUD_OvStamp> m_aStamps = {};   // live + fallback stamps reaching it
	bool m_bLive;               // a live stamp reached it
	bool m_bReplay;             // its stamps changed: field again from them
	int m_iDirty;               // 0 clean, 1 rebuild (other pace), 2 rebuild (wheel pace)
	bool m_bMesh;
	float m_fBuiltAt = -100;
	int m_iSig;                 // GroundSig at the last poll
	int m_iHoles;
	float m_fCam2;
	float m_fLiveAt = -100;     // last live stamp (time)
	float m_fReplayAt = -100;
	ref MUD_OvGround m_G;       // ground reads, kept until the next op (LAG FIX 2)
	IEntity m_Ent;
	ref Resource m_Res;

	void Init(int tx, int tz, float thick)
	{
		m_iTx = tx;
		m_iTz = tz;
		for (int c = 0; c < 400; c++)
		{
			m_aD[c] = thick;
			m_aOu[c] = 0;
			m_aOv[c] = 0;
			m_aSt[c] = 0;
		}
	}
}

//! Ground reads of one tile since op count m_iOps: own vertices (m_aKnown 1 = read) + the pad ring's voxel columns
class MUD_OvGround
{
	int m_iOps = -1;
	float m_aU[441];
	float m_aG[441];
	int m_aHole[441];
	int m_aKnown[441];
	int m_aPad[729];            // -1 = not read, else ColumnAt
}

//! One tyre step (live or fallback)
class MUD_OvStamp
{
	vector m_vA;
	vector m_vB;
	float m_fH;                 // ridge height (speed + sink at the live stamp)
	float m_fHalf;              // tyre half width before WIDEN
	float m_fT;                 // pass time (fallback: a fake time per pass)
	int m_iVeh;
	bool m_bFall;
	int m_iOwner;               // tile key: fallback = the tile whose scan made it
	ref array<int> m_aKeys = {};   // tiles holding it
}

//! GRADUAL pass detection: per cell the last stamp time, its vehicle, the mud depth when its pass began
class MUD_OvPass
{
	ref map<int, float> m_mT = new map<int, float>();
	ref map<int, int> m_mV = new map<int, int>();
	ref map<int, float> m_mD = new map<int, float>();
	float m_fPruneT = -1000;

	void Prune(float time, float gap)
	{
		if (time - m_fPruneT < 5)
			return;
		m_fPruneT = time;
		array<int> old = {};
		foreach (int pk, float pt : m_mT)
		{
			if (time - pt > gap)
				old.Insert(pk);
		}
		foreach (int ok : old)
		{
			m_mT.Remove(ok);
			m_mV.Remove(ok);
			m_mD.Remove(ok);
		}
	}
}

class MUD_OvVeh
{
	Vehicle m_Veh;
	ref array<vector> m_aLast = {};
	ref array<float> m_aTime = {};
	ref array<int> m_aHas = {};
	int m_iId;
}
