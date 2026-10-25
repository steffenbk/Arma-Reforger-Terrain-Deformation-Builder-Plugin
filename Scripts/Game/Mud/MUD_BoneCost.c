//! BONE TILE COST PROBE (TEST_KIT, 2026-10-23, plan docs/superpowers/plans/2026-10-23-bone-tile-mud.md step 3 B1-B3;
//! checkpoint v304 = this file before the hooks in MUD_TestKit / MUD_MudLab). For the STANDALONE game / a DS session's client - never read perf from Workbench.
//! No keys (the standalone exe has no diag keys): CLIENT settings in $profile:MUD_TestKit.txt (MUD_TestKit):
//!   bone_cost = tiles (0 = off), bone_cost_tile = 9 or 15, bone_cost_phase = s per phase, bone_cost_wheels = n.
//! START_WAIT s after the ground is up it lays a block of tiles (cols x rows, 4 m each, axis-aligned) in front of the
//! camera, draped on the ground with bone tilt (as MUD_BoneProbe TILT, user-OK WB 2026-10-23), then cycles for ever:
//!   OFF   - tiles hidden (baseline: a hidden entity is not drawn)
//!   IDLE  - tiles shown, no bone changes (B1: the per-draw bone palette copy, IDA 2026-10-19 D6)
//!   PRESS - tiles shown, bone_cost_wheels virtual wheels drive straight lines over the block at WHEEL_SPEED and press
//!           a TEST_D dip; only changed bones of touched tiles are set (B2)
//!   SWEEP - tiles shown, SWEEP_SETS[run % 3] bone sets per frame, every set a new value (B3: cost per set)
//! STAND STILL and do not turn the camera during the session (the view decides the render numbers).
//! Log per phase: 'MUD bone cost: run R PHASE ...' (fps, frame / render / GPU ms from System.GetProfileData, probe
//! script ms per frame, bone sets per frame, us per set). The frame right after a phase switch is not counted.
//! Look only: local entities, nothing synced, nothing felt, never on a DS.
class MUD_BoneCost
{
	static const float START_WAIT = 30;     // s after the first tick (ground up, player spawned)
	static const float SIZE = 4;            // tile side, m
	static const float LIFT = 0.02;
	static const float BOX_Y = 1.45;        // bone offset clamp (prefab BoundingBox +-1.5 m)
	static const float TEST_D = 0.15;       // PRESS dip depth, m
	static const float TEST_R = 0.35;       // PRESS dip half width, m
	static const float WHEEL_SPEED = 8;     // m/s
	static const float EPS_H = 0.001;
	static const float SWEEP_D = 0.03;      // SWEEP: every other pass sets bones this much lower (a new value each set)
	static const int GRID_ROWS_PER_FRAME = 8;
	static const int SPAWN_PER_FRAME = 10;
	static const int PH_OFF = 0;
	static const int PH_IDLE = 1;
	static const int PH_PRESS = 2;
	static const int PH_SWEEP = 3;

	protected static float s_fFirst = -1;
	protected static float s_fTickLast = -1;
	protected static int s_iStage;              // 0 wait, 1 read ground, 2 spawn, 3 phases, 4 done / failed
	protected static ref array<ref MUD_BoneTileP> s_aTiles;
	protected static ref array<float> s_aH;     // ground grid (world y)
	protected static ref array<float> s_aP;     // PRESS dip per grid point
	protected static ref array<bool> s_aDirty;  // PRESS: tile touched this frame
	protected static ref array<int> s_aSweepSets;
	protected static ref Resource s_Res;
	protected static int s_iSide;
	protected static int s_iCols;
	protected static int s_iRows;
	protected static int s_iTiles;
	protected static int s_iGX;                 // grid points per row / column
	protected static int s_iGZ;
	protected static float s_fX0;               // block corner (world)
	protected static float s_fZ0;
	protected static float s_fGX0;              // grid origin = corner - one step
	protected static float s_fGZ0;
	protected static float s_fStep;
	protected static int s_iReadRow;
	// phases
	protected static int s_iPhase;
	protected static int s_iRun;
	protected static float s_fPhaseEnd;
	protected static float s_fPhaseStart;
	protected static bool s_bSkip;
	protected static int s_iCursor;             // SWEEP: next tile * bones + bone
	protected static int s_iPass;               // SWEEP: passes over all bones (parity = offset)
	// per phase stats
	protected static int s_iFrames;
	protected static float s_fDtSum;
	protected static float s_fDtMax;
	protected static float s_fEngFrame;
	protected static float s_fEngRender;
	protected static float s_fEngGpu;
	protected static int s_iMs;
	protected static int s_iSets;
	protected static int s_iUpdates;
	protected static int s_iClamps;

	//------------------------------------------------------------------------------------------------
	//! Every frame from MUD_MudLab.Tick (render machines, ground ready)
	static void Tick(float time)
	{
		if (!MUD_TestKit.TEST_KIT)
			return;
		if (time < s_fTickLast)
		{
			// new play: entities are gone, start over
			s_fFirst = -1;
			s_iStage = 0;
			if (s_aTiles)
				s_aTiles.Clear();
		}
		float dt = 0;
		if (s_fTickLast >= 0)
			dt = time - s_fTickLast;
		s_fTickLast = time;
		if (s_iStage >= 4)
			return;
		if (s_iStage == 0)
		{
			if (MUD_TestKit.BoneCost() <= 0 || RplSession.Mode() == RplMode.Dedicated)
			{
				s_iStage = 4;
				return;
			}
			if (s_fFirst < 0)
				s_fFirst = time;
			if (time - s_fFirst < START_WAIT)
				return;
			Layout();
			return;
		}
		if (s_iStage == 1)
		{
			ReadRows();
			return;
		}
		if (s_iStage == 2)
		{
			SpawnSome(time);
			return;
		}
		PhaseTick(time, dt);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Layout()
	{
		s_iTiles = MUD_TestKit.BoneCost();
		s_iSide = 9;
		ResourceName prefab = "{4D55444250460009}Prefabs/Mud/MudBoneTile9.et";
		if (MUD_TestKit.BoneCostTile() == 15)
		{
			s_iSide = 15;
			prefab = "{4D55444250460015}Prefabs/Mud/MudBoneTile15.et";
		}
		s_Res = Resource.Load(prefab);
		if (!s_Res || !s_Res.IsValid())
		{
			Print(string.Format("MUD bone cost: prefab '%1' did not load - off", prefab), LogLevel.ERROR);
			s_iStage = 4;
			return;
		}
		s_iCols = Math.Ceil(Math.Sqrt(s_iTiles));
		float tilesF = s_iTiles;
		s_iRows = Math.Ceil(tilesF / s_iCols);
		float w = s_iCols * SIZE;
		float d = s_iRows * SIZE;
		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		vector fwd = cam[2];
		fwd[1] = 0;
		if (fwd.LengthSq() < 0.0001)
			fwd = vector.Forward;
		fwd.Normalize();
		vector mid = cam[3] + fwd * (Math.Max(w, d) * 0.5 + 4);
		s_fX0 = Math.Round((mid[0] - w * 0.5) / SIZE) * SIZE;
		s_fZ0 = Math.Round((mid[2] - d * 0.5) / SIZE) * SIZE;
		float sidef = s_iSide - 1;
		s_fStep = SIZE / sidef;
		s_iGX = s_iCols * (s_iSide - 1) + 3;
		s_iGZ = s_iRows * (s_iSide - 1) + 3;
		s_fGX0 = s_fX0 - s_fStep;
		s_fGZ0 = s_fZ0 - s_fStep;
		s_aH = {};
		s_aP = {};
		s_aH.Resize(s_iGX * s_iGZ);
		s_aP.Resize(s_iGX * s_iGZ);
		for (int g = 0; g < s_aP.Count(); g++)
		{
			s_aP[g] = 0;
		}
		s_aTiles = {};
		s_aDirty = {};
		s_aSweepSets = {100, 1000, 5000};
		s_iReadRow = 0;
		s_iStage = 1;
		Print(string.Format("MUD bone cost: layout %1 tiles (%2 x %3), tile %4 x %4 bones, block corner %5 %6, %7 x %8 m, grid %9 points",
			s_iTiles, s_iCols, s_iRows, s_iSide, s_fX0, s_fZ0, w, d, s_iGX * s_iGZ), LogLevel.NORMAL);
		Print(string.Format("MUD bone cost: phase %1 s, wheels %2, sweep sets per frame 100 / 1000 / 5000 by run. STAND STILL, keep the camera on the block.",
			MUD_TestKit.BoneCostPhase(), MUD_TestKit.BoneCostWheels()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool GroundAt(float x, float z, out float y)
	{
		float dug;
		float undug;
		if (MUD_WorldGroundEntity.DugUndug(x, z, dug, undug))
		{
			y = dug;
			return true;
		}
		y = GetGame().GetWorld().GetSurfaceY(x, z);
		return y > -200;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReadRows()
	{
		int end = Math.Min(s_iGZ, s_iReadRow + GRID_ROWS_PER_FRAME);
		for (int gz = s_iReadRow; gz < end; gz++)
		{
			float fz = gz;
			float z = s_fGZ0 + fz * s_fStep;
			for (int gx = 0; gx < s_iGX; gx++)
			{
				float fx = gx;
				float y;
				if (!GroundAt(s_fGX0 + fx * s_fStep, z, y))
					y = 0;
				s_aH[gz * s_iGX + gx] = y;
			}
		}
		s_iReadRow = end;
		if (s_iReadRow >= s_iGZ)
			s_iStage = 2;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SpawnSome(float time)
	{
		BaseWorld world = GetGame().GetWorld();
		for (int n = 0; n < SPAWN_PER_FRAME && s_aTiles.Count() < s_iTiles; n++)
		{
			int t = s_aTiles.Count();
			int ti = t % s_iCols;
			int tj = t / s_iCols;
			float fi = ti;
			float fj = tj;
			float ox = s_fX0 + SIZE * 0.5 + fi * SIZE;
			float oz = s_fZ0 + SIZE * 0.5 + fj * SIZE;
			float oy;
			if (!GroundAt(ox, oz, oy))
				oy = 0;
			MUD_BoneTileP tile = SpawnTile(world, Vector(ox, oy, oz));
			if (!tile)
			{
				s_iStage = 4;
				return;
			}
			s_aTiles.Insert(tile);
			s_aDirty.Insert(false);
			ApplyTile(tile, true, 0);
		}
		if (s_aTiles.Count() < s_iTiles)
			return;
		int found;
		int bones;
		foreach (MUD_BoneTileP tl : s_aTiles)
		{
			for (int b = 0; b < tl.m_aGrid.Count(); b++)
			{
				bones++;
				if (tl.m_aGrid[b] >= 0)
					found++;
			}
		}
		Print(string.Format("MUD bone cost: %1 tiles up, %2 of %3 bones on the grid, clamps %4 - phases start", s_aTiles.Count(), found, bones, s_iClamps), LogLevel.NORMAL);
		s_iStage = 3;
		s_iRun = 0;
		StartPhase(PH_OFF, time);
	}

	//------------------------------------------------------------------------------------------------
	protected static MUD_BoneTileP SpawnTile(BaseWorld world, vector org)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = org;
		IEntity ent = GetGame().SpawnEntityPrefabLocal(s_Res, world, params);
		if (!ent)
		{
			Print("MUD bone cost: SpawnEntityPrefabLocal returned null - off", LogLevel.ERROR);
			return null;
		}
		Animation anim = ent.GetAnimation();
		if (!anim)
		{
			Print("MUD bone cost: GetAnimation() null on the tile - off", LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(ent);
			return null;
		}
		MUD_BoneTileP tile = new MUD_BoneTileP();
		tile.m_Ent = ent;
		tile.m_vOrg = org;
		string abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		for (int j = 0; j < s_iSide; j++)
		{
			for (int i = 0; i < s_iSide; i++)
			{
				int id = anim.GetBoneIndex("g" + abc.Get(i) + abc.Get(j));
				vector m[4];
				Math3D.MatrixIdentity4(m);
				int g = -1;
				if (id >= 0 && anim.GetBoneMatrix(id, m))
				{
					vector p = m[3];
					int gx = Math.Round((org[0] + p[0] - s_fGX0) / s_fStep);
					int gz = Math.Round((org[2] + p[2] - s_fGZ0) / s_fStep);
					if (gx >= 1 && gz >= 1 && gx <= s_iGX - 2 && gz <= s_iGZ - 2)
						g = gz * s_iGX + gx;
				}
				tile.m_aId.Insert(id);
				tile.m_aGrid.Insert(g);
				tile.m_aAx0.Insert(m[0]);
				tile.m_aAx1.Insert(m[1]);
				tile.m_aAx2.Insert(m[2]);
				tile.m_aLastH.Insert(-999);
				tile.m_aLastSx.Insert(0);
				tile.m_aLastSz.Insert(0);
			}
		}
		return tile;
	}

	//------------------------------------------------------------------------------------------------
	//! Set the bones of one tile that changed (all when force); extra = SWEEP offset. Returns bones set.
	protected static int ApplyTile(MUD_BoneTileP tile, bool force, float extra)
	{
		Animation anim = tile.m_Ent.GetAnimation();
		if (!anim)
			return 0;
		int sets;
		for (int b = 0; b < tile.m_aId.Count(); b++)
		{
			if (SetBone(tile, anim, b, force, extra))
				sets++;
		}
		if (sets > 0)
		{
			tile.m_Ent.Update();
			s_iUpdates++;
		}
		return sets;
	}

	//------------------------------------------------------------------------------------------------
	//! One bone: drape + dip + extra, turned to the slope (TILT). False = unchanged / missing.
	protected static bool SetBone(MUD_BoneTileP tile, Animation anim, int b, bool force, float extra)
	{
		int id = tile.m_aId[b];
		int g = tile.m_aGrid[b];
		if (id < 0 || g < 0)
			return false;
		vector org = tile.m_vOrg;
		float h = s_aH[g] + LIFT - s_aP[g] - org[1] - extra;
		if (h > BOX_Y || h < -BOX_Y)
		{
			s_iClamps++;
			h = Math.Clamp(h, -BOX_Y, BOX_Y);
		}
		float sx = (s_aH[g + 1] - s_aP[g + 1] - s_aH[g - 1] + s_aP[g - 1]) / (2 * s_fStep);
		float sz = (s_aH[g + s_iGX] - s_aP[g + s_iGX] - s_aH[g - s_iGX] + s_aP[g - s_iGX]) / (2 * s_fStep);
		if (!force && Math.AbsFloat(h - tile.m_aLastH[b]) < EPS_H && Math.AbsFloat(sx - tile.m_aLastSx[b]) < EPS_H && Math.AbsFloat(sz - tile.m_aLastSz[b]) < EPS_H)
			return false;
		tile.m_aLastH[b] = h;
		tile.m_aLastSx[b] = sx;
		tile.m_aLastSz[b] = sz;
		vector a0 = tile.m_aAx0[b];
		vector a1 = tile.m_aAx1[b];
		vector a2 = tile.m_aAx2[b];
		vector rx = Vector(1, sx, 0).Normalized();
		vector rz = Vector(0, sz, 1).Normalized();
		vector ry = Cross(rz, rx).Normalized();
		rz = Cross(rx, ry);
		vector m[4];
		m[0] = ToBone(rx * a0[0] + ry * a0[1] + rz * a0[2], a0, a1, a2);
		m[1] = ToBone(rx * a1[0] + ry * a1[1] + rz * a1[2], a0, a1, a2);
		m[2] = ToBone(rx * a2[0] + ry * a2[1] + rz * a2[2], a0, a1, a2);
		m[3] = ToBone(Vector(0, h, 0), a0, a1, a2);
		anim.SetBoneMatrix(tile.m_Ent, id, m);
		s_iSets++;
		return true;
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

	//------------------------------------------------------------------------------------------------
	protected static void ShowAll(bool on)
	{
		foreach (MUD_BoneTileP tile : s_aTiles)
		{
			if (on)
				tile.m_Ent.SetFlags(EntityFlags.VISIBLE, false);
			else
				tile.m_Ent.ClearFlags(EntityFlags.VISIBLE, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Phase switch work (not counted: s_bSkip drops the next frame's sample)
	protected static void StartPhase(int phase, float time)
	{
		// back to the plain drape after PRESS / SWEEP (dips cleared, sweep offset gone)
		for (int g = 0; g < s_aP.Count(); g++)
		{
			s_aP[g] = 0;
		}
		if (s_iPhase == PH_PRESS || s_iPhase == PH_SWEEP)
		{
			foreach (MUD_BoneTileP tile : s_aTiles)
			{
				ApplyTile(tile, false, 0);
			}
		}
		s_iPhase = phase;
		ShowAll(phase != PH_OFF);
		s_fPhaseStart = time;
		s_fPhaseEnd = time + Math.Max(5, MUD_TestKit.BoneCostPhase());
		s_iCursor = 0;
		s_iPass = 0;
		s_bSkip = true;
		s_iFrames = 0;
		s_fDtSum = 0;
		s_fDtMax = 0;
		s_fEngFrame = 0;
		s_fEngRender = 0;
		s_fEngGpu = 0;
		s_iMs = 0;
		s_iSets = 0;
		s_iUpdates = 0;
		s_iClamps = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static string PhaseName(int phase)
	{
		if (phase == PH_OFF)
			return "OFF (hidden)";
		if (phase == PH_IDLE)
			return "IDLE";
		if (phase == PH_PRESS)
			return "PRESS";
		return string.Format("SWEEP %1/frame", s_aSweepSets[s_iRun % 3]);
	}

	//------------------------------------------------------------------------------------------------
	protected static void PhaseTick(float time, float dt)
	{
		int t0 = System.GetTickCount();
		if (s_iPhase == PH_PRESS)
			PressFrame(time);
		else if (s_iPhase == PH_SWEEP)
			SweepFrame();
		int ms = System.GetTickCount() - t0;
		if (s_bSkip)
		{
			// the switch frame (and its work) is not part of either phase
			s_bSkip = false;
			s_iSets = 0;
			s_iUpdates = 0;
		}
		else
		{
			ProfileData pd = System.GetProfileData();
			s_iFrames++;
			s_fDtSum += dt * 1000;
			s_fDtMax = Math.Max(s_fDtMax, dt * 1000);
			s_iMs += ms;
			if (pd)
			{
				s_fEngFrame += pd.FrameTimeMs;
				s_fEngRender += pd.RenderTimeMs;
				s_fEngGpu += pd.GPUTimeMs;
			}
		}
		if (time < s_fPhaseEnd)
			return;
		Report();
		int next = s_iPhase + 1;
		if (next > PH_SWEEP)
		{
			next = PH_OFF;
			s_iRun++;
		}
		StartPhase(next, time);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Report()
	{
		float n = Math.Max(1, s_iFrames);
		float secs = Math.Max(0.001, s_fDtSum / 1000);
		float fps = s_iFrames / secs;
		float perSet = 0;
		if (s_iSets > 0)
			perSet = s_iMs * 1000.0 / s_iSets;
		Print(string.Format("MUD bone cost: run %1 %2 | tiles %3 x %4 bones | frames %5, fps %6, frame avg %7 ms max %8",
			s_iRun, PhaseName(s_iPhase), s_aTiles.Count(), s_iSide * s_iSide, s_iFrames, fps, s_fDtSum / n, s_fDtMax)
			+ string.Format(" | engine frame %1, render %2, GPU %3 ms | probe script %4 ms/frame | bone sets %5/frame, tile updates %6/frame, us per set %7, clamps %8",
			s_fEngFrame / n, s_fEngRender / n, s_fEngGpu / n, s_iMs / n, s_iSets / n, s_iUpdates / n, perSet, s_iClamps), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! PRESS: each virtual wheel runs along x on its own z line, stamping a dip; touched tiles set their changed bones
	protected static void PressFrame(float time)
	{
		int wheels = Math.Max(1, MUD_TestKit.BoneCostWheels());
		float w = s_iCols * SIZE;
		float d = s_iRows * SIZE;
		float t = time - s_fPhaseStart;
		for (int k = 0; k < wheels; k++)
		{
			float fk = k;
			float z = s_fZ0 + d * (fk + 0.5) / wheels;
			float run = t * WHEEL_SPEED + fk * 7;
			float x = s_fX0 + run - Math.Floor(run / w) * w;
			Stamp(x, z);
		}
		for (int i = 0; i < s_aTiles.Count(); i++)
		{
			if (!s_aDirty[i])
				continue;
			s_aDirty[i] = false;
			ApplyTile(s_aTiles[i], false, 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stamp(float x, float z)
	{
		int gx0 = Math.Max(1, Math.Ceil((x - TEST_R - s_fGX0) / s_fStep));
		int gx1 = Math.Min(s_iGX - 2, Math.Floor((x + TEST_R - s_fGX0) / s_fStep));
		int gz0 = Math.Max(1, Math.Ceil((z - TEST_R - s_fGZ0) / s_fStep));
		int gz1 = Math.Min(s_iGZ - 2, Math.Floor((z + TEST_R - s_fGZ0) / s_fStep));
		bool grew;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			float fz = gz;
			float dz = s_fGZ0 + fz * s_fStep - z;
			for (int gx = gx0; gx <= gx1; gx++)
			{
				float fx = gx;
				float dx = s_fGX0 + fx * s_fStep - x;
				float dd = Math.Sqrt(dx * dx + dz * dz);
				if (dd >= TEST_R)
					continue;
				float k = dd / TEST_R;
				float want = TEST_D * (1 - k * k);
				int g = gz * s_iGX + gx;
				if (want > s_aP[g] + EPS_H)
				{
					s_aP[g] = want;
					grew = true;
				}
			}
		}
		if (!grew)
			return;
		// tiles under the stamp box, + 1 step for the slope of the neighbour bones
		float r = TEST_R + s_fStep;
		int ti0 = Math.Max(0, Math.Floor((x - r - s_fX0) / SIZE));
		int ti1 = Math.Min(s_iCols - 1, Math.Floor((x + r - s_fX0) / SIZE));
		int tj0 = Math.Max(0, Math.Floor((z - r - s_fZ0) / SIZE));
		int tj1 = Math.Min(s_iRows - 1, Math.Floor((z + r - s_fZ0) / SIZE));
		for (int tj = tj0; tj <= tj1; tj++)
		{
			for (int ti = ti0; ti <= ti1; ti++)
			{
				int idx = tj * s_iCols + ti;
				if (idx < s_aDirty.Count())
					s_aDirty[idx] = true;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! SWEEP: the next N bones (round robin over every tile), each to a new value (pass parity = offset 0 / SWEEP_D)
	protected static void SweepFrame()
	{
		int want = s_aSweepSets[s_iRun % 3];
		int per = s_iSide * s_iSide;
		int total = s_aTiles.Count() * per;
		if (total <= 0)
			return;
		int done;
		int guard;
		MUD_BoneTileP last;
		Animation anim;
		while (done < want && guard < total)
		{
			guard++;
			int ti = s_iCursor / per;
			int b = s_iCursor - ti * per;
			MUD_BoneTileP tile = s_aTiles[ti];
			if (tile != last)
			{
				if (last)
				{
					last.m_Ent.Update();
					s_iUpdates++;
				}
				last = tile;
				anim = tile.m_Ent.GetAnimation();
			}
			float extra = 0;
			if (s_iPass % 2 == 1)
				extra = SWEEP_D;
			if (anim && SetBone(tile, anim, b, true, extra))
				done++;
			s_iCursor++;
			if (s_iCursor >= total)
			{
				s_iCursor = 0;
				s_iPass++;
			}
		}
		if (last)
		{
			last.m_Ent.Update();
			s_iUpdates++;
		}
	}
}
