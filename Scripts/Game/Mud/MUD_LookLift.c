//! LOOK LIFT (user 2026-10-23, v306 = before the probe; plans/2026-10-23-bone-tile-mud.md 'Look lift probe' and
//! 'Look lift for real': the user picked it as what ships, step 1 = own switch here, ticked by MUD_WorldGround).
//! With ENABLED the coarse skin hole cells and every patch mesh are drawn LiftM() higher
//! (look only, colliders untouched) = thick mud over the whole offroad with no tile entities. Grass rides on any drawn
//! ground above the heightmap (mud-gotchas 2026-10-08), so the lift must go back down where something drove or stands.
//! MUD_WorldGroundEntity.LiftAt asks PressedOut per patch vertex - no lift where:
//!   - the drawn ground is cut (d under CUT_D: rut floor, crater, dig),
//!   - the synced wear count is > 0 (every pass counts, pass 1 too; in the JIP snapshot, MUD_VoxelProbe m_aWear),
//!   - this client sees a wheel in contact or a player character on foot there (local, held WHEEL_HOLD / FOOT_HOLD:
//!     the server's wear lands a few hundred ms after the wheel; feet leave no wear, so their press springs back).
//! A new press re-draws its patches (MUD_WorldGroundEntity.LookDirty: mesh only, no collider, no net). A missing
//! patch draws nothing (the skin carries it): those keys are not kept, the next pass tries again; players on foot
//! make the patch under them while this runs (MakeUnder, called from NearPlayers).
//! Client look only, nothing synced. Ticked from MUD_WorldGroundEntity every frame on render machines (never a DS:
//! skin and patch lift both check m_bRender, so a DS draws and lifts nothing).
//! Log (TEST_KIT, every 5 s): 'MUD look lift: ...' pressed vertices, presses, re-draw calls, missing patch, expired.
class MUD_LookLift
{
	static const bool ENABLED = true;       // the look lift on (was MUD_MudSheet.LOOK_LIFT_TEST until 2026-10-23)
	static const float LIFT_M = 0.05;       // m BASE drawn over the ground (was 0.25 flat until v321; numpad 0 steps it)
	// GROW (user 2026-10-23: 'feet are under the layer, the camera goes under it in 3rd person; start at the minimum that
	// fixes our feet, driving back and forth in the same place makes it higher'): the base stays low, and every skin
	// vertex (2 m) round driven ground lifts GROW_PASS more per pass, up to GROW_MAX. Passes come from the SYNCED rut
	// field (wear = pass 1, then 1 + cut depth / RUT_PASS_MAX), so every machine and a joiner (snapshot) agree.
	// Skin and patches read the same vertex lift (VertexLift), so the seam rule of 2026-10-23 holds.
	static const bool GROW = true;
	static const float GROW_PASS = 0.05;    // m more lift per pass round driven ground
	static const float GROW_MAX = 0.3;      // m most extra lift (base + this = the top, near a deep rut)
	static const float GROW_STEP = 0.01;    // m: grow is kept in whole steps - smaller changes do not re-draw
	static const int GROW_SKIN_PER = 2;     // skin vertices whose chunks re-build per press pass (EVERY) at most
	static const int SKEY = 20000;          // skin vertex key = sz * SKEY + sx
	static const bool PRESS_OUT = true;     // false = ENABLED lifts everything (the 2026-10-19 test)
	static const float CUT_D = -0.003;      // drawn d under this = cut ground: no lift
	static const float WEAR_MIN = 0.001;    // synced wear above this = no lift
	static const float R_WHEEL = 0.25;      // half width of a wheel press, m
	static const float R_FOOT = 0.3;
	static const float R_PRONE = 0.45;
	static const float PRONE_HALF = 1.0;    // prone body: pressed from origin - this to + this along the facing (INFERENCE where the origin sits)
	static const float RANGE = 150;         // m from the camera
	static const float MAKE_R = 1;          // MakeUnder: a missing patch this close to a player on foot is made
	static const float EVERY = 0.1;         // s between press passes
	static const float JUMP = 3;            // wheel farther than this from its last press point (air, teleport): no line
	static const float WHEEL_HOLD = 3;      // s a wheel press stays (the server wear lands within that)
	static const float FOOT_HOLD = 0.6;     // s a foot / body press stays after the character left
	static const float SWEEP = 0.25;        // s between expiry passes
	static const int KEYW = 20000;          // key = gx * KEYW + gz (global fine vertex; the grid is 16384 per side today)
	static const int BOX_MAX = 40;          // fine vertices per side of one press box at most
	static const bool BODY_PRESS = true;    // the vehicle body presses the lift where it hangs into it (BodyPress)
	static const float BODY_RANGE = 60;     // m from the camera for body presses
	static const float BODY_STEP = 0.5;     // m between body samples (grows for big boxes, BODY_MAX)
	static const int BODY_MAX = 120;        // body samples per vehicle per pass at most
	static const float R_BODY = 0.35;       // press radius round a body sample
	// SNOW TEST (user 2026-10-23, stable test build: 'a section of the map like 1/3 ... 0.75 m high ... white to
	// simulate snow'): the north part of the map from SNOW_FROM of its depth (skin rows, cut to a whole skin chunk so
	// skin chunks and patches switch material on the same line) is lifted SNOW_M and drawn with SNOW_MAT. The lift
	// ramps up over SNOW_RAMP skin vertices inside the zone edge. Look only, as the rest of the lift; GROW adds on top.
	static const bool SNOW = true;
	static const float SNOW_M = 0.75;       // m base lift in the snow zone
	static const float SNOW_FROM = 0.6667;  // share of the map depth (south edge = 0) where the zone starts
	static const int SNOW_RAMP = 8;         // skin vertices (2 m each) from the zone edge to the full SNOW_M
	static const ResourceName SNOW_MAT = "{4D55534E4F573031}Assets/Mud/Data/MUD_Snow.emat";
	protected static int s_iSnowRow = -1;   // first skin vertex row of the zone (-1 = not known yet)
	protected static int s_iSnowNcz;        // skin rows s_iSnowRow was cut for
	protected static int s_iSnowMatOk;      // 0 = not checked, 1 = SNOW_MAT loads, -1 = it does not (no white)

	protected static ref map<int, float> s_mUntil;              // pressed vertex -> world time it stays pressed until
	// BODY CAP (user 2026-10-23, 0.75 m snow: 'the body flattens the rut down to the floor, it doesn't stop at the vehicle
	// height'): a body press no longer presses to our ground - it caps the lift just under the body's underside, so the
	// gap between frame and ground keeps its snow. Wheels / feet / wear still press to the ground (s_mUntil).
	static const float BODY_GAP = 0.03;     // m the capped lift stays under the body
	static const float NO_CAP = 1000;
	// LOOK SLOPE (user shot 2026-10-23: deep presses in 0.75 m snow left sharp spikes - single lifted vertices between
	// pressed ones stood the full lift high): the drawn lift rises at most SLOPE m per m away from a pressed / capped
	// vertex (MUD_WorldGroundEntity.LookCapSetup). Its reach is MUD_WorldGroundEntity.LOOK_CAP_R fine vertices, so a
	// press re-draws the patches within that (DIRTY_PAD).
	static const float SLOPE = 1.0;
	static const int DIRTY_PAD = 6;         // fine vertices round a press box that re-draw (LOOK_CAP_R + 1)
	// FPS FIX (user 2026-10-23 'this change has done something to our fps'; WB log 22:00: 2933 ms script per 10 s =
	// ~19 ms per patch cap, 153 caps): the slope runs only where the base lift is over SLOPE_MIN_LIFT (the snow zone -
	// spikes of base 0.05 + GROW <= 0.35 were never seen), and the wide re-draw pad only there too.
	static const float SLOPE_MIN_LIFT = 0.4;
	protected static ref map<int, float> s_mCap;                // capped vertex -> most lift (m over our ground)
	protected static ref map<int, float> s_mCapUntil;           // capped vertex -> world time the cap holds until
	protected static ref map<int, float> s_mGrow;               // GROW: skin vertex key -> extra lift (m)
	protected static ref array<int> s_aGrowSkin;                // GROW: skin vertex keys whose skin chunks still re-build
	protected static ref set<int> s_sGrowSkin;                  // GROW: the same keys, for the 'already queued' test
	protected static int s_iGrowUps;                            // GROW: vertices raised (5 s log)
	protected static int s_iGrowSkin;                           // GROW: skin vertices re-built (5 s log)
	protected static ref map<Vehicle, ref array<vector>> s_mLast;   // vehicle -> last press point per wheel
	protected static ref array<int> s_aNew;                     // scratch: keys one Press added
	protected static ref array<int> s_aGone;                    // scratch: expired keys
	protected static ref array<int> s_aIds;                     // scratch: player ids
	protected static ref map<Vehicle, vector> s_mBodyAt;        // vehicle -> (x, time, z) of its last body trace pass
	protected static ref TraceParam s_BodyTrace;
	protected static ref array<Vehicle> s_aHelis = {};          // BodyPress: helis (MUD_HeliGround registry)
	protected static int s_iBodyHits;
	protected static float s_fNext;
	protected static float s_fNextSweep;
	protected static float s_fNextLog;
	protected static float s_fLastT = -1;
	protected static float s_fX0;
	protected static float s_fZ0;
	protected static float s_fFine;
	protected static int s_iPresses;
	protected static int s_iDirtyCalls;
	protected static int s_iMissing;
	protected static int s_iExpired;
	protected static int s_iMs;

	// TEST_KIT numpad . (user 2026-10-23: '3 options - our new ruts only, the ruts on our actual terrain only, both on';
	// first ask: what made the square cut-out over a top-down tunnel?). Cycle BOTH -> LIFT ONLY -> TERRAIN ONLY -> BOTH.
	// LIFT ONLY = only the dents pressed into the look lift: the server cuts no new rut segment (MUD_VoxelProbe.RutWheel;
	// works where this machine is the server = WB play / host, a peer client pauses nothing) and, as no wear lands then,
	// local wheel dents are held for good instead of WHEEL_HOLD. TERRAIN ONLY = lift off (look drawn at the collider,
	// re-drawn within RANGE of the camera): only the real rut field / voxel ruts show.
	static const int MODE_BOTH = 0;
	static const int MODE_LIFT_ONLY = 1;
	static const int MODE_TERRAIN_ONLY = 2;
	static const int MODES = 3;
	static const float HOLD_FOREVER = 1000000;
	protected static int s_iMode;
	// TEST_KIT numpad 0 (user 2026-10-23: 'a button to increase the lift ruts height'): next height of HeightOf, wraps
	// to the lowest; re-draws skin + patches within RANGE (farther ground keeps the old height until it re-draws)
	static const int HEIGHTS = 6;
	protected static int s_iHeight = -1;     // index into HeightOf, -1 = MUD_LookLift.LIFT_M

	//------------------------------------------------------------------------------------------------
	//! Drawn lift height now (m): every look lift user reads this, not LIFT_M
	static float LiftM()
	{
		if (s_iHeight < 0)
			return MUD_LookLift.LIFT_M;
		return HeightOf(s_iHeight);
	}

	//------------------------------------------------------------------------------------------------
	protected static float HeightOf(int i)
	{
		// base heights since GROW (v322): the grow adds on top
		if (i == 0)
			return 0.02;
		if (i == 1)
			return 0.05;
		if (i == 2)
			return 0.1;
		if (i == 3)
			return 0.15;
		if (i == 4)
			return 0.25;
		return 0.35;
	}

	//------------------------------------------------------------------------------------------------
	//! SNOW: first skin vertex row of the zone, -1 = off or no skin yet
	protected static int SnowRow()
	{
		if (!SNOW)
			return -1;
		MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
		if (!skin)
			return -1;
		float x0;
		float z0;
		int ncx;
		int ncz;
		skin.GetGrid(x0, z0, ncx, ncz);
		if (ncz <= 0)
			return -1;
		if (s_iSnowRow < 0 || s_iSnowNcz != ncz)
		{
			s_iSnowNcz = ncz;
			int chunk = MUD_CoarseSkinEntity.CELLS;
			s_iSnowRow = Math.Floor(ncz * SNOW_FROM / chunk) * chunk;
		}
		return s_iSnowRow;
	}

	//------------------------------------------------------------------------------------------------
	//! Base lift (m) at skin row fz (may be fractional): LiftM(), ramped up to SNOW_M inside the snow zone
	protected static float BaseAt(float fz)
	{
		int row = SnowRow();
		if (row < 0 || fz < row || SNOW_M <= LiftM())
			return LiftM();
		float t = Math.Min(1, (fz - row) / SNOW_RAMP);
		return LiftM() + (SNOW_M - LiftM()) * t;
	}

	//------------------------------------------------------------------------------------------------
	//! Base lift (m) at world z (BaseAt by the skin grid)
	protected static float BaseAtZ(float z)
	{
		MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
		if (!SNOW || !skin || skin.GetCell() <= 0)
			return LiftM();
		float x0;
		float z0;
		int ncx;
		int ncz;
		skin.GetGrid(x0, z0, ncx, ncz);
		return BaseAt((z - z0) / skin.GetCell());
	}

	//------------------------------------------------------------------------------------------------
	//! Highest base lift anywhere (MUD_CameraLift skip band)
	static float MaxBase()
	{
		if (SNOW && SNOW_M > LiftM())
			return SNOW_M;
		return LiftM();
	}

	//------------------------------------------------------------------------------------------------
	//! SNOW: surface material of a skin chunk / patch whose south edge is at world z (normal = the usual one).
	//! Zone rows are cut to whole skin chunks, so a chunk or patch is all in or all out.
	static string SurfaceMat(string usual, float z)
	{
		MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
		int row = SnowRow();
		if (row < 0 || !ENABLED || skin.GetCell() <= 0)
			return usual;
		float x0;
		float z0;
		int ncx;
		int ncz;
		skin.GetGrid(x0, z0, ncx, ncz);
		if ((z - z0) / skin.GetCell() < row - 0.01)
			return usual;
		if (s_iSnowMatOk == 0)
		{
			s_iSnowMatOk = -1;
			Resource r = Resource.Load(SNOW_MAT);
			if (r && r.IsValid())
				s_iSnowMatOk = 1;
			else
				Print(string.Format("MUD snow: material '%1' did not load - snow zone drawn with the normal look", SNOW_MAT), LogLevel.ERROR);
		}
		if (s_iSnowMatOk < 0)
			return usual;
		return SNOW_MAT;
	}

	//------------------------------------------------------------------------------------------------
	//! Lift (m) of global skin vertex (sx, sz) where it lifts at all (MUD_CoarseSkinEntity.LiftsVertex): base + grow.
	//! The skin (LookLift) and the patches (LookLiftSetup) both read this - one number per shared point.
	static float VertexLift(int sx, int sz)
	{
		float lift = BaseAt(sz);
		if (!GROW || !s_mGrow || s_mGrow.IsEmpty())
			return lift;
		return lift + s_mGrow.Get(sz * SKEY + sx);
	}

	//------------------------------------------------------------------------------------------------
	//! GROW at world x, z: bilinear over the 4 skin vertices round it (0 before the first press pass set the grid)
	static float GrowAt(float x, float z)
	{
		if (!GROW || !s_mGrow || s_mGrow.IsEmpty() || s_fFine <= 0)
			return 0;
		float cell = s_fFine * MUD_WorldGroundEntity.PCELLS / MUD_WorldGroundEntity.SKIN;
		float fx = (x - s_fX0) / cell;
		float fz = (z - s_fZ0) / cell;
		int sx = Math.Floor(fx);
		int sz = Math.Floor(fz);
		float u = fx - sx;
		float w = fz - sz;
		float g0 = s_mGrow.Get(sz * SKEY + sx) * (1 - u) + s_mGrow.Get(sz * SKEY + sx + 1) * u;
		float g1 = s_mGrow.Get((sz + 1) * SKEY + sx) * (1 - u) + s_mGrow.Get((sz + 1) * SKEY + sx + 1) * u;
		return g0 * (1 - w) + g1 * w;
	}

	//------------------------------------------------------------------------------------------------
	//! New play (MUD_WorldGroundEntity start): forget the grow of the last play
	static void GrowReset()
	{
		s_mGrow = new map<int, float>();
		s_aGrowSkin = {};
		s_sGrowSkin = new set<int>();
	}

	//------------------------------------------------------------------------------------------------
	//! GROW: synced rut field samples [gx0..gx1] x [gz0..gz1] (global fine verts = floor samples) changed (rut op,
	//! wear pass, JIP snapshot). Each driven sample raises the 4 skin vertices of its skin cell to its pass count's
	//! grow (never lowers: ruts only deepen). A raised vertex re-draws the patches round it (mesh only) and queues its
	//! skin chunks (re-built a few per pass in Tick, and only where a skin cell there still draws). Render machines only.
	static void GrowBox(int gx0, int gx1, int gz0, int gz1)
	{
		if (!GROW || !MUD_LookLift.ENABLED)
			return;
		if (!s_mGrow)
			GrowReset();
		float x0;
		float z0;
		float psize = MUD_WorldGroundEntity.ProbePatchGrid(x0, z0);
		if (psize <= 0)
			return;
		float fine = psize / MUD_WorldGroundEntity.PCELLS;
		int per = MUD_WorldGroundEntity.PCELLS / MUD_WorldGroundEntity.SKIN;     // fine verts per skin cell (8)
		// every 2nd sample (0.5 m, even indices): a wheel rut core is 2 x R_WHEEL = 0.5 m wide, so one lands in it;
		// cost per op is a quarter of a full scan (review 2026-10-23: a long op box is ~1000+ samples)
		int sz0 = gz0 + (gz0 & 1);
		int sx0 = gx0 + (gx0 & 1);
		if (gz1 == gz0)
			sz0 = gz0;     // a single sample (wear pass) is always read
		if (gx1 == gx0)
			sx0 = gx0;
		for (int gz = sz0; gz <= gz1; gz += 2)
		{
			for (int gx = sx0; gx <= gx1; gx += 2)
			{
				float x = x0 + gx * fine;
				float z = z0 + gz * fine;
				float pass = Math.Min(MUD_VoxelProbe.WearAt(x, z), 1);
				float fl = MUD_VoxelProbe.FieldFloor(gx, gz);
				if (fl < MUD_VoxelProbe.FLOOR_NONE * 0.5)
				{
					float dug;
					float undug;
					if (MUD_WorldGroundEntity.DugUndug(x, z, dug, undug) && undug - fl > 0.001)
						pass = 1 + (undug - fl) / MUD_VoxelProbe.RUT_PASS_MAX;
				}
				if (pass <= WEAR_MIN)
					continue;
				float g = Math.Min(GROW_MAX, GROW_PASS * pass);
				g = Math.Floor(g / GROW_STEP) * GROW_STEP;
				if (g <= 0)
					continue;
				int cx = gx / per;     // gx, gz >= 0 on the grid
				int cz = gz / per;
				for (int sz = cz; sz <= cz + 1; sz++)
				{
					for (int sx = cx; sx <= cx + 1; sx++)
					{
						int key = sz * SKEY + sx;
						if (s_mGrow.Get(key) >= g - 0.0001)
							continue;
						s_mGrow.Set(key, g);
						s_iGrowUps++;
						MUD_WorldGroundEntity.LookDirty(sx * per - per, sx * per + per, sz * per - per, sz * per + per);
						if (!s_sGrowSkin.Contains(key))
						{
							s_sGrowSkin.Insert(key);
							s_aGrowSkin.Insert(key);
						}
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static bool LiftOff()
	{
		return s_iMode == MODE_TERRAIN_ONLY;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.RutWheel: true = no new rut segment (numpad . LIFT ONLY)
	static bool RutsPaused()
	{
		return s_iMode == MODE_LIFT_ONLY;
	}

	//------------------------------------------------------------------------------------------------
	protected static string ModeText()
	{
		if (s_iMode == MODE_LIFT_ONLY)
			return "LIFT RUTS ONLY (dents in the lifted look, held; no real ruts cut)";
		if (s_iMode == MODE_TERRAIN_ONLY)
			return "TERRAIN RUTS ONLY (lift off within " + RANGE.ToString() + " m; real ruts as before)";
		return "BOTH ON (lift dents + real ruts)";
	}

	//------------------------------------------------------------------------------------------------
	//! Re-draw the patches (and with skin the skin chunks) within RANGE of the camera; returns patches marked
	protected static int Redraw(bool skin, out int chunks)
	{
		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		vector c = cam[3];
		int ga = Math.Floor((c[0] - RANGE - s_fX0) / s_fFine);
		int gb = Math.Ceil((c[0] + RANGE - s_fX0) / s_fFine);
		int za = Math.Floor((c[2] - RANGE - s_fZ0) / s_fFine);
		int zb = Math.Ceil((c[2] + RANGE - s_fZ0) / s_fFine);
		chunks = 0;
		if (skin)
			chunks = MUD_CoarseSkinEntity.RedrawNear(c, RANGE);
		return MUD_WorldGroundEntity.LookDirty(ga, gb, za, zb);
	}

	//------------------------------------------------------------------------------------------------
	protected static void KeyTick()
	{
#ifdef ENABLE_DIAG
		if (!MUD_TestKit.TEST_KIT)
			return;
		if (!MUD_MudLab.LabOn() && Debug.KeyState(KeyCode.KC_NUMPAD0))
		{
			// numpad 0 is the sheet lab's lever key while a lab mode runs (MUD_LabSheetLocal)
			Debug.ClearKey(KeyCode.KC_NUMPAD0);
			int next = 0;
			for (int i = HEIGHTS - 1; i >= 0; i--)
			{
				if (HeightOf(i) > LiftM() + 0.001)
					next = i;
			}
			s_iHeight = next;
			int hChunks;
			int hPatches = Redraw(true, hChunks);
			string hTxt = "LIFT HEIGHT " + LiftM().ToString() + " m (numpad 0 = higher, wraps)";
			Print(string.Format("MUD look lift: %1 - re-drawn %2 patches, %3 skin chunks", hTxt, hPatches, hChunks), LogLevel.NORMAL);
			SCR_HintManagerComponent.ShowCustomHint(hTxt + "\n" + ModeText(), "Mud look lift", 5, true);
		}
		if (!Debug.KeyState(KeyCode.KC_DECIMAL))
			return;
		Debug.ClearKey(KeyCode.KC_DECIMAL);
		bool wasOff = LiftOff();
		bool wasHeld = s_iMode == MODE_LIFT_ONLY;
		s_iMode = (s_iMode + 1) % MODES;
		int patches;
		int chunks;
		if (wasHeld)
		{
			s_mUntil.Clear();     // the held dents go (no wear under them)
			s_mCap.Clear();
			s_mCapUntil.Clear();
		}
		if (wasOff != LiftOff() || wasHeld)
			patches = Redraw(wasOff != LiftOff(), chunks);
		Print(string.Format("MUD look lift: mode %1 = %2 (numpad .) - re-drawn %3 patches, %4 skin chunks", s_iMode, ModeText(), patches, chunks), LogLevel.NORMAL);
		SCR_HintManagerComponent.ShowCustomHint(ModeText() + "\n(numpad .)", "Mud look lift", 5, true);
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Patch vertex at global fine vertex gx, gz (world wx, wz), drawn displacement d: true = draw it without the lift
	static bool PressedOut(int gx, int gz, float d, float wx, float wz)
	{
		if (!PRESS_OUT)
			return false;
		if (d < CUT_D)
			return true;
		if (MUD_VoxelProbe.WearAt(wx, wz) > WEAR_MIN)
			return true;
		if (s_mUntil && !s_mUntil.IsEmpty() && s_mUntil.Contains(gx * KEYW + gz))
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! How much higher than our ground (collider) the look is drawn at world x, z - for things laid ON the drawn
	//! ground (trench ghost, user 2026-10-23: the preview lay hidden under the lift). 0 = test off, kept terrain,
	//! cut ground, or pressed out (wear / a local press). INFERENCE: ignores the 2 m ramp down to a road / voxel cell
	//! (LookLiftSetup), so within 2 m of one it can sit up to the lift too high (a preview / the camera clamp
	//! MUD_CameraLift, so on top is the safe side). Base + GROW (GrowAt, bilinear over the skin vertices, as drawn).
	static float DrawnLift(float x, float z)
	{
		if (!MUD_LookLift.ENABLED || LiftOff())
			return 0;
		BaseWorld world = GetGame().GetWorld();
		if (!world || !MUD_Surface.IsHole(world, x, z))
			return 0;
		if (PRESS_OUT)
		{
			float dug;
			float undug;
			if (MUD_WorldGroundEntity.DugUndug(x, z, dug, undug) && dug - undug < CUT_D)
				return 0;
			if (MUD_VoxelProbe.WearAt(x, z) > WEAR_MIN)
				return 0;
			if (s_mUntil && s_fFine > 0 && !s_mUntil.IsEmpty())
			{
				int gx = Math.Round((x - s_fX0) / s_fFine);
				int gz = Math.Round((z - s_fZ0) / s_fFine);
				if (s_mUntil.Contains(gx * KEYW + gz))
					return 0;
			}
		}
		float drawn = BaseAtZ(z) + GrowAt(x, z);
		if (s_mCap && s_fFine > 0 && !s_mCap.IsEmpty())
			drawn = Math.Min(drawn, CapAt(Math.Round((x - s_fX0) / s_fFine), Math.Round((z - s_fZ0) / s_fFine)));
		return drawn;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-draw pad (fine vertices) round a press at global fine row gz: DIRTY_PAD where the slope runs, else 1
	protected static int PadAt(int gz)
	{
		if (BaseAtZ(s_fZ0 + gz * s_fFine) > SLOPE_MIN_LIFT)
			return DIRTY_PAD;
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! LOOK SLOPE seed (MUD_WorldGroundEntity.LookCapSetup): most lift (m) the vertex itself allows - 0 = pressed out
	//! (local press, synced wear, cut ground by DugUndug as DrawnLift), a body cap, or NO_CAP
	static float SeedAt(int gx, int gz, float x, float z)
	{
		if (!PRESS_OUT || gx < 0 || gz < 0 || gx >= KEYW || gz >= KEYW)
			return NO_CAP;
		int key = gx * KEYW + gz;
		if (s_mUntil && !s_mUntil.IsEmpty() && s_mUntil.Contains(key))
			return 0;
		if (MUD_VoxelProbe.WearAt(x, z) > WEAR_MIN)
			return 0;
		// FPS FIX: no DugUndug cut test here (cost) - cut ground (crater / trench) does not seed the slope
		float cap;
		if (s_mCap && !s_mCap.IsEmpty() && s_mCap.Find(key, cap))
			return cap;
		return NO_CAP;
	}

	//------------------------------------------------------------------------------------------------
	//! BODY CAP: most lift (m) at global fine vertex gx, gz (NO_CAP = none). MUD_WorldGroundEntity.LiftAt takes the min.
	static float CapAt(int gx, int gz)
	{
		if (!s_mCap || s_mCap.IsEmpty())
			return NO_CAP;
		float cap;
		if (s_mCap.Find(gx * KEYW + gz, cap))
			return cap;
		return NO_CAP;
	}

	//------------------------------------------------------------------------------------------------
	//! NearPlayers (MUD_WorldGroundEntity): true = make the missing patch under this player entity now (on foot,
	//! near the camera), so the press under the feet has a patch to draw in
	static bool MakeUnder(IEntity ent)
	{
		if (!PRESS_OUT || !MUD_LookLift.ENABLED)
			return false;
		ChimeraCharacter ch = ChimeraCharacter.Cast(ent);
		if (!ch || ch.IsInVehicle())
			return false;
		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		return vector.DistanceSqXZ(cam[3], ch.GetOrigin()) < RANGE * RANGE;
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame from MUD_WorldGroundEntity (render machines, ground ready; not the mud lab since 2026-10-23)
	static void Tick(float time)
	{
		if (!PRESS_OUT || !MUD_LookLift.ENABLED)
			return;
		if (!s_mUntil || time < s_fLastT)
		{
			// first call or a new play (world time went back): start clean
			s_mUntil = new map<int, float>();
			s_mCap = new map<int, float>();
			s_mCapUntil = new map<int, float>();
			s_mLast = new map<Vehicle, ref array<vector>>();
			s_aNew = {};
			s_aGone = {};
			s_aIds = {};
			s_mBodyAt = new map<Vehicle, vector>();
			s_BodyTrace = new TraceParam();
			s_fNext = 0;
			s_fNextSweep = 0;
			s_fNextLog = time + 5;
			s_iMode = 0;
			s_iHeight = -1;
			s_fFine = 0;
		}
		s_fLastT = time;
		if (s_fFine > 0)
			KeyTick();     // needs the grid (s_fX0 / s_fZ0 / s_fFine) from a pass below
		if (time < s_fNext)
			return;
		s_fNext = time + EVERY;
		float gx0;
		float gz0;
		float psize = MUD_WorldGroundEntity.ProbePatchGrid(gx0, gz0);
		if (psize <= 0)
			return;
		s_fX0 = gx0;
		s_fZ0 = gz0;
		s_fFine = psize / MUD_WorldGroundEntity.PCELLS;
		if (s_mLast.Count() > 2 * MUD_Wheels.GetAll().Count() + 8)
		{
			s_mLast.Clear();     // deleted vehicles' keys
			s_mBodyAt.Clear();
		}
		int t0 = System.GetTickCount();
		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		vector cp = cam[3];
		float r2 = RANGE * RANGE;

		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh || vector.DistanceSqXZ(veh.GetOrigin(), cp) > r2)
				continue;
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
			if (!sim || !sim.IsValid())
				continue;
			array<vector> last = s_mLast.Get(veh);
			if (!last)
			{
				last = {};
				s_mLast.Set(veh, last);
			}
			int nw = sim.WheelCount();
			while (last.Count() < nw)
				last.Insert(Vector(0, -99999, 0));
			for (int w = 0; w < nw; w++)
			{
				if (!sim.WheelHasContact(w))
				{
					last[w] = Vector(0, -99999, 0);
					continue;
				}
				vector p = MUD_VoxelProbe.RutPoint(veh, sim, w);
				vector a = last[w];
				if (a[1] < -99000 || vector.DistanceXZ(a, p) > JUMP)
					a = p;
				float hold = time + WHEEL_HOLD;
				if (s_iMode == MODE_LIFT_ONLY)
					hold = HOLD_FOREVER;     // numpad . LIFT ONLY: no wear lands, keep the dent
				Press(a, p, R_WHEEL, hold);
				last[w] = p;
			}
			if (BODY_PRESS && vector.DistanceSqXZ(veh.GetOrigin(), cp) < BODY_RANGE * BODY_RANGE)
				BodyPress(veh, time);
		}
		// helis (user 2026-10-24, 0.75 m snow: 'it doesnt take into account the body or hull, only wheels and skid'):
		// MUD_Wheels holds wheeled vehicles only, so a heli body never pressed the lift
		if (BODY_PRESS)
		{
			s_aHelis.Clear();
			MUD_HeliGround.GetAll(s_aHelis);
			foreach (Vehicle hv : s_aHelis)
			{
				if (hv && vector.DistanceSqXZ(hv.GetOrigin(), cp) < BODY_RANGE * BODY_RANGE)
					BodyPress(hv, time);
			}
		}

		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
		{
			s_aIds.Clear();
			pm.GetPlayers(s_aIds);
			foreach (int id : s_aIds)
			{
				ChimeraCharacter ch = ChimeraCharacter.Cast(pm.GetPlayerControlledEntity(id));
				if (!ch || ch.IsInVehicle())
					continue;
				vector o = ch.GetOrigin();
				if (vector.DistanceSqXZ(o, cp) > r2)
					continue;
				CharacterControllerComponent ctrl = ch.GetCharacterController();
				if (ctrl && ctrl.GetStance() == ECharacterStance.PRONE)
				{
					vector cm[4];
					ch.GetWorldTransform(cm);
					vector fwd = cm[2];
					fwd[1] = 0;
					fwd = fwd.Normalized();
					Press(o - fwd * PRONE_HALF, o + fwd * PRONE_HALF, R_PRONE, time + FOOT_HOLD);
				}
				else
				{
					Press(o, o, R_FOOT, time + FOOT_HOLD);
				}
			}
		}

		if (time >= s_fNextSweep)
		{
			s_fNextSweep = time + SWEEP;
			Sweep(time);
		}
		// GROW: a few raised skin vertices re-build their skin chunks (only where a skin cell there still draws)
		if (s_aGrowSkin)
		{
			for (int gs = 0; gs < GROW_SKIN_PER && !s_aGrowSkin.IsEmpty(); gs++)
			{
				int gk = s_aGrowSkin[s_aGrowSkin.Count() - 1];
				s_aGrowSkin.Remove(s_aGrowSkin.Count() - 1);
				s_sGrowSkin.RemoveItem(gk);
				int gsz = gk / SKEY;
				s_iGrowSkin += MUD_CoarseSkinEntity.GrowRedraw(gk - gsz * SKEY, gsz);
			}
		}
		s_iMs += System.GetTickCount(t0);

		if (MUD_TestKit.TEST_KIT && time >= s_fNextLog)
		{
			s_fNextLog = time + 5;
			int growN;
			if (s_mGrow)
				growN = s_mGrow.Count();
			int growWait;
			if (s_aGrowSkin)
				growWait = s_aGrowSkin.Count();
			Print(string.Format("MUD look lift: base %1 m, grown skin vertices %2 (raised %3, skin chunks re-built %4, waiting %5) | GROW %6 per pass, max %7",
				LiftM(), growN, s_iGrowUps, s_iGrowSkin, growWait, GROW_PASS, GROW_MAX), LogLevel.NORMAL);
			s_iGrowUps = 0;
			s_iGrowSkin = 0;
			Print(string.Format("MUD look lift: lift %1 m, pressed vertices %2, presses %3, re-draw calls %4, missing patch %5, expired %6, body hits %7, script %8 ms (5 s)",
				LiftM(), s_mUntil.Count(), s_iPresses, s_iDirtyCalls, s_iMissing, s_iExpired, s_iBodyHits, s_iMs), LogLevel.NORMAL);
			s_iBodyHits = 0;
			s_iPresses = 0;
			s_iDirtyCalls = 0;
			s_iMissing = 0;
			s_iExpired = 0;
			s_iMs = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Press every fine vertex within r of segment a-b (XZ) until 'until'; new ones re-draw their patches
	protected static void Press(vector a, vector b, float r, float until)
	{
		int gx0 = Math.Floor((Math.Min(a[0], b[0]) - r - s_fX0) / s_fFine);
		int gx1 = Math.Ceil((Math.Max(a[0], b[0]) + r - s_fX0) / s_fFine);
		int gz0 = Math.Floor((Math.Min(a[2], b[2]) - r - s_fZ0) / s_fFine);
		int gz1 = Math.Ceil((Math.Max(a[2], b[2]) + r - s_fZ0) / s_fFine);
		if (gx0 < 0 || gz0 < 0 || gx1 >= KEYW || gz1 >= KEYW)
			return;
		if (gx1 - gx0 > BOX_MAX || gz1 - gz0 > BOX_MAX)
			return;
		s_iPresses++;
		float abx = b[0] - a[0];
		float abz = b[2] - a[2];
		float ab2 = abx * abx + abz * abz;
		float rr = r * r;
		s_aNew.Clear();
		for (int gz = gz0; gz <= gz1; gz++)
		{
			float wz = s_fZ0 + gz * s_fFine;
			for (int gx = gx0; gx <= gx1; gx++)
			{
				float wx = s_fX0 + gx * s_fFine;
				float t = 0;
				if (ab2 > 0.000001)
					t = Math.Clamp(((wx - a[0]) * abx + (wz - a[2]) * abz) / ab2, 0, 1);
				float dx = wx - a[0] - abx * t;
				float dz = wz - a[2] - abz * t;
				if (dx * dx + dz * dz > rr)
					continue;
				int key = gx * KEYW + gz;
				float old;
				if (s_mUntil.Find(key, old))
				{
					if (until > old)
						s_mUntil.Set(key, until);
					continue;
				}
				s_mUntil.Insert(key, until);
				s_aNew.Insert(key);
			}
		}
		if (s_aNew.IsEmpty())
			return;
		s_iDirtyCalls++;
		int got = MUD_WorldGroundEntity.LookDirty(gx0 - 1, gx1 + 1, gz0 - 1, gz1 + 1);
		if (got > 0)
		{
			int pad = PadAt(gz0);
			if (pad > 1)
				MUD_WorldGroundEntity.LookDirty(gx0 - pad, gx1 + pad, gz0 - pad, gz1 + pad);     // LOOK SLOPE reach
			return;
		}
		// no patch there (the skin carries it, nothing to draw in): forget the new keys, the next pass tries again
		s_iMissing++;
		foreach (int nk : s_aNew)
			s_mUntil.Remove(nk);
	}

	//------------------------------------------------------------------------------------------------
	//! BODY PRESS (user 2026-10-23: 'the front doesn't push anything, only the wheels'): the lift is look only, so the
	//! body went through it. Samples every BODY_STEP m over the vehicle's box (local XZ); at each a short trace UP from
	//! our ground (collider) to the drawn lift top that sees only this vehicle - a hit = the body hangs into the look
	//! there -> press R_BODY round it (held as a wheel press). A parked vehicle re-traces only when its hold runs low.
	//! INFERENCE: the hull collider is on a layer the Projectile preset hits (vehicles take bullets) - check 'body hits'.
	protected static void BodyPress(Vehicle veh, float time)
	{
		vector at = veh.GetOrigin();
		vector was;
		if (s_mBodyAt.Find(veh, was) && vector.DistanceXZ(was, at) < 0.05 && time - was[1] < WHEEL_HOLD - 0.5)
			return;
		s_mBodyAt.Set(veh, Vector(at[0], time, at[2]));
		float hold = time + WHEEL_HOLD;
		if (s_iMode == MODE_LIFT_ONLY)
			hold = HOLD_FOREVER;
		float lift = BaseAtZ(at[2]);     // SNOW: the zone's lift where the vehicle is
		vector mins;
		vector maxs;
		veh.GetBounds(mins, maxs);
		float sx = maxs[0] - mins[0];
		float sz = maxs[2] - mins[2];
		float step = Math.Max(BODY_STEP, Math.Sqrt(sx * sz / BODY_MAX));
		int nx = Math.Floor(sx / step) + 1;
		int nz = Math.Floor(sz / step) + 1;
		float ox = mins[0] + (sx - (nx - 1) * step) * 0.5;     // centred on the box
		float oz = mins[2] + (sz - (nz - 1) * step) * 0.5;
		BaseWorld world = veh.GetWorld();
		s_BodyTrace.Include = veh;
		s_BodyTrace.Flags = TraceFlags.ENTS;
		s_BodyTrace.LayerMask = EPhysicsLayerPresets.Projectile;
		for (int iz = 0; iz < nz; iz++)
		{
			for (int ix = 0; ix < nx; ix++)
			{
				vector wp = veh.CoordToParent(Vector(ox + ix * step, mins[1], oz + iz * step));
				float dug;
				float undug;
				if (!MUD_WorldGroundEntity.DugUndug(wp[0], wp[2], dug, undug))
					continue;
				wp[1] = dug;
				float top = lift + GrowAt(wp[0], wp[2]);
				s_BodyTrace.Start = wp + Vector(0, 0.05, 0);
				s_BodyTrace.End = wp + Vector(0, top, 0);
				float hit = world.TraceMove(s_BodyTrace, null);
				if (hit >= 1)
					continue;
				// BODY CAP: the underside sits 0.05 + hit * (top - 0.05) over our ground; the lift stays just under it
				CapPress(wp, R_BODY, hold, Math.Max(0, 0.05 + hit * (top - 0.05) - BODY_GAP));
				s_iBodyHits++;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! BODY CAP: cap the lift of every fine vertex within r of a (XZ) at cap m until 'until'; a new or lower cap
	//! re-draws its patches (as Press)
	protected static void CapPress(vector a, float r, float until, float cap)
	{
		int gx0 = Math.Floor((a[0] - r - s_fX0) / s_fFine);
		int gx1 = Math.Ceil((a[0] + r - s_fX0) / s_fFine);
		int gz0 = Math.Floor((a[2] - r - s_fZ0) / s_fFine);
		int gz1 = Math.Ceil((a[2] + r - s_fZ0) / s_fFine);
		if (gx0 < 0 || gz0 < 0 || gx1 >= KEYW || gz1 >= KEYW)
			return;
		s_iPresses++;
		float rr = r * r;
		s_aNew.Clear();
		for (int gz = gz0; gz <= gz1; gz++)
		{
			float dz = s_fZ0 + gz * s_fFine - a[2];
			for (int gx = gx0; gx <= gx1; gx++)
			{
				float dx = s_fX0 + gx * s_fFine - a[0];
				if (dx * dx + dz * dz > rr)
					continue;
				int key = gx * KEYW + gz;
				float old;
				if (s_mCap.Find(key, old) && old <= cap + 0.01)
				{
					// same or lower cap already there: only the hold grows
					if (until > s_mCapUntil.Get(key))
						s_mCapUntil.Set(key, until);
					continue;
				}
				s_mCap.Set(key, cap);
				s_mCapUntil.Set(key, until);
				s_aNew.Insert(key);
			}
		}
		if (s_aNew.IsEmpty())
			return;
		s_iDirtyCalls++;
		int got = MUD_WorldGroundEntity.LookDirty(gx0 - 1, gx1 + 1, gz0 - 1, gz1 + 1);
		if (got > 0)
		{
			int pad = PadAt(gz0);
			if (pad > 1)
				MUD_WorldGroundEntity.LookDirty(gx0 - pad, gx1 + pad, gz0 - pad, gz1 + pad);     // LOOK SLOPE reach
			return;
		}
		s_iMissing++;     // no patch there: forget, the next pass tries again
		foreach (int nk : s_aNew)
		{
			s_mCap.Remove(nk);
			s_mCapUntil.Remove(nk);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Drop expired presses; where no wear holds the vertex down, its patch re-draws with the lift back
	protected static void Sweep(float time)
	{
		s_aGone.Clear();
		foreach (int ck, float cUntil : s_mCapUntil)
		{
			if (cUntil < time)
				s_aGone.Insert(ck);
		}
		foreach (int cg : s_aGone)
		{
			s_mCap.Remove(cg);
			s_mCapUntil.Remove(cg);
			s_iExpired++;
			int cx = cg / KEYW;
			int cz = cg - cx * KEYW;
			s_iDirtyCalls++;
			int cpad = PadAt(cz);
			MUD_WorldGroundEntity.LookDirty(cx - cpad, cx + cpad, cz - cpad, cz + cpad);
		}
		s_aGone.Clear();
		foreach (int key, float until : s_mUntil)
		{
			if (until < time)
				s_aGone.Insert(key);
		}
		foreach (int gk : s_aGone)
		{
			s_mUntil.Remove(gk);
			s_iExpired++;
			int gx = gk / KEYW;
			int gz = gk - gx * KEYW;
			if (MUD_VoxelProbe.WearAt(s_fX0 + gx * s_fFine, s_fZ0 + gz * s_fFine) > WEAR_MIN)
				continue;     // the synced wear keeps it pressed: nothing changes on screen
			s_iDirtyCalls++;
			int gpad = PadAt(gz);
			MUD_WorldGroundEntity.LookDirty(gx - gpad, gx + gpad, gz - gpad, gz + gpad);
		}
	}
}
