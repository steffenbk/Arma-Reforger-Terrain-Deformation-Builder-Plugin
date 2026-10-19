//! TEMP road look probe (whole-map cut plan 7A.3, 2026-10-18): does OUR copy of a road (a ribbon mesh with the road's
//! own .emat, from the baked Data/Mud/mud_roads.c) look close enough to the vanilla road? Rendering machines only
//! (called from MUD_LookRuts.Tick). Look at a road (centre of the screen), then:
//!   Numpad 4 = hide that RoadEntity and draw our ribbon of the whole road in its place
//!   Numpad 6 = swap: vanilla road shown + ours hidden, or back (A/B look)
//!   Numpad 5 = undo: vanilla back, our pieces deleted
//!   Numpad 1 = next V scale (V_SCALES), Numpad 2 = next UV mode (UV_MODES), both rebuild the last road at once
//! First look (user 2026-10-18, U 0-1, V x1): two lanes with a grass strip between + streaks along the road. The emat
//! is MatPBRRoad, NumVersions 2 (two road versions side by side in the texture, alpha between) - so U 0-1 drew both.
//! Ribbon: points every STEP m along the baked line (straight lines between spline points - whether the vanilla mesh
//! curves between them is one of the questions), COLS across the width, y = ground under it + LIFT, U 0-1 across,
//! V = distance / width * V_SCALE. Pieces of PIECE_STEPS steps, one local GenericEntity each.
//! Lines: 'MUD road look:'. false = no probe.
class MUD_RoadLookProbe
{
	static const bool ENABLED = true;
	static const string FILE = "$Testingplanvechmsh:Data/Mud/mud_roads.c";
	static const float REACH = 300;         // crosshair trace length (m)
	static const float MATCH = 0.1;         // baked origin vs RoadEntity origin (m)
	static const float STEP = 2;            // along the line (m)
	static const int PIECE_STEPS = 10;      // steps per piece (20 m)
	static const int COLS = 5;              // verts across
	static const float LIFT = 0.02;         // over the ground (m)
	static const float V_SCALES[] = {1, 0.25, 4, 16, 0.0625};     // V = distance / width * this (numpad 1 cycles)
	static const int V_SCALE_COUNT = 5;     // = V_SCALES length (a static const array has no .Count(), compile 2026-10-19)
	// UV modes (numpad 2 cycles): 0 U 0-1 across, 1 U 0-0.5 (version 1), 2 U 0.5-1 (version 2), 3 swapped
	// (U along = distance / width * scale, V 0-0.5 across), 4 swapped with V 0-1 across
	static const int UV_MODES = 5;
	static const int MAX_PIECES = 400;      // safety (a 1.2 km runway = 61 pieces)
	// mesh sizes: (PIECE_STEPS + 1) * COLS verts, PIECE_STEPS * (COLS - 1) * 6 indices - also the literal array
	// sizes in BuildPiece: change all together
	static const int PIECE_VERTS = 55;
	static const int PIECE_IDX = 240;

	protected static bool s_bLoaded;
	protected static ref array<vector> s_aOrigin = {};
	protected static ref array<float> s_aWidth = {};
	protected static ref array<string> s_aMat = {};
	protected static ref array<int> s_aFirst = {};     // first point index in s_aPts
	protected static ref array<int> s_aCount = {};
	protected static ref array<vector> s_aPts = {};

	protected static RoadEntity s_Road;
	protected static ref array<IEntity> s_aPieces = {};
	protected static ref array<ref Resource> s_aRes = {};
	protected static bool s_bOursShown;
	protected static ref TraceParam s_Trace;
	protected static int s_iVScale;
	protected static int s_iUVMode = 1;
	protected static int s_iLastRoad = -1;

	//------------------------------------------------------------------------------------------------
	static void Tick(float now)
	{
		if (!ENABLED)
			return;
		if (Debug.KeyState(KeyCode.KC_NUMPAD4))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD4);
			Copy();
		}
		if (Debug.KeyState(KeyCode.KC_NUMPAD6))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD6);
			Swap();
		}
		if (Debug.KeyState(KeyCode.KC_NUMPAD1))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD1);
			s_iVScale = (s_iVScale + 1) % V_SCALE_COUNT;
			Rebuild();
		}
		if (Debug.KeyState(KeyCode.KC_NUMPAD2))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD2);
			s_iUVMode = (s_iUVMode + 1) % UV_MODES;
			Rebuild();
		}
		if (Debug.KeyState(KeyCode.KC_NUMPAD5))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD5);
			Undo();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Load()
	{
		if (s_bLoaded)
			return !s_aOrigin.IsEmpty();
		s_bLoaded = true;
		FileHandle f = FileIO.OpenFile(FILE, FileMode.READ);
		if (!f)
		{
			Print("MUD road look: cannot open " + FILE, LogLevel.ERROR);
			return false;
		}
		string line;
		int done = -1;
		while (f.ReadLine(line) >= 0)
		{
			array<string> w = {};
			line.Split(" ", w, true);
			if (w.Count() == 4 && w[0] == "p")
				s_aPts.Insert(Vector(w[1].ToFloat(), w[2].ToFloat(), w[3].ToFloat()));
			else if (w.Count() == 10 && w[0] == "r")
			{
				s_aWidth.Insert(w[4].ToFloat());
				s_aCount.Insert(w[5].ToInt());
				s_aFirst.Insert(s_aPts.Count());
				s_aOrigin.Insert(Vector(w[6].ToFloat(), w[7].ToFloat(), w[8].ToFloat()));
				string mat = w[9];
				if (mat == "-")
					mat = "";
				s_aMat.Insert(mat);
			}
			else if (w.Count() >= 4 && w[0] == "MUD" && w[1] == "roads" && w[2] == "done")
				done = w[3].ToInt();
		}
		f.Close();
		int last = s_aFirst.Count() - 1;
		bool ok = done == s_aOrigin.Count() && last >= 0 && s_aFirst[last] + s_aCount[last] == s_aPts.Count();
		if (!ok)
		{
			Print(string.Format("MUD road look: %1 broken (done %2, roads %3, points %4) - refused", FILE, done, s_aOrigin.Count(), s_aPts.Count()), LogLevel.ERROR);
			s_aOrigin.Clear();
			return false;
		}
		Print(string.Format("MUD road look: loaded %1 roads, %2 points", s_aOrigin.Count(), s_aPts.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity TraceCrosshair(out vector at)
	{
		BaseWorld world = GetGame().GetWorld();
		vector cm[4];
		world.GetCurrentCamera(cm);
		if (!s_Trace)
			s_Trace = new TraceParam();
		s_Trace.Start = cm[3];
		s_Trace.End = cm[3] + cm[2] * REACH;
		s_Trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		s_Trace.Exclude = SCR_PlayerController.GetLocalControlledEntity();
		s_Trace.TraceEnt = null;
		float frac = world.TraceMove(s_Trace, null);
		at = s_Trace.Start + (s_Trace.End - s_Trace.Start) * frac;
		return s_Trace.TraceEnt;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Copy()
	{
		if (s_Road)
			Undo();
		if (!Load())
			return;
		vector at;
		RoadEntity road = RoadEntity.Cast(TraceCrosshair(at));
		if (!road)
		{
			Print(string.Format("MUD road look: no RoadEntity under the crosshair (at %1)", at));
			return;
		}
		vector o = road.GetOrigin();
		int ri = -1;
		for (int i = 0; i < s_aOrigin.Count(); i++)
		{
			if (vector.Distance(s_aOrigin[i], o) <= MATCH)
			{
				ri = i;
				break;
			}
		}
		if (ri < 0)
		{
			Print(string.Format("MUD road look: road at %1 is not in %2 - re-export?", o, FILE), LogLevel.WARNING);
			return;
		}
		if (s_aMat[ri].IsEmpty())
		{
			Print(string.Format("MUD road look: road %1 has no material - nothing to draw", ri), LogLevel.WARNING);
			return;
		}
		Build(ri, road);
	}

	//------------------------------------------------------------------------------------------------
	//! Same road again with the current V scale / UV mode (numpad 1 / 2).
	protected static void Rebuild()
	{
		RoadEntity road = s_Road;
		int ri = s_iLastRoad;
		if (!road || ri < 0)
		{
			Print(string.Format("MUD road look: V scale %1, UV mode %2 - numpad 4 on a road to see it", V_SCALES[s_iVScale], s_iUVMode));
			return;
		}
		Undo();
		Build(ri, road);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Build(int ri, RoadEntity road)
	{
		int t0 = System.GetTickCount();
		array<vector> line = {};
		Resample(ri, line);
		s_Road = road;
		s_iLastRoad = ri;
		road.Show(false);
		s_bOursShown = true;
		int built;
		for (int s = 0; s < line.Count() - 1 && built < MAX_PIECES; s += PIECE_STEPS)
		{
			if (BuildPiece(line, s, s_aWidth[ri], s_aMat[ri]))
				built++;
		}
		Print(string.Format("MUD road look: road %1 width %2 mat %3 - %4 samples, %5 pieces in %6 ms (road hidden), V scale %7, UV mode %8",
			ri, s_aWidth[ri], s_aMat[ri], line.Count(), built, System.GetTickCount(t0), V_SCALES[s_iVScale], s_iUVMode));
	}

	//------------------------------------------------------------------------------------------------
	//! Points every STEP m along road ri's polyline (xz distance), first and last point included.
	protected static void Resample(int ri, notnull array<vector> outPts)
	{
		int a = s_aFirst[ri];
		int n = s_aCount[ri];
		outPts.Insert(s_aPts[a]);
		float carry = 0;    // distance since the last sample
		for (int k = 0; k < n - 1; k++)
		{
			vector p0 = s_aPts[a + k];
			vector p1 = s_aPts[a + k + 1];
			float len = vector.DistanceXZ(p0, p1);
			if (len <= 0.001)
				continue;
			float d = STEP - carry;
			while (d < len)
			{
				outPts.Insert(p0 + (p1 - p0) * (d / len));
				d += STEP;
			}
			carry = len - (d - STEP);
		}
		vector end = s_aPts[a + n - 1];
		if (vector.DistanceXZ(outPts[outPts.Count() - 1], end) > 0.01)
			outPts.Insert(end);
	}

	//------------------------------------------------------------------------------------------------
	//! One piece from sample s to s + PIECE_STEPS (short last piece: the last row repeats = zero-area quads).
	protected static bool BuildPiece(notnull array<vector> line, int s, float width, string mat)
	{
		int last = line.Count() - 1;
		vector base = line[s];
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = base;
		IEntity e = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
		if (!e)
			return false;
		e.ClearFlags(EntityFlags.TRACEABLE, false);
		// static arrays with literal sizes (a const-sized static array is unproven here); keep = PIECE_VERTS / PIECE_IDX
		vector verts[55];
		float uvs[110];
		int idx[240];
		float dist = s * STEP;
		for (int r = 0; r <= PIECE_STEPS; r++)
		{
			int si = Math.Min(s + r, last);
			vector c = line[si];
			vector prev = line[Math.Max(si - 1, 0)];
			vector next = line[Math.Min(si + 1, last)];
			vector t = next - prev;
			t[1] = 0;
			t.Normalize();
			vector side = Vector(t[2], 0, -t[0]);
			if (r > 0 && s + r <= last)
				dist += vector.DistanceXZ(line[si - 1], c);
			for (int col = 0; col < COLS; col++)
			{
				float u = col / (COLS - 1.0);
				vector w = c + side * ((u - 0.5) * width);
				w[1] = GroundY(w) + LIFT;
				int v = r * COLS + col;
				verts[v] = w - base;
				float along = dist / width * V_SCALES[s_iVScale];
				float across = u;
				if (s_iUVMode == 1 || s_iUVMode == 3)
					across = u * 0.5;
				else if (s_iUVMode == 2)
					across = 0.5 + u * 0.5;
				if (s_iUVMode >= 3)
				{
					uvs[v * 2] = along;
					uvs[v * 2 + 1] = across;
				}
				else
				{
					uvs[v * 2] = across;
					uvs[v * 2 + 1] = along;
				}
			}
		}
		int n;
		for (int r2 = 0; r2 < PIECE_STEPS; r2++)
		{
			for (int c2 = 0; c2 < COLS - 1; c2++)
			{
				int qa = r2 * COLS + c2;
				int qb = qa + 1;            // + side
				int qc = qa + COLS;         // + along
				int qd = qc + 1;
				// (c - a) x (b - a) = along x side = up (same winding as MUD_BoundsMesh)
				idx[n] = qa;
				idx[n + 1] = qc;
				idx[n + 2] = qb;
				idx[n + 3] = qb;
				idx[n + 4] = qc;
				idx[n + 5] = qd;
				n += 6;
			}
		}
		int numVerts[] = {PIECE_VERTS};
		int numIndices[] = {PIECE_IDX};
		string materials[] = {mat};
		Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!res || !res.GetResource())
		{
			Print(string.Format("MUD road look: MeshObject.Create failed - material '%1' did not load?", mat), LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(e);
			return false;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		mesh.UpdateIndices(0, idx);       // indices first: UpdateVerts rebuilds normals from the triangle list
		mesh.UpdateVerts(0, verts, uvs);
		e.SetObject(mesh, "");
		e.Update();
		s_aPieces.Insert(e);
		s_aRes.Insert(res);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Ground y under w: trace down (our ground or the terrain; the hidden road has no collider, our pieces are
	//! not traceable). Miss = terrain height.
	protected static float GroundY(vector w)
	{
		BaseWorld world = GetGame().GetWorld();
		s_Trace.Start = w + Vector(0, 5, 0);
		s_Trace.End = w - Vector(0, 10, 0);
		s_Trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		s_Trace.Exclude = SCR_PlayerController.GetLocalControlledEntity();
		s_Trace.TraceEnt = null;
		float frac = world.TraceMove(s_Trace, null);
		if (frac >= 1)
			return world.GetSurfaceY(w[0], w[2]);
		return s_Trace.Start[1] + (s_Trace.End[1] - s_Trace.Start[1]) * frac;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Swap()
	{
		if (!s_Road)
		{
			Print("MUD road look: nothing to swap - numpad 4 on a road first");
			return;
		}
		s_bOursShown = !s_bOursShown;
		s_Road.Show(!s_bOursShown);
		foreach (IEntity e : s_aPieces)
		{
			if (!e)
				continue;
			if (s_bOursShown)
				e.SetFlags(EntityFlags.VISIBLE, false);
			else
				e.ClearFlags(EntityFlags.VISIBLE, false);
		}
		string shown = "vanilla";
		if (s_bOursShown)
			shown = "ours";
		Print("MUD road look: showing " + shown);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Undo()
	{
		if (s_Road)
			s_Road.Show(true);
		int n;
		foreach (IEntity e : s_aPieces)
		{
			if (!e)
				continue;
			SCR_EntityHelper.DeleteEntityAndChildren(e);
			n++;
		}
		s_aPieces.Clear();
		s_aRes.Clear();
		s_Road = null;      // s_iLastRoad stays: Rebuild keeps the road it had
		Print(string.Format("MUD road look: undone, %1 pieces deleted, vanilla road back", n));
	}
}
