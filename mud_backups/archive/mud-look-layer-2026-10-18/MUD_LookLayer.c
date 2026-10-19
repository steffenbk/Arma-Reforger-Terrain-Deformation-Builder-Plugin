//! Mud look layer (user go 2026-10-17; docs/features/mud-look/README.md): the mud sheet's MudRunner look as a
//! CLIENT-ONLY drawn mesh on top of the felt rut, near the camera only (MUD_WorldGroundEntity.LOOK_LAYER /
//! LOOK_RADIUS). A rut patch in range draws a 0.125 m mesh (65 x 65) instead of its 0.25 m one: a narrow tyre groove
//! with a dirt ridge right at its wall (the sheet's BOX + SOFT profile), over the wide felt bowl. Collider, ops, net,
//! hashes and the snapshot never see it. A DS never builds it (m_bRender false).
//! SAME ON EVERY CLIENT: the look is a pure function of replicated data - the rut floor (MUD_VoxelProbe.FieldFloor,
//! never the client look ruts), the patch base heights and the edge lock. No time, no vehicle state, no randomness.
//! The camera only decides WHETHER a patch gets the look.
//! Maths = Tools/mud_look_proto.py look_fast (Stage 1, results/2026-10-17-mud-look-fast.md: same table as look_run):
//! per 8 m patch + 0.75 m margin on the 0.125 m grid - centre samples (>= 0.9 x 5 x 5 local max) -> two-pass chamfer
//! carrying the nearest centre (x, z, depth) -> 5 x 5 blur of lat / Dc -> profile.
//! Buffers live in holder classes (a class holds at most 256 KB, static arrays at most 65536 entries).

//! Inputs of one build (MUD_WorldGroundEntity.BuildMeshLook fills them)
class MUD_LookIn
{
	float m_aFeltV[1089];       // world y the plain mesh would draw at each patch vertex (before the groove reveal)
	float m_aRevV[1089];        // groove reveal input per vertex (rfD in BuildMesh)
	float m_aBaseV[1089];       // undug world y (p.m_aBase)
	float m_aLockV[1089];       // edge lock (p.m_aEdgeLock): 0 at kept terrain (road) -> the look fades out there
	float m_aDepV[1521];        // rut floor depth under the undug ground (>= 0, edge-locked), 39 x 39 fine verts = patch +- 3
	int m_aDiag[1444];          // 1 = the fine cell's diagonal is tl-br (skin TLBR), 38 x 38 cells = patch +- 3
	int m_aCell[16];            // the patch's m_aCell (index layout)
}

//! Per-sample work, 77 x 77 samples (patch + 6 samples margin each side)
class MUD_LookWorkA
{
	float m_aDep[5929];
	float m_aTmp[5929];
	float m_aBest[5929];        // squared distance (samples^2) to the nearest centre found
	float m_aCx[5929];          // that centre, sample coords
	float m_aCz[5929];
	float m_aCd[5929];          // its depth
}

class MUD_LookWorkB
{
	float m_aLat[5929];         // blurred distance to the track centre (m)
	float m_aDc[5929];          // blurred centre depth (m)
	float m_aTmp[5929];
}

//! Per output vertex (65 x 65)
class MUD_LookOut
{
	float m_aLook[4225];        // drawn height relative to the undug ground (groove < 0, ridge > 0)
	float m_aDirt[4225];        // 0..1: lower the top here (groove reveal = dirt)
	int m_aTyre[4225];          // 1 = inside the drawn tyre strip
	int m_aHas[4225];           // 1 = a look is drawn here (centre depth > HAS_D)
}

//! Mesh buffers: surface verts [0, 4225) + skirt copies [4225, 4745); under layer 4225
class MUD_LookBuf
{
	vector m_aVerts[4745];
	float m_aUVs[9490];
	vector m_aUnder[4225];
	float m_aGrooveUVs[8450];
}

//! Index list of one layout: surface tris [0, 24576) (the under layer uses only these), skirts [24576, 26112)
class MUD_LookIdx
{
	int m_aCell[16];
	int m_aIdx[26112];
}

class MUD_LookLayer
{
	static const int NV = 65;               // output verts per side (0.125 m)
	static const int NS = 77;               // samples per side incl. margin
	static const int M = 6;                 // margin samples (0.75 m)
	static const int NI = 24576;            // surface indices (64 x 64 x 2 x 3)
	static const int NI_ALL = 26112;        // + skirts (4 x 64 x 6)
	static const int NV_ALL = 4745;         // + skirt copies (4 x 130)
	static const int IDX_RING = 4;
	static const float BIG = 1000000000;
	// profile (Tools/mud_look_proto.py, sheet constants MUD_MudSheet.c); Stage 3 tunes these with the user
	static const float LOOK_HALF = 0.21;        // drawn tyre half width
	static const float LOOK_RIDGE_K = 0.35;     // look ridge = K x centre depth, capped HMAX
	static const float LOOK_LIFT = 0.02;        // drawn groove floor this far over the felt floor at the centre
	static const float SOFT_WALL = 0.35;        // sheet SOFT_WALL: cosine wall over this share of the half width
	static const float RIDGE_TOP = 0.35;        // sheet RIDGE_TOP: ridge crest position in the band
	static const float HMAX = 0.28;
	static const float BAND_OUT = 0.5833;       // LOOK_HALF + BAND 0.32 x LOOK_HALF / HALF 0.18: ridge band outer edge
	static const float CENTRE_D = 0.02;         // a centre sample is deeper than this ...
	static const float CENTRE_K = 0.9;          // ... and at least this share of the 5 x 5 local max
	static const float HAS_D = 0.02;            // centre depth needed for any look
	static const float DIRT_D = 0.01;           // dirt from this depth / ridge height ...
	static const float DIRT_BLEND = 0.02;       // ... full dirt this much further

	ref MUD_LookIn m_In = new MUD_LookIn();
	ref MUD_LookWorkA m_A = new MUD_LookWorkA();
	ref MUD_LookWorkB m_B = new MUD_LookWorkB();
	ref MUD_LookOut m_Out = new MUD_LookOut();
	ref MUD_LookBuf m_Buf = new MUD_LookBuf();
	ref array<ref MUD_LookIdx> m_aIdx = {};
	int m_iIdxNext;
	int m_iIdxMisses;

	//------------------------------------------------------------------------------------------------
	//! Value at a 0.125 m sample inside a fine cell from its corners (u2 / w2 = 1: half way), on the triangles the
	//! 0.25 m mesh draws: an edge middle is the edge mean, the cell centre the mean of the diagonal's ends
	static float Mid(float h00, float h10, float h01, float h11, int u2, int w2, int diag)
	{
		if (u2 == 0 && w2 == 0)
			return h00;
		if (w2 == 0)
			return (h00 + h10) * 0.5;
		if (u2 == 0)
			return (h00 + h01) * 0.5;
		if (diag == 1)
			return (h01 + h10) * 0.5;
		return (h00 + h11) * 0.5;
	}

	//------------------------------------------------------------------------------------------------
	//! Depth at sample (i, k) of the 77 x 77 grid (sample 2j = margin vertex j)
	protected float DepAt(int i, int k)
	{
		int jx = i / 2;
		int jz = k / 2;
		int u2 = i - jx * 2;
		int w2 = k - jz * 2;
		int v = jz * 39 + jx;
		float h00 = m_In.m_aDepV[v];
		float h10 = h00;
		float h01 = h00;
		float h11 = h00;
		if (u2 == 1)
			h10 = m_In.m_aDepV[v + 1];
		if (w2 == 1)
			h01 = m_In.m_aDepV[v + 39];
		if (u2 == 1 && w2 == 1)
			h11 = m_In.m_aDepV[v + 40];
		int diag = 0;
		if (u2 == 1 && w2 == 1)
			diag = m_In.m_aDiag[jz * 38 + jx];
		return Mid(h00, h10, h01, h11, u2, w2, diag);
	}

	//------------------------------------------------------------------------------------------------
	//! Look heights + dirt for every output vertex from m_In (pure function of it). step = sample spacing (m)
	void Compute(float step)
	{
		int n = NS * NS;
		// depth per sample
		for (int k = 0; k < NS; k++)
		{
			for (int i = 0; i < NS; i++)
				m_A.m_aDep[k * NS + i] = DepAt(i, k);
		}
		// 5 x 5 local max (separable, edges clamped): rows into m_aTmp, then columns into m_B.m_aTmp
		for (int rk = 0; rk < NS; rk++)
		{
			for (int ri = 0; ri < NS; ri++)
			{
				float mx = -1;
				for (int d = -2; d <= 2; d++)
				{
					int ci = Math.ClampInt(ri + d, 0, NS - 1);
					float dv = m_A.m_aDep[rk * NS + ci];
					if (dv > mx)
						mx = dv;
				}
				m_A.m_aTmp[rk * NS + ri] = mx;
			}
		}
		for (int ck = 0; ck < NS; ck++)
		{
			for (int cx2 = 0; cx2 < NS; cx2++)
			{
				float my = -1;
				for (int d2 = -2; d2 <= 2; d2++)
				{
					int rr = Math.ClampInt(ck + d2, 0, NS - 1);
					float tv = m_A.m_aTmp[rr * NS + cx2];
					if (tv > my)
						my = tv;
				}
				m_B.m_aTmp[ck * NS + cx2] = my;
			}
		}
		// centre samples seed the chamfer
		for (int s = 0; s < n; s++)
		{
			float dep = m_A.m_aDep[s];
			if (dep > CENTRE_D && dep >= CENTRE_K * m_B.m_aTmp[s])
			{
				int sk = s / NS;
				m_A.m_aBest[s] = 0;
				m_A.m_aCx[s] = s - sk * NS;
				m_A.m_aCz[s] = sk;
				m_A.m_aCd[s] = dep;
			}
			else
			{
				m_A.m_aBest[s] = BIG;
				m_A.m_aCx[s] = 0;
				m_A.m_aCz[s] = 0;
				m_A.m_aCd[s] = 0;
			}
		}
		Chamfer(1);
		Chamfer(-1);
		// lat (m) into m_A.m_aTmp, then the 5 x 5 blur of lat and Dc on the output area (+- 2 rows for the columns)
		for (int ls = 0; ls < n; ls++)
		{
			if (m_A.m_aBest[ls] < BIG)
				m_A.m_aTmp[ls] = Math.Sqrt(m_A.m_aBest[ls]) * step;
			else
				m_A.m_aTmp[ls] = 99;
		}
		Blur();
		// profile per output vertex
		float inw = LOOK_HALF * SOFT_WALL;
		float band = BAND_OUT - LOOK_HALF;
		for (int oz = 0; oz < NV; oz++)
		{
			for (int ox = 0; ox < NV; ox++)
			{
				int o = oz * NV + ox;
				int so = (oz + M) * NS + ox + M;
				float lat = m_B.m_aLat[so];
				float dc = m_B.m_aDc[so];
				float dp = m_A.m_aDep[so];
				float dirt = Math.Clamp((dp - DIRT_D) / DIRT_BLEND, 0, 1);
				m_Out.m_aLook[o] = 0;
				m_Out.m_aTyre[o] = 0;
				m_Out.m_aHas[o] = 0;
				if (dc > HAS_D && lat < BAND_OUT)
				{
					m_Out.m_aHas[o] = 1;
					if (lat < LOOK_HALF)
					{
						m_Out.m_aTyre[o] = 1;
						float fs = LOOK_HALF - lat;
						float pf = 1;
						if (fs < inw)
							pf = 0.5 * (1 - Math.Cos(Math.PI * fs / inw));
						m_Out.m_aLook[o] = -(dc - LOOK_LIFT) * pf;
					}
					else
					{
						float x = (lat - LOOK_HALF) / band;
						float w;
						if (x > RIDGE_TOP)
							w = 0.5 * (1 + Math.Cos(Math.PI * (x - RIDGE_TOP) / (1 - RIDGE_TOP)));
						else
							w = Math.Sin(0.5 * Math.PI * x / RIDGE_TOP);
						float hL = LOOK_RIDGE_K * dc;
						if (hL > HMAX)
							hL = HMAX;
						float rg = hL * w;
						m_Out.m_aLook[o] = rg;
						float dr = Math.Clamp((rg - DIRT_D) / DIRT_BLEND, 0, 1);
						if (dr > dirt)
							dirt = dr;
					}
				}
				m_Out.m_aDirt[o] = dirt;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One chamfer pass (dir 1 = forward from the top-left, -1 = backward); each sample takes a neighbour's centre if
	//! that centre is nearer to it
	protected void Chamfer(int dir)
	{
		int dI[] = {-1, -1, 0, 1};
		int dK[] = {0, -1, -1, -1};
		int k0 = 0;
		int k1 = NS;
		if (dir < 0)
		{
			k0 = NS - 1;
			k1 = -1;
		}
		for (int k = k0; k != k1; k += dir)
		{
			for (int ii = 0; ii < NS; ii++)
			{
				int i = ii;
				if (dir < 0)
					i = NS - 1 - ii;
				int s = k * NS + i;
				for (int q = 0; q < 4; q++)
				{
					int ni = i + dI[q] * dir;
					int nk = k + dK[q] * dir;
					if (ni < 0 || ni >= NS || nk < 0 || nk >= NS)
						continue;
					int ns = nk * NS + ni;
					if (m_A.m_aBest[ns] >= BIG)
						continue;
					float ex = i - m_A.m_aCx[ns];
					float ez = k - m_A.m_aCz[ns];
					float d2 = ex * ex + ez * ez;
					if (d2 < m_A.m_aBest[s])
					{
						m_A.m_aBest[s] = d2;
						m_A.m_aCx[s] = m_A.m_aCx[ns];
						m_A.m_aCz[s] = m_A.m_aCz[ns];
						m_A.m_aCd[s] = m_A.m_aCd[ns];
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! 5 x 5 box blur of lat (m_A.m_aTmp) and Dc (m_A.m_aCd) into m_B.m_aLat / m_aDc on the output area; the margin
	//! (6 >= 2) keeps every read inside the grid. No array parameters (members only)
	protected void Blur()
	{
		// rows: output columns M .. M + NV - 1, all rows M - 2 .. M + NV + 1. m_B.m_aTmp = lat, m_B.m_aDc = dc (temp)
		for (int k = M - 2; k < M + NV + 2; k++)
		{
			for (int i = M; i < M + NV; i++)
			{
				int s = k * NS + i;
				m_B.m_aTmp[s] = m_A.m_aTmp[s - 2] + m_A.m_aTmp[s - 1] + m_A.m_aTmp[s] + m_A.m_aTmp[s + 1] + m_A.m_aTmp[s + 2];
				m_B.m_aDc[s] = m_A.m_aCd[s - 2] + m_A.m_aCd[s - 1] + m_A.m_aCd[s] + m_A.m_aCd[s + 1] + m_A.m_aCd[s + 2];
			}
		}
		// columns: lat into m_B.m_aLat; Dc column sums into m_A.m_aTmp (lat is done with it), then copied back
		for (int ck = M; ck < M + NV; ck++)
		{
			for (int ci = M; ci < M + NV; ci++)
			{
				int c = ck * NS + ci;
				m_B.m_aLat[c] = (m_B.m_aTmp[c - 2 * NS] + m_B.m_aTmp[c - NS] + m_B.m_aTmp[c] + m_B.m_aTmp[c + NS] + m_B.m_aTmp[c + 2 * NS]) / 25.0;
				m_A.m_aTmp[c] = (m_B.m_aDc[c - 2 * NS] + m_B.m_aDc[c - NS] + m_B.m_aDc[c] + m_B.m_aDc[c + NS] + m_B.m_aDc[c + 2 * NS]) / 25.0;
			}
		}
		for (int bk = M; bk < M + NV; bk++)
		{
			for (int bi = M; bi < M + NV; bi++)
			{
				int b = bk * NS + bi;
				m_B.m_aDc[b] = m_A.m_aTmp[b];
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Mesh verts / UVs into m_Buf from m_In + m_Out. Felt = what the 0.25 m mesh draws (exact on its triangles).
	//! Inside the tyre strip the look groove (shifted down where the felt is under the rut floor, e.g. a heightfield
	//! crater); outside it max(look, felt) - nothing floats over drawn ground. groove: two sections (reveal trick).
	//! The look x the edge lock (none at a road edge, as the felt floor); a vertex on a drawn-nothing cell (kept, voxel,
	//! old look probe: collapseBits) keeps the felt height, so it meets the road / bricks as the plain mesh does.
	void Assemble(float step, int ux, int uz, float uvDiv, bool groove, float revDepth, float revBlend, float revGap, float skirt, int collapseBits)
	{
		if (revBlend < 0.01)
			revBlend = 0.01;
		float sink = revGap * 3;
		for (int oz = 0; oz < NV; oz++)
		{
			int jz = oz / 2;
			int w2 = oz - jz * 2;
			for (int ox = 0; ox < NV; ox++)
			{
				int jx = ox / 2;
				int u2 = ox - jx * 2;
				int o = oz * NV + ox;
				int v = jz * 33 + jx;
				int diag = 0;
				if (u2 == 1 && w2 == 1)
					diag = m_In.m_aDiag[(jz + 3) * 38 + jx + 3];
				int v10 = v;
				int v01 = v;
				int v11 = v;
				if (u2 == 1)
					v10 = v + 1;
				if (w2 == 1)
					v01 = v + 33;
				if (u2 == 1 && w2 == 1)
					v11 = v + 34;
				float bs = Mid(m_In.m_aBaseV[v], m_In.m_aBaseV[v10], m_In.m_aBaseV[v01], m_In.m_aBaseV[v11], u2, w2, diag);
				float felt = Mid(m_In.m_aFeltV[v], m_In.m_aFeltV[v10], m_In.m_aFeltV[v01], m_In.m_aFeltV[v11], u2, w2, diag);
				float rev = Mid(m_In.m_aRevV[v], m_In.m_aRevV[v10], m_In.m_aRevV[v01], m_In.m_aRevV[v11], u2, w2, diag);
				float lockF = Mid(m_In.m_aLockV[v], m_In.m_aLockV[v10], m_In.m_aLockV[v01], m_In.m_aLockV[v11], u2, w2, diag);
				float y = felt;
				float dirt = 0;
				bool pinned = Pinned(ox, oz, collapseBits);     // true: felt height, plain reveal only
				if (!pinned && m_Out.m_aHas[o] == 1)
				{
					int so = (oz + M) * NS + ox + M;
					float excess = bs - m_A.m_aDep[so] - felt;      // felt under the rut floor (crater, dig): follow it down
					if (excess < 0)
						excess = 0;
					float lookY = bs + m_Out.m_aLook[o] * lockF - excess;
					if (m_Out.m_aTyre[o] == 1 || lookY > felt)
						y = lookY;
					dirt = m_Out.m_aDirt[o];
				}
				else if (!pinned && m_Out.m_aDirt[o] > 0 && y < bs)
				{
					dirt = m_Out.m_aDirt[o];      // floor deeper than DIRT_D without a centre (shallow edge): soil as the look
				}
				if (groove)
				{
					m_Buf.m_aUnder[o] = Vector(ox * step, y - revGap, oz * step);
					float t = Math.Clamp((-rev - revDepth) / revBlend, 0, 1);
					if (dirt > t)
						t = dirt;
					y -= t * t * (3 - 2 * t) * sink;
					m_Buf.m_aGrooveUVs[o * 2] = ox / 64.0;
					m_Buf.m_aGrooveUVs[o * 2 + 1] = 1 - oz / 64.0;
				}
				m_Buf.m_aVerts[o] = Vector(ox * step, y, oz * step);
				m_Buf.m_aUVs[o * 2] = (ux + ox * 0.5) / uvDiv;
				m_Buf.m_aUVs[o * 2 + 1] = 1 - (uz + oz * 0.5) / uvDiv;
			}
		}
		// skirts: per border a top copy row + a bottom row hanging skirt m (hides cracks / steps at a plain neighbour)
		for (int b = 0; b < 4; b++)
		{
			for (int j = 0; j < NV; j++)
			{
				int src = BorderVert(b, j);
				int top = NV * NV + b * NV * 2 + j;
				vector p = m_Buf.m_aVerts[src];
				m_Buf.m_aVerts[top] = p;
				m_Buf.m_aVerts[top + NV] = Vector(p[0], p[1] - skirt, p[2]);
				m_Buf.m_aUVs[top * 2] = m_Buf.m_aUVs[src * 2];
				m_Buf.m_aUVs[top * 2 + 1] = m_Buf.m_aUVs[src * 2 + 1];
				m_Buf.m_aUVs[(top + NV) * 2] = m_Buf.m_aUVs[src * 2];
				m_Buf.m_aUVs[(top + NV) * 2 + 1] = m_Buf.m_aUVs[src * 2 + 1];
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True = output vertex (ox, oz) touches a skin cell of this patch that draws nothing (kept terrain or collapseBits)
	protected bool Pinned(int ox, int oz, int collapseBits)
	{
		int sx0 = (ox - 1) / 16;
		int sx1 = ox / 16;
		int sz0 = (oz - 1) / 16;
		int sz1 = oz / 16;
		if (sx1 > 3)
			sx1 = 3;
		if (sz1 > 3)
			sz1 = 3;
		for (int sz = sz0; sz <= sz1; sz++)
		{
			for (int sx = sx0; sx <= sx1; sx++)
			{
				int cell = m_In.m_aCell[sz * 4 + sx];
				if (cell < 0 || (cell & collapseBits) != 0)
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Output vertex j along border b: 0 south (z 0), 1 north (z max), 2 west (x 0), 3 east (x max)
	static int BorderVert(int b, int j)
	{
		if (b == 0)
			return j;
		if (b == 1)
			return (NV - 1) * NV + j;
		if (b == 2)
			return j * NV;
		return j * NV + NV - 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Index list for m_In.m_aCell: cells < 0 (kept terrain) or with a collapse bit (voxel, old look probe) draw
	//! nothing, as the 0.25 m list (MUD_WorldGroundEntity.FillIndices); same diagonal per skin cell. Cached (ring).
	MUD_LookIdx Indices(int tlbrBit, int collapseBits)
	{
		foreach (MUD_LookIdx c : m_aIdx)
		{
			bool same = true;
			for (int q = 0; q < 16; q++)
			{
				if (c.m_aCell[q] != m_In.m_aCell[q])
				{
					same = false;
					break;
				}
			}
			if (same)
				return c;
		}
		m_iIdxMisses++;
		MUD_LookIdx ix = new MUD_LookIdx();
		for (int qc = 0; qc < 16; qc++)
			ix.m_aCell[qc] = m_In.m_aCell[qc];
		int n;
		for (int fz = 0; fz < NV - 1; fz++)
		{
			for (int fx = 0; fx < NV - 1; fx++)
			{
				int cell = m_In.m_aCell[(fz / 16) * 4 + fx / 16];
				int bl = fz * NV + fx;
				int br = bl + 1;
				int tl = bl + NV;
				int tr = tl + 1;
				if (cell < 0 || (cell & collapseBits) != 0)
				{
					for (int j = 0; j < 6; j++)
						ix.m_aIdx[n + j] = bl;
				}
				else if ((cell & tlbrBit) != 0)
				{
					ix.m_aIdx[n] = bl;
					ix.m_aIdx[n + 1] = tl;
					ix.m_aIdx[n + 2] = br;
					ix.m_aIdx[n + 3] = tl;
					ix.m_aIdx[n + 4] = tr;
					ix.m_aIdx[n + 5] = br;
				}
				else
				{
					ix.m_aIdx[n] = bl;
					ix.m_aIdx[n + 1] = tl;
					ix.m_aIdx[n + 2] = tr;
					ix.m_aIdx[n + 3] = tr;
					ix.m_aIdx[n + 4] = br;
					ix.m_aIdx[n + 5] = bl;
				}
				n += 6;
			}
		}
		// skirts face outward (front = cross(b - a, c - a), as the surface tris face up): south / east one winding,
		// north / west the other. A segment beside a drawn-nothing cell collapses.
		for (int b = 0; b < 4; b++)
		{
			for (int sj = 0; sj < NV - 1; sj++)
			{
				int sx = sj;
				int sz = 0;
				if (b == 1)
					sz = NV - 2;
				else if (b == 2)
				{
					sx = 0;
					sz = sj;
				}
				else if (b == 3)
				{
					sx = NV - 2;
					sz = sj;
				}
				int scell = m_In.m_aCell[(sz / 16) * 4 + sx / 16];
				int t0 = NV * NV + b * NV * 2 + sj;
				int t1 = t0 + 1;
				int b0 = t0 + NV;
				int b1 = b0 + 1;
				if (scell < 0 || (scell & collapseBits) != 0)
				{
					for (int sk = 0; sk < 6; sk++)
						ix.m_aIdx[n + sk] = t0;
				}
				else if (b == 0 || b == 3)
				{
					ix.m_aIdx[n] = t0;
					ix.m_aIdx[n + 1] = t1;
					ix.m_aIdx[n + 2] = b0;
					ix.m_aIdx[n + 3] = t1;
					ix.m_aIdx[n + 4] = b1;
					ix.m_aIdx[n + 5] = b0;
				}
				else
				{
					ix.m_aIdx[n] = t0;
					ix.m_aIdx[n + 1] = b0;
					ix.m_aIdx[n + 2] = t1;
					ix.m_aIdx[n + 3] = t1;
					ix.m_aIdx[n + 4] = b0;
					ix.m_aIdx[n + 5] = b1;
				}
				n += 6;
			}
		}
		if (m_aIdx.Count() < IDX_RING)
		{
			m_aIdx.Insert(ix);
		}
		else
		{
			m_aIdx.Set(m_iIdxNext, ix);
			m_iIdxNext = (m_iIdxNext + 1) % IDX_RING;
		}
		return ix;
	}
}
