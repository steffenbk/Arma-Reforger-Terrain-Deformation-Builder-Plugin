//! One ground area waiting for an AI navmesh rebuild
class MUD_NavArea
{
	vector m_vMin;
	vector m_vMax;
	float m_fFirst;     // first change in it (s, ground time)
	float m_fLast;      // newest change in it
}

//! Server: AI navmesh rebuild after digging (idea: atim, 2026-10-12; vanilla does the same when a GM places
//! an object, SCR_AIWorld.RequestNavmeshRebuildEntity). Changes collect into areas; an area is rebuilt once
//! nothing changed in it for the quiet time (the digger is done), or at the latest after max wait (a barrage
//! that never stops). Ruts are not noted - too many, too shallow.
//! UNPROVEN 2026-10-12: whether the navmesh generator sees our script trimesh ground over the holed terrain.
//! Check with the AI navmesh diag after a rebuild.
class MUD_NavRefresh
{
	protected ref array<ref MUD_NavArea> m_aAreas = {};
	protected ref array<ref Shape> m_aShapes = {};     // debug: last rebuilt boxes
	protected int m_iRebuilt;

	//------------------------------------------------------------------------------------------------
	//! Add a changed box (world, margin already in). Grows an area it touches unless that area would get
	//! wider than maxSide (m); else starts a new area.
	void Note(vector lo, vector hi, float now, float maxSide)
	{
		foreach (MUD_NavArea a : m_aAreas)
		{
			if (a.m_vMin[0] > hi[0] || a.m_vMax[0] < lo[0] || a.m_vMin[2] > hi[2] || a.m_vMax[2] < lo[2])
				continue;
			vector nlo = Vector(Math.Min(a.m_vMin[0], lo[0]), Math.Min(a.m_vMin[1], lo[1]), Math.Min(a.m_vMin[2], lo[2]));
			vector nhi = Vector(Math.Max(a.m_vMax[0], hi[0]), Math.Max(a.m_vMax[1], hi[1]), Math.Max(a.m_vMax[2], hi[2]));
			if (nhi[0] - nlo[0] > maxSide || nhi[2] - nlo[2] > maxSide)
				continue;
			a.m_vMin = nlo;
			a.m_vMax = nhi;
			a.m_fLast = now;
			return;
		}
		MUD_NavArea na = new MUD_NavArea();
		na.m_vMin = lo;
		na.m_vMax = hi;
		na.m_fFirst = now;
		na.m_fLast = now;
		m_aAreas.Insert(na);
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuild every area quiet for `quiet` s or waiting `maxWait` s since its first change
	void Tick(float now, float quiet, float maxWait, bool verbose)
	{
		if (m_aAreas.IsEmpty())
			return;
		AIWorld aiw = GetGame().GetAIWorld();
		for (int i = m_aAreas.Count() - 1; i >= 0; i--)
		{
			MUD_NavArea a = m_aAreas[i];
			if (now - a.m_fLast < quiet && now - a.m_fFirst < maxWait)
				continue;
			m_aAreas.Remove(i);
			if (!aiw)
			{
				if (verbose)
					Print("MUD navmesh: no AI world, rebuild skipped", LogLevel.WARNING);
				continue;
			}
			int t0 = System.GetTickCount();
			aiw.RequestNavmeshRebuild(a.m_vMin, a.m_vMax);
			m_iRebuilt++;
			if (!verbose)
				continue;
			vector size = a.m_vMax - a.m_vMin;
			Print(string.Format("MUD navmesh: rebuild requested %1 .. %2 (%3 x %4 m), waited %5 s, request %6 ms, total %7",
				a.m_vMin, a.m_vMax, size[0], size[2], now - a.m_fFirst, System.GetTickCount(t0), m_iRebuilt));
			if (m_aShapes.Count() >= 8)
				m_aShapes.Remove(0);
			m_aShapes.Insert(Shape.Create(ShapeType.BBOX, ARGB(255, 0, 230, 255), ShapeFlags.WIREFRAME | ShapeFlags.NOZBUFFER, a.m_vMin, a.m_vMax));
		}
	}
}
