//! Layered-SDF ground, phase 2 (2026-10-13): band surface-nets mesher of one 8 m fine patch, H only.
//! Port of Tools/sdf_patch_proto.py BandMesher (counts / seams measured there, phase 1).
//! Used by MUD_WorldGroundEntity.BuildMeshBand / BuildColliderBand when Band Mesher is on (default off).
//!  - Field f = Y(x, z) - y, > 0 solid. Y on the patch's 33 x 33 fine verts; cells FINE in x / z, 0.5 m in y on
//!    WORLD planes y = j * 0.5 (so two patches cut the same planes). Only the band of each column is visited.
//!  - Off cells (outside the patch, kept terrain, voxel) next to an on cell are GHOSTS: vertex = mean of the
//!    crossings on the faces they share with on cells (or on the shared corner edge), clamped onto them. A ghost on
//!    the patch border reads only border samples, which both patches hold as identical copies -> exact seam.
//!  - Every rim edge (quad loop edge between two ghost verts) gets a skirt quad hanging skirt m down (own verts).
//! Speed (in-game 2026-10-13 21:45: the plain port took ~7 ms per build): most on cells have all 4 corners in one
//! layer. Their vertex is the corner mean at the cell centre (what the general code gives), and a y-edge quad
//! between 4 such cells is a square in x / z: always flipped to face up, no zero-area tris, no rim. Both are done
//! inline (FAST paths); everything else goes through the general Vertex / Quad as in the prototype.
//! Index layout: surface tris [0, m_iI), skirt tris [m_iI, m_iI + m_iSkirtI). Verts: surface [0, m_iV), skirt
//! copies [m_iV, m_iVAll).

class MUD_BandVerts
{
	vector m_aV[12000];     // MUD_BandMesher.MAX_V
}

class MUD_BandUVs
{
	float m_aTop[24000];    // MAX_V * 2
	float m_aUnder[24000];
}

class MUD_BandIdx
{
	int m_aI[60000];        // MUD_BandMesher.MAX_I
}

class MUD_BandVertInfo
{
	float m_aD[12000];      // displayed deformation interpolated at the vertex (groove reveal)
	float m_aVy[12000];     // vertex y (the FAST quad split reads it without copying a vector)
	int m_aGhost[12000];    // 1 = rim vertex (ghost cell)
	int m_aTop[12000];      // skirt: surface vertex -> its top copy, -1 = none
	int m_aRim[8000];       // rim edges as vertex pairs
}

class MUD_BandTriSlot
{
	int m_aS[20000];        // collider surface slot of each surface triangle
}

class MUD_BandMesher
{
	static const int NC = 32;       // fine cells per patch side
	static const int NV = 33;       // fine verts per patch side
	static const int CW = 34;       // cell table side: cells -1 .. 32
	static const int SKIN_FINE = 8; // fine cells per skin cell side
	static const int MAX_V = 12000;
	static const int MAX_I = 60000;
	static const int MAX_RIM = 8000;

	// input (caller fills before Build)
	float m_aY[1089];       // world Y of each fine vertex
	float m_aD[1089];       // deformation shown there (groove reveal), 0 for the collider
	int m_aSkin[16];        // per skin cell (z * 4 + x): -1 = off (kept terrain / voxel), else collider slot

	// output
	ref MUD_BandVerts m_Vb = new MUD_BandVerts();
	ref MUD_BandIdx m_Ib = new MUD_BandIdx();
	ref MUD_BandVertInfo m_Info = new MUD_BandVertInfo();
	ref MUD_BandTriSlot m_Slot = new MUD_BandTriSlot();
	ref MUD_BandIdx m_Scratch = new MUD_BandIdx();      // collider: one slot's tris / the reversed slab
	ref MUD_BandVerts m_Aux = new MUD_BandVerts();      // groove under-layer (mesh) or slab (collider)
	ref MUD_BandUVs m_Uv = new MUD_BandUVs();
	int m_iV;               // surface verts
	int m_iVAll;            // + skirt verts
	int m_iI;               // surface indices
	int m_iSkirtI;          // skirt indices
	int m_iCells;           // band cells visited
	int m_iDegen;
	int m_iMissing;
	int m_iRim;             // ints in m_Info.m_aRim
	int m_iFastV;           // verts / y quads done on the FAST path (stats)
	int m_iFastQ;
	bool m_bOverflow;
	// REBUILD 5 option 4 (2026-10-19, v240 = before): collider only - a 2 x 2 block of FAST y quads whose 9 cells are
	// single-vertex on cells of one skin cell, whose 16 samples are untouched (m_aTouch 0) and whose 5 inner vertices lie
	// within m_fMergeTol of the block's 2 big triangles is drawn as ONE quad (MergeMask / EmitMerged). Mesh builds: off.
	bool m_bMerge;
	float m_fMergeTol = 0.01;
	int m_aTouch[1089];     // caller (collider): 1 = sample moved off the plain ground (rut, ridge, hold) - never merged
	int m_iMerged;          // blocks merged in the last Build (6 tris less each)
	protected int m_aMergeB[256];     // block (bz * 16 + bx): 1 = merged

	// cell table, cells -1 .. 32, c = (z + 1) * CW + x + 1. Kind / faces / slots depend only on m_aSkin (Layout).
	protected int m_aKind[1156];    // 0 off, 1 on, 2 ghost
	protected int m_aFx[1156];      // ghost: grid x of its shared x face, -1 = none
	protected int m_aFz[1156];
	protected int m_aCx[1156];      // ghost: shared diagonal corner, -1 = none
	protected int m_aCz[1156];
	protected int m_aCSlot[1156];   // on cell: its collider slot, else -1
	protected int m_aSingle[1156];  // on cell with one vertex (all corners in one layer): that vertex, else -1
	protected int m_aJlo[1156];
	protected int m_aJn[1156];      // layers with a vertex
	protected int m_aV0[1156];      // first vertex
	protected int m_aJ[1089];       // layer of each sample: ceil(Y / 0.5) - 1
	protected int m_aLastSkin[16];  // m_aSkin the cell table was made for
	protected bool m_bLayout;
	// REBUILD 6 LAYOUT_SPLIT (2026-10-19, research/2026-10-19-rebuild-and-bugs-research.md 2.2): kinds / faces / corners
	// depend only on which skin cells are on - a new m_aSkin with the same on-mask refreshes only m_aCSlot.
	static bool s_bLayoutSplit = true;     // A/B switch (MUD_StutterRobot sets it per robot run on the client)
	static const bool LAYOUT_CHECK = true;  // TEMP: every LAYOUT_CHECK_EVERY-th slot refresh also makes the full table, compares
	static const int LAYOUT_CHECK_EVERY = 10;
	protected int m_iLastOnMask = -1;
	protected int m_aCSlotChk[1156];
	int m_iLayoutFull;       // log (MUD_WorldGround 'MUD rebuild 6'): full tables made
	int m_iLayoutSlots;      //   slot refreshes only
	int m_iLayoutHits;       //   nothing changed
	int m_iLayoutChecks;     //   LAYOUT_CHECK compares
	int m_iLayoutBad;        //   cells whose slot differed from the full table (must be 0)
	protected int m_iSlotRuns;

	protected float m_fFine;
	// Vertex accumulators (Vert / Hor)
	protected float m_fY0;
	protected float m_fY1;
	protected float m_fSx;
	protected float m_fSy;
	protected float m_fSz;
	protected float m_fSd;
	protected int m_iN;

	//------------------------------------------------------------------------------------------------
	//! REBUILD 5 option 5 (2026-10-19): the mesher as it was before (v240) - kept for the A/B switch
	//! (MUD_WorldGroundEntity.MESHER_FAST false) and the TEMP bench (Bench). Never merges.
	//! Mesh of m_aY / m_aD / m_aSkin. skirt <= 0: no skirts. False = overflow (buffers too small) or no triangle.
	bool BuildRef(float fine, float skirt)
	{
		m_fFine = fine;
		m_iV = 0;
		m_iVAll = 0;
		m_iI = 0;
		m_iSkirtI = 0;
		m_iCells = 0;
		m_iDegen = 0;
		m_iMissing = 0;
		m_iRim = 0;
		m_iFastV = 0;
		m_iFastQ = 0;
		m_bOverflow = false;

		// 1. cell kinds (only when the skin cells changed)
		Layout();

		// 2. sample layers
		for (int s = 0; s < 1089; s++)
		{
			int cj = Math.Ceil(m_aY[s] * 2);    // exact: 0.5 is a power of two
			m_aJ[s] = cj - 1;
		}

		// 3. vertices, column by column, band only
		for (int vz = -1; vz <= NC; vz++)
		{
			for (int vx = -1; vx <= NC; vx++)
			{
				int vc = (vz + 1) * CW + vx + 1;
				int kind = m_aKind[vc];
				if (kind == 0)
					continue;
				int j0 = 0;
				int j1 = 0;
				if (kind == 1)
				{
					int s00 = vz * NV + vx;
					int ja = m_aJ[s00];
					int jb = m_aJ[s00 + 1];
					int jc = m_aJ[s00 + NV];
					int jd = m_aJ[s00 + NV + 1];
					if (ja == jb && ja == jc && ja == jd)
					{
						// FAST: 4 vertical crossings in layer ja, no horizontal one -> corner mean at the cell centre
						m_iCells++;
						if (m_iV >= MAX_V)
						{
							m_bOverflow = true;
							return false;
						}
						int fv = m_iV;
						float py = (m_aY[s00] + m_aY[s00 + 1] + m_aY[s00 + NV] + m_aY[s00 + NV + 1]) * 0.25;
						m_Vb.m_aV[fv] = Vector((vx + 0.5) * m_fFine, py, (vz + 0.5) * m_fFine);
						m_Info.m_aVy[fv] = py;
						m_Info.m_aD[fv] = (m_aD[s00] + m_aD[s00 + 1] + m_aD[s00 + NV] + m_aD[s00 + NV + 1]) * 0.25;
						m_Info.m_aGhost[fv] = 0;
						m_aJlo[vc] = ja;
						m_aJn[vc] = 1;
						m_aV0[vc] = fv;
						m_aSingle[vc] = fv;
						m_iV++;
						m_iFastV++;
						continue;
					}
					m_aSingle[vc] = -1;
					j0 = ja;
					j1 = ja;
					if (jb < j0)
						j0 = jb;
					if (jb > j1)
						j1 = jb;
					if (jc < j0)
						j0 = jc;
					if (jc > j1)
						j1 = jc;
					if (jd < j0)
						j0 = jd;
					if (jd > j1)
						j1 = jd;
				}
				else
				{
					float lo = 0;
					float hi = 0;
					ColRange(vc, vx, vz, lo, hi);
					j0 = CeilHalf(lo) - 1;
					j1 = CeilHalf(hi) - 1;
				}
				m_aJlo[vc] = j0;
				m_aJn[vc] = 0;
				m_aV0[vc] = m_iV;
				for (int j = j0; j <= j1; j++)
				{
					m_iCells++;
					if (m_iV >= MAX_V)
					{
						m_bOverflow = true;
						return false;
					}
					bool made = false;
					if (kind == 1)
						made = VertexOn(vx, vz, j);
					else
						made = Vertex(vc, vx, vz, j);
					if (!made)
						break;
					m_aJn[vc] = m_aJn[vc] + 1;
				}
			}
		}

		// 4. quads: y edges at every grid point, x / z edges on every plane they cross
		vector airX = Vector(1, 0, 0);
		vector airZ = Vector(0, 0, 1);
		for (int gz = 0; gz < NV; gz++)
		{
			for (int gx = 0; gx < NV; gx++)
			{
				int g = gz * NV + gx;
				int jy = m_aJ[g];
				// cells round the y edge: (gx-1, gz-1), (gx, gz-1), (gx, gz), (gx-1, gz)
				int ca = gz * CW + gx;
				int qa = m_aSingle[ca];
				int qb = m_aSingle[ca + 1];
				int qc = m_aSingle[ca + CW + 1];
				int qd = m_aSingle[ca + CW];
				if (qa >= 0 && qb >= 0 && qc >= 0 && qd >= 0)
				{
					// FAST: 4 single-vertex on cells (their layer is jy) = a square in x / z. The general Quad flips it
					// to a d c b (normal -y otherwise) and splits on the shorter diagonal: x / z parts are equal.
					if (m_iI + 6 > MAX_I)
					{
						m_bOverflow = true;
						return false;
					}
					float dac = m_Info.m_aVy[qa] - m_Info.m_aVy[qc];
					float dbd = m_Info.m_aVy[qb] - m_Info.m_aVy[qd];
					int ti = m_iI;
					m_Ib.m_aI[ti] = qa;
					m_Ib.m_aI[ti + 1] = qd;
					if (dac * dac <= dbd * dbd)
					{
						m_Ib.m_aI[ti + 2] = qc;
						m_Ib.m_aI[ti + 3] = qa;
						m_Ib.m_aI[ti + 4] = qc;
						m_Ib.m_aI[ti + 5] = qb;
					}
					else
					{
						m_Ib.m_aI[ti + 2] = qb;
						m_Ib.m_aI[ti + 3] = qd;
						m_Ib.m_aI[ti + 4] = qc;
						m_Ib.m_aI[ti + 5] = qb;
					}
					int fs = m_aCSlot[ca];
					int tt = ti / 3;
					m_Slot.m_aS[tt] = fs;
					m_Slot.m_aS[tt + 1] = fs;
					m_iI += 6;
					m_iFastQ++;
				}
				else
				{
					Quad(gx - 1, gz - 1, jy, gx, gz - 1, jy, gx, gz, jy, gx - 1, gz, jy, vector.Up);
				}
				if (gx < NC)
				{
					int jr = m_aJ[g + 1];
					if (jr != jy)
					{
						vector ax = airX;
						if (m_aY[g] <= m_aY[g + 1])
							ax = airX * -1;
						int p0 = jy + 1;
						int p1 = jr + 1;
						if (jr < jy)
						{
							p0 = jr + 1;
							p1 = jy + 1;
						}
						for (int jp = p0; jp < p1; jp++)
							Quad(gx, gz - 1, jp - 1, gx, gz, jp - 1, gx, gz, jp, gx, gz - 1, jp, ax);
					}
				}
				if (gz < NC)
				{
					int ju = m_aJ[g + NV];
					if (ju != jy)
					{
						vector az = airZ;
						if (m_aY[g] <= m_aY[g + NV])
							az = airZ * -1;
						int q0 = jy + 1;
						int q1 = ju + 1;
						if (ju < jy)
						{
							q0 = ju + 1;
							q1 = jy + 1;
						}
						for (int jq = q0; jq < q1; jq++)
							Quad(gx - 1, gz, jq - 1, gx, gz, jq - 1, gx, gz, jq, gx - 1, gz, jq, az);
					}
				}
				if (m_bOverflow)
					return false;
			}
		}
		m_iVAll = m_iV;
		if (skirt > 0)
			Skirts(skirt);
		if (m_bOverflow)
			return false;
		return m_iI > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Mesh of m_aY / m_aD / m_aSkin. skirt <= 0: no skirts. False = overflow (buffers too small) or no triangle.
	//! REBUILD 5 option 5 (2026-10-19, v240 = before): BuildRef's code with the output buffers and counters in locals on
	//! the FAST paths, split in Verts / Quads (64 locals per method) - same floats in the same order (Bench compares).
	//! Option 4: m_bMerge (collider) merges flat untouched blocks (MergeMask).
	bool Build(float fine, float skirt)
	{
		m_fFine = fine;
		m_iV = 0;
		m_iVAll = 0;
		m_iI = 0;
		m_iSkirtI = 0;
		m_iCells = 0;
		m_iDegen = 0;
		m_iMissing = 0;
		m_iRim = 0;
		m_iFastV = 0;
		m_iFastQ = 0;
		m_iMerged = 0;
		m_bOverflow = false;

		// 1. cell kinds (only when the skin cells changed)
		Layout();

		// 2. sample layers
		for (int s = 0; s < 1089; s++)
		{
			int cj = Math.Ceil(m_aY[s] * 2);    // exact: 0.5 is a power of two
			m_aJ[s] = cj - 1;
		}

		// 3. vertices, 4. quads
		if (!Verts(fine))
			return false;
		if (!Quads())
			return false;
		m_iVAll = m_iV;
		if (skirt > 0)
			Skirts(skirt);
		if (m_bOverflow)
			return false;
		return m_iI > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Build step 3: vertices, column by column, band only (BuildRef's loop; FAST path on locals)
	protected bool Verts(float fine)
	{
		MUD_BandVerts vb = m_Vb;
		MUD_BandVertInfo info = m_Info;
		int iv = 0;
		int cells = 0;
		int fastV = 0;
		for (int vz = -1; vz <= NC; vz++)
		{
			float fz = (vz + 0.5) * fine;
			int rowC = (vz + 1) * CW + 1;
			for (int vx = -1; vx <= NC; vx++)
			{
				int vc = rowC + vx;
				int kind = m_aKind[vc];
				if (kind == 0)
					continue;
				int j0 = 0;
				int j1 = 0;
				if (kind == 1)
				{
					int s00 = vz * NV + vx;
					int ja = m_aJ[s00];
					int jb = m_aJ[s00 + 1];
					int jc = m_aJ[s00 + NV];
					int jd = m_aJ[s00 + NV + 1];
					if (ja == jb && ja == jc && ja == jd)
					{
						// FAST: 4 vertical crossings in layer ja, no horizontal one -> corner mean at the cell centre
						cells++;
						if (iv >= MAX_V)
						{
							m_iV = iv;
							m_iCells = cells;
							m_bOverflow = true;
							return false;
						}
						float py = (m_aY[s00] + m_aY[s00 + 1] + m_aY[s00 + NV] + m_aY[s00 + NV + 1]) * 0.25;
						vb.m_aV[iv] = Vector((vx + 0.5) * fine, py, fz);
						info.m_aVy[iv] = py;
						info.m_aD[iv] = (m_aD[s00] + m_aD[s00 + 1] + m_aD[s00 + NV] + m_aD[s00 + NV + 1]) * 0.25;
						info.m_aGhost[iv] = 0;
						m_aJlo[vc] = ja;
						m_aJn[vc] = 1;
						m_aV0[vc] = iv;
						m_aSingle[vc] = iv;
						iv++;
						fastV++;
						continue;
					}
					m_aSingle[vc] = -1;
					j0 = ja;
					j1 = ja;
					if (jb < j0)
						j0 = jb;
					if (jb > j1)
						j1 = jb;
					if (jc < j0)
						j0 = jc;
					if (jc > j1)
						j1 = jc;
					if (jd < j0)
						j0 = jd;
					if (jd > j1)
						j1 = jd;
				}
				else
				{
					float lo = 0;
					float hi = 0;
					ColRange(vc, vx, vz, lo, hi);
					j0 = CeilHalf(lo) - 1;
					j1 = CeilHalf(hi) - 1;
				}
				m_aJlo[vc] = j0;
				m_aJn[vc] = 0;
				m_aV0[vc] = iv;
				// general path: Vertex / VertexOn use the members
				m_iV = iv;
				m_iCells = cells;
				for (int j = j0; j <= j1; j++)
				{
					m_iCells++;
					if (m_iV >= MAX_V)
					{
						m_bOverflow = true;
						return false;
					}
					bool made = false;
					if (kind == 1)
						made = VertexOn(vx, vz, j);
					else
						made = Vertex(vc, vx, vz, j);
					if (!made)
						break;
					m_aJn[vc] = m_aJn[vc] + 1;
				}
				iv = m_iV;
				cells = m_iCells;
			}
		}
		m_iV = iv;
		m_iCells = cells;
		m_iFastV = fastV;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Build step 4: y edges at every grid point, x / z edges on every plane they cross (BuildRef's loop; FAST quad on
	//! locals; merged blocks when m_bMerge)
	protected bool Quads()
	{
		vector airX = Vector(1, 0, 0);
		vector airZ = Vector(0, 0, 1);
		MUD_BandIdx ib = m_Ib;
		MUD_BandTriSlot sl = m_Slot;
		MUD_BandVertInfo info = m_Info;
		int ii = m_iI;
		int fastQ = 0;
		if (m_bMerge)
			MergeMask();
		for (int gz = 0; gz < NV; gz++)
		{
			for (int gx = 0; gx < NV; gx++)
			{
				int g = gz * NV + gx;
				int jy = m_aJ[g];
				bool yDone = false;
				if (m_bMerge && gx > 0 && gz > 0)
				{
					int mbx = (gx - 1) / 2;
					int mbz = (gz - 1) / 2;
					if (m_aMergeB[mbz * 16 + mbx] != 0)
					{
						yDone = true;     // inside a merged block: its anchor point draws the one quad
						if (gx - 1 == mbx * 2 && gz - 1 == mbz * 2)
						{
							if (ii + 6 > MAX_I)
							{
								m_iI = ii;
								m_bOverflow = true;
								return false;
							}
							ii = EmitMerged(mbx, mbz, ii);
						}
					}
				}
				// cells round the y edge: (gx-1, gz-1), (gx, gz-1), (gx, gz), (gx-1, gz)
				int ca = gz * CW + gx;
				int qa = m_aSingle[ca];
				int qb = m_aSingle[ca + 1];
				int qc = m_aSingle[ca + CW + 1];
				int qd = m_aSingle[ca + CW];
				if (!yDone && qa >= 0 && qb >= 0 && qc >= 0 && qd >= 0)
				{
					// FAST: 4 single-vertex on cells (their layer is jy) = a square in x / z (see BuildRef)
					if (ii + 6 > MAX_I)
					{
						m_iI = ii;
						m_bOverflow = true;
						return false;
					}
					float dac = info.m_aVy[qa] - info.m_aVy[qc];
					float dbd = info.m_aVy[qb] - info.m_aVy[qd];
					ib.m_aI[ii] = qa;
					ib.m_aI[ii + 1] = qd;
					if (dac * dac <= dbd * dbd)
					{
						ib.m_aI[ii + 2] = qc;
						ib.m_aI[ii + 3] = qa;
						ib.m_aI[ii + 4] = qc;
						ib.m_aI[ii + 5] = qb;
					}
					else
					{
						ib.m_aI[ii + 2] = qb;
						ib.m_aI[ii + 3] = qd;
						ib.m_aI[ii + 4] = qc;
						ib.m_aI[ii + 5] = qb;
					}
					int fs = m_aCSlot[ca];
					int tt = ii / 3;
					sl.m_aS[tt] = fs;
					sl.m_aS[tt + 1] = fs;
					ii += 6;
					fastQ++;
				}
				else if (!yDone)
				{
					m_iI = ii;
					Quad(gx - 1, gz - 1, jy, gx, gz - 1, jy, gx, gz, jy, gx - 1, gz, jy, vector.Up);
					ii = m_iI;
				}
				if (gx < NC)
				{
					int jr = m_aJ[g + 1];
					if (jr != jy)
					{
						vector ax = airX;
						if (m_aY[g] <= m_aY[g + 1])
							ax = airX * -1;
						int p0 = jy + 1;
						int p1 = jr + 1;
						if (jr < jy)
						{
							p0 = jr + 1;
							p1 = jy + 1;
						}
						m_iI = ii;
						for (int jp = p0; jp < p1; jp++)
							Quad(gx, gz - 1, jp - 1, gx, gz, jp - 1, gx, gz, jp, gx, gz - 1, jp, ax);
						ii = m_iI;
					}
				}
				if (gz < NC)
				{
					int ju = m_aJ[g + NV];
					if (ju != jy)
					{
						vector az = airZ;
						if (m_aY[g] <= m_aY[g + NV])
							az = airZ * -1;
						int q0 = jy + 1;
						int q1 = ju + 1;
						if (ju < jy)
						{
							q0 = ju + 1;
							q1 = jy + 1;
						}
						m_iI = ii;
						for (int jq = q0; jq < q1; jq++)
							Quad(gx - 1, gz, jq - 1, gx, gz, jq - 1, gx, gz, jq, gx - 1, gz, jq, az);
						ii = m_iI;
					}
				}
				if (m_bOverflow)
				{
					m_iI = ii;
					return false;
				}
			}
		}
		m_iI = ii;
		m_iFastQ = fastQ;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Option 4: m_aMergeB from the cells made by Verts. Block (bx, bz) = cells 2bx .. 2bx + 2, 2bz .. 2bz + 2 (the 4 y
	//! quads at grid points 2bx + 1 .. 2bx + 2). A block that would straddle a skin cell (bx or bz % 4 == 3) never merges
	//! (slot + rut mud are per skin cell).
	protected void MergeMask()
	{
		float tol = m_fMergeTol;
		for (int bz = 0; bz < 16; bz++)
		{
			for (int bx = 0; bx < 16; bx++)
			{
				int b = bz * 16 + bx;
				m_aMergeB[b] = 0;
				if (bx - (bx / 4) * 4 == 3 || bz - (bz / 4) * 4 == 3)
					continue;
				int c0 = (2 * bz + 1) * CW + 2 * bx + 1;     // cell (2bx, 2bz)
				int slot = m_aCSlot[c0];
				if (slot < 0)
					continue;
				bool ok = true;
				for (int k = 0; k < 9 && ok; k++)
				{
					int kz = k / 3;
					int ck = c0 + kz * CW + k - kz * 3;
					if (m_aSingle[ck] < 0 || m_aCSlot[ck] != slot)
						ok = false;
				}
				for (int t = 0; t < 16 && ok; t++)
				{
					int tz = t / 4;
					if (m_aTouch[(2 * bz + tz) * NV + 2 * bx + t - tz * 4] != 0)
						ok = false;
				}
				if (!ok)
					continue;
				// vertex y by cell offset (x, z) in the block
				float y00 = m_Info.m_aVy[m_aSingle[c0]];
				float y10 = m_Info.m_aVy[m_aSingle[c0 + 1]];
				float y20 = m_Info.m_aVy[m_aSingle[c0 + 2]];
				float y01 = m_Info.m_aVy[m_aSingle[c0 + CW]];
				float y11 = m_Info.m_aVy[m_aSingle[c0 + CW + 1]];
				float y21 = m_Info.m_aVy[m_aSingle[c0 + CW + 2]];
				float y02 = m_Info.m_aVy[m_aSingle[c0 + 2 * CW]];
				float y12 = m_Info.m_aVy[m_aSingle[c0 + 2 * CW + 1]];
				float y22 = m_Info.m_aVy[m_aSingle[c0 + 2 * CW + 2]];
				// edge middles on the big edges, the centre on the diagonal EmitMerged splits on
				if (Math.AbsFloat(y10 - (y00 + y20) * 0.5) > tol || Math.AbsFloat(y12 - (y02 + y22) * 0.5) > tol)
					continue;
				if (Math.AbsFloat(y01 - (y00 + y02) * 0.5) > tol || Math.AbsFloat(y21 - (y20 + y22) * 0.5) > tol)
					continue;
				float dac = y00 - y22;
				float dbd = y20 - y02;
				float yc = (y00 + y22) * 0.5;
				if (dac * dac > dbd * dbd)
					yc = (y20 + y02) * 0.5;
				if (Math.AbsFloat(y11 - yc) > tol)
					continue;
				m_aMergeB[b] = 1;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Option 4: the merged block's quad from its corner cells, wound and split as the FAST quad; returns the next index
	protected int EmitMerged(int bx, int bz, int ii)
	{
		int c0 = (2 * bz + 1) * CW + 2 * bx + 1;
		int a = m_aSingle[c0];
		int b = m_aSingle[c0 + 2];
		int c = m_aSingle[c0 + 2 * CW + 2];
		int d = m_aSingle[c0 + 2 * CW];
		float dac = m_Info.m_aVy[a] - m_Info.m_aVy[c];
		float dbd = m_Info.m_aVy[b] - m_Info.m_aVy[d];
		m_Ib.m_aI[ii] = a;
		m_Ib.m_aI[ii + 1] = d;
		if (dac * dac <= dbd * dbd)
		{
			m_Ib.m_aI[ii + 2] = c;
			m_Ib.m_aI[ii + 3] = a;
			m_Ib.m_aI[ii + 4] = c;
			m_Ib.m_aI[ii + 5] = b;
		}
		else
		{
			m_Ib.m_aI[ii + 2] = b;
			m_Ib.m_aI[ii + 3] = d;
			m_Ib.m_aI[ii + 4] = c;
			m_Ib.m_aI[ii + 5] = b;
		}
		int fs = m_aCSlot[c0];
		int tt = ii / 3;
		m_Slot.m_aS[tt] = fs;
		m_Slot.m_aS[tt + 1] = fs;
		m_iMerged++;
		return ii + 6;
	}

	//------------------------------------------------------------------------------------------------
	//! Option 5 (TEMP bench): reps x BuildRef, then reps x Build on the same input, merge off. oldMs / newMs = their ms;
	//! true = both made the same output (counts, indices, slots, vertices, D, ghost flags - exact). The caller builds
	//! for real after this.
	bool Bench(float fine, float skirt, int reps, out int oldMs, out int newMs)
	{
		bool merge = m_bMerge;
		m_bMerge = false;
		int t = System.GetTickCount();
		for (int r = 0; r < reps; r++)
			BuildRef(fine, skirt);
		oldMs = System.GetTickCount(t);
		int hi = SigI();
		vector hv = SigV();
		t = System.GetTickCount();
		for (int r2 = 0; r2 < reps; r2++)
			Build(fine, skirt);
		newMs = System.GetTickCount(t);
		int hi2 = SigI();
		vector hv2 = SigV();
		m_bMerge = merge;
		return hi == hi2 && hv[0] == hv2[0] && hv[1] == hv2[1] && hv[2] == hv2[2];
	}

	//------------------------------------------------------------------------------------------------
	//! Bench: counts + indices + slots folded into one int (wraps)
	protected int SigI()
	{
		int h = m_iV * 7 + m_iVAll * 13 + m_iI * 17 + m_iSkirtI * 19 + m_iRim * 23 + m_iDegen * 29 + m_iMissing * 31;
		if (m_bOverflow)
			h = h + 1;
		int nI = m_iI + m_iSkirtI;
		for (int i = 0; i < nI; i++)
			h = h * 31 + m_Ib.m_aI[i];
		int nT = m_iI / 3;
		for (int t = 0; t < nT; t++)
			h = h * 31 + m_Slot.m_aS[t];
		return h;
	}

	//------------------------------------------------------------------------------------------------
	//! Bench: sum of every vertex; x also gets D + ghost flags
	protected vector SigV()
	{
		vector sum = vector.Zero;
		float sd = 0;
		for (int i = 0; i < m_iVAll; i++)
		{
			sum = sum + m_Vb.m_aV[i];
			sd += m_Info.m_aD[i] * 3 + m_Info.m_aGhost[i];
		}
		return sum + Vector(sd, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Cell kinds, ghost faces / corners and slots from m_aSkin; kept while m_aSkin does not change
	protected void Layout()
	{
		int onMask = 0;
		bool same = m_bLayout;
		for (int k = 0; k < 16; k++)
		{
			if (m_aSkin[k] >= 0)
				onMask = onMask | (1 << k);
			if (m_aLastSkin[k] != m_aSkin[k])
				same = false;
		}
		if (same)
		{
			m_iLayoutHits++;
			return;
		}
		for (int k2 = 0; k2 < 16; k2++)
			m_aLastSkin[k2] = m_aSkin[k2];
		if (s_bLayoutSplit && m_bLayout && onMask == m_iLastOnMask)
		{
			// LAYOUT_SPLIT: same on cells - kinds / faces / corners stay, only the on cells' slots change (off cells keep -1)
			for (int sz = 0; sz < NC; sz++)
			{
				int sRow = (sz / SKIN_FINE) * 4;
				int cRow = (sz + 1) * CW + 1;
				for (int sx = 0; sx < NC; sx++)
				{
					if (m_aKind[cRow + sx] == 1)
						m_aCSlot[cRow + sx] = m_aSkin[sRow + sx / SKIN_FINE];
				}
			}
			m_iLayoutSlots++;
			m_iSlotRuns++;
			if (!LAYOUT_CHECK || m_iSlotRuns < LAYOUT_CHECK_EVERY)
				return;
			// TEMP LAYOUT_CHECK: the full table must give the same slots
			m_iSlotRuns = 0;
			m_iLayoutChecks++;
			for (int q = 0; q < CW * CW; q++)
				m_aCSlotChk[q] = m_aCSlot[q];
			LayoutFull();
			for (int q2 = 0; q2 < CW * CW; q2++)
			{
				if (m_aCSlotChk[q2] != m_aCSlot[q2])
					m_iLayoutBad++;
			}
			return;
		}
		m_iLastOnMask = onMask;
		m_iLayoutFull++;
		LayoutFull();
	}

	//------------------------------------------------------------------------------------------------
	//! The whole cell table from m_aSkin (Layout's cache miss)
	protected void LayoutFull()
	{
		m_bLayout = true;
		for (int z = -1; z <= NC; z++)
		{
			for (int x = -1; x <= NC; x++)
			{
				int c = (z + 1) * CW + x + 1;
				m_aFx[c] = -1;
				m_aFz[c] = -1;
				m_aCx[c] = -1;
				m_aCz[c] = -1;
				m_aJn[c] = 0;
				m_aJlo[c] = 0;
				m_aV0[c] = 0;
				m_aKind[c] = 0;
				m_aCSlot[c] = -1;
				m_aSingle[c] = -1;
				if (On(x, z))
				{
					m_aKind[c] = 1;
					m_aCSlot[c] = m_aSkin[(z / SKIN_FINE) * 4 + x / SKIN_FINE];
					continue;
				}
				if (On(x - 1, z))
					m_aFx[c] = x;
				else if (On(x + 1, z))
					m_aFx[c] = x + 1;
				if (On(x, z - 1))
					m_aFz[c] = z;
				else if (On(x, z + 1))
					m_aFz[c] = z + 1;
				if (m_aFx[c] >= 0 || m_aFz[c] >= 0)
				{
					m_aKind[c] = 2;
					continue;
				}
				if (On(x - 1, z - 1))
				{
					m_aCx[c] = x;
					m_aCz[c] = z;
				}
				else if (On(x + 1, z - 1))
				{
					m_aCx[c] = x + 1;
					m_aCz[c] = z;
				}
				else if (On(x - 1, z + 1))
				{
					m_aCx[c] = x;
					m_aCz[c] = z + 1;
				}
				else if (On(x + 1, z + 1))
				{
					m_aCx[c] = x + 1;
					m_aCz[c] = z + 1;
				}
				if (m_aCx[c] >= 0)
					m_aKind[c] = 2;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! ceil(y / 0.5): exact in float (0.5 is a power of two)
	protected static int CeilHalf(float y)
	{
		int c = Math.Ceil(y * 2);
		return c;
	}

	//------------------------------------------------------------------------------------------------
	protected bool On(int x, int z)
	{
		if (x < 0 || z < 0 || x >= NC || z >= NC)
			return false;
		return m_aSkin[(z / SKIN_FINE) * 4 + x / SKIN_FINE] >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Collider slot of cell (x, z), -1 = not an on cell
	protected int SlotAt(int x, int z)
	{
		if (x < -1 || z < -1 || x > NC || z > NC)
			return -1;
		return m_aCSlot[(z + 1) * CW + x + 1];
	}

	//------------------------------------------------------------------------------------------------
	//! y range of the samples a ghost cell's vertex reads: its faces / corner
	protected void ColRange(int c, int x, int z, out float lo, out float hi)
	{
		lo = 1000000;
		hi = -1000000;
		int f = m_aFx[c];
		int g = m_aFz[c];
		if (f >= 0)
		{
			RangeAt(f, z, lo, hi);
			RangeAt(f, z + 1, lo, hi);
		}
		if (g >= 0)
		{
			RangeAt(x, g, lo, hi);
			RangeAt(x + 1, g, lo, hi);
		}
		if (m_aCx[c] >= 0)
			RangeAt(m_aCx[c], m_aCz[c], lo, hi);
	}

	//------------------------------------------------------------------------------------------------
	protected void RangeAt(int px, int pz, inout float lo, inout float hi)
	{
		int cx = Math.ClampInt(px, 0, NC);
		int cz = Math.ClampInt(pz, 0, NC);
		float yy = m_aY[cz * NV + cx];
		if (yy < lo)
			lo = yy;
		if (yy > hi)
			hi = yy;
	}

	//------------------------------------------------------------------------------------------------
	//! Mean of the crossings on the cell's edges (ghost: its shared faces / corner), layer j. False = none.
	protected bool Vertex(int c, int x, int z, int j)
	{
		m_fY0 = j * 0.5;
		m_fY1 = m_fY0 + 0.5;
		m_fSx = 0;
		m_fSy = 0;
		m_fSz = 0;
		m_fSd = 0;
		m_iN = 0;
		int k = m_aKind[c];
		int f = m_aFx[c];
		int g = m_aFz[c];
		if (k == 1)
		{
			Vert(x, z);
			Vert(x + 1, z);
			Vert(x, z + 1);
			Vert(x + 1, z + 1);
			Hor(x, z, x + 1, z);
			Hor(x, z + 1, x + 1, z + 1);
			Hor(x, z, x, z + 1);
			Hor(x + 1, z, x + 1, z + 1);
		}
		else
		{
			if (f >= 0)
			{
				Vert(f, z);
				Vert(f, z + 1);
				Hor(f, z, f, z + 1);
			}
			if (g >= 0)
			{
				// the corner vertical edge (f, g) lies on both faces: count it once
				if (x != f)
					Vert(x, g);
				if (x + 1 != f)
					Vert(x + 1, g);
				Hor(x, g, x + 1, g);
			}
			if (f < 0 && g < 0)
				Vert(m_aCx[c], m_aCz[c]);
		}
		if (m_iN == 0)
			return false;
		float inv = 1.0 / m_iN;
		float px = m_fSx * inv;
		float py = m_fSy * inv;
		float pz = m_fSz * inv;
		if (k == 2)
		{
			if (f >= 0)
				px = f;
			if (g >= 0)
				pz = g;
			if (f < 0 && g < 0)
			{
				px = m_aCx[c];
				pz = m_aCz[c];
			}
		}
		m_Vb.m_aV[m_iV] = Vector(px * m_fFine, py, pz * m_fFine);
		m_Info.m_aVy[m_iV] = py;
		m_Info.m_aD[m_iV] = m_fSd * inv;
		m_Info.m_aGhost[m_iV] = 0;
		if (k == 2)
			m_Info.m_aGhost[m_iV] = 1;
		m_iV++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Vertex() for an on cell whose corners span 2+ layers, without the Vert / Hor calls: the same crossings
	//! summed in the same order (so the same floats). False = none in layer j.
	protected bool VertexOn(int x, int z, int j)
	{
		float y0 = j * 0.5;
		float y1 = y0 + 0.5;
		int s00 = z * NV + x;
		float ya = m_aY[s00];
		float yb = m_aY[s00 + 1];
		float yc = m_aY[s00 + NV];
		float yd = m_aY[s00 + NV + 1];
		float da = m_aD[s00];
		float db = m_aD[s00 + 1];
		float dc = m_aD[s00 + NV];
		float dd = m_aD[s00 + NV + 1];
		float sx = 0;
		float sy = 0;
		float sz = 0;
		float sd = 0;
		int n = 0;
		float t = 0;
		// vertical edges (x, z), (x+1, z), (x, z+1), (x+1, z+1)
		if (ya > y0 && ya <= y1)
		{
			sx += x;
			sy += ya;
			sz += z;
			sd += da;
			n++;
		}
		if (yb > y0 && yb <= y1)
		{
			sx += x + 1;
			sy += yb;
			sz += z;
			sd += db;
			n++;
		}
		if (yc > y0 && yc <= y1)
		{
			sx += x;
			sy += yc;
			sz += z + 1;
			sd += dc;
			n++;
		}
		if (yd > y0 && yd <= y1)
		{
			sx += x + 1;
			sy += yd;
			sz += z + 1;
			sd += dd;
			n++;
		}
		// horizontal edges a-b (z), c-d (z+1) along x; a-c (x), b-d (x+1) along z; plane y0 then y1
		if ((ya > y0) != (yb > y0))
		{
			t = (ya - y0) / (ya - yb);
			sx += x + t;
			sy += y0;
			sz += z;
			sd += da + (db - da) * t;
			n++;
		}
		if ((ya > y1) != (yb > y1))
		{
			t = (ya - y1) / (ya - yb);
			sx += x + t;
			sy += y1;
			sz += z;
			sd += da + (db - da) * t;
			n++;
		}
		if ((yc > y0) != (yd > y0))
		{
			t = (yc - y0) / (yc - yd);
			sx += x + t;
			sy += y0;
			sz += z + 1;
			sd += dc + (dd - dc) * t;
			n++;
		}
		if ((yc > y1) != (yd > y1))
		{
			t = (yc - y1) / (yc - yd);
			sx += x + t;
			sy += y1;
			sz += z + 1;
			sd += dc + (dd - dc) * t;
			n++;
		}
		if ((ya > y0) != (yc > y0))
		{
			t = (ya - y0) / (ya - yc);
			sx += x;
			sy += y0;
			sz += z + t;
			sd += da + (dc - da) * t;
			n++;
		}
		if ((ya > y1) != (yc > y1))
		{
			t = (ya - y1) / (ya - yc);
			sx += x;
			sy += y1;
			sz += z + t;
			sd += da + (dc - da) * t;
			n++;
		}
		if ((yb > y0) != (yd > y0))
		{
			t = (yb - y0) / (yb - yd);
			sx += x + 1;
			sy += y0;
			sz += z + t;
			sd += db + (dd - db) * t;
			n++;
		}
		if ((yb > y1) != (yd > y1))
		{
			t = (yb - y1) / (yb - yd);
			sx += x + 1;
			sy += y1;
			sz += z + t;
			sd += db + (dd - db) * t;
			n++;
		}
		if (n == 0)
			return false;
		float inv = 1.0 / n;
		float py = sy * inv;
		m_Vb.m_aV[m_iV] = Vector(sx * inv * m_fFine, py, sz * inv * m_fFine);
		m_Info.m_aVy[m_iV] = py;
		m_Info.m_aD[m_iV] = sd * inv;
		m_Info.m_aGhost[m_iV] = 0;
		m_iV++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Vertical sample edge (px, pz) crosses the field inside the current layer
	protected void Vert(int px, int pz)
	{
		int v = pz * NV + px;
		float yy = m_aY[v];
		if (yy <= m_fY0 || yy > m_fY1)
			return;
		m_fSx += px;
		m_fSy += yy;
		m_fSz += pz;
		m_fSd += m_aD[v];
		m_iN++;
	}

	//------------------------------------------------------------------------------------------------
	//! Horizontal edge a-b crosses the current layer's bottom / top plane
	protected void Hor(int ax, int az, int bx, int bz)
	{
		int va = az * NV + ax;
		int vb = bz * NV + bx;
		float ya = m_aY[va];
		float yb = m_aY[vb];
		HorPlane(ax, az, bx, bz, ya, yb, va, vb, m_fY0);
		HorPlane(ax, az, bx, bz, ya, yb, va, vb, m_fY1);
	}

	//------------------------------------------------------------------------------------------------
	protected void HorPlane(int ax, int az, int bx, int bz, float ya, float yb, int va, int vb, float p)
	{
		if ((ya > p) == (yb > p))
			return;
		float t = (ya - p) / (ya - yb);
		m_fSx += ax + (bx - ax) * t;
		m_fSy += p;
		m_fSz += az + (bz - az) * t;
		float da = m_aD[va];
		m_fSd += da + (m_aD[vb] - da) * t;
		m_iN++;
	}

	//------------------------------------------------------------------------------------------------
	//! Vertex of cell (x, z) in layer j: -1 = no cell there, -2 = layer without a vertex
	protected int Vid(int x, int z, int j)
	{
		if (x < -1 || z < -1 || x > NC || z > NC)
			return -1;
		int c = (z + 1) * CW + x + 1;
		if (m_aKind[c] == 0)
			return -1;
		int r = j - m_aJlo[c];
		if (r < 0 || r >= m_aJn[c])
			return -2;
		return m_aV0[c] + r;
	}

	//------------------------------------------------------------------------------------------------
	//! Quad over one sign-changing edge from its 4 cells; drawn when one of them is on (slot = the first on one)
	protected void Quad(int x0, int z0, int j0, int x1, int z1, int j1, int x2, int z2, int j2, int x3, int z3, int j3, vector air)
	{
		if (m_bOverflow)
			return;
		int slot = SlotAt(x0, z0);
		if (slot < 0)
			slot = SlotAt(x1, z1);
		if (slot < 0)
			slot = SlotAt(x2, z2);
		if (slot < 0)
			slot = SlotAt(x3, z3);
		if (slot < 0)
			return;
		int a = Vid(x0, z0, j0);
		int b = Vid(x1, z1, j1);
		int c = Vid(x2, z2, j2);
		int d = Vid(x3, z3, j3);
		if (a < 0 || b < 0 || c < 0 || d < 0)
		{
			m_iMissing++;
			return;
		}
		vector pa = m_Vb.m_aV[a];
		vector pb = m_Vb.m_aV[b];
		vector pc = m_Vb.m_aV[c];
		vector pd = m_Vb.m_aV[d];
		vector n = Cross(pc - pa, pd - pb);
		if (vector.Dot(n, air) < 0)
		{
			int bt = b;
			b = d;
			d = bt;
			vector pt = pb;
			pb = pd;
			pd = pt;
		}
		if (vector.DistanceSq(pa, pc) <= vector.DistanceSq(pb, pd))
		{
			Tri(a, b, c, slot);
			Tri(a, c, d, slot);
		}
		else
		{
			Tri(a, b, d, slot);
			Tri(b, c, d, slot);
		}
		// rim: a loop edge between two ghost verts (never a diagonal) - each is in exactly one quad
		if (m_Info.m_aGhost[a] + m_Info.m_aGhost[b] + m_Info.m_aGhost[c] + m_Info.m_aGhost[d] < 2)
			return;
		Rim(a, b);
		Rim(b, c);
		Rim(c, d);
		Rim(d, a);
	}

	//------------------------------------------------------------------------------------------------
	protected void Rim(int u, int w)
	{
		if (m_Info.m_aGhost[u] == 0 || m_Info.m_aGhost[w] == 0)
			return;
		if (m_iRim + 2 > MAX_RIM)
		{
			m_bOverflow = true;
			return;
		}
		m_Info.m_aRim[m_iRim] = u;
		m_Info.m_aRim[m_iRim + 1] = w;
		m_iRim += 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Already facing the air (Quad orders it); drops only zero-area ones
	protected void Tri(int a, int b, int c, int slot)
	{
		if (m_iI + 3 > MAX_I)
		{
			m_bOverflow = true;
			return;
		}
		if (a == b || b == c || a == c)
		{
			m_iDegen++;
			return;
		}
		vector pa = m_Vb.m_aV[a];
		vector n = Cross(m_Vb.m_aV[b] - pa, m_Vb.m_aV[c] - pa);
		if (n.LengthSq() < 0.000000000001)
		{
			m_iDegen++;
			return;
		}
		m_Ib.m_aI[m_iI] = a;
		m_Ib.m_aI[m_iI + 1] = b;
		m_Ib.m_aI[m_iI + 2] = c;
		m_Slot.m_aS[m_iI / 3] = slot;
		m_iI += 3;
	}

	//------------------------------------------------------------------------------------------------
	//! Every rim edge: a quad hanging depth down, own verts (top + bottom copy per rim vertex, so the surface
	//! normals stay as they are), wound like the edge reversed so it faces out
	protected void Skirts(float depth)
	{
		for (int v = 0; v < m_iV; v++)
			m_Info.m_aTop[v] = -1;
		for (int r = 0; r < m_iRim; r += 2)
		{
			int a = m_Info.m_aRim[r];
			int b = m_Info.m_aRim[r + 1];
			if (vector.DistanceSq(m_Vb.m_aV[a], m_Vb.m_aV[b]) == 0)
				continue;
			int ta = SkirtTop(a, depth);
			int tb = SkirtTop(b, depth);
			if (ta < 0 || tb < 0 || m_iI + m_iSkirtI + 6 > MAX_I)
			{
				m_bOverflow = true;
				return;
			}
			int s = m_iI + m_iSkirtI;
			m_Ib.m_aI[s] = tb;
			m_Ib.m_aI[s + 1] = ta;
			m_Ib.m_aI[s + 2] = ta + 1;
			m_Ib.m_aI[s + 3] = tb;
			m_Ib.m_aI[s + 4] = ta + 1;
			m_Ib.m_aI[s + 5] = tb + 1;
			m_iSkirtI += 6;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Top copy of rim vertex v (the bottom copy follows it), -1 = no room
	protected int SkirtTop(int v, float depth)
	{
		int t = m_Info.m_aTop[v];
		if (t >= 0)
			return t;
		if (m_iVAll + 2 > MAX_V)
			return -1;
		t = m_iVAll;
		vector p = m_Vb.m_aV[v];
		m_Vb.m_aV[t] = p;
		m_Vb.m_aV[t + 1] = p - vector.Up * depth;
		m_Info.m_aVy[t] = p[1];
		m_Info.m_aVy[t + 1] = p[1] - depth;
		m_Info.m_aD[t] = m_Info.m_aD[v];
		m_Info.m_aD[t + 1] = m_Info.m_aD[v];
		m_Info.m_aGhost[t] = 0;
		m_Info.m_aGhost[t + 1] = 0;
		m_Info.m_aTop[v] = t;
		m_iVAll += 2;
		return t;
	}

	//------------------------------------------------------------------------------------------------
	protected static vector Cross(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}
}
