//! BONE TILE FIELD (TEST_KIT, 2026-10-23, user: 'cover the whole map, very thick, tilted, let me do a driving test').
//! Plan: docs/superpowers/plans/2026-10-23-bone-tile-mud.md (after step 3). Checkpoint v305 = before.
//! The whole map is too big for tiles everywhere (Arland = millions of 4 m tiles), so a FIELD of N x N prefab tiles
//! follows the camera on the 4 m world grid: a tile that falls out behind is moved (SetOrigin + all bones re-set) to
//! the new row in front, nearest first. Driving anywhere = mud everywhere around you.
//! Look only: local entities, no collider, never a DS. Feel stays the rut field.
//! THICK mud: on our ground (holed cells) the tile sits THICK over the ground as the patches draw it ('dug' =
//! heightfield + rut field), so wheels and feet sink into it by eye. Wheels / feet press it down to the ground (dip,
//! kept for the session in a world map, so the track stays when you drive away and come back) and push a low berm up
//! beside the track. Kept terrain (roads, asphalt, buildings) gets no thickness: the tile lies flat on it.
//! TILT (default): bones turn to the slope of the mud top, so the dents get real shading (user-OK 2026-10-23).
//! MAP mode (user 2026-10-23: 'no pop-in, cover the actual full map, only offroad - try it'):
//! tiles never move. They spawn ring by ring outward from where MAP was switched on (MAP_SPAWN_MS of spawning per
//! frame) over every 4 m cell with any offroad (holed) ground, until a whole ring is off the patch grid (= map done)
//! or the frame rate falls under MAP_MIN_FPS (safety stop, so WB does not lock up; NOT a perf number).
//! MAP AT LOAD (user 2026-10-23: 'have it built as I load into the map, as we do with our terrain'): test kit
//! bone_map 1 starts MAP by itself once the ground is ready, from the MAP CENTRE outward, with MAP_LOAD_MS per frame
//! while you have no character yet (spawn screen), then MAP_SPAWN_MS. The skin builds in one freeze; this cannot
//! (millions of tiles), so it builds over frames from the moment the world ticks.
//! EDGE FADE (FIELD only, user 2026-10-23 'try 1' - the MAP build is ~1 h for the whole map): the mud thickness
//! fades to 0 towards the field edge (square distance from the field's centre tile, FADE_IN .. FADE_OUT m), so a tile
//! that moves in at the edge lies flat on the ground (= looks like the ground already there) and only thickens as you
//! come near. When the centre tile changes (every 4 m driven) the tiles in the fade band are re-set, FADE_PER_FRAME
//! tiles per frame (each step changes a point by at most THICK * 4 / band).
//! Keys: through MUD_BoneProbe (numpad 1 cycle, 2 look mode, 4 press on / off).
//! Log: 'MUD bone field: ...' (start, every 5 s: tiles, moves, stamps, sets, clamps, press points, script ms - WB not perf).
class MUD_BoneFieldTile
{
	IEntity m_Ent;
	int m_iTx = -999999;                        // tile coords on the 4 m world grid
	int m_iTz = -999999;
	float m_fBaseY;
	ref array<float> m_aLastH = {};
	ref array<float> m_aLastSx = {};
	ref array<float> m_aLastSz = {};
	bool m_bDirty;                              // in s_aDirty
	bool m_bQueued;                             // in s_aRefresh
	bool m_bFadeQ;                              // in s_aFadeQ
	int m_iBx0;                                 // dirty box (global grid points)
	int m_iBz0;
	int m_iBx1;
	int m_iBz1;
}

class MUD_BoneField
{
	static const float SIZE = 4;            // tile side, m (Blender build)
	static const int HALF = 8;              // field = (2 HALF + 1)^2 tiles = 17 x 17 = 68 x 68 m (cost run: 300 tiles fine)
	static const float THICK = 0.25;        // mud thickness over our ground, m
	static const float LIFT = 0.02;         // over the patch mesh (must not poke through)
	static const float BERM = 0.08;         // berm beside a wheel track, m
	static const float BOX_Y = 1.45;        // bone offset clamp (prefab BoundingBox +-1.5 m)
	static const float EPS_H = 0.001;
	static const float R_WHEEL = 0.4;       // dent half width (raised to 1.3 bone steps on the coarse tile)
	static const float R_FOOT = 0.3;
	static const int MOVES_MAX = 4;         // tiles moved / spawned per frame
	static const int REFRESH_MAX = 2;       // tiles re-read from the ground per frame (near vehicles: rut field)
	static const float REFRESH_S = 0.25;    // queue the tiles around each vehicle this often (rut ops every 0.25 s)
	static const int RING = 256;            // ground cache = RING x RING points, indexed by world point mod RING
	static const int KEYW = 46000;          // point key = gx * KEYW + gz (0.29 m step: 13 km fits in an int)
	static const float MAP_SPAWN_MS = 6;    // MAP: spawn tiles for this long per frame
	static const float MAP_MIN_FPS = 15;    // MAP: stop spawning under this (2 s average, after MAP_GUARD_FROM tiles)
	static const float MAP_LOAD_MS = 40;    // MAP: spawn budget per frame while there is no local character yet
	static const float MAP_LOAD_MIN_FPS = 5;    // MAP: safety stop while loading (the big budget alone gives ~20 fps)
	static const int MAP_GUARD_FROM = 2000;
	static const int MAP_NEAR = 15;         // MAP: a look-mode switch re-sets tiles within this many tiles only
	static const float FADE_OUT = 33;       // FIELD: thickness 0 at this square distance from the centre tile (edge 34 m)
	static const float FADE_IN = 13;        // FIELD: full thickness inside this
	static const int FADE_PER_FRAME = 16;   // FIELD: fade-band tiles re-set per frame after a centre change
	static const bool FADE_ON = false;      // user 2026-10-23 drive test: fade + at-load 'bad' -> off, plain camera field
	static const int MODE_FLAT = 0;
	static const int MODE_TILT = 1;
	static const int MODE_HIDDEN = 2;

	protected static bool s_bOn;
	protected static bool s_bMap;                               // MAP mode: tiles never move, cover the map
	protected static ref map<int, MUD_BoneFieldTile> s_mTiles;  // MAP: tile by tx * 100000 + tz
	protected static int s_iRing;                               // MAP spawn spiral: ring, index in ring, start tile
	protected static int s_iRingK;
	protected static int s_iCtx0;
	protected static int s_iCtz0;
	protected static bool s_bRingOnGrid;
	protected static bool s_bMapDone;
	protected static bool s_bMapStopped;
	protected static int s_iSkipped;
	protected static float s_fLastT = -1;
	protected static float s_fWinT;
	protected static int s_iWinFrames;
	protected static float s_fFps;
	protected static bool s_bNoGuard;                           // BUILD: MAP without the safety stop
	protected static bool s_bAutoDone;                          // field / MAP at load started this play
	protected static bool s_bFade;                              // FIELD: edge fade on
	protected static int s_iFx = -999999;                       // FIELD: fade centre tile
	protected static int s_iFz = -999999;
	protected static float s_fFcx;                              // FIELD: fade centre (world)
	protected static float s_fFcz;
	protected static ref array<MUD_BoneFieldTile> s_aFadeQ;
	protected static int s_iSide;           // bones per side
	protected static int s_iS1;             // steps per tile side (side - 1)
	protected static float s_fStep;
	protected static int s_iN;              // tiles per field side
	protected static int s_iMode = MODE_TILT;
	protected static bool s_bPress = true;
	protected static ResourceName s_sPrefab;
	protected static ref array<ref MUD_BoneFieldTile> s_aTiles;  // slot = (tz mod N) * N + (tx mod N)
	protected static ref array<int> s_aOrderDx;                 // field offsets, nearest first
	protected static ref array<int> s_aOrderDz;
	protected static ref array<int> s_aId;                      // per bone (shared: same prefab, same rest pose)
	protected static ref array<int> s_aIx;                      // grid step inside the tile, 0 .. S1
	protected static ref array<int> s_aIz;
	protected static ref array<vector> s_aAx0;
	protected static ref array<vector> s_aAx1;
	protected static ref array<vector> s_aAx2;
	protected static ref array<int> s_aRingKey;
	protected static ref array<float> s_aRingY;
	protected static ref array<float> s_aRingT;
	protected static ref map<int, float> s_mDip;                // press, kept for the session (per tile size)
	protected static ref map<int, float> s_mBerm;
	protected static ref array<MUD_BoneFieldTile> s_aDirty;
	protected static ref array<MUD_BoneFieldTile> s_aRefresh;
	protected static float s_fNextQueue;
	protected static float s_fNextStat;
	protected static int s_iMoves;
	protected static int s_iStamps;
	protected static int s_iSets;
	protected static int s_iClamps;
	protected static int s_iSetFails;
	protected static int s_iReads;
	protected static int s_iMs;
	protected static int s_iFrames;

	//------------------------------------------------------------------------------------------------
	static bool IsOn()
	{
		return s_bOn;
	}

	//------------------------------------------------------------------------------------------------
	//! side 15 or 9. Builds nothing yet: Tick spawns the tiles, nearest first. whole = MAP mode.
	static void Start(int side, int mode, bool press, bool whole, bool fromCentre = false)
	{
		Stop();
		if (RplSession.Mode() == RplMode.Dedicated)
			return;
		if (side != s_iSide)
		{
			s_mDip = null;      // press points are on the old tile size's grid
			s_mBerm = null;
		}
		if (!s_mDip)
		{
			s_mDip = new map<int, float>();
			s_mBerm = new map<int, float>();
		}
		s_iSide = side;
		s_iS1 = side - 1;
		float s1f = s_iS1;
		s_fStep = SIZE / s1f;
		s_iMode = mode;
		s_bPress = press;
		s_sPrefab = "{4D55444250460009}Prefabs/Mud/MudBoneTile9.et";
		if (side == 15)
			s_sPrefab = "{4D55444250460015}Prefabs/Mud/MudBoneTile15.et";
		s_iN = 2 * HALF + 1;
		s_aTiles = {};
		s_bMap = whole;
		s_bNoGuard = false;
		s_bFade = !whole && FADE_ON;
		s_iFx = -999999;
		s_iFz = -999999;
		s_aFadeQ = {};
		s_mTiles = null;
		if (whole)
		{
			s_mTiles = new map<int, MUD_BoneFieldTile>();
			vector cam[4];
			GetGame().GetWorld().GetCurrentCamera(cam);
			vector c0 = cam[3];
			MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
			if (fromCentre && skin)
			{
				float gx0;
				float gz0;
				int ncx;
				int ncz;
				skin.GetGrid(gx0, gz0, ncx, ncz);
				float half = skin.GetCell() * 0.5;
				c0 = Vector(gx0 + ncx * half, 0, gz0 + ncz * half);
			}
			s_iCtx0 = Math.Floor(c0[0] / SIZE);
			s_iCtz0 = Math.Floor(c0[2] / SIZE);
			s_iRing = 0;
			s_iRingK = 0;
			s_bRingOnGrid = false;
			s_bMapDone = false;
			s_bMapStopped = false;
			s_iSkipped = 0;
		}
		else
		{
			for (int i = 0; i < s_iN * s_iN; i++)
			{
				s_aTiles.Insert(new MUD_BoneFieldTile());
			}
		}
		s_fLastT = -1;
		s_fWinT = 0;
		s_iWinFrames = 0;
		s_aOrderDx = {};
		s_aOrderDz = {};
		for (int r = 0; r <= HALF; r++)
		{
			for (int dz = -r; dz <= r; dz++)
			{
				for (int dx = -r; dx <= r; dx++)
				{
					if (Math.AbsInt(dx) == r || Math.AbsInt(dz) == r)
					{
						s_aOrderDx.Insert(dx);
						s_aOrderDz.Insert(dz);
					}
				}
			}
		}
		s_aRingKey = {};
		s_aRingY = {};
		s_aRingT = {};
		s_aRingKey.Resize(RING * RING);
		s_aRingY.Resize(RING * RING);
		s_aRingT.Resize(RING * RING);
		for (int k = 0; k < RING * RING; k++)
		{
			s_aRingKey[k] = -1;
		}
		s_aDirty = {};
		s_aRefresh = {};
		s_aId = null;
		s_bOn = true;
		if (whole)
			Print(string.Format("MUD bone field: MAP ON | tiles of %1 x %1 bones spawn outward from tile %2 %3 over offroad ground, %4 ms per frame, safety stop under %5 fps",
				side, s_iCtx0, s_iCtz0, MAP_SPAWN_MS, MAP_MIN_FPS), LogLevel.NORMAL);
		else
			Print(string.Format("MUD bone field: ON | %1 x %1 tiles of %2 x %2 bones (step %3 m), %4 m square around the camera, thick %5 m, berm %6 m, mode %7, press %8, press points kept %9",
			s_iN, side, s_fStep, s_iN * SIZE, THICK, BERM, s_iMode, s_bPress, s_mDip.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! BUILD: MAP from the camera outward with no safety stop - the user stops it (StopBuilding)
	static void StartBuild(int mode, bool press)
	{
		Start(9, mode, press, true);
		s_bNoGuard = true;
		Print("MUD bone field: BUILD - MAP with no safety stop; numpad 1 stops spawning and keeps the tiles", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! BUILD: stop spawning, keep every tile (the 5 s lines go on: tiles + fps with nothing building)
	static void StopBuilding()
	{
		if (!s_bOn || !s_bMap)
			return;
		s_bMapStopped = true;
		Print(string.Format("MUD bone field: BUILD STOPPED by key at %1 tiles (ring %2 = %3 m out), last fps while building %4 (WB, not perf)",
			s_aTiles.Count(), s_iRing, s_iRing * SIZE, s_fFps), LogLevel.NORMAL);
		SCR_HintManagerComponent.ShowCustomHint(string.Format("BUILD stopped: %1 tiles kept, %2 m out", s_aTiles.Count(), s_iRing * SIZE), "Mud bone field", 8, true);
	}

	//------------------------------------------------------------------------------------------------
	static void Stop()
	{
		if (s_aTiles)
		{
			foreach (MUD_BoneFieldTile tile : s_aTiles)
			{
				if (tile.m_Ent && !tile.m_Ent.IsDeleted())
					SCR_EntityHelper.DeleteEntityAndChildren(tile.m_Ent);
			}
		}
		s_aTiles = null;
		s_mTiles = null;
		s_aDirty = null;
		s_aRefresh = null;
		s_bOn = false;
	}

	//------------------------------------------------------------------------------------------------
	//! World restart (play stop / start): entities are gone, forget them
	static void Reset()
	{
		s_bAutoDone = false;
		s_aTiles = null;
		s_mTiles = null;
		s_bOn = false;
		s_mDip = null;
		s_mBerm = null;
	}

	//------------------------------------------------------------------------------------------------
	static void SetMode(int mode)
	{
		s_iMode = mode;
		if (!s_bOn)
			return;
		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		vector cp = cam[3];
		int ctx = Math.Floor(cp[0] / SIZE);
		int ctz = Math.Floor(cp[2] / SIZE);
		foreach (MUD_BoneFieldTile tile : s_aTiles)
		{
			if (!tile.m_Ent)
				continue;
			if (mode == MODE_HIDDEN)
				tile.m_Ent.ClearFlags(EntityFlags.VISIBLE, false);
			else
				tile.m_Ent.SetFlags(EntityFlags.VISIBLE, false);
			// MAP: re-set only the tiles near you (all of them would freeze the frame); far ones keep the old look
			if (!s_bMap || (Math.AbsInt(tile.m_iTx - ctx) <= MAP_NEAR && Math.AbsInt(tile.m_iTz - ctz) <= MAP_NEAR))
				MarkAll(tile);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void SetPress(bool on)
	{
		s_bPress = on;
		if (!on && s_mDip)
		{
			s_mDip.Clear();
			s_mBerm.Clear();
		}
		SetMode(s_iMode);   // re-set every bone
	}

	//------------------------------------------------------------------------------------------------
	static void Tick(float time)
	{
		if (!s_bAutoDone)
		{
			s_bAutoDone = true;
			if (MUD_TestKit.TEST_KIT && MUD_TestKit.BoneMap() > 0 && RplSession.Mode() != RplMode.Dedicated)
			{
				if (MUD_TestKit.BoneMap() == 2)
					Start(9, s_iMode, s_bPress, true, true);    // whole-map MAP test (slow: ~1 h for Arland in WB)
				else
					Start(9, s_iMode, s_bPress, false);         // field around you with the edge fade
			}
		}
		if (!s_bOn || !s_aTiles)
			return;
		int t0 = System.GetTickCount();
		s_iFrames++;
		if (s_fLastT >= 0 && time > s_fLastT)
		{
			s_fWinT += time - s_fLastT;
			s_iWinFrames++;
		}
		s_fLastT = time;
		if (s_fWinT >= 2)
		{
			s_fFps = s_iWinFrames / s_fWinT;
			s_fWinT = 0;
			s_iWinFrames = 0;
			float minFps = MAP_MIN_FPS;
			if (!SCR_PlayerController.GetLocalControlledEntity())
				minFps = MAP_LOAD_MIN_FPS;
			if (s_bMap && !s_bNoGuard && !s_bMapDone && !s_bMapStopped && s_aTiles.Count() >= MAP_GUARD_FROM && s_fFps < minFps)
			{
				s_bMapStopped = true;
				Print(string.Format("MUD bone field: MAP SAFETY STOP at %1 tiles (ring %2 = %3 m out), fps %4 < %5 (WB, not perf)",
					s_aTiles.Count(), s_iRing, s_iRing * SIZE, s_fFps, minFps), LogLevel.WARNING);
				SCR_HintManagerComponent.ShowCustomHint(string.Format("MAP spawning stopped at %1 tiles (%2 m out): fps %3", s_aTiles.Count(), s_iRing * SIZE, s_fFps), "Mud bone field", 8, true);
			}
		}
		BaseWorld world = GetGame().GetWorld();
		vector cam[4];
		world.GetCurrentCamera(cam);
		vector cp = cam[3];
		int ctx = Math.Floor(cp[0] / SIZE);
		int ctz = Math.Floor(cp[2] / SIZE);

		// 0. FIELD fade: new centre tile -> re-set the fade band (inner tiles never change)
		if (s_bFade && (ctx != s_iFx || ctz != s_iFz))
		{
			s_iFx = ctx;
			s_iFz = ctz;
			float fcx = ctx;
			float fcz = ctz;
			s_fFcx = fcx * SIZE + SIZE * 0.5;
			s_fFcz = fcz * SIZE + SIZE * 0.5;
			int inner = Math.Floor(FADE_IN / SIZE) - 1;
			foreach (MUD_BoneFieldTile ft : s_aTiles)
			{
				if (ft.m_Ent && !ft.m_bFadeQ && (Math.AbsInt(ft.m_iTx - ctx) > inner || Math.AbsInt(ft.m_iTz - ctz) > inner))
				{
					ft.m_bFadeQ = true;
					s_aFadeQ.Insert(ft);
				}
			}
		}

		// 1. MAP: spawn outward (tiles never move); FIELD: move / spawn tiles, nearest first
		if (s_bMap)
		{
			MapSpawn(world, t0);
			if (!s_bOn)
				return;
		}
		int moves;
		for (int o = 0; o < s_aOrderDx.Count() && moves < MOVES_MAX && !s_bMap; o++)
		{
			int tx = ctx + s_aOrderDx[o];
			int tz = ctz + s_aOrderDz[o];
			MUD_BoneFieldTile tile = s_aTiles[SlotOf(tx, tz)];
			if (tile.m_iTx == tx && tile.m_iTz == tz)
				continue;
			moves++;
			MoveTile(tile, tx, tz, world);
			if (!s_bOn)
				return;     // spawn failed: field off
		}
		if (s_iMode == MODE_HIDDEN)
		{
			s_iMs += System.GetTickCount() - t0;
			Stat(time);
			return;
		}

		// 2. press under wheels / feet
		if (s_bPress)
			PressTick(cp);

		// 3. re-read the ground around vehicles (the real rut field moves under the mud)
		if (time >= s_fNextQueue)
		{
			s_fNextQueue = time + REFRESH_S;
			QueueNearVehicles(cp);
		}
		for (int q = 0; q < REFRESH_MAX && !s_aRefresh.IsEmpty(); q++)
		{
			MUD_BoneFieldTile rt = s_aRefresh[0];
			s_aRefresh.RemoveOrdered(0);
			rt.m_bQueued = false;
			if (!rt.m_Ent)
				continue;
			Forget(rt);
			MarkAll(rt);
		}

		for (int fq = 0; fq < FADE_PER_FRAME && !s_aFadeQ.IsEmpty(); fq++)
		{
			MUD_BoneFieldTile qt = s_aFadeQ[0];
			s_aFadeQ.RemoveOrdered(0);
			qt.m_bFadeQ = false;
			MarkAll(qt);
		}

		// 4. set the changed bones
		foreach (MUD_BoneFieldTile dt : s_aDirty)
		{
			Apply(dt);
		}
		s_aDirty.Clear();
		s_iMs += System.GetTickCount() - t0;
		Stat(time);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stat(float time)
	{
		if (time < s_fNextStat)
			return;
		s_fNextStat = time + 5;
		int live;
		foreach (MUD_BoneFieldTile tile : s_aTiles)
		{
			if (tile.m_Ent)
				live++;
		}
		if (s_bMap)
			Print(string.Format("MUD bone field: MAP 5 s | tiles %1 (offroad cells skipped %2), ring %3 = %4 m out, done %5, safety stop %6, fps %7 (WB, not perf)",
				live, s_iSkipped, s_iRing, s_iRing * SIZE, s_bMapDone, s_bMapStopped, s_fFps), LogLevel.NORMAL);
		Print(string.Format("MUD bone field: 5 s | tiles %1, moved %2, stamps %3, bone sets %4, clamps %5, set fails %6, ground reads %7, press points %8, %9 ms (WB, not perf)",
			live, s_iMoves, s_iStamps, s_iSets, s_iClamps, s_iSetFails, s_iReads, s_mDip.Count() + s_mBerm.Count(), s_iMs), LogLevel.NORMAL);
		s_iMoves = 0;
		s_iStamps = 0;
		s_iSets = 0;
		s_iClamps = 0;
		s_iReads = 0;
		s_iMs = 0;
		s_iFrames = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static int SlotOf(int tx, int tz)
	{
		int sx = ((tx % s_iN) + s_iN) % s_iN;
		int sz = ((tz % s_iN) + s_iN) % s_iN;
		return sz * s_iN + sx;
	}

	//------------------------------------------------------------------------------------------------
	protected static void MoveTile(MUD_BoneFieldTile tile, int tx, int tz, BaseWorld world)
	{
		s_iMoves++;
		tile.m_iTx = tx;
		tile.m_iTz = tz;
		float ftx = tx;
		float ftz = tz;
		float cx = ftx * SIZE + SIZE * 0.5;
		float cz = ftz * SIZE + SIZE * 0.5;
		float y;
		float th;
		Point(tx * s_iS1 + s_iS1 / 2, tz * s_iS1 + s_iS1 / 2, y, th);
		tile.m_fBaseY = y;
		vector org = Vector(cx, y, cz);
		if (!tile.m_Ent)
		{
			Resource res = Resource.Load(s_sPrefab);
			if (!res || !res.IsValid())
			{
				Print(string.Format("MUD bone field: prefab '%1' did not load - field off", s_sPrefab), LogLevel.ERROR);
				Stop();
				return;
			}
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[0] = vector.Right;
			params.Transform[1] = vector.Up;
			params.Transform[2] = vector.Forward;
			params.Transform[3] = org;
			tile.m_Ent = GetGame().SpawnEntityPrefabLocal(res, world, params);
			if (!tile.m_Ent || !tile.m_Ent.GetAnimation())
			{
				Print("MUD bone field: tile spawn failed or no bones - field off", LogLevel.ERROR);
				Stop();
				return;
			}
			if (!s_aId)
				ReadRest(tile.m_Ent.GetAnimation());
			int nb = s_aId.Count();
			tile.m_aLastH.Resize(nb);
			tile.m_aLastSx.Resize(nb);
			tile.m_aLastSz.Resize(nb);
			if (s_iMode == MODE_HIDDEN)
				tile.m_Ent.ClearFlags(EntityFlags.VISIBLE, false);
		}
		else
		{
			tile.m_Ent.SetOrigin(org);
		}
		for (int b = 0; b < tile.m_aLastH.Count(); b++)
		{
			tile.m_aLastH[b] = -999;
		}
		MarkAll(tile);
		if (s_iMode == MODE_HIDDEN)
			tile.m_Ent.Update();
	}

	//------------------------------------------------------------------------------------------------
	//! Bone ids, rest axes and grid steps from the first tile (all tiles: same prefab, same rest pose)
	protected static void ReadRest(Animation anim)
	{
		s_aId = {};
		s_aIx = {};
		s_aIz = {};
		s_aAx0 = {};
		s_aAx1 = {};
		s_aAx2 = {};
		string abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		int misfit;
		for (int j = 0; j < s_iSide; j++)
		{
			for (int i = 0; i < s_iSide; i++)
			{
				int id = anim.GetBoneIndex("g" + abc.Get(i) + abc.Get(j));
				vector m[4];
				if (id < 0 || !anim.GetBoneMatrix(id, m))
				{
					misfit++;
					continue;
				}
				vector p = m[3];
				float fx = (p[0] + SIZE * 0.5) / s_fStep;
				float fz = (p[2] + SIZE * 0.5) / s_fStep;
				int ix = Math.Round(fx);
				int iz = Math.Round(fz);
				if (Math.AbsFloat(fx - ix) > 0.02 || Math.AbsFloat(fz - iz) > 0.02 || ix < 0 || iz < 0 || ix > s_iS1 || iz > s_iS1)
				{
					misfit++;
					continue;
				}
				s_aId.Insert(id);
				s_aIx.Insert(ix);
				s_aIz.Insert(iz);
				s_aAx0.Insert(m[0]);
				s_aAx1.Insert(m[1]);
				s_aAx2.Insert(m[2]);
			}
		}
		Print(string.Format("MUD bone field: rest pose read | bones on the grid %1 of %2, missing / off grid %3", s_aId.Count(), s_iSide * s_iSide, misfit), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Ground (as the patches draw it) and mud thickness at global grid point gx, gz, cached in the ring
	protected static void Point(int gx, int gz, out float y, out float thick)
	{
		int slot = (gz & (RING - 1)) * RING + (gx & (RING - 1));
		int key = gx * KEYW + gz;
		if (s_aRingKey[slot] != key)
		{
			s_iReads++;
			float fx = gx;
			float fz = gz;
			float x = fx * s_fStep;
			float z = fz * s_fStep;
			BaseWorld world = GetGame().GetWorld();
			float dug;
			float undug;
			float gy;
			if (MUD_WorldGroundEntity.DugUndug(x, z, dug, undug))
				gy = dug;
			else
				gy = world.GetSurfaceY(x, z);
			float t = 0;
			if (MUD_Surface.IsHole(world, x, z))
				t = THICK;          // our ground: mud; kept terrain (roads, asphalt, buildings): none
			s_aRingKey[slot] = key;
			s_aRingY[slot] = gy;
			s_aRingT[slot] = t;
		}
		y = s_aRingY[slot];
		thick = s_aRingT[slot];
	}

	//------------------------------------------------------------------------------------------------
	//! Mud top (world y) at a grid point: ground + lift + thickness - dip + berm
	protected static float Surf(int gx, int gz)
	{
		float y;
		float t;
		Point(gx, gz, y, t);
		if (t <= 0)
			return y + LIFT;
		if (s_bFade)
		{
			// square distance from the fade centre -> smooth 1 .. 0 between FADE_IN and FADE_OUT
			float fx = gx;
			float fz = gz;
			float d = Math.Max(Math.AbsFloat(fx * s_fStep - s_fFcx), Math.AbsFloat(fz * s_fStep - s_fFcz));
			float k = Math.Clamp((FADE_OUT - d) / (FADE_OUT - FADE_IN), 0, 1);
			t = t * k * k * (3 - 2 * k);
			if (t <= 0)
				return y + LIFT;
		}
		float top = t;
		if (s_bPress)
		{
			int key = gx * KEYW + gz;
			float dip;
			if (s_mDip.Find(key, dip))
				top = Math.Max(0, t - dip);
			float berm;
			if (dip < 0.01 && s_mBerm.Find(key, berm))
				top += berm * t / THICK;
		}
		return y + LIFT + top;
	}

	//------------------------------------------------------------------------------------------------
	//! Drop a tile's cached ground points so the next Surf reads the ground again
	protected static void Forget(MUD_BoneFieldTile tile)
	{
		int gx0 = tile.m_iTx * s_iS1;
		int gz0 = tile.m_iTz * s_iS1;
		for (int gz = gz0; gz <= gz0 + s_iS1; gz++)
		{
			for (int gx = gx0; gx <= gx0 + s_iS1; gx++)
			{
				s_aRingKey[(gz & (RING - 1)) * RING + (gx & (RING - 1))] = -1;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void MarkAll(MUD_BoneFieldTile tile)
	{
		MarkBox(tile, tile.m_iTx * s_iS1, tile.m_iTz * s_iS1, (tile.m_iTx + 1) * s_iS1, (tile.m_iTz + 1) * s_iS1);
	}

	//------------------------------------------------------------------------------------------------
	protected static void MarkBox(MUD_BoneFieldTile tile, int bx0, int bz0, int bx1, int bz1)
	{
		if (!tile.m_Ent)
			return;
		if (!tile.m_bDirty)
		{
			tile.m_bDirty = true;
			tile.m_iBx0 = bx0;
			tile.m_iBz0 = bz0;
			tile.m_iBx1 = bx1;
			tile.m_iBz1 = bz1;
			s_aDirty.Insert(tile);
			return;
		}
		tile.m_iBx0 = Math.Min(tile.m_iBx0, bx0);
		tile.m_iBz0 = Math.Min(tile.m_iBz0, bz0);
		tile.m_iBx1 = Math.Max(tile.m_iBx1, bx1);
		tile.m_iBz1 = Math.Max(tile.m_iBz1, bz1);
	}

	//------------------------------------------------------------------------------------------------
	//! Set the tile's bones inside its dirty box whose offset / slope changed
	protected static void Apply(MUD_BoneFieldTile tile)
	{
		tile.m_bDirty = false;
		if (!tile.m_Ent)
			return;
		Animation anim = tile.m_Ent.GetAnimation();
		if (!anim)
			return;
		bool tilt = s_iMode == MODE_TILT;
		float step2 = 2 * s_fStep;
		int gxT = tile.m_iTx * s_iS1;
		int gzT = tile.m_iTz * s_iS1;
		bool any;
		for (int b = 0; b < s_aId.Count(); b++)
		{
			int gx = gxT + s_aIx[b];
			int gz = gzT + s_aIz[b];
			if (gx < tile.m_iBx0 || gx > tile.m_iBx1 || gz < tile.m_iBz0 || gz > tile.m_iBz1)
				continue;
			float h = Surf(gx, gz) - tile.m_fBaseY;
			if (h > BOX_Y || h < -BOX_Y)
			{
				s_iClamps++;
				h = Math.Clamp(h, -BOX_Y, BOX_Y);
			}
			float sx = 0;
			float sz = 0;
			if (tilt)
			{
				// central differences of the mud top on the world grid (same numbers on both sides of a seam)
				sx = (Surf(gx + 1, gz) - Surf(gx - 1, gz)) / step2;
				sz = (Surf(gx, gz + 1) - Surf(gx, gz - 1)) / step2;
			}
			if (Math.AbsFloat(h - tile.m_aLastH[b]) < EPS_H && Math.AbsFloat(sx - tile.m_aLastSx[b]) < EPS_H && Math.AbsFloat(sz - tile.m_aLastSz[b]) < EPS_H)
				continue;
			tile.m_aLastH[b] = h;
			tile.m_aLastSx[b] = sx;
			tile.m_aLastSz[b] = sz;
			vector a0 = s_aAx0[b];
			vector a1 = s_aAx1[b];
			vector a2 = s_aAx2[b];
			vector m[4];
			Math3D.MatrixIdentity4(m);
			if (tilt)
			{
				// as MUD_BoneProbe (proven 2026-10-23): rotation up -> mud normal, row i = ToBone(R(rest row i))
				vector rx = Vector(1, sx, 0).Normalized();
				vector rz = Vector(0, sz, 1).Normalized();
				vector ry = Cross(rz, rx).Normalized();
				rz = Cross(rx, ry);
				m[0] = ToBone(rx * a0[0] + ry * a0[1] + rz * a0[2], a0, a1, a2);
				m[1] = ToBone(rx * a1[0] + ry * a1[1] + rz * a1[2], a0, a1, a2);
				m[2] = ToBone(rx * a2[0] + ry * a2[1] + rz * a2[2], a0, a1, a2);
			}
			m[3] = ToBone(Vector(0, h, 0), a0, a1, a2);
			s_iSets++;
			any = true;
			if (!anim.SetBoneMatrix(tile.m_Ent, s_aId[b], m))
			{
				s_iSetFails++;
				if (s_iSetFails == 1 || s_iSetFails == 1000)
					Print(string.Format("MUD bone field: SetBoneMatrix false (bone %1, fails %2)", s_aId[b], s_iSetFails), LogLevel.WARNING);
			}
		}
		if (any)
			tile.m_Ent.Update();
	}

	//------------------------------------------------------------------------------------------------
	//! Stamp every wheel contact in the field and the local player's feet (every frame, so a moving wheel leaves a line)
	protected static void PressTick(vector cp)
	{
		float reach = (HALF + 1) * SIZE;
		float reach2 = reach * reach;
		float rw = Math.Max(R_WHEEL, s_fStep * 1.3);
		float rf = Math.Max(R_FOOT, s_fStep * 1.1);
		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh || vector.DistanceSqXZ(veh.GetOrigin(), cp) > reach2)
				continue;
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
			if (!sim || !sim.IsValid())
				continue;
			for (int w = 0; w < sim.WheelCount(); w++)
			{
				if (sim.WheelHasContact(w))
					Stamp(MUD_VoxelProbe.RutPoint(veh, sim, w), rw, true);
			}
		}
		ChimeraCharacter ch = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (ch && !ch.IsInVehicle())
			Stamp(ch.GetOrigin(), rf, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stamp(vector p, float r, bool berm)
	{
		float reach = r;
		if (berm)
			reach = 2 * r;
		int gx0 = Math.Ceil((p[0] - reach) / s_fStep);
		int gx1 = Math.Floor((p[0] + reach) / s_fStep);
		int gz0 = Math.Ceil((p[2] - reach) / s_fStep);
		int gz1 = Math.Floor((p[2] + reach) / s_fStep);
		if (gx0 < 1 || gz0 < 1 || gx1 >= KEYW - 1 || gz1 >= KEYW - 1)
			return;
		float y;
		float t;
		int pgx = Math.Round(p[0] / s_fStep);
		int pgz = Math.Round(p[2] / s_fStep);
		Point(pgx, pgz, y, t);
		if (Math.AbsFloat(p[1] - y) > 2)
			return;     // far above / below the ground (roof, bridge)
		s_iStamps++;
		bool changed;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			float fz = gz;
			float dz = fz * s_fStep - p[2];
			for (int gx = gx0; gx <= gx1; gx++)
			{
				float fx = gx;
				float dx = fx * s_fStep - p[0];
				float d = Math.Sqrt(dx * dx + dz * dz);
				int key = gx * KEYW + gz;
				float cur;
				if (d < r)
				{
					// flat-bottomed dent: full depth in the inner half, then a soft wall
					float want = THICK * Math.Clamp((1 - d / r) * 2, 0, 1);
					cur = 0;
					s_mDip.Find(key, cur);
					if (want > cur + EPS_H)
					{
						s_mDip.Set(key, want);
						changed = true;
					}
				}
				else if (berm && d < reach)
				{
					float k = (d - r) / r;
					float wantB = BERM * 4 * k * (1 - k);
					cur = 0;
					s_mBerm.Find(key, cur);
					if (wantB > cur + EPS_H)
					{
						s_mBerm.Set(key, wantB);
						changed = true;
					}
				}
			}
		}
		if (changed)
			MarkGrid(gx0 - 1, gz0 - 1, gx1 + 1, gz1 + 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Mark every field tile that holds a grid point in the box (a point on a tile edge is in 2 or 4 tiles)
	protected static void MarkGrid(int bx0, int bz0, int bx1, int bz1)
	{
		float s1f = s_iS1;
		float f;
		f = bx0;
		int tx0 = Math.Ceil(f / s1f) - 1;
		f = bx1;
		int tx1 = Math.Floor(f / s1f);
		f = bz0;
		int tz0 = Math.Ceil(f / s1f) - 1;
		f = bz1;
		int tz1 = Math.Floor(f / s1f);
		for (int tz = tz0; tz <= tz1; tz++)
		{
			for (int tx = tx0; tx <= tx1; tx++)
			{
				MUD_BoneFieldTile tile = TileAt(tx, tz);
				if (tile)
					MarkBox(tile, bx0, bz0, bx1, bz1);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Queue the 3 x 3 tiles around each vehicle in the field for a ground re-read (the rut field changes there)
	protected static void QueueNearVehicles(vector cp)
	{
		float reach = (HALF + 1) * SIZE;
		float reach2 = reach * reach;
		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh || vector.DistanceSqXZ(veh.GetOrigin(), cp) > reach2)
				continue;
			vector vp = veh.GetOrigin();
			int vtx = Math.Floor(vp[0] / SIZE);
			int vtz = Math.Floor(vp[2] / SIZE);
			for (int tz = vtz - 1; tz <= vtz + 1; tz++)
			{
				for (int tx = vtx - 1; tx <= vtx + 1; tx++)
				{
					MUD_BoneFieldTile tile = TileAt(tx, tz);
					if (tile && !tile.m_bQueued)
					{
						tile.m_bQueued = true;
						s_aRefresh.Insert(tile);
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The live tile at tile coords tx, tz, else null
	protected static MUD_BoneFieldTile TileAt(int tx, int tz)
	{
		if (s_bMap)
			return s_mTiles.Get(tx * 100000 + tz);
		MUD_BoneFieldTile tile = s_aTiles[SlotOf(tx, tz)];
		if (tile.m_iTx == tx && tile.m_iTz == tz)
			return tile;
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! MAP: walk the spiral of tiles around the start tile, ring by ring, for MAP_SPAWN_MS; spawn a tile on every cell
	//! with offroad (holed) ground at its centre or a corner. A whole ring off the patch grid = the map is covered.
	protected static void MapSpawn(BaseWorld world, int t0)
	{
		if (s_bMapDone || s_bMapStopped)
			return;
		float budget = MAP_SPAWN_MS;
		if (!SCR_PlayerController.GetLocalControlledEntity())
			budget = MAP_LOAD_MS;     // spawn screen: nobody is playing yet, build fast
		while (System.GetTickCount() - t0 < budget)
		{
			int dx = 0;
			int dz = 0;
			int r = s_iRing;
			if (r > 0)
			{
				int sideI = s_iRingK / (2 * r);
				int off = s_iRingK % (2 * r);
				if (sideI == 0)
				{
					dx = -r + off;
					dz = -r;
				}
				else if (sideI == 1)
				{
					dx = r;
					dz = -r + off;
				}
				else if (sideI == 2)
				{
					dx = r - off;
					dz = r;
				}
				else
				{
					dx = -r;
					dz = r - off;
				}
			}
			int tx = s_iCtx0 + dx;
			int tz = s_iCtz0 + dz;
			if (CellOffroad(world, tx, tz))
			{
				MUD_BoneFieldTile tile = new MUD_BoneFieldTile();
				s_aTiles.Insert(tile);
				s_mTiles.Set(tx * 100000 + tz, tile);
				MoveTile(tile, tx, tz, world);
				if (!s_bOn)
					return;     // spawn failed: field off
				// drape it now, inside the spawn time budget (MoveTile queued it last in s_aDirty)
				Apply(tile);
				int last = s_aDirty.Count() - 1;
				if (last >= 0 && s_aDirty[last] == tile)
					s_aDirty.Remove(last);
			}
			else
			{
				s_iSkipped++;
			}
			s_iRingK++;
			int per = 8 * r;
			if (r == 0)
				per = 1;
			if (s_iRingK >= per)
			{
				if (r > 3 && !s_bRingOnGrid)
				{
					s_bMapDone = true;
					Print(string.Format("MUD bone field: MAP DONE | %1 tiles over the whole patch grid (offroad cells skipped %2, last ring %3 = %4 m out), fps %5 (WB, not perf)",
						s_aTiles.Count(), s_iSkipped, r, r * SIZE, s_fFps), LogLevel.NORMAL);
					SCR_HintManagerComponent.ShowCustomHint(string.Format("MAP done: %1 tiles", s_aTiles.Count()), "Mud bone field", 8, true);
					return;
				}
				s_iRing++;
				s_iRingK = 0;
				s_bRingOnGrid = false;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MAP: any offroad (holed) ground at the cell's centre or corners; also notes whether the ring touches the grid
	protected static bool CellOffroad(BaseWorld world, int tx, int tz)
	{
		float ftx = tx;
		float ftz = tz;
		float x0 = ftx * SIZE;
		float z0 = ftz * SIZE;
		float dug;
		float undug;
		if (!MUD_WorldGroundEntity.DugUndug(x0 + SIZE * 0.5, z0 + SIZE * 0.5, dug, undug))
			return false;
		s_bRingOnGrid = true;
		if (MUD_Surface.IsHole(world, x0 + SIZE * 0.5, z0 + SIZE * 0.5))
			return true;
		if (MUD_Surface.IsHole(world, x0 + 0.05, z0 + 0.05) || MUD_Surface.IsHole(world, x0 + SIZE - 0.05, z0 + 0.05))
			return true;
		return MUD_Surface.IsHole(world, x0 + 0.05, z0 + SIZE - 0.05) || MUD_Surface.IsHole(world, x0 + SIZE - 0.05, z0 + SIZE - 0.05);
	}

	//------------------------------------------------------------------------------------------------
	protected static vector ToBone(vector v, vector a0, vector a1, vector a2)
	{
		return Vector(vector.Dot(v, a0), vector.Dot(v, a1), vector.Dot(v, a2));
	}

	//------------------------------------------------------------------------------------------------
	protected static vector Cross(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}
}
