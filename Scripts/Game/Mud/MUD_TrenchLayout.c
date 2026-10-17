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
	static const int MAX_LAYERS = 10;         // 2.5 m (user 2026-10-13; was 6 = 1.5 m). Shovel free dig goes as deep.
	static const int DEFAULT_LAYERS = 6;      // a new plan starts at 1.5 m; the mouse wheel goes to MAX_LAYERS
	static const int WIDTHS = 4;
	static const float SLOPE_RUN = 0.58;      // m the wall leans out per m of depth (~60 deg; user 2026-10-10 00:21)

	int m_iWidth = 1;                         // WidthOf: 0.5 / 1.0 / 1.5 / 2.0 m (floor width)
	int m_iLayers = DEFAULT_LAYERS;
	ref array<vector> m_aPoints = {};
	ref array<float> m_aBends = {};           // one per point; the last one is unused (0)

	//------------------------------------------------------------------------------------------------
	static float WidthOf(int index)
	{
		// multiples of the 0.25 m ground grid: the floor is this wide (user 2026-10-09 23:50; 0.75 dug a
		// 0.5 m floor, verts only at -0.25 / 0 / +0.25)
		if (index == 0)
			return 0.5;
		if (index == 2)
			return 1.5;
		if (index == 3)
			return 2.0;
		return 1.0;
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
	static const float END_EPS = 0.001;       // m of float slack at the chain's start and end
	static const float SIDE_EPS = 0.002;      // m of float slack at the side walls

	string m_sError;                          // why Build failed
	string m_sPlan;                           // Pack text of the plan Build got (voxel ground ops carry it, TEMP 2026-10-13)
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

		m_sPlan = plan.Pack();
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
				// start = the previous piece's end (the same float: no gap / overlap between them)
				piece.m_fX0 = -seg.m_fLength * 0.5;
				if (k > 0)
					piece.m_fX0 = m_aPieces[m_aPieces.Count() - 1].m_fX1;
				piece.m_fX1 = -seg.m_fLength * 0.5 + (k + 1) * step;
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
	//! How far world (x, z) lies outside piece index's own shape (0 = inside): to its span of the
	//! segment box (along the centre line, across the side) or to its corner disc, whichever is nearer.
	//! The sloped wall uses it (CarveTrench).
	float OutsideDist(int index, float px, float pz)
	{
		MUD_TrenchPiece piece = m_aPieces[index];
		MUD_TrenchSeg seg = m_aSegs[piece.m_iSeg];
		float x;
		float side;
		OnSeg(seg, px, pz, x, side);
		float dx = 0;
		if (x < piece.m_fX0)
			dx = piece.m_fX0 - x;
		else if (x > piece.m_fX1)
			dx = x - piece.m_fX1;
		float ds = Math.AbsFloat(side) - m_fHalf;
		if (ds < 0)
			ds = 0;
		float d = Math.Sqrt(dx * dx + ds * ds);
		if (HasDisc(index))
		{
			float ex = px - seg.m_vEnd[0];
			float ez = pz - seg.m_vEnd[2];
			float dd = Math.Sqrt(ex * ex + ez * ez) - m_fHalf;
			if (dd < 0)
				dd = 0;
			if (dd < d)
				d = dd;
		}
		return d;
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 6 (parapet, 2026-10-17): how far world (x, z) lies outside piece index's SIDE wall, -1 when it
	//! is not beside the piece (along the centre line past its span - past the chain's ends, or beside another piece).
	//! A segment's last piece before a corner also takes its corner disc, so a bend's outer side has no gap.
	float SideDist(int index, float px, float pz)
	{
		MUD_TrenchPiece piece = m_aPieces[index];
		MUD_TrenchSeg seg = m_aSegs[piece.m_iSeg];
		float x;
		float side;
		OnSeg(seg, px, pz, x, side);
		float d = -1;
		if (x >= piece.m_fX0 && x <= piece.m_fX1)
			d = Math.Max(0, Math.AbsFloat(side) - m_fHalf);
		if (HasDisc(index))
		{
			float ex = px - seg.m_vEnd[0];
			float ez = pz - seg.m_vEnd[2];
			float dd = Math.Max(0, Math.Sqrt(ex * ex + ez * ez) - m_fHalf);
			if (d < 0 || dd < d)
				d = dd;
		}
		return d;
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
		// the chain's two ends get END_EPS: the end point itself must be inside despite float error in
		// OnSeg (self-test 2026-10-09 23:48: (10.4, 0) on a 10.4 m trench came out past the end)
		float x0 = piece.m_fX0;
		if (index == 0)
			x0 -= END_EPS;
		// SIDE_EPS: a vertex exactly on the edge (half width is a grid multiple) is inside despite float error
		if (Math.AbsFloat(side) <= m_fHalf + SIDE_EPS && x >= x0)
		{
			if (x < piece.m_fX1)
				return true;
			if (index == m_aPieces.Count() - 1 && x <= piece.m_fX1 + END_EPS)
				return true;
		}

		if (!HasDisc(index))
			return false;
		float dx = px - seg.m_vEnd[0];
		float dz = pz - seg.m_vEnd[2];
		return dx * dx + dz * dz <= (m_fHalf + SIDE_EPS) * (m_fHalf + SIDE_EPS);
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
	protected void CheckDist(MUD_TrenchLayout lay, int piece, float x, float z, float want, string what)
	{
		float got = lay.OutsideDist(piece, x, z);
		Check(Math.AbsFloat(got - want) < 0.002, string.Format("%1: piece %2 (%3, %4) outside %5, want %6", what, piece, x, z, got, want));
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
	//! 10 m along +x, width 1.0: 10 pieces of 1 m; edges, ends and sides
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
		CheckAt(lay, 5.5, 0.5, 5, "straight side edge");
		CheckAt(lay, 5.5, -0.5, 5, "straight side edge");
		CheckAt(lay, 5.5, 0.52, -1, "straight side out");
		CheckDist(lay, 5, 5, 0, 0, "straight inside");
		CheckDist(lay, 5, 5, 1.0, 0.5, "straight side");
		CheckDist(lay, 0, -1, 0, 1.0, "straight before start");
		CheckDist(lay, 0, -1, 1.5, 1.41421, "straight corner");
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
		CheckAt(lay, 4.4, -0.4, -1, "corner outside disc");
		CheckAt(lay, 3.9, 0.2, 3, "corner overlap owned by earlier piece");
		CheckDist(lay, 3, 4.6, 0, 0.1, "corner disc");
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
		CheckAt(lay, 5, 2.45, 5, "curve side in");
		CheckAt(lay, 5, 2.55, -1, "curve side out");
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

		array<string> garbage = {"", "1,6", "1,6;0,0,0", "x,6;0,0,0;100,0,0", "1,11;0,0,0;100,0,0", "4,6;0,0,0;100,0,0",
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
	//! 5 cm grid over a 3-segment chain (width 2.0): PieceAt >= 0 exactly where the plain rectangle /
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
					{
						bad++;
						if (bad <= 5)
							Print(string.Format("MUD trench test: grid (%1, %2) inside, PieceAt -1", x, z), LogLevel.WARNING);
					}
					if (lay.ShapesAt(x, z) > 1)
						shared++;
				}
				else if (!loose && got >= 0)
				{
					bad++;
					if (bad <= 5)
						Print(string.Format("MUD trench test: grid (%1, %2) outside, PieceAt %3", x, z, got), LogLevel.WARNING);
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
