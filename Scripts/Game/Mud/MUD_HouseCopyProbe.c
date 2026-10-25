//! TEMP probe 2026-10-24: V2 building copy (docs/superpowers/research/2026-10-24-building-copy-voxel.md sec 5, 8-10).
//! TEST_KIT + diag exe / Workbench, SP only, nothing synced. Logs 'MUD house copy:'.
//! Insert (DOORS): the building under the crosshair (PICK_RANGE) gets its shell hidden + collision off exactly as
//! MUD_VoxelHouse.GoLive does (VISIBLE cleared on the root only, interaction layers 0); its children are not touched.
//! Every child is logged (prefab, visible, physics) now and RECHECK s later. Insert again: the shell comes back.
//! Page Down (COPY): the baked House_Village_E_1I01 (Tools/bake_house_copy.py, data version 3, Data/Mud/HouseCopy)
//! in place of the house under the crosshair: the vanilla MatPBRMulti on repacked UV1 (round 3 fix C, user
//! 2026-10-25: identical to vanilla). Per CELL_M cell one mesh entity + one static body per collider (FireGeo,
//! BuildingView), one geom per gamemat, each body on the shell geom's interaction layer. The shell is hidden and
//! its collision off. Page Down again: copy off, holes gone, shell back. Insert brings the shell back over the copy.
//! Round 3 HOLES (2026-10-25): End = hole HOLE_R at the crosshair, a blast near the house (MUD_WorldGround hook,
//! Blast) = hole of the MUD_VoxelHouse size; on a vanilla house either one first swaps the copy in (first breach).
//! Page Up = all holes off. A hole rebuilds only the cells it touches, from the baked data + the hole list:
//! visual triangles + collider faces inside the sphere go, crossing ones are split down to CUT_MIN / CUT_MIN_COLL
//! (MUD_HouseCopyCut); the cut wall gets a voxel rim (MUD_HouseCopyRim, surface nets on the baked FireGeo solid
//! grid) drawn with the rim brick emat and added to the brick geom of both colliders. Small children the sphere
//! reaches (doors, windows, CHILD_MAX) are hidden + collision off, restored when the copy goes.
//! Remove with: this file, its Tick line in MUD_MudLab.c, its Blast line in MUD_WorldGround.c,
//! Tools/bake_house_copy.py, Assets/Mud/HouseCopy, Data/Mud/HouseCopy.
class MUD_HouseCopyBuf
{
	static const int SIZE_V = 8000;
	static const int SIZE_UV = SIZE_V * 2;
	static const int SIZE_I = 24000;
	vector m_aV[SIZE_V];
	float m_aUV[SIZE_UV];
	int m_aI[SIZE_I];
}

//! One baked cell (Tools/bake_house_copy.py layout, version 3)
class MUD_HouseCopyCell
{
	vector m_vMin;
	vector m_vMax;
	vector m_vLo;                               // box of everything in the cell (triangles reach past the cell box)
	vector m_vHi;
	ref array<int> m_aSecKey = {};
	ref array<ref array<float>> m_aSecV = {};     // per section: x y z u v per vertex
	ref array<ref array<int>> m_aSecI = {};
	ref array<int> m_aGeoColl = {};
	ref array<int> m_aGeoGm = {};
	ref array<ref array<float>> m_aGeoV = {};     // per geom: x y z per vertex
	ref array<ref array<int>> m_aGeoI = {};
}

//! What one built cell owns (freed in DropCell: bodies, then geoms, then entities - mud-gotchas 2026-10-09)
class MUD_HouseCopyLive
{
	IEntity m_Mesh;
	ref Resource m_Res;
	ref array<IEntity> m_aBodies = {};
	ref array<PhysicsGeom> m_aGeoms = {};
	int m_iTris;
	int m_iRim;
	int m_iFaces;
}

//! FireGeo solid grid of the bake: signed distance (m, < 0 solid), int8 packed 4 per int
class MUD_HouseCopyGrid
{
	ref array<int> m_aW;
	int m_iNX;
	int m_iNY;
	int m_iNZ;
	float m_fG;
	float m_fShell;
	float m_fQ;             // metres per int8 step
	vector m_vOrg;          // house box min = sample (0, 0, 0)

	//------------------------------------------------------------------------------------------------
	//! Outside the grid = air
	float S(int i, int j, int k)
	{
		if (i < 0 || j < 0 || k < 0 || i >= m_iNX || j >= m_iNY || k >= m_iNZ)
			return 127 * m_fQ;
		int id = (k * m_iNY + j) * m_iNX + i;
		int b = (m_aW[id >> 2] >> ((id & 3) * 8)) & 0xFF;
		if (b > 127)
			b -= 256;
		return b * m_fQ;
	}
}

//! Cuts one section / geom against the holes of a cell: triangles inside a sphere go, crossing ones split at edge
//! midpoints (shared per edge, so two crossing neighbours stay crack-free) down to m_fMin, then kept by centre
class MUD_HouseCopyCut
{
	ref array<vector> m_aC = {};        // hole centres (model space)
	ref array<float> m_aR = {};
	ref array<vector> m_aV = {};        // out
	ref array<float> m_aUV = {};
	ref array<int> m_aI = {};
	ref map<int, int> m_mMid = new map<int, int>();
	bool m_bUV;
	float m_fMin;
	int m_iFull;                        // triangles a full buffer stopped

	//------------------------------------------------------------------------------------------------
	void Begin(array<float> src, int stride, float minEdge)
	{
		m_aV.Clear();
		m_aUV.Clear();
		m_aI.Clear();
		m_mMid.Clear();
		m_bUV = stride == 5;
		m_fMin = minEdge;
		int n = src.Count() / stride;
		for (int i = 0; i < n; i++)
		{
			int o = i * stride;
			m_aV.Insert(Vector(src[o], src[o + 1], src[o + 2]));
			if (m_bUV)
			{
				m_aUV.Insert(src[o + 3]);
				m_aUV.Insert(src[o + 4]);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	void Tris(array<int> idx)
	{
		array<int> stack = {};
		int cnt = idx.Count();
		for (int t = 0; t + 2 < cnt; t += 3)
		{
			if (m_aC.IsEmpty())
			{
				m_aI.Insert(idx[t]);
				m_aI.Insert(idx[t + 1]);
				m_aI.Insert(idx[t + 2]);
				continue;
			}
			stack.Insert(idx[t]);
			stack.Insert(idx[t + 1]);
			stack.Insert(idx[t + 2]);
			while (!stack.IsEmpty())
			{
				int n = stack.Count();
				int a = stack[n - 3];
				int b = stack[n - 2];
				int c = stack[n - 1];
				stack.Resize(n - 3);
				Tri(a, b, c, stack);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Tri(int a, int b, int c, array<int> stack)
	{
		vector pa = m_aV[a];
		vector pb = m_aV[b];
		vector pc = m_aV[c];
		int st = Classify(pa, pb, pc);
		if (st == 2)
			return;
		if (st == 1)
		{
			float e = Math.Max(vector.Distance(pa, pb), Math.Max(vector.Distance(pb, pc), vector.Distance(pc, pa)));
			if (e > m_fMin && m_aV.Count() + 3 <= MUD_HouseCopyBuf.SIZE_V)
			{
				int ab = Mid(a, b);
				int bc = Mid(b, c);
				int ca = Mid(c, a);
				stack.Insert(a); stack.Insert(ab); stack.Insert(ca);
				stack.Insert(ab); stack.Insert(b); stack.Insert(bc);
				stack.Insert(ca); stack.Insert(bc); stack.Insert(c);
				stack.Insert(ab); stack.Insert(bc); stack.Insert(ca);
				return;
			}
			if (Inside((pa + pb + pc) * (1.0 / 3.0)))
				return;
		}
		if (m_aI.Count() + 3 > MUD_HouseCopyBuf.SIZE_I)
		{
			m_iFull++;
			return;
		}
		m_aI.Insert(a);
		m_aI.Insert(b);
		m_aI.Insert(c);
	}

	//------------------------------------------------------------------------------------------------
	//! 0 clear of every hole, 1 crosses one, 2 inside one (goes)
	protected int Classify(vector a, vector b, vector c)
	{
		int st;
		for (int h = 0; h < m_aC.Count(); h++)
		{
			vector o = m_aC[h];
			float r2 = m_aR[h] * m_aR[h];
			if (vector.DistanceSq(a, o) <= r2 && vector.DistanceSq(b, o) <= r2 && vector.DistanceSq(c, o) <= r2)
				return 2;
			if (st == 0 && vector.DistanceSq(ClosestOnTri(o, a, b, c), o) < r2)
				st = 1;
		}
		return st;
	}

	//------------------------------------------------------------------------------------------------
	bool Inside(vector p)
	{
		for (int h = 0; h < m_aC.Count(); h++)
		{
			if (vector.DistanceSq(p, m_aC[h]) < m_aR[h] * m_aR[h])
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected int Mid(int a, int b)
	{
		int lo = a;
		int hi = b;
		if (b < a)
		{
			lo = b;
			hi = a;
		}
		int key = lo * 16384 + hi;     // ids < SIZE_V 8000
		int m;
		if (m_mMid.Find(key, m))
			return m;
		m = m_aV.Count();
		m_aV.Insert((m_aV[a] + m_aV[b]) * 0.5);
		if (m_bUV)
		{
			m_aUV.Insert((m_aUV[a * 2] + m_aUV[b * 2]) * 0.5);
			m_aUV.Insert((m_aUV[a * 2 + 1] + m_aUV[b * 2 + 1]) * 0.5);
		}
		m_mMid.Insert(key, m);
		return m;
	}

	//------------------------------------------------------------------------------------------------
	//! Closest point of triangle abc to p (Ericson, Real-Time Collision Detection 5.1.5)
	static vector ClosestOnTri(vector p, vector a, vector b, vector c)
	{
		vector ab = b - a;
		vector ac = c - a;
		vector ap = p - a;
		float d1 = vector.Dot(ab, ap);
		float d2 = vector.Dot(ac, ap);
		if (d1 <= 0 && d2 <= 0)
			return a;
		vector bp = p - b;
		float d3 = vector.Dot(ab, bp);
		float d4 = vector.Dot(ac, bp);
		if (d3 >= 0 && d4 <= d3)
			return b;
		float vc = d1 * d4 - d3 * d2;
		if (vc <= 0 && d1 >= 0 && d3 <= 0)
			return a + ab * (d1 / (d1 - d3));
		vector cp = p - c;
		float d5 = vector.Dot(ab, cp);
		float d6 = vector.Dot(ac, cp);
		if (d6 >= 0 && d5 <= d6)
			return c;
		float vb = d5 * d2 - d1 * d6;
		if (vb <= 0 && d2 >= 0 && d6 <= 0)
			return a + ac * (d2 / (d2 - d6));
		float va = d3 * d6 - d5 * d4;
		if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0)
			return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
		float sum = va + vb + vc;
		if (sum < 0.0000001)
			return a;
		return a + ab * (vb / sum) + ac * (vc / sum);
	}
}

//! Voxel rim of one cell: surface nets on F = max(solid distance, hole carve) over the grid edges whose base sample
//! the cell owns. Only edges whose air end is CARVED solid make a quad (the rest is the vanilla skin, drawn by the
//! cut copy), so the rim is the sphere surface inside the wall. Each edge belongs to one cell -> no seams.
class MUD_HouseCopyRim
{
	ref array<float> m_aF = {};
	ref array<float> m_aS = {};
	ref array<vector> m_aV = {};        // out, model space
	ref array<int> m_aI = {};
	ref map<int, int> m_mCube = new map<int, int>();
	protected MUD_HouseCopyGrid m_G;
	protected int m_iX0;                // local sample range origin (owned range - 1)
	protected int m_iY0;
	protected int m_iZ0;
	protected int m_iLX;
	protected int m_iLY;
	protected int m_iLZ;

	//------------------------------------------------------------------------------------------------
	//! Owned edge bases i0..i1 (inclusive) etc.; holes from cut.m_aC / m_aR
	void Build(MUD_HouseCopyGrid g, MUD_HouseCopyCut holes, int i0, int i1, int j0, int j1, int k0, int k1)
	{
		m_aV.Clear();
		m_aI.Clear();
		m_mCube.Clear();
		m_aF.Clear();
		m_aS.Clear();
		if (i1 < i0 || j1 < j0 || k1 < k0)
			return;
		m_G = g;
		m_iX0 = i0 - 1;
		m_iY0 = j0 - 1;
		m_iZ0 = k0 - 1;
		m_iLX = i1 - i0 + 3;
		m_iLY = j1 - j0 + 3;
		m_iLZ = k1 - k0 + 3;
		for (int k = 0; k < m_iLZ; k++)
		{
			for (int j = 0; j < m_iLY; j++)
			{
				for (int i = 0; i < m_iLX; i++)
				{
					float s = g.S(m_iX0 + i, m_iY0 + j, m_iZ0 + k);
					vector p = g.m_vOrg + Vector(m_iX0 + i, m_iY0 + j, m_iZ0 + k) * g.m_fG;
					float carve = -1000;
					for (int h = 0; h < holes.m_aC.Count(); h++)
						carve = Math.Max(carve, holes.m_aR[h] - vector.Distance(p, holes.m_aC[h]));
					m_aS.Insert(s);
					m_aF.Insert(Math.Max(s, carve));
				}
			}
		}
		for (int k2 = 1; k2 < m_iLZ - 1; k2++)
		{
			for (int j2 = 1; j2 < m_iLY - 1; j2++)
			{
				for (int i2 = 1; i2 < m_iLX - 1; i2++)
				{
					Edge(i2, j2, k2, 0);
					Edge(i2, j2, k2, 1);
					Edge(i2, j2, k2, 2);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int L(int i, int j, int k)
	{
		return (k * m_iLY + j) * m_iLX + i;
	}

	//------------------------------------------------------------------------------------------------
	//! Edge from local sample (i, j, k) along axis
	protected void Edge(int i, int j, int k, int axis)
	{
		int di;
		int dj;
		int dk;
		if (axis == 0)
			di = 1;
		else if (axis == 1)
			dj = 1;
		else
			dk = 1;
		int p = L(i, j, k);
		int q = L(i + di, j + dj, k + dk);
		float fp = m_aF[p];
		float fq = m_aF[q];
		bool inP = fp < 0;
		if (inP == (fq < 0))
			return;
		int air = q;
		if (!inP)
			air = p;
		if (m_aS[air] >= 0)
			return;     // a skin of the house, not a cut
		// the 4 cubes round the edge, in a ring over the other two axes
		int a0;
		int a1;
		int a2;
		int a3;
		if (axis == 0)
		{
			a0 = Cube(i, j - 1, k - 1);
			a1 = Cube(i, j, k - 1);
			a2 = Cube(i, j, k);
			a3 = Cube(i, j - 1, k);
		}
		else if (axis == 1)
		{
			a0 = Cube(i - 1, j, k - 1);
			a1 = Cube(i, j, k - 1);
			a2 = Cube(i, j, k);
			a3 = Cube(i - 1, j, k);
		}
		else
		{
			a0 = Cube(i - 1, j - 1, k);
			a1 = Cube(i, j - 1, k);
			a2 = Cube(i, j, k);
			a3 = Cube(i - 1, j, k);
		}
		// front face (cross(b - a, c - a), the xob / copy convention) towards the carved air end
		vector want = Vector(di, dj, dk);
		if (!inP)
			want = -want;
		vector e1 = m_aV[a1] - m_aV[a0];
		vector e2 = m_aV[a2] - m_aV[a0];
		vector n = Vector(e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]);
		if (vector.Dot(n, want) >= 0)
		{
			m_aI.Insert(a0); m_aI.Insert(a1); m_aI.Insert(a2);
			m_aI.Insert(a0); m_aI.Insert(a2); m_aI.Insert(a3);
		}
		else
		{
			m_aI.Insert(a0); m_aI.Insert(a2); m_aI.Insert(a1);
			m_aI.Insert(a0); m_aI.Insert(a3); m_aI.Insert(a2);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Surface-nets vertex of the cube at local base (i, j, k): mean of its sign-changing edge crossings
	protected int Cube(int i, int j, int k)
	{
		int key = L(i, j, k);
		int v;
		if (m_mCube.Find(key, v))
			return v;
		vector sum;
		int n;
		for (int c = 0; c < 8; c++)
		{
			int ci = c & 1;
			int cj = (c >> 1) & 1;
			int ck = (c >> 2) & 1;
			float f0 = m_aF[L(i + ci, j + cj, k + ck)];
			for (int bit = 0; bit < 3; bit++)
			{
				if (((c >> bit) & 1) != 0)
					continue;
				int ei = ci;
				int ej = cj;
				int ek = ck;
				if (bit == 0)
					ei++;
				else if (bit == 1)
					ej++;
				else
					ek++;
				float f1 = m_aF[L(i + ei, j + ej, k + ek)];
				if ((f0 < 0) == (f1 < 0))
					continue;
				float t = f0 / (f0 - f1);
				sum += Vector(ci, cj, ck) + (Vector(ei, ej, ek) - Vector(ci, cj, ck)) * t;
				n++;
			}
		}
		vector loc = Vector(0.5, 0.5, 0.5);
		if (n > 0)
			loc = sum * (1.0 / n);
		v = m_aV.Count();
		m_aV.Insert(m_G.m_vOrg + (Vector(m_iX0 + i, m_iY0 + j, m_iZ0 + k) + loc) * m_G.m_fG);
		m_mCube.Insert(key, v);
		return v;
	}
}

//! Blast hook: the House_Village_E_1I01 root in reach of a blast
class MUD_HouseCopyQuery
{
	IEntity m_Found;

	//------------------------------------------------------------------------------------------------
	bool Add(IEntity e)
	{
		if (!e || m_Found)
			return true;
		IEntity root = e.GetRootParent();
		VObject vo = root.GetVObject();
		if (vo && vo.GetResourceName().Contains(MUD_HouseCopyProbe.MODEL))
			m_Found = root;
		return true;
	}
}

class MUD_HouseCopyProbe
{
	static const bool ENABLED = true;
	static const string MODEL = "House_Village_E_1I01.xob";
	static const string DATA = "Data/Mud/HouseCopy/House_Village_E_1I01_multi.c";
	static const string NAMES = "Data/Mud/HouseCopy/House_Village_E_1I01_multi_names.c";
	static const int MAGIC = 1346589517;     // 'MUCP'
	static const int VERSION = 3;
	static const int MAX_SEC = 16;           // + 1 rim section
	static const int MAX_CELLS = 256;
	static const int MAX_GEOMS = 6;          // MakeStatic
	static const int MAX_HOLES = 64;
	static const float PICK_RANGE = 60;
	static const float RECHECK = 2;         // s
	static const float HOLE_R = 1.25;       // End
	static const float CUT_MIN = 0.12;      // visual triangles crossing a hole split down to this edge (m)
	static const float CUT_MIN_COLL = 0.25; // collider faces
	static const float RIM_TILE_M = 1.0;    // m per rim brick texture repeat (eyeballed)
	static const float CHILD_MAX = 4;       // as MUD_VoxelHouse: a child at most this big goes with a hole

	protected static float s_fLastT = -1;
	// doors probe
	protected static IEntity s_Shell;
	protected static ref array<int> s_aShellLayers;
	protected static float s_fRecheckAt = -1;
	// copy
	protected static IEntity s_House;
	protected static ref array<ref MUD_HouseCopyLive> s_aLive;     // per cell (null = copy off)
	protected static ref array<ref MUD_HouseCopyCell> s_aCells;
	protected static ref MUD_HouseCopyGrid s_Grid;
	protected static vector s_vBoxMin;
	protected static vector s_vBoxMax;
	protected static ref array<string> s_aEmat;     // per key
	protected static string s_sRim;
	protected static int s_iRimK;
	protected static ref array<string> s_aCollName; // per collider
	protected static ref array<string> s_aCollLayer;
	protected static ref array<ResourceName> s_aGm; // per gamemat
	protected static int s_iBrickGm;
	protected static ref array<int> s_aCollMask;    // per collider, read from the shell before it is hidden
	protected static ref MUD_HouseCopyBuf s_Buf;
	// holes (model space)
	protected static ref array<vector> s_aHoleC;
	protected static ref array<float> s_aHoleR;
	protected static ref array<IEntity> s_aGone;    // children a hole took
	protected static ref array<ref array<int>> s_aGoneLayers;
	protected static ref MUD_HouseCopyCut s_Cut;
	protected static ref MUD_HouseCopyRim s_Rim;

	//------------------------------------------------------------------------------------------------
	//! Every frame from MUD_MudLab.Tick
	static void Tick(float time)
	{
		if (!ENABLED || !MUD_TestKit.TEST_KIT || RplSession.Mode() != RplMode.None)
			return;
		if (time < s_fLastT)
		{
			s_Shell = null;     // new play: the old world's entities are gone (not freed: mud-gotchas play-stop crash)
			s_aShellLayers = null;
			s_fRecheckAt = -1;
			s_House = null;
			s_aLive = null;
			s_aHoleC = null;
			s_aHoleR = null;
			s_aGone = null;
			s_aGoneLayers = null;
		}
		s_fLastT = time;
#ifdef ENABLE_DIAG
		if (Debug.KeyState(KeyCode.KC_INSERT))
		{
			Debug.ClearKey(KeyCode.KC_INSERT);
			DoorsKey(time);
		}
		if (Debug.KeyState(KeyCode.KC_NEXT))
		{
			Debug.ClearKey(KeyCode.KC_NEXT);
			CopyKey();
		}
		if (Debug.KeyState(KeyCode.KC_END))
		{
			Debug.ClearKey(KeyCode.KC_END);
			HoleKey();
		}
		if (Debug.KeyState(KeyCode.KC_PRIOR))
		{
			Debug.ClearKey(KeyCode.KC_PRIOR);
			ResetHoles();
		}
#endif
		if (s_fRecheckAt >= 0 && time >= s_fRecheckAt)
		{
			s_fRecheckAt = -1;
			if (s_Shell)
				LogChildren(s_Shell, string.Format("%1 s later", RECHECK));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_WorldGroundEntity explosion hook (next to MUD_VoxelHouse.Blast): a blast of kg TNT at wp holes the copy,
	//! or swaps the copy in on a vanilla House_Village_E_1I01 it reaches (first breach) and holes that
	static void Blast(vector wp, float kg)
	{
		if (!ENABLED || !MUD_TestKit.TEST_KIT || RplSession.Mode() != RplMode.None)
			return;
		float r = Math.Clamp(0.6 + 0.9 * Math.Pow(Math.Max(kg, 0.01), 1.0 / 3.0), 0.8, 4);    // as MUD_VoxelHouse.Blast
		IEntity b = s_House;
		if (!s_aLive || !b || b.IsDeleted() || !Reaches(b, wp, r))
		{
			MUD_HouseCopyQuery q = new MUD_HouseCopyQuery();
			GetGame().GetWorld().QueryEntitiesBySphere(wp, r, q.Add, null, EQueryEntitiesFlags.ALL);
			b = q.m_Found;
			if (!b || !Reaches(b, wp, r))
				return;
		}
		if (!EnsureCopy(b))
			return;
		AddHole(ToModel(b, wp), r, string.Format("blast %1 kg", kg));
	}

	//------------------------------------------------------------------------------------------------
	//! wp within r of the baked house box
	protected static bool Reaches(IEntity b, vector wp, float r)
	{
		if (!s_aCells && !Load())
			return false;
		vector m = ToModel(b, wp);
		vector p = Vector(Math.Clamp(m[0], s_vBoxMin[0], s_vBoxMax[0]), Math.Clamp(m[1], s_vBoxMin[1], s_vBoxMax[1]), Math.Clamp(m[2], s_vBoxMin[2], s_vBoxMax[2]));
		return vector.Distance(p, m) <= r;
	}

	//------------------------------------------------------------------------------------------------
	protected static vector ToModel(IEntity b, vector wp)
	{
		vector mat[4];
		b.GetWorldTransform(mat);
		vector d = wp - mat[3];
		return Vector(vector.Dot(d, mat[0]) / mat[0].LengthSq(), vector.Dot(d, mat[1]) / mat[1].LengthSq(), vector.Dot(d, mat[2]) / mat[2].LengthSq());
	}

	//------------------------------------------------------------------------------------------------
	//! True for a mesh or body entity of the live copy
	protected static bool IsCopy(IEntity e)
	{
		if (!s_aLive || !e)
			return false;
		foreach (MUD_HouseCopyLive l : s_aLive)
		{
			if (l.m_Mesh == e || l.m_aBodies.Contains(e))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Root of what the crosshair hits within PICK_RANGE (Projectile layer), or null; hit = the hit point
	protected static IEntity Pick(out vector hit)
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		CameraManager cameras = GetGame().GetCameraManager();
		if (!cameras || !cameras.CurrentCamera())
			return null;
		vector cam[4];
		cameras.CurrentCamera().GetTransform(cam);
		TraceParam tr = new TraceParam();
		tr.Start = cam[3];
		tr.End = cam[3] + cam[2] * PICK_RANGE;
		tr.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		tr.LayerMask = EPhysicsLayerPresets.Projectile;
		tr.Exclude = player;
		float f = GetGame().GetWorld().TraceMove(tr, null);
		if (f >= 1 || !tr.TraceEnt)
			return null;
		hit = tr.Start + (tr.End - tr.Start) * f;
		if (IsCopy(tr.TraceEnt))
			return s_House;
		return tr.TraceEnt.GetRootParent();
	}

	//------------------------------------------------------------------------------------------------
	protected static string ModelOf(IEntity e)
	{
		VObject vo = e.GetVObject();
		if (!vo)
			return "(no model)";
		return FilePath.StripPath(vo.GetResourceName());
	}

	//------------------------------------------------------------------------------------------------
	protected static string PrefabOf(IEntity e)
	{
		EntityPrefabData pd = e.GetPrefabData();
		if (pd && pd.GetPrefabName() != "")
			return FilePath.StripPath(pd.GetPrefabName());
		return "(no prefab)";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Hint(string text)
	{
		SCR_HintManagerComponent.ShowCustomHint(text, "Mud house copy", 5, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Insert: hide / show the shell of the building under the crosshair
	protected static void DoorsKey(float time)
	{
		if (s_Shell)
		{
			string name = string.Format("%1", s_Shell);
			ShowShell();
			Print("MUD house copy: DOORS shell back - " + name, LogLevel.NORMAL);
			Hint("Shell back (Insert)");
			return;
		}
		vector hit;
		IEntity b = Pick(hit);
		if (!b)
		{
			Hint("Insert: nothing under the crosshair");
			return;
		}
		HideShell(b);
		Print(string.Format("MUD house copy: DOORS shell hidden - %1 prefab %2 model %3 at %4, %5 physics geoms off",
			b, PrefabOf(b), ModelOf(b), b.GetOrigin(), s_aShellLayers.Count() - 1), LogLevel.NORMAL);
		LogChildren(b, "now");
		s_fRecheckAt = time + RECHECK;
		Hint("Shell hidden (Insert = back). Check doors / windows: show, open, block you, bullets.");
	}

	//------------------------------------------------------------------------------------------------
	//! As MUD_VoxelHouse.Remove, layers kept for ShowShell (first = body, then one per geom)
	protected static void HideShell(IEntity b)
	{
		s_Shell = b;
		s_aShellLayers = {};
		b.ClearFlags(EntityFlags.VISIBLE, false);
		Physics ph = b.GetPhysics();
		if (!ph)
			return;
		s_aShellLayers.Insert(ph.GetInteractionLayer());
		int n = ph.GetNumGeoms();
		for (int g = 0; g < n; g++)
			s_aShellLayers.Insert(ph.GetGeomInteractionLayer(g));
		ph.SetInteractionLayer(0);
		for (int g2 = 0; g2 < n; g2++)
			ph.SetGeomInteractionLayer(g2, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ShowShell()
	{
		IEntity b = s_Shell;
		s_Shell = null;
		s_fRecheckAt = -1;
		if (!b || b.IsDeleted())
			return;
		b.SetFlags(EntityFlags.VISIBLE, false);
		Physics ph = b.GetPhysics();
		if (ph && s_aShellLayers && !s_aShellLayers.IsEmpty())
		{
			ph.SetInteractionLayer(s_aShellLayers[0]);
			int n = s_aShellLayers.Count() - 1;
			if (ph.GetNumGeoms() < n)
				n = ph.GetNumGeoms();
			for (int g = 0; g < n; g++)
				ph.SetGeomInteractionLayer(g, s_aShellLayers[g + 1]);
		}
		s_aShellLayers = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void LogChildren(IEntity b, string when)
	{
		int n;
		int shown;
		IEntity c = b.GetChildren();
		while (c)
		{
			n++;
			bool vis = (c.GetFlags() & EntityFlags.VISIBLE) != 0;
			if (vis)
				shown++;
			Physics ph = c.GetPhysics();
			int geoms = -1;
			int layer = -1;
			if (ph)
			{
				geoms = ph.GetNumGeoms();
				layer = ph.GetInteractionLayer();
			}
			Print(string.Format("MUD house copy: DOORS child (%1) %2 %3 model %4 visible %5 physics geoms %6 layer %7 grandchildren %8",
				when, c.ClassName(), PrefabOf(c), ModelOf(c), vis, geoms, layer, c.GetChildren() != null), LogLevel.NORMAL);
			c = c.GetSibling();
		}
		Print(string.Format("MUD house copy: DOORS (%1) %2 children, %3 with VISIBLE on, shell VISIBLE %4",
			when, n, shown, (b.GetFlags() & EntityFlags.VISIBLE) != 0), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Page Down: copy on (in place of the house under the crosshair) / off
	protected static void CopyKey()
	{
		if (s_aLive)
		{
			DropCopy();
			Print("MUD house copy: COPY off", LogLevel.NORMAL);
			Hint("Copy off (Page Down)");
			return;
		}
		vector hit;
		IEntity b = Pick(hit);
		if (!b || !ModelOf(b).Contains(MODEL))
		{
			string got = "nothing";
			if (b)
				got = ModelOf(b);
			Print("MUD house copy: COPY needs a " + MODEL + " under the crosshair, got " + got, LogLevel.NORMAL);
			Hint("Page Down: look at a House_Village_E_1I01 (got " + got + ")");
			return;
		}
		if (EnsureCopy(b))
			Hint("Copy on (Page Down = off). End = hole at the crosshair, Page Up = holes off, or throw a grenade.");
	}

	//------------------------------------------------------------------------------------------------
	//! End: hole HOLE_R at the crosshair (swaps the copy in first on a vanilla house)
	protected static void HoleKey()
	{
		vector hit;
		IEntity b = Pick(hit);
		if (!b || !ModelOf(b).Contains(MODEL))
		{
			Hint("End: look at a House_Village_E_1I01 (or its copy)");
			return;
		}
		if (!EnsureCopy(b))
			return;
		AddHole(ToModel(b, hit), HOLE_R, "End");
	}

	//------------------------------------------------------------------------------------------------
	//! The copy live on b (one house at a time): shell hidden + collision off, every cell built
	protected static bool EnsureCopy(IEntity b)
	{
		if (s_aLive && s_House == b)
			return true;
		if (s_aLive)
			DropCopy();
		if (!Load())
			return false;
		if (s_Shell)
			ShowShell();
		ReadCollMasks(b);
		HideShell(b);
		s_House = b;
		s_aHoleC = {};
		s_aHoleR = {};
		s_aGone = {};
		s_aGoneLayers = {};
		int t0 = System.GetTickCount();
		s_aLive = {};
		int tris;
		int geoms;
		int faces;
		for (int c = 0; c < s_aCells.Count(); c++)
		{
			MUD_HouseCopyLive live = new MUD_HouseCopyLive();
			s_aLive.Insert(live);
			BuildCell(c, live);
			tris += live.m_iTris;
			faces += live.m_iFaces;
			geoms += live.m_aGeoms.Count();
		}
		vector mat[4];
		b.GetWorldTransform(mat);
		Print(string.Format("MUD house copy: COPY on in place (shell hidden, its collision off) - %1 cells, %2 tris, %3 geoms / %4 collider faces, build %5 ms (WB), on %6, house scale %7 (copy assumes 1)",
			s_aCells.Count(), tris, geoms, faces, System.GetTickCount(t0), b, mat[0].Length()), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Per collider: the shell's geom of the same name -> its interaction layer (logged). Name not found but the shell
	//! has one geom per baked collider: geom c (INFERENCE: geoms in xob COLL order). Else the FireGeo preset.
	protected static void ReadCollMasks(IEntity b)
	{
		s_aCollMask = {};
		Physics ph = b.GetPhysics();
		int n = 0;
		if (ph)
			n = ph.GetNumGeoms();
		for (int c = 0; c < s_aCollName.Count(); c++)
		{
			int idx = -1;
			if (ph)
				idx = ph.GetGeom(s_aCollName[c]);
			int mask = EPhysicsLayerPresets.FireGeo;
			string from = "NOT FOUND - FireGeo preset";
			if (idx >= 0)
			{
				mask = ph.GetGeomInteractionLayer(idx);
				from = string.Format("shell geom %1 by name", idx);
			}
			else if (n == s_aCollName.Count())
			{
				mask = ph.GetGeomInteractionLayer(c);
				from = string.Format("shell geom %1 BY ORDER (name not found)", c);
			}
			s_aCollMask.Insert(mask);
			Print(string.Format("MUD house copy: COPY collider %1 (%2, %3) layer %4 from %5",
				c, s_aCollName[c], s_aCollLayer[c], mask, from), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Bodies, then geoms, then entities (mud-gotchas 2026-10-09: other orders crashed or leaked)
	protected static void DropCell(MUD_HouseCopyLive l)
	{
		foreach (IEntity e : l.m_aBodies)
		{
			if (!e)
				continue;
			Physics ph = e.GetPhysics();
			if (ph)
				ph.Destroy();
		}
		foreach (PhysicsGeom g : l.m_aGeoms)
		{
			if (g)
				g.Destroy();
		}
		foreach (IEntity e2 : l.m_aBodies)
		{
			if (e2)
				delete e2;
		}
		if (l.m_Mesh)
			delete l.m_Mesh;
		l.m_aBodies.Clear();
		l.m_aGeoms.Clear();
		l.m_Mesh = null;
		l.m_Res = null;
		l.m_iTris = 0;
		l.m_iRim = 0;
		l.m_iFaces = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Copy off: cells freed, children back, holes gone, shell back
	protected static void DropCopy()
	{
		if (s_aLive)
		{
			foreach (MUD_HouseCopyLive l : s_aLive)
				DropCell(l);
		}
		s_aLive = null;
		RestoreChildren();
		s_aHoleC = null;
		s_aHoleR = null;
		if (s_Shell && s_Shell == s_House)
			ShowShell();
		s_House = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Page Up: every hole off, the cells they touched rebuilt clean, children back
	protected static void ResetHoles()
	{
		if (!s_aLive || !s_aHoleC || s_aHoleC.IsEmpty())
		{
			Hint("Page Up: no holes");
			return;
		}
		array<int> touched = {};
		for (int c = 0; c < s_aCells.Count(); c++)
		{
			if (CellHoles(c, null))
				touched.Insert(c);
		}
		int n = s_aHoleC.Count();
		s_aHoleC.Clear();
		s_aHoleR.Clear();
		foreach (int c2 : touched)
		{
			DropCell(s_aLive[c2]);
			BuildCell(c2, s_aLive[c2]);
		}
		int kids = s_aGone.Count();
		RestoreChildren();
		Print(string.Format("MUD house copy: HOLES off - %1 holes, %2 cells rebuilt, %3 children back", n, touched.Count(), kids), LogLevel.NORMAL);
		Hint("Holes off (Page Up)");
	}

	//------------------------------------------------------------------------------------------------
	//! A hole at model-space centre m, radius r: the touched cells rebuild with every hole so far
	protected static void AddHole(vector m, float r, string why)
	{
		if (s_aHoleC.Count() >= MAX_HOLES)
		{
			Print(string.Format("MUD house copy: HOLE skipped (%1) - %2 holes is the probe's max", why, MAX_HOLES), LogLevel.NORMAL);
			return;
		}
		int t0 = System.GetTickCount();
		s_aHoleC.Insert(m);
		s_aHoleR.Insert(r);
		int last = s_aHoleC.Count() - 1;
		int cells;
		int trisOld;
		int trisNew;
		int rim;
		int facesOld;
		int facesNew;
		string list;
		for (int c = 0; c < s_aCells.Count(); c++)
		{
			if (!Touches(c, m, r))
				continue;
			MUD_HouseCopyLive l = s_aLive[c];
			trisOld += l.m_iTris;
			facesOld += l.m_iFaces;
			DropCell(l);
			BuildCell(c, l);
			trisNew += l.m_iTris;
			facesNew += l.m_iFaces;
			rim += l.m_iRim;
			cells++;
			list += " " + c.ToString();
		}
		int kids = RemoveChildren(m, r);
		Print(string.Format("MUD house copy: HOLE %1 (%2) at model %3 r %4 m - %5 cells rebuilt (%6), visual tris %7 -> %8 (rim %9)",
			last + 1, why, m, r, cells, list.Trim(), trisOld, trisNew, rim)
			+ string.Format(", collider faces %1 -> %2, %3 children gone, cut buffer stops %4, %5 ms (WB)",
			facesOld, facesNew, kids, s_Cut.m_iFull, System.GetTickCount(t0)), LogLevel.NORMAL);
		s_Cut.m_iFull = 0;
		Hint(string.Format("Hole %1: r %2 m, %3 cells rebuilt, rim %4 tris (Page Up = holes off)", last + 1, r, cells, rim));
	}

	//------------------------------------------------------------------------------------------------
	//! Sphere (m, r) touches cell c's triangles / faces or its grid edges (rim)
	protected static bool Touches(int c, vector m, float r)
	{
		MUD_HouseCopyCell cell = s_aCells[c];
		float g = s_Grid.m_fG;
		vector lo = Vector(Math.Min(cell.m_vLo[0], cell.m_vMin[0] - g), Math.Min(cell.m_vLo[1], cell.m_vMin[1] - g), Math.Min(cell.m_vLo[2], cell.m_vMin[2] - g));
		vector hi = Vector(Math.Max(cell.m_vHi[0], cell.m_vMax[0] + g), Math.Max(cell.m_vHi[1], cell.m_vMax[1] + g), Math.Max(cell.m_vHi[2], cell.m_vMax[2] + g));
		vector p = Vector(Math.Clamp(m[0], lo[0], hi[0]), Math.Clamp(m[1], lo[1], hi[1]), Math.Clamp(m[2], lo[2], hi[2]));
		return vector.Distance(p, m) <= r;
	}

	//------------------------------------------------------------------------------------------------
	//! The holes touching cell c into cut (cleared first; null = only count). Returns how many.
	protected static int CellHoles(int c, MUD_HouseCopyCut cut)
	{
		if (cut)
		{
			cut.m_aC.Clear();
			cut.m_aR.Clear();
		}
		int n;
		if (!s_aHoleC)
			return 0;
		for (int h = 0; h < s_aHoleC.Count(); h++)
		{
			if (!Touches(c, s_aHoleC[h], s_aHoleR[h]))
				continue;
			n++;
			if (cut)
			{
				cut.m_aC.Insert(s_aHoleC[h]);
				cut.m_aR.Insert(s_aHoleR[h]);
			}
		}
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! Mesh + bodies of cell c from the bake and the holes
	protected static void BuildCell(int c, MUD_HouseCopyLive live)
	{
		if (!s_Cut)
			s_Cut = new MUD_HouseCopyCut();
		if (!s_Rim)
			s_Rim = new MUD_HouseCopyRim();
		if (!s_Buf)
			s_Buf = new MUD_HouseCopyBuf();
		MUD_HouseCopyCell cell = s_aCells[c];
		int holes = CellHoles(c, s_Cut);
		s_Rim.m_aV.Clear();
		s_Rim.m_aI.Clear();
		if (holes > 0)
			BuildRim(cell);
		vector mat[4];
		s_House.GetWorldTransform(mat);
		BuildMesh(cell, live, mat);
		BuildBodies(cell, live, mat);
	}

	//------------------------------------------------------------------------------------------------
	//! Rim of the cell (s_Cut holds its holes): grid edges the cell owns, clamped to the holes' box
	protected static void BuildRim(MUD_HouseCopyCell cell)
	{
		float g = s_Grid.m_fG;
		vector org = s_Grid.m_vOrg;
		int i0 = Math.Round((cell.m_vMin[0] - org[0]) / g);
		int j0 = Math.Round((cell.m_vMin[1] - org[1]) / g);
		int k0 = Math.Round((cell.m_vMin[2] - org[2]) / g);
		int i1 = Math.Round((cell.m_vMax[0] - org[0]) / g) - 1;
		int j1 = Math.Round((cell.m_vMax[1] - org[1]) / g) - 1;
		int k1 = Math.Round((cell.m_vMax[2] - org[2]) / g) - 1;
		vector lo = Vector(1000000, 1000000, 1000000);
		vector hi = Vector(-1000000, -1000000, -1000000);
		for (int h = 0; h < s_Cut.m_aC.Count(); h++)
		{
			vector r = Vector(s_Cut.m_aR[h], s_Cut.m_aR[h], s_Cut.m_aR[h]);
			vector a = s_Cut.m_aC[h] - r;
			vector b = s_Cut.m_aC[h] + r;
			for (int x = 0; x < 3; x++)
			{
				lo[x] = Math.Min(lo[x], a[x]);
				hi[x] = Math.Max(hi[x], b[x]);
			}
		}
		i0 = Math.Max(Math.Max(i0, Math.Floor((lo[0] - org[0]) / g) - 1), -1);
		j0 = Math.Max(Math.Max(j0, Math.Floor((lo[1] - org[1]) / g) - 1), -1);
		k0 = Math.Max(Math.Max(k0, Math.Floor((lo[2] - org[2]) / g) - 1), -1);
		i1 = Math.Min(Math.Min(i1, Math.Ceil((hi[0] - org[0]) / g) + 1), s_Grid.m_iNX);
		j1 = Math.Min(Math.Min(j1, Math.Ceil((hi[1] - org[1]) / g) + 1), s_Grid.m_iNY);
		k1 = Math.Min(Math.Min(k1, Math.Ceil((hi[2] - org[2]) / g) + 1), s_Grid.m_iNZ);
		s_Rim.Build(s_Grid, s_Cut, i0, i1, j0, j1, k0, k1);
	}

	//------------------------------------------------------------------------------------------------
	//! Rim UV: box projection on the axis the hole's normal leans to most, RIM_TILE_M per repeat, s_iRimK repeats
	protected static void RimUV(vector p, vector cmin, out float u, out float v)
	{
		vector n = p - s_Cut.m_aC[0];
		float best = 1000000;
		for (int h = 0; h < s_Cut.m_aC.Count(); h++)
		{
			float d = Math.AbsFloat(vector.Distance(p, s_Cut.m_aC[h]) - s_Cut.m_aR[h]);
			if (d < best)
			{
				best = d;
				n = p - s_Cut.m_aC[h];
			}
		}
		vector q = (p - cmin + Vector(0.5, 0.5, 0.5)) * (1.0 / (RIM_TILE_M * s_iRimK));
		if (Math.AbsFloat(n[0]) >= Math.AbsFloat(n[1]) && Math.AbsFloat(n[0]) >= Math.AbsFloat(n[2]))
		{
			u = q[2];
			v = q[1];
		}
		else if (Math.AbsFloat(n[1]) >= Math.AbsFloat(n[2]))
		{
			u = q[0];
			v = q[2];
		}
		else
		{
			u = q[0];
			v = q[1];
		}
		u = Math.Clamp(u, 0, 1);
		v = Math.Clamp(v, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Opens path, then under each addon ID (as MUD_CoarseSkin.LoadBaked); null = none opened
	protected static FileHandle Open(string path)
	{
		array<string> paths = {path, "$Testingplanvechmsh:" + path, "$Terraindeformation:" + path};    // lint-ok: fallbacks after the plain path
		foreach (string p : paths)
		{
			FileHandle f = FileIO.OpenFile(p, FileMode.READ);
			if (f)
			{
				Print("MUD house copy: opened " + p, LogLevel.NORMAL);
				return f;
			}
		}
		Print(string.Format("MUD house copy: '%1' did not open (tried %2 paths)", path, paths.Count()), LogLevel.ERROR);
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Names file: 'S emat' per key, 'R rim emat K', 'C name layer' per collider, 'G gamemat' per gamemat
	protected static bool LoadNames()
	{
		s_aEmat = {};
		s_sRim = "";
		s_aCollName = {};
		s_aCollLayer = {};
		s_aGm = {};
		s_iBrickGm = -1;
		FileHandle fn = Open(NAMES);
		if (!fn)
			return false;
		string line;
		while (fn.ReadLine(line) >= 0)
		{
			line = line.Trim();
			if (line.IsEmpty())
				continue;
			array<string> parts = {};
			line.Split(" ", parts, true);
			bool ok = false;
			if (parts[0] == "S" && parts.Count() == 2)
			{
				s_aEmat.Insert(parts[1]);
				ok = true;
			}
			else if (parts[0] == "R" && parts.Count() == 3)
			{
				s_sRim = parts[1];
				s_iRimK = parts[2].ToInt();
				ok = s_iRimK > 0;
			}
			else if (parts[0] == "C" && parts.Count() == 3)
			{
				s_aCollName.Insert(parts[1]);
				s_aCollLayer.Insert(parts[2]);
				ok = true;
			}
			else if (parts[0] == "G" && parts.Count() == 2)
			{
				if (parts[1].Contains("/brick.gamemat"))
					s_iBrickGm = s_aGm.Count();
				s_aGm.Insert(parts[1]);
				ok = true;
			}
			if (!ok)
			{
				Print("MUD house copy: bad names line '" + line + "'", LogLevel.ERROR);
				fn.Close();
				return false;
			}
		}
		fn.Close();
		if (s_sRim.IsEmpty() || s_iBrickGm < 0)
		{
			Print(string.Format("MUD house copy: names file has no rim emat ('%1') or no brick gamemat (%2)", s_sRim, s_iBrickGm), LogLevel.ERROR);
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! count little-endian floats into a fresh array
	protected static array<float> ReadFloats(FileHandle f, int count)
	{
		array<float> a = {};
		a.Resize(count);
		f.ReadArray(a, 4, count);
		return a;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<int> ReadInts(FileHandle f, int count)
	{
		array<int> a = {};
		a.Resize(count);
		f.ReadArray(a, 4, count);
		return a;
	}

	//------------------------------------------------------------------------------------------------
	//! Grows the cell's content box over count points of stride floats
	protected static void Grow(MUD_HouseCopyCell cell, array<float> v, int stride)
	{
		int n = v.Count() / stride;
		for (int i = 0; i < n; i++)
		{
			int o = i * stride;
			for (int x = 0; x < 3; x++)
			{
				cell.m_vLo[x] = Math.Min(cell.m_vLo[x], v[o + x]);
				cell.m_vHi[x] = Math.Max(cell.m_vHi[x], v[o + x]);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One cell; false = bad counts (logged)
	protected static bool LoadCell(FileHandle f, MUD_HouseCopyCell cell, int keys, int colls, int gms)
	{
		array<float> box = ReadFloats(f, 6);
		cell.m_vMin = Vector(box[0], box[1], box[2]);
		cell.m_vMax = Vector(box[3], box[4], box[5]);
		cell.m_vLo = cell.m_vMin;
		cell.m_vHi = cell.m_vMax;
		int ns;
		f.Read(ns, 4);
		if (ns < 0 || ns > MAX_SEC)
		{
			Print(string.Format("MUD house copy: cell has %1 sections (max %2)", ns, MAX_SEC), LogLevel.ERROR);
			return false;
		}
		for (int s = 0; s < ns; s++)
		{
			int key;
			int nv;
			int ni;
			f.Read(key, 4);
			f.Read(nv, 4);
			f.Read(ni, 4);
			if (key < 0 || key >= keys || nv < 3 || ni < 3 || nv > MUD_HouseCopyBuf.SIZE_V || ni > MUD_HouseCopyBuf.SIZE_I)
			{
				Print(string.Format("MUD house copy: section key %1 / %2 verts / %3 indices out of range", key, nv, ni), LogLevel.ERROR);
				return false;
			}
			cell.m_aSecKey.Insert(key);
			cell.m_aSecV.Insert(ReadFloats(f, nv * 5));
			cell.m_aSecI.Insert(ReadInts(f, ni));
			Grow(cell, cell.m_aSecV[s], 5);
		}
		int ng;
		f.Read(ng, 4);
		if (ng < 0 || ng > MAX_GEOMS * colls)
		{
			Print(string.Format("MUD house copy: cell has %1 geoms (max %2)", ng, MAX_GEOMS * colls), LogLevel.ERROR);
			return false;
		}
		for (int g = 0; g < ng; g++)
		{
			int coll;
			int gm;
			int gv;
			int gi;
			f.Read(coll, 4);
			f.Read(gm, 4);
			f.Read(gv, 4);
			f.Read(gi, 4);
			if (coll < 0 || coll >= colls || gm < 0 || gm >= gms || gv < 3 || gi < 3 || gv > MUD_HouseCopyBuf.SIZE_V || gi > MUD_HouseCopyBuf.SIZE_I)
			{
				Print(string.Format("MUD house copy: geom collider %1 gamemat %2 / %3 verts / %4 indices out of range", coll, gm, gv, gi), LogLevel.ERROR);
				return false;
			}
			cell.m_aGeoColl.Insert(coll);
			cell.m_aGeoGm.Insert(gm);
			cell.m_aGeoV.Insert(ReadFloats(f, gv * 3));
			cell.m_aGeoI.Insert(ReadInts(f, gi));
			Grow(cell, cell.m_aGeoV[g], 3);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The FireGeo solid grid at the end of the data file
	protected static bool LoadGrid(FileHandle f)
	{
		array<int> n = ReadInts(f, 3);
		array<float> p = ReadFloats(f, 3);
		int words;
		f.Read(words, 4);
		int samples = n[0] * n[1] * n[2];
		if (n[0] < 2 || n[1] < 2 || n[2] < 2 || p[0] <= 0 || p[2] <= 0 || words * 4 < samples || words > 4000000)
		{
			Print(string.Format("MUD house copy: bad grid %1 x %2 x %3, spacing %4, qmax %5, %6 words", n[0], n[1], n[2], p[0], p[2], words), LogLevel.ERROR);
			return false;
		}
		s_Grid = new MUD_HouseCopyGrid();
		s_Grid.m_iNX = n[0];
		s_Grid.m_iNY = n[1];
		s_Grid.m_iNZ = n[2];
		s_Grid.m_fG = p[0];
		s_Grid.m_fShell = p[1];
		s_Grid.m_fQ = p[2] / 127;
		s_Grid.m_vOrg = s_vBoxMin;
		s_Grid.m_aW = ReadInts(f, words);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the bake once (layout: Tools/bake_house_copy.py docstring)
	protected static bool Load()
	{
		if (s_aCells && !s_aCells.IsEmpty())
			return true;
		s_aCells = null;
		int t0 = System.GetTickCount();
		if (!LoadNames())
			return false;
		FileHandle f = Open(DATA);
		if (!f)
			return false;
		array<int> head = ReadInts(f, 6);
		array<float> box = ReadFloats(f, 6);
		int keys = head[2];
		int colls = head[3];
		int gms = head[4];
		int cells = head[5];
		if (head[0] != MAGIC || head[1] != VERSION || keys != s_aEmat.Count() || colls != s_aCollName.Count()
			|| gms != s_aGm.Count() || cells < 1 || cells > MAX_CELLS)
		{
			Print(string.Format("MUD house copy: bad header (magic %1 version %2 keys %3 colliders %4 gamemats %5 cells %6; names %7 / %8 / %9)",
				head[0], head[1], keys, colls, gms, cells, s_aEmat.Count(), s_aCollName.Count(), s_aGm.Count()), LogLevel.ERROR);
			f.Close();
			return false;
		}
		s_vBoxMin = Vector(box[0], box[1], box[2]);
		s_vBoxMax = Vector(box[3], box[4], box[5]);
		array<ref MUD_HouseCopyCell> all = {};
		for (int c = 0; c < cells; c++)
		{
			MUD_HouseCopyCell cell = new MUD_HouseCopyCell();
			if (!LoadCell(f, cell, keys, colls, gms))
			{
				f.Close();
				return false;
			}
			all.Insert(cell);
		}
		bool grid = LoadGrid(f);
		bool whole = f.GetPos() == f.GetLength();
		f.Close();
		if (!grid || !whole)
		{
			Print(string.Format("MUD house copy: grid read %1, read to the file end %2", grid, whole), LogLevel.ERROR);
			return false;
		}
		s_aCells = all;
		Print(string.Format("MUD house copy: loaded %1 cells, %2 keys, %3 colliders, %4 gamemats, house box %5 .. %6",
			cells, keys, colls, gms, s_vBoxMin, s_vBoxMax) + string.Format(", grid %1 x %2 x %3 at %4 m (shell %5 m) in %6 ms",
			s_Grid.m_iNX, s_Grid.m_iNY, s_Grid.m_iNZ, s_Grid.m_fG, s_Grid.m_fShell, System.GetTickCount(t0)), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity SpawnAt(vector mat[4])
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = mat[0];
		params.Transform[1] = mat[1];
		params.Transform[2] = mat[2];
		params.Transform[3] = mat[3];
		return GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! The cell's mesh entity: one section per baked key (cut by the holes) + a rim section. Empty sections are left
	//! out (a cell all inside a hole has no mesh).
	protected static void BuildMesh(MUD_HouseCopyCell cell, MUD_HouseCopyLive live, vector mat[4])
	{
		array<ref array<vector>> outV = {};
		array<ref array<float>> outUV = {};
		array<ref array<int>> outI = {};
		array<string> outMat = {};
		for (int s = 0; s < cell.m_aSecKey.Count(); s++)
		{
			s_Cut.Begin(cell.m_aSecV[s], 5, CUT_MIN);
			s_Cut.Tris(cell.m_aSecI[s]);
			if (s_Cut.m_aI.IsEmpty())
				continue;
			array<vector> v = {};
			v.Copy(s_Cut.m_aV);
			array<float> uv = {};
			uv.Copy(s_Cut.m_aUV);
			array<int> idx = {};
			idx.Copy(s_Cut.m_aI);
			outV.Insert(v);
			outUV.Insert(uv);
			outI.Insert(idx);
			outMat.Insert(s_aEmat[cell.m_aSecKey[s]]);
		}
		int rimN = s_Rim.m_aV.Count();
		if (!s_Rim.m_aI.IsEmpty() && rimN <= MUD_HouseCopyBuf.SIZE_V && s_Rim.m_aI.Count() <= MUD_HouseCopyBuf.SIZE_I)
		{
			array<vector> rv = {};
			rv.Copy(s_Rim.m_aV);
			array<float> ruv = {};
			for (int i = 0; i < rimN; i++)
			{
				float u;
				float w;
				RimUV(rv[i], cell.m_vMin, u, w);
				ruv.Insert(u);
				ruv.Insert(w);
			}
			array<int> ri = {};
			ri.Copy(s_Rim.m_aI);
			outV.Insert(rv);
			outUV.Insert(ruv);
			outI.Insert(ri);
			outMat.Insert(s_sRim);
			live.m_iRim = ri.Count() / 3;
		}
		int n = outV.Count();
		if (n == 0)
			return;
		int nv[MAX_SEC + 1];
		int ni[MAX_SEC + 1];
		string mats[MAX_SEC + 1];
		for (int s2 = 0; s2 < n; s2++)
		{
			nv[s2] = outV[s2].Count();
			ni[s2] = outI[s2].Count();
			mats[s2] = outMat[s2];
		}
		Resource res = MeshObject.Create(n, nv, ni, mats, 0);
		if (!res || !res.GetResource())
		{
			Print("MUD house copy: MeshObject.Create failed", LogLevel.ERROR);
			return;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		for (int s3 = 0; s3 < n; s3++)
		{
			array<vector> sv = outV[s3];
			array<float> suv = outUV[s3];
			array<int> si = outI[s3];
			for (int k = 0; k < sv.Count(); k++)
			{
				s_Buf.m_aV[k] = sv[k];
				s_Buf.m_aUV[k * 2] = suv[k * 2];
				s_Buf.m_aUV[k * 2 + 1] = suv[k * 2 + 1];
			}
			for (int k2 = 0; k2 < si.Count(); k2++)
				s_Buf.m_aI[k2] = si[k2];
			mesh.UpdateIndices(s3, s_Buf.m_aI);   // indices first: UpdateVerts rebuilds normals from them
			mesh.UpdateVerts(s3, s_Buf.m_aV, s_Buf.m_aUV);
			live.m_iTris += si.Count() / 3;
		}
		IEntity ent = SpawnAt(mat);
		if (!ent)
		{
			Print("MUD house copy: spawn failed", LogLevel.ERROR);
			return;
		}
		ent.SetObject(mesh, "");
		ent.Update();
		live.m_Mesh = ent;
		live.m_Res = res;
	}

	//------------------------------------------------------------------------------------------------
	//! One static body per collider in this cell, one trimesh geom per gamemat (cut by the holes); the rim goes into
	//! the collider's brick geom (a new one, or the last one when the body already has MAX_GEOMS)
	protected static void BuildBodies(MUD_HouseCopyCell cell, MUD_HouseCopyLive live, vector mat[4])
	{
		for (int c = 0; c < s_aCollName.Count(); c++)
		{
			array<ref array<vector>> gv = {};
			array<ref array<int>> gi = {};
			array<int> gm = {};
			for (int k = 0; k < cell.m_aGeoColl.Count(); k++)
			{
				if (cell.m_aGeoColl[k] != c)
					continue;
				s_Cut.Begin(cell.m_aGeoV[k], 3, CUT_MIN_COLL);
				s_Cut.Tris(cell.m_aGeoI[k]);
				if (s_Cut.m_aI.IsEmpty())
					continue;
				array<vector> v = {};
				v.Copy(s_Cut.m_aV);
				array<int> idx = {};
				idx.Copy(s_Cut.m_aI);
				gv.Insert(v);
				gi.Insert(idx);
				gm.Insert(cell.m_aGeoGm[k]);
			}
			if (!s_Rim.m_aI.IsEmpty())
				AddRimGeom(gv, gi, gm);
			MakeBody(c, gv, gi, gm, live, mat);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddRimGeom(array<ref array<vector>> gv, array<ref array<int>> gi, array<int> gm)
	{
		int at = gm.Find(s_iBrickGm);
		if (at < 0 && gm.Count() < MAX_GEOMS)
		{
			gv.Insert(new array<vector>());
			gi.Insert(new array<int>());
			gm.Insert(s_iBrickGm);
			at = gm.Count() - 1;
		}
		if (at < 0)
			at = gm.Count() - 1;
		array<vector> v = gv[at];
		array<int> idx = gi[at];
		if (v.Count() + s_Rim.m_aV.Count() > MUD_HouseCopyBuf.SIZE_V || idx.Count() + s_Rim.m_aI.Count() > MUD_HouseCopyBuf.SIZE_I)
		{
			Print("MUD house copy: rim collider skipped - geom buffer full", LogLevel.WARNING);
			return;
		}
		int base = v.Count();
		foreach (vector p : s_Rim.m_aV)
			v.Insert(p);
		foreach (int i : s_Rim.m_aI)
			idx.Insert(base + i);
	}

	//------------------------------------------------------------------------------------------------
	protected static void MakeBody(int c, array<ref array<vector>> gv, array<ref array<int>> gi, array<int> gm, MUD_HouseCopyLive live, vector mat[4])
	{
		array<PhysicsGeom> g = {};
		array<ResourceName> m = {};
		array<int> l = {};
		for (int k = 0; k < gv.Count(); k++)
		{
			array<vector> v = gv[k];
			array<int> idx = gi[k];
			int cnt = v.Count();
			int ic = idx.Count();
			if (ic < 3)
				continue;
			for (int i = 0; i < cnt; i++)
				s_Buf.m_aV[i] = v[i];
			for (int j = 0; j < ic; j++)
				s_Buf.m_aI[j] = idx[j];
			PhysicsGeom geom = PhysicsGeom.CreateTriMesh(s_Buf.m_aV, s_Buf.m_aI, cnt, ic);
			if (!geom)
			{
				Print(string.Format("MUD house copy: collider %1 geom failed (%2 verts / %3 indices)", c, cnt, ic), LogLevel.ERROR);
				continue;
			}
			live.m_aGeoms.Insert(geom);
			g.Insert(geom);
			m.Insert(s_aGm[gm[k]]);
			l.Insert(s_aCollMask[c]);
			live.m_iFaces += ic / 3;
		}
		if (g.IsEmpty())
			return;
		IEntity ent = SpawnAt(mat);
		if (!ent)
		{
			Print("MUD house copy: body spawn failed", LogLevel.ERROR);
			return;
		}
		live.m_aBodies.Insert(ent);
		if (!MakeStatic(ent, g, m, l))
			Print(string.Format("MUD house copy: collider %1 body failed (%2 geoms)", c, g.Count()), LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! As MUD_VoxelHouse.RemoveChildren: visible children of the house (any depth) up to CHILD_MAX whose world box the
	//! hole (model centre m, r) reaches are hidden + collision off; layers kept for RestoreChildren
	protected static int RemoveChildren(vector m, float r)
	{
		vector mat[4];
		s_House.GetWorldTransform(mat);
		vector wp = mat[3] + mat[0] * m[0] + mat[1] * m[1] + mat[2] * m[2];
		array<IEntity> todo = {};
		IEntity c = s_House.GetChildren();
		while (c)
		{
			todo.Insert(c);
			c = c.GetSibling();
		}
		int gone;
		while (!todo.IsEmpty())
		{
			IEntity e = todo[todo.Count() - 1];
			todo.Remove(todo.Count() - 1);
			IEntity k = e.GetChildren();
			while (k)
			{
				todo.Insert(k);
				k = k.GetSibling();
			}
			if ((e.GetFlags() & EntityFlags.VISIBLE) == 0 || s_aGone.Contains(e))
				continue;
			vector lo;
			vector hi;
			e.GetWorldBounds(lo, hi);
			vector size = hi - lo;
			if (size[0] > CHILD_MAX || size[1] > CHILD_MAX || size[2] > CHILD_MAX)
				continue;
			vector p = Vector(Math.Clamp(wp[0], lo[0], hi[0]), Math.Clamp(wp[1], lo[1], hi[1]), Math.Clamp(wp[2], lo[2], hi[2]));
			if (vector.Distance(p, wp) > r)
				continue;
			array<int> layers = {};
			e.ClearFlags(EntityFlags.VISIBLE, false);
			Physics ph = e.GetPhysics();
			if (ph)
			{
				layers.Insert(ph.GetInteractionLayer());
				for (int g = 0; g < ph.GetNumGeoms(); g++)
					layers.Insert(ph.GetGeomInteractionLayer(g));
				ph.SetInteractionLayer(0);
				for (int g2 = 0; g2 < ph.GetNumGeoms(); g2++)
					ph.SetGeomInteractionLayer(g2, 0);
			}
			s_aGone.Insert(e);
			s_aGoneLayers.Insert(layers);
			gone++;
		}
		return gone;
	}

	//------------------------------------------------------------------------------------------------
	protected static void RestoreChildren()
	{
		if (!s_aGone)
			return;
		for (int n = 0; n < s_aGone.Count(); n++)
		{
			IEntity e = s_aGone[n];
			if (!e || e.IsDeleted())
				continue;
			e.SetFlags(EntityFlags.VISIBLE, false);
			Physics ph = e.GetPhysics();
			array<int> layers = s_aGoneLayers[n];
			if (!ph || layers.IsEmpty())
				continue;
			ph.SetInteractionLayer(layers[0]);
			int cnt = Math.Min(ph.GetNumGeoms(), layers.Count() - 1);
			for (int g = 0; g < cnt; g++)
				ph.SetGeomInteractionLayer(g, layers[g + 1]);
		}
		s_aGone.Clear();
		s_aGoneLayers.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Static body from 1-6 geoms (copy of MUD_CoarseSkin.MakeStatic: CreateStaticEx takes a fixed-size array)
	protected static Physics MakeStatic(IEntity ent, array<PhysicsGeom> g, array<ResourceName> m, array<int> l)
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
}
