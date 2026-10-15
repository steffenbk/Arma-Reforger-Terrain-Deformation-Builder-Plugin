//! TEMP probe 2026-10-13: WALL PANELS for the voxel house (user: "try 3", flat wall panels with holes cut in them).
//! After MUD_VoxelHouse samples the building, the solid samples are split into axis boxes (greedy, building frame).
//! A box thin on one axis (<= PANEL_THIN samples) and wide on both others (>= PANEL_MIN) is a panel: a flat slab
//! (wall, floor, ceiling) whose two faces come from rays at the real collider. Its samples leave the voxel grid; the
//! rest (roof slopes, chimneys, stairs) stays voxels. A panel is cut into chunks of at most CHUNK m (one mesh +
//! collider each, UVs 0..1 per chunk). A chunk with no hole is a plain box (2 tris per face). A hole is a circle on
//! the panel (the blast sphere where it meets the slab, rough edge from two sines) cut straight through: the chunk's
//! faces come from marching squares at RES over the hole field, the hole walls are extruded contour segments.
class MUD_PanelChunk
{
	IEntity m_Ent;
	ref Resource m_Res;
	ref array<PhysicsGeom> m_aGeoms = {};
	float m_fU0;
	float m_fU1;
	float m_fV0;
	float m_fV1;
}

class MUD_WallPanel
{
	int m_iT;                           // thin axis (building local 0 x, 1 y, 2 z); u = axis T + 1, v = axis T + 2
	float m_fU0;
	float m_fU1;
	float m_fV0;
	float m_fV1;
	float m_fT0;
	float m_fT1;
	bool m_bRayed;                      // faces from rays (else from the samples)
	ref array<float> m_aHole = {};      // u, v, radius, phase per hole
	ref array<ref MUD_PanelChunk> m_aChunks = {};
}

class MUD_HousePanels
{
	static const int PANEL_THIN = 4;    // samples: a panel is at most this thick (1 m at 0.25)
	static const int PANEL_MIN = 4;     // samples: and at least this wide on both other axes
	static const float CHUNK = 4;       // chunk side (m); MUD_VoxelPlaster.emat tiles 2 per 0..1
	static const float RES = 0.125;     // hole grid step (m)
	static const int MAX_V = 8000;      // as MUD_VoxelVerts
	static const int MAX_I = 24000;     // as MUD_VoxelIdx
	static const int KEYS = 3300;       // corner / x-edge / y-edge keys of one face (33 x 33 grid at most)

	ref array<ref MUD_WallPanel> m_aPanels = {};
	int m_iBoxes;
	int m_iMoved;                       // samples that went into panels
	int m_iRayed;
	int m_iChunks;

	protected IEntity m_House;
	protected vector m_vX;              // building axes + origin (world)
	protected vector m_vY;
	protected vector m_vZ;
	protected vector m_vO;
	protected ResourceName m_sMat;
	protected ResourceName m_sGameMat;

	// build state
	protected ref MUD_VoxelVerts m_Vb = new MUD_VoxelVerts();
	protected ref MUD_VoxelIdx m_Ib = new MUD_VoxelIdx();
	protected int m_aKs[6600];          // key stamp (front keys, back keys + KEYS... see FaceV)
	protected int m_aKv[6600];
	protected int m_iStamp;
	protected int m_iV;
	protected int m_iI;
	protected bool m_bOverflow;
	protected MUD_WallPanel m_P;
	protected MUD_PanelChunk m_C;
	protected vector m_vCOrg;           // chunk origin, building local
	protected ref array<int> m_aCH = {};        // holes reaching the chunk
	protected ref array<float> m_aF = {};       // field per grid point
	protected int m_iNu;
	protected int m_iNv;
	protected float m_fDu;
	protected float m_fDv;
	protected float m_aPu[8];           // one cell's solid polygon
	protected float m_aPv[8];
	protected int m_aPk[8];             // face key
	protected int m_aPx[8];             // 1 = on a crossing (else a corner); int, not bool (mud-gotchas 2026-10-15)

	//------------------------------------------------------------------------------------------------
	void Init(IEntity house, vector x, vector y, vector z, vector o, ResourceName mat, ResourceName gameMat)
	{
		m_House = house;
		m_vX = x;
		m_vY = y;
		m_vZ = z;
		m_vO = o;
		m_sMat = mat;
		m_sGameMat = gameMat;
	}

	//------------------------------------------------------------------------------------------------
	//! Building-local point from panel coordinates (u, v on the panel, t across it) of thin axis ta
	static vector PV(int ta, float u, float v, float t)
	{
		int ua = (ta + 1) % 3;
		float c[3];
		c[ua] = u;
		c[(ta + 2) % 3] = v;
		c[ta] = t;
		return Vector(c[0], c[1], c[2]);
	}

	//------------------------------------------------------------------------------------------------
	protected vector ToWorld(vector local)
	{
		return m_vO + m_vX * local[0] + m_vY * local[1] + m_vZ * local[2];
	}

	//------------------------------------------------------------------------------------------------
	//! Greedy axis boxes over the solid samples (d > 0, id = (k * sy + j) * sx + i, sample at org + i * cell);
	//! panel boxes go out of d (set to air). Returns the panels made.
	int Extract(array<float> d, int sx, int sy, int sz, vector org, float cell)
	{
		int total = d.Count();
		array<int> used = {};
		used.Resize(total);
		for (int n = 0; n < total; n++)
			used[n] = 0;
		for (int k = 0; k < sz; k++)
		{
			for (int j = 0; j < sy; j++)
			{
				for (int i = 0; i < sx; i++)
				{
					int id = (k * sy + j) * sx + i;
					if (d[id] <= 0 || used[id] != 0)
						continue;
					// grow +x, then +y (whole row free), then +z (whole slab free)
					int i1 = i;
					while (i1 + 1 < sx && d[id + i1 + 1 - i] > 0 && used[id + i1 + 1 - i] == 0)
						i1++;
					int j1 = j;
					while (j1 + 1 < sy && BoxFree(d, used, sx, sy, i, i1, j1 + 1, j1 + 1, k, k))
						j1++;
					int k1 = k;
					while (k1 + 1 < sz && BoxFree(d, used, sx, sy, i, i1, j, j1, k1 + 1, k1 + 1))
						k1++;
					m_iBoxes++;
					int nx = i1 - i + 1;
					int ny = j1 - j + 1;
					int nz = k1 - k + 1;
					int ta = 0;
					int thin = nx;
					if (ny < thin)
					{
						ta = 1;
						thin = ny;
					}
					if (nz < thin)
					{
						ta = 2;
						thin = nz;
					}
					int wideA = ny;
					int wideB = nz;
					if (ta == 1)
					{
						wideA = nz;
						wideB = nx;
					}
					else if (ta == 2)
					{
						wideA = nx;
						wideB = ny;
					}
					bool panel = thin <= PANEL_THIN && wideA >= PANEL_MIN && wideB >= PANEL_MIN;
					for (int bk = k; bk <= k1; bk++)
					{
						for (int bj = j; bj <= j1; bj++)
						{
							for (int bi = i; bi <= i1; bi++)
							{
								int b = (bk * sy + bj) * sx + bi;
								used[b] = 1;
								if (panel)
									d[b] = -1;
							}
						}
					}
					if (!panel)
						continue;
					m_iMoved += nx * ny * nz;
					vector lo = org + Vector(i * cell, j * cell, k * cell);
					vector hi = org + Vector(i1 * cell, j1 * cell, k1 * cell);
					MakePanel(ta, lo, hi);
				}
			}
		}
		return m_aPanels.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool BoxFree(array<float> d, array<int> used, int sx, int sy, int i0, int i1, int j0, int j1, int k0, int k1)
	{
		for (int k = k0; k <= k1; k++)
		{
			for (int j = j0; j <= j1; j++)
			{
				int row = (k * sy + j) * sx;
				for (int i = i0; i <= i1; i++)
				{
					if (d[row + i] <= 0 || used[row + i] != 0)
						return false;
				}
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! lo / hi = first / last sample positions (building local). A sample is solid when the collider is within
	//! SAMPLE_R of it, so an edge lies within about +-0.13 m of its last solid sample: the samples are the edges.
	//! The two faces come from rays at the real collider where they find both.
	protected void MakePanel(int ta, vector lo, vector hi)
	{
		MUD_WallPanel p = new MUD_WallPanel();
		p.m_iT = ta;
		int ua = (ta + 1) % 3;
		int va = (ta + 2) % 3;
		p.m_fU0 = lo[ua];
		p.m_fU1 = hi[ua];
		p.m_fV0 = lo[va];
		p.m_fV1 = hi[va];
		float mid = (lo[ta] + hi[ta]) * 0.5;
		float half = Math.Max((hi[ta] - lo[ta]) * 0.5, 0.06);
		p.m_fT0 = mid - half;
		p.m_fT1 = mid + half;
		RayFaces(p);
		// chunks: equal pieces of at most CHUNK
		int cu = Math.Ceil((p.m_fU1 - p.m_fU0) / CHUNK - 0.001);
		int cv = Math.Ceil((p.m_fV1 - p.m_fV0) / CHUNK - 0.001);
		cu = Math.Max(cu, 1);
		cv = Math.Max(cv, 1);
		float wu = (p.m_fU1 - p.m_fU0) / cu;
		float wv = (p.m_fV1 - p.m_fV0) / cv;
		for (int b = 0; b < cv; b++)
		{
			for (int a = 0; a < cu; a++)
			{
				MUD_PanelChunk c = new MUD_PanelChunk();
				c.m_fU0 = p.m_fU0 + a * wu;
				c.m_fU1 = p.m_fU0 + (a + 1) * wu;
				c.m_fV0 = p.m_fV0 + b * wv;
				c.m_fV1 = p.m_fV0 + (b + 1) * wv;
				p.m_aChunks.Insert(c);
			}
		}
		m_aPanels.Insert(p);
	}

	//------------------------------------------------------------------------------------------------
	//! Faces from rays across the slab at 3 points (Include = the building, collision still on): the + face is the
	//! highest hit coming from +t, the - face the lowest from -t, each kept within 0.15 m of the sample slab
	protected void RayFaces(MUD_WallPanel p)
	{
		float hiT = -100000;
		float loT = 100000;
		float vm = (p.m_fV0 + p.m_fV1) * 0.5;
		for (int s = 1; s <= 3; s++)
		{
			float u = p.m_fU0 + (p.m_fU1 - p.m_fU0) * s * 0.25;
			float t;
			if (Ray(p, u, vm, p.m_fT1 + 0.6, p.m_fT0 - 0.15, t) && t >= p.m_fT0 - 0.15 && t <= p.m_fT1 + 0.15)
				hiT = Math.Max(hiT, t);
			if (Ray(p, u, vm, p.m_fT0 - 0.6, p.m_fT1 + 0.15, t) && t >= p.m_fT0 - 0.15 && t <= p.m_fT1 + 0.15)
				loT = Math.Min(loT, t);
		}
		if (hiT > loT + 0.04)
		{
			p.m_fT0 = loT;
			p.m_fT1 = hiT;
			p.m_bRayed = true;
			m_iRayed++;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool Ray(MUD_WallPanel p, float u, float v, float tFrom, float tTo, out float tHit)
	{
		TraceParam tr = new TraceParam();
		tr.Start = ToWorld(PV(p.m_iT, u, v, tFrom));
		tr.End = ToWorld(PV(p.m_iT, u, v, tTo));
		tr.Flags = TraceFlags.ENTS;
		tr.LayerMask = EPhysicsLayerPresets.Projectile;
		tr.Include = m_House;
		float f = GetGame().GetWorld().TraceMove(tr, null);
		if (f >= 1)
			return false;
		tHit = tFrom + (tTo - tFrom) * f;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every chunk (hidden on the first build). Returns ms.
	int BuildAll(bool first)
	{
		int t0 = System.GetTickCount();
		foreach (MUD_WallPanel p : m_aPanels)
		{
			foreach (MUD_PanelChunk c : p.m_aChunks)
				BuildChunk(p, c, first);
		}
		return System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	void Show()
	{
		foreach (MUD_WallPanel p : m_aPanels)
		{
			foreach (MUD_PanelChunk c : p.m_aChunks)
			{
				if (c.m_Ent)
					c.m_Ent.SetFlags(EntityFlags.VISIBLE, false);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	bool Owns(IEntity e)
	{
		foreach (MUD_WallPanel p : m_aPanels)
		{
			foreach (MUD_PanelChunk c : p.m_aChunks)
			{
				if (c.m_Ent && c.m_Ent == e)
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Sphere at building-local lc, r: a hole in every panel it reaches. Returns chunks rebuilt.
	int Carve(vector lc, float r, out int holes)
	{
		holes = 0;
		int rebuilt;
		foreach (MUD_WallPanel p : m_aPanels)
		{
			int ta = p.m_iT;
			int ua = (ta + 1) % 3;
			int va = (ta + 2) % 3;
			float dt = 0;
			if (lc[ta] < p.m_fT0)
				dt = p.m_fT0 - lc[ta];
			else if (lc[ta] > p.m_fT1)
				dt = lc[ta] - p.m_fT1;
			if (dt >= r)
				continue;
			float re = Math.Sqrt(r * r - dt * dt);
			if (re < 0.15)
				continue;
			float u = lc[ua];
			float v = lc[va];
			if (u + re < p.m_fU0 || u - re > p.m_fU1 || v + re < p.m_fV0 || v - re > p.m_fV1)
				continue;
			int nh = p.m_aHole.Count() / 4;
			p.m_aHole.Insert(u);
			p.m_aHole.Insert(v);
			p.m_aHole.Insert(re);
			p.m_aHole.Insert(nh * 2.399 + u * 1.7 + v * 0.9);
			holes++;
			float reach = re * 1.25 + RES;
			foreach (MUD_PanelChunk c : p.m_aChunks)
			{
				if (u + reach < c.m_fU0 || u - reach > c.m_fU1 || v + reach < c.m_fV0 || v - reach > c.m_fV1)
					continue;
				BuildChunk(p, c, false);
				rebuilt++;
			}
		}
		return rebuilt;
	}

	//------------------------------------------------------------------------------------------------
	void Free()
	{
		foreach (MUD_WallPanel p : m_aPanels)
		{
			foreach (MUD_PanelChunk c : p.m_aChunks)
				Drop(c);
		}
		m_aPanels.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Body through the live entity only (mud-gotchas 2026-10-13 19:19 crash), then geoms, then the entity
	protected void Drop(MUD_PanelChunk c)
	{
		if (c.m_Ent)
		{
			Physics ph = c.m_Ent.GetPhysics();
			if (ph)
				ph.Destroy();
			foreach (PhysicsGeom g : c.m_aGeoms)
			{
				if (g)
					g.Destroy();
			}
			delete c.m_Ent;
		}
		c.m_aGeoms.Clear();
		c.m_Ent = null;
		c.m_Res = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Hole field at u, v (> 0 solid): distance to the nearest hole edge, rough by two sines of the angle
	protected float Field(float u, float v)
	{
		float f = 1000;
		foreach (int h : m_aCH)
		{
			float du = u - m_P.m_aHole[h * 4];
			float dv = v - m_P.m_aHole[h * 4 + 1];
			float r = m_P.m_aHole[h * 4 + 2];
			float dist = Math.Sqrt(du * du + dv * dv);
			if (dist - r * 1.25 > f)
				continue;
			float ph = m_P.m_aHole[h * 4 + 3];
			float ang = Math.Atan2(dv, du);
			float g = dist - r * (1 + 0.12 * Math.Sin(3 * ang + ph) + 0.08 * Math.Sin(5 * ang + 2 * ph));
			if (g < f)
				f = g;
		}
		if (f > -0.002 && f < 0.002)
			f = -0.002;
		return f;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildChunk(MUD_WallPanel p, MUD_PanelChunk c, bool first)
	{
		m_P = p;
		m_C = c;
		m_aCH.Clear();
		int nh = p.m_aHole.Count() / 4;
		for (int h = 0; h < nh; h++)
		{
			float hu = p.m_aHole[h * 4];
			float hv = p.m_aHole[h * 4 + 1];
			float reach = p.m_aHole[h * 4 + 2] * 1.25 + RES;
			if (hu + reach < c.m_fU0 || hu - reach > c.m_fU1 || hv + reach < c.m_fV0 || hv - reach > c.m_fV1)
				continue;
			m_aCH.Insert(h);
		}
		m_iNu = 1;
		m_iNv = 1;
		if (!m_aCH.IsEmpty())
		{
			int nu = Math.Ceil((c.m_fU1 - c.m_fU0) / RES);
			int nv = Math.Ceil((c.m_fV1 - c.m_fV0) / RES);
			m_iNu = Math.ClampInt(nu, 1, 32);
			m_iNv = Math.ClampInt(nv, 1, 32);
		}
		m_fDu = (c.m_fU1 - c.m_fU0) / m_iNu;
		m_fDv = (c.m_fV1 - c.m_fV0) / m_iNv;
		int gw = m_iNu + 1;
		m_aF.Resize(gw * (m_iNv + 1));
		for (int jv = 0; jv <= m_iNv; jv++)
		{
			for (int iu = 0; iu <= m_iNu; iu++)
				m_aF[jv * gw + iu] = Field(c.m_fU0 + iu * m_fDu, c.m_fV0 + jv * m_fDv);
		}
		m_iStamp++;
		m_iV = 0;
		m_iI = 0;
		m_bOverflow = false;
		m_vCOrg = PV(p.m_iT, c.m_fU0, c.m_fV0, p.m_fT0);

		for (int cj = 0; cj < m_iNv; cj++)
		{
			for (int ci = 0; ci < m_iNu; ci++)
				Cell(ci, cj);
		}
		// slab edges where this chunk sits on the panel edge
		if (c.m_fU0 <= p.m_fU0 + 0.0001)
		{
			for (int a = 0; a < m_iNv; a++)
				Border(0, a, 0, a + 1, PV(p.m_iT, -1, 0, 0));
		}
		if (c.m_fU1 >= p.m_fU1 - 0.0001)
		{
			for (int b = 0; b < m_iNv; b++)
				Border(m_iNu, b, m_iNu, b + 1, PV(p.m_iT, 1, 0, 0));
		}
		if (c.m_fV0 <= p.m_fV0 + 0.0001)
		{
			for (int e = 0; e < m_iNu; e++)
				Border(e, 0, e + 1, 0, PV(p.m_iT, 0, -1, 0));
		}
		if (c.m_fV1 >= p.m_fV1 - 0.0001)
		{
			for (int g = 0; g < m_iNu; g++)
				Border(g, m_iNv, g + 1, m_iNv, PV(p.m_iT, 0, 1, 0));
		}
		if (m_bOverflow)
		{
			Print(string.Format("MUD wall panels: chunk overflow (%1 verts, %2 holes) - not built", m_iV, m_aCH.Count()), LogLevel.ERROR);
			return;
		}
		Drop(c);
		if (m_iI == 0)
			return;     // all hole
		MakeEnt(c, first);
	}

	//------------------------------------------------------------------------------------------------
	protected float CornerF(int iu, int jv)
	{
		return m_aF[jv * (m_iNu + 1) + iu];
	}

	//------------------------------------------------------------------------------------------------
	//! Crossing on the grid edge from corner (iu0, jv0) to (iu1, jv1), always from the lower corner (shared
	//! by both cells: the same floats)
	protected void Cross2(int iu0, int jv0, int iu1, int jv1, out float u, out float v)
	{
		float fa = CornerF(iu0, jv0);
		float fb = CornerF(iu1, jv1);
		float s = fa / (fa - fb);
		float ua = m_C.m_fU0 + iu0 * m_fDu;
		float va = m_C.m_fV0 + jv0 * m_fDv;
		u = ua + (m_C.m_fU0 + iu1 * m_fDu - ua) * s;
		v = va + (m_C.m_fV0 + jv1 * m_fDv - va) * s;
	}

	//------------------------------------------------------------------------------------------------
	//! One marching-squares cell: solid polygon (corners + crossings in order), fan on both faces, hole walls on
	//! the contour segments (two crossings in a row)
	protected void Cell(int iu, int jv)
	{
		int gw = m_iNu + 1;
		int cu[4] = {iu, iu + 1, iu + 1, iu};
		int cv[4] = {jv, jv, jv + 1, jv + 1};
		// edge keys: 0 = x edge (iu, jv), 1 = y edge (iu + 1, jv), 2 = x edge (iu, jv + 1), 3 = y edge (iu, jv)
		int ek[4] = {1100 + jv * m_iNu + iu, 2200 + jv * gw + iu + 1, 1100 + (jv + 1) * m_iNu + iu, 2200 + jv * gw + iu};
		int n = 0;
		for (int q = 0; q < 4; q++)
		{
			int q1 = (q + 1) % 4;
			float fq = CornerF(cu[q], cv[q]);
			float fn = CornerF(cu[q1], cv[q1]);
			if (fq > 0)
			{
				m_aPu[n] = m_C.m_fU0 + cu[q] * m_fDu;
				m_aPv[n] = m_C.m_fV0 + cv[q] * m_fDv;
				m_aPk[n] = cv[q] * gw + cu[q];
				m_aPx[n] = 0;
				n++;
			}
			if ((fq > 0) != (fn > 0))
			{
				float xu;
				float xv;
				// lower corner first: edges 0, 1 run up from q; edges 2, 3 run down
				if (q < 2)
					Cross2(cu[q], cv[q], cu[q1], cv[q1], xu, xv);
				else
					Cross2(cu[q1], cv[q1], cu[q], cv[q], xu, xv);
				m_aPu[n] = xu;
				m_aPv[n] = xv;
				m_aPk[n] = ek[q];
				m_aPx[n] = 1;
				n++;
			}
		}
		if (n < 3)
			return;
		int ta = m_P.m_iT;
		vector up = PV(ta, 0, 0, 1);
		vector down = PV(ta, 0, 0, -1);
		int f0 = FaceV(m_aPk[0], m_aPu[0], m_aPv[0], false);
		int b0 = FaceV(m_aPk[0], m_aPu[0], m_aPv[0], true);
		for (int t = 1; t < n - 1; t++)
		{
			Tri(f0, FaceV(m_aPk[t], m_aPu[t], m_aPv[t], false), FaceV(m_aPk[t + 1], m_aPu[t + 1], m_aPv[t + 1], false), up);
			Tri(b0, FaceV(m_aPk[t], m_aPu[t], m_aPv[t], true), FaceV(m_aPk[t + 1], m_aPu[t + 1], m_aPv[t + 1], true), down);
		}
		for (int s = 0; s < n; s++)
		{
			int s1 = (s + 1) % n;
			if (m_aPx[s] == 0 || m_aPx[s1] == 0)
				continue;
			// toward the air: the side of the segment where the field is lower
			float du = m_aPu[s1] - m_aPu[s];
			float dv = m_aPv[s1] - m_aPv[s];
			float nu = dv;
			float nv = -du;
			float mu = (m_aPu[s] + m_aPu[s1]) * 0.5;
			float mv = (m_aPv[s] + m_aPv[s1]) * 0.5;
			if (Field(mu + nu * 0.05, mv + nv * 0.05) > Field(mu - nu * 0.05, mv - nv * 0.05))
			{
				nu = -nu;
				nv = -nv;
			}
			Side(m_aPu[s], m_aPv[s], m_aPu[s1], m_aPv[s1], PV(ta, nu, nv, 0));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Slab edge along the grid edge corner a -> corner b (on the panel edge): its solid part
	protected void Border(int iua, int jva, int iub, int jvb, vector want)
	{
		float fa = CornerF(iua, jva);
		float fb = CornerF(iub, jvb);
		float ua = m_C.m_fU0 + iua * m_fDu;
		float va = m_C.m_fV0 + jva * m_fDv;
		float ub = m_C.m_fU0 + iub * m_fDu;
		float vb = m_C.m_fV0 + jvb * m_fDv;
		if (fa > 0 && fb > 0)
		{
			Side(ua, va, ub, vb, want);
			return;
		}
		if (fa <= 0 && fb <= 0)
			return;
		float xu;
		float xv;
		Cross2(iua, jva, iub, jvb, xu, xv);
		if (fa > 0)
			Side(ua, va, xu, xv, want);
		else
			Side(xu, xv, ub, vb, want);
	}

	//------------------------------------------------------------------------------------------------
	//! A wall across the slab under segment a - b (own vertices: a hard edge), facing want
	protected void Side(float ua, float va, float ub, float vb, vector want)
	{
		int a1 = NewV(ua, va, m_P.m_fT1);
		int b1 = NewV(ub, vb, m_P.m_fT1);
		int b0 = NewV(ub, vb, m_P.m_fT0);
		int a0 = NewV(ua, va, m_P.m_fT0);
		Tri(a1, b1, b0, want);
		Tri(a1, b0, a0, want);
	}

	//------------------------------------------------------------------------------------------------
	//! Shared face vertex by key (front t1 / back t0), UVs 0..1 across the chunk
	protected int FaceV(int key, float u, float v, bool back)
	{
		int k = key;
		if (back)
			k += KEYS;
		if (m_aKs[k] == m_iStamp)
			return m_aKv[k];
		if (m_iV >= MAX_V)
		{
			m_bOverflow = true;
			return 0;
		}
		float t = m_P.m_fT1;
		if (back)
			t = m_P.m_fT0;
		int vi = m_iV;
		m_Vb.m_aV[vi] = PV(m_P.m_iT, u, v, t) - m_vCOrg;
		m_Vb.m_aUV[vi * 2] = Math.Clamp((u - m_C.m_fU0) / CHUNK, 0, 1);
		m_Vb.m_aUV[vi * 2 + 1] = 1 - Math.Clamp((v - m_C.m_fV0) / CHUNK, 0, 1);
		m_iV++;
		m_aKs[k] = m_iStamp;
		m_aKv[k] = vi;
		return vi;
	}

	//------------------------------------------------------------------------------------------------
	protected int NewV(float u, float v, float t)
	{
		if (m_iV >= MAX_V)
		{
			m_bOverflow = true;
			return 0;
		}
		int vi = m_iV;
		m_Vb.m_aV[vi] = PV(m_P.m_iT, u, v, t) - m_vCOrg;
		m_Vb.m_aUV[vi * 2] = Math.Clamp(((u - m_C.m_fU0) + (v - m_C.m_fV0)) / (2 * CHUNK), 0, 1);
		m_Vb.m_aUV[vi * 2 + 1] = 1 - Math.Clamp((t - m_P.m_fT0) / CHUNK, 0, 1);
		m_iV++;
		return vi;
	}

	//------------------------------------------------------------------------------------------------
	//! Faces want (flipped when not); drops zero-area ones
	protected void Tri(int a, int b, int c, vector want)
	{
		if (m_bOverflow)
			return;
		if (m_iI + 3 > MAX_I)
		{
			m_bOverflow = true;
			return;
		}
		if (a == b || b == c || a == c)
			return;
		vector pa = m_Vb.m_aV[a];
		vector n = Cross(m_Vb.m_aV[b] - pa, m_Vb.m_aV[c] - pa);
		if (n.LengthSq() < 0.000000000001)
			return;
		if (vector.Dot(n, want) < 0)
		{
			int tmp = b;
			b = c;
			c = tmp;
		}
		m_Ib.m_aI[m_iI] = a;
		m_Ib.m_aI[m_iI + 1] = b;
		m_Ib.m_aI[m_iI + 2] = c;
		m_iI += 3;
	}

	//------------------------------------------------------------------------------------------------
	//! The built buffers as mesh + static trimesh on a new entity at the chunk origin (building frame)
	protected void MakeEnt(MUD_PanelChunk c, bool first)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = m_vX;
		params.Transform[1] = m_vY;
		params.Transform[2] = m_vZ;
		params.Transform[3] = ToWorld(m_vCOrg);
		IEntity ent = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
		if (!ent)
			return;
		if (first)
			ent.ClearFlags(EntityFlags.VISIBLE, false);
		int nv[] = {m_iV};
		int ni[] = {m_iI};
		string mats[] = {m_sMat};
		Resource res = MeshObject.Create(1, nv, ni, mats, 0);
		if (!res || !res.GetResource())
		{
			Print("MUD wall panels: MeshObject.Create failed", LogLevel.ERROR);
			delete ent;
			return;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		mesh.UpdateIndices(0, m_Ib.m_aI);
		mesh.UpdateVerts(0, m_Vb.m_aV, m_Vb.m_aUV);
		ent.SetObject(mesh, "");
		ent.Update();
		c.m_Ent = ent;
		c.m_Res = res;
		m_iChunks++;
		PhysicsGeom g = PhysicsGeom.CreateTriMesh(m_Vb.m_aV, m_Ib.m_aI, m_iV, m_iI);
		if (!g)
		{
			Print("MUD wall panels: collider geom failed", LogLevel.ERROR);
			return;
		}
		int layer = EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage;
		autoptr PhysicsGeomDef defs[] = {PhysicsGeomDef("", g, m_sGameMat, layer)};
		Physics phys = Physics.CreateStaticEx(ent, defs);
		c.m_aGeoms.Insert(g);
		if (!phys)
			Print("MUD wall panels: collider failed", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected static vector Cross(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}
}
