//! MUD LAB stage 2 part B1 (2026-10-20, TEST_KIT only): one record per crater op on a client / host: the local
//! warhead init, receive, submit, the voxel go-live and the sheet local tiles the crater reached - by class, and when
//! each one is built (and why, BW_*). Prints 'MUD crater seq ...' once every tracked tile is built, or 3 s after the
//! go-live. A DS prints one 'MUD crater srv' line per crater and keeps nothing else.
//! Every call comes from a single 'if (MUD_TestKit.TEST_KIT)' stamp; nothing here changes the ground or the sheet.
class MUD_CraterRec
{
	int m_iSeq;
	vector m_vWp;
	int m_iRecv;                    // tick ms at receive (host: at issue)
	int m_iAhead = -1;              // ops held ahead of it at receive (-1: host, no receive)
	int m_iBoom = -1;               // ms from the local warhead init to the receive (-1: none matched)
	int m_iSubmit = -1;             // tick ms at submit
	int m_iLive = -1;               // tick ms at the go-live start (-1: not live yet)
	int m_iLiveFr;                  // MUD_CraterTrace frame of the go-live
	int m_iFrames;                  // voxel batch frames
	int m_iWorst;                   // worst batch frame ms
	int m_iLiveMs;                  // go-live frame ms (all of GoLive)
	int m_iVoxMs;                   // ... of it UpdateVox
	int m_iSheetMs;                 // ... of it MUD_MudSheet GroundChanged + FlushNow
	int m_aCls[11];                 // tiles per class (MUD_CraterTrace.C_*)
	int m_iEases;                   // running eases on sheet cells DigNow wrote
	ref map<int, int> m_mTrack = new map<int, int>();      // tracked tile key -> class
	ref map<int, int> m_mMs = new map<int, int>();         // built tile key -> tick ms of its first build
	ref map<int, int> m_mFr = new map<int, int>();         // ... -> frame
	ref map<int, int> m_mWhy = new map<int, int>();        // ... -> BW_* reason
	ref array<int> m_aOrder = {};                          // tracked keys in order
}

class MUD_CraterTrace
{
	static const int C_LIVE_BUILT = 0;      // BuildNowSlots built the slot in the go-live frame
	static const int C_TRACED_NOW = 1;      // ... after CRATER_TRACE_NOW traced its missing cells
	static const int C_SKIP_UNTRACED = 2;   // BuildNowSlots skipped: not all cells traced
	static const int C_SKIP_UNBORN = 3;     // ... no mud zone in the slot
	static const int C_SKIP_NOENT = 4;      // ... zone, no tile entity
	static const int C_STORED = 5;          // BlastNow / DigNow: stored tile with a mesh (built in DigNow)
	static const int C_DATA = 6;            // ... data-only store (no mesh)
	static const int C_FAR = 7;             // ... compact far tile taken back first
	static const int C_LOW = 8;             // ... stored tile built at 11 x 11 (past FAR_R)
	static const int C_TAPER = 9;           // DigNow taper neighbour at the front of the stored queue
	static const int C_TAPER_NOW = 10;      // ... built by CRATER_TAPER_NOW in the go-live frame
	static const int DONE_MS = 3000;        // print at the latest this long after the go-live
	static const int LOST_MS = 10000;       // a record with no go-live this long after receive is dropped
	static const int BOOM_M = 2;            // local warhead init within this (x z) of the crater ...
	static const int BOOM_MS = 3000;        // ... and this long before the receive

	protected static ref array<ref MUD_CraterRec> s_aRecs = {};
	protected static MUD_CraterRec s_Cur;                 // the crater the sheet works on (GroundChanged .. go-live end)
	protected static ref array<vector> s_aBoomP = {};
	protected static ref array<int> s_aBoomT = {};
	protected static int s_iDs = -1;                     // 1 = dedicated server (keeps nothing)
	protected static int s_iFrame;
	protected static float s_fFrameWt = -1;
	protected static int s_iSheetMs;                     // sheet ms inside the running GoLive
	protected static vector s_vHit;                      // server: last crater hit (WorldGround, before Crater)
	protected static float s_fHitKg;
	protected static int s_iHitT = -100000;
	// 10 s part (sheet local lab line)
	protected static int s_iN;
	protected static int s_iSumRG;
	protected static int s_iMaxRG;
	protected static int s_iMaxLive;
	protected static int s_iSkip;
	protected static int s_iTaperLate;
	protected static int s_iEase;

	//------------------------------------------------------------------------------------------------
	protected static bool IsDs()
	{
		if (s_iDs < 0)
		{
			s_iDs = 0;
			if (RplSession.Mode() == RplMode.Dedicated)
				s_iDs = 1;
		}
		return s_iDs == 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Frame number: steps whenever the world time moved (any stamp), so the order of the callers in a frame does not matter
	protected static int Fr()
	{
		BaseWorld w = GetGame().GetWorld();
		if (w)
		{
			float wt = w.GetWorldTime();
			if (wt != s_fFrameWt)
			{
				s_fFrameWt = wt;
				s_iFrame++;
			}
		}
		return s_iFrame;
	}

	//------------------------------------------------------------------------------------------------
	protected static int Key(int tx, int tz)
	{
		return tx * 20000 + tz;     // = MUD_MudSheet.TrailKey
	}

	//------------------------------------------------------------------------------------------------
	protected static MUD_CraterRec Find(int seq)
	{
		foreach (MUD_CraterRec r : s_aRecs)
		{
			if (r.m_iSeq == seq)
				return r;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! New record at receive (client) / issue (host); matches the newest local warhead init near it
	protected static MUD_CraterRec Open(int seq, vector wp, int ahead)
	{
		MUD_CraterRec r = Find(seq);
		if (r)
			return r;
		r = new MUD_CraterRec();
		r.m_iSeq = seq;
		r.m_vWp = wp;
		r.m_iRecv = System.GetTickCount();
		r.m_iAhead = ahead;
		for (int b = s_aBoomP.Count() - 1; b >= 0; b--)
		{
			vector bp = s_aBoomP[b];
			float dx = bp[0] - wp[0];
			float dz = bp[2] - wp[2];
			int age = r.m_iRecv - s_aBoomT[b];
			if (dx * dx + dz * dz < BOOM_M * BOOM_M && age >= 0 && age < BOOM_MS)
			{
				r.m_iBoom = age;
				break;
			}
		}
		if (s_aRecs.Count() >= 64)
			s_aRecs.RemoveOrdered(0);     // no sheet frame ticking (sheet off): keep the list small
		s_aRecs.Insert(r);
		return r;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_WarheadComponent.EOnInit (client / host): a warhead went off here
	static void Boom(vector p)
	{
		if (IsDs())
			return;
		int now = System.GetTickCount();
		for (int i = s_aBoomT.Count() - 1; i >= 0; i--)
		{
			if (now - s_aBoomT[i] > BOOM_MS)
			{
				s_aBoomT.Remove(i);
				s_aBoomP.Remove(i);
			}
		}
		if (s_aBoomT.Count() >= 32)
			return;
		s_aBoomP.Insert(p);
		s_aBoomT.Insert(now);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_WorldGroundEntity.Blast (server / host): just before Crater(pos, kg)
	static void SrvHit(vector pos, float kg)
	{
		s_vHit = pos;
		s_fHitKg = kg;
		s_iHitT = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.Issue (server / host): op seq of kind went out. Crater: the server line; a host also opens its record.
	static void Issued(int seq, int kind, vector wp)
	{
		if (kind != 0)
			return;
		int hitMs = -1;
		float kg = -1;
		float hx = s_vHit[0] - wp[0];
		float hz = s_vHit[2] - wp[2];
		if (hx * hx + hz * hz < 9 && System.GetTickCount() - s_iHitT < 3000)
		{
			hitMs = System.GetTickCount() - s_iHitT;
			kg = s_fHitKg;
		}
		Print(string.Format("MUD crater srv: seq %1 at %2 kg %3 | hit->issue %4 ms", seq, wp, kg, hitMs), LogLevel.NORMAL);
		if (!IsDs())
			Open(seq, wp, -1);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.Receive (MP client): op text held; ahead = ops held now (this one included)
	static void Recv(string text, int ahead)
	{
		array<string> f = {};
		text.Split("|", f, false);
		if (f.Count() < 5 || f[1] != "0")
			return;
		vector wp = Vector(f[2].ToInt() * 0.01, f[3].ToInt() * 0.01, f[4].ToInt() * 0.01);
		Open(f[0].ToInt(), wp, ahead - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.SubmitBody (client / host): op seq starts on this machine
	static void Submitted(int seq, int kind)
	{
		if (kind != 0 || IsDs())
			return;
		MUD_CraterRec r = Find(seq);
		if (r && r.m_iSubmit < 0)
			r.m_iSubmit = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_MudSheet.GroundChanged with a blast (inside GoLive): the sheet now works on the crater at wp
	static void SheetHit(vector wp)
	{
		s_Cur = null;
		float best = 1;
		foreach (MUD_CraterRec r : s_aRecs)
		{
			if (r.m_iLive >= 0 || r.m_iSubmit < 0)
				continue;
			float dx = r.m_vWp[0] - wp[0];
			float dz = r.m_vWp[2] - wp[2];
			float dd = dx * dx + dz * dz;
			if (dd < best)
			{
				best = dd;
				s_Cur = r;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_MudSheet GroundChanged / FlushNow inside GoLive: their ms (the go-live frame split)
	static void SheetMs(int ms)
	{
		s_iSheetMs += ms;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.GoLive end (client / host): every submitted crater not live yet went live in this frame
	static void GoLive(int frames, int worst, int liveMs, int voxMs)
	{
		if (IsDs())
			return;
		int now = System.GetTickCount();
		int fr = Fr();
		foreach (MUD_CraterRec r : s_aRecs)
		{
			if (r.m_iLive >= 0 || r.m_iSubmit < 0)
				continue;
			r.m_iLive = now - liveMs;
			r.m_iLiveFr = fr;
			r.m_iFrames = frames;
			r.m_iWorst = worst;
			r.m_iLiveMs = liveMs;
			r.m_iVoxMs = voxMs;
			r.m_iSheetMs = s_iSheetMs;
		}
		s_iSheetMs = 0;
		s_Cur = null;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_MudSheet: tile tx, tz of class cls for the crater the sheet works on (track: wait for its build)
	static void Tile(int cls, int tx, int tz, bool track)
	{
		if (!s_Cur)
			return;
		s_Cur.m_aCls[cls] = s_Cur.m_aCls[cls] + 1;
		if (!track)
			return;
		int key = Key(tx, tz);
		if (s_Cur.m_mTrack.Contains(key))
			return;
		s_Cur.m_mTrack.Set(key, cls);
		s_Cur.m_aOrder.Insert(key);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_MudSheet.DigNow: a cell it wrote had a running ease
	static void EaseHit()
	{
		if (s_Cur)
			s_Cur.m_iEases++;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_MudSheet.BuildTile (every live / stored build): first build of a tracked tile
	static void Built(int tx, int tz, int why, bool mesh)
	{
		if (s_aRecs.IsEmpty())
			return;
		int key = Key(tx, tz);
		foreach (MUD_CraterRec r : s_aRecs)
		{
			if (!r.m_mTrack.Contains(key) || r.m_mMs.Contains(key))
				continue;
			r.m_mMs.Set(key, System.GetTickCount());
			r.m_mFr.Set(key, Fr());
			int w = why;
			if (!mesh)
				w = -1 - why;     // built, no mesh came out
			r.m_mWhy.Set(key, w);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_MudSheet.Frame (client), every frame: prints and drops finished records
	static void Tick()
	{
		Fr();
		if (s_aRecs.IsEmpty())
			return;
		int now = System.GetTickCount();
		for (int i = s_aRecs.Count() - 1; i >= 0; i--)
		{
			MUD_CraterRec r = s_aRecs[i];
			if (r == s_Cur)
				continue;
			if (r.m_iLive < 0)
			{
				if (now - r.m_iRecv > LOST_MS)
				{
					Print(string.Format("MUD crater seq %1: no go-live %2 ms after receive (submitted %3)", r.m_iSeq, LOST_MS, r.m_iSubmit >= 0), LogLevel.NORMAL);
					s_aRecs.Remove(i);
				}
				continue;
			}
			if (now - r.m_iLive < DONE_MS && r.m_mMs.Count() < r.m_mTrack.Count())
				continue;
			Report(r);
			s_aRecs.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Report(MUD_CraterRec r)
	{
		string boom = "none";
		if (r.m_iBoom >= 0)
			boom = string.Format("-%1", r.m_iBoom);
		int sub = -1;
		if (r.m_iSubmit >= 0)
			sub = r.m_iSubmit - r.m_iRecv;
		int rg = r.m_iLive - r.m_iRecv;
		int swap = r.m_iLiveMs - r.m_iVoxMs - r.m_iSheetMs;
		// skipped live slots: their last build after the go-live (Upload)
		int upLast = -1;
		int upOpen = 0;
		int tapN = 0;
		int tapSame = 0;
		int tapLast = -1;
		int tapOpen = 0;
		string list = "";
		int listed = 0;
		foreach (int key : r.m_aOrder)
		{
			int cls = r.m_mTrack.Get(key);
			bool built = r.m_mMs.Contains(key);
			int bms = -1;
			int bfr = -1;
			if (built)
			{
				bms = r.m_mMs.Get(key) - r.m_iLive;
				bfr = r.m_mFr.Get(key) - r.m_iLiveFr;
			}
			if (cls == C_SKIP_UNTRACED || cls == C_SKIP_NOENT)
			{
				if (!built)
					upOpen++;
				else if (bms > upLast)
					upLast = bms;
			}
			if (cls == C_TAPER || cls == C_TAPER_NOW)
			{
				tapN++;
				if (!built)
					tapOpen++;
				else if (bfr <= 0)
					tapSame++;
				else if (bfr > tapLast)
					tapLast = bfr;
			}
			if (listed < 16)
			{
				int tx = Math.Round(key / 20000.0);
				int tz = key - tx * 20000;
				if (built)
					list += string.Format(" %1,%2 c%3 +%4ms +%5fr why %6;", tx, tz, cls, bms, bfr, r.m_mWhy.Get(key));
				else
					list += string.Format(" %1,%2 c%3 not built;", tx, tz, cls);
				listed++;
			}
		}
		int skipped = r.m_aCls[C_SKIP_UNTRACED] + r.m_aCls[C_SKIP_UNBORN] + r.m_aCls[C_SKIP_NOENT];
		string s1 = string.Format("MUD crater seq %1: boom(local) %2 | recv 0 (ahead %3) submit +%4 | golive +%5 (%6 fr, worst %7 ms; golive frame ms: %8 / %9 /",
			r.m_iSeq, boom, r.m_iAhead, sub, rg, r.m_iFrames, r.m_iWorst, swap, r.m_iVoxMs);
		string s2 = string.Format(" %1) | sheet: live now built %2 (traced now %3) / skipped %4 (%5 / %6 / %7) -> built +%8 by upload (%9 not built)",
			r.m_iSheetMs, r.m_aCls[C_LIVE_BUILT] + r.m_aCls[C_TRACED_NOW], r.m_aCls[C_TRACED_NOW], skipped, r.m_aCls[C_SKIP_UNTRACED], r.m_aCls[C_SKIP_UNBORN], r.m_aCls[C_SKIP_NOENT], upLast, upOpen);
		string s3 = string.Format(" | stored now %1, data-only %2, far-taken %3, low 11x11 %4 | taper front %5: same frame %6, last +%7 fr (%8 not built, %9 by taper now)",
			r.m_aCls[C_STORED], r.m_aCls[C_DATA], r.m_aCls[C_FAR], r.m_aCls[C_LOW], tapN, tapSame, tapLast, tapOpen, r.m_aCls[C_TAPER_NOW]);
		string s4 = string.Format(" | eases in box %1 | tiles (tx,tz class +ms +fr why):%2", r.m_iEases, list);
		Print(s1 + s2 + s3 + s4, LogLevel.NORMAL);
		s_iN++;
		s_iSumRG += rg;
		if (rg > s_iMaxRG)
			s_iMaxRG = rg;
		if (r.m_iLiveMs > s_iMaxLive)
			s_iMaxLive = r.m_iLiveMs;
		s_iSkip += skipped - r.m_aCls[C_SKIP_UNBORN];
		s_iTaperLate += tapN - tapSame;
		s_iEase += r.m_iEases;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_LabSheetLocal.Log10s: the 10 s crater part ("" without TEST_KIT or with no crater printed)
	static string Part10()
	{
		if (!MUD_TestKit.TEST_KIT || s_iN == 0)
			return "";
		string s = string.Format(" | craters %1: recv->golive %2/%3 ms, golive frame max %4 ms, now-slots skipped %5, taper late %6, eases hit %7",
			s_iN, s_iSumRG / s_iN, s_iMaxRG, s_iMaxLive, s_iSkip, s_iTaperLate, s_iEase);
		s_iN = 0;
		s_iSumRG = 0;
		s_iMaxRG = 0;
		s_iMaxLive = 0;
		s_iSkip = 0;
		s_iTaperLate = 0;
		s_iEase = 0;
		return s;
	}
}
