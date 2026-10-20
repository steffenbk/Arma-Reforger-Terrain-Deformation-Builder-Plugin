//! SYNCED MUD SHEET Stage 2 (user go 2026-10-17, op encoding OK 2026-10-18; docs/features/mud-sheet/README.md; v179 =
//! before): the SYNC layer - the mud sheet pressed ONLY from the replicated rut ops (kind 4), in op order, on every
//! machine that draws the sheet (not a DS), so all of them hold the same mud.
//! - MUD_VoxelProbe.Submit calls RutOp for each kind-4 op. The op's seed field carries the sheet tag (MakeTag, put on
//!   by the server in MUD_VoxelProbe.DoRutOp): vehicle number, speed (0.1 m/s), tyre radius (cm), server time (0.1 s).
//! - Per op segment one MUD_MudSheet.Stamp (BOX + SOFT + GRADUAL maths, the sheet's constants; keep both in step),
//!   then SLUMP_PER_OP slump iterations over the box the op changed, on the data ground (GroundY: no traces).
//! - Pass rule from the op: a cell starts a new pass when another vehicle number touches it, or the same one more than
//!   PASS_GAP_T (op time) after its last stamp there.
//! - Storage: sparse world tiles of TQ x TQ cells (no shared edges), made only on zone tiles a stamp or slump reaches.
//! MUD_MudSheet draws this layer + its own per-frame stamps at the tyres it sees (prediction), which it drops as the
//! ops holding them arrive (MUD_MudSheet.SyncBegin / SyncReplay). Proto: Tools/mud_sheet_sync_proto.py.
//! Log: 'MUD sheet hash seq N: H' per op that reached a zone (Tools/compare_op_hashes.py, client A vs client B).
//! Stage 4 OP CLEARING (2026-10-18; v183 = before): crater / tunnel / trench / fill ops (kinds 0-3) clear the mud
//! here too, in op order: a crater bares every cell MUD_MudSheet.BlastKeep takes (pure maths from the op), a dig
//! bares the cells whose data ground moved more than MUD_MudSheet.MOVED (ClearBefore reads the ground in Submit just
//! before the op is applied, the clear job reads it again after). Bare cells (m_aB) are read by the drawn sheet when
//! it builds a tile / restores a box, so a dig past the camera radius is kept and every client has the same holes.
//! Stage 5 LATE JOINERS (2026-10-18; v185 = before): the server keeps this layer too (a DS as well: SERVER_LAYER,
//! DsTick), and the J2 snapshot (MUD_VoxelProbe.MakeSnap) carries it as "M|" parts (SnapParts). A joiner skips the
//! rut ops <= S (placeholders), so it loads the layer at S instead (LoadSnap, from MUD_VoxelProbe.LoadSnap) and
//! goes on with the ops > S. Every op ends with JOB_QUANT (floats to QUANT_F steps), so the text holds the exact
//! values and later ops give the same mud on the joiner.
class MUD_MudSync
{
	static const bool SERVER_LAYER = true;      // Stage 5: a DS keeps the layer (for the snapshot); false = joiners on a DS get no sheet history
	static const float QUANT_F = 100000.0;      // Stage 5: every op ends with its box's floats on 0.01 mm steps (exact in the snapshot text)
	static const int SNAP_CHUNK = 100;          // Stage 5: cells per snapshot item (one array of one tile)
	static const int SNAP_CHARS = 1500;         // Stage 5: snapshot part size (= MUD_VoxelProbe.SNAP_CHARS, a size already sent)
	static const int SLUMP_PER_OP = 3;          // slump iterations per op (proto: 3)
	static const int PASS_GAP_T = 40;           // MUD_MudSheet.PASS_GAP (4 s) in op time units (0.1 s)
	static const int BOX_MAX = 120;             // cells a side: a bigger changed box keeps its stamps but gets no slump
	static const int HASH_M = 50000017;         // hash modulus (h x 31 + q stays inside an int)
	static const int TN = 400;                  // MUD_MudSheet.TQ squared: cells per sync tile
	static const int NEVER = -1000000;          // m_aPT of a cell no stamp touched
	static const int CLEAR_MAX = 400;           // cells a side: a bigger crater box clears nothing (WARNING, every machine; a dig: BOX_MAX)
	// SHEET COST switches (research 2026-10-19 sec 15.5, prompts/2026-10-19-local-sheet-cost-followup-v2.md Part A;
	// v239 = before). BUILD switches: they change the synced maths, so the DS and every client need the same build
	// (hash lines re-baseline). Offline reference: Tools/mud_sheet_cost_exp.py.
	// A1 SLUMP_ONE: the op's slump runs only on the cells within 1 of its stamped cells (4 neighbours), 1 pass at
	// SLUMP_ONE_K (the local sheet slump keeps MUD_MudSheet.SLUMP); ground read only on the even cells they use.
	static const bool SLUMP_ONE = false;
	static const float SLUMP_ONE_K = 0.6;       // sync slump factor (before: MUD_MudSheet.SLUMP 0.25 x 3 passes)
	static const int SLUMP_ONE_PASSES = 1;
	// A2 DRAG_STRIP: each stamp's drag limit walks only the cells the stamp's strip reached + 1 ring (in row order,
	// then reversed), not the stamp's whole box
	static const bool DRAG_STRIP = false;
	// TEST KIT self-test (Part A step 7): once, this many rut ops after start, one op runs through the old and the new
	// A1 / A2 paths on copies of its tiles ('MUD sheet cost self-test:' line)
	static const int SELF_TEST_AFTER = 20;

	protected static ref MUD_MudSync s_Inst;
	protected static int s_iSnapTo;             // Stage 5 joiner: ops <= this come in the snapshot layer - no work for them here
	protected ref map<int, ref MUD_MudSyncTile> m_mTiles = new map<int, ref MUD_MudSyncTile>();
	protected ref map<int, int> m_mZoneTile = new map<int, int>();   // tile key -> 1 = zone tile, 0 = not (cache)
	protected ref array<MUD_MudZoneEntity> m_aZones = {};
	protected int m_iLastKey = -1;              // TileOf cache
	protected MUD_MudSyncTile m_LastTile;
	// world cells the current op changed (stamps), empty = m_iBi1 < m_iBi0
	protected int m_iBi0;
	protected int m_iBi1 = -1;
	protected int m_iBk0;
	protected int m_iBk1 = -1;
	// since the last StatLine
	protected int m_iStatOps;
	protected int m_iStatStamps;
	protected int m_iStatMs;
	protected int m_iStatMaxMs;
	protected int m_iStatNoTag;
	protected int m_iStatBig;
	// COST split (2026-10-18, user: big fps drop with Stage 2; WB log 13-35 ms per op): ms per phase + exact counts
	protected int m_iMsStamp;
	protected int m_iMsSlump;
	protected int m_iMsHash;
	protected int m_iMsSheet;
	protected int m_iStatGround;                // GroundY calls
	protected int m_iStatSlumpCells;
	protected int m_iStatQueueMax;
	// STAMP split (2026-10-18, WB 11:00: stamps still ~1 ms each after perf fix 4; v189 = before): ms of each part of
	// Stamp (sums of 1 ms ticks - read over many stamps) + cells walked / stamped
	protected int m_iMsStSide;                  // widen read + the two side GroundY reads (sink)
	protected int m_iMsStCells;                 // the cell loop
	protected int m_iMsStDrag;                  // LimitDrag
	protected int m_iStatStWalk;                // cells the loop walked
	protected int m_iStatStHit;                 // cells stamped
	protected int m_iStatDrainFrames;           // frames that ran at least one op
	protected int m_iStatDrainMaxMs;            // most ms one frame spent on ops
	// ops waiting, in seq order (Drain works them off DRAIN_MS a frame; the order is all that must match)
	protected ref array<ref MUD_VoxelOp> m_aQueue = {};
	static const int DRAIN_MS = 3;
	// OP SPLIT (user 2026-10-18, WB run 2: one op = one 8-22 ms frame): an op is worked in small steps (one stamp, one
	// ground row, one slump pass, ...) across frames. Flush (MUD_VoxelProbe.Submit) ends the open op before the next
	// op changes the ground, so ground reads stay the same on every machine.
	static const int JOB_STAMP = 1;     // one op segment per step
	static const int JOB_GROUND = 2;    // one row of the even-cell ground grid per step
	static const int JOB_CELLS = 3;     // slump grid from the tiles
	static const int JOB_PASS = 4;      // one slump iteration per step
	static const int JOB_WRITE = 5;     // slump result back into the tiles
	static const int JOB_HASH = 6;      // test kit hash line
	static const int JOB_SHEET = 7;     // drawn sheet: take own stamps, restore the box (MUD_MudSheet.SyncBegin)
	static const int JOB_REPLAY = 8;    // drawn sheet: own stamps again, a chunk per step (MUD_MudSheet.SyncReplay)
	static const int JOB_CLEAR = 9;     // Stage 4: one box row of a crater / dig op's clearing per step
	static const int JOB_QUANT = 10;    // Stage 5: one box row of floats to QUANT_F steps per step (before the hash)
	protected ref MUD_VoxelOp m_Job;    // op being worked, null = none
	protected bool m_bJobClear;         // Stage 4: the open op is a crater / dig (JOB_CLEAR, hash, end)
	protected float m_fJobR;            // ... crater crest radius (MUD_VoxelProbe.CraterCrest), 0 for a dig
	// Stage 4: the dig op whose column tops the voxel probe kept (ClearBefore; MUD_VoxelProbe.TopsMoved / TopWasAt)
	protected ref MUD_VoxelOp m_CbOp;
	protected int m_iStatClears;        // crater / dig ops cleared since the last StatLine
	protected int m_iStatBared;         // cells they bared
	protected int m_iMsClear;
	protected int m_iJobStep;
	protected int m_iJobI;              // index inside the step kind (segment, ground row, slump pass)
	protected int m_iJobEnd;            // set by a step: 1 = op ends (nothing landed), 2 = op ends (counted)
	protected int m_iJobMs;             // ms this op took so far (all its steps)
	protected int m_iJobVeh;
	protected float m_fJobSpeed;
	protected float m_fJobHalf;
	protected int m_iJobTime;
	protected bool m_bJobBig;           // box > BOX_MAX: no slump, hash -1
	protected int m_iJi0;               // the op's box + 1 cell (slump, hash, sheet restore)
	protected int m_iJi1;
	protected int m_iJk0;
	protected int m_iJk1;
	// slump state between steps (see Slump* below)
	protected ref array<float> m_aEg = {};      // ground on the even-cell grid
	protected ref array<int> m_aEgOk = {};
	protected ref array<float> m_aSg = {};      // per box cell: ground, mud, flow, usable
	protected ref array<float> m_aSd = {};
	protected ref array<float> m_aSdd = {};
	protected ref array<int> m_aSok = {};
	protected int m_iEi0;
	protected int m_iEk0;
	protected int m_iEw;
	protected int m_iEh;
	protected int m_iStatFlush;         // Flush calls that had work (a burst / J1: those ops ran in one frame)
	protected int m_iStatFlushMs;
	protected int m_iSt10Stamps;           // stamps since the sheet's 10 s Stage 3 line (TakeStamps10)
	protected int m_iDsLogAt;              // Stage 5, DS: last 'MUD mud sheet sync (server)' line
	// SHEET COST A1 / A2: per instance (= the build switches; only a self-test copy differs)
	protected bool m_bA1;
	protected bool m_bA2;
	protected ref array<int> m_aStI = {};       // A1, open op: stamped cells, gi (duplicates fine)
	protected ref array<int> m_aStK = {};       //                         gk
	protected ref array<int> m_aSmSeed = {};    // A1, per box cell: stamped
	protected ref array<int> m_aSm = {};        // A1, per box cell: in the mask (seed dilated by 1)
	protected ref array<int> m_aSmList = {};    // A1, box cell indices of the mask, row order
	protected ref array<int> m_aEgNeed = {};    // A1, per even cell: a mask cell reads it
	protected ref array<int> m_aDm = {};        // A2, per cell of the stamp box + 1: 1 = passed the stamp's strip test
	protected ref array<int> m_aDl = {};        // A2, marked cells + ring, as m_iDw-box indices, row order
	protected int m_iDw;
	protected int m_iDh;
	protected int m_iStatDragCells;             // cells the drag limit visited (both paths)
	protected int m_iStatSlumpMoved;            // cells whose depth the slump changed (both paths)
	// self-test (TEST_KIT)
	protected bool m_bTest;                     // a self-test copy: no hash line, no sheet restore, no lift
	protected int m_iSelfTestN;
	protected static bool s_bSelfTestDone;

	//------------------------------------------------------------------------------------------------
	void MUD_MudSync()
	{
		m_bA1 = SLUMP_ONE;
		m_bA2 = DRAG_STRIP;
	}

	//------------------------------------------------------------------------------------------------
	//! Sheet cost switch states for the 'MUD prof 5 s' and 'MUD mud sheet sync' lines
	static string SwitchTag()
	{
		return string.Format(" | sheet cost switches: A1 slump one %1, A2 drag strip %2, A3 server layer (DS) %3, A5 stored render gate %4",
			SLUMP_ONE, DRAG_STRIP, MUD_TestKit.ServerLayer(), MUD_MudSheet.STORED_RENDERED_GATE);
	}

	//------------------------------------------------------------------------------------------------
	//! Does this machine keep the layer? Every machine that draws the sheet; Stage 5: a DS too (SERVER_LAYER)
	protected static bool OnHere()
	{
		if (!MUD_MudSheet.ENABLED || !MUD_MudSheet.SYNC)
			return false;
		if (RplSession.Mode() == RplMode.Dedicated)
			return MUD_TestKit.ServerLayer();     // A3: SERVER_LAYER, or the DS test kit file's server_layer
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server (MUD_VoxelProbe.DoRutOp): the sheet tag of a rut op. "m<vehicle>,<speed 0.1 m/s>,<radius cm>,<time 0.1 s>"
	static string MakeTag(int veh, float speed, float radius, int time)
	{
		int sp = Math.Round(speed * 10);
		int rc = Math.Round(radius * 100);
		return string.Format("m%1,%2,%3,%4", veh, sp, rc, time);
	}

	//------------------------------------------------------------------------------------------------
	//! MakeTag back. False = no tag (an op from before Stage 2) or malformed.
	static bool ParseTag(string tag, out int veh, out float speed, out float radius, out int time)
	{
		if (tag.Length() < 8 || tag.Substring(0, 1) != "m")
			return false;
		array<string> f = {};
		tag.Substring(1, tag.Length() - 1).Split(",", f, false);
		if (f.Count() != 4)
			return false;
		foreach (string fs : f)
		{
			if (!MUD_TrenchPlan.IsInt(fs))
				return false;
		}
		veh = f[0].ToInt();
		speed = f[1].ToInt() * 0.1;
		radius = f[2].ToInt() * 0.01;
		time = f[3].ToInt();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.Submit, every machine, in seq order: a rut op. No-op with the sheet off (a DS: SERVER_LAYER).
	static void RutOp(MUD_VoxelOp op)
	{
		if (MUD_MudSheet.ENABLED && !MUD_MudSheet.SYNC)
		{
			// MUD LAB part E: sheet local - the op's segments go straight into the sheet's tile data (client only)
			if (RplSession.Mode() != RplMode.Dedicated)
				MUD_MudSheet.OpStamps(op);
			return;
		}
		if (!OnHere())
			return;
		if (op.m_iKind != 4 || op.m_aSegA.IsEmpty() || op.m_iSeq <= s_iSnapTo)
			return;
		if (!s_Inst)
			s_Inst = new MUD_MudSync();
		s_Inst.m_aQueue.Insert(op);
		if (s_Inst.m_aQueue.Count() > s_Inst.m_iStatQueueMax)
			s_Inst.m_iStatQueueMax = s_Inst.m_aQueue.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 4, MUD_VoxelProbe.Submit just before a tunnel / trench / fill op (kinds 1-3) is applied. True = the op is
	//! near a mud zone: the voxel probe keeps each column top the op changes as it was before (KeepTop; fix 2026-10-18,
	//! WB: reading every sheet cell of the op box was ~7400 reads per trench piece - a piece op carries the whole
	//! trench's box). False with the sheet off (a DS: SERVER_LAYER).
	static bool ClearBefore(MUD_VoxelOp op)
	{
		if (!OnHere())
			return false;
		if (!op || op.m_iKind < 1 || op.m_iKind > 3 || op.m_iSeq <= s_iSnapTo)
			return false;
		if (!s_Inst)
			s_Inst = new MUD_MudSync();
		return s_Inst.DigBefore(op);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 4, MUD_VoxelProbe.Submit after a crater / tunnel / trench / fill op (kinds 0-3) was applied: its clearing
	//! joins the op queue (worked a row per step; Flush at the next Submit ends it before the ground changes again).
	static void ClearOp(MUD_VoxelOp op)
	{
		if (MUD_MudSheet.ENABLED && !MUD_MudSheet.SYNC)
		{
			// MUD LAB part E: sheet local - the sheet takes craters / digs at their go-live (GroundChanged); counted only
			if (RplSession.Mode() != RplMode.Dedicated)
				MUD_MudSheet.OpClear(op);
			return;
		}
		if (!OnHere())
			return;
		if (!op || op.m_iKind < 0 || op.m_iKind > 3 || op.m_iSeq <= s_iSnapTo)
			return;
		if (!s_Inst)
			s_Inst = new MUD_MudSync();
		if (op.m_iKind != 0 && s_Inst.m_CbOp != op)
			return;     // no tops kept for it (far from every zone)
		s_Inst.m_aQueue.Insert(op);
		if (s_Inst.m_aQueue.Count() > s_Inst.m_iStatQueueMax)
			s_Inst.m_iStatQueueMax = s_Inst.m_aQueue.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! ClearBefore: is the dig op near a mud zone? (then it is m_CbOp)
	protected bool DigBefore(MUD_VoxelOp op)
	{
		m_CbOp = null;
		if (m_aZones.IsEmpty())
			MUD_MudZoneEntity.GetInWorld(m_aZones);
		if (!BoxNearZone(op.m_vLo, op.m_vHi))
			return false;
		m_CbOp = op;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame (MUD_MudSheet.Tick): op steps in order until DRAIN_MS is used (at least one step a frame), so one
	//! op's work spreads over frames instead of one long frame
	static void Drain()
	{
		if (!s_Inst || (s_Inst.m_Job == null && s_Inst.m_aQueue.IsEmpty()))
			return;
		int t0 = System.GetTickCount();
		int n = 0;
		while ((s_Inst.m_Job != null || !s_Inst.m_aQueue.IsEmpty()) && (n == 0 || System.GetTickCount(t0) < DRAIN_MS))
		{
			s_Inst.Step();
			n++;
		}
		int ms = System.GetTickCount(t0);
		s_Inst.m_iStatDrainFrames++;
		if (ms > s_Inst.m_iStatDrainMaxMs)
			s_Inst.m_iStatDrainMaxMs = ms;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.Submit, before an op changes the ground: all open op work runs to its end now. An op's ground
	//! reads (sink, slump) must see the ground of its own seq on every machine - a later op's rut floor or dig must
	//! not leak in, and when that later op arrives differs per machine. Ops further apart than the work takes (one
	//! truck: ~4 ops a second) never flush; a burst (several trucks, a J1 replay) runs in one frame as before.
	static void Flush()
	{
		if (!s_Inst || (s_Inst.m_Job == null && s_Inst.m_aQueue.IsEmpty()))
			return;
		int t0 = System.GetTickCount();
		while (s_Inst.m_Job != null || !s_Inst.m_aQueue.IsEmpty())
			s_Inst.Step();
		s_Inst.m_iStatFlush++;
		s_Inst.m_iStatFlushMs += System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	//! New world (MUD_VoxelProbe.Clear): the layer starts empty
	static void Clear()
	{
		s_Inst = null;
		s_iSnapTo = 0;
		s_bSelfTestDone = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5 joiner (MUD_VoxelProbe.ReceiveSnapHead): a snapshot at seq s is coming - the crater / dig ops <= s it
	//! replays do no clearing here (the snapshot layer holds it; their hash lines would differ from the other machines)
	static void SnapComing(int s)
	{
		if (s > s_iSnapTo)
			s_iSnapTo = s;
	}

	//------------------------------------------------------------------------------------------------
	//! Sync mud at world cell gi, gk (x = gi * CELL). False = no sync tile there (plain layer: THICK, no drag).
	//! bare 1 = a crater / dig cleared the mud here (Stage 4; the drawn sheet's m_aBare).
	static bool Get(int gi, int gk, out float d, out float ou, out float ov, out int bare)
	{
		d = MUD_MudSheet.THICK;
		ou = 0;
		ov = 0;
		bare = 0;
		if (!s_Inst)
			return false;
		int l;
		MUD_MudSyncTile t = s_Inst.TileOf(gi, gk, false, l);
		if (!t)
			return false;
		d = t.m_aD[l];
		ou = t.m_aOu[l];
		ov = t.m_aOv[l];
		bare = t.m_aB[l];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! SPARSE SHEET A2: is there a sync tile at tile tx, tz (3 m, TrailKey grid)?
	static bool HasTile(int tx, int tz)
	{
		return s_Inst && s_Inst.m_mTiles.Contains(MUD_MudSheet.TrailKey(tx, tz));
	}

	//------------------------------------------------------------------------------------------------
	//! Any sync tile under sheet tile tx, tz? Its cells 0..TQ reach into the next tile (cell TQ = the next tile's
	//! cell 0), so tx..tx+1 x tz..tz+1. False = every Get there returns plain THICK mud (BuildZoneTile skips them).
	static bool AnyTileNear(int tx, int tz)
	{
		if (!s_Inst || s_Inst.m_mTiles.IsEmpty())
			return false;
		for (int dz = 0; dz <= 1; dz++)
		{
			for (int dx = 0; dx <= 1; dx++)
			{
				if (s_Inst.m_mTiles.Contains(MUD_MudSheet.TrailKey(tx + dx, tz + dz)))
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Synced sheet Stage 3 log: sync tiles touched so far (the only mud data kept for the whole zone)
	static int TileCount()
	{
		if (!s_Inst)
			return 0;
		return s_Inst.m_mTiles.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Synced sheet Stage 3 log: stamps since the last call (resets)
	static int TakeStamps10()
	{
		if (!s_Inst)
			return 0;
		int n = s_Inst.m_iSt10Stamps;
		s_Inst.m_iSt10Stamps = 0;
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! For the sheet's 5 s log line; resets the counts
	static string StatLine()
	{
		if (!s_Inst)
			return "sync layer: no op yet";
		MUD_MudSync s = s_Inst;
		string p1 = string.Format("sync layer: %1 ops, %2 stamps, %3 tiles, %4 ms (max %5 ms per op), %6 untagged, %7 too big to slump",
			s.m_iStatOps, s.m_iStatStamps, s.m_mTiles.Count(), s.m_iStatMs, s.m_iStatMaxMs, s.m_iStatNoTag, s.m_iStatBig);
		string p2 = string.Format(" | COST ms: stamps %1, slump %2 (%3 ground reads, %4 cells, %7 moved), quant + hash %5, sheet restore %6",
			s.m_iMsStamp, s.m_iMsSlump, s.m_iStatGround, s.m_iStatSlumpCells, s.m_iMsHash, s.m_iMsSheet, s.m_iStatSlumpMoved);
		s.m_iStatSlumpMoved = 0;
		string p3 = string.Format(" | queue max %1, now %2, %3 frames ran op steps, worst frame %4 ms, %5 flushes (%6 ms)",
			s.m_iStatQueueMax, s.m_aQueue.Count(), s.m_iStatDrainFrames, s.m_iStatDrainMaxMs, s.m_iStatFlush, s.m_iStatFlushMs);
		string p4 = string.Format(" | op clearing: %1 crater / dig ops, %2 cells bared, %3 ms", s.m_iStatClears, s.m_iStatBared, s.m_iMsClear);
		string p5 = string.Format(" | STAMP split ms: side reads %1, cells %2 (%3 walked, %4 stamped), drag limit %5 (%6 cells)",
			s.m_iMsStSide, s.m_iMsStCells, s.m_iStatStWalk, s.m_iStatStHit, s.m_iMsStDrag, s.m_iStatDragCells);
		string sw = SwitchTag();
		string line = p1 + p2 + p3 + p4 + p5 + sw;
		s.m_iStatDragCells = 0;
		s.m_iMsStSide = 0;
		s.m_iMsStCells = 0;
		s.m_iMsStDrag = 0;
		s.m_iStatStWalk = 0;
		s.m_iStatStHit = 0;
		s.m_iStatClears = 0;
		s.m_iStatBared = 0;
		s.m_iMsClear = 0;
		s.m_iStatFlush = 0;
		s.m_iStatFlushMs = 0;
		s.m_iMsStamp = 0;
		s.m_iMsSlump = 0;
		s.m_iMsHash = 0;
		s.m_iMsSheet = 0;
		s.m_iStatGround = 0;
		s.m_iStatSlumpCells = 0;
		s.m_iStatQueueMax = 0;
		s.m_iStatDrainFrames = 0;
		s.m_iStatDrainMaxMs = 0;
		s.m_iStatOps = 0;
		s.m_iStatStamps = 0;
		s.m_iStatMs = 0;
		s.m_iStatMaxMs = 0;
		s.m_iStatNoTag = 0;
		s.m_iStatBig = 0;
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! One small piece of op work (Drain, Flush): starts the next queued op, or runs the open op's current step
	protected void Step()
	{
		if (!m_Job)
		{
			StartJob();
			return;
		}
		int t0 = System.GetTickCount();
		int st = m_iJobStep;
		if (st == JOB_STAMP)
			StepStamp();
		else if (st == JOB_GROUND)
			SlumpGroundRow();
		else if (st == JOB_CELLS)
			SlumpCells();
		else if (st == JOB_PASS)
			SlumpPass();
		else if (st == JOB_WRITE)
			SlumpWrite();
		else if (st == JOB_HASH)
			StepHash();
		else if (st == JOB_SHEET)
			StepSheet();
		else if (st == JOB_CLEAR)
			StepClear();
		else if (st == JOB_QUANT)
			StepQuant();
		else
			StepReplay();
		int ms = System.GetTickCount(t0);
		m_iJobMs += ms;
		if (st == JOB_STAMP)
			m_iMsStamp += ms;
		else if (st == JOB_CLEAR)
			m_iMsClear += ms;
		else if (st <= JOB_WRITE)
			m_iMsSlump += ms;
		else if (st == JOB_HASH || st == JOB_QUANT)
			m_iMsHash += ms;
		else
			m_iMsSheet += ms;
		if (m_iJobEnd == 0)
			return;
		if (m_iJobEnd == 2 && !m_bJobClear)     // rut ops only (clears have their own count)
		{
			m_iStatOps++;
			m_iStatMs += m_iJobMs;
			if (m_iJobMs > m_iStatMaxMs)
				m_iStatMaxMs = m_iJobMs;
		}
		m_Job = null;
		m_iJobEnd = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The next queued op becomes the open op (untagged / far from every zone: dropped here)
	protected void StartJob()
	{
		MUD_VoxelOp op = m_aQueue[0];
		m_aQueue.RemoveOrdered(0);
		m_bJobClear = false;
		if (op.m_iKind != 4)
		{
			StartClear(op);
			return;
		}
		int veh;
		float speed;
		float radius;
		int time;
		if (!ParseTag(op.m_sSeed, veh, speed, radius, time))
		{
			m_iStatNoTag++;
			return;
		}
		if (m_aZones.IsEmpty())
			MUD_MudZoneEntity.GetInWorld(m_aZones);
		if (!BoxNearZone(op.m_vLo, op.m_vHi))
			return;
		m_Job = op;
		m_iJobStep = JOB_STAMP;
		m_iJobI = 0;
		m_iJobEnd = 0;
		m_iJobMs = 0;
		m_iJobVeh = veh;
		m_fJobSpeed = speed;
		m_fJobHalf = MUD_MudSheet.TyreHalf(radius);
		m_iJobTime = time;
		m_bJobBig = false;
		m_iBi0 = 0;
		m_iBi1 = -1;
		m_iBk0 = 0;
		m_iBk1 = -1;
		m_aStI.Clear();
		m_aStK.Clear();
		if (MUD_TestKit.TEST_KIT && !m_bTest && !s_bSelfTestDone)
			SelfTestMaybe(op);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 4: a crater / dig op becomes the open op (JOB_CLEAR). Crater box = its BlastKeep reach; dig box = the
	//! ClearBefore box. Far from every zone / too big: dropped here (the same on every machine).
	protected void StartClear(MUD_VoxelOp op)
	{
		float cell = MUD_MudSheet.CELL;
		m_fJobR = 0;
		if (op.m_iKind == 0)
		{
			m_fJobR = MUD_VoxelProbe.CraterCrest(op.m_fDepth);
			float reach = MUD_MudSheet.BlastReach(m_fJobR);
			vector wp = op.m_vWp;
			vector lo = Vector(wp[0] - reach, 0, wp[2] - reach);
			vector hi = Vector(wp[0] + reach, 0, wp[2] + reach);
			if (m_aZones.IsEmpty())
				MUD_MudZoneEntity.GetInWorld(m_aZones);
			if (!BoxNearZone(lo, hi))
				return;
			m_iJi0 = Math.Floor(lo[0] / cell);
			m_iJi1 = Math.Ceil(hi[0] / cell);
			m_iJk0 = Math.Floor(lo[2] / cell);
			m_iJk1 = Math.Ceil(hi[2] / cell);
			if (m_iJi1 - m_iJi0 + 1 > CLEAR_MAX || m_iJk1 - m_iJk0 + 1 > CLEAR_MAX)
			{
				Print(string.Format("MUD sheet sync: seq %1 crater reach %2 m > CLEAR_MAX %3 cells - mud not cleared", op.m_iSeq, reach, CLEAR_MAX), LogLevel.WARNING);
				return;
			}
		}
		else
		{
			if (op != m_CbOp)
				return;
			// only round the columns whose top moved (the voxel probe's kept tops are good until the next Submit,
			// which flushes this job first)
			float x0;
			float z0;
			float x1;
			float z1;
			if (!MUD_VoxelProbe.TopsMoved(x0, z0, x1, z1))
			{
				m_CbOp = null;
				return;
			}
			m_iJi0 = Math.Floor(x0 / cell);
			m_iJi1 = Math.Ceil(x1 / cell);
			m_iJk0 = Math.Floor(z0 / cell);
			m_iJk1 = Math.Ceil(z1 / cell);
			if (m_iJi1 - m_iJi0 + 1 > CLEAR_MAX || m_iJk1 - m_iJk0 + 1 > CLEAR_MAX)
			{
				Print(string.Format("MUD sheet sync: seq %1 dig moved %2 x %3 cells > CLEAR_MAX %4 - mud not cleared", op.m_iSeq, m_iJi1 - m_iJi0 + 1, m_iJk1 - m_iJk0 + 1, CLEAR_MAX), LogLevel.WARNING);
				m_CbOp = null;
				return;
			}
		}
		m_Job = op;
		m_bJobClear = true;
		m_iJobStep = JOB_CLEAR;
		m_iJobI = 0;
		m_iJobEnd = 0;
		m_iJobMs = 0;
		// a crater box can be big: hashed only up to BOX_MAX a side (else hash -1, as an over-big rut op)
		m_bJobBig = m_iJi1 - m_iJi0 + 1 > BOX_MAX || m_iJk1 - m_iJk0 + 1 > BOX_MAX;
	}

	//------------------------------------------------------------------------------------------------
	//! JOB_CLEAR: one row of the open crater / dig op's box. Crater: cells BlastKeep takes keep that share of their mud
	//! and go bare (as MUD_MudSheet.BlastNow). Dig: cells whose data ground moved more than MOVED lose their mud and go
	//! bare (as MUD_MudSheet.DigNow). Only zone tiles; a crater makes the tiles it needs.
	protected void StepClear()
	{
		float cell = MUD_MudSheet.CELL;
		int tq = MUD_MudSheet.TQ;
		int k = m_iJk0 + m_iJobI;
		bool crater = m_Job.m_iKind == 0;
		for (int i = m_iJi0; i <= m_iJi1; i++)
		{
			if (!ZoneTile(i / tq, k / tq))
				continue;
			float keep = 0;
			if (crater)
			{
				keep = MUD_MudSheet.BlastKeep(i * cell, k * cell, m_Job.m_vWp, m_fJobR);
				if (keep >= 1)
					continue;
			}
			else
			{
				// a dig only moves voxel columns: before = TopAt with the kept tops, after = the data ground now
				float was;
				if (!MUD_VoxelProbe.ColumnAt(i * cell, k * cell) || !MUD_VoxelProbe.TopWasAt(i * cell, k * cell, was))
					continue;
				bool ok;
				float y = GroundY(i * cell, k * cell, ok);
				if (!ok || Math.AbsFloat(y - was) <= MUD_MudSheet.MOVED)
					continue;
			}
			int l;
			MUD_MudSyncTile t = TileOf(i, k, true, l);
			if (!t)
				continue;
			t.m_aD[l] = t.m_aD[l] * keep;
			t.m_aB[l] = 1;
			m_iStatBared++;
		}
		m_iJobI++;
		if (k < m_iJk1)
			return;
		m_iStatClears++;
		m_CbOp = null;
		m_iJobStep = JOB_QUANT;
		m_iJobI = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! JOB_QUANT (Stage 5): one row of the op box, its floats to QUANT_F steps (a value at its array's default stays
	//! as it is). Every write of an op lies in this box, so every float in the layer is exact in the snapshot text.
	protected void StepQuant()
	{
		int k = m_iJk0 + m_iJobI;
		float thick = MUD_MudSheet.THICK;
		for (int i = m_iJi0; i <= m_iJi1; i++)
		{
			int l;
			MUD_MudSyncTile t = TileOf(i, k, false, l);
			if (!t)
				continue;
			t.m_aD[l] = Quant(t.m_aD[l], thick);
			t.m_aOu[l] = Quant(t.m_aOu[l], 0);
			t.m_aOv[l] = Quant(t.m_aOv[l], 0);
			t.m_aP[l] = Quant(t.m_aP[l], thick);
		}
		m_iJobI++;
		if (k >= m_iJk1)
			m_iJobStep = JOB_HASH;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5: v on QUANT_F steps (def = the array's default: left as it is, sent as a run)
	protected static float Quant(float v, float def)
	{
		if (v == def)
			return v;
		int q = Math.Round(v * QUANT_F);
		return FromQ(q);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5: the float of quant step q - the ONE expression for it (server Quant and joiner LoadSnap give the same float)
	protected static float FromQ(int q)
	{
		return q / QUANT_F;
	}

	//------------------------------------------------------------------------------------------------
	//! JOB_STAMP: the next op segment; after the last one, the op's box -> slump (or straight to the hash if too big)
	protected void StepStamp()
	{
		int segs = m_Job.m_aSegA.Count();
		if (m_iJobI < segs)
		{
			vector a = m_Job.m_aSegA[m_iJobI];
			vector b = m_Job.m_aSegB[m_iJobI];
			m_iJobI++;
			// as the sheet's Wheels: a wheel stamps only where it stands on a zone tile, and never across a jump
			if (!ZoneTileAtCell(Math.Round(b[0] / MUD_MudSheet.CELL), Math.Round(b[2] / MUD_MudSheet.CELL)))
				return;
			float len = Math.Sqrt((b[0] - a[0]) * (b[0] - a[0]) + (b[2] - a[2]) * (b[2] - a[2]));
			if (len < 0.01 || len >= 3)
				return;
			Stamp(a, b, m_fJobSpeed, m_iJobTime, m_iJobVeh, m_fJobHalf);
			m_iStatStamps++;
			m_iSt10Stamps++;
			return;
		}
		if (m_iBi1 < m_iBi0)
		{
			m_iJobEnd = 1;     // no stamp landed on a zone tile
			return;
		}
		m_iJi0 = m_iBi0 - 1;
		m_iJi1 = m_iBi1 + 1;
		m_iJk0 = m_iBk0 - 1;
		m_iJk1 = m_iBk1 + 1;
		int bw = m_iJi1 - m_iJi0 + 1;
		int bh = m_iJk1 - m_iJk0 + 1;
		if (bw > BOX_MAX || bh > BOX_MAX)
		{
			// too big: no slump, no hash (the same on every machine)
			m_bJobBig = true;
			m_iStatBig++;
			Print(string.Format("MUD sheet sync: seq %1 changed %2 x %3 cells > BOX_MAX %4 - no slump for it", m_Job.m_iSeq, bw, bh, BOX_MAX), LogLevel.WARNING);
			m_iJobStep = JOB_QUANT;
			m_iJobI = 0;
			return;
		}
		SlumpBegin();
		if (m_bA1)
			BuildSlumpMask();
		m_iJobStep = JOB_GROUND;
		m_iJobI = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! JOB_HASH: test kit, like the field hash: client A vs client B (Tools/compare_op_hashes.py); no hash work without it
	protected void StepHash()
	{
		m_iJobStep = JOB_SHEET;
		if (m_bJobClear || m_bTest)
			m_iJobEnd = 2;     // Stage 4: the drawn sheet clears at the op's go-live (MUD_MudSheet.GroundChanged); a self-test copy ends here
		if (!MUD_TestKit.TEST_KIT || m_bTest)
			return;
		int h = -1;
		if (!m_bJobBig)
			h = Hash(m_iJi0, m_iJi1, m_iJk0, m_iJk1);
		Print(string.Format("MUD sheet hash seq %1: %2 (box %3 x %4 cells at cell %5 %6)", m_Job.m_iSeq, h, m_iJi1 - m_iJi0 + 1, m_iJk1 - m_iJk0 + 1, m_iJi0, m_iJk0), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! JOB_SHEET: the drawn sheet drops the own stamps the op holds and restores the box; own stamps left -> JOB_REPLAY
	protected void StepSheet()
	{
		if (MUD_MudSheet.SyncBegin(m_iJi0, m_iJi1, m_iJk0, m_iJk1, m_Job.m_aSegA, m_Job.m_aSegB))
			m_iJobStep = JOB_REPLAY;
		else
			m_iJobEnd = 2;
	}

	//------------------------------------------------------------------------------------------------
	//! JOB_REPLAY: a chunk of own stamps again; the op ends with the last chunk
	protected void StepReplay()
	{
		if (!MUD_MudSheet.SyncReplay())
			m_iJobEnd = 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Does the x, z box lo..hi (+ 2 m: a stamp reaches past a segment) overlap a mud zone?
	protected bool BoxNearZone(vector lo, vector hi)
	{
		if (MUD_MudZoneEntity.AnyBox())
			return true;     // H3 ALL_OFFROAD: ZoneTile decides per tile
		foreach (MUD_MudZoneEntity z : m_aZones)
		{
			if (!z)
				continue;
			float x0;
			float z0;
			float x1;
			float z1;
			z.GetRect(x0, z0, x1, z1);
			if (hi[0] + 2 >= x0 && lo[0] - 2 <= x1 && hi[2] + 2 >= z0 && lo[2] - 2 <= z1)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Is world tile tx, tz a sheet zone tile (its middle in a zone, as MUD_MudSheet.TileInZone)? Cached.
	protected bool ZoneTile(int tx, int tz)
	{
		int key = MUD_MudSheet.TrailKey(tx, tz);
		int known;
		if (m_mZoneTile.Find(key, known))
			return known != 0;
		int inZone = 0;
		if (MUD_MudZoneEntity.TileCovered(tx, tz))     // placed zones, or H3 ALL_OFFROAD holed ground
			inZone = 1;
		m_mZoneTile.Set(key, inZone);
		return inZone != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! World cell gi, gk lies on a zone tile (tile = gi / TQ, as the sheet's ZoneAtCell; Arland cells are >= 0)
	protected bool ZoneTileAtCell(int gi, int gk)
	{
		return ZoneTile(gi / MUD_MudSheet.TQ, gk / MUD_MudSheet.TQ);
	}

	//------------------------------------------------------------------------------------------------
	//! Sync tile holding world cell gi, gk and the cell's index l in it. make: a missing zone tile is made (plain
	//! layer). Null = no tile (not made, or not a zone tile).
	protected MUD_MudSyncTile TileOf(int gi, int gk, bool make, out int l)
	{
		int tq = MUD_MudSheet.TQ;
		int tx = gi / tq;
		int tz = gk / tq;
		l = (gk - tz * tq) * tq + gi - tx * tq;
		int key = MUD_MudSheet.TrailKey(tx, tz);
		if (key == m_iLastKey && m_LastTile)
			return m_LastTile;
		MUD_MudSyncTile t = m_mTiles.Get(key);
		if (!t && make && ZoneTile(tx, tz))
		{
			t = new MUD_MudSyncTile();
			for (int c = 0; c < TN; c++)
			{
				t.m_aD[c] = MUD_MudSheet.THICK;
				t.m_aOu[c] = 0;
				t.m_aOv[c] = 0;
				t.m_aP[c] = MUD_MudSheet.THICK;
				t.m_aPT[c] = NEVER;
				t.m_aPV[c] = 0;
				t.m_aB[c] = 0;
			}
			m_mTiles.Set(key, t);
			if (!MUD_MudSheet.LIFT_BY_DRAWN && !m_bTest)
				MUD_WorldGroundEntity.SheetLiftDirty(tx * MUD_MudSheet.TILE_M, tz * MUD_MudSheet.TILE_M);     // A2: lift off there
		}
		if (t)
		{
			m_iLastKey = key;
			m_LastTile = t;
		}
		return t;
	}

	//------------------------------------------------------------------------------------------------
	//! Data ground at x, z, the same on every machine at the same op: the voxel top (no rut floor, no cache) on a voxel
	//! column, else the heightfield + rut floor + ridges (MUD_WorldGroundEntity.DugUndug). ok false = not known.
	protected float GroundY(float x, float z, out bool ok)
	{
		m_iStatGround++;
		ok = true;
		float y;
		if (MUD_VoxelProbe.ColumnAt(x, z) && MUD_VoxelProbe.TopAt(x, z, y))
			return y;
		float dug;
		float undug;
		if (MUD_WorldGroundEntity.DugUndug(x, z, dug, undug))
			return dug;
		ok = false;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_MudSheet.Stamp on the sync tiles (BOX + SOFT + GRADUAL path; keep in step with it). time = op time (0.1 s),
	//! veh = op vehicle number. Sink: the data ground beside the tyre against the op's contact height (b[1]).
	protected void Stamp(vector a, vector b, float speed, int time, int veh, float half)
	{
		int tS = System.GetTickCount();
		float cell = MUD_MudSheet.CELL;
		float thick = MUD_MudSheet.THICK;
		// WIDEN: how pressed the mud at the contact was when this pass began
		int wl;
		MUD_MudSyncTile wt = TileOf(Math.Round(b[0] / cell), Math.Round(b[2] / cell), false, wl);
		float wd = thick;
		if (wt)
		{
			wd = wt.m_aD[wl];
			if (wt.m_aPV[wl] == veh && time - wt.m_aPT[wl] <= PASS_GAP_T)
				wd = wt.m_aP[wl];
		}
		float pressed = Math.Clamp((thick - wd) / thick, 0, 1);
		half = half * (MUD_MudSheet.W_FIRST + (1 - MUD_MudSheet.W_FIRST) * pressed);
		float band = MUD_MudSheet.BAND * half / MUD_MudSheet.HALF;
		vector dir = Vector(b[0] - a[0], 0, b[2] - a[2]);
		float len = dir.Length();
		dir = dir * (1 / len);
		vector side = Vector(dir[2], 0, -dir[0]);
		float reach = half + band;
		float nose = len + MUD_MudSheet.NOSE;
		float far = nose + MUD_MudSheet.FRONT_LEN;
		float ex = a[0] + dir[0] * far;
		float ez = a[2] + dir[2] * far;
		int i0 = Math.Floor((Math.Min(a[0], ex) - reach) / cell);
		int i1 = Math.Ceil((Math.Max(a[0], ex) + reach) / cell);
		int k0 = Math.Floor((Math.Min(a[2], ez) - reach) / cell);
		int k1 = Math.Ceil((Math.Max(a[2], ez) + reach) / cell);
		// sink: the data ground just outside the ridge band against the tyre's contact height
		float sideSum = 0;
		int sideN = 0;
		for (int sd = -1; sd <= 1; sd += 2)
		{
			vector q = b + side * (sd * (reach + cell));
			bool gok;
			float gq = GroundY(q[0], q[2], gok);
			if (!gok)
				continue;
			sideSum += gq;
			sideN++;
		}
		float sink = 0;
		if (sideN > 0)
			sink = Math.Max(0, sideSum / sideN - b[1]);
		float h = Math.Min(MUD_MudSheet.HMAX, MUD_MudSheet.H0 + MUD_MudSheet.H_SPEED * speed + MUD_MudSheet.H_SINK * sink);
		float drag = MUD_MudSheet.DRAG * len;
		m_iMsStSide += System.GetTickCount(tS);
		int tC = System.GetTickCount();
		bool any = false;
		if (m_bA2)
			DragStripBegin(i1 - i0 + 3, k1 - k0 + 3);
		// sheet perf fix 4 (2026-10-18, MUD_Prof WB 10:48: stamps ~1 ms each; v188 = before): plain float maths instead of a
		// Vector + 2 vector.Dot per cell, and each row only walks the cells the stamp's strip can reach (+1 cell each side,
		// so float rounding never drops a cell). The per-cell test below is unchanged, so the same cells are stamped.
		float dx = dir[0];
		float dz = dir[2];
		float ax = a[0];
		float az = a[2];
		for (int k = k0; k <= k1; k++)
		{
			float pz = k * cell - az;
			// strip along dir: 0 <= t <= far, |lat| < reach, with t = px*dx + pz*dz and lat = px*dz - pz*dx
			float lo = -1000000.0;
			float hi = 1000000.0;
			float tz = pz * dz;
			if (Math.AbsFloat(dx) > 0.0001)
			{
				float ta = (0 - tz) / dx;
				float tb = (far - tz) / dx;
				lo = Math.Max(lo, Math.Min(ta, tb));
				hi = Math.Min(hi, Math.Max(ta, tb));
			}
			else if (tz < -cell || tz > far + cell)
				continue;
			float lz = -pz * dx;
			if (Math.AbsFloat(dz) > 0.0001)
			{
				float la = (-reach - lz) / dz;
				float lb = (reach - lz) / dz;
				lo = Math.Max(lo, Math.Min(la, lb));
				hi = Math.Min(hi, Math.Max(la, lb));
			}
			else if (lz <= -reach - cell || lz >= reach + cell)
				continue;
			if (hi < lo)
				continue;
			int ri0 = Math.Max(i0, Math.Floor((lo + ax) / cell) - 1);
			int ri1 = Math.Min(i1, Math.Ceil((hi + ax) / cell) + 1);
			for (int i = ri0; i <= ri1; i++)
			{
				m_iStatStWalk++;
				float px = i * cell - ax;
				float t = px * dx + pz * dz;
				if (t < 0 || t > far)
					continue;
				float latS = px * dz - pz * dx;
				float lat = Math.AbsFloat(latS);
				if (lat >= reach || (t > nose && lat >= half))
					continue;
				if (m_bA2)
					m_aDm[(k - k0 + 1) * m_iDw + i - i0 + 1] = 1;     // A2: the strip reached it (before the tile test)
				int l;
				MUD_MudSyncTile tile = TileOf(i, k, true, l);
				if (!tile)
					continue;   // not a zone tile: the sheet draws nothing there
				float d0 = tile.m_aD[l];
				if (tile.m_aPV[l] != veh || time - tile.m_aPT[l] > PASS_GAP_T)
					tile.m_aP[l] = d0;      // a new pass starts here
				else
					d0 = tile.m_aP[l];
				tile.m_aPT[l] = time;
				tile.m_aPV[l] = veh;
				float target = 0;
				float du = 0;
				float dv = 0;
				if (t <= nose && lat < half)
				{
					// PRESS (SOFT_WALL: full in the middle, none at the side), texture dragged along
					float pf = 1;
					float inW = half * MUD_MudSheet.SOFT_WALL;
					float fromSide = half - lat;
					if (fromSide < inW)
						pf = 0.5 * (1 - Math.Cos(Math.PI * fromSide / inW));
					tile.m_aD[l] = Math.Min(tile.m_aD[l], Math.Max(0, Math.Min(d0, thick) - MUD_MudSheet.PRESS_STEP * pf));
					du = dir[0] * drag;
					dv = dir[2] * drag;
				}
				else
				{
					// PUSH: push ahead (tyre wide) or side ridge (round shoulder, flat top, falls off)
					float w = 1;
					float hh = h;
					vector pushDir = dir;
					if (t > nose)
					{
						hh = h * MUD_MudSheet.ARCH_H;
						w = Math.Cos((t - nose) / MUD_MudSheet.FRONT_LEN * Math.PI * 0.5);
					}
					else
					{
						float x = (lat - half) / band;
						if (x > MUD_MudSheet.RIDGE_TOP)
							w = 0.5 * (1 + Math.Cos(Math.PI * (x - MUD_MudSheet.RIDGE_TOP) / (1 - MUD_MudSheet.RIDGE_TOP)));
						else
							w = Math.Sin(0.5 * Math.PI * x / MUD_MudSheet.RIDGE_TOP);
						pushDir = side;
						if (latS < 0)
							pushDir = -side;
					}
					target = Math.Min(thick + hh * w, Math.Max(d0, thick) + hh * w * MUD_MudSheet.PRESS_STEP / thick);
					du = pushDir[0] * drag * 0.5 * w;
					dv = pushDir[2] * drag * 0.5 * w;
				}
				if (target > tile.m_aD[l])
					tile.m_aD[l] = target;
				float ou = tile.m_aOu[l] + du;
				float ov = tile.m_aOv[l] + dv;
				float ol = Math.Sqrt(ou * ou + ov * ov);
				if (ol > MUD_MudSheet.DRAG_MAX)
				{
					ou *= MUD_MudSheet.DRAG_MAX / ol;
					ov *= MUD_MudSheet.DRAG_MAX / ol;
				}
				tile.m_aOu[l] = ou;
				tile.m_aOv[l] = ov;
				any = true;
				m_iStatStHit++;
				if (m_bA1)
				{
					m_aStI.Insert(i);
					m_aStK.Insert(k);
				}
			}
		}
		m_iMsStCells += System.GetTickCount(tC);
		if (!any)
			return;
		int tD = System.GetTickCount();
		if (MUD_MudSheet.DRAG_LIM > 0)
		{
			if (m_bA2)
				LimitDragStrip(i0, k0);
			else
				LimitDrag(i0, i1, k0, k1);
		}
		m_iMsStDrag += System.GetTickCount(tD);
		if (m_iBi1 < m_iBi0)
		{
			m_iBi0 = i0;
			m_iBi1 = i1;
			m_iBk0 = k0;
			m_iBk1 = k1;
		}
		m_iBi0 = Math.Min(m_iBi0, i0);
		m_iBi1 = Math.Max(m_iBi1, i1);
		m_iBk0 = Math.Min(m_iBk0, k0);
		m_iBk1 = Math.Max(m_iBk1, k1);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_MudSheet.LimitDrag on the sync tiles: each cell's drag pulled to within DRAG_LIM of its 4 neighbours
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
					LimitDragCell(i, k);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One cell of the drag limit (LimitDrag / LimitDragStrip): its drag pulled to within DRAG_LIM of its 4 neighbours.
	//! No tile: skipped; a neighbour with no tile reads THICK / 0 drag (GetI).
	protected void LimitDragCell(int i, int k)
	{
		m_iStatDragCells++;
		float lim = MUD_MudSheet.DRAG_LIM;
		// sheet perf P1 (2026-10-18, DS client 11:08: LimitDrag = 84-93 % of the sync stamp ms; v190 = before): a cell
		// whose 4 neighbours lie in its own tile reads them straight from that tile's arrays (same values, same order
		// as GetI); cells on a tile edge (or near 0, where TileOf's int division differs) keep the TileOf path.
		int tq = MUD_MudSheet.TQ;
		int l;
		MUD_MudSyncTile t = TileOf(i, k, false, l);
		if (!t)
			return;
		float ou = t.m_aOu[l];
		float ov = t.m_aOv[l];
		int li = l % tq;
		int lk = l / tq;
		if (i > tq && k > tq && li > 0 && li < tq - 1 && lk > 0 && lk < tq - 1)
		{
			// neighbours in the order of the loop below: i - 1, i + 1, k - 1, k + 1
			float au = t.m_aOu[l - 1];
			float av = t.m_aOv[l - 1];
			ou = Math.Clamp(ou, au - lim, au + lim);
			ov = Math.Clamp(ov, av - lim, av + lim);
			au = t.m_aOu[l + 1];
			av = t.m_aOv[l + 1];
			ou = Math.Clamp(ou, au - lim, au + lim);
			ov = Math.Clamp(ov, av - lim, av + lim);
			au = t.m_aOu[l - tq];
			av = t.m_aOv[l - tq];
			ou = Math.Clamp(ou, au - lim, au + lim);
			ov = Math.Clamp(ov, av - lim, av + lim);
			au = t.m_aOu[l + tq];
			av = t.m_aOv[l + tq];
			ou = Math.Clamp(ou, au - lim, au + lim);
			ov = Math.Clamp(ov, av - lim, av + lim);
			t.m_aOu[l] = ou;
			t.m_aOv[l] = ov;
			return;
		}
		for (int q = 0; q < 4; q++)
		{
			int ni = i;
			int nk = k;
			if (q == 0)
				ni--;
			else if (q == 1)
				ni++;
			else if (q == 2)
				nk--;
			else
				nk++;
			float nd;
			float nou;
			float nov;
			GetI(ni, nk, nd, nou, nov);
			ou = Math.Clamp(ou, nou - lim, nou + lim);
			ov = Math.Clamp(ov, nov - lim, nov + lim);
		}
		// the neighbour read may have moved the cache: write through the cell's own tile again
		MUD_MudSyncTile tw = TileOf(i, k, false, l);
		tw.m_aOu[l] = ou;
		tw.m_aOv[l] = ov;
	}

	//------------------------------------------------------------------------------------------------
	//! A2 DRAG_STRIP, Stamp start: the strip marks of this stamp, dw x dh = its box + 1 cell each side, all 0
	protected void DragStripBegin(int dw, int dh)
	{
		m_iDw = dw;
		m_iDh = dh;
		int n = dw * dh;
		m_aDm.Resize(n);
		for (int z = 0; z < n; z++)
			m_aDm[z] = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! A2 DRAG_STRIP, Stamp end: the drag limit on the marked cells + 1 ring (4 neighbours, from the marks - not in
	//! place), in row order (k up, i up), then the same list reversed (= LimitDrag's reversed k and i). i0, k0 = the
	//! stamp box corner (box cell x, y = world i0 - 1 + x, k0 - 1 + y).
	protected void LimitDragStrip(int i0, int k0)
	{
		int dw = m_iDw;
		int dh = m_iDh;
		m_aDl.Clear();
		for (int y = 0; y < dh; y++)
		{
			for (int x = 0; x < dw; x++)
			{
				int c = y * dw + x;
				bool on = m_aDm[c] != 0;
				if (!on && x > 0 && m_aDm[c - 1] != 0)
					on = true;
				if (!on && x < dw - 1 && m_aDm[c + 1] != 0)
					on = true;
				if (!on && y > 0 && m_aDm[c - dw] != 0)
					on = true;
				if (!on && y < dh - 1 && m_aDm[c + dw] != 0)
					on = true;
				if (on)
					m_aDl.Insert(c);
			}
		}
		int cnt = m_aDl.Count();
		for (int sweep = 0; sweep < 2; sweep++)
		{
			for (int q = 0; q < cnt; q++)
			{
				int idx = q;
				if (sweep == 1)
					idx = cnt - 1 - q;
				int c2 = m_aDl[idx];
				int xx = c2 % dw;
				int yy = c2 / dw;
				LimitDragCell(i0 - 1 + xx, k0 - 1 + yy);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void GetI(int gi, int gk, out float d, out float ou, out float ov)
	{
		d = MUD_MudSheet.THICK;
		ou = 0;
		ov = 0;
		int l;
		MUD_MudSyncTile t = TileOf(gi, gk, false, l);
		if (!t)
			return;
		d = t.m_aD[l];
		ou = t.m_aOu[l];
		ov = t.m_aOv[l];
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_MudSheet.Slump on the sync tiles, SLUMP_PER_OP times over the op box m_iJi0..m_iJi1 x m_iJk0..m_iJk1 (the
	//! box is the fixed op box, so every machine flows the same cells), in steps: SlumpBegin, SlumpGroundRow per row,
	//! SlumpCells, SlumpPass per iteration, SlumpWrite. Ground: GroundY on even cells, the cells between the mean of
	//! their even neighbours (as the sheet's traces). Cells off the zone tiles take and give nothing.
	protected void SlumpBegin()
	{
		int ri = m_iJi0 % 2;
		int rk = m_iJk0 % 2;
		m_iEi0 = m_iJi0 - ri;
		m_iEk0 = m_iJk0 - rk;
		m_iEw = (m_iJi1 - m_iEi0) / 2 + 2;
		m_iEh = (m_iJk1 - m_iEk0) / 2 + 2;
		m_aEg.Resize(m_iEw * m_iEh);
		m_aEgOk.Resize(m_iEw * m_iEh);
	}

	//------------------------------------------------------------------------------------------------
	//! JOB_GROUND: row m_iJobI of the even-cell ground grid
	protected void SlumpGroundRow()
	{
		float cell = MUD_MudSheet.CELL;
		int ek = m_iJobI;
		for (int ei = 0; ei < m_iEw; ei++)
		{
			bool gok;
			int e = ek * m_iEw + ei;
			if (m_bA1 && m_aEgNeed[e] == 0)
			{
				m_aEg[e] = 0;     // A1: no mask cell reads it
				m_aEgOk[e] = 0;
				continue;
			}
			m_aEg[e] = GroundY((m_iEi0 + ei * 2) * cell, (m_iEk0 + ek * 2) * cell, gok);
			m_aEgOk[e] = 0;
			if (gok)
				m_aEgOk[e] = 1;
		}
		m_iJobI++;
		if (m_iJobI >= m_iEh)
			m_iJobStep = JOB_CELLS;
	}

	//------------------------------------------------------------------------------------------------
	//! JOB_CELLS: per box cell the ground (mean of its even neighbours), the mud and whether it takes part
	protected void SlumpCells()
	{
		int w = m_iJi1 - m_iJi0 + 1;
		int hgt = m_iJk1 - m_iJk0 + 1;
		int n = w * hgt;
		int ew = m_iEw;
		m_aSg.Resize(n);
		m_aSd.Resize(n);
		m_aSdd.Resize(n);
		m_aSok.Resize(n);
		for (int k = 0; k < hgt; k++)
		{
			for (int i = 0; i < w; i++)
			{
				int c = k * w + i;
				if (m_bA1 && m_aSm[c] == 0)
				{
					m_aSok[c] = 0;     // A1: outside the mask - neither gives nor takes, no tile made
					m_aSd[c] = 0;
					m_aSg[c] = 0;
					continue;
				}
				int oi = m_iJi0 + i - m_iEi0;
				int ok2 = m_iJk0 + k - m_iEk0;
				int mi = oi % 2;
				int mk = ok2 % 2;
				int a0 = (oi - mi) / 2;
				int a1 = a0 + mi;
				int b0 = (ok2 - mk) / 2;
				int b1 = b0 + mk;
				m_aSg[c] = (m_aEg[b0 * ew + a0] + m_aEg[b0 * ew + a1] + m_aEg[b1 * ew + a0] + m_aEg[b1 * ew + a1]) * 0.25;
				int gAll = m_aEgOk[b0 * ew + a0] * m_aEgOk[b0 * ew + a1] * m_aEgOk[b1 * ew + a0] * m_aEgOk[b1 * ew + a1];
				int l;
				MUD_MudSyncTile t = TileOf(m_iJi0 + i, m_iJk0 + k, true, l);
				if (t && gAll != 0)
				{
					m_aSok[c] = 1;
					m_aSd[c] = t.m_aD[l];
				}
				else
				{
					m_aSok[c] = 0;
					m_aSd[c] = 0;
				}
			}
		}
		m_iJobStep = JOB_PASS;
		m_iJobI = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! JOB_PASS: one slump iteration over the box (all flows from the old depths, then added)
	protected void SlumpPass()
	{
		if (m_bA1)
		{
			SlumpPassList();
			return;
		}
		int w = m_iJi1 - m_iJi0 + 1;
		int hgt = m_iJk1 - m_iJk0 + 1;
		int n = w * hgt;
		float step = MUD_MudSheet.REPOSE * MUD_MudSheet.CELL;
		for (int z = 0; z < n; z++)
			m_aSdd[z] = 0;
		for (int k2 = 0; k2 < hgt; k2++)
		{
			for (int i2 = 0; i2 < w; i2++)
			{
				int v = k2 * w + i2;
				float da = m_aSd[v];
				if (m_aSok[v] == 0 || da <= 0)
					continue;
				float sa = m_aSg[v] + da;
				float give = 0;
				for (int nb = 0; nb < 4; nb++)
				{
					int ni = i2;
					int nk = k2;
					if (nb == 0)
						ni++;
					else if (nb == 1)
						ni--;
					else if (nb == 2)
						nk++;
					else
						nk--;
					if (ni < 0 || ni >= w || nk < 0 || nk >= hgt)
						continue;
					int nv = nk * w + ni;
					if (m_aSok[nv] == 0)
						continue;
					float drop = sa - m_aSg[nv] - m_aSd[nv] - step;
					if (drop <= 0)
						continue;
					float f = Math.Min(drop * MUD_MudSheet.SLUMP * 0.5, da * 0.25);
					if (f < 0.0002)
						continue;
					m_aSdd[nv] = m_aSdd[nv] + f;
					give += f;
				}
				m_aSdd[v] = m_aSdd[v] - give;
			}
		}
		for (int z2 = 0; z2 < n; z2++)
			m_aSd[z2] = m_aSd[z2] + m_aSdd[z2];
		m_iJobI++;
		if (m_iJobI >= SLUMP_PER_OP)
			m_iJobStep = JOB_WRITE;
	}

	//------------------------------------------------------------------------------------------------
	//! A1 SLUMP_ONE (StepStamp, after SlumpBegin - the op box is known only after its last stamp): the mask = the
	//! stamped cells (m_aStI / m_aStK) dilated by 1 over 4 neighbours, read from the seed copy (never in place); the
	//! op box's +1 margin holds the ring. m_aSmList = mask cells in row order.
	protected void BuildSlumpMask()
	{
		int w = m_iJi1 - m_iJi0 + 1;
		int h = m_iJk1 - m_iJk0 + 1;
		int n = w * h;
		m_aSmSeed.Resize(n);
		m_aSm.Resize(n);
		for (int z = 0; z < n; z++)
		{
			m_aSmSeed[z] = 0;
			m_aSm[z] = 0;
		}
		int cnt = m_aStI.Count();
		for (int s = 0; s < cnt; s++)
		{
			int c = (m_aStK[s] - m_iJk0) * w + m_aStI[s] - m_iJi0;
			m_aSmSeed[c] = 1;
		}
		m_aSmList.Clear();
		for (int k = 0; k < h; k++)
		{
			for (int i = 0; i < w; i++)
			{
				int c2 = k * w + i;
				bool on = m_aSmSeed[c2] != 0;
				if (!on && i > 0 && m_aSmSeed[c2 - 1] != 0)
					on = true;
				if (!on && i < w - 1 && m_aSmSeed[c2 + 1] != 0)
					on = true;
				if (!on && k > 0 && m_aSmSeed[c2 - w] != 0)
					on = true;
				if (!on && k < h - 1 && m_aSmSeed[c2 + w] != 0)
					on = true;
				if (on)
				{
					m_aSm[c2] = 1;
					m_aSmList.Insert(c2);
				}
			}
		}
		MarkEvenNeed(w);
	}

	//------------------------------------------------------------------------------------------------
	//! A1: the even cells some mask cell reads (its a0 / a1 x b0 / b1 in SlumpCells) - only these get a GroundY
	protected void MarkEvenNeed(int w)
	{
		int ne = m_iEw * m_iEh;
		m_aEgNeed.Resize(ne);
		for (int z = 0; z < ne; z++)
			m_aEgNeed[z] = 0;
		foreach (int c : m_aSmList)
		{
			int i = c % w;
			int k = c / w;
			int oi = m_iJi0 + i - m_iEi0;
			int ok2 = m_iJk0 + k - m_iEk0;
			int mi = oi % 2;
			int mk = ok2 % 2;
			int a0 = (oi - mi) / 2;
			int b0 = (ok2 - mk) / 2;
			m_aEgNeed[b0 * m_iEw + a0] = 1;
			m_aEgNeed[b0 * m_iEw + a0 + mi] = 1;
			m_aEgNeed[(b0 + mk) * m_iEw + a0] = 1;
			m_aEgNeed[(b0 + mk) * m_iEw + a0 + mi] = 1;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A1 JOB_PASS: one slump iteration over the mask cells only (SLUMP_ONE_K; flows only between two m_aSok cells,
	//! and m_aSok is set only inside the mask, so every giver and taker is a list cell). All flows from the old depths,
	//! then added, as SlumpPass.
	protected void SlumpPassList()
	{
		int w = m_iJi1 - m_iJi0 + 1;
		int hgt = m_iJk1 - m_iJk0 + 1;
		float step = MUD_MudSheet.REPOSE * MUD_MudSheet.CELL;
		int cnt = m_aSmList.Count();
		for (int z = 0; z < cnt; z++)
			m_aSdd[m_aSmList[z]] = 0;
		for (int q = 0; q < cnt; q++)
		{
			int v = m_aSmList[q];
			float da = m_aSd[v];
			if (m_aSok[v] == 0 || da <= 0)
				continue;
			int i2 = v % w;
			int k2 = v / w;
			float sa = m_aSg[v] + da;
			float give = 0;
			for (int nb = 0; nb < 4; nb++)
			{
				int ni = i2;
				int nk = k2;
				if (nb == 0)
					ni++;
				else if (nb == 1)
					ni--;
				else if (nb == 2)
					nk++;
				else
					nk--;
				if (ni < 0 || ni >= w || nk < 0 || nk >= hgt)
					continue;
				int nv = nk * w + ni;
				if (m_aSok[nv] == 0)
					continue;
				float drop = sa - m_aSg[nv] - m_aSd[nv] - step;
				if (drop <= 0)
					continue;
				float f = Math.Min(drop * SLUMP_ONE_K * 0.5, da * 0.25);
				if (f < 0.0002)
					continue;
				m_aSdd[nv] = m_aSdd[nv] + f;
				give += f;
			}
			m_aSdd[v] = m_aSdd[v] - give;
		}
		for (int z2 = 0; z2 < cnt; z2++)
		{
			int v2 = m_aSmList[z2];
			m_aSd[v2] = m_aSd[v2] + m_aSdd[v2];
		}
		m_iJobI++;
		if (m_iJobI >= SLUMP_ONE_PASSES)
			m_iJobStep = JOB_WRITE;
	}

	//------------------------------------------------------------------------------------------------
	//! TEST KIT self-test (StartJob, live layer only, before the op's own stamps): the SELF_TEST_AFTER-th rut op (or the
	//! next one that stamps and slumps) runs on two copies of the tiles around it - old maths (A1 / A2 off) and new
	//! (both on) - and the copies are compared. Prints once per game.
	protected void SelfTestMaybe(MUD_VoxelOp op)
	{
		m_iSelfTestN++;
		if (m_iSelfTestN < SELF_TEST_AFTER)
			return;
		if (SelfTest(op) || m_iSelfTestN >= SELF_TEST_AFTER + 50)
			s_bSelfTestDone = true;     // printed, or 50 ops in a row too big / landed nothing: no line
	}

	//------------------------------------------------------------------------------------------------
	//! Self-test: depth diff (A1 changes only depths) and drag diff (A2 changes only drag) between the two copies
	protected bool SelfTest(MUD_VoxelOp op)
	{
		int t0 = System.GetTickCount();
		MUD_MudSync a = SelfTestCopy(op, false, false);
		MUD_MudSync b = SelfTestCopy(op, true, true);
		if (!a.SelfTestRun(op) || !b.SelfTestRun(op))
			return false;
		// every tile either copy holds (a tile only one copy made = plain layer in the other)
		array<int> keys = {};
		foreach (int ka, MUD_MudSyncTile ta : a.m_mTiles)
			keys.Insert(ka);
		foreach (int kb, MUD_MudSyncTile tb : b.m_mTiles)
		{
			if (!a.m_mTiles.Contains(kb))
				keys.Insert(kb);
		}
		int nD = 0;
		float mD = 0;
		int nG = 0;
		float mG = 0;
		foreach (int key : keys)
		{
			MUD_MudSyncTile x = a.m_mTiles.Get(key);
			MUD_MudSyncTile y = b.m_mTiles.Get(key);
			for (int c = 0; c < TN; c++)
			{
				float dd = Math.AbsFloat(TileVal(x, c, 0) - TileVal(y, c, 0));
				if (dd > 0.000001)
				{
					nD++;
					if (dd > mD)
						mD = dd;
				}
				float du = Math.AbsFloat(TileVal(x, c, 1) - TileVal(y, c, 1));
				float dv = Math.AbsFloat(TileVal(x, c, 2) - TileVal(y, c, 2));
				float dg = Math.Max(du, dv);
				if (dg > 0.000001)
				{
					nG++;
					if (dg > mG)
						mG = dg;
				}
			}
		}
		string p1 = string.Format("MUD sheet cost self-test: cells changed %1, max diff %2 m (A1) | drag cells %3, max diff %4 m (A2)",
			nD, mD.ToString(-1, 6), nG, mG.ToString(-1, 6));
		string p2 = string.Format(" | seq %1, box %2 x %3 cells, slump cells old %4 new %5, ground reads old %6 new %7, drag limit cells old %8 new %9",
			op.m_iSeq, a.m_iJi1 - a.m_iJi0 + 1, a.m_iJk1 - a.m_iJk0 + 1, a.m_iStatSlumpCells, b.m_iStatSlumpCells, a.m_iStatGround, b.m_iStatGround, a.m_iStatDragCells, b.m_iStatDragCells);
		string p3 = string.Format(" | slump moved cells old %4 new %5 | %1 tiles compared, %2 stamps, %3 ms (test kit, once)", keys.Count(), a.m_iStatStamps, System.GetTickCount(t0), a.m_iStatSlumpMoved, b.m_iStatSlumpMoved);
		Print(p1 + p2 + p3, LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Self-test copy of the layer around op (its box + 1 tile each side), A1 / A2 as given
	protected MUD_MudSync SelfTestCopy(MUD_VoxelOp op, bool a1, bool a2)
	{
		MUD_MudSync cp = new MUD_MudSync();
		cp.m_bTest = true;
		cp.m_bA1 = a1;
		cp.m_bA2 = a2;
		float span = MUD_MudSheet.TQ * MUD_MudSheet.CELL;
		int tx0 = Math.Floor(op.m_vLo[0] / span) - 1;
		int tx1 = Math.Floor(op.m_vHi[0] / span) + 1;
		int tz0 = Math.Floor(op.m_vLo[2] / span) - 1;
		int tz1 = Math.Floor(op.m_vHi[2] / span) + 1;
		if (tx1 - tx0 > 20 || tz1 - tz0 > 20)
			return cp;     // odd op box: both copies start plain (still a fair old vs new compare)
		for (int tz = tz0; tz <= tz1; tz++)
		{
			for (int tx = tx0; tx <= tx1; tx++)
			{
				int key = MUD_MudSheet.TrailKey(tx, tz);
				MUD_MudSyncTile t = m_mTiles.Get(key);
				if (t)
					cp.m_mTiles.Set(key, CopyTile(t));
			}
		}
		return cp;
	}

	//------------------------------------------------------------------------------------------------
	//! Self-test: op from start to end on this copy, in one go. True = it stamped and slumped (not too big)
	protected bool SelfTestRun(MUD_VoxelOp op)
	{
		m_aQueue.Insert(op);
		StartJob();
		int guard = 0;
		while (m_Job && guard < 100000)
		{
			Step();
			guard++;
		}
		return m_iStatOps > 0 && !m_bJobBig;
	}

	//------------------------------------------------------------------------------------------------
	protected static MUD_MudSyncTile CopyTile(MUD_MudSyncTile t)
	{
		MUD_MudSyncTile c = new MUD_MudSyncTile();
		for (int l = 0; l < TN; l++)
		{
			c.m_aD[l] = t.m_aD[l];
			c.m_aOu[l] = t.m_aOu[l];
			c.m_aOv[l] = t.m_aOv[l];
			c.m_aP[l] = t.m_aP[l];
			c.m_aPT[l] = t.m_aPT[l];
			c.m_aPV[l] = t.m_aPV[l];
			c.m_aB[l] = t.m_aB[l];
		}
		return c;
	}

	//------------------------------------------------------------------------------------------------
	//! Self-test: cell c of tile t, what 0 = depth, 1 / 2 = drag; no tile = the plain layer (THICK, 0)
	protected static float TileVal(MUD_MudSyncTile t, int c, int what)
	{
		if (!t)
		{
			if (what == 0)
				return MUD_MudSheet.THICK;
			return 0;
		}
		if (what == 0)
			return t.m_aD[c];
		if (what == 1)
			return t.m_aOu[c];
		return t.m_aOv[c];
	}

	//------------------------------------------------------------------------------------------------
	//! JOB_WRITE: the slumped depths back into the tiles
	protected void SlumpWrite()
	{
		int w = m_iJi1 - m_iJi0 + 1;
		int hgt = m_iJk1 - m_iJk0 + 1;
		for (int k3 = 0; k3 < hgt; k3++)
		{
			for (int i3 = 0; i3 < w; i3++)
			{
				int c3 = k3 * w + i3;
				if (m_aSok[c3] == 0)
					continue;
				int l3;
				MUD_MudSyncTile t3 = TileOf(m_iJi0 + i3, m_iJk0 + k3, false, l3);
				if (t3.m_aD[l3] != m_aSd[c3])
					m_iStatSlumpMoved++;
				t3.m_aD[l3] = m_aSd[c3];
			}
		}
		if (m_bA1)
			m_iStatSlumpCells += m_aSmList.Count() * SLUMP_ONE_PASSES;
		else
			m_iStatSlumpCells += w * hgt;
		m_iJobStep = JOB_QUANT;
		m_iJobI = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5, DS (MUD_MudSheet.Tick): the op queue as on a client (no sheet to restore), a 5 s cost line while ops come
	static void DsTick()
	{
		if (!OnHere())
			return;
		Drain();
		if (!s_Inst)
			return;
		if (s_Inst.m_iDsLogAt == 0)
		{
			s_Inst.m_iDsLogAt = System.GetTickCount();
			return;
		}
		if (System.GetTickCount(s_Inst.m_iDsLogAt) < 5000)
			return;
		s_Inst.m_iDsLogAt = System.GetTickCount();
		if (s_Inst.m_iStatOps + s_Inst.m_iStatClears == 0)
			return;
		string line = StatLine();
		Print("MUD mud sheet sync (server): " + line, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5, server (MUD_VoxelProbe.MakeSnap, after Flush): the layer at seq s as parts "M|s|i|item/item/..." added
	//! to 'parts' (numbered on from its count). First item "L<tiles>" (the server has a layer - a joiner with no M parts
	//! keeps its own). Item "key,a,start:tokens" = array a (0 depth, 1 / 2 drag, 3 pass depth, 4 pass time, 5 pass
	//! vehicle, 6 bare) of tile key, cells start .. start + SNAP_CHUNK - 1 (RunToks). Every tile sends "key,0,0:" at
	//! least (a tile's existence is part of the state: LimitDrag and the hash read only existing tiles).
	static void SnapParts(int s, array<string> parts)
	{
		if (!OnHere())
			return;
		int t0 = System.GetTickCount();
		int n0 = parts.Count();
		int tiles = 0;
		if (s_Inst)
			tiles = s_Inst.m_mTiles.Count();
		string cur = "L" + tiles.ToString();
		if (s_Inst)
		{
			foreach (int key, MUD_MudSyncTile t : s_Inst.m_mTiles)
			{
				for (int a = 0; a < 7; a++)
				{
					for (int st = 0; st < TN; st += SNAP_CHUNK)
					{
						string toks = RunToks(t, a, st, SNAP_CHUNK);
						if (toks.IsEmpty() && (a != 0 || st != 0))
							continue;
						string item = string.Format("%1,%2,%3:", key, a, st) + toks;
						if (cur.Length() + item.Length() + 1 > SNAP_CHARS)
						{
							parts.Insert(string.Format("M|%1|%2|", s, parts.Count()) + cur);
							cur = item;
						}
						else
							cur = cur + "/" + item;
					}
				}
			}
		}
		parts.Insert(string.Format("M|%1|%2|", s, parts.Count()) + cur);
		int bytes = 0;
		for (int p = n0; p < parts.Count(); p++)
			bytes += parts[p].Length();
		int h = 0;
		if (s_Inst)
			h = LayerHash(s_Inst.m_mTiles);
		Print(string.Format("MUD sheet sync: snapshot at seq %1: %2 tiles in %3 parts, %4 bytes, %5 ms - layer hash %6", s, tiles, parts.Count() - n0, bytes, System.GetTickCount(t0), h), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5: cell l of array a as a snapshot int q. True = the array's default (sent as a run, q unset).
	protected static bool CellQ(MUD_MudSyncTile t, int a, int l, out int q)
	{
		q = 0;
		float thick = MUD_MudSheet.THICK;
		if (a == 0)
		{
			if (t.m_aD[l] == thick)
				return true;
			q = Math.Round(t.m_aD[l] * QUANT_F);
		}
		else if (a == 1)
		{
			if (t.m_aOu[l] == 0)
				return true;
			q = Math.Round(t.m_aOu[l] * QUANT_F);
		}
		else if (a == 2)
		{
			if (t.m_aOv[l] == 0)
				return true;
			q = Math.Round(t.m_aOv[l] * QUANT_F);
		}
		else if (a == 3)
		{
			if (t.m_aP[l] == thick)
				return true;
			q = Math.Round(t.m_aP[l] * QUANT_F);
		}
		else if (a == 4)
		{
			if (t.m_aPT[l] == NEVER)
				return true;
			q = t.m_aPT[l];
		}
		else if (a == 5)
		{
			if (t.m_aPV[l] == 0)
				return true;
			q = t.m_aPV[l];
		}
		else
		{
			if (t.m_aB[l] == 0)
				return true;
			q = t.m_aB[l];
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5: CellQ back into cell l of array a (floats through FromQ, the server's own expression)
	protected static void SetQ(MUD_MudSyncTile t, int a, int l, int q)
	{
		if (a == 0)
			t.m_aD[l] = FromQ(q);
		else if (a == 1)
			t.m_aOu[l] = FromQ(q);
		else if (a == 2)
			t.m_aOv[l] = FromQ(q);
		else if (a == 3)
			t.m_aP[l] = FromQ(q);
		else if (a == 4)
			t.m_aPT[l] = q;
		else if (a == 5)
			t.m_aPV[l] = q;
		else
			t.m_aB[l] = q;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5: tokens of array a, cells st .. st + cnt - 1: "n<c>" = c cells at the default, "r<c>" = c cells at the
	//! value before, else the change from the value before (the first from 0). Trailing defaults left out; "" = all default.
	protected static string RunToks(MUD_MudSyncTile t, int a, int st, int cnt)
	{
		string s = "";
		int prev = 0;
		int nDef = 0;
		int nRep = 0;
		for (int l = st; l < st + cnt; l++)
		{
			int q;
			if (CellQ(t, a, l, q))
			{
				if (nRep > 0)
				{
					s = AddTok(s, "r" + nRep.ToString());
					nRep = 0;
				}
				nDef++;
				continue;
			}
			if (nDef > 0)
			{
				s = AddTok(s, "n" + nDef.ToString());
				nDef = 0;
			}
			if (q == prev)
			{
				nRep++;
				continue;
			}
			if (nRep > 0)
			{
				s = AddTok(s, "r" + nRep.ToString());
				nRep = 0;
			}
			int d = q - prev;
			s = AddTok(s, d.ToString());
			prev = q;
		}
		if (nRep > 0)
			s = AddTok(s, "r" + nRep.ToString());
		return s;
	}

	//------------------------------------------------------------------------------------------------
	protected static string AddTok(string s, string tok)
	{
		if (s.IsEmpty())
			return tok;
		return s + "," + tok;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5: a fresh sync tile (the plain layer)
	protected static MUD_MudSyncTile NewTile()
	{
		MUD_MudSyncTile t = new MUD_MudSyncTile();
		for (int c = 0; c < TN; c++)
		{
			t.m_aD[c] = MUD_MudSheet.THICK;
			t.m_aOu[c] = 0;
			t.m_aOv[c] = 0;
			t.m_aP[c] = MUD_MudSheet.THICK;
			t.m_aPT[c] = NEVER;
			t.m_aPV[c] = 0;
			t.m_aB[c] = 0;
		}
		return t;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5: snapshot part "M|s|i|..." into 'into'. hasL: an "L" item was in it. False = malformed or not from
	//! snapshot s (what parsed before the fault stays).
	protected static bool ParseSnapPart(string part, int s, map<int, ref MUD_MudSyncTile> into, out bool hasL)
	{
		hasL = false;
		array<string> f = {};
		part.Split("|", f, false);
		if (f.Count() != 4 || f[0] != "M" || !MUD_TrenchPlan.IsInt(f[1]) || f[1].ToInt() != s)
			return false;
		array<string> items = {};
		f[3].Split("/", items, true);
		array<string> hd = {};
		array<string> toks = {};
		foreach (string it : items)
		{
			if (it.Substring(0, 1) == "L")
			{
				hasL = true;
				continue;
			}
			int colon = it.IndexOf(":");
			if (colon < 1)
				return false;
			hd.Clear();
			it.Substring(0, colon).Split(",", hd, false);
			if (hd.Count() != 3 || !MUD_TrenchPlan.IsInt(hd[0]) || !MUD_TrenchPlan.IsInt(hd[1]) || !MUD_TrenchPlan.IsInt(hd[2]))
				return false;
			int key = hd[0].ToInt();
			int a = hd[1].ToInt();
			int at = hd[2].ToInt();
			if (a < 0 || a > 6 || at < 0 || at >= TN)
				return false;
			MUD_MudSyncTile t = into.Get(key);
			if (!t)
			{
				t = NewTile();
				into.Set(key, t);
			}
			toks.Clear();
			it.Substring(colon + 1, it.Length() - colon - 1).Split(",", toks, true);
			int prev = 0;
			int end = at + SNAP_CHUNK;
			if (end > TN)
				end = TN;
			foreach (string tok : toks)
			{
				string c0 = tok.Substring(0, 1);
				if (c0 == "n" || c0 == "r")
				{
					string cnt = tok.Substring(1, tok.Length() - 1);
					if (!MUD_TrenchPlan.IsInt(cnt) || cnt.ToInt() < 1 || at + cnt.ToInt() > end)
						return false;
					int c = cnt.ToInt();
					if (c0 == "r")
					{
						for (int z = 0; z < c; z++)
							SetQ(t, a, at + z, prev);
					}
					at += c;
					continue;
				}
				if (!MUD_TrenchPlan.IsInt(tok) || at >= end)
					return false;
				prev = prev + tok.ToInt();
				SetQ(t, a, at, prev);
				at++;
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5, joiner (MUD_VoxelProbe.LoadSnap: every op <= s is in, none after): the server's layer at seq s replaces
	//! ours, then the drawn sheet takes it (MUD_MudSheet.SyncReload per tile, old and new). No "L" item = the server
	//! keeps no layer (a DS with SERVER_LAYER false): ours stays.
	static void LoadSnap(int s, array<string> parts)
	{
		if (!OnHere())
			return;
		int t0 = System.GetTickCount();
		if (!s_Inst)
			s_Inst = new MUD_MudSync();
		Flush();     // ops <= s still queued (crater / dig clears) end first; the snapshot holds them anyway
		map<int, ref MUD_MudSyncTile> got = new map<int, ref MUD_MudSyncTile>();
		int bad = 0;
		bool hasL = false;
		foreach (string part : parts)
		{
			bool pl;
			if (!ParseSnapPart(part, s, got, pl))
				bad++;
			if (pl)
				hasL = true;
		}
		if (!hasL)
		{
			Print(string.Format("MUD sheet sync: snapshot at seq %1 has no sheet layer (%2 parts) - mud before it not known here", s, parts.Count()), LogLevel.WARNING);
			return;
		}
		array<int> keys = {};
		foreach (int ok, MUD_MudSyncTile ot : s_Inst.m_mTiles)
			keys.Insert(ok);
		foreach (int nk, MUD_MudSyncTile nt : got)
		{
			if (!s_Inst.m_mTiles.Contains(nk))
				keys.Insert(nk);
		}
		s_Inst.m_mTiles = got;
		s_Inst.m_iLastKey = -1;
		s_Inst.m_LastTile = null;
		int tq = MUD_MudSheet.TQ;
		foreach (int key : keys)
		{
			int tx = key / 20000;     // MUD_MudSheet.TrailKey back
			int tz = key - tx * 20000;
			MUD_MudSheet.SyncReload(tx * tq, tx * tq + tq - 1, tz * tq, tz * tq + tq - 1);
			if (!MUD_MudSheet.LIFT_BY_DRAWN)
				MUD_WorldGroundEntity.SheetLiftDirty(tx * MUD_MudSheet.TILE_M, tz * MUD_MudSheet.TILE_M);     // A2: lift on / off
		}
		Print(string.Format("MUD sheet sync: snapshot loaded at seq %1: %2 tiles, %3 bad parts, %4 tiles redrawn, %5 ms - layer hash %6",
			s, got.Count(), bad, keys.Count(), System.GetTickCount(t0), LayerHash(got)), LogLevel.NORMAL);
		if (bad > 0)
			Print("MUD sheet sync: snapshot parts did not parse - mud on this machine differs from the server's", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5: hash of a whole layer, the same whatever order the map gives (sum of per-tile hashes): server
	//! 'snapshot at seq S ... layer hash H' vs joiner 'snapshot loaded at seq S ... layer hash H'
	protected static int LayerHash(map<int, ref MUD_MudSyncTile> tiles)
	{
		int total = 0;
		foreach (int key, MUD_MudSyncTile t : tiles)
		{
			int h = HashI(0, key);
			for (int l = 0; l < TN; l++)
			{
				h = HashS(h, t.m_aD[l]);
				h = HashS(h, t.m_aOu[l]);
				h = HashS(h, t.m_aOv[l]);
				h = HashS(h, t.m_aP[l]);
				h = HashI(h, t.m_aPT[l]);
				h = HashI(h, t.m_aPV[l]);
				h = HashI(h, t.m_aB[l]);
			}
			total = total + h;
			total = total % HASH_M;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 5 test kit, SP server (MUD_VoxelProbe.SnapSelfTest): the layer to snapshot text and back into a fresh map;
	//! every value of every tile must come back exactly
	static void SnapSelfTest(int s)
	{
		if (!OnHere() || !s_Inst || s_Inst.m_mTiles.IsEmpty())
			return;
		Flush();
		int t0 = System.GetTickCount();
		array<string> parts = {};
		SnapParts(s, parts);
		map<int, ref MUD_MudSyncTile> back = new map<int, ref MUD_MudSyncTile>();
		int bad = 0;
		int bytes = 0;
		foreach (string part : parts)
		{
			bytes += part.Length();
			bool pl;
			if (!ParseSnapPart(part, s, back, pl))
				bad++;
		}
		int wrong = 0;
		int missing = 0;
		foreach (int key, MUD_MudSyncTile t : s_Inst.m_mTiles)
		{
			MUD_MudSyncTile b = back.Get(key);
			if (!b)
			{
				missing++;
				continue;
			}
			for (int c = 0; c < TN; c++)
			{
				if (t.m_aD[c] != b.m_aD[c] || t.m_aOu[c] != b.m_aOu[c] || t.m_aOv[c] != b.m_aOv[c] || t.m_aP[c] != b.m_aP[c])
					wrong++;
				else if (t.m_aPT[c] != b.m_aPT[c] || t.m_aPV[c] != b.m_aPV[c] || t.m_aB[c] != b.m_aB[c])
					wrong++;
			}
		}
		int extra = back.Count() - (s_Inst.m_mTiles.Count() - missing);
		Print(string.Format("MUD sheet snap self-test: seq %1, %2 tiles, %3 parts, %4 bytes, %5 cells wrong, %6 missing, %7 extra, %8 bad parts, %9 ms",
			s, s_Inst.m_mTiles.Count(), parts.Count(), bytes, wrong, missing, extra, bad, System.GetTickCount(t0)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Hash of the sync mud (depth, drag; 0.1 mm steps; Stage 4: + bare) in world cells bi0..bi1 x bk0..bk1
	protected int Hash(int bi0, int bi1, int bk0, int bk1)
	{
		int h = 0;
		for (int k = bk0; k <= bk1; k++)
		{
			for (int i = bi0; i <= bi1; i++)
			{
				float d;
				float ou;
				float ov;
				GetI(i, k, d, ou, ov);
				h = HashStep(h, d);
				h = HashStep(h, ou);
				h = HashStep(h, ov);
				int l;
				MUD_MudSyncTile t = TileOf(i, k, false, l);
				if (t)
					h = HashStep(h, t.m_aB[l]);
			}
		}
		return h;
	}

	//------------------------------------------------------------------------------------------------
	protected int HashStep(int h, float val)
	{
		return MUD_MudSync.HashS(h, val);
	}

	//------------------------------------------------------------------------------------------------
	//! A float in 0.1 mm steps into hash h
	protected static int HashS(int h, float val)
	{
		int q = Math.Round(val * 10000);
		return HashI(h, q);
	}

	//------------------------------------------------------------------------------------------------
	//! An int into hash h
	protected static int HashI(int h, int q)
	{
		q = q % HASH_M;
		q = q + HASH_M;
		q = q % HASH_M;
		int r = h * 31 + q;
		r = r % HASH_M;
		return r;
	}
}

//! One sync tile: TQ x TQ world cells (no shared edges), 7 x 400 x 4 bytes
class MUD_MudSyncTile
{
	float m_aD[400];        // mud depth over the ground (m), THICK = untouched layer
	float m_aOu[400];       // texture drag x / z (m)
	float m_aOv[400];
	float m_aP[400];        // GRADUAL: mud depth when the cell's current pass began
	int m_aPT[400];         // op time (0.1 s) of the last stamp here, MUD_MudSync.NEVER = none
	int m_aPV[400];         // vehicle number of the last stamp here
	int m_aB[400];          // Stage 4: 1 = a crater / dig op cleared the mud here (bare)
}
