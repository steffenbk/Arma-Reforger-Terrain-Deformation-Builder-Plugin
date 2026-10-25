//! Steep trench walls (2026-10-23, user: "at 90 degrees the texture looks very stretched and i can easy just walk up
//! a 90 degree wall like its nothing i should have to vault and jump"). The heightfield is one grid of shared
//! vertices with top-down UVs, so a wall that drops within one 0.25 m step stretches its texture (1.5 m over 0.25 m =
//! 6x), and the collider blur (Collider Smooth 2 = 1 4 6 4 1) made a ~60 deg ramp of it that reached 0.5 m into the
//! trench (model in chat 2026-10-23: 1.5 m deep, 1 m wide -> collider 0.09 / 0.47 / 1.03 / 1.41 m deep across the edge).
//!  - COLLIDER: MUD_WorldGroundEntity.UpdateSmooth keeps the own height of every vertex with a step over STEP in its
//!    blur window. A 60 deg wall is 0.43 m per step, craters at most ~0.23 (m_fCraterMaxSlope 0.9): they blur as before.
//!  - LOOK: MUD_WorldGroundEntity.WallQuads puts a vertical strip in front of every steep step (over STEP), with its
//!    own UVs (along the wall x height, TILE m per tile like the groove layer), as one more section of the patch mesh.
//!    Mesh only. The stretched slope stays behind it (inside 0.25 m). Curves give 0.25 m stair steps (grid edges).
class MUD_TrenchWalls
{
	// OFF 2026-10-23 (user after the v314 test: 'this looked pretty bad, we can live with walking up the wall, probably
	// better for bugs too'): collider blur and look back to the pre-2026-10-23 state. Code kept, both switches off.
	static const bool COLLIDER = false;
	static const bool LOOK = false;
	static const float STEP = 0.6;          // m of dig depth between two neighbour vertices: 60 deg = 0.43, 75 = 0.93, 90 = all
	static const int MAX_QUADS = 512;       // per patch (m_aV / m_aI below); more are left out, counted in s_iDropped
	static const float TILE = 8;            // m per UV tile (the groove layer: 0..1 per 8 m patch)
	static const float TUCK = 0.05;         // m the strip reaches under the floor's drawn height (no seam)
	// own .emat for the strip section (a copy of the groove's DirtCover_01): a MeshObject must never use one .emat on
	// two sections (mud-gotchas 2026-10-14, crash) - the 75 / 90 deg trench crashes of 2026-10-23
	static const ResourceName WALL_MAT = "{4D55445452454E37}Assets/Mud/MUD_TrenchWall.emat";

	static int s_iDropped;
	protected static int s_iMatOk;          // 0 = not checked, 1 = WALL_MAT loads, -1 = it does not (no strips)

	//------------------------------------------------------------------------------------------------
	static bool WallMatOk()
	{
		if (s_iMatOk == 0)
		{
			s_iMatOk = -1;
			Resource r = Resource.Load(WALL_MAT);
			if (r && r.IsValid())
				s_iMatOk = 1;
			else
				Print(string.Format("MUD trench walls: material '%1' did not load - wall strips off", WALL_MAT), LogLevel.ERROR);
		}
		return s_iMatOk > 0;
	}
}

//! Wall strip mesh of the patch being built (scratch, shared by all patches)
class MUD_WallBuf
{
	vector m_aV[2048];      // MAX_QUADS * 4
	float m_aUV[4096];
	int m_aI[3072];         // MAX_QUADS * 6
	int m_iV;
	int m_iI;
}
