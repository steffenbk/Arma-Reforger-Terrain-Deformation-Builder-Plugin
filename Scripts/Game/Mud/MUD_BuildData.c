//! Trench builder F4 (spec docs/superpowers/specs/2026-10-24-trench-builder-design.md): constants and the pure math
//! every machine shares - the build area quad, the part texts and the capsule lists of roofed parts. The same text
//! gives the same pieces on every machine (the server digs them, clients draw ghosts and find the piece in front).
//! Roofed parts (covered trench, tunnel link, dugout) are chains of the shovel TUNNEL capsule (MUD_VoxelProbe
//! ServerTunnel: MP / JIP / guards as proven for shovel tunnels); trenches and firing steps are MUD_TrenchPlan text.
class MUD_Build
{
	static const int TRENCH = 1;    // MUD_TrenchPlan text: heightfield trench (lines can end on another line = junction)
	static const int STEP = 2;      // firing step: a narrow shallow trench beside a trench (MUD_TrenchPlan text)
	static const int COVER = 3;     // roofed stretch of a trench: "trenchUid,depthCm;x,z;x,z;..."
	static const int TUNNEL = 4;    // tunnel link: "depthAcm,depthBcm;x,z;x,z;..."
	static const int DUGOUT = 5;    // box room: "x,z,yawDeg,widthCm,lengthCm,depthCm"

	static const float AREA_MIN_SIDE = 5;       // m
	static const float AREA_MAX_SIDE = 200;     // m (user 2026-10-24: "200x200 meter max")
	static const int MAX_AREAS = 10;            // server-wide (free-mode trench sites have their own MAX_SITES)
	static const int OWNER_HOLD_S = 600;        // user 2026-10-24: only the staker edits for ~10 min, then everyone
	static const int AREA_IDLE_S = 7200;        // no dig and no edit this long: the area + its plan go (dug ground stays)
	static const int LOCK_IDLE_S = 180;         // an editor lock with no keep-alive this long is freed
	static const int KEEP_ALIVE_MS = 60000;     // manager open: keep-alive to the server this often
	static const float LINK_REACH = 50;         // m: a trench line / tunnel may reach this far out of its area (links)
	static const float STAKE_REACH = 30;        // m: the player this near the last corner when staking
	static const float INSIDE_TOL = 0.5;        // m of slack at an area edge
	static const int MAX_PARTS = 200;           // per area
	static const int MAX_PIECES = 2000;         // trench pieces per area (first guess, tune on the DS)
	static const int MAX_CAPS = 600;            // capsules per area: voxel ops are the cost (first guess, tune on the DS)
	static const int MAX_TEXT = 400;            // chars per part text (the trench plan RPC limit)

	static const float CAP_STEP = 0.8;          // m between capsules (radius MUD_VoxelProbe.TUNNEL_R 0.8)
	static const float ROOF = 0.6;              // m of undug ground kept over a covered trench / tunnel middle
	static const float SLOPE = 0.5;             // m down per m along a passage (27 deg, the shovel tunnel start slope)
	static const float DUGOUT_D = 3.5;          // m: dugout floor under the undug ground (2.5 m room + ~1 m roof)
	static const float COVER_ENTRY = 1.5;       // m at each end of a covered stretch where the trench stays open
	static const float COVER_MAX = 40;          // m along the trench
	static const float TUNNEL_MAX = 100;        // m along the path
	static const int PATH_MAX_POINTS = 20;
	static const float PATH_MIN_GAP = 0.3;      // m between path points
	static const float PATH_MAX_GAP = 12;       // m between path points
	static const int MIN_DEPTH_CM = 50;         // tunnel / cover end depth
	static const int MAX_DEPTH_CM = 600;
	static const float DIG_REACH = 3;           // m: capsule middle to the player's chest (MUD_TunnelAimOk uses 3)
	static const float DIG_PICK = 1.5;          // m (flat): capsule to the dig point in front of the player
	static const float DIG_FEET_OVER = 0.5;     // m: feet at most this far over a capsule middle (no digging it from the surface)
	static const int DUG_SIZES = 4;

	//------------------------------------------------------------------------------------------------
	static string KindName(int kind)
	{
		if (kind == TRENCH)
			return "trench";
		if (kind == STEP)
			return "firing step";
		if (kind == COVER)
			return "roofed trench";
		if (kind == TUNNEL)
			return "tunnel";
		if (kind == DUGOUT)
			return "dugout";
		return "?";
	}

	//------------------------------------------------------------------------------------------------
	//! m from a capsule's floor to its top (floor + TUNNEL_MID = middle; + HALF + R = top): 2.5 m today
	static float CapHeight()
	{
		return MUD_VoxelProbe.TUNNEL_MID + MUD_VoxelProbe.TUNNEL_HALF + MUD_VoxelProbe.TUNNEL_R;
	}

	//------------------------------------------------------------------------------------------------
	//! Dugout room sizes (X in the manager): width / length in m
	static float DugWidth(int index)
	{
		if (index == 0)
			return 2;
		if (index == 3)
			return 4;
		return 3;
	}

	//------------------------------------------------------------------------------------------------
	static float DugLength(int index)
	{
		if (index <= 1)
			return 2;
		return 3;
	}

	//------------------------------------------------------------------------------------------------
	//! Undug ground height (the same float on every machine: skin triangles, heightmap off our ground)
	static bool UndugAt(float x, float z, out float y)
	{
		return MUD_WorldGroundEntity.UndugY(x, z, y);
	}

	//------------------------------------------------------------------------------------------------
	//! "12,-3,40" -> 12, -3, 40 appended to outInts; false on anything that is not a plain integer
	static bool Ints(string text, array<int> outInts)
	{
		array<string> nums = {};
		text.Split(",", nums, false);
		foreach (string n : nums)
		{
			if (!MUD_TrenchPlan.IsInt(n))
				return false;
			outInts.Insert(n.ToInt());
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static int Cm(float m)
	{
		return Math.Round(m * 100);
	}

	//------------------------------------------------------------------------------------------------
	//! 2D cross (XZ): > 0 when b turns left (counter-clockwise seen from above, x right, z up) of a
	static float Cross(vector a, vector b)
	{
		return a[0] * b[2] - a[2] * b[0];
	}

	//------------------------------------------------------------------------------------------------
	static float PolyLength(array<vector> pts)
	{
		float total = 0;
		for (int i = 0; i < pts.Count() - 1; i++)
		{
			total += vector.DistanceXZ(pts[i], pts[i + 1]);
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Point at distance s along the polyline (y 0); dir = its flat direction there
	static vector PolyPoint(array<vector> pts, float s, out vector dir)
	{
		dir = vector.Forward;
		int n = pts.Count();
		if (n == 0)
			return vector.Zero;
		if (n == 1)
			return Vector(pts[0][0], 0, pts[0][2]);
		float left = s;
		for (int i = 0; i < n - 1; i++)
		{
			vector a = Vector(pts[i][0], 0, pts[i][2]);
			vector b = Vector(pts[i + 1][0], 0, pts[i + 1][2]);
			float len = vector.Distance(a, b);
			if (len < 0.0001)
				continue;
			dir = (b - a) * (1 / len);
			if (left <= len || i == n - 2)
			{
				float t = Math.Clamp(left / len, 0, 1);
				return a + (b - a) * t;
			}
			left -= len;
		}
		return Vector(pts[n - 1][0], 0, pts[n - 1][2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Floor heights of a roofed passage (pure, the self-test feeds it): at distance s[i] along a passage of length
	//! total, undug ground undug[i]. Ends at undug - dA / dB (the trench / room floor there); in between deep enough for
	//! a ROOF over a CapHeight capsule, but never dropping faster than SLOPE from either end (the entrance ramps), and
	//! never rising faster than SLOPE between samples (a hill makes it deeper, not a climb).
	static void Profile(array<float> s, array<float> undug, float dA, float dB, float total, array<float> floors)
	{
		floors.Clear();
		int n = s.Count();
		if (n == 0)
			return;
		float yA = undug[0] - dA;
		float yB = undug[n - 1] - dB;
		float deep = ROOF + CapHeight();
		for (int i = 0; i < n; i++)
		{
			float y = undug[i] - deep;
			y = Math.Max(y, yA - SLOPE * s[i]);
			y = Math.Max(y, yB - SLOPE * (total - s[i]));
			floors.Insert(y);
		}
		// rise limit both ways (keeps it walkable under a hill; only ever lowers a sample)
		for (int f = 1; f < n; f++)
		{
			float limit = floors[f - 1] + SLOPE * (s[f] - s[f - 1]);
			if (floors[f] > limit)
				floors[f] = limit;
		}
		for (int b = n - 2; b >= 0; b--)
		{
			float limitB = floors[b + 1] + SLOPE * (s[b + 1] - s[b]);
			if (floors[b] > limitB)
				floors[b] = limitB;
		}
	}
}

//------------------------------------------------------------------------------------------------
//! A build area: 4 corners (world XZ), sorted counter-clockwise round their middle so any click order gives one
//! simple quad. Text "x,z;x,z;x,z;x,z" in cm.
class MUD_BuildQuad
{
	ref array<vector> m_aC = {};

	//------------------------------------------------------------------------------------------------
	//! null on anything malformed (server input). Shape limits are Valid's job.
	static MUD_BuildQuad Unpack(string text)
	{
		if (text.Length() > 120)
			return null;
		array<string> parts = {};
		text.Split(";", parts, false);
		if (parts.Count() != 4)
			return null;
		MUD_BuildQuad q = new MUD_BuildQuad();
		foreach (string p : parts)
		{
			array<int> v = {};
			if (!MUD_Build.Ints(p, v) || v.Count() != 2)
				return null;
			q.m_aC.Insert(Vector(v[0] * 0.01, 0, v[1] * 0.01));
		}
		q.Order();
		return q;
	}

	//------------------------------------------------------------------------------------------------
	static MUD_BuildQuad FromPoints(array<vector> pts)
	{
		if (pts.Count() != 4)
			return null;
		MUD_BuildQuad q = new MUD_BuildQuad();
		foreach (vector p : pts)
		{
			q.m_aC.Insert(Vector(MUD_Build.Cm(p[0]) * 0.01, 0, MUD_Build.Cm(p[2]) * 0.01));
		}
		q.Order();
		return q;
	}

	//------------------------------------------------------------------------------------------------
	string Pack()
	{
		string s;
		for (int i = 0; i < m_aC.Count(); i++)
		{
			if (i > 0)
				s += ";";
			s += string.Format("%1,%2", MUD_Build.Cm(m_aC[i][0]), MUD_Build.Cm(m_aC[i][2]));
		}
		return s;
	}

	//------------------------------------------------------------------------------------------------
	vector Centre()
	{
		vector c = vector.Zero;
		foreach (vector p : m_aC)
		{
			c = c + p;
		}
		if (m_aC.Count() > 0)
			c = c * (1 / m_aC.Count());
		c[1] = 0;
		return c;
	}

	//------------------------------------------------------------------------------------------------
	//! m from the middle to the farthest corner
	float Radius()
	{
		vector c = Centre();
		float r = 0;
		foreach (vector p : m_aC)
		{
			r = Math.Max(r, vector.DistanceXZ(c, p));
		}
		return r;
	}

	//------------------------------------------------------------------------------------------------
	//! Sort the corners by angle round their middle (selection sort of 4)
	protected void Order()
	{
		vector c = Centre();
		array<float> ang = {};
		foreach (vector p : m_aC)
		{
			ang.Insert(Math.Atan2(p[2] - c[2], p[0] - c[0]));
		}
		for (int i = 0; i < m_aC.Count() - 1; i++)
		{
			int best = i;
			for (int j = i + 1; j < m_aC.Count(); j++)
			{
				if (ang[j] < ang[best])
					best = j;
			}
			if (best == i)
				continue;
			vector tp = m_aC[i];
			m_aC[i] = m_aC[best];
			m_aC[best] = tp;
			float ta = ang[i];
			ang[i] = ang[best];
			ang[best] = ta;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Convex, every side AREA_MIN_SIDE .. AREA_MAX_SIDE, both diagonals inside a 200 x 200 m square's
	bool Valid(out string why)
	{
		if (m_aC.Count() != 4)
		{
			why = "an area needs 4 corners";
			return false;
		}
		for (int i = 0; i < 4; i++)
		{
			vector a = m_aC[i];
			vector b = m_aC[(i + 1) % 4];
			vector c = m_aC[(i + 2) % 4];
			float side = vector.DistanceXZ(a, b);
			if (side < MUD_Build.AREA_MIN_SIDE - 0.01 || side > MUD_Build.AREA_MAX_SIDE + 0.01)
			{
				why = string.Format("side %1 is %2 m (%3 to %4 m)", i + 1, Math.Round(side), MUD_Build.AREA_MIN_SIDE, MUD_Build.AREA_MAX_SIDE);
				return false;
			}
			if (MUD_Build.Cross(b - a, c - b) <= 0.01)
			{
				why = "the 4 corners must make a convex shape (no corner pointing in)";
				return false;
			}
		}
		float diag = MUD_Build.AREA_MAX_SIDE * 1.4143 + 0.5;
		if (vector.DistanceXZ(m_aC[0], m_aC[2]) > diag || vector.DistanceXZ(m_aC[1], m_aC[3]) > diag)
		{
			why = string.Format("bigger than %1 x %1 m", MUD_Build.AREA_MAX_SIDE);
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Signed distance of (x, z) inside edge i (> 0 = inside side)
	protected float EdgeIn(int i, float x, float z)
	{
		vector a = m_aC[i];
		vector b = m_aC[(i + 1) % 4];
		vector e = b - a;
		float len = Math.Sqrt(e[0] * e[0] + e[2] * e[2]);
		if (len < 0.0001)
			return 0;
		return MUD_Build.Cross(e, Vector(x - a[0], 0, z - a[2])) / len;
	}

	//------------------------------------------------------------------------------------------------
	//! (x, z) inside, the edges grown out by grow m
	bool Inside(float x, float z, float grow = 0)
	{
		if (m_aC.Count() != 4)
			return false;
		for (int i = 0; i < 4; i++)
		{
			if (EdgeIn(i, x, z) < -grow)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! How far (x, z) lies outside (0 inside; the edge-line distance, a little short at the corners)
	float DistOutside(float x, float z)
	{
		float d = 0;
		for (int i = 0; i < m_aC.Count(); i++)
		{
			d = Math.Max(d, -EdgeIn(i, x, z));
		}
		return d;
	}

	//------------------------------------------------------------------------------------------------
	//! The two quads share more than a tol m wide strip (separating axis test over the 8 edge normals). Touching
	//! edges do not count: neighbour areas may share a border.
	bool Overlaps(MUD_BuildQuad o, float tol = 0.5)
	{
		if (!o || m_aC.Count() != 4 || o.m_aC.Count() != 4)
			return false;
		for (int k = 0; k < 8; k++)
		{
			array<vector> src = m_aC;
			int i = k;
			if (k >= 4)
			{
				src = o.m_aC;
				i = k - 4;
			}
			vector e = src[(i + 1) % 4] - src[i];
			vector nrm = Vector(-e[2], 0, e[0]);
			float len = nrm.Length();
			if (len < 0.0001)
				continue;
			nrm = nrm * (1 / len);
			float minA = 1000000000;
			float maxA = -1000000000;
			float minB = 1000000000;
			float maxB = -1000000000;
			for (int j = 0; j < 4; j++)
			{
				float pa = vector.Dot(m_aC[j], nrm);
				float pb = vector.Dot(o.m_aC[j], nrm);
				minA = Math.Min(minA, pa);
				maxA = Math.Max(maxA, pa);
				minB = Math.Min(minB, pb);
				maxB = Math.Max(maxB, pb);
			}
			if (maxA - minB < tol || maxB - minA < tol)
				return false;
		}
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! One part's text parsed (any machine). Trench kinds keep their MUD_TrenchPlan + layout; roofed kinds a capsule
//! list (floor points: XZ from the text, y from the undug ground once Floors() worked).
class MUD_BuildSpec
{
	int m_iKind;
	string m_sError;
	ref MUD_TrenchPlan m_Plan;              // TRENCH / STEP
	ref MUD_TrenchLayout m_Layout;
	ref array<vector> m_aPath = {};         // COVER / TUNNEL path points (y 0)
	ref array<vector> m_aCaps = {};         // capsule floor points
	ref array<float> m_aCapS = {};          // COVER / TUNNEL: distance of each capsule along the path
	bool m_bFloors;
	int m_iCoverOf;                         // COVER: uid of the trench part it roofs
	float m_fDepthA;                        // COVER / TUNNEL: end floor depths under the undug ground (m)
	float m_fDepthB;
	float m_fLength;                        // COVER / TUNNEL path length
	vector m_vMid;                          // DUGOUT middle (y 0)
	float m_fYaw;                           // DUGOUT: door -> room direction, deg
	float m_fW;
	float m_fL;
	float m_fDepth;                         // DUGOUT floor depth
	int m_iRows;                            // DUGOUT capsule grid (row 0 = door end)
	int m_iCols;

	//------------------------------------------------------------------------------------------------
	static MUD_BuildSpec Make(int kind, string text)
	{
		MUD_BuildSpec spec = new MUD_BuildSpec();
		if (!spec.Parse(kind, text))
			return null;
		return spec;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Fail(string why)
	{
		m_sError = why;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! False (m_sError says why) on anything malformed or out of limits (server input: never trust it)
	bool Parse(int kind, string text)
	{
		m_iKind = kind;
		if (text.IsEmpty() || text.Length() > MUD_Build.MAX_TEXT)
			return Fail("empty or too long");
		if (kind == MUD_Build.TRENCH || kind == MUD_Build.STEP)
		{
			m_Plan = MUD_TrenchPlan.Unpack(text);
			if (!m_Plan)
				return Fail("trench text does not unpack");
			m_Layout = new MUD_TrenchLayout();
			if (!m_Layout.Build(m_Plan))
				return Fail("trench: " + m_Layout.m_sError);
			return true;
		}
		if (kind == MUD_Build.COVER || kind == MUD_Build.TUNNEL)
			return ParsePath(text);
		if (kind == MUD_Build.DUGOUT)
			return ParseDugout(text);
		return Fail(string.Format("unknown kind %1", kind));
	}

	//------------------------------------------------------------------------------------------------
	protected bool ParsePath(string text)
	{
		array<string> parts = {};
		text.Split(";", parts, false);
		if (parts.Count() < 3 || parts.Count() > MUD_Build.PATH_MAX_POINTS + 1)
			return Fail(string.Format("%1 points (2 to %2)", parts.Count() - 1, MUD_Build.PATH_MAX_POINTS));
		array<int> head = {};
		if (!MUD_Build.Ints(parts[0], head) || head.Count() != 2)
			return Fail("bad head");
		if (m_iKind == MUD_Build.COVER)
		{
			m_iCoverOf = head[0];
			if (head[1] < MUD_Build.MIN_DEPTH_CM || head[1] > MUD_Build.MAX_DEPTH_CM)
				return Fail("bad depth");
			m_fDepthA = head[1] * 0.01;
			m_fDepthB = m_fDepthA;
		}
		else
		{
			if (head[0] < MUD_Build.MIN_DEPTH_CM || head[0] > MUD_Build.MAX_DEPTH_CM || head[1] < MUD_Build.MIN_DEPTH_CM || head[1] > MUD_Build.MAX_DEPTH_CM)
				return Fail("bad end depth");
			m_fDepthA = head[0] * 0.01;
			m_fDepthB = head[1] * 0.01;
		}
		for (int i = 1; i < parts.Count(); i++)
		{
			array<int> pt = {};
			if (!MUD_Build.Ints(parts[i], pt) || pt.Count() != 2)
				return Fail("bad point");
			vector p = Vector(pt[0] * 0.01, 0, pt[1] * 0.01);
			if (m_aPath.Count() > 0)
			{
				float gap = vector.DistanceXZ(p, m_aPath[m_aPath.Count() - 1]);
				if (gap < MUD_Build.PATH_MIN_GAP || gap > MUD_Build.PATH_MAX_GAP + 0.01)
					return Fail(string.Format("point %1 is %2 m from the last (%3 to %4 m)", i, gap, MUD_Build.PATH_MIN_GAP, MUD_Build.PATH_MAX_GAP));
			}
			m_aPath.Insert(p);
		}
		m_fLength = MUD_Build.PolyLength(m_aPath);
		float most = MUD_Build.TUNNEL_MAX;
		if (m_iKind == MUD_Build.COVER)
			most = MUD_Build.COVER_MAX;
		if (m_fLength > most + 0.01)
			return Fail(string.Format("%1 m long (max %2 m)", Math.Round(m_fLength), most));

		// - 0.001: 8 m / 0.8 must give 10 steps, not 11 from float error
		int n = Math.Ceil(m_fLength / MUD_Build.CAP_STEP - 0.001) + 1;
		if (n < 2)
			n = 2;
		float step = m_fLength / (n - 1);
		for (int k = 0; k < n; k++)
		{
			float s = step * k;
			vector dir;
			m_aCaps.Insert(MUD_Build.PolyPoint(m_aPath, s, dir));
			m_aCapS.Insert(s);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Capsules along len (m) of room: one when it fits in one capsule width, else evenly CAP_STEP or closer
	protected static int CapCount(float len)
	{
		float inner = len - 2 * MUD_VoxelProbe.TUNNEL_R;
		if (inner <= 0.01)
			return 1;
		return Math.Ceil(inner / MUD_Build.CAP_STEP - 0.001) + 1;
	}

	//------------------------------------------------------------------------------------------------
	protected static float CapAt(float len, int count, int k)
	{
		if (count <= 1)
			return 0;
		float inner = len - 2 * MUD_VoxelProbe.TUNNEL_R;
		return -inner * 0.5 + inner * k / (count - 1);
	}

	//------------------------------------------------------------------------------------------------
	protected bool ParseDugout(string text)
	{
		array<int> v = {};
		if (!MUD_Build.Ints(text, v) || v.Count() != 6)
			return Fail("dugout text");
		bool sized = false;
		for (int i = 0; i < MUD_Build.DUG_SIZES; i++)
		{
			if (v[3] == MUD_Build.Cm(MUD_Build.DugWidth(i)) && v[4] == MUD_Build.Cm(MUD_Build.DugLength(i)))
				sized = true;
		}
		if (!sized)
			return Fail("dugout size");
		if (v[2] < -360 || v[2] > 360 || v[5] < 250 || v[5] > MUD_Build.MAX_DEPTH_CM)
			return Fail("dugout yaw / depth");
		m_vMid = Vector(v[0] * 0.01, 0, v[1] * 0.01);
		m_fYaw = v[2];
		m_fW = v[3] * 0.01;
		m_fL = v[4] * 0.01;
		m_fDepth = v[5] * 0.01;
		m_iRows = CapCount(m_fL);
		m_iCols = CapCount(m_fW);
		vector fwd = DugFwd();
		vector right = Vector(fwd[2], 0, -fwd[0]);
		for (int r = 0; r < m_iRows; r++)
		{
			for (int c = 0; c < m_iCols; c++)
			{
				m_aCaps.Insert(m_vMid + fwd * CapAt(m_fL, m_iRows, r) + right * CapAt(m_fW, m_iCols, c));
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	vector DugFwd()
	{
		float a = m_fYaw * Math.DEG2RAD;
		return Vector(Math.Sin(a), 0, Math.Cos(a));
	}

	//------------------------------------------------------------------------------------------------
	bool IsCapsules()
	{
		return m_iKind == MUD_Build.COVER || m_iKind == MUD_Build.TUNNEL || m_iKind == MUD_Build.DUGOUT;
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		if (m_Layout)
			return m_Layout.Count();
		return m_aCaps.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Layers a piece takes: trench plan layers, a capsule 1
	int Layers()
	{
		if (m_Layout)
			return m_Layout.Layers();
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Middle of piece i, y 0
	vector PieceXZ(int i)
	{
		if (m_Layout)
			return m_Layout.PieceMiddle(i);
		vector p = m_aCaps[i];
		p[1] = 0;
		return p;
	}

	//------------------------------------------------------------------------------------------------
	//! Every point that must lie in the area: plan points, path points, the dugout's 4 corners (y 0)
	void Points(array<vector> pts)
	{
		pts.Clear();
		if (m_Plan)
		{
			foreach (vector p : m_Plan.m_aPoints)
			{
				pts.Insert(Vector(p[0], 0, p[2]));
			}
			return;
		}
		if (m_iKind == MUD_Build.DUGOUT)
		{
			vector fwd = DugFwd();
			vector right = Vector(fwd[2], 0, -fwd[0]);
			pts.Insert(m_vMid + fwd * (m_fL * 0.5) + right * (m_fW * 0.5));
			pts.Insert(m_vMid + fwd * (m_fL * 0.5) - right * (m_fW * 0.5));
			pts.Insert(m_vMid - fwd * (m_fL * 0.5) + right * (m_fW * 0.5));
			pts.Insert(m_vMid - fwd * (m_fL * 0.5) - right * (m_fW * 0.5));
			return;
		}
		foreach (vector q : m_aPath)
		{
			pts.Insert(q);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Flat middle and radius of everything (ghost range checks)
	void Bounds(out vector mid, out float radius)
	{
		array<vector> pts = {};
		Points(pts);
		mid = vector.Zero;
		radius = 1;
		if (pts.Count() == 0)
			return;
		vector lo = pts[0];
		vector hi = pts[0];
		foreach (vector p : pts)
		{
			lo[0] = Math.Min(lo[0], p[0]);
			lo[2] = Math.Min(lo[2], p[2]);
			hi[0] = Math.Max(hi[0], p[0]);
			hi[2] = Math.Max(hi[2], p[2]);
		}
		mid = (lo + hi) * 0.5;
		radius = vector.DistanceXZ(lo, hi) * 0.5 + 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Capsule floor heights from the undug ground (any machine; false while the ground is not ready)
	bool Floors()
	{
		if (m_bFloors || !IsCapsules())
			return m_bFloors;
		if (m_iKind == MUD_Build.DUGOUT)
		{
			float u;
			if (!MUD_Build.UndugAt(m_vMid[0], m_vMid[2], u))
				return false;
			for (int i = 0; i < m_aCaps.Count(); i++)
			{
				vector c = m_aCaps[i];
				c[1] = u - m_fDepth;
				m_aCaps[i] = c;
			}
			m_bFloors = true;
			return true;
		}
		array<float> undug = {};
		foreach (vector p : m_aCaps)
		{
			float y;
			if (!MUD_Build.UndugAt(p[0], p[2], y))
				return false;
			undug.Insert(y);
		}
		array<float> floors = {};
		MUD_Build.Profile(m_aCapS, undug, m_fDepthA, m_fDepthB, m_fLength, floors);
		for (int k = 0; k < m_aCaps.Count(); k++)
		{
			vector q = m_aCaps[k];
			q[1] = floors[k];
			m_aCaps[k] = q;
		}
		m_bFloors = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Middle of capsule i (the ServerTunnel point): floor + TUNNEL_MID. Floors() must have worked.
	vector CapMid(int i)
	{
		return m_aCaps[i] + Vector(0, MUD_VoxelProbe.TUNNEL_MID, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! DUGOUT: capsules i and j touch (same or next row / column)
	bool Touch(int i, int j)
	{
		if (m_iCols <= 0)
			return Math.AbsInt(i - j) == 1;
		int ri = i / m_iCols;
		int ci = i - ri * m_iCols;
		int rj = j / m_iCols;
		int cj = j - rj * m_iCols;
		return i != j && Math.AbsInt(ri - rj) <= 1 && Math.AbsInt(ci - cj) <= 1;
	}

	//------------------------------------------------------------------------------------------------
	//! DUGOUT: capsule i is in row 0 (the door end: always diggable)
	bool DoorRow(int i)
	{
		return m_iCols > 0 && i < m_iCols;
	}

	//------------------------------------------------------------------------------------------------
	//! DUGOUT: (x, z) inside the room footprint
	bool InRoom(float x, float z)
	{
		if (m_iKind != MUD_Build.DUGOUT)
			return false;
		vector fwd = DugFwd();
		vector right = Vector(fwd[2], 0, -fwd[0]);
		vector rel = Vector(x - m_vMid[0], 0, z - m_vMid[2]);
		return Math.AbsFloat(vector.Dot(rel, fwd)) <= m_fL * 0.5 && Math.AbsFloat(vector.Dot(rel, right)) <= m_fW * 0.5;
	}
}

//------------------------------------------------------------------------------------------------
//! Once per session (called from MUD_TrenchSelfTest.Run): the builder math. Logs 'MUD build test: PASS (N checks)'.
class MUD_BuildSelfTest
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
		MUD_BuildSelfTest t = new MUD_BuildSelfTest();
		t.All();
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(bool ok, string what)
	{
		m_iChecks++;
		if (ok)
			return;
		m_iFails++;
		Print("MUD build test: FAIL " + what, LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void All()
	{
		Quads();
		Specs();
		Profiles();
		Chain();
		Shapes();
		if (m_iFails == 0)
			Print(string.Format("MUD build test: PASS (%1 checks)", m_iChecks));
		else
			Print(string.Format("MUD build test: FAIL %1 of %2 checks", m_iFails, m_iChecks), LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Round 2 shapes (2026-10-24): every drag shape, small and big, gives plans the layout accepts; the arc bows
	//! left of the drag; a ring and a rectangle end where they start; a pencil zigzag keeps its corners
	protected void Shapes()
	{
		array<float> sizes = {6, 30, 150};
		for (int shape = MUD_BuildShapes.LINE; shape < MUD_BuildShapes.FOXHOLE; shape++)     // stamps are built by the manager
		{
			foreach (float size : sizes)
			{
				for (int turn = 0; turn < 2; turn++)
				{
					array<ref MUD_BuildStroke> strokes = {};
					float ta = turn * 30 * Math.DEG2RAD;
					vector ax = Vector(Math.Cos(ta), 0, Math.Sin(ta));
					vector s0 = Vector(100, 0, 100);
					vector e0 = s0 + ax * size + MUD_BuildShapes.Left(ax) * (size * 0.7);     // the drag in the turned frame (test fix 2026-10-24)
					MUD_BuildShapes.Make(shape, s0, e0, turn * 30, false, strokes);
					array<string> texts = {};
					string why;
					bool ok = MUD_BuildShapes.ToPlans(strokes, 1, 6, MUD_TrenchPlan.DEFAULT_WALL, texts, why);
					Check(ok, string.Format("shape %1 size %2 turn %3: %4", MUD_BuildShapes.Name(shape), size, turn * 30, why));
				}
			}
		}
		array<ref MUD_BuildStroke> arc = {};
		MUD_BuildShapes.Make(MUD_BuildShapes.ARC, Vector(0, 0, 0), Vector(20, 0, 0), 0, false, arc);
		float top = -1000;
		foreach (MUD_BuildStroke st : arc)
		{
			array<vector> line = {};
			st.Polyline(line, 6);
			foreach (vector p : line)
			{
				top = Math.Max(top, p[2]);
			}
		}
		Check(Math.AbsFloat(top - 10) < 0.3, string.Format("arc east: top z %1, not 10 (bows left = north)", top));
		array<int> closed = {MUD_BuildShapes.RING, MUD_BuildShapes.RECT};
		foreach (int c : closed)
		{
			array<ref MUD_BuildStroke> loop = {};
			MUD_BuildShapes.Make(c, Vector(0, 0, 0), Vector(40, 0, 25), 0, false, loop);
			array<string> lt = {};
			string lw;
			MUD_BuildShapes.ToPlans(loop, 1, 6, MUD_TrenchPlan.DEFAULT_WALL, lt, lw);
			if (lt.IsEmpty())
				continue;
			MUD_TrenchPlan first = MUD_TrenchPlan.Unpack(lt[0]);
			MUD_TrenchPlan last = MUD_TrenchPlan.Unpack(lt[lt.Count() - 1]);
			vector a = first.m_aPoints[0];
			vector b = last.m_aPoints[last.m_aPoints.Count() - 1];
			Check(vector.DistanceXZ(a, b) < 0.05, string.Format("%1 closes: start %2 end %3", MUD_BuildShapes.Name(c), a, b));
		}
		array<vector> drawn = {};
		for (int i = 0; i <= 40; i++)
		{
			float zz = 0;
			if (i > 20)
				zz = i - 20;
			drawn.Insert(Vector(Math.Min(i, 20), 0, zz));
		}
		array<ref MUD_BuildStroke> pen = {};
		MUD_BuildShapes.Pencil(drawn, pen);
		int penPts = 0;
		if (pen.Count() == 1)
			penPts = pen[0].Count();
		Check(penPts == 3, string.Format("pencil L: %1 strokes, %2 points, not 1 / 3", pen.Count(), penPts));
	}

	//------------------------------------------------------------------------------------------------
	protected void Quads()
	{
		// clicked out of order: still one simple square
		MUD_BuildQuad q = MUD_BuildQuad.Unpack("0,0;1000,1000;1000,0;0,1000");
		Check(q != null, "quad unpack");
		if (!q)
			return;
		string why;
		Check(q.Valid(why), "quad 10 m square valid: " + why);
		Check(q.Inside(5, 5), "quad inside middle");
		Check(!q.Inside(11, 5), "quad outside");
		Check(q.Inside(10.3, 5, 0.5), "quad inside grown");
		Check(Math.AbsFloat(q.DistOutside(13, 5) - 3) < 0.01, string.Format("quad outside dist %1, want 3", q.DistOutside(13, 5)));
		MUD_BuildQuad touch = MUD_BuildQuad.Unpack("1000,0;2000,0;2000,1000;1000,1000");
		Check(touch && !q.Overlaps(touch), "quad touching neighbour is no overlap");
		MUD_BuildQuad over = MUD_BuildQuad.Unpack("500,500;1500,500;1500,1500;500,1500");
		Check(over && q.Overlaps(over), "quad overlap");
		MUD_BuildQuad big = MUD_BuildQuad.Unpack("0,0;25000,0;25000,1000;0,1000");
		Check(big && !big.Valid(why), "quad 250 m side refused");
		MUD_BuildQuad tiny = MUD_BuildQuad.Unpack("0,0;200,0;200,200;0,200");
		Check(tiny && !tiny.Valid(why), "quad 2 m side refused");
		MUD_BuildQuad dart = MUD_BuildQuad.Unpack("0,0;1000,0;1000,1000;900,100");
		Check(dart && !dart.Valid(why), "quad corner pointing in refused");
		array<string> bad = {"", "0,0;1,1;2,2", "0,0;1000,0;1000,1000;0,x", "0,0;1000,0;1000,1000;0,1000;5,5"};
		foreach (string b : bad)
		{
			Check(MUD_BuildQuad.Unpack(b) == null, "quad unpack should refuse '" + b + "'");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Specs()
	{
		MUD_BuildSpec t = MUD_BuildSpec.Make(MUD_Build.TUNNEL, "200,350;0,0;800,0");
		Check(t != null, "tunnel spec");
		if (t)
			Check(t.Count() == 11, string.Format("tunnel 8 m: %1 capsules, want 11", t.Count()));
		MUD_BuildSpec d = MUD_BuildSpec.Make(MUD_Build.DUGOUT, "0,0,90,300,300,350");
		Check(d != null, "dugout spec");
		if (d)
		{
			Check(d.Count() == 9, string.Format("dugout 3 x 3: %1 capsules, want 9", d.Count()));
			Check(d.DoorRow(0) && !d.DoorRow(3), "dugout door row");
			Check(d.Touch(0, 4) && !d.Touch(0, 8), "dugout touch");
			Check(d.InRoom(1.4, 0) && !d.InRoom(1.6, 0), "dugout footprint (yaw 90 = +x)");
		}
		MUD_BuildSpec c = MUD_BuildSpec.Make(MUD_Build.COVER, "7,150;0,0;300,0;600,0");
		Check(c != null && c.m_iCoverOf == 7, "cover spec");
		array<string> badTunnel = {"200,350;0,0", "10,350;0,0;800,0", "200,350;0,0;1300,0", "200,350;0,0;10,0", "200;0,0;800,0"};
		foreach (string b : badTunnel)
		{
			Check(MUD_BuildSpec.Make(MUD_Build.TUNNEL, b) == null, "tunnel should refuse '" + b + "'");
		}
		Check(MUD_BuildSpec.Make(MUD_Build.DUGOUT, "0,0,90,500,300,350") == null, "dugout should refuse size 5 x 3");
		Check(MUD_BuildSpec.Make(MUD_Build.COVER, "7,150;0,0;1200,0;2400,0;3600,0;4800,0") == null, "cover 48 m refused");
		Check(MUD_BuildSpec.Make(9, "1,6;0,0,0;400,0,0") == null, "unknown kind refused");
		Check(MUD_BuildSpec.Make(MUD_Build.TRENCH, "1,6;0,0,0;400,0,0") != null, "trench spec");
	}

	//------------------------------------------------------------------------------------------------
	//! Flat ground at 0, 20 m passage from a 1.5 m trench to a 1.5 m trench: ends at -1.5, middle at roof depth,
	//! never steeper than SLOPE
	protected void Profiles()
	{
		array<float> s = {};
		array<float> undug = {};
		for (int i = 0; i <= 20; i++)
		{
			s.Insert(i * 1.0);
			undug.Insert(0.0);
		}
		array<float> floors = {};
		MUD_Build.Profile(s, undug, 1.5, 1.5, 20, floors);
		float deep = -(MUD_Build.ROOF + MUD_Build.CapHeight());
		Check(Math.AbsFloat(floors[0] + 1.5) < 0.001, string.Format("profile start %1, want -1.5", floors[0]));
		Check(Math.AbsFloat(floors[20] + 1.5) < 0.001, string.Format("profile end %1, want -1.5", floors[20]));
		Check(Math.AbsFloat(floors[10] - deep) < 0.001, string.Format("profile middle %1, want %2", floors[10], deep));
		float worst = 0;
		for (int k = 1; k <= 20; k++)
		{
			worst = Math.Max(worst, Math.AbsFloat(floors[k] - floors[k - 1]));
		}
		Check(worst <= MUD_Build.SLOPE + 0.001, string.Format("profile steepest %1 m per m, max %2", worst, MUD_Build.SLOPE));
		// a 4 m hill in the middle: deeper there, still no step steeper than SLOPE
		undug[10] = 4;
		MUD_Build.Profile(s, undug, 1.5, 1.5, 20, floors);
		worst = 0;
		for (int h = 1; h <= 20; h++)
		{
			worst = Math.Max(worst, Math.AbsFloat(floors[h] - floors[h - 1]));
		}
		Check(worst <= MUD_Build.SLOPE + 0.001, string.Format("profile hill steepest %1 m per m", worst));
	}

	//------------------------------------------------------------------------------------------------
	protected void Chain()
	{
		MUD_TrenchPlan plan = MUD_TrenchPlan.Unpack("1,6;0,0,0;400,0,0;400,400,0");
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		Check(plan && lay.Build(plan), "chain layout");
		if (!lay.Count())
			return;
		float off;
		float s = lay.ChainPos(4.3, 2, off);
		Check(Math.AbsFloat(s - 6) < 0.01 && Math.AbsFloat(off - 0.3) < 0.01, string.Format("chain pos %1 off %2, want 6 / 0.3", s, off));
		vector p = lay.ChainPoint(2);
		Check(vector.DistanceXZ(p, Vector(2, 0, 0)) < 0.01, string.Format("chain point %1", p));
		Check(Math.AbsFloat(lay.ChainLength() - 8) < 0.01, string.Format("chain length %1", lay.ChainLength()));
	}
}
