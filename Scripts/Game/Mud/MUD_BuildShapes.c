//! Trench builder F4, round 2 (user 2026-10-24: "like Paint - pre shapes you attach together, or free draw"): the
//! trench shapes of the manager's palette. A shape is made from one mouse drag (start -> end) and turns into one or
//! more strokes (centre lines: points + bends, the MUD_TrenchPlan form); ToPlans cuts the strokes into plan texts
//! (<= MAX_POINTS points, segments MIN_SEGMENT..MAX_SEGMENT) that the manager adds as normal TRENCH parts. The palette
//! icons are drawn from the same strokes. Pure geometry: no entities, no net.

//------------------------------------------------------------------------------------------------
//! One centre line: points + one bend per point (bend of the segment that starts there; the last one unused)
class MUD_BuildStroke
{
	ref array<vector> m_aPts = {};
	ref array<float> m_aBends = {};

	//------------------------------------------------------------------------------------------------
	void Add(vector p, float bend = 0)
	{
		p[1] = 0;
		m_aPts.Insert(p);
		m_aBends.Insert(bend);
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_aPts.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The centre line as a plain polyline (arcs sampled, perSeg points per curved segment)
	void Polyline(array<vector> outPts, int perSeg = 6)
	{
		outPts.Clear();
		int n = m_aPts.Count();
		if (n == 0)
			return;
		outPts.Insert(m_aPts[0]);
		for (int i = 0; i < n - 1; i++)
		{
			array<vector> arc = {};
			MUD_BuildShapes.ArcPoints(m_aPts[i], m_aPts[i + 1], m_aBends[i], perSeg, arc);
			for (int k = 1; k < arc.Count(); k++)
			{
				outPts.Insert(arc[k]);
			}
		}
	}
}

//------------------------------------------------------------------------------------------------
class MUD_BuildShapes
{
	static const int POINTS = 0;        // click points (the first builder's trench tool)
	static const int PENCIL = 1;        // hold LMB and draw
	static const int LINE = 2;
	static const int ZIGZAG = 3;        // traverse zigzag
	static const int BAYS = 4;          // fire bays: square wave (crenellated fire trench)
	static const int ARC = 5;           // half circle, bulges left of the drag
	static const int RECT = 6;
	static const int RING = 7;          // circle / ellipse
	static const int SHAPE_L = 8;
	static const int SHAPE_U = 9;
	static const int SHAPE_T = 10;
	static const int CROSS = 11;
	static const int FOXHOLE = 12;      // stamps (round 2d): one click, square to the trench clicked beside (MUD_BuildManager.StampParts)
	static const int MG_NEST = 13;
	static const int RAMP = 14;
	static const int SHAPES = 15;

	static const float MIN_SIZE = 1;          // m: a drag shorter than this (or a box side) makes nothing
	static const float ZIG_STEP = 5;          // m along the drag per zig
	static const float ZIG_AMP = 3;           // m to the side
	static const float BAY_LEN = 8;           // m fire bay
	static const float BAY_AMP = 3;           // m traverse step
	static const float CURVE_CHORD = 7;       // m chord of a curve piece (>= MIN_BEND_CHORD, < MAX_SEGMENT)
	static const float SNAP_DEG = 15;         // Shift: line drags snap to this angle
	static const float PENCIL_STEP = 1;       // m between recorded pencil points
	static const float PENCIL_TOL = 0.4;      // m the simplified pencil line may leave the drawn one

	//------------------------------------------------------------------------------------------------
	static string Name(int shape)
	{
		array<string> names = {"Points", "Pencil", "Line", "Zigzag", "Fire bays", "Arc", "Rectangle", "Ring", "L", "U", "T", "Cross", "Foxhole", "MG nest", "Entrance ramp"};
		if (shape < 0 || shape >= names.Count())
			return "?";
		return names[shape];
	}

	//------------------------------------------------------------------------------------------------
	//! Box shapes: the drag is one corner -> the opposite corner (frame turned by the shape angle, R key)
	static bool IsBox(int shape)
	{
		return shape >= RECT && shape <= CROSS;
	}

	//------------------------------------------------------------------------------------------------
	//! Stamps: placed by a click (or a drag that sets the facing), built by the manager, not by Make
	static bool IsStamp(int shape)
	{
		return shape >= FOXHOLE;
	}

	//------------------------------------------------------------------------------------------------
	//! Line shapes that bow to one side (R flips them)
	static bool CanFlip(int shape)
	{
		return shape == ZIGZAG || shape == BAYS || shape == ARC;
	}

	//------------------------------------------------------------------------------------------------
	//! Flat left normal of dir (MUD_TrenchSeg.m_vFwd: a positive bend bows this way)
	static vector Left(vector dir)
	{
		return Vector(-dir[2], 0, dir[0]);
	}

	//------------------------------------------------------------------------------------------------
	//! e turned round s to the nearest SNAP_DEG step (Shift on line drags)
	static vector SnapAngle(vector s, vector e)
	{
		vector d = Vector(e[0] - s[0], 0, e[2] - s[2]);
		float len = d.Length();
		if (len < 0.001)
			return e;
		float a = Math.Atan2(d[2], d[0]) * Math.RAD2DEG;
		a = Math.Round(a / SNAP_DEG) * SNAP_DEG * Math.DEG2RAD;
		return Vector(s[0] + Math.Cos(a) * len, 0, s[2] + Math.Sin(a) * len);
	}

	//------------------------------------------------------------------------------------------------
	//! Strokes of shape for a drag s -> e. Box shapes use a frame turned angle deg; square = Shift (box: equal
	//! sides; line shapes: SNAP_DEG steps). Nothing when the drag is too small or the shape is POINTS / PENCIL.
	static void Make(int shape, vector s, vector e, float angle, bool square, array<ref MUD_BuildStroke> strokes, bool flip = false)
	{
		strokes.Clear();
		s[1] = 0;
		e[1] = 0;
		if (IsBox(shape))
		{
			MakeBox(shape, s, e, angle, square, strokes);
			return;
		}
		if (square)
			e = SnapAngle(s, e);
		vector d = e - s;
		float len = d.Length();
		if (len < MIN_SIZE || shape == POINTS || shape == PENCIL || IsStamp(shape))
			return;
		vector r = d * (1 / len);
		vector n = Left(r);
		if (flip)
			n = n * -1;     // R on a zigzag / fire bays / arc: the other side of the drag
		MUD_BuildStroke st = new MUD_BuildStroke();
		if (shape == LINE)
		{
			st.Add(s);
			st.Add(e);
		}
		else if (shape == ZIGZAG)
		{
			int zigs = Math.Max(2, 2 * Math.Round(len / (2 * ZIG_STEP)));     // even: both ends on the drag line
			float step = len / zigs;
			for (int k = 0; k <= zigs; k++)
			{
				float off = 0;
				if (k % 2 == 1)
					off = ZIG_AMP;
				st.Add(s + r * (k * step) + n * off);
			}
		}
		else if (shape == BAYS)
		{
			int bays = Math.Max(1, Math.Round(len / BAY_LEN));
			float bay = len / bays;
			st.Add(s);
			for (int i = 0; i < bays; i++)
			{
				float side = 0;
				if (i % 2 == 1)
					side = BAY_AMP;
				st.Add(s + r * ((i + 1) * bay) + n * side);
				if (i < bays - 1)
				{
					float next = BAY_AMP - side;
					st.Add(s + r * ((i + 1) * bay) + n * next);
				}
			}
		}
		else if (shape == ARC)
		{
			// t = 0 at s, t = PI at e, bulging to the left of the drag
			Ellipse((s + e) * 0.5, r * -1, n, len * 0.5, len * 0.5, 0, Math.PI, 3, st);
		}
		if (st.Count() >= 2)
			strokes.Insert(st);
	}

	//------------------------------------------------------------------------------------------------
	protected static void MakeBox(int shape, vector s, vector e, float angle, bool square, array<ref MUD_BuildStroke> strokes)
	{
		float a = angle * Math.DEG2RAD;
		vector ax = Vector(Math.Cos(a), 0, Math.Sin(a));
		vector az = Left(ax);
		float dx = vector.Dot(e - s, ax);
		float dz = vector.Dot(e - s, az);
		if (square)
		{
			float m = Math.Max(Math.AbsFloat(dx), Math.AbsFloat(dz));
			dx = m * Sign(dx);
			dz = m * Sign(dz);
		}
		if (Math.AbsFloat(dx) < MIN_SIZE || Math.AbsFloat(dz) < MIN_SIZE)
			return;
		vector ux = ax * dx;
		vector uz = az * dz;
		MUD_BuildStroke st = new MUD_BuildStroke();
		if (shape == RECT)
		{
			st.Add(s);
			st.Add(s + ux);
			st.Add(s + ux + uz);
			st.Add(s + uz);
			st.Add(s);
		}
		else if (shape == RING)
		{
			Ellipse(s + (ux + uz) * 0.5, ax, az, Math.AbsFloat(dx) * 0.5, Math.AbsFloat(dz) * 0.5, 0, Math.PI * 2, 6, st);
		}
		else if (shape == SHAPE_L)
		{
			st.Add(s + uz);
			st.Add(s);
			st.Add(s + ux);
		}
		else if (shape == SHAPE_U)
		{
			st.Add(s + uz);
			st.Add(s);
			st.Add(s + ux);
			st.Add(s + ux + uz);
		}
		else if (shape == SHAPE_T)
		{
			st.Add(s + uz);
			st.Add(s + ux + uz);
			MUD_BuildStroke stem = new MUD_BuildStroke();
			stem.Add(s + ux * 0.5 + uz);     // starts ON the bar: a junction
			stem.Add(s + ux * 0.5);
			strokes.Insert(stem);
		}
		else if (shape == CROSS)
		{
			st.Add(s + uz * 0.5);
			st.Add(s + ux + uz * 0.5);
			MUD_BuildStroke st2 = new MUD_BuildStroke();
			st2.Add(s + ux * 0.5);
			st2.Add(s + ux * 0.5 + uz);
			strokes.Insert(st2);
		}
		if (st.Count() >= 2)
			strokes.InsertAt(st, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected static float Sign(float v)
	{
		if (v < 0)
			return -1;
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Points of c + axA * ra * cos t + axB * rb * sin t for t0..t1, cut into arcs of ~CURVE_CHORD (at least minSegs);
	//! each segment's bend = how far the curve's middle lies off its chord (exact for a circle)
	static void Ellipse(vector c, vector axA, vector axB, float ra, float rb, float t0, float t1, int minSegs, MUD_BuildStroke st)
	{
		float arcLen = (t1 - t0) * (ra + rb) * 0.5;
		int segs = Math.Max(minSegs, Math.Ceil(arcLen / CURVE_CHORD));
		for (int k = 0; k <= segs; k++)
		{
			float t = t0 + (t1 - t0) * k / segs;
			vector p = c + axA * (ra * Math.Cos(t)) + axB * (rb * Math.Sin(t));
			float bend = 0;
			if (k < segs)
			{
				float tm = t0 + (t1 - t0) * (k + 0.5) / segs;
				float tn = t0 + (t1 - t0) * (k + 1) / segs;
				vector pm = c + axA * (ra * Math.Cos(tm)) + axB * (rb * Math.Sin(tm));
				vector pn = c + axA * (ra * Math.Cos(tn)) + axB * (rb * Math.Sin(tn));
				vector chord = pn - p;
				chord[1] = 0;
				float len = chord.Length();
				if (len > 0.001)
					bend = vector.Dot(pm - (p + pn) * 0.5, Left(chord * (1 / len)));
			}
			st.Add(p, bend);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! a -> b as points (a and b included); a bent segment is the circle arc through its bow point, n pieces
	static void ArcPoints(vector a, vector b, float bend, int n, array<vector> outPts)
	{
		outPts.Clear();
		vector chord = b - a;
		chord[1] = 0;
		float len = chord.Length();
		if (Math.AbsFloat(bend) < 0.01 || len < 0.01 || n < 2)
		{
			outPts.Insert(a);
			outPts.Insert(b);
			return;
		}
		vector fwd = Left(chord * (1 / len));
		vector mid = (a + b) * 0.5;
		vector bow = mid + fwd * bend;
		float h = len * 0.5;
		float radius = (h * h + bend * bend) / (2 * Math.AbsFloat(bend));
		vector c = bow - fwd * (Sign(bend) * radius);
		float angA = Math.Atan2(a[2] - c[2], a[0] - c[0]);
		float angM = Math.Atan2(bow[2] - c[2], bow[0] - c[0]);
		float half = angM - angA;
		while (half > Math.PI)
			half -= Math.PI * 2;
		while (half < -Math.PI)
			half += Math.PI * 2;
		for (int k = 0; k <= n; k++)
		{
			float ang = angA + 2 * half * k / n;
			outPts.Insert(Vector(c[0] + Math.Cos(ang) * radius, 0, c[2] + Math.Sin(ang) * radius));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pencil: the drawn points thinned (Douglas-Peucker, PENCIL_TOL) into one stroke of straight segments
	static void Pencil(array<vector> drawn, array<ref MUD_BuildStroke> strokes)
	{
		strokes.Clear();
		int n = drawn.Count();
		if (n < 2)
			return;
		array<bool> keep = {};
		for (int i = 0; i < n; i++)
		{
			keep.Insert(i == 0 || i == n - 1);
		}
		Thin(drawn, 0, n - 1, keep);
		MUD_BuildStroke st = new MUD_BuildStroke();
		for (int j = 0; j < n; j++)
		{
			if (keep[j])
				st.Add(drawn[j]);
		}
		if (st.Count() >= 2)
			strokes.Insert(st);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Thin(array<vector> pts, int i0, int i1, array<bool> keep)
	{
		if (i1 - i0 < 2)
			return;
		vector a = pts[i0];
		vector b = pts[i1];
		vector ab = b - a;
		ab[1] = 0;
		float len = ab.Length();
		int far = -1;
		float farD = PENCIL_TOL;
		for (int i = i0 + 1; i < i1; i++)
		{
			vector ap = pts[i] - a;
			ap[1] = 0;
			float d;
			if (len < 0.001)
				d = ap.Length();
			else
				d = Math.AbsFloat(vector.Dot(ap, Left(ab * (1 / len))));
			if (d > farD)
			{
				farD = d;
				far = i;
			}
		}
		if (far < 0)
			return;
		keep[far] = true;
		Thin(pts, i0, far, keep);
		Thin(pts, far, i1, keep);
	}

	//------------------------------------------------------------------------------------------------
	//! Strokes -> plan texts with the given width / depth / walls. Long straight segments are split, too-short
	//! ones merged, bends kept only where the plan allows them, and every MAX_POINTS points a new line starts where
	//! the last ended. False (why says) when a piece still fails MUD_TrenchLayout.Build.
	static bool ToPlans(array<ref MUD_BuildStroke> strokes, int width, int layers, int wall, array<string> texts, out string why)
	{
		texts.Clear();
		foreach (MUD_BuildStroke src : strokes)
		{
			MUD_BuildStroke st = Clean(src);
			int n = st.Count();
			if (n < 2)
				continue;
			int first = 0;
			while (first < n - 1)
			{
				int last = Math.Min(n - 1, first + MUD_TrenchPlan.MAX_POINTS - 1);
				MUD_TrenchPlan plan = new MUD_TrenchPlan();
				plan.m_iWidth = width;
				plan.m_iLayers = layers;
				plan.m_iWall = wall;
				for (int i = first; i <= last; i++)
				{
					float bend = 0;
					if (i < last)
						bend = st.m_aBends[i];
					plan.AddPoint(st.m_aPts[i], bend);
				}
				string text = plan.Pack();
				MUD_TrenchLayout lay = new MUD_TrenchLayout();
				if (!lay.Build(MUD_TrenchPlan.Unpack(text)))
				{
					why = lay.m_sError;
					return false;
				}
				texts.Insert(text);
				first = last;
			}
		}
		if (texts.IsEmpty())
		{
			why = "drag a bigger shape";
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A stroke the plan limits accept: segments split under MAX_SEGMENT, points closer than MIN_SEGMENT merged,
	//! bends only on chords >= MIN_BEND_CHORD and inside MAX_BEND
	protected static MUD_BuildStroke Clean(MUD_BuildStroke src)
	{
		MUD_BuildStroke res = new MUD_BuildStroke();
		int n = src.Count();
		if (n == 0)
			return res;
		float maxSeg = MUD_TrenchPlan.MAX_SEGMENT - 0.5;
		res.Add(src.m_aPts[0]);
		for (int i = 0; i < n - 1; i++)
		{
			vector a = src.m_aPts[i];
			vector b = src.m_aPts[i + 1];
			float bend = src.m_aBends[i];
			vector prev = res.m_aPts[res.Count() - 1];
			float len = vector.DistanceXZ(prev, b);     // the segment really made (prev = a unless a was merged)
			if (vector.DistanceXZ(prev, b) < MUD_TrenchPlan.MIN_SEGMENT)
			{
				if (i == n - 2 && res.Count() >= 2)
				{
					res.m_aPts[res.Count() - 1] = b;     // keep the true end (a closed shape meets its start)
					res.m_aBends[res.Count() - 2] = 0;   // that segment's end moved: its bend may no longer fit
				}
				continue;
			}
			if (vector.DistanceXZ(prev, a) > 0.01)
				bend = 0;                               // a was merged away: this segment starts elsewhere
			if (len > maxSeg)
			{
				if (bend != 0)
				{
					array<vector> arc = {};
					int pieces = Math.Ceil(len / maxSeg) * 2;
					ArcPoints(prev, b, bend, pieces, arc);
					for (int k = 1; k < arc.Count(); k++)
					{
						res.Add(arc[k]);
					}
					continue;
				}
				int parts = Math.Ceil(len / maxSeg);
				for (int p = 1; p <= parts; p++)
				{
					res.Add(prev + (b - prev) * (p * 1.0 / parts));     // p / parts alone is int division
				}
				continue;
			}
			float half = vector.DistanceXZ(prev, b) * 0.5;
			if (half * 2 < MUD_TrenchPlan.MIN_BEND_CHORD)
				bend = 0;
			bend = Math.Clamp(bend, -half * MUD_TrenchPlan.MAX_BEND * 0.99, half * MUD_TrenchPlan.MAX_BEND * 0.99);
			res.m_aBends[res.Count() - 1] = bend;
			res.Add(b);
		}
		return res;
	}

	//------------------------------------------------------------------------------------------------
	//! The palette icon of a shape: its strokes for a set drag (x right, z up), as polylines
	static void IconLines(int shape, array<ref array<vector>> lines)
	{
		lines.Clear();
		array<ref MUD_BuildStroke> strokes = {};
		if (shape == POINTS)
		{
			MUD_BuildStroke pts = new MUD_BuildStroke();
			pts.Add(Vector(0, 0, 2));
			pts.Add(Vector(7, 0, 12));
			pts.Add(Vector(13, 0, 4));
			pts.Add(Vector(20, 0, 10));
			strokes.Insert(pts);
		}
		else if (shape == PENCIL)
		{
			MUD_BuildStroke sq = new MUD_BuildStroke();
			Ellipse(Vector(5, 0, 7), Vector(1, 0, 0), Vector(0, 0, 1), 5, 5, Math.PI, 0, 3, sq);
			MUD_BuildStroke sq2 = new MUD_BuildStroke();
			Ellipse(Vector(15, 0, 7), Vector(1, 0, 0), Vector(0, 0, 1), 5, 5, Math.PI, Math.PI * 2, 3, sq2);
			strokes.Insert(sq);
			strokes.Insert(sq2);
		}
		else if (IsBox(shape))
		{
			Make(shape, Vector(0, 0, 0), Vector(20, 0, 14), 0, false, strokes);
		}
		else if (shape == ARC)
		{
			Make(shape, Vector(0, 0, 2), Vector(20, 0, 2), 0, false, strokes);
		}
		else if (shape == FOXHOLE)
		{
			MUD_BuildStroke trench = new MUD_BuildStroke();
			trench.Add(Vector(0, 0, 0));
			trench.Add(Vector(20, 0, 0));
			MUD_BuildStroke hole = new MUD_BuildStroke();
			hole.Add(Vector(10, 0, 0));
			hole.Add(Vector(10, 0, 6));
			MUD_BuildStroke bar = new MUD_BuildStroke();
			bar.Add(Vector(6, 0, 8));
			bar.Add(Vector(14, 0, 8));
			strokes.Insert(trench);
			strokes.Insert(hole);
			strokes.Insert(bar);
		}
		else if (shape == MG_NEST)
		{
			MUD_BuildStroke line = new MUD_BuildStroke();
			line.Add(Vector(0, 0, 0));
			line.Add(Vector(20, 0, 0));
			MUD_BuildStroke link = new MUD_BuildStroke();
			link.Add(Vector(10, 0, 0));
			link.Add(Vector(10, 0, 4));
			MUD_BuildStroke ring = new MUD_BuildStroke();
			Ellipse(Vector(10, 0, 9), Vector(0, 0, -1), Vector(1, 0, 0), 5, 5, 0, Math.PI * 2, 8, ring);
			strokes.Insert(line);
			strokes.Insert(link);
			strokes.Insert(ring);
		}
		else if (shape == RAMP)
		{
			MUD_BuildStroke stairs = new MUD_BuildStroke();
			stairs.Add(Vector(0, 0, 0));
			for (int k = 0; k < 5; k++)
			{
				stairs.Add(Vector(4 + k * 4, 0, k * 3));
				stairs.Add(Vector(4 + k * 4, 0, k * 3 + 3));
			}
			stairs.Add(Vector(24, 0, 15));
			strokes.Insert(stairs);
		}
		else
		{
			Make(shape, Vector(0, 0, 4), Vector(20, 0, 4), 0, false, strokes);
		}
		foreach (MUD_BuildStroke st : strokes)
		{
			array<vector> line = {};
			st.Polyline(line, 6);
			lines.Insert(line);
		}
	}
}
