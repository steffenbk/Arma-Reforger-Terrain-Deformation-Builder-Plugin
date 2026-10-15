# Trench Plan Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** With a shovel, plan a trench point to point with a ghost preview, confirm it, then dig it piece by piece into a flat-floored, steep-walled box cut in the world ground.

**Architecture:** `MUD_TrenchLayout` (pure math, every machine) turns a plan into ~1 m pieces; `MUD_WorldGroundEntity.CarveTrench` sinks one piece's fine verts to its layer depth; `MUD_TrenchSiteComponent` (server-owned, replicated plan + progress text) holds one confirmed trench and its ghosts; `MUD_TrenchPlanner` (local) is the CSK-style plan mode; the existing dig helper offers 'Dig trench' over an unfinished piece.

**Tech Stack:** Enforce Script (Arma Reforger Experimental 192624), Workbench play test, log reading. No git.

**Spec:** `docs/superpowers/specs/2026-10-09-mud-trench-plan-mode-design.md` (approved 2026-10-09 23:08). Design notes: `docs/superpowers/plans/2026-10-09-mud-trench-plan-mode-NOTES.md`.

## Global Constraints

- Target Experimental 192624 (same as Plan 3). No git steps. Backups go to `ArmaReforgerWorkbench/mud_backups/archive/scripts/<file>.2026-10-09-before-trench<N>` before each edited file is touched.
- CSK is a separate mod: copy its pattern, no dependency on CSK classes or files.
- Width (floor) 0.5 / 1.0 / 1.5 / 2.0 m (X cycles), default 1.0 (changed 2026-10-09 23:50 after stage 1: 0.75 dug a 0.5 m floor on the 0.25 m grid). The code in Task 1 below is the first version; the live file MUD_TrenchLayout.c has the width list, SIDE_EPS / END_EPS and shared piece ends. Depth 0.5 to 1.5 m in 0.25 m layers (wheel), default 1.5.
- Up to 10 points; segment chord 0.5-12 m; one bend per segment, max 0.98 of the half chord, only on chords >= 3 m.
- Pieces per segment `max(1, round(arcLength / 1.0))`, stretched evenly; half-open spans, chain's last piece closed; corner disc radius width/2 belongs to the earlier segment's last piece; owner = first piece in chain order.
- One swing = one 0.25 m layer of the aimed piece; target `-0.25 * L` relative to undug ground; only lowers; skip `m_aLock <= 0` or `VertexSoft <= 0`; cap at `m_aFloor`.
- Keys: R toggle (shovel in hand), LMB point, RMB undo point, wheel depth, X width, V curve, F confirm. Context `MUD_TrenchPlanContext`, priority 46, flags `0xa 0`.
- Plain 'Dig' stays; 'Dig fast' and 'Square off' are removed.
- Enforce rules: no ternary; the same local name twice in one function (even sibling blocks) is a compile error; `MeshObject.Create` takes static arrays; `UpdateIndices` before `UpdateVerts`; keep a ref to the mesh `Resource`.
- Every stage ends: user compiles (F7) and plays; Claude reads the log and writes the result to the Plan 3 ledger.

## Review Focus

1. A trench planned across a road or building: those verts must stay put (locked count in the check line > 0, WRONG 0) and red pieces never get a 'Dig trench' action. Test: Task 4 play step (c).
2. Play stop with a live site and with plan mode open: no crash, no leaked ghost entities (ghosts are plain local entities deleted in `OnDelete` / destructors). Test: every play in Tasks 2-4 ends with play stop while a site / plan is alive; Claude checks the log for a crash / `0xc0000374`.
3. Swinging at a piece whose fine patch is still building: the swing must not count and must not crack the ground (CarveTrench -1). Test: Task 3 log line 'swing not counted'; checksum dig lines = swings.
4. Two trenches crossing: any unfinished piece under the dig point gets the swing; the crossing ends at the deeper of the two. Test: Task 4 play step (d).
5. Leaving plan mode by switching away from the shovel / getting in a vehicle: plan mode closes, ghosts go. Test: Task 4 play step (e).

---

## File map

| File | New / edit | Job |
|---|---|---|
| `Scripts/Game/Mud/MUD_TrenchLayout.c` | new (T1) | `MUD_TrenchPlan`, `MUD_TrenchSeg`, `MUD_TrenchPiece`, `MUD_TrenchLayout`, `MUD_TrenchSelfTest` |
| `Scripts/Game/Mud/MUD_WorldGround.c` | edit (T1-T4) | self-test hook, `Heights` / `SurfaceY` / `AtGround`, `CarveTrench`, `CheckTrench`, TEMP hooks, dig-mode removal, rim fix |
| `Scripts/Game/Mud/MUD_TrenchGhost.c` | new (T3) | `MUD_TrenchGhosts`: see-through slab per piece |
| `Scripts/Game/Mud/MUD_TrenchSite.c` | new (T3) | `MUD_TrenchSiteComponent` |
| `Prefabs/Mud/MudTrenchSite.et` (+ .meta) | new (T3) | site entity: component + RplComponent |
| `Assets/Mud/MUD_TrenchGhost*.emat` (+ .meta) | new (T3) | green / red / dug ghost materials |
| `Scripts/Game/Mud/MUD_Dig.c` | rewrite (T3), edit (T4) | single Dig action, 'Dig trench' label, trench branch in the dig RPC, R hook, helper hidden while planning |
| `Prefabs/Mud/MudDigHelper.et` | restore (T3) | single Dig action |
| `Configs/System/chimeraInputCommon.conf` | new (T4) | trench plan keys |
| `Scripts/Game/Mud/MUD_TrenchPlanner.c` | new (T4) | `MUD_TrenchPlanner` + modded `SCR_PlayerController` (plan mode, place RPC) |

---

### Task 1: Plan, layout and self-test

**Files:**
- Create: `Scripts/Game/Mud/MUD_TrenchLayout.c`
- Modify: `Scripts/Game/Mud/MUD_WorldGround.c` (Setup, after the "MUD world: ready" Print)

**Interfaces:**
- Produces: `MUD_TrenchPlan` (`m_iWidth`, `m_iLayers`, `m_aPoints`, `m_aBends`, `AddPoint(vector, float = 0)`, `Width()`, `Pack()`, `static Unpack(string)`, `static WidthOf(int)`, constants `MAX_POINTS MIN_SEGMENT MAX_SEGMENT MAX_BEND MIN_BEND_CHORD LAYER MIN_LAYERS MAX_LAYERS WIDTHS`); `MUD_TrenchLayout` (`bool Build(MUD_TrenchPlan)`, `string m_sError`, `int Count()`, `int Layers()`, `float Half()`, `int SegCount()`, `float SegLength(int)`, `int SegPieces(int)`, `int PieceAt(float x, float z)`, `int ShapesAt(float x, float z)`, `void PieceBox(int, out vector lo, out vector hi)`, `vector PieceMiddle(int)`, `vector PieceEdge(int, float t, float side)`); `MUD_TrenchSelfTest.Run()`.

- [ ] **Step 1: Create `Scripts/Game/Mud/MUD_TrenchLayout.c`**

```c
//! Trench plan mode, pure math (spec docs/superpowers/specs/2026-10-09-mud-trench-plan-mode-design.md
//! section 4). The same input gives the same pieces on every machine: the planner draws its ghosts,
//! the server spawns the site and the ground carves from this one class. World XZ only (y ignored).
//! Curve math copied from CSK_SandbagWallLayout (FrameAt, OnPiece, CurvatureOf, ArcLength).

//------------------------------------------------------------------------------------------------
//! What the player planned. Travels as int-centimetre text "w,l;x,z,b;x,z,b;..." (Pack / Unpack):
//! w = width index (WidthOf), l = layers of LAYER m, then per point x, z and the bend (sagitta) of
//! the segment that starts there (the last point's bend is always 0).
class MUD_TrenchPlan
{
	static const int MAX_POINTS = 10;
	static const float MIN_SEGMENT = 0.5;     // m, chord
	static const float MAX_SEGMENT = 12;      // m, chord (CSK value)
	static const float MAX_BEND = 0.98;       // share of the half chord (CSK_SandbagWallLayout.MAX_BEND)
	static const float MIN_BEND_CHORD = 3;    // m: shorter segments stay straight
	static const float LAYER = 0.25;          // m one swing digs
	static const int MIN_LAYERS = 2;
	static const int MAX_LAYERS = 6;
	static const int WIDTHS = 4;

	int m_iWidth = 1;                         // WidthOf: 0.5 / 0.75 / 1.0 / 1.5 m
	int m_iLayers = MAX_LAYERS;
	ref array<vector> m_aPoints = {};
	ref array<float> m_aBends = {};           // one per point; the last one is unused (0)

	//------------------------------------------------------------------------------------------------
	static float WidthOf(int index)
	{
		if (index == 0)
			return 0.5;
		if (index == 2)
			return 1.0;
		if (index == 3)
			return 1.5;
		return 0.75;
	}

	//------------------------------------------------------------------------------------------------
	float Width()
	{
		return WidthOf(m_iWidth);
	}

	//------------------------------------------------------------------------------------------------
	void AddPoint(vector p, float bend = 0)
	{
		m_aPoints.Insert(p);
		m_aBends.Insert(bend);
	}

	//------------------------------------------------------------------------------------------------
	string Pack()
	{
		string s = string.Format("%1,%2", m_iWidth, m_iLayers);
		int n = m_aPoints.Count();
		for (int i = 0; i < n; i++)
		{
			vector p = m_aPoints[i];
			int x = Math.Round(p[0] * 100);
			int z = Math.Round(p[2] * 100);
			int b = 0;
			if (i < n - 1)
				b = Math.Round(m_aBends[i] * 100);
			s += string.Format(";%1,%2,%3", x, z, b);
		}
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! null on anything malformed (server input: never trust it). Geometry limits are Build's job.
	static MUD_TrenchPlan Unpack(string text)
	{
		array<string> parts = {};
		text.Split(";", parts, false);
		if (parts.Count() < 3 || parts.Count() > MAX_POINTS + 1)
			return null;

		array<int> head = {};
		if (!Ints(parts[0], head) || head.Count() != 2)
			return null;
		if (head[0] < 0 || head[0] >= WIDTHS || head[1] < MIN_LAYERS || head[1] > MAX_LAYERS)
			return null;

		MUD_TrenchPlan plan = new MUD_TrenchPlan();
		plan.m_iWidth = head[0];
		plan.m_iLayers = head[1];
		for (int i = 1; i < parts.Count(); i++)
		{
			array<int> pt = {};
			if (!Ints(parts[i], pt) || pt.Count() != 3)
				return null;
			plan.AddPoint(Vector(pt[0] * 0.01, 0, pt[1] * 0.01), pt[2] * 0.01);
		}
		plan.m_aBends[plan.m_aBends.Count() - 1] = 0;
		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! "12,-3,40" -> 12, -3, 40 appended to outInts; false on anything that is not a plain integer
	protected static bool Ints(string text, array<int> outInts)
	{
		array<string> nums = {};
		text.Split(",", nums, false);
		foreach (string n : nums)
		{
			if (!IsInt(n))
				return false;
			outInts.Insert(n.ToInt());
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Optional '-', then 1-8 digits
	static bool IsInt(string s)
	{
		int len = s.Length();
		if (len == 0 || len > 9)
			return false;
		string digits = "0123456789";
		for (int i = 0; i < len; i++)
		{
			string c = s.Substring(i, 1);
			if (i == 0 && c == "-" && len > 1)
				continue;
			if (digits.IndexOf(c) < 0)
				return false;
		}
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! One segment of the centre line: an arc (or straight) between two plan points. Local x runs
//! along the arc from -m_fLength / 2 to +m_fLength / 2, 0 at the arc middle m_vMid.
class MUD_TrenchSeg
{
	vector m_vMid;      // arc middle (y 0)
	vector m_vRight;    // chord direction, start -> end
	vector m_vFwd;      // right x up; a positive bend bows this way
	float m_fCurv;      // 1 / radius, signed; 0 = straight
	float m_fLength;    // arc length
	vector m_vEnd;      // end point (y 0): centre of the corner disc
	int m_iFirst;       // first piece index
	int m_iCount;       // pieces on this segment
}

//------------------------------------------------------------------------------------------------
//! One ~1 m piece: span [m_fX0, m_fX1) of segment m_iSeg's local x (the chain's last piece is closed)
class MUD_TrenchPiece
{
	int m_iSeg;
	float m_fX0;
	float m_fX1;
	bool m_bLastOfSeg;
	vector m_vLo;       // quick-reject box (world XZ), padded
	vector m_vHi;
}

//------------------------------------------------------------------------------------------------
class MUD_TrenchLayout
{
	static const float PIECE = 1.0;           // m, nominal piece length
	static const float CURVE_EPS = 0.0001;
	static const float BOX_PAD = 0.05;

	string m_sError;                          // why Build failed
	protected ref array<ref MUD_TrenchSeg> m_aSegs = {};
	protected ref array<ref MUD_TrenchPiece> m_aPieces = {};
	protected float m_fHalf;
	protected int m_iLayers;

	//------------------------------------------------------------------------------------------------
	//! False (m_sError says why) when the plan breaks a limit; then the layout is empty
	bool Build(MUD_TrenchPlan plan)
	{
		m_aSegs.Clear();
		m_aPieces.Clear();
		m_sError = "";
		if (!plan)
			return Fail("no plan");

		int n = plan.m_aPoints.Count();
		if (n < 2 || n > MUD_TrenchPlan.MAX_POINTS)
			return Fail(string.Format("%1 points (2 to %2)", n, MUD_TrenchPlan.MAX_POINTS));
		if (plan.m_iLayers < MUD_TrenchPlan.MIN_LAYERS || plan.m_iLayers > MUD_TrenchPlan.MAX_LAYERS)
			return Fail(string.Format("%1 layers", plan.m_iLayers));

		m_fHalf = plan.Width() * 0.5;
		m_iLayers = plan.m_iLayers;
		for (int i = 0; i < n - 1; i++)
		{
			vector a = plan.m_aPoints[i];
			vector b = plan.m_aPoints[i + 1];
			a[1] = 0;
			b[1] = 0;
			vector chord = b - a;
			float len = chord.Length();
			if (len < MUD_TrenchPlan.MIN_SEGMENT - 0.001 || len > MUD_TrenchPlan.MAX_SEGMENT + 0.01)
				return Fail(string.Format("segment %1 is %2 m (%3 to %4 m)", i + 1, len, MUD_TrenchPlan.MIN_SEGMENT, MUD_TrenchPlan.MAX_SEGMENT));

			float halfChord = len * 0.5;
			float bend = plan.m_aBends[i];
			if (bend != 0 && len < MUD_TrenchPlan.MIN_BEND_CHORD - 0.01)
				return Fail(string.Format("segment %1: only segments of %2 m or more can bend", i + 1, MUD_TrenchPlan.MIN_BEND_CHORD));
			if (Math.AbsFloat(bend) > halfChord * MUD_TrenchPlan.MAX_BEND + 0.01)
				return Fail(string.Format("segment %1: bend %2 m is more than %3 m", i + 1, bend, halfChord * MUD_TrenchPlan.MAX_BEND));

			MUD_TrenchSeg seg = new MUD_TrenchSeg();
			seg.m_vRight = chord * (1 / len);
			seg.m_vFwd = Vector(-seg.m_vRight[2], 0, seg.m_vRight[0]);
			seg.m_fCurv = CurvatureOf(halfChord, bend);
			seg.m_vMid = (a + b) * 0.5 + seg.m_vFwd * bend;
			seg.m_fLength = ArcLength(halfChord, seg.m_fCurv);
			seg.m_vEnd = b;
			seg.m_iFirst = m_aPieces.Count();
			int count = Math.Max(1, Math.Round(seg.m_fLength / PIECE));
			seg.m_iCount = count;
			float step = seg.m_fLength / count;
			for (int k = 0; k < count; k++)
			{
				MUD_TrenchPiece piece = new MUD_TrenchPiece();
				piece.m_iSeg = m_aSegs.Count();
				piece.m_fX0 = -seg.m_fLength * 0.5 + k * step;
				piece.m_fX1 = piece.m_fX0 + step;
				if (k == count - 1)
				{
					piece.m_fX1 = seg.m_fLength * 0.5;
					piece.m_bLastOfSeg = true;
				}
				m_aPieces.Insert(piece);
			}
			m_aSegs.Insert(seg);
		}

		// boxes once every segment exists (the corner disc needs to know which segment is last)
		for (int p = 0; p < m_aPieces.Count(); p++)
		{
			MakeBox(p);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Fail(string why)
	{
		m_sError = why;
		m_aSegs.Clear();
		m_aPieces.Clear();
		return false;
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_aPieces.Count();
	}

	//------------------------------------------------------------------------------------------------
	int Layers()
	{
		return m_iLayers;
	}

	//------------------------------------------------------------------------------------------------
	float Half()
	{
		return m_fHalf;
	}

	//------------------------------------------------------------------------------------------------
	int SegCount()
	{
		return m_aSegs.Count();
	}

	//------------------------------------------------------------------------------------------------
	float SegLength(int s)
	{
		return m_aSegs[s].m_fLength;
	}

	//------------------------------------------------------------------------------------------------
	int SegPieces(int s)
	{
		return m_aSegs[s].m_iCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Piece that owns world (x, z): the first in chain order whose shape holds it; -1 = outside
	int PieceAt(float x, float z)
	{
		for (int i = 0; i < m_aPieces.Count(); i++)
		{
			if (InShape(i, x, z))
				return i;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! How many piece shapes hold (x, z) (self-test: overlaps at corners / tight curves)
	int ShapesAt(float x, float z)
	{
		int n;
		for (int i = 0; i < m_aPieces.Count(); i++)
		{
			if (InShape(i, x, z))
				n++;
		}
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! World XZ box around the piece (padded), y 0
	void PieceBox(int index, out vector lo, out vector hi)
	{
		lo = m_aPieces[index].m_vLo;
		hi = m_aPieces[index].m_vHi;
	}

	//------------------------------------------------------------------------------------------------
	//! Centre-line point in the middle of the piece (y 0)
	vector PieceMiddle(int index)
	{
		MUD_TrenchPiece piece = m_aPieces[index];
		return PointAt(m_aSegs[piece.m_iSeg], (piece.m_fX0 + piece.m_fX1) * 0.5, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Point at share t (0..1) along the piece, side -1 .. 1 of the half width off the centre line (y 0)
	vector PieceEdge(int index, float t, float side)
	{
		MUD_TrenchPiece piece = m_aPieces[index];
		return PointAt(m_aSegs[piece.m_iSeg], piece.m_fX0 + (piece.m_fX1 - piece.m_fX0) * t, side * m_fHalf);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasDisc(int index)
	{
		MUD_TrenchPiece piece = m_aPieces[index];
		return piece.m_bLastOfSeg && piece.m_iSeg < m_aSegs.Count() - 1;
	}

	//------------------------------------------------------------------------------------------------
	//! The piece's own shape, ownership ignored: its span of the segment box (half-open, the chain's
	//! last piece closed) plus, for a segment's last piece before a corner, the disc around the corner
	protected bool InShape(int index, float px, float pz)
	{
		MUD_TrenchPiece piece = m_aPieces[index];
		if (px < piece.m_vLo[0] || px > piece.m_vHi[0] || pz < piece.m_vLo[2] || pz > piece.m_vHi[2])
			return false;

		MUD_TrenchSeg seg = m_aSegs[piece.m_iSeg];
		float x;
		float side;
		OnSeg(seg, px, pz, x, side);
		if (Math.AbsFloat(side) <= m_fHalf && x >= piece.m_fX0)
		{
			if (x < piece.m_fX1)
				return true;
			if (index == m_aPieces.Count() - 1 && x <= piece.m_fX1)
				return true;
		}

		if (!HasDisc(index))
			return false;
		float dx = px - seg.m_vEnd[0];
		float dz = pz - seg.m_vEnd[2];
		return dx * dx + dz * dz <= m_fHalf * m_fHalf;
	}

	//------------------------------------------------------------------------------------------------
	protected void MakeBox(int index)
	{
		MUD_TrenchPiece piece = m_aPieces[index];
		MUD_TrenchSeg seg = m_aSegs[piece.m_iSeg];
		vector lo = Vector(100000000, 0, 100000000);
		vector hi = Vector(-100000000, 0, -100000000);
		for (int j = 0; j <= 4; j++)
		{
			float x = piece.m_fX0 + (piece.m_fX1 - piece.m_fX0) * j * 0.25;
			Grow(lo, hi, PointAt(seg, x, -m_fHalf));
			Grow(lo, hi, PointAt(seg, x, m_fHalf));
		}
		if (HasDisc(index))
		{
			Grow(lo, hi, seg.m_vEnd - Vector(m_fHalf, 0, m_fHalf));
			Grow(lo, hi, seg.m_vEnd + Vector(m_fHalf, 0, m_fHalf));
		}
		piece.m_vLo = lo - Vector(BOX_PAD, 0, BOX_PAD);
		piece.m_vHi = hi + Vector(BOX_PAD, 0, BOX_PAD);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Grow(inout vector lo, inout vector hi, vector p)
	{
		lo[0] = Math.Min(lo[0], p[0]);
		lo[2] = Math.Min(lo[2], p[2]);
		hi[0] = Math.Max(hi[0], p[0]);
		hi[2] = Math.Max(hi[2], p[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Point x along the segment's centre line, side off it (+ = forward side for a straight segment,
	//! away from the circle centre for a positive bend). CSK_SandbagWallLayout.FrameAt.
	static vector PointAt(MUD_TrenchSeg seg, float x, float side)
	{
		float k = seg.m_fCurv;
		if (Math.AbsFloat(k) < CURVE_EPS)
			return seg.m_vMid + seg.m_vRight * x + seg.m_vFwd * side;

		float angle = k * x;
		float c = Math.Cos(angle);
		float s = Math.Sin(angle);
		vector dir = seg.m_vRight * s + seg.m_vFwd * c;
		return seg.m_vMid + seg.m_vRight * (s / k) - seg.m_vFwd * ((1 - c) / k) + dir * side;
	}

	//------------------------------------------------------------------------------------------------
	//! Where (px, pz) lies on the segment: x along its centre line, side off it. Inverse of PointAt
	//! (CSK_SandbagWallLayout.OnPiece).
	static void OnSeg(MUD_TrenchSeg seg, float px, float pz, out float x, out float side)
	{
		vector rel = Vector(px - seg.m_vMid[0], 0, pz - seg.m_vMid[2]);
		float k = seg.m_fCurv;
		if (Math.AbsFloat(k) < CURVE_EPS)
		{
			x = vector.Dot(rel, seg.m_vRight);
			side = vector.Dot(rel, seg.m_vFwd);
			return;
		}

		// circle centre: 1 / k towards -forward from the arc middle (PointAt bends towards it)
		vector fromCentre = rel + seg.m_vFwd * (1 / k);
		float angle = Math.Atan2(vector.Dot(fromCentre, seg.m_vRight) * k, vector.Dot(fromCentre, seg.m_vFwd) * k);
		x = angle / k;
		side = fromCentre.Length() - Math.AbsFloat(1 / k);
		if (k < 0)
			side = -side;
	}

	//------------------------------------------------------------------------------------------------
	//! Curvature (1 / radius, signed) of the arc through two points halfChord apart whose middle is
	//! bend (sagitta) off the chord (CSK_SandbagWallLayout.CurvatureOf)
	static float CurvatureOf(float halfChord, float bend)
	{
		if (Math.AbsFloat(bend) < CURVE_EPS)
			return 0;

		return 2 * bend / (halfChord * halfChord + bend * bend);
	}

	//------------------------------------------------------------------------------------------------
	//! Arc length between two points halfChord apart on a circle of this curvature (CSK ArcLength)
	static float ArcLength(float halfChord, float curvature)
	{
		if (Math.AbsFloat(curvature) < CURVE_EPS)
			return 2 * halfChord;

		float sinHalf = Math.Clamp(halfChord * curvature, -1, 1);
		return 2 * Math.Asin(sinHalf) / curvature;
	}
}

//------------------------------------------------------------------------------------------------
//! Once per session: checks the layout math, logs 'MUD trench test: PASS (N checks)' or a FAIL line
//! per failed check. Cases from the plan (Task 1).
class MUD_TrenchSelfTest
{
	protected static bool s_bRan;
	protected int m_iChecks;
	protected int m_iFails;

	//------------------------------------------------------------------------------------------------
	static void Run()
	{
		if (s_bRan)
			return;
		s_bRan = true;
		MUD_TrenchSelfTest t = new MUD_TrenchSelfTest();
		t.All();
	}

	//------------------------------------------------------------------------------------------------
	protected void All()
	{
		Straight();
		Stretch();
		Corner();
		Curve();
		Packing();
		Refusals();
		Grid();
		if (m_iFails == 0)
			Print(string.Format("MUD trench test: PASS (%1 checks)", m_iChecks));
		else
			Print(string.Format("MUD trench test: FAIL %1 of %2 checks", m_iFails, m_iChecks), LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(bool ok, string what)
	{
		m_iChecks++;
		if (ok)
			return;
		m_iFails++;
		Print("MUD trench test: FAIL " + what, LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckAt(MUD_TrenchLayout lay, float x, float z, int want, string what)
	{
		int got = lay.PieceAt(x, z);
		Check(got == want, string.Format("%1: (%2, %3) piece %4, want %5", what, x, z, got, want));
	}

	//------------------------------------------------------------------------------------------------
	protected MUD_TrenchLayout Make(string packed, string what)
	{
		MUD_TrenchPlan plan = MUD_TrenchPlan.Unpack(packed);
		Check(plan != null, what + ": unpack " + packed);
		if (!plan)
			return null;
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		bool built = lay.Build(plan);
		Check(built, what + ": build - " + lay.m_sError);
		if (!built)
			return null;
		return lay;
	}

	//------------------------------------------------------------------------------------------------
	//! 10 m along +x, width 0.75: 10 pieces of 1 m; edges, ends and sides
	protected void Straight()
	{
		MUD_TrenchLayout lay = Make("1,6;0,0,0;1000,0,0", "straight");
		if (!lay)
			return;
		Check(lay.Count() == 10, string.Format("straight: %1 pieces, want 10", lay.Count()));
		CheckAt(lay, 0, 0, 0, "straight start");
		CheckAt(lay, 0.5, 0, 0, "straight");
		CheckAt(lay, 1, 0, 1, "straight half-open");
		CheckAt(lay, 9.99, 0, 9, "straight");
		CheckAt(lay, 10, 0, 9, "straight closed end");
		CheckAt(lay, 10.01, 0, -1, "straight past end");
		CheckAt(lay, -0.01, 0, -1, "straight before start");
		CheckAt(lay, 5.5, 0.37, 5, "straight side in");
		CheckAt(lay, 5.5, -0.37, 5, "straight side in");
		CheckAt(lay, 5.5, 0.38, -1, "straight side out");
	}

	//------------------------------------------------------------------------------------------------
	//! 10.4 m: still 10 pieces, stretched to 1.04 m
	protected void Stretch()
	{
		MUD_TrenchLayout lay = Make("1,6;0,0,0;1040,0,0", "stretch");
		if (!lay)
			return;
		Check(lay.Count() == 10, string.Format("stretch: %1 pieces, want 10", lay.Count()));
		CheckAt(lay, 1.03, 0, 0, "stretch");
		CheckAt(lay, 1.05, 0, 1, "stretch");
		CheckAt(lay, 10.4, 0, 9, "stretch closed end");
	}

	//------------------------------------------------------------------------------------------------
	//! (0,0)-(4,0)-(4,4): corner disc, ownership, outside the disc
	protected void Corner()
	{
		MUD_TrenchLayout lay = Make("1,6;0,0,0;400,0,0;400,400,0", "corner");
		if (!lay)
			return;
		Check(lay.Count() == 8, string.Format("corner: %1 pieces, want 8", lay.Count()));
		CheckAt(lay, 4.2, -0.2, 3, "corner disc");
		CheckAt(lay, 4, 0.6, 4, "corner second segment");
		CheckAt(lay, 4.3, -0.3, -1, "corner outside disc");
		CheckAt(lay, 3.9, 0.2, 3, "corner overlap owned by earlier piece");
		Check(lay.ShapesAt(3.9, 0.2) == 2, string.Format("corner overlap: %1 shapes, want 2", lay.ShapesAt(3.9, 0.2)));
	}

	//------------------------------------------------------------------------------------------------
	//! Chord 10 m along +x, bend 2 m towards +z: radius 7.25, arc 11.035 m, 11 pieces
	protected void Curve()
	{
		MUD_TrenchLayout lay = Make("1,6;0,0,200;1000,0,0", "curve");
		if (!lay)
			return;
		Check(Math.AbsFloat(lay.SegLength(0) - 11.035) < 0.01, string.Format("curve: arc %1 m, want 11.035", lay.SegLength(0)));
		Check(lay.Count() == 11, string.Format("curve: %1 pieces, want 11", lay.Count()));
		CheckAt(lay, 5, 2, 5, "curve middle");
		CheckAt(lay, 5, 2.3, 5, "curve side in");
		CheckAt(lay, 5, 2.4, -1, "curve side out");
		CheckAt(lay, 5, 0, -1, "curve chord middle");
	}

	//------------------------------------------------------------------------------------------------
	protected void Packing()
	{
		MUD_TrenchPlan plan = new MUD_TrenchPlan();
		plan.m_iWidth = 2;
		plan.m_iLayers = 4;
		plan.AddPoint(Vector(2110.37, 49.5, 2752.12), 1.5);
		plan.AddPoint(Vector(2118, 49.5, 2752), 0.7);
		string packed = plan.Pack();
		Check(packed == "2,4;211037,275212,150;211800,275200,0", "pack: " + packed);
		MUD_TrenchPlan back = MUD_TrenchPlan.Unpack(packed);
		Check(back != null, "pack: unpack of own text");
		if (back)
			Check(back.Pack() == packed, "pack: round trip " + back.Pack());

		array<string> garbage = {"", "1,6", "1,6;0,0,0", "x,6;0,0,0;100,0,0", "1,9;0,0,0;100,0,0", "4,6;0,0,0;100,0,0",
			"1,6;0,0;100,0,0", "1,6;0,0,0;1e3,0,0", "1,6;0,0,0;100,0,0;200,0,0;300,0,0;400,0,0;500,0,0;600,0,0;700,0,0;800,0,0;900,0,0;1000,0,0"};
		foreach (string g : garbage)
		{
			Check(MUD_TrenchPlan.Unpack(g) == null, "unpack should refuse '" + g + "'");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Refusals()
	{
		array<string> bad = {"1,6;0,0,0;0,0,0", "1,6;0,0,0;1300,0,0", "1,6;0,0,50;200,0,0", "1,6;0,0,500;1000,0,0"};
		foreach (string b : bad)
		{
			MUD_TrenchPlan plan = MUD_TrenchPlan.Unpack(b);
			MUD_TrenchLayout lay = new MUD_TrenchLayout();
			Check(plan != null && !lay.Build(plan), "build should refuse '" + b + "'");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! 5 cm grid over a 3-segment chain (width 1.5): PieceAt >= 0 exactly where the plain rectangle /
	//! corner-disc math says inside. Points within 5 mm of an edge are skipped (float ties).
	protected void Grid()
	{
		MUD_TrenchLayout lay = Make("3,6;0,0,0;400,0,0;400,400,0;800,600,0", "grid");
		if (!lay)
			return;
		array<vector> pts = {Vector(0, 0, 0), Vector(4, 0, 0), Vector(4, 0, 4), Vector(8, 0, 6)};
		float half = lay.Half();
		int inside;
		int bad;
		int shared;
		for (int iz = 0; iz <= 180; iz++)
		{
			for (int ix = 0; ix <= 220; ix++)
			{
				float x = -1.5 + ix * 0.05;
				float z = -1.5 + iz * 0.05;
				bool strict = NaiveIn(pts, half, -0.005, x, z);
				bool loose = NaiveIn(pts, half, 0.005, x, z);
				int got = lay.PieceAt(x, z);
				if (strict)
				{
					inside++;
					if (got < 0)
						bad++;
					if (lay.ShapesAt(x, z) > 1)
						shared++;
				}
				else if (!loose && got >= 0)
				{
					bad++;
				}
			}
		}
		Check(bad == 0, string.Format("grid: %1 of %2 inside points mismatched", bad, inside));
		Print(string.Format("MUD trench test: grid %1 points inside, %2 mismatched, %3 in 2+ shapes (owned by the first)", inside, bad, shared));
	}

	//------------------------------------------------------------------------------------------------
	//! Straight segment rectangles (grown by grow) or a disc at an inner corner
	protected bool NaiveIn(array<vector> pts, float half, float grow, float px, float pz)
	{
		for (int i = 0; i < pts.Count() - 1; i++)
		{
			vector a = pts[i];
			vector b = pts[i + 1];
			float dx = b[0] - a[0];
			float dz = b[2] - a[2];
			float len = Math.Sqrt(dx * dx + dz * dz);
			float ux = dx / len;
			float uz = dz / len;
			float rx = px - a[0];
			float rz = pz - a[2];
			float t = rx * ux + rz * uz;
			float d = -rx * uz + rz * ux;
			if (t >= -grow && t <= len + grow && Math.AbsFloat(d) <= half + grow)
				return true;
			if (i > 0 && rx * rx + rz * rz <= (half + grow) * (half + grow))
				return true;
		}
		return false;
	}
}
```

- [ ] **Step 2: Back up and hook the self-test into `MUD_WorldGround.c` Setup**

Backup: copy `Scripts/Game/Mud/MUD_WorldGround.c` to `mud_backups/archive/scripts/MUD_WorldGround.c.2026-10-09-before-trench1`.

In `Setup()`, replace

```c
		Print(string.Format("MUD world: ready, patch grid %1 x %2, fine %3 m, render %4, groove %5, edge mode %6, rpl %7",
			m_iNPX, m_iNPZ, m_fFine, m_bRender, m_bGroove, typename.EnumToString(MUD_EEdgeMode, m_eEdgeMode), m_RplComponent != null));
		return true;
```

with

```c
		Print(string.Format("MUD world: ready, patch grid %1 x %2, fine %3 m, render %4, groove %5, edge mode %6, rpl %7",
			m_iNPX, m_iNPZ, m_fFine, m_bRender, m_bGroove, typename.EnumToString(MUD_EEdgeMode, m_eEdgeMode), m_RplComponent != null));
		MUD_TrenchSelfTest.Run();
		return true;
```

No play yet: Task 2 ends the stage.

---

### Task 2: CarveTrench, heights, check and a TEMP carved trench

**Files:**
- Modify: `Scripts/Game/Mud/MUD_WorldGround.c` (new methods after `DigAt`; TEMP members; Setup; destructor)

**Interfaces:**
- Consumes: `MUD_TrenchLayout` (Task 1).
- Produces: `static int MUD_WorldGroundEntity.CarveTrench(MUD_TrenchLayout lay, int piece, int layers)` (-1 not ready, -2 nothing diggable, else verts moved); `static bool SurfaceY(float x, float z, out float y)`; `static bool AtGround(vector wp)`; `static void CheckTrench(MUD_TrenchLayout lay, array<int> layers, string tag)`.

- [ ] **Step 1: Add the trench methods to `MUD_WorldGround.c`, right after the `DigAt` method**

```c
	//------------------------------------------------------------------------------------------------
	//! Ground heights at world (x, z): dug = what is there now (bilinear over the fine verts), undug =
	//! before any deformation (skin triangles). On kept terrain both are the heightmap. False off the
	//! patch grid.
	protected bool Heights(float x, float z, out float dug, out float undug)
	{
		float fx;
		float fz;
		if (!ToFine(Vector(x, 0, z), fx, fz))
			return false;
		bool ok;
		undug = BaseAt(fx, fz, ok);
		if (!ok)
		{
			undug = GetWorld().GetSurfaceY(x, z);
			dug = undug;
			return undug > -255;
		}
		int ix = Math.ClampInt(Math.Floor(fx), 0, m_iNPX * PCELLS - 1);
		int iz = Math.ClampInt(Math.Floor(fz), 0, m_iNPZ * PCELLS - 1);
		float u = Math.Clamp(fx - ix, 0, 1);
		float w = Math.Clamp(fz - iz, 0, 1);
		float h = GetH(ix, iz) * (1 - u) * (1 - w) + GetH(ix + 1, iz) * u * (1 - w) + GetH(ix, iz + 1) * (1 - u) * w + GetH(ix + 1, iz + 1) * u * w;
		dug = undug + h;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: ground height now at world (x, z) (trench ghosts, site spawn). False = unknown.
	static bool SurfaceY(float x, float z, out float y)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		float undug;
		return s_Inst.Heights(x, z, y, undug);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a soldier whose feet are at wp stands on this ground (dug or undug surface within
	//! 1.5 m), not on a roof or bridge above it
	static bool AtGround(vector wp)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		float dug;
		float undug;
		if (!s_Inst.Heights(wp[0], wp[2], dug, undug))
			return false;
		return Math.AbsFloat(wp[1] - dug) <= 1.5 || Math.AbsFloat(wp[1] - undug) <= 1.5;
	}

	//------------------------------------------------------------------------------------------------
	//! Server (clients from Plan 3 Task 9): sink every vertex of trench piece `piece` to layers x 0.25 m
	//! under the undug ground (spec section 5). Only lowers; locked / hard verts stay; the floor caps it.
	//! \return verts moved (0 is fine: already that deep), -1 = a patch is missing / building (swing not
	//! counted, try again), -2 = nothing in the piece can be dug (road, hard ground, locked)
	static int CarveTrench(MUD_TrenchLayout lay, int piece, int layers)
	{
		if (!s_Inst || !s_Inst.m_bReady || !lay || piece < 0 || piece >= lay.Count())
			return -1;
		return s_Inst.DoCarveTrench(lay, piece, layers);
	}

	//------------------------------------------------------------------------------------------------
	//! Global fine vertex range of a trench piece's box. False if it is off the patch grid.
	protected bool TrenchRange(MUD_TrenchLayout lay, int piece, out int gx0, out int gx1, out int gz0, out int gz1)
	{
		vector lo;
		vector hi;
		lay.PieceBox(piece, lo, hi);
		gx0 = Math.ClampInt(Math.Ceil((lo[0] - m_fX0) / m_fFine), 0, m_iNPX * PCELLS);
		gx1 = Math.ClampInt(Math.Floor((hi[0] - m_fX0) / m_fFine), 0, m_iNPX * PCELLS);
		gz0 = Math.ClampInt(Math.Ceil((lo[2] - m_fZ0) / m_fFine), 0, m_iNPZ * PCELLS);
		gz1 = Math.ClampInt(Math.Floor((hi[2] - m_fZ0) / m_fFine), 0, m_iNPZ * PCELLS);
		return gx0 <= gx1 && gz0 <= gz1;
	}

	//------------------------------------------------------------------------------------------------
	protected int DoCarveTrench(MUD_TrenchLayout lay, int piece, int layers)
	{
		int gx0;
		int gx1;
		int gz0;
		int gz1;
		if (!TrenchRange(lay, piece, gx0, gx1, gz0, gz1))
			return -2;
		// a patch in range still missing or building: no swing, else a live edge sinks next to skin (a crack)
		if (!EnsureRange(gx0, gx1, gz0, gz1))
			return -1;

		float target = -MUD_TrenchPlan.LAYER * layers;
		int inside;
		int diggable;
		int moved;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				if (lay.PieceAt(m_fX0 + gx * m_fFine, m_fZ0 + gz * m_fFine) != piece)
					continue;
				inside++;
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p)
					continue;
				float lock = p.m_aLock[v];
				if (lock <= 0 || VertexSoft(gx, gz) <= 0)
					continue;
				diggable++;
				float y = Math.Max(target * lock, p.m_aFloor[v]);
				if (y < p.m_aH[v] - 0.0005)
				{
					SetH(gx, gz, y);
					moved++;
				}
			}
		}
		if (diggable == 0)
			return -2;
		if (moved > 0)
			MarkDirty(gx0, gx1, gz0, gz1);
		m_iDigs++;
		if (m_bDebug)
			Print(string.Format("MUD trench: piece %1 verts %2 diggable %3 moved %4 depth %5", piece, inside, diggable, moved, -target));
		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! Debug: every piece's verts against its target (layers[piece] x 0.25 m; 0 = not dug). One line:
	//! 'MUD trench check <tag>: inside N, at depth D, deeper K, locked L, undug U, no patch M, WRONG W,
	//! outside dug O'. WRONG = a diggable vertex shallower than its target; outside dug = a vertex in a
	//! piece box but in no piece that is below -1 cm (a crater / dig there, or a spill: should be 0 on
	//! fresh ground; may count a vertex twice where boxes overlap).
	static void CheckTrench(MUD_TrenchLayout lay, array<int> layers, string tag)
	{
		if (!s_Inst || !s_Inst.m_bReady || !lay)
			return;
		s_Inst.DoCheckTrench(lay, layers, tag);
	}

	//------------------------------------------------------------------------------------------------
	protected void DoCheckTrench(MUD_TrenchLayout lay, array<int> layers, string tag)
	{
		int inside;
		int atDepth;
		int deeper;
		int locked;
		int undug;
		int noPatch;
		int wrong;
		int outside;
		for (int piece = 0; piece < lay.Count(); piece++)
		{
			int gx0;
			int gx1;
			int gz0;
			int gz1;
			if (!TrenchRange(lay, piece, gx0, gx1, gz0, gz1))
				continue;
			int want = 0;
			if (piece < layers.Count())
				want = layers[piece];
			for (int gz = gz0; gz <= gz1; gz++)
			{
				for (int gx = gx0; gx <= gx1; gx++)
				{
					int owner = lay.PieceAt(m_fX0 + gx * m_fFine, m_fZ0 + gz * m_fFine);
					int v;
					MUD_FinePatch p = At(gx, gz, v);
					if (owner < 0)
					{
						if (p && p.m_aH[v] < -0.01)
							outside++;
						continue;
					}
					if (owner != piece)
						continue;
					inside++;
					if (want <= 0)
					{
						undug++;
						continue;
					}
					if (!p)
					{
						noPatch++;
						continue;
					}
					float lock = p.m_aLock[v];
					if (lock <= 0 || VertexSoft(gx, gz) <= 0)
					{
						locked++;
						continue;
					}
					float target = Math.Max(-MUD_TrenchPlan.LAYER * want * lock, p.m_aFloor[v]);
					float h = p.m_aH[v];
					if (Math.AbsFloat(h - target) <= 0.002)
					{
						atDepth++;
					}
					else if (h < target)
					{
						deeper++;
					}
					else
					{
						wrong++;
						if (wrong <= 3)
							Print(string.Format("MUD trench check %1: WRONG piece %2 vertex %3 %4 h %5 target %6", tag, piece, gx, gz, h, target), LogLevel.WARNING);
					}
				}
			}
		}
		Print(string.Format("MUD trench check %1: inside %2, at depth %3, deeper %4, locked %5, undug %6, no patch %7, WRONG %8, outside dug %9",
			tag, inside, atDepth, deeper, locked, undug, noPatch, wrong, outside));
	}
```

- [ ] **Step 2: Add the TEMP carve (removed in Task 3)**

Add these members right after `protected static bool s_bHidden;`:

```c
	// TEMP trench Task 2 (removed in Task 3): a fixed test trench carved at full depth once ready
	protected static const string TEMP_TRENCH = "1,6;211000,275200,0;211800,275200,0;211800,275800,150;212600,275800,0";
	protected ref MUD_TrenchLayout m_TempLay;
	protected int m_iTempPiece;
	protected int m_iTempTries;
```

Add this method right after `DoCheckTrench`:

```c
	//------------------------------------------------------------------------------------------------
	//! TEMP trench Task 2 (removed in Task 3): carve TEMP_TRENCH piece by piece at full depth, retry
	//! once a second while patches build (max 30 tries), then log the check line
	protected void TempTrenchCarve()
	{
		if (!m_TempLay)
		{
			m_TempLay = new MUD_TrenchLayout();
			if (!m_TempLay.Build(MUD_TrenchPlan.Unpack(TEMP_TRENCH)))
			{
				Print("MUD trench TEMP: plan does not build - " + m_TempLay.m_sError, LogLevel.ERROR);
				GetGame().GetCallqueue().Remove(TempTrenchCarve);
				return;
			}
			Print(string.Format("MUD trench TEMP: %1 pieces, %2 layers", m_TempLay.Count(), m_TempLay.Layers()));
		}

		while (m_iTempPiece < m_TempLay.Count())
		{
			int r = CarveTrench(m_TempLay, m_iTempPiece, m_TempLay.Layers());
			if (r == -1)
			{
				m_iTempTries++;
				if (m_iTempTries < 30)
					return;
				Print(string.Format("MUD trench TEMP: patches never ready, gave up at piece %1", m_iTempPiece), LogLevel.ERROR);
				break;
			}
			if (r == -2)
				Print(string.Format("MUD trench TEMP: piece %1 nothing diggable", m_iTempPiece));
			m_iTempPiece++;
		}
		GetGame().GetCallqueue().Remove(TempTrenchCarve);

		array<int> layers = {};
		for (int i = 0; i < m_TempLay.Count(); i++)
		{
			layers.Insert(m_TempLay.Layers());
		}
		CheckTrench(m_TempLay, layers, "TEMP");
	}
```

In `Setup()`, after `MUD_TrenchSelfTest.Run();` add:

```c
		// TEMP trench Task 2 (removed in Task 3)
		if (!IsProxy())
			GetGame().GetCallqueue().CallLater(TempTrenchCarve, 1000, true);
```

In the destructor `~MUD_WorldGroundEntity()`, after the `if (m_bExplosionHooked) ...Remove(OnExplosion);` line add:

```c
		// TEMP trench Task 2 (removed in Task 3)
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(TempTrenchCarve);
```

- [ ] **Step 3: User compiles (F7) and plays; Claude reads the log**

User: F7 in Workbench (Script Editor), then play in Arland, walk to x ~2110-2126, z ~2752-2758 (the TEMP trench), look at it, jump in and out, then stop play.

Claude reads the newest `ArmaReforgerWorkbench/logs/<run>/console.log` and checks:
- no compile error `(E)` in `MUD_Trench*` / `MUD_WorldGround.c`;
- `MUD trench test: PASS (N checks)` and the grid line (`mismatched 0`);
- `MUD trench TEMP: 23 pieces, 6 layers`; 23 `MUD trench: piece` lines (0-22), each with `depth 1.5`;
- `MUD trench check TEMP: ... WRONG 0, outside dug 0`; record the locked count (a road under the TEMP trench shows as locked > 0; 0 = no road there, road test moves to Task 4);
- stats lines: `seams mismatched 0`;
- no crash / `0xc0000374` at play stop.

User reports: floor flat across piece joins, walls steep, curve and corner look right.

Claude writes the result (with log path, line numbers, counts) to the Plan 3 ledger. If the gate fails: superpowers:systematic-debugging before any change.

---

### Task 3: Site, ghosts and 'Dig trench'; dig modes removed; rim fix

**Files:**
- Create: `Scripts/Game/Mud/MUD_TrenchGhost.c`, `Scripts/Game/Mud/MUD_TrenchSite.c`, `Prefabs/Mud/MudTrenchSite.et` (+ `.meta`), `Assets/Mud/MUD_TrenchGhost.emat`, `Assets/Mud/MUD_TrenchGhostRed.emat`, `Assets/Mud/MUD_TrenchGhostDug.emat` (+ `.meta` each)
- Rewrite: `Scripts/Game/Mud/MUD_Dig.c`; restore `Prefabs/Mud/MudDigHelper.et` from backup
- Modify: `Scripts/Game/Mud/MUD_WorldGround.c` (remove dig modes and TEMP carve, rim fix, TEMP sites)

**Interfaces:**
- Consumes: `CarveTrench`, `SurfaceY`, `AtGround`, `CheckTrench`, `IsOver` (WorldGround); `MUD_TrenchLayout`, `MUD_TrenchPlan`.
- Produces: `MUD_TrenchGhosts` (`Draw(MUD_TrenchLayout, array<int> states)`, `Clear()`, constants `HIDDEN GREEN RED DUG`); `MUD_TrenchSiteComponent` (`static Spawn(MUD_TrenchPlan, out string why)`, `static bool FindPiece(vector wp, out MUD_TrenchSiteComponent site, out int piece)`, `bool DigPiece(int)`, `int Digit(int)`, `int Layers()`, `static TempSpawn()`); `MUD_DigAction.GetActionNameScript`.

- [ ] **Step 1: Backups**

Copy to `mud_backups/archive/scripts/` with suffix `.2026-10-09-before-trench3`: `MUD_WorldGround.c`, `MUD_Dig.c`, `MudDigHelper.et`.

- [ ] **Step 2: Create `Assets/Mud/MUD_TrenchGhost.emat`**

Based on CSK's in-engine-proven `CSK_GhostSite.emat`; shipped `Assets/Editor/PlacingPreview/Preview.emat` (read from the Exp pak 2026-10-09: Additive, AlphaMul 0.3, EnableFresnelToOpacity 1) fades flat faces seen from above, so no fresnel here.

```
MatCommon {
 Color 0.3 1 0.35 1
 Cull none
 Sort overall
 SortBias 4
 CastShadow 0
 ReceiveShadow 0
 OccludeRain 0
 OccludeWetness 0
 Specular 1 1 1 1
 SpecularMul 0.2
 AlphaTest 0.003
 AlphaMul 0.5
 BlendMode Additive
 DisableVertexColor 1
 DisablePrexposure 1
}
```

- [ ] **Step 3: Create `Assets/Mud/MUD_TrenchGhostRed.emat`**

```
MatCommon {
 Color 1 0.2 0.15 1
 Cull none
 Sort overall
 SortBias 4
 CastShadow 0
 ReceiveShadow 0
 OccludeRain 0
 OccludeWetness 0
 Specular 1 1 1 1
 SpecularMul 0.2
 AlphaTest 0.003
 AlphaMul 0.5
 BlendMode Additive
 DisableVertexColor 1
 DisablePrexposure 1
}
```

- [ ] **Step 4: Create `Assets/Mud/MUD_TrenchGhostDug.emat`**

```
MatCommon {
 Color 0.3 1 0.35 1
 Cull none
 Sort overall
 SortBias 4
 CastShadow 0
 ReceiveShadow 0
 OccludeRain 0
 OccludeWetness 0
 Specular 1 1 1 1
 SpecularMul 0.2
 AlphaTest 0.003
 AlphaMul 0.2
 BlendMode Additive
 DisableVertexColor 1
 DisablePrexposure 1
}
```

- [ ] **Step 5: Create the three `.emat.meta` files**

`Assets/Mud/MUD_TrenchGhost.emat.meta`:

```
MetaFileClass {
 Name "{4D55445452454E34}Assets/Mud/MUD_TrenchGhost.emat"
 Configurations {
  EMATResourceClass PC {
  }
  EMATResourceClass XBOX_ONE : PC {
  }
  EMATResourceClass XBOX_SERIES : PC {
  }
  EMATResourceClass PS4 : PC {
  }
  EMATResourceClass PS5 : PC {
  }
  EMATResourceClass HEADLESS : PC {
  }
 }
}
```

`Assets/Mud/MUD_TrenchGhostRed.emat.meta`:

```
MetaFileClass {
 Name "{4D55445452454E35}Assets/Mud/MUD_TrenchGhostRed.emat"
 Configurations {
  EMATResourceClass PC {
  }
  EMATResourceClass XBOX_ONE : PC {
  }
  EMATResourceClass XBOX_SERIES : PC {
  }
  EMATResourceClass PS4 : PC {
  }
  EMATResourceClass PS5 : PC {
  }
  EMATResourceClass HEADLESS : PC {
  }
 }
}
```

`Assets/Mud/MUD_TrenchGhostDug.emat.meta`:

```
MetaFileClass {
 Name "{4D55445452454E36}Assets/Mud/MUD_TrenchGhostDug.emat"
 Configurations {
  EMATResourceClass PC {
  }
  EMATResourceClass XBOX_ONE : PC {
  }
  EMATResourceClass XBOX_SERIES : PC {
  }
  EMATResourceClass PS4 : PC {
  }
  EMATResourceClass PS5 : PC {
  }
  EMATResourceClass HEADLESS : PC {
  }
 }
}
```

- [ ] **Step 6: Create `Scripts/Game/Mud/MUD_TrenchGhost.c`**

```c
//! Trench ghosts (spec section 7): one see-through slab per piece, lying 2 cm over the ground on the
//! piece footprint (5 sections along it). Local entities, nothing replicated, none on a dedicated
//! server. Entities are reused between draws; each draw makes fresh meshes.
class MUD_TrenchGhosts
{
	static const ResourceName MAT_GREEN = "{4D55445452454E34}Assets/Mud/MUD_TrenchGhost.emat";
	static const ResourceName MAT_RED = "{4D55445452454E35}Assets/Mud/MUD_TrenchGhostRed.emat";
	static const ResourceName MAT_DUG = "{4D55445452454E36}Assets/Mud/MUD_TrenchGhostDug.emat";
	static const int HIDDEN = 0;
	static const int GREEN = 1;     // can be dug
	static const int RED = 2;       // skipped: kept cell (road, ...) - plan mode only
	static const int DUG = 3;       // site: some layers dug
	static const float LIFT = 0.02;

	protected ref array<IEntity> m_aEnts = {};
	protected ref array<ref Resource> m_aRes = {};  // mesh of each entity (a mesh goes with its last ref)
	protected static bool s_bWarned;

	//------------------------------------------------------------------------------------------------
	void ~MUD_TrenchGhosts()
	{
		Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! One slab per piece whose state is not HIDDEN (states[i] for piece i; missing = HIDDEN)
	void Draw(MUD_TrenchLayout lay, array<int> states)
	{
		if (!lay || RplSession.Mode() == RplMode.Dedicated)
		{
			Clear();
			return;
		}
		int used;
		for (int i = 0; i < lay.Count(); i++)
		{
			int state = HIDDEN;
			if (i < states.Count())
				state = states[i];
			if (state == HIDDEN)
				continue;
			if (DrawPiece(used, lay, i, state))
				used++;
		}
		Trim(used);
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		Trim(0);
	}

	//------------------------------------------------------------------------------------------------
	protected void Trim(int keep)
	{
		for (int i = m_aEnts.Count() - 1; i >= keep; i--)
		{
			if (m_aEnts[i])
				SCR_EntityHelper.DeleteEntityAndChildren(m_aEnts[i]);
			m_aEnts.Remove(i);
			m_aRes.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static ResourceName MaterialOf(int state)
	{
		if (state == RED)
			return MAT_RED;
		if (state == DUG)
			return MAT_DUG;
		return MAT_GREEN;
	}

	//------------------------------------------------------------------------------------------------
	protected static float GroundY(float x, float z)
	{
		float y;
		if (!MUD_WorldGroundEntity.SurfaceY(x, z, y))
			y = GetGame().GetWorld().GetSurfaceY(x, z);
		return y;
	}

	//------------------------------------------------------------------------------------------------
	protected bool DrawPiece(int slot, MUD_TrenchLayout lay, int piece, int state)
	{
		vector mid = lay.PieceMiddle(piece);
		mid[1] = GroundY(mid[0], mid[2]);

		// MeshObject takes static arrays (array<T> = compile error)
		vector verts[10];
		float uvs[20];
		for (int j = 0; j < 5; j++)
		{
			vector l = lay.PieceEdge(piece, j * 0.25, -1);
			vector r = lay.PieceEdge(piece, j * 0.25, 1);
			l[1] = GroundY(l[0], l[2]) + LIFT;
			r[1] = GroundY(r[0], r[2]) + LIFT;
			verts[j * 2] = l - mid;
			verts[j * 2 + 1] = r - mid;
			uvs[j * 4] = 0;
			uvs[j * 4 + 1] = j * 0.25;
			uvs[j * 4 + 2] = 1;
			uvs[j * 4 + 3] = j * 0.25;
		}
		int idx[24];
		for (int q = 0; q < 4; q++)
		{
			int a = q * 2;
			idx[q * 6] = a;
			idx[q * 6 + 1] = a + 2;
			idx[q * 6 + 2] = a + 1;
			idx[q * 6 + 3] = a + 1;
			idx[q * 6 + 4] = a + 2;
			idx[q * 6 + 5] = a + 3;
		}

		int numVerts[] = {10};
		int numIndices[] = {24};
		string materials[] = {MaterialOf(state)};
		Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!res || !res.GetResource())
		{
			if (!s_bWarned)
				Print(string.Format("MUD trench ghost: MeshObject.Create failed - material '%1' did not load?", MaterialOf(state)), LogLevel.ERROR);
			s_bWarned = true;
			return false;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		// indices first: UpdateVerts rebuilds normals from the triangle list
		mesh.UpdateIndices(0, idx);
		mesh.UpdateVerts(0, verts, uvs);

		IEntity ent;
		if (slot < m_aEnts.Count())
			ent = m_aEnts[slot];
		if (!ent)
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[0] = vector.Right;
			params.Transform[1] = vector.Up;
			params.Transform[2] = vector.Forward;
			params.Transform[3] = mid;
			ent = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
			if (!ent)
				return false;
			if (slot < m_aEnts.Count())
			{
				m_aEnts[slot] = ent;
			}
			else
			{
				m_aEnts.Insert(ent);
				m_aRes.Insert(null);
			}
		}
		ent.SetOrigin(mid);
		ent.SetObject(mesh, "");
		ent.Update();       // fresh bounds, else culling keeps the spawn-time ones
		m_aRes[slot] = res;
		return true;
	}
}
```

- [ ] **Step 7: Create `Scripts/Game/Mud/MUD_TrenchSite.c`**

```c
[ComponentEditorProps(category: "Mud", description: "One confirmed trench (plan mode), dug piece by piece with a shovel. Spawned by the server; needs an RplComponent.")]
class MUD_TrenchSiteComponentClass : ScriptComponentClass
{
}

//! One confirmed trench plan (spec section 6). The server owns the progress (one digit per piece:
//! layers dug, SKIPPED = red / hard / locked, never dug); every machine rebuilds the layout from the
//! plan text and shows ghosts over unfinished pieces. Deletes itself when every piece is done.
class MUD_TrenchSiteComponent : ScriptComponent
{
	static const ResourceName PREFAB = "{4D55445452454E30}Prefabs/Mud/MudTrenchSite.et";
	static const int MAX_SITES = 20;
	static const int SKIPPED = 9;

	[RplProp(onRplName: "OnPlanRpl")]
	protected string m_sPlan;           // MUD_TrenchPlan.Pack text

	[RplProp(onRplName: "OnProgressRpl")]
	protected string m_sProgress;       // one digit per piece

	protected ref MUD_TrenchLayout m_Layout;
	protected int m_iLayers;
	protected ref MUD_TrenchGhosts m_Ghosts = new MUD_TrenchGhosts();
	protected bool m_bDeleting;
	protected int m_iId;                // server: log name

	protected static ref array<MUD_TrenchSiteComponent> s_aAll = {};   // every site on this machine
	protected static int s_iNextId;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		s_aAll.RemoveItem(null);
		s_aAll.Insert(this);
		// a joining client may get the plan before init without the onRpl call
		if (!m_sPlan.IsEmpty())
		{
			Rebuild();
			UpdateGhosts();
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		m_Ghosts.Clear();
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(DeleteSite);
		s_aAll.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: spawn a site for plan. null and why when refused.
	static MUD_TrenchSiteComponent Spawn(MUD_TrenchPlan plan, out string why)
	{
		s_aAll.RemoveItem(null);
		if (s_aAll.Count() >= MAX_SITES)
		{
			why = string.Format("%1 trench sites already (max %2)", s_aAll.Count(), MAX_SITES);
			return null;
		}
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!lay.Build(plan))
		{
			why = "bad plan: " + lay.m_sError;
			return null;
		}
		Resource res = Resource.Load(PREFAB);
		if (!res || !res.IsValid())
		{
			why = "site prefab will not load: " + PREFAB;
			return null;
		}

		vector origin = plan.m_aPoints[0];
		float y;
		if (!MUD_WorldGroundEntity.SurfaceY(origin[0], origin[2], y))
			y = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
		origin[1] = y;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = origin;
		IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!ent)
		{
			why = "spawn failed";
			return null;
		}
		MUD_TrenchSiteComponent site = MUD_TrenchSiteComponent.Cast(ent.FindComponent(MUD_TrenchSiteComponent));
		if (!site)
		{
			why = "prefab has no MUD_TrenchSiteComponent";
			RplComponent.DeleteRplEntity(ent, false);
			return null;
		}
		site.SetPlan(plan.Pack());
		return site;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, right after spawn: plan text in, red pieces (middle not over a hole cell) start SKIPPED
	protected void SetPlan(string packed)
	{
		s_iNextId++;
		m_iId = s_iNextId;
		m_sPlan = packed;
		Rebuild();
		m_sProgress = "";
		int red;
		if (m_Layout)
		{
			for (int i = 0; i < m_Layout.Count(); i++)
			{
				if (MUD_WorldGroundEntity.IsOver(m_Layout.PieceMiddle(i)))
				{
					m_sProgress += "0";
				}
				else
				{
					m_sProgress += SKIPPED.ToString();
					red++;
				}
			}
		}
		Replication.BumpMe();
		int count;
		if (m_Layout)
			count = m_Layout.Count();
		Print(string.Format("MUD trench site: %1 placed %2, pieces %3, red %4, layers %5", m_iId, m_sPlan, count, red, m_iLayers));
		UpdateGhosts();
		CheckFinished();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPlanRpl()
	{
		Rebuild();
		UpdateGhosts();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnProgressRpl()
	{
		UpdateGhosts();
	}

	//------------------------------------------------------------------------------------------------
	protected void Rebuild()
	{
		m_Layout = null;
		m_iLayers = 0;
		MUD_TrenchPlan plan = MUD_TrenchPlan.Unpack(m_sPlan);
		if (!plan)
		{
			Print("MUD trench site: plan does not unpack: " + m_sPlan, LogLevel.WARNING);
			return;
		}
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!lay.Build(plan))
		{
			Print("MUD trench site: plan does not build: " + lay.m_sError, LogLevel.WARNING);
			return;
		}
		m_Layout = lay;
		m_iLayers = plan.m_iLayers;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers dug of piece i (SKIPPED for red ones and out of range)
	int Digit(int i)
	{
		if (i < 0 || i >= m_sProgress.Length())
			return SKIPPED;
		return m_sProgress.Substring(i, 1).ToInt();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetDigit(int i, int d)
	{
		int len = m_sProgress.Length();
		m_sProgress = m_sProgress.Substring(0, i) + d.ToString() + m_sProgress.Substring(i + 1, len - i - 1);
	}

	//------------------------------------------------------------------------------------------------
	int Layers()
	{
		return m_iLayers;
	}

	//------------------------------------------------------------------------------------------------
	MUD_TrenchLayout GetLayout()
	{
		return m_Layout;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDeleting()
	{
		return m_bDeleting;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: the first live site with an unfinished piece under wp (world XZ)
	static bool FindPiece(vector wp, out MUD_TrenchSiteComponent site, out int piece)
	{
		foreach (MUD_TrenchSiteComponent s : s_aAll)
		{
			if (!s || s.IsDeleting() || !s.GetLayout())
				continue;
			int p = s.GetLayout().PieceAt(wp[0], wp[2]);
			if (p < 0 || s.Digit(p) >= s.Layers())
				continue;
			site = s;
			piece = p;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one swing on piece. False = not counted (patch building, done, not the server).
	bool DigPiece(int piece)
	{
		if (m_bDeleting || !m_Layout || !Replication.IsServer())
			return false;
		int dug = Digit(piece);
		if (dug >= m_iLayers)
			return false;

		int r = MUD_WorldGroundEntity.CarveTrench(m_Layout, piece, dug + 1);
		if (r == -1)
			return false;
		if (r == -2)
		{
			SetDigit(piece, SKIPPED);
			Print(string.Format("MUD trench dig: site %1 piece %2 skipped (hard / locked ground)", m_iId, piece));
		}
		else
		{
			SetDigit(piece, dug + 1);
			Print(string.Format("MUD trench dig: site %1 piece %2 layer %3 / %4", m_iId, piece, dug + 1, m_iLayers));
		}
		Replication.BumpMe();
		UpdateGhosts();     // own RplProp changes do not call the onRpl handler here
		CheckFinished();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateGhosts()
	{
		if (!m_Layout || m_sProgress.Length() != m_Layout.Count())
		{
			m_Ghosts.Clear();
			return;
		}
		array<int> states = {};
		for (int i = 0; i < m_Layout.Count(); i++)
		{
			int dug = Digit(i);
			if (dug >= m_iLayers)
				states.Insert(MUD_TrenchGhosts.HIDDEN);
			else if (dug > 0)
				states.Insert(MUD_TrenchGhosts.DUG);
			else
				states.Insert(MUD_TrenchGhosts.GREEN);
		}
		m_Ghosts.Draw(m_Layout, states);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every piece done or skipped -> check line, delete in 0.5 s
	protected void CheckFinished()
	{
		if (m_bDeleting || !m_Layout || !Replication.IsServer())
			return;
		for (int i = 0; i < m_Layout.Count(); i++)
		{
			if (Digit(i) < m_iLayers)
				return;
		}
		m_bDeleting = true;
		array<int> layers = {};
		for (int i2 = 0; i2 < m_Layout.Count(); i2++)
		{
			int d = Digit(i2);
			if (d == SKIPPED)
				d = 0;
			layers.Insert(d);
		}
		MUD_WorldGroundEntity.CheckTrench(m_Layout, layers, string.Format("site %1", m_iId));
		Print(string.Format("MUD trench site: %1 done, deleting", m_iId));
		GetGame().GetCallqueue().CallLater(DeleteSite, 500, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteSite()
	{
		IEntity owner = GetOwner();
		if (owner && !owner.IsDeleted())
			RplComponent.DeleteRplEntity(owner, false);
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP trench Task 3 (removed in Task 4): two fixed sites next to the Task 2 test trench
	static void TempSpawn()
	{
		array<string> plans = {"1,2;211000,274200,0;211300,274200,0", "2,4;211000,274500,0;211800,274500,0;211800,275000,0"};
		foreach (string packed : plans)
		{
			string why;
			MUD_TrenchPlan plan = MUD_TrenchPlan.Unpack(packed);
			if (!plan || !Spawn(plan, why))
				Print("MUD trench site: TEMP spawn failed " + packed + " - " + why, LogLevel.ERROR);
		}
	}
}
```

- [ ] **Step 8: Create `Prefabs/Mud/MudTrenchSite.et` and its `.meta`**

`Prefabs/Mud/MudTrenchSite.et`:

```
GenericEntity {
 ID "4D55445452454E31"
 components {
  MUD_TrenchSiteComponent "{4D55445452454E32}" {
  }
  RplComponent "{4D55445452454E33}" {
  }
 }
}
```

`Prefabs/Mud/MudTrenchSite.et.meta`:

```
MetaFileClass {
 Name "{4D55445452454E30}Prefabs/Mud/MudTrenchSite.et"
 Configurations {
  EntityTemplateResourceClass PC {
  }
  EntityTemplateResourceClass XBOX_ONE : PC {
  }
  EntityTemplateResourceClass XBOX_SERIES : PC {
  }
  EntityTemplateResourceClass PS4 : PC {
  }
  EntityTemplateResourceClass PS5 : PC {
  }
  EntityTemplateResourceClass HEADLESS : PC {
  }
 }
}
```

Then register the 4 new resources with Workbench (Claude: enfusion `wb_resources` / blender-bk `register_in_workbench`; if neither works, the user restarts Workbench) and confirm `Resource.Load` sees them in Step 13's log (no 'site prefab will not load', no ghost material ERROR).

- [ ] **Step 9: Restore `Prefabs/Mud/MudDigHelper.et`**

Copy `mud_backups/archive/scripts/MudDigHelper.et.2026-10-09-before-digmodes` over `Prefabs/Mud/MudDigHelper.et` (one `MUD_DigAction` 'Dig', no `m_eMode`).

- [ ] **Step 10: Rewrite `Scripts/Game/Mud/MUD_Dig.c`**

The pre-dig-modes file plus the trench label and the trench branch in the RPC:

```c
//! Shovel digging on mud ground. Same idea as the shipped foliage cutting (SCR_CampaignBuildingGadgetToolComponent
//! moves Prefabs/Systems/Destruction/FoliageCutHelper.et to the bush, the helper carries the action):
//! while the entrenching tool is in hand and the spot in front of the soldier is mud, a LOCAL helper
//! entity (MudDigHelper.et) sits there and offers a looping "Dig" action. Each loop plays the shipped
//! shovel animation (CMD_Item_Action arg 1, as SCR_CampaignBuildingBuildUserAction) and asks the server
//! for one swing; the server digs at its own idea of the spot, and the ground syncs like ruts do.
//! Over an unfinished trench piece (MUD_TrenchSiteComponent) the same action reads 'Dig trench' and
//! the server digs one layer of that piece instead.

//------------------------------------------------------------------------------------------------
class MUD_Dig
{
	static const ResourceName HELPER = "{01EE55B2D1E71522}Prefabs/Mud/MudDigHelper.et";
	static const float REACH = 1.0;         // dig this far in front of the soldier's feet (m)
	static const float MIN_GAP_MS = 1000;   // server: at most one swing per player this often

	protected static IEntity s_Helper;

	//------------------------------------------------------------------------------------------------
	//! Where character c digs: REACH in front of its feet, flat
	static vector DigPoint(notnull IEntity c)
	{
		vector m[4];
		c.GetWorldTransform(m);
		vector fwd = m[2];
		fwd[1] = 0;
		fwd.Normalize();
		return m[3] + fwd * REACH;
	}

	//------------------------------------------------------------------------------------------------
	//! True if c holds an entrenching tool in hand (and, with notPlacing, is not placing an item with it)
	static bool HoldsShovel(IEntity c, bool notPlacing)
	{
		SCR_GadgetManagerComponent gm = SCR_GadgetManagerComponent.GetGadgetManager(c);
		if (!gm)
			return false;

		SCR_CampaignBuildingGadgetToolComponent tool = SCR_CampaignBuildingGadgetToolComponent.Cast(gm.GetHeldGadgetComponent());
		if (!tool || tool.GetMode() != EGadgetMode.IN_HAND)
			return false;

		return !notPlacing || !tool.GetIsPlacementModeEnabled();
	}

	//------------------------------------------------------------------------------------------------
	//! Local player only: keep the helper in front of c while that spot is mud, else remove it
	static void UpdateHelper(ChimeraCharacter c)
	{
		if (!c || c != SCR_PlayerController.GetLocalControlledEntity() || c.IsInVehicle())
		{
			RemoveHelper();
			return;
		}

		vector wp = DigPoint(c);
		if (!MUD_GroundEntity.IsOnAnyGround(wp) && !MUD_WorldGroundEntity.IsOver(wp))
		{
			RemoveHelper();
			return;
		}

		if (!s_Helper)
		{
			Resource res = Resource.Load(HELPER);
			if (!res || !res.IsValid())
				return;
			s_Helper = GetGame().SpawnEntityPrefabLocal(res, c.GetWorld(), null);
			if (!s_Helper)
				return;
		}

		s_Helper.SetOrigin(wp);
		s_Helper.Update();
	}

	//------------------------------------------------------------------------------------------------
	static void RemoveHelper()
	{
		if (s_Helper)
			SCR_EntityHelper.DeleteEntityAndChildren(s_Helper);
		s_Helper = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Looping "Dig" on MudDigHelper.et: one server swing per loop (Duration < 0 in the prefab)
class MUD_DigAction : SCR_ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(user);
		if (!c || c.IsInVehicle() || !MUD_Dig.HoldsShovel(c, true))
			return false;

		return super.CanBeShownScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(user);
		if (!c)
			return false;

		CharacterControllerComponent cc = c.GetCharacterController();
		if (cc && !IsInProgress() && !cc.CanUseItem())
			return false;

		return MUD_Dig.HoldsShovel(c, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Over an unfinished trench piece: 'Dig trench (layers dug / layers)'
	override bool GetActionNameScript(out string outName)
	{
		MUD_TrenchSiteComponent site;
		int piece;
		if (!MUD_TrenchSiteComponent.FindPiece(GetOwner().GetOrigin(), site, piece))
			return false;
		outName = string.Format("Dig trench (%1 / %2)", site.Digit(piece), site.Layers());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionStart(IEntity pUserEntity)
	{
		super.OnActionStart(pUserEntity);

		ChimeraCharacter c = ChimeraCharacter.Cast(pUserEntity);
		if (!c)
			return;
		CharacterControllerComponent cc = c.GetCharacterController();
		SCR_GadgetManagerComponent gm = SCR_GadgetManagerComponent.GetGadgetManager(c);
		if (!cc || !gm)
			return;

		// shipped shovel loop, same parameters as SCR_CampaignBuildingBuildUserAction.OnActionStart
		ItemUseParameters params = new ItemUseParameters();
		params.SetEntity(gm.GetHeldGadget());
		params.SetAllowMovementDuringAction(false);
		params.SetKeepInHandAfterSuccess(true);
		params.SetCommandID(cc.GetAnimationComponent().BindCommand("CMD_Item_Action"));
		params.SetCommandIntArg(1);
		cc.TryUseItemOverrideParams(params);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformContinuousAction(IEntity pOwnerEntity, IEntity pUserEntity, float timeSlice)
	{
		if (LoopActionUpdate(timeSlice))
			PerformAction(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity())
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.MUD_RequestDig();
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionCanceled(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		super.OnActionCanceled(pOwnerEntity, pUserEntity);
		StopAnimation(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override void OnConfirmed(IEntity pUserEntity)
	{
		StopAnimation(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void StopAnimation(IEntity user)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(user);
		if (!c)
			return;
		CharacterControllerComponent cc = c.GetCharacterController();
		if (!cc || !cc.GetAnimationComponent())
			return;
		CharacterCommandHandlerComponent handler = cc.GetAnimationComponent().GetCommandHandler();
		if (handler)
			handler.FinishItemUse(true);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_CampaignBuildingGadgetToolComponent
{
	//------------------------------------------------------------------------------------------------
	override void Update(float timeSlice)
	{
		super.Update(timeSlice);

		if (MUD_Dig.HoldsShovel(m_CharacterOwner, true))
			MUD_Dig.UpdateHelper(m_CharacterOwner);
		else
			MUD_Dig.RemoveHelper();
	}

	//------------------------------------------------------------------------------------------------
	override void ToolToInventory()
	{
		super.ToolToInventory();
		MUD_Dig.RemoveHelper();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	protected float m_fMudLastDig = -100000;

	//------------------------------------------------------------------------------------------------
	//! Owner client: ask the server for one shovel swing in front of the controlled character
	void MUD_RequestDig()
	{
		if (Replication.IsServer())
			MUD_RpcAsk_Dig();
		else
			Rpc(MUD_RpcAsk_Dig);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_Dig()
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(GetControlledEntity());
		if (!c || c.IsInVehicle())
			return;

		float now = GetGame().GetWorld().GetWorldTime();
		if (now - m_fMudLastDig < MUD_Dig.MIN_GAP_MS)
			return;

		if (!MUD_Dig.HoldsShovel(c, false))
		{
			Print("MUD dig: refused, server sees no entrenching tool in hand", LogLevel.WARNING);
			return;
		}

		m_fMudLastDig = now;
		vector dp = MUD_Dig.DigPoint(c);

		// an unfinished trench piece there takes the swing (one layer of that piece)
		MUD_TrenchSiteComponent site;
		int piece;
		if (MUD_TrenchSiteComponent.FindPiece(dp, site, piece))
		{
			if (!MUD_WorldGroundEntity.AtGround(dp))
				Print(string.Format("MUD trench dig: refused at %1, soldier not on the ground there", dp));
			else if (!site.DigPiece(piece))
				Print(string.Format("MUD trench dig: swing not counted at %1 (patch building)", dp));
			return;
		}

		if (!MUD_GroundEntity.DigAt(dp) && !MUD_WorldGroundEntity.DigAt(dp))
			Print(string.Format("MUD dig: nothing dug at %1 (not mud, hard ground, locked or patch not ready)", dp));
	}
}
```

- [ ] **Step 11: Remove the dig modes and the TEMP carve from `MUD_WorldGround.c`; rim fix; TEMP sites**

(a) Delete the two attributes:

```c
	[Attribute("5", UIWidgets.EditBox, "Shovel 'Dig fast' (debug): one swing digs this many times Shovel Step", category: "Digging")]
	protected float m_fDigFastMul;

	[Attribute("0.5", UIWidgets.EditBox, "Shovel 'Square off': half side of the square made flat at the deepest dug level (m, snapped to the 0.25 m grid). Walls follow the map's north / east grid.", category: "Digging")]
	protected float m_fSquareHalf;

```

(b) Delete the `IsDebug()` method with its comment and divider:

```c
	//------------------------------------------------------------------------------------------------
	//! Any machine: debug on (shows the 'Dig fast' shovel action)
	static bool IsDebug()
	{
		return s_Inst && s_Inst.m_bDebug;
	}

```

(c) Replace `DigAt` with:

```c
	//------------------------------------------------------------------------------------------------
	//! Server: one shovel swing at world wp. False if not on the world ground or refused (hard, locked).
	static bool DigAt(vector wp)
	{
		if (!s_Inst || !s_Inst.m_bReady || s_Inst.IsProxy())
			return false;
		return s_Inst.Dig(wp, s_Inst.m_fDigStep);
	}
```

(d) Rim fix: in `ShovelSpot`, replace

```c
		// soldier not on this ground (on a roof above it, ...): ignore
		return Math.AbsFloat(wp[1] - surf) <= 1.5;
```

with

```c
		// soldier not on this ground (on a roof above it, ...): ignore. Against the undug surface too:
		// from the rim of a 1.5 m hole the dug surface is > 1.5 m below the feet (proven 2026-10-09 22:16)
		float undug = surf;
		if (cp)
			undug = cp.m_aBase[cv];
		return Math.AbsFloat(wp[1] - surf) <= 1.5 || Math.AbsFloat(wp[1] - undug) <= 1.5;
```

(e) Delete the whole `Square(vector wp)` method: from its divider line and the comment `//! 'Square off': every vertex in a square around the dig spot ...` down to its closing `}` (the line before the divider of `OnExplosion`).

(f) Delete the TEMP members (`TEMP_TRENCH`, `m_TempLay`, `m_iTempPiece`, `m_iTempTries`), the `TempTrenchCarve` method, and the destructor's TEMP lines. In `Setup()` replace

```c
		// TEMP trench Task 2 (removed in Task 3)
		if (!IsProxy())
			GetGame().GetCallqueue().CallLater(TempTrenchCarve, 1000, true);
```

with

```c
		// TEMP trench Task 3 (removed in Task 4)
		if (!IsProxy())
			GetGame().GetCallqueue().CallLater(TempTrenchSites, 3000, false);
```

and add this method after `DoCheckTrench`:

```c
	//------------------------------------------------------------------------------------------------
	//! TEMP trench Task 3 (removed in Task 4)
	protected void TempTrenchSites()
	{
		MUD_TrenchSiteComponent.TempSpawn();
	}
```

and in the destructor, after the `OnExplosion` remove line:

```c
		// TEMP trench Task 3 (removed in Task 4)
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(TempTrenchSites);
```

(g) Grep the addon for `MUD_EDigMode`, `IsDebug(`, `m_fDigFastMul`, `m_fSquareHalf`, `Square(`, `TempTrenchCarve`: expected 0 hits outside backups and docs.

- [ ] **Step 12: User compiles (F7) and plays; Claude reads the log**

User, shovel in hand:
(a) Site 1 (x 2110-2113, z 2742, 3 pieces, 2 layers): dig every piece to the end, counting swings; the ghost goes faint after the first layer and disappears when a piece is done; the site deletes itself at the end.
(b) Site 2 (width 1.0, 4 layers, corner at 2118 / 2745): dig the LAST piece first (near 2118 / 2750), then one in the middle, then stand on the rim of a dug piece and dig the next one (rim fix).
(c) Away from the sites: plain 'Dig' still makes a round hole; dig it 1.5 m deep and keep digging from the rim (rim fix for plain Dig).
(d) Stop play with site 2 unfinished.

Claude checks the log:
- compile clean; self-test PASS; `MUD trench site: 1 placed ... pieces 3, red 0, layers 2` and `2 placed ... pieces 13`;
- `MUD trench dig: site S piece N layer L / M` lines = swings the user made over trench pieces (plus `swing not counted` lines, which must match user-noticed lost swings);
- site 1: 6 dig lines, then `MUD trench check site 1: ... WRONG 0, outside dug 0`, `site 1 done, deleting`;
- a done piece never gets another dig line; site 2 pieces dug in the user's order;
- no 'refused ... soldier not on the ground' while standing on the rim; plain `MUD world: dig at` lines continue from the rim;
- no ghost material ERROR, no crash at play stop.

Claude records results in the ledger. Failures: superpowers:systematic-debugging first.

---

### Task 3b: Free dig replaces the round hole (added 2026-10-10, user idea; done in code, see ledger)

Shovel in hand off a site: a green ghost of one trench piece (1 m along the facing x the plan-mode width, default 1.0 m) lies 1 m in front; 'Dig' digs that square one 0.25 m layer deeper (1 + the fewest layers its diggable verts already have, max 1.5 m). Client sends the packed square (MUD_Dig.FreePlan); server checks shape, 2.5 m range and AtGround, then MUD_WorldGroundEntity.FreeDig. Task 4's planner sets MUD_Dig.s_iWidth when X changes width. Code lives in MUD_Dig.c / MUD_WorldGround.c (backups *.2026-10-10-before-freedig).

- [ ] **Step 1: User compiles (F7) and plays; Claude reads the log**

(a) Free dig on fresh ground 6 swings: ghost follows the facing; 6 'MUD free dig' lines layer 1..6, then 'already 1.5 m deep'. (b) Turn 90 deg next to the hole and dig: the new square starts at layer 1. (c) Over the unfinished site 2 pieces: no free ghost, 'Dig trench' as before. (d) No crash at play stop.

---

### Task 4: Plan mode (keys, planner, place RPC); TEMP removed

**Files:**
- Create: `Configs/System/chimeraInputCommon.conf`, `Scripts/Game/Mud/MUD_TrenchPlanner.c`
- Modify: `Scripts/Game/Mud/MUD_Dig.c` (R hook, helper hidden while planning), `Scripts/Game/Mud/MUD_TrenchSite.c` (remove `TempSpawn`), `Scripts/Game/Mud/MUD_WorldGround.c` (remove TEMP sites)

**Interfaces:**
- Consumes: `MUD_TrenchPlan`, `MUD_TrenchLayout`, `MUD_TrenchGhosts`, `MUD_TrenchSiteComponent.Spawn`, `MUD_Dig.HoldsShovel`, `MUD_WorldGroundEntity.IsOver`.
- Produces: `MUD_TrenchPlanner` (`Start()`, `Stop()`, `Update(float)`); `SCR_PlayerController.MUD_IsTrenchPlanning()`, `MUD_StartTrenchPlanning()`, `MUD_StopTrenchPlanning()`, `MUD_AskPlaceTrench(string)`.

- [ ] **Step 1: Backups**

Copy `MUD_WorldGround.c`, `MUD_Dig.c`, `MUD_TrenchSite.c` to `mud_backups/archive/scripts/<name>.2026-10-09-before-trench4`.

- [ ] **Step 2: Create `Configs/System/chimeraInputCommon.conf`**

```
ActionManager {
 Actions {
  Action MUD_TrenchPoint {
   InputSource InputSourceSum "{4D55445450430001}" {
    Sources {
     InputSourceValue "{4D55445450430002}" {
      FilterPreset "click"
      Input "mouse:button0"
      Filter InputFilterClick "{4D55445450430003}" {
      }
     }
    }
   }
  }
  Action MUD_TrenchUndo {
   InputSource InputSourceSum "{4D55445450430011}" {
    Sources {
     InputSourceValue "{4D55445450430012}" {
      FilterPreset "click"
      Input "mouse:button1"
      Filter InputFilterClick "{4D55445450430013}" {
      }
     }
    }
   }
  }
  Action MUD_TrenchDepth {
   Type AnalogRelative
   InputSource InputSourceSum "{4D55445450430021}" {
    Sources {
     InputSourceValue "{4D55445450430022}" {
      Input "mouse:wheel"
      Filter InputFilterValue "{4D55445450430023}" {
      }
     }
    }
   }
  }
  Action MUD_TrenchWidth {
   InputSource InputSourceSum "{4D55445450430031}" {
    Sources {
     InputSourceValue "{4D55445450430032}" {
      FilterPreset "click"
      Input "keyboard:KC_X"
      Filter InputFilterClick "{4D55445450430033}" {
      }
     }
    }
   }
  }
  Action MUD_TrenchBend {
   InputSource InputSourceSum "{4D55445450430041}" {
    Sources {
     InputSourceValue "{4D55445450430042}" {
      FilterPreset "click"
      Input "keyboard:KC_V"
      Filter InputFilterClick "{4D55445450430043}" {
      }
     }
    }
   }
  }
  Action MUD_TrenchConfirm {
   InputSource InputSourceSum "{4D55445450430051}" {
    Sources {
     InputSourceValue "{4D55445450430052}" {
      FilterPreset "click"
      Input "keyboard:KC_F"
      Filter InputFilterClick "{4D55445450430053}" {
      }
     }
    }
   }
  }
 }
 Contexts {
  ActionContext MUD_TrenchPlanContext {
   Priority 46
   Flags 0xa 0
   ActionRefs {
    "MUD_TrenchPoint"
    "MUD_TrenchUndo"
    "MUD_TrenchDepth"
    "MUD_TrenchWidth"
    "MUD_TrenchBend"
    "MUD_TrenchConfirm"
   }
  }
 }
}
```

- [ ] **Step 3: Create `Scripts/Game/Mud/MUD_TrenchPlanner.c`**

```c
//! Trench plan mode (spec sections 3-4; pattern of CSK_SandbagWallPlanner Start / Stop / Update /
//! TraceAim / MoveBend). Local player only, nothing is sent before F. Shovel in hand, R toggles.
//! Keys: Configs/System/chimeraInputCommon.conf (MUD_TrenchPlanContext).
class MUD_TrenchPlanner
{
	static const string CONTEXT = "MUD_TrenchPlanContext";
	static const float AIM_RANGE = 15;          // m, camera trace length
	static const float AIM_MOVE_REDRAW = 0.05;  // m the aim must move before the live ghost is redrawn
	static const float BEND_SNAP = 0.15;        // m: a smaller bend snaps back to straight
	static const float BEND_PICK = 3;           // m from a segment's middle that V picks it

	protected ref MUD_TrenchPlan m_Plan = new MUD_TrenchPlan();
	protected ref MUD_TrenchGhosts m_Placed = new MUD_TrenchGhosts();    // the planned segments
	protected ref MUD_TrenchGhosts m_Live = new MUD_TrenchGhosts();      // last point -> aim (or the aim marker)
	protected bool m_bHasAim;
	protected vector m_vAim;
	protected vector m_vDrawnAim;
	protected bool m_bDirty = true;
	protected bool m_bFinished;
	protected bool m_bListening;
	protected int m_iBending = -1;      // segment whose bend follows the aim (V), -1 = none
	protected float m_fBendFrom;        // its bend before, for RMB
	protected int m_iPieces;
	protected int m_iRed;
	protected string m_sNote;
	protected string m_sShownHint;

	//------------------------------------------------------------------------------------------------
	void ~MUD_TrenchPlanner()
	{
		RemoveListeners();
	}

	//------------------------------------------------------------------------------------------------
	bool Start()
	{
		MUD_TrenchSelfTest.Run();
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return false;
		input.AddActionListener("MUD_TrenchPoint", EActionTrigger.DOWN, OnPoint);
		input.AddActionListener("MUD_TrenchUndo", EActionTrigger.DOWN, OnUndo);
		input.AddActionListener("MUD_TrenchWidth", EActionTrigger.DOWN, OnWidth);
		input.AddActionListener("MUD_TrenchBend", EActionTrigger.DOWN, OnBend);
		input.AddActionListener("MUD_TrenchConfirm", EActionTrigger.DOWN, OnConfirm);
		m_bListening = true;
		Print("MUD trench plan: start");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Stop()
	{
		RemoveListeners();
		m_Placed.Clear();
		m_Live.Clear();
		SCR_HintManagerComponent.ClearLatestHint();
		Print("MUD trench plan: stop");
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveListeners()
	{
		if (!m_bListening)
			return;
		m_bListening = false;
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;
		input.RemoveActionListener("MUD_TrenchPoint", EActionTrigger.DOWN, OnPoint);
		input.RemoveActionListener("MUD_TrenchUndo", EActionTrigger.DOWN, OnUndo);
		input.RemoveActionListener("MUD_TrenchWidth", EActionTrigger.DOWN, OnWidth);
		input.RemoveActionListener("MUD_TrenchBend", EActionTrigger.DOWN, OnBend);
		input.RemoveActionListener("MUD_TrenchConfirm", EActionTrigger.DOWN, OnConfirm);
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while planning. False = plan mode ends (confirmed, shovel gone, no player).
	bool Update(float timeSlice)
	{
		if (m_bFinished)
			return false;
		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!player || player.IsInVehicle() || !MUD_Dig.HoldsShovel(player, true))
			return false;

		InputManager input = GetGame().GetInputManager();
		input.ActivateContext(CONTEXT);
		m_bHasAim = TraceAim(m_vAim);

		// mouse wheel: one notch = one 0.25 m layer, the whole trench
		float wheel = input.GetActionValue("MUD_TrenchDepth");
		if (wheel != 0)
		{
			int delta = 1;
			if (wheel < 0)
				delta = -1;
			m_Plan.m_iLayers = Math.ClampInt(m_Plan.m_iLayers + delta, MUD_TrenchPlan.MIN_LAYERS, MUD_TrenchPlan.MAX_LAYERS);
			m_bDirty = true;
		}

		if (m_bHasAim && vector.Distance(m_vAim, m_vDrawnAim) > AIM_MOVE_REDRAW)
		{
			m_vDrawnAim = m_vAim;
			if (m_iBending >= 0)
			{
				MoveBend();
				m_bDirty = true;
			}
			else
			{
				DrawLive();
			}
		}

		if (m_bDirty)
		{
			m_bDirty = false;
			DrawPlaced();
			DrawLive();
		}
		UpdateHint();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Ground point under the crosshair (camera ray, then straight down if the ray hits nothing).
	//! Copy of CSK_SandbagWallPlanner.TraceAim (vanilla SCR_ItemPlacementComponent trace setup).
	protected bool TraceAim(out vector aim)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		CameraManager cameras = GetGame().GetCameraManager();
		if (!character || !cameras || !cameras.CurrentCamera())
			return false;

		vector cameraMat[4];
		cameras.CurrentCamera().GetTransform(cameraMat);
		BaseWorld world = character.GetWorld();

		TraceParam param = new TraceParam();
		param.Start = character.EyePosition();
		param.End = param.Start + cameraMat[2] * AIM_RANGE;
		param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		param.Exclude = character;
		param.LayerMask = EPhysicsLayerPresets.Projectile;
		float hit = world.TraceMove(param, null);
		aim = param.Start + (param.End - param.Start) * hit;

		if (hit >= 1)
		{
			param.Start = param.End + vector.Up;
			param.End = param.Start - vector.Up * 20;
			hit = world.TraceMove(param, null);
			if (hit >= 1)
				return false;

			aim = param.Start + (param.End - param.Start) * hit;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The aim, pulled back to MAX_SEGMENT from `from` (flat) when it is farther
	protected vector ClampAim(vector from)
	{
		vector d = m_vAim - from;
		d[1] = 0;
		float len = d.Length();
		if (len <= MUD_TrenchPlan.MAX_SEGMENT)
			return m_vAim;
		vector p = from + d * (MUD_TrenchPlan.MAX_SEGMENT / len);
		p[1] = m_vAim[1];
		return p;
	}

	//------------------------------------------------------------------------------------------------
	//! Green where the piece middle is over a hole cell, red elsewhere (kept: road, ...). Returns red count.
	protected int States(MUD_TrenchLayout lay, array<int> states)
	{
		int red;
		for (int i = 0; i < lay.Count(); i++)
		{
			if (MUD_WorldGroundEntity.IsOver(lay.PieceMiddle(i)))
			{
				states.Insert(MUD_TrenchGhosts.GREEN);
			}
			else
			{
				states.Insert(MUD_TrenchGhosts.RED);
				red++;
			}
		}
		return red;
	}

	//------------------------------------------------------------------------------------------------
	//! The planned segments, built from the packed text (what the server gets), so ghost = dug
	protected void DrawPlaced()
	{
		m_iPieces = 0;
		m_iRed = 0;
		if (m_Plan.m_aPoints.Count() < 2)
		{
			m_Placed.Clear();
			return;
		}
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!lay.Build(MUD_TrenchPlan.Unpack(m_Plan.Pack())))
		{
			m_Placed.Clear();
			SetNote("Plan: " + lay.m_sError);
			return;
		}
		array<int> states = {};
		m_iRed = States(lay, states);
		m_iPieces = lay.Count();
		m_Placed.Draw(lay, states);
	}

	//------------------------------------------------------------------------------------------------
	//! Last point -> aim (clamped to 12 m); with no point yet a 0.5 m marker across the aim
	protected void DrawLive()
	{
		int n = m_Plan.m_aPoints.Count();
		if (!m_bHasAim || m_iBending >= 0 || n >= MUD_TrenchPlan.MAX_POINTS)
		{
			m_Live.Clear();
			return;
		}
		MUD_TrenchPlan live = new MUD_TrenchPlan();
		live.m_iWidth = m_Plan.m_iWidth;
		live.m_iLayers = m_Plan.m_iLayers;
		if (n == 0)
		{
			live.AddPoint(m_vAim - Vector(0.25, 0, 0));
			live.AddPoint(m_vAim + Vector(0.25, 0, 0));
		}
		else
		{
			live.AddPoint(m_Plan.m_aPoints[n - 1]);
			live.AddPoint(ClampAim(m_Plan.m_aPoints[n - 1]));
		}
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!lay.Build(MUD_TrenchPlan.Unpack(live.Pack())))
		{
			m_Live.Clear();
			return;
		}
		array<int> states = {};
		States(lay, states);
		m_Live.Draw(lay, states);
	}

	//------------------------------------------------------------------------------------------------
	//! LMB: place a point at the aim (bending: keep the bend)
	protected void OnPoint(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_iBending >= 0)
		{
			m_iBending = -1;
			m_bDirty = true;
			return;
		}
		if (!m_bHasAim)
			return;
		int n = m_Plan.m_aPoints.Count();
		if (n >= MUD_TrenchPlan.MAX_POINTS)
		{
			SetNote(string.Format("At most %1 points", MUD_TrenchPlan.MAX_POINTS));
			return;
		}
		vector p = m_vAim;
		if (n > 0)
		{
			p = ClampAim(m_Plan.m_aPoints[n - 1]);
			if (vector.DistanceSqXZ(p, m_Plan.m_aPoints[n - 1]) < MUD_TrenchPlan.MIN_SEGMENT * MUD_TrenchPlan.MIN_SEGMENT)
			{
				SetNote("Too close to the last point");
				return;
			}
		}
		m_Plan.AddPoint(p);
		SetNote("");
	}

	//------------------------------------------------------------------------------------------------
	//! RMB: undo the last point (bending: put the bend back)
	protected void OnUndo(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_iBending >= 0)
		{
			m_Plan.m_aBends[m_iBending] = m_fBendFrom;
			m_iBending = -1;
			m_bDirty = true;
			return;
		}
		int n = m_Plan.m_aPoints.Count();
		if (n == 0)
			return;
		m_Plan.m_aPoints.Remove(n - 1);
		m_Plan.m_aBends.Remove(n - 1);
		// the segment that ended at the removed point is gone: its bend (stored on the point before) too
		if (n >= 2)
			m_Plan.m_aBends[n - 2] = 0;
		SetNote("");
	}

	//------------------------------------------------------------------------------------------------
	//! X: next width (whole trench)
	protected void OnWidth(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		m_Plan.m_iWidth = (m_Plan.m_iWidth + 1) % MUD_TrenchPlan.WIDTHS;
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! V: bend the segment nearest the aim (its middle follows the aim); V / LMB keep, RMB undo
	protected void OnBend(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_iBending >= 0)
		{
			m_iBending = -1;
			m_bDirty = true;
			return;
		}
		int seg = NearestSegment();
		if (seg < 0)
		{
			SetNote(string.Format("V: aim near the middle of a segment of %1 m or more", MUD_TrenchPlan.MIN_BEND_CHORD));
			return;
		}
		m_iBending = seg;
		m_fBendFrom = m_Plan.m_aBends[seg];
		SetNote("");
	}

	//------------------------------------------------------------------------------------------------
	//! Segment (chord >= MIN_BEND_CHORD) whose chord middle is nearest the aim, within BEND_PICK; -1 = none
	protected int NearestSegment()
	{
		if (!m_bHasAim)
			return -1;
		int best = -1;
		float bestSq = BEND_PICK * BEND_PICK;
		for (int i = 0; i < m_Plan.m_aPoints.Count() - 1; i++)
		{
			vector a = m_Plan.m_aPoints[i];
			vector b = m_Plan.m_aPoints[i + 1];
			if (vector.DistanceSqXZ(a, b) < MUD_TrenchPlan.MIN_BEND_CHORD * MUD_TrenchPlan.MIN_BEND_CHORD)
				continue;
			float dSq = vector.DistanceSqXZ((a + b) * 0.5, m_vAim);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = i;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Bent segment follows the aim: its middle goes as far off the chord as the aim is
	//! (CSK_SandbagWallPlanner.MoveBend)
	protected void MoveBend()
	{
		if (m_iBending < 0 || m_iBending >= m_Plan.m_aPoints.Count() - 1)
			return;
		vector a = m_Plan.m_aPoints[m_iBending];
		vector b = m_Plan.m_aPoints[m_iBending + 1];
		a[1] = 0;
		b[1] = 0;
		vector chord = b - a;
		float len = chord.Length();
		if (len < 0.001)
			return;
		vector right = chord * (1 / len);
		vector fwd = Vector(-right[2], 0, right[0]);
		vector rel = m_vAim - (a + b) * 0.5;
		rel[1] = 0;
		float maxBend = len * 0.5 * MUD_TrenchPlan.MAX_BEND;
		float bend = Math.Clamp(vector.Dot(rel, fwd), -maxBend, maxBend);
		if (Math.AbsFloat(bend) < BEND_SNAP)
			bend = 0;
		m_Plan.m_aBends[m_iBending] = bend;
	}

	//------------------------------------------------------------------------------------------------
	//! F: send the plan to the server once, leave plan mode
	protected void OnConfirm(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_Plan.m_aPoints.Count() < 2)
		{
			SetNote("Place at least 2 points");
			return;
		}
		string packed = m_Plan.Pack();
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!lay.Build(MUD_TrenchPlan.Unpack(packed)))
		{
			SetNote("Plan: " + lay.m_sError);
			return;
		}
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;
		array<int> states = {};
		int red = States(lay, states);
		Print(string.Format("MUD trench plan: confirm %1, pieces %2, red %3", packed, lay.Count(), red));
		pc.MUD_AskPlaceTrench(packed);
		m_bFinished = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetNote(string note)
	{
		m_sNote = note;
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHint()
	{
		string mouse;
		if (m_iBending >= 0)
			mouse = string.Format("Bending segment %1: move the aim to the side\nLMB / V: keep\nRMB: put it back", m_iBending + 1);
		else
			mouse = string.Format("LMB: place point (%1/%2)\nRMB: undo last point\nV: bend the segment under the aim", m_Plan.m_aPoints.Count(), MUD_TrenchPlan.MAX_POINTS);

		string red;
		if (m_iRed > 0)
			red = string.Format(", %1 red (road / hard ground: not dug)", m_iRed);

		string text = string.Format("%1\nWheel: depth %2 m\nX: width %3 m\nF: confirm, then dig it with the shovel\nR: leave\n\nPieces: %4%5\n%6",
			mouse, m_Plan.m_iLayers * MUD_TrenchPlan.LAYER, m_Plan.Width(), m_iPieces, red, m_sNote);
		if (text == m_sShownHint)
			return;
		m_sShownHint = text;
		SCR_HintManagerComponent.ShowCustomHint(text, "Trench", 3600, true);
	}
}

//------------------------------------------------------------------------------------------------
//! Trench plan mode: the planner lives on the local player controller; the confirmed plan goes to
//! the server once (the controller is owned by its client, so it can send server RPCs)
modded class SCR_PlayerController
{
	static const float MUD_PLACE_RANGE = 60;      // m: every plan point this near the player (server check)
	static const float MUD_PLACE_GAP_MS = 2000;   // server: one placed trench per player this often
	static const int MUD_PLAN_MAX_CHARS = 400;

	protected ref MUD_TrenchPlanner m_MUD_Planner;    // local player only, while plan mode is on
	protected float m_fMudLastPlace = -100000;

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(float timeSlice)
	{
		super.OnUpdate(timeSlice);

		if (m_MUD_Planner && !m_MUD_Planner.Update(timeSlice))
			MUD_StopTrenchPlanning();
	}

	//------------------------------------------------------------------------------------------------
	bool MUD_IsTrenchPlanning()
	{
		return m_MUD_Planner != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Local player: shovel R with nothing deployable to place (MUD_Dig.c gadget hook)
	void MUD_StartTrenchPlanning()
	{
		if (this != GetGame().GetPlayerController() || m_MUD_Planner)
			return;
		MUD_TrenchPlanner planner = new MUD_TrenchPlanner();
		if (!planner.Start())
			return;
		m_MUD_Planner = planner;
		MUD_Dig.RemoveHelper();
	}

	//------------------------------------------------------------------------------------------------
	void MUD_StopTrenchPlanning()
	{
		if (!m_MUD_Planner)
			return;
		m_MUD_Planner.Stop();
		m_MUD_Planner = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Owner client: place a confirmed plan (MUD_TrenchPlan.Pack text)
	void MUD_AskPlaceTrench(string packed)
	{
		if (Replication.IsServer())
			MUD_RpcAsk_PlaceTrench(packed);
		else
			Rpc(MUD_RpcAsk_PlaceTrench, packed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_PlaceTrench(string packed)
	{
		ChimeraCharacter c = ChimeraCharacter.Cast(GetControlledEntity());
		if (!c || c.IsInVehicle())
			return;

		float now = GetGame().GetWorld().GetWorldTime();
		if (now - m_fMudLastPlace < MUD_PLACE_GAP_MS)
		{
			Print("MUD trench site: refused, placed one less than 2 s ago", LogLevel.WARNING);
			return;
		}
		if (!MUD_Dig.HoldsShovel(c, false))
		{
			Print("MUD trench site: refused, server sees no entrenching tool in hand", LogLevel.WARNING);
			return;
		}
		if (packed.Length() > MUD_PLAN_MAX_CHARS)
		{
			Print(string.Format("MUD trench site: refused, plan text %1 chars", packed.Length()), LogLevel.WARNING);
			return;
		}
		MUD_TrenchPlan plan = MUD_TrenchPlan.Unpack(packed);
		if (!plan)
		{
			Print("MUD trench site: refused, plan does not unpack: " + packed, LogLevel.WARNING);
			return;
		}
		vector feet = c.GetOrigin();
		foreach (vector p : plan.m_aPoints)
		{
			if (vector.DistanceSqXZ(p, feet) > MUD_PLACE_RANGE * MUD_PLACE_RANGE)
			{
				Print(string.Format("MUD trench site: refused, point %1 more than %2 m from the player", p, MUD_PLACE_RANGE), LogLevel.WARNING);
				return;
			}
		}

		m_fMudLastPlace = now;
		string why;
		if (!MUD_TrenchSiteComponent.Spawn(plan, why))
			Print("MUD trench site: refused, " + why, LogLevel.WARNING);
	}
}
```

- [ ] **Step 4: R hook and helper hidden while planning in `MUD_Dig.c`**

Replace the modded `SCR_CampaignBuildingGadgetToolComponent` class with:

```c
//------------------------------------------------------------------------------------------------
//! The shovel's activate key (R, vanilla "GadgetActivate") opens and closes trench plan mode (as
//! CSK_BuildingGadgetTool.c). Vanilla deployable placement keeps the key while it is on, or when
//! the player carries something it can place. No dig helper while planning (F = confirm there).
modded class SCR_CampaignBuildingGadgetToolComponent
{
	//------------------------------------------------------------------------------------------------
	override void Update(float timeSlice)
	{
		super.Update(timeSlice);

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		bool planning = pc && pc.MUD_IsTrenchPlanning();
		if (!planning && MUD_Dig.HoldsShovel(m_CharacterOwner, true))
			MUD_Dig.UpdateHelper(m_CharacterOwner);
		else
			MUD_Dig.RemoveHelper();
	}

	//------------------------------------------------------------------------------------------------
	override void ToolToInventory()
	{
		super.ToolToInventory();
		MUD_Dig.RemoveHelper();
	}

	//------------------------------------------------------------------------------------------------
	override protected void ActivateAction()
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
		{
			super.ActivateAction();
			return;
		}

		if (pc.MUD_IsTrenchPlanning())
		{
			pc.MUD_StopTrenchPlanning();
			return;
		}

		if (m_bPlacementModeEnabled || FindDeployableItems(null, true) > 0)
		{
			super.ActivateAction();
			return;
		}

		pc.MUD_StartTrenchPlanning();
	}
}
```

- [ ] **Step 5: Remove the TEMP sites**

`MUD_TrenchSite.c`: delete the `TempSpawn()` method with its divider and comment.
`MUD_WorldGround.c`: delete the `TempTrenchSites()` method, the Setup lines

```c
		// TEMP trench Task 3 (removed in Task 4)
		if (!IsProxy())
			GetGame().GetCallqueue().CallLater(TempTrenchSites, 3000, false);
```

and the destructor lines

```c
		// TEMP trench Task 3 (removed in Task 4)
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(TempTrenchSites);
```

Grep the addon for `TEMP trench`, `TempSpawn`, `TempTrenchSites`: expected 0 hits outside backups and docs.

- [ ] **Step 6: User compiles (F7) and plays; Claude reads the log**

User, shovel in hand, R to open plan mode:
(a) Plan a 3-point trench with one bent segment (V), change width (X) and depth (wheel), RMB-undo a point and place it again, F to confirm. The ghost after F must match the ghost before F.
(b) Dig two pieces fully, then stop the plan here and dig the rest later or leave it.
(c) Plan a trench across a road (start in a field, cross the road, end in the field): road pieces red in plan mode; after F they have no ghost and no 'Dig trench'; dig the field pieces next to the road.
(d) Plan a second trench crossing the first; dig the crossing piece from both.
(e) Open plan mode, place 2 points, then switch to a rifle: plan mode closes, ghosts go; R again with the shovel starts empty.
(f) Stop play with a site and plan mode open.

Claude checks the log:
- compile clean, self-test PASS;
- each `MUD trench plan: confirm <text>, pieces N, red R` is followed by `MUD trench site: S placed <same text>, pieces N, red R` (same text, same counts);
- dig line count = user swings; road trench: `MUD trench check site S` after completion shows `locked > 0` or red pieces, `WRONG 0`; crossing: no WRONG;
- `MUD trench plan: start` / `stop` pairs match R presses and the weapon switch;
- no crash at play stop, no ghost ERROR.

User reports: ghost before F = dug trench after (spec gate 'what the ghost shows = what gets dug'). Claude records results in the ledger.

---

### Task 5: Results doc, adversarial review, back to Plan 3

**Files:**
- Create: `docs/superpowers/results/2026-10-09-trench-plan-mode.md`
- Modify: `.superpowers/sdd/2026-10-09-mud-whole-map-holes-plan3-ground/progress.md`, `docs/superpowers/plans/2026-10-09-mud-whole-map-holes-plan3-ground.md` (Task 7 marked superseded by this plan; Task 9 / 10 notes: sync site progress, JIP carves each piece's layer count)

- [ ] **Step 1: Results doc**

Per stage: log path, line ranges, the counted lines (self-test checks, piece lines, dig lines vs swings, check lines with all counts), each with a checksum (e.g. 'dig lines 6 = swings 6'). Recount from the raw log, not from the ledger.

- [ ] **Step 2: Adversarial review**

Run engineering-skills:adversarial-reviewer on `MUD_TrenchLayout.c`, `MUD_TrenchGhost.c`, `MUD_TrenchSite.c`, `MUD_TrenchPlanner.c`, `MUD_Dig.c` and the trench / rim parts of `MUD_WorldGround.c`. Each finding gets a ruling line in the ledger (`Trench: Ruling: ... - cost if wrong: ...`); fixes go through a compile + play like any stage.

- [ ] **Step 3: Plan 3 hand-back**

In the Plan 3 plan file, under Task 7, add one line: 'SUPERSEDED 2026-10-09 by docs/superpowers/plans/2026-10-09-mud-trench-plan-mode.md (done <date>)'. Under Task 9 add: 'Also sync MUD_TrenchSiteComponent: clients carve each piece to its replicated layer count (CarveTrench on proxies).' Under Task 10 add: 'JIP: a joining client carves every live site's pieces to their current layer counts.' Then continue Plan 3 at Task 8.

## As built (2026-10-10, user-approved in play, Exp 192624)

Changes made during play testing; these override the text above where they differ.
- Free dig square = one trench piece, 1 x 1 m (FREE_LENGTH 1.0, width idx 1, reach 0.75, server range 2.5 m).
- Free square snaps to the 0.25 m grid, 45 deg turn steps.
- Sloped walls ~60 deg (SLOPE_RUN 0.58) for free dig and trenches; a piece never slopes into a neighbour piece of the same trench.
- R on the shovel cycles off -> Dig -> Fill -> Plan -> off (hint each step); no ghost/helper when off, except over an unfinished site piece.
- Fill: one 0.25 m layer per swing, refused over a site piece and on old ground.
- LMB in plan mode needs Configs/System/chimeraInputCommon.conf.meta to carry the vanilla GUID {795184CF9AD764DB}, then a Workbench restart.
- TEMP: MUD_TrenchSiteComponent.DEBUG_INSTANT = true (F digs the whole trench). Set false before release.
