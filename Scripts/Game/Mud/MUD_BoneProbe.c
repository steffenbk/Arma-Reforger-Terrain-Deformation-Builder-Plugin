//! BONE TILE PROBE (TEST_KIT, 2026-10-23). Plan: docs/superpowers/plans/2026-10-23-bone-tile-mud.md (step 2).
//! Checkpoints: v299 = first probe, v301 = user-OK S demo (one tile, virtual wheel), v302 = before this step-2 rewrite
//! (the S demo is in v302 / git if it is needed again).
//! Step 2: a 2 x 2 block of PREFAB tiles (Prefabs/Mud/MudBoneTile9 / 15 = MeshObject + BoundingBox override, so a
//! bent tile is not culled by its flat bind-pose box) on the 4 m world grid, draped by its bones on the REAL ground as
//! the patches draw it (MUD_WorldGroundEntity.DugUndug 'dug' = heightfield + rut field floor + ridges). Drive or dig
//! over it: the bones follow the rut field. Look only: local entities (SpawnEntityPrefabLocal), no collider, never a DS.
//! Bone heights come from ONE world grid shared by the 4 tiles, so the shared edge bones of two tiles get the same
//! number (seam test). Blender check 2026-10-23: edge verts are weighted only to edge bones (plan doc step 1).
//! Bone (world) positions come from the engine's rest matrices (GetBoneMatrix m[3]), not from the bone names.
//! Keys (diag exes only):
//! - numpad 1: off -> MAP tile 9 -> BUILD -> BUILD STOPPED -> FIELD tile 15 -> FIELD tile 9 -> 2 x 2 tile 9 -> 2 x 2
//!   tile 15 -> PLATE COLLIDERS 5 x 5 tile 9 -> WATER pond material -> WATER glass material -> off (MUD_WaterProbe, own file). BUILD = MAP without the safety stop; the next press stops spawning and KEEPS the tiles, so you
//!   can see how many tiles the frame takes (user 2026-10-23). MAP = tiles
//!   spawn outward over the whole map's offroad ground and never move (MUD_BoneField MAP mode). FIELD = MUD_BoneField
//!   (2026-10-23: thick mud tiles that follow the camera over the whole map, own file). The 2 x 2 block spawns at the
//!   4 m grid corner nearest a point AHEAD m in front of the camera.
//! - numpad 2: look mode: FLAT (bones only move, normals stay up) -> TILT (bones also turn to the ground slope, so
//!   the normals follow) -> HIDDEN (tiles hidden, our patch ground only, to compare).
//! - numpad 4: TEST PRESS on / off (default on; user 2026-10-23 'so I can see the tiles react'). On top of the rut
//!   field, the tiles dip TEST_D under every wheel contact (MUD_Wheels vehicles, hub point as the ruts) and under the
//!   local player's feet, every frame. Look only, local, never synced, nothing felt; the dip only grows until the
//!   block respawns or the key turns it off.
//! Log: 'MUD bone probe: ...' (bones found, rest range, grid misfits; every 5 s refreshes / reads / bone sets / clamps).
//! PLATE COLLIDERS (2026-10-23, user: 'build a small bone tile with one tilted plate collider per bone and test driving
//! on it'; mud-gotchas: a script SetBoneMatrix moves a bone-bound collider, proven on the Ural v_body): prefab
//! MudBoneCol9 = BoneTile9 + one UBX_ plate (0.6 x 0.6 x 0.1 m, top at the bone, dirt gamemat) parented to each bone
//! (Blender build: Tools/blender/bonecol9_build.py, sent with Tools/blender/bl_send.py). A COL_BLOCK x COL_BLOCK block (20 m), physics made
//! with Physics.CreateStatic on each tile, raised COL_RAISE over the ground with a COL_RAMP m ramp at the block edge, so
//! the TEST PRESS dip (COL_D, wheels COL_R_WHEEL) stays above our own ground collider and the plates are what the wheels
//! touch. Every 5 s 'MUD bone col: ...' traces down onto the block centre (only that tile) and compares the hit with the
//! bone height there: FOLLOWS = the static collider took the script bones.
//! WB 2026-10-23 16:52 (v318): a STATIC body (CreateStatic) STAYED at the spawn pose (hit y 39.00, bone 39.42; user: got
//! stuck, could not walk onto it). So the body is now made in COL_BODY order: 1 dynamic + SimulationState.COLLISION
//! (moves with nothing, never falls), 2 dynamic + SIMULATION, gravity off, held in place every frame; two STAYED checks
//! in a row move on to the next. 0 = the static body, kept for the record.
class MUD_BoneTileP
{
	IEntity m_Ent;
	vector m_vOrg;                              // entity origin (world), no rotation
	ref array<int> m_aId = {};                  // node id per grid bone, -1 = missing
	ref array<int> m_aGrid = {};                // index into the shared height grid, -1 = off grid
	ref array<vector> m_aAx0 = {};              // rest matrix rows (model space) per bone
	ref array<vector> m_aAx1 = {};
	ref array<vector> m_aAx2 = {};
	ref array<float> m_aLastH = {};             // last set offset, slopes (change test)
	ref array<float> m_aLastSx = {};
	ref array<float> m_aLastSz = {};
}

class MUD_BoneProbe
{
	static const float AHEAD = 6;
	static const float SIZE = 4;            // tile side, m (Blender build)
	static const float LIFT = 0.02;         // tile drawn this much over the ground (patch mesh must not poke through)
	static const float BOX_Y = 1.45;        // bone offset clamp (prefab BoundingBox is +-1.5 m)
	static const float REFRESH_S = 0.2;     // re-read the ground this often (rut ops come every 0.25 s per vehicle)
	static const float EPS_H = 0.001;       // set a bone only when its offset / slope moved more than this
	static const int MODE_FLAT = 0;
	static const int MODE_TILT = 1;
	static const int MODE_HIDDEN = 2;
	static const float TEST_D = 0.15;       // TEST PRESS depth, m
	static const float TEST_R_WHEEL = 0.35; // TEST PRESS half width under a wheel, m
	static const float TEST_R_FOOT = 0.25;  // under the player's feet, m
	static const float TEST_NEAR = 30;      // vehicles within this of the block are read
	static const int COL_TILE = 2;          // s_aPrefabs index of the plate collider tile
	static const int COL_BLOCK = 5;         // tiles per side (20 m)
	static const float COL_RAISE = 0.4;     // m over the ground inside the ramp
	static const float COL_RAMP = 3;        // m from the block edge up to COL_RAISE
	static const float COL_D = 0.3;         // TEST PRESS depth on the plate block
	static const float COL_R_WHEEL = 0.6;   // TEST PRESS half width under a wheel on the plate block
	static const int COL_BODY_FIRST = 0;    // 0 static, 1 dynamic COLLISION (both STAYED v318/v319 without UpdateGeometries;
	                                        // 2 SIMULATION held jittered + broke apart, user 2026-10-23 - no longer reached)
	static const float COL_MASS = 100000;   // kg per tile body (dynamic modes)

	protected static ref array<ResourceName> s_aPrefabs;
	protected static ref array<int> s_aSide;    // bones per side
	protected static int s_iTile = -1;          // -1 = off
	protected static int s_iSel;                // numpad 1 cycle: 0 off, 1 map 9, 2 build, 3 build stopped, 4 field 15,
	                                            // 5 field 9, 6 block 9, 7 block 15, 8 plate colliders, 9 water pond, 10 water glass
	protected static int s_iMode = MODE_TILT;
	protected static int s_iBlock = 2;          // tiles per side of the block
	protected static ref array<float> s_aRaise = {};   // per shared grid point: lift over the ground (plate block ramp)
	protected static int s_iColBody;            // body mode now (COL_BODY_FIRST ..)
	protected static int s_iColStayed;          // STAYED checks in a row
	protected static float s_fTickLast = -1;
	protected static float s_fNextRefresh;
	protected static float s_fNextStat;
	protected static ref array<ref MUD_BoneTileP> s_aTiles;
	protected static ref array<float> s_aGridH; // shared height grid over the block + 1 bone step border
	protected static int s_iGridN;              // grid points per side
	protected static float s_fGridX0;
	protected static float s_fGridZ0;
	protected static float s_fStep;
	protected static float s_fBaseY;            // tiles' plane height (world)
	protected static bool s_bForce;
	protected static bool s_bPressOn = true;
	protected static bool s_bPressDirty;        // TEST PRESS grew: re-apply bones next frame (cached ground)
	protected static ref array<float> s_aPress; // TEST PRESS depth per shared grid point
	protected static vector s_vMid;             // block centre (world)
	protected static int s_iStamps;
	protected static int s_iRefreshes;          // stats per 5 s
	protected static int s_iReads;
	protected static int s_iReadFails;
	protected static int s_iSets;
	protected static int s_iClamps;
	protected static int s_iSetFails;
	protected static int s_iMs;

	//------------------------------------------------------------------------------------------------
	static void Tick(float time)
	{
		if (!MUD_TestKit.TEST_KIT)
			return;
		if (!s_aPrefabs)
		{
			s_aPrefabs = {"{4D55444250460009}Prefabs/Mud/MudBoneTile9.et", "{4D55444250460015}Prefabs/Mud/MudBoneTile15.et", "{4D55444250460109}Prefabs/Mud/MudBoneCol9.et"};
			s_aSide = {9, 15, 9};
			s_aTiles = {};
			s_aGridH = {};
			s_aPress = {};
		}
		if (time < s_fTickLast)
		{
			s_aTiles.Clear();
			s_iTile = -1;
			s_iSel = 0;
			MUD_BoneField.Reset();
			MUD_WaterProbe.Stop();
		}
		s_fTickLast = time;
#ifdef ENABLE_DIAG
		if (Debug.KeyState(KeyCode.KC_NUMPAD1))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD1);
			s_iSel = (s_iSel + 1) % 11;
			if (s_iSel == 3)
			{
				MUD_BoneField.StopBuilding();     // BUILD -> stopped: the tiles stay, nothing new spawns
			}
			else
			{
				MUD_BoneField.Stop();
				MUD_WaterProbe.Stop();
				Stop();
				s_iTile = -1;
				if (s_iSel == 1)
					MUD_BoneField.Start(9, s_iMode, s_bPressOn, true);
				else if (s_iSel == 2)
					MUD_BoneField.StartBuild(s_iMode, s_bPressOn);
				else if (s_iSel == 4)
					MUD_BoneField.Start(15, s_iMode, s_bPressOn, false);
				else if (s_iSel == 5)
					MUD_BoneField.Start(9, s_iMode, s_bPressOn, false);
				else if (s_iSel == 6 || s_iSel == 7)
				{
					s_iTile = s_iSel - 6;
					Start(time);
				}
				else if (s_iSel == 8)
				{
					s_iTile = COL_TILE;
					Start(time);
				}
				else if (s_iSel >= 9)
					MUD_WaterProbe.Start(s_iSel - 9);
			}
			Show();
		}
		if (Debug.KeyState(KeyCode.KC_NUMPAD2) && MUD_WaterProbe.IsOn())
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD2);
			MUD_WaterProbe.NextLevel();     // water: numpad 2 = wave level
			Show();
		}
		if (Debug.KeyState(KeyCode.KC_NUMPAD4) && MUD_WaterProbe.IsOn())
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD4);
			MUD_WaterProbe.Splash();        // water: numpad 4 = splash where you look
		}
		if (Debug.KeyState(KeyCode.KC_NUMPAD2))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD2);
			s_iMode = (s_iMode + 1) % 3;
			ShowTiles(s_iMode != MODE_HIDDEN);
			MUD_BoneField.SetMode(s_iMode);
			s_bForce = true;
			Show();
		}
		if (Debug.KeyState(KeyCode.KC_NUMPAD4))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD4);
			s_bPressOn = !s_bPressOn;
			ClearPress();
			MUD_BoneField.SetPress(s_bPressOn);
			s_bForce = true;
			Show();
		}
#endif
		MUD_BoneField.Tick(time);
		MUD_WaterProbe.Tick(time);      // water bone probe 2026-10-23 (numpad 1 entries 8 / 9, own file)
		MUD_VehBoneProbe.Tick(time);    // vehicle bone probe 2026-10-23 (numpad +, own file)
		if (s_iTile < 0 || s_aTiles.IsEmpty())
			return;
		if (s_bPressOn)
			PressTick();
		if (s_iTile == COL_TILE && s_iColBody == 2)
			HoldBodies();
		if (time >= s_fNextRefresh)
		{
			s_fNextRefresh = time + REFRESH_S;
			if (s_iMode != MODE_HIDDEN)
				Refresh(true);
		}
		else if (s_bPressDirty && s_iMode != MODE_HIDDEN)
		{
			Refresh(false);
		}
		if (time >= s_fNextStat)
		{
			s_fNextStat = time + 5;
			Print(string.Format("MUD bone probe: 5 s | refreshes %1, ground reads %2 (fails %3), bone sets %4, clamps %5, set fails %6, test stamps %7, %8 ms (WB, not perf)",
				s_iRefreshes, s_iReads, s_iReadFails, s_iSets, s_iClamps, s_iSetFails, s_iStamps, s_iMs), LogLevel.NORMAL);
			if (s_iTile == COL_TILE)
				ColCheck();
			s_iStamps = 0;
			s_iRefreshes = 0;
			s_iReads = 0;
			s_iReadFails = 0;
			s_iSets = 0;
			s_iClamps = 0;
			s_iMs = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Show()
	{
		string what = "OFF";
		if (s_iTile == COL_TILE)
			what = string.Format("PLATE COLLIDERS: %1 x %1 tiles (raised %2 m, ramp %3 m) - drive on it", COL_BLOCK, COL_RAISE, COL_RAMP);
		else if (s_iTile >= 0)
			what = string.Format("2 x 2 tiles, %1 x %1 bones", s_aSide[s_iTile]);
		else if (s_iSel == 1)
			what = "MAP (whole map offroad, thick mud, spawning outward, safety stop), 9 x 9 bones";
		else if (s_iSel == 2)
			what = "BUILD (whole map outward, NO safety stop) - numpad 1 again = stop building, keep the tiles";
		else if (s_iSel == 3)
			what = "BUILD STOPPED - tiles kept (count + fps in the log), numpad 1 = next";
		else if (s_iSel == 4)
			what = "FIELD (follows you, thick mud), 15 x 15 bones";
		else if (s_iSel == 5)
			what = "FIELD (follows you, thick mud), 9 x 9 bones";
		else if (s_iSel >= 9)
			what = MUD_WaterProbe.Text();
		string mode = "FLAT normals";
		if (s_iMode == MODE_TILT)
			mode = "TILT bones";
		else if (s_iMode == MODE_HIDDEN)
			mode = "HIDDEN (ground only)";
		string press = "TEST PRESS off (rut field only)";
		if (s_bPressOn)
			press = "TEST PRESS on (wheels + feet, look only)";
		Print(string.Format("MUD bone probe: %1 | mode %2 | %3 | numpad 1 tile, 2 mode, 4 test press", what, mode, press), LogLevel.NORMAL);
		SCR_HintManagerComponent.ShowCustomHint(what + "\nmode " + mode + "\n" + press + "\n(numpad 1 / 2 / 4)", "Mud bone probe", 6, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Ground as the patches draw it at x, z; false = unknown
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
		return y > -200;    // holed cell: GetSurfaceY is -256 there
	}

	//------------------------------------------------------------------------------------------------
	protected static void Start(float time)
	{
		Stop();
		if (RplSession.Mode() == RplMode.Dedicated)
			return;
		BaseWorld world = GetGame().GetWorld();
		vector cam[4];
		world.GetCurrentCamera(cam);
		vector fwd = cam[2];
		fwd[1] = 0;
		if (fwd.LengthSq() < 0.0001)
			fwd = vector.Forward;
		fwd.Normalize();
		vector look = cam[3] + fwd * AHEAD;
		// block = s_iBlock x s_iBlock tiles; 2 x 2 around the nearest 4 m grid corner, the odd plate block around the
		// nearest tile centre
		s_iBlock = 2;
		if (s_iTile == COL_TILE)
			s_iBlock = COL_BLOCK;
		float blockHalf = s_iBlock * SIZE * 0.5;
		float cx = Math.Round(look[0] / SIZE) * SIZE;
		float cz = Math.Round(look[2] / SIZE) * SIZE;
		if (s_iBlock % 2 == 1)
		{
			cx += SIZE * 0.5;
			cz += SIZE * 0.5;
		}
		int side = s_aSide[s_iTile];
		float sidef = side - 1;
		s_fStep = SIZE / sidef;
		s_iGridN = s_iBlock * (side - 1) + 1 + 2;      // block bones + 1 step border each side (slopes at the block edge)
		s_fGridX0 = cx - blockHalf - s_fStep;
		s_fGridZ0 = cz - blockHalf - s_fStep;
		s_aGridH.Clear();
		s_aGridH.Resize(s_iGridN * s_iGridN);
		s_aPress.Clear();
		s_aPress.Resize(s_iGridN * s_iGridN);
		ClearPress();
		MakeRaise();
		s_vMid = Vector(cx, 0, cz);
		float baseY;
		if (!GroundAt(cx, cz, baseY))
		{
			vector camPos = cam[3];
			baseY = camPos[1] - 1.7;
		}
		s_fBaseY = baseY;

		Resource res = Resource.Load(s_aPrefabs[s_iTile]);
		if (!res || !res.IsValid())
		{
			Print(string.Format("MUD bone probe: prefab '%1' did not load", s_aPrefabs[s_iTile]), LogLevel.ERROR);
			return;
		}
		for (int t = 0; t < s_iBlock * s_iBlock; t++)
		{
			int tx = t % s_iBlock;
			int tz = t / s_iBlock;
			float ox = cx - blockHalf + SIZE * 0.5 + tx * SIZE;
			float oz = cz - blockHalf + SIZE * 0.5 + tz * SIZE;
			MUD_BoneTileP tile = SpawnTile(res, world, Vector(ox, s_fBaseY, oz), side);
			if (!tile)
				continue;
			s_aTiles.Insert(tile);
		}
		if (s_iTile == COL_TILE)
		{
			s_iColStayed = 0;
			MakeBodies(COL_BODY_FIRST);
		}
		ShowTiles(s_iMode != MODE_HIDDEN);
		s_bForce = true;
		s_fNextRefresh = time;
		s_fNextStat = time + 5;
		s_iSetFails = 0;
		Print(string.Format("MUD bone probe: %1 tiles at corner %2 %3, base y %4, step %5 m, grid %6 x %6", s_aTiles.Count(), cx, cz, s_fBaseY, s_fStep, s_iGridN), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static MUD_BoneTileP SpawnTile(Resource res, BaseWorld world, vector org, int side)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = org;
		IEntity ent = GetGame().SpawnEntityPrefabLocal(res, world, params);
		if (!ent)
		{
			Print("MUD bone probe: SpawnEntityPrefabLocal returned null", LogLevel.ERROR);
			return null;
		}
		Animation anim = ent.GetAnimation();
		if (!anim)
		{
			Print("MUD bone probe: GetAnimation() is NULL on the prefab tile - no bones to drive", LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(ent);
			return null;
		}
		MUD_BoneTileP tile = new MUD_BoneTileP();
		tile.m_Ent = ent;
		tile.m_vOrg = org;
		int found;
		int misfit;
		float minX = 999;
		float maxX = -999;
		float minY = 999;
		float maxY = -999;
		for (int j = 0; j < side; j++)
		{
			for (int i = 0; i < side; i++)
			{
				int id = anim.GetBoneIndex(BoneName(i, j));
				vector m[4];
				Math3D.MatrixIdentity4(m);
				int g = -1;
				if (id >= 0 && anim.GetBoneMatrix(id, m))
				{
					found++;
					vector p = m[3];
					minX = Math.Min(minX, p[0]);
					maxX = Math.Max(maxX, p[0]);
					minY = Math.Min(minY, p[1]);
					maxY = Math.Max(maxY, p[1]);
					float gxf = (org[0] + p[0] - s_fGridX0) / s_fStep;
					float gzf = (org[2] + p[2] - s_fGridZ0) / s_fStep;
					int gx = Math.Round(gxf);
					int gz = Math.Round(gzf);
					if (Math.AbsFloat(gxf - gx) > 0.02 || Math.AbsFloat(gzf - gz) > 0.02 || gx < 1 || gz < 1 || gx > s_iGridN - 2 || gz > s_iGridN - 2)
						misfit++;
					else
						g = gz * s_iGridN + gx;
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
		Print(string.Format("MUD bone probe: tile at %1 | grid bones found %2 of %3, rest x %4 .. %5, rest y %6 .. %7, off-grid %8",
			org, found, side * side, minX, maxX, minY, maxY, misfit), LogLevel.NORMAL);
		return tile;
	}

	//------------------------------------------------------------------------------------------------
	//! Grid bone name as built in Blender: 'g' + 'A'+i + 'A'+j (3 chars: GetBoneIndex misses 6-7 char names)
	static string BoneName(int i, int j)
	{
		string abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		return "g" + abc.Get(i) + abc.Get(j);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stop()
	{
		if (!s_aTiles)
			return;
		foreach (MUD_BoneTileP tile : s_aTiles)
		{
			if (tile.m_Ent && !tile.m_Ent.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(tile.m_Ent);
		}
		s_aTiles.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static void ShowTiles(bool on)
	{
		foreach (MUD_BoneTileP tile : s_aTiles)
		{
			if (!tile.m_Ent)
				continue;
			if (on)
				tile.m_Ent.SetFlags(EntityFlags.VISIBLE, false);
			else
				tile.m_Ent.ClearFlags(EntityFlags.VISIBLE, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Read the shared ground grid once (readGround; else the cached one - TEST PRESS frames), then set every bone whose
	//! offset / slope changed
	protected static void Refresh(bool readGround)
	{
		int t0 = System.GetTickCount();
		s_iRefreshes++;
		s_bPressDirty = false;
		for (int gz = 0; gz < s_iGridN && readGround; gz++)
		{
			float fz = gz;
			float z = s_fGridZ0 + fz * s_fStep;
			for (int gx = 0; gx < s_iGridN; gx++)
			{
				float fx = gx;
				float y;
				s_iReads++;
				if (!GroundAt(s_fGridX0 + fx * s_fStep, z, y))
				{
					s_iReadFails++;
					y = s_fBaseY;
				}
				s_aGridH[gz * s_iGridN + gx] = y;
			}
		}
		bool force = s_bForce;
		s_bForce = false;
		foreach (MUD_BoneTileP tile : s_aTiles)
		{
			ApplyTile(tile, force);
		}
		s_iMs += System.GetTickCount() - t0;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ApplyTile(MUD_BoneTileP tile, bool force)
	{
		if (!tile.m_Ent)
			return;
		Animation anim = tile.m_Ent.GetAnimation();
		if (!anim)
			return;
		bool tilt = s_iMode == MODE_TILT;
		bool any;
		for (int b = 0; b < tile.m_aId.Count(); b++)
		{
			int id = tile.m_aId[b];
			int g = tile.m_aGrid[b];
			if (id < 0 || g < 0)
				continue;
			float h = H(g) + LIFT - s_fBaseY;
			if (h > BOX_Y || h < -BOX_Y)
			{
				s_iClamps++;
				h = Math.Clamp(h, -BOX_Y, BOX_Y);
			}
			float sx = 0;
			float sz = 0;
			if (tilt)
			{
				// central differences on the shared grid (same numbers on both sides of a seam)
				sx = (H(g + 1) - H(g - 1)) / (2 * s_fStep);
				sz = (H(g + s_iGridN) - H(g - s_iGridN)) / (2 * s_fStep);
			}
			if (!force && Math.AbsFloat(h - tile.m_aLastH[b]) < EPS_H && Math.AbsFloat(sx - tile.m_aLastSx[b]) < EPS_H && Math.AbsFloat(sz - tile.m_aLastSz[b]) < EPS_H)
				continue;
			tile.m_aLastH[b] = h;
			tile.m_aLastSx[b] = sx;
			tile.m_aLastSz[b] = sz;
			vector a0 = tile.m_aAx0[b];
			vector a1 = tile.m_aAx1[b];
			vector a2 = tile.m_aAx2[b];
			vector m[4];
			Math3D.MatrixIdentity4(m);
			if (tilt)
			{
				// model-space rotation that turns up into the ground normal: X along the x slope, Z along the z slope
				vector rx = Vector(1, sx, 0).Normalized();
				vector rz = Vector(0, sz, 1).Normalized();
				vector ry = Cross(rz, rx).Normalized();
				rz = Cross(rx, ry);
				// into the bone's own axes (as mode A's offset, proven 2026-10-23): row i = ToBone(R(rest row i))
				m[0] = ToBone(rx * a0[0] + ry * a0[1] + rz * a0[2], a0, a1, a2);
				m[1] = ToBone(rx * a1[0] + ry * a1[1] + rz * a1[2], a0, a1, a2);
				m[2] = ToBone(rx * a2[0] + ry * a2[1] + rz * a2[2], a0, a1, a2);
			}
			m[3] = ToBone(Vector(0, h, 0), a0, a1, a2);
			s_iSets++;
			any = true;
			if (!anim.SetBoneMatrix(tile.m_Ent, id, m))
			{
				s_iSetFails++;
				if (s_iSetFails == 1 || s_iSetFails == 1000)
					Print(string.Format("MUD bone probe: SetBoneMatrix false (bone %1, fails %2)", id, s_iSetFails), LogLevel.WARNING);
			}
		}
		if (any)
		{
			tile.m_Ent.Update();
			// plate block: ask physics to re-read the bone poses into the collider shapes (last try, v320)
			if (s_iTile == COL_TILE)
			{
				Physics ph = tile.m_Ent.GetPhysics();
				if (ph)
					ph.UpdateGeometries();
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void ClearPress()
	{
		for (int g = 0; g < s_aPress.Count(); g++)
		{
			s_aPress[g] = 0;
		}
		s_bPressDirty = false;
	}

	//------------------------------------------------------------------------------------------------
	//! TEST PRESS: stamp every wheel contact near the block and the local player's feet (every frame, so a moving
	//! wheel leaves a line: 10 m/s at 60 fps = 0.17 m per frame, under the stamp radius)
	protected static void PressTick()
	{
		float near2 = TEST_NEAR * TEST_NEAR;
		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh || vector.DistanceSqXZ(veh.GetOrigin(), s_vMid) > near2)
				continue;
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
			if (!sim || !sim.IsValid())
				continue;
			for (int w = 0; w < sim.WheelCount(); w++)
			{
				if (sim.WheelHasContact(w))
				{
					if (s_iTile == COL_TILE)
						Stamp(MUD_VoxelProbe.RutPoint(veh, sim, w), COL_R_WHEEL);
					else
						Stamp(MUD_VoxelProbe.RutPoint(veh, sim, w), TEST_R_WHEEL);
				}
			}
		}
		ChimeraCharacter ch = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (ch && !ch.IsInVehicle())
			Stamp(ch.GetOrigin(), TEST_R_FOOT);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stamp(vector p, float r)
	{
		if (Math.AbsFloat(p[1] - s_fBaseY) > 2)
			return;     // far above / below the block (roof, bridge)
		int gx0 = Math.Max(1, Math.Ceil((p[0] - r - s_fGridX0) / s_fStep));
		int gx1 = Math.Min(s_iGridN - 2, Math.Floor((p[0] + r - s_fGridX0) / s_fStep));
		int gz0 = Math.Max(1, Math.Ceil((p[2] - r - s_fGridZ0) / s_fStep));
		int gz1 = Math.Min(s_iGridN - 2, Math.Floor((p[2] + r - s_fGridZ0) / s_fStep));
		if (gx0 > gx1 || gz0 > gz1)
			return;
		s_iStamps++;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			float fz = gz;
			float dz = s_fGridZ0 + fz * s_fStep - p[2];
			for (int gx = gx0; gx <= gx1; gx++)
			{
				float fx = gx;
				float dx = s_fGridX0 + fx * s_fStep - p[0];
				float d = Math.Sqrt(dx * dx + dz * dz);
				if (d >= r)
					continue;
				float k = d / r;
				float depth = TEST_D;
				if (s_iTile == COL_TILE)
					depth = COL_D;
				float want = depth * (1 - k * k);     // soft round dip, as the S demo
				int g = gz * s_iGridN + gx;
				if (want > s_aPress[g] + EPS_H)
				{
					s_aPress[g] = want;
					s_bPressDirty = true;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Bone target height at shared grid point g (world): ground + plate block raise - TEST PRESS dip
	protected static float H(int g)
	{
		return s_aGridH[g] + s_aRaise[g] - s_aPress[g];
	}

	//------------------------------------------------------------------------------------------------
	//! Plate block: COL_RAISE inside, ramping from 0 at the block edge over COL_RAMP m; 0 for the other blocks
	protected static void MakeRaise()
	{
		s_aRaise.Clear();
		s_aRaise.Resize(s_iGridN * s_iGridN);
		for (int gz = 0; gz < s_iGridN; gz++)
		{
			for (int gx = 0; gx < s_iGridN; gx++)
			{
				float r = 0;
				if (s_iTile == COL_TILE)
				{
					int e = Math.Min(Math.Min(gx - 1, gz - 1), Math.Min(s_iGridN - 2 - gx, s_iGridN - 2 - gz));
					float ef = e;
					r = COL_RAISE * Math.Clamp(ef * s_fStep / COL_RAMP, 0, 1);
				}
				s_aRaise[gz * s_iGridN + gx] = r;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Plate block: trace straight down onto the block centre (only the centre tile) and compare with the bone there
	protected static void ColCheck()
	{
		int gc = (s_iGridN - 1) / 2;
		int g = gc * s_iGridN + gc;
		float fc = gc;
		vector at = Vector(s_fGridX0 + fc * s_fStep, 0, s_fGridZ0 + fc * s_fStep);
		IEntity mid;
		float best = 1000000;
		foreach (MUD_BoneTileP tile : s_aTiles)
		{
			if (!tile.m_Ent)
				continue;
			float d = vector.DistanceSqXZ(tile.m_vOrg, at);
			if (d < best)
			{
				best = d;
				mid = tile.m_Ent;
			}
		}
		if (!mid)
			return;
		float want = H(g) + LIFT;
		TraceParam tr = new TraceParam();
		tr.Include = mid;
		tr.Flags = TraceFlags.ENTS;
		tr.LayerMask = EPhysicsLayerPresets.Projectile;
		tr.Start = Vector(at[0], want + 3, at[2]);
		tr.End = Vector(at[0], want - 3, at[2]);
		float f = GetGame().GetWorld().TraceMove(tr, null);
		string verdict = "NO HIT (the tile has no collider the trace sees)";
		float hitY = 0;
		bool stayed;
		if (f < 1)
		{
			hitY = want + 3 - 6 * f;
			verdict = "STAYED (collider at the rest / spawn pose)";
			stayed = true;
			if (Math.AbsFloat(hitY - want) < 0.05)
			{
				verdict = "FOLLOWS (collider at the bone)";
				stayed = false;
			}
		}
		Print(string.Format("MUD bone col: body %1, centre bone y %2 (raise %3, dip %4), trace hit y %5, spawn plane y %6 -> %7",
			BodyName(s_iColBody), want, s_aRaise[g], s_aPress[g], hitY, s_fBaseY, verdict), LogLevel.NORMAL);
		if (stayed)
			s_iColStayed++;
		else
			s_iColStayed = 0;
		if (s_iColStayed >= 2 && s_iColBody < 1)
		{
			s_iColStayed = 0;
			MakeBodies(s_iColBody + 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static string BodyName(int mode)
	{
		if (mode == 0)
			return "STATIC";
		if (mode == 1)
			return "DYNAMIC COLLISION";
		return "DYNAMIC SIMULATION held";
	}

	//------------------------------------------------------------------------------------------------
	//! Plate block: (re)make every tile's body in this mode (0 static, 1 dynamic COLLISION, 2 dynamic SIMULATION held)
	protected static void MakeBodies(int mode)
	{
		s_iColBody = mode;
		int made;
		foreach (MUD_BoneTileP tile : s_aTiles)
		{
			if (!tile.m_Ent)
				continue;
			Physics old = tile.m_Ent.GetPhysics();
			if (old)
				old.Destroy();
			Physics ph;
			if (mode == 0)
			{
				ph = Physics.CreateStatic(tile.m_Ent);
			}
			else
			{
				ph = Physics.CreateDynamic(tile.m_Ent, COL_MASS);
				if (ph)
				{
					ph.EnableGravity(false);
					ph.SetVelocity(vector.Zero);
					ph.SetAngularVelocity(vector.Zero);
					if (mode == 1)
						ph.ChangeSimulationState(SimulationState.COLLISION);
					else
						ph.ChangeSimulationState(SimulationState.SIMULATION);
				}
			}
			if (ph)
			{
				made++;
				ph.UpdateGeometries(); // the bones are already raised - read them in once (v320)
			}
		}
		Print(string.Format("MUD bone col: %1 tiles, %2 got a %3 body (+ UpdateGeometries), block %4 m, raise %5 m", s_aTiles.Count(), made, BodyName(mode), s_iBlock * SIZE, COL_RAISE), LogLevel.NORMAL);
		SCR_HintManagerComponent.ShowCustomHint("plate body: " + BodyName(mode), "Mud bone probe", 4, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Mode 2: a simulated body is pushed by what hits it - put every tile back and stop it, every frame
	protected static void HoldBodies()
	{
		foreach (MUD_BoneTileP tile : s_aTiles)
		{
			if (!tile.m_Ent)
				continue;
			Physics ph = tile.m_Ent.GetPhysics();
			if (!ph)
				continue;
			ph.SetVelocity(vector.Zero);
			ph.SetAngularVelocity(vector.Zero);
			if (vector.DistanceSq(tile.m_Ent.GetOrigin(), tile.m_vOrg) > 0.000001)
			{
				vector m[4];
				Math3D.MatrixIdentity4(m);
				m[3] = tile.m_vOrg;
				tile.m_Ent.SetWorldTransform(m);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Model-space direction into the bone's rest axes (mode A of the first probe)
	static vector ToBone(vector v, vector a0, vector a1, vector a2)
	{
		return Vector(vector.Dot(v, a0), vector.Dot(v, a1), vector.Dot(v, a2));
	}

	//------------------------------------------------------------------------------------------------
	static vector Cross(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}
}
