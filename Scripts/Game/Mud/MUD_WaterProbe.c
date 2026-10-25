//! WATER BONE PROBE (TEST_KIT, user 2026-10-23: 'a water effect in the bone probe menu - reactive waves on the ocean
//! too, waves we make that other things react to, in a variable amount'). OUTSIDE THE FEATURE FREEZE: a throwaway look
//! probe - archive it after the test unless the user keeps it.
//! A TILES x TILES block of the 4 m bone tile (BoneTile15, 15 x 15 bones) with a water material (Prefabs/Mud/
//! MudWaterTile15.et = vanilla PondSurface_01 MatWaterPool; MudWaterTile15Glass.et = our MatPBRBasicGlass on the
//! character glass base, a skinned-safe fallback if MatWaterPool does not draw on a skinned mesh - UNPROVEN either way).
//! Every bone is one point of a ripple grid (wave equation, speed C, damped, soaked up at the edge): characters and
//! vehicles (boats too) in the block push it by their speed, so wakes spread and add up; numpad 4 drops a splash where
//! the camera looks. The block rides the real ocean waves (GetOceanHeight per bone) OCEAN_OVER above them.
//! OCEAN ONLY (user 2026-10-23 after the land test: 'only at the ocean ... react to me driving in it or swimming or
//! walking in it'): picking it in the menu only ARMS it. The block appears once the local player (or the vehicle
//! they sit in) is over sea at least SEA_DEPTH deep, centred on them, follows them in whole grid steps (ripples
//! shift along) and goes away LEAVE_SEC after they are back on land. Its edge ring sinks EDGE_SINK under the real
//! surface so the square border hides under the sea.
//! Level (numpad 2): CALM / GENTLE / CHOPPY / STORM = extra swell on top of the real waves + how hard things push.
//! Look only: local entities, no collider, nothing synced, never a DS. Picked in MUD_BoneProbe's numpad 1 cycle.
//! Log every 5 s: 'MUD water probe: ...'.
class MUD_WaterTile
{
	IEntity m_Ent;
	float m_fY;                                 // entity origin y
	ref array<int> m_aId = {};                  // node id per grid bone, -1 = missing
	ref array<int> m_aGrid = {};                // grid point per bone, -1 = off grid
	ref array<vector> m_aAx0 = {};              // rest matrix rows (model space) per bone
	ref array<vector> m_aAx1 = {};
	ref array<vector> m_aAx2 = {};
	ref array<float> m_aLastH = {};
	ref array<float> m_aLastSx = {};
	ref array<float> m_aLastSz = {};
}

class MUD_WaterProbe
{
	static const ResourceName PREFAB_POND = "{4D55445741540015}Prefabs/Mud/MudWaterTile15.et";
	static const ResourceName PREFAB_GLASS = "{4D55445741570015}Prefabs/Mud/MudWaterTile15Glass.et";
	static const float SIZE = 4;            // tile side, m
	static const int SIDE = 15;             // bones per tile side
	static const int TILES = 3;             // block = TILES x TILES tiles (12 m; 2025 bones)
	static const float AHEAD = 8;           // numpad 4 splash this far in front when the camera looks up
	static const float OCEAN_OVER = 0.03;   // this far over the real (waving) ocean surface
	static const float SEA_DEPTH = 0.2;     // the player counts as 'at sea' where the ground is this far under the ocean base
	static const float CHECK_EVERY = 0.25;  // s between 'where is the player' checks
	static const float RECENTER = 2;        // m off the block middle before the block steps after the player
	static const float LEAVE_SEC = 2;       // s on land before the block goes away
	static const float EDGE_SINK = 0.15;    // m the edge ring (SPONGE) dips under the real surface
	static const float BOX_Y = 1.45;        // bone offset clamp (prefab BoundingBox is +-1.5 m)
	static const float C = 4;               // ripple speed, m/s (stable up to ~0.7 * step / dt = 6 at 30 fps)
	static const float DAMP = 0.5;          // ripple speed loss per s
	static const int SPONGE = 5;            // grid points at the block edge that soak ripples up
	static const float SPONGE_K = 6;        // ... extra loss per s at the very edge
	static const float DT_MAX = 0.033;
	static const float DRAW_EVERY = 0.033;  // s between bone passes
	static const float QUERY_EVERY = 0.1;   // s between entity scans (speed = position change)
	static const float NEAR_Y = 2.5;        // things with their origin within this below the water (0.6 above) push it
	static const float R_CHAR = 0.4;
	static const float K_CHAR = 1.5;        // ripple push per m/s of speed (m/s per s)
	static const float K_VEH = 1.0;
	static const float K_FALL = 2.0;        // ... per m/s of vertical speed (falling / jumping in)
	static const float SPLASH_R = 0.9;
	static const float SPLASH_V = 3;        // numpad 4 splash: ripple speed kick at the middle, m/s (x level gain)
	static const float EPS = 0.002;
	static const int LEVELS = 4;

	protected static ref MUD_WaterProbe s_Inst;
	protected static int s_iLevel = 1;
	protected static float s_fLastT = -1;

	protected ref array<ref MUD_WaterTile> m_aTiles = {};
	protected ref array<float> m_aH = {};       // ripple height per grid point
	protected ref array<float> m_aV = {};       // ripple vertical speed
	protected ref array<float> m_aDraw = {};    // drawn surface y per grid point
	protected ref array<float> m_aSponge = {};  // extra loss per s per grid point (edge)
	protected ref array<float> m_aWave = {};    // ambient swell: per wave kx, kz, omega, share
	protected ref array<IEntity> m_aFound = {};
	protected ref map<IEntity, vector> m_mLast = new map<IEntity, vector>();
	protected ref array<vector> m_aSrcP = {};   // sources of this scan: position, velocity, radius, push
	protected ref array<vector> m_aSrcV = {};
	protected ref array<float> m_aSrcR = {};
	protected ref array<float> m_aSrcK = {};
	protected int m_iN;                         // grid points per side (block bones + 1 border each side)
	protected float m_fX0;
	protected float m_fZ0;
	protected float m_fStep;
	protected float m_fY;                       // ocean base + OCEAN_OVER
	protected float m_fMidX;                    // block middle
	protected float m_fMidZ;
	protected int m_iLook;
	protected float m_fNextCheck;
	protected float m_fDryT = -1;               // time the player was first seen back on land, -1 = at sea
	protected int m_iShifts;
	protected float m_fT = -1;
	protected float m_fQueryT = -1;
	protected float m_fNextDraw;
	protected float m_fNextQuery;
	protected float m_fNextStat;
	protected int m_iSets;
	protected int m_iClamps;
	protected int m_iSrc;
	protected int m_iSplashes;
	protected int m_iMs;

	//------------------------------------------------------------------------------------------------
	static bool IsOn()
	{
		return s_Inst != null;
	}

	//------------------------------------------------------------------------------------------------
	//! look 0 = pond water material, 1 = glass fallback. Arms the probe; the block appears at sea (DoTick).
	static void Start(int look)
	{
		Stop();
		if (RplSession.Mode() == RplMode.Dedicated)
			return;
		s_Inst = new MUD_WaterProbe();
		s_Inst.m_iLook = look;
		Print(string.Format("MUD water probe: armed (look %1) - shows once you are at sea, ocean in this world %2", look, GetGame().GetWorld().IsOcean()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static void Stop()
	{
		if (s_Inst)
			s_Inst.Clear();
		s_Inst = null;
	}

	//------------------------------------------------------------------------------------------------
	static void NextLevel()
	{
		s_iLevel = (s_iLevel + 1) % LEVELS;
	}

	//------------------------------------------------------------------------------------------------
	static void Splash()
	{
		if (s_Inst && !s_Inst.m_aTiles.IsEmpty())     // armed but not at sea: no grid yet
			s_Inst.DoSplash();
	}

	//------------------------------------------------------------------------------------------------
	//! Hint / log text of the probe state
	static string Text()
	{
		if (!s_Inst)
			return "WATER off";
		string look = "pond water material";
		if (s_Inst.m_iLook == 1)
			look = "glass fallback material";
		string where = "WAITING - go out to sea";
		if (!s_Inst.m_aTiles.IsEmpty())
			where = "on the OCEAN, follows you";
		return string.Format("WATER %1 x %1 tiles, %2, %3 | level %4 (numpad 2) | numpad 4 = splash", TILES, look, where, LevelName(s_iLevel));
	}

	//------------------------------------------------------------------------------------------------
	static string LevelName(int l)
	{
		if (l == 0)
			return "CALM";
		if (l == 1)
			return "GENTLE";
		if (l == 2)
			return "CHOPPY";
		return "STORM";
	}

	//------------------------------------------------------------------------------------------------
	//! Ambient swell height (m) of level l
	protected static float LevelAmb(int l)
	{
		if (l == 0)
			return 0;
		if (l == 1)
			return 0.04;
		if (l == 2)
			return 0.12;
		return 0.3;
	}

	//------------------------------------------------------------------------------------------------
	//! How hard things push the ripples at level l
	protected static float LevelGain(int l)
	{
		if (l == 0)
			return 0.6;
		if (l == 1)
			return 1;
		if (l == 2)
			return 1.6;
		return 2.5;
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame from MUD_BoneProbe.Tick
	static void Tick(float time)
	{
		if (time < s_fLastT)
			Stop();     // new play: the tiles of the old world are gone
		s_fLastT = time;
		if (s_Inst)
			s_Inst.DoTick(time);
	}

	//------------------------------------------------------------------------------------------------
	protected void Clear()
	{
		foreach (MUD_WaterTile tile : m_aTiles)
		{
			if (tile.m_Ent && !tile.m_Ent.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(tile.m_Ent);
		}
		m_aTiles.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Our ground (as the patches draw it) at x, z; the camera height - 1.7 if unknown
	protected float GroundY(float x, float z, vector camPos)
	{
		float dug;
		float undug;
		if (MUD_WorldGroundEntity.DugUndug(x, z, dug, undug))
			return dug;
		float y = GetGame().GetWorld().GetSurfaceY(x, z);
		if (y > -200)
			return y;
		return camPos[1] - 1.7;     // holed cell with no patch answer
	}

	//------------------------------------------------------------------------------------------------
	//! The local player's vehicle if they sit in one, else the player; null with no player
	protected static IEntity Anchor()
	{
		ChimeraCharacter ch = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!ch)
			return null;
		if (ch.IsInVehicle())
		{
			IEntity veh = CompartmentAccessComponent.GetVehicleIn(ch);
			if (veh)
				return veh;
		}
		return ch;
	}

	//------------------------------------------------------------------------------------------------
	//! True = sea at least SEA_DEPTH deep under p (anchor origin). Ground = ours (most real terrain is holed);
	//! with no answer, the anchor's own height stands in (feet / wheels / a swimmer under the surface)
	protected bool AtSea(BaseWorld world, vector p)
	{
		if (!world.IsOcean())
			return false;
		return GroundY(p[0], p[2], p + Vector(0, 1.7, 0)) < world.GetOceanBaseHeight() - SEA_DEPTH;
	}

	//------------------------------------------------------------------------------------------------
	//! Every CHECK_EVERY s: show the block at sea, step it after the player, take it away on land
	protected void Follow(float time)
	{
		BaseWorld world = GetGame().GetWorld();
		IEntity a = Anchor();
		if (!a)
			return;
		vector p = a.GetOrigin();
		if (!AtSea(world, p))
		{
			if (m_aTiles.IsEmpty())
				return;
			if (m_fDryT < 0)
				m_fDryT = time;
			else if (time - m_fDryT > LEAVE_SEC)
			{
				Clear();
				Print("MUD water probe: back on land - block gone (still armed)", LogLevel.NORMAL);
			}
			return;
		}
		m_fDryT = -1;
		if (m_aTiles.IsEmpty())
		{
			if (!BuildAt(p))
				Clear();
			return;
		}
		int sx = Math.Round((p[0] - m_fMidX) / m_fStep);
		int sz = Math.Round((p[2] - m_fMidZ) / m_fStep);
		float fsx = sx;
		float fsz = sz;
		if (Math.AbsFloat(fsx * m_fStep) > RECENTER || Math.AbsFloat(fsz * m_fStep) > RECENTER)
			Shift(sx, sz);
	}

	//------------------------------------------------------------------------------------------------
	//! Move the block sx, sz grid steps: the ripples go along (new ground calm), the tiles move by the same metres,
	//! so every bone keeps its grid point
	protected void Shift(int sx, int sz)
	{
		int n = m_iN;
		array<float> h = {};
		array<float> v = {};
		h.Resize(n * n);
		v.Resize(n * n);
		for (int gz = 0; gz < n; gz++)
		{
			for (int gx = 0; gx < n; gx++)
			{
				int ox = gx + sx;
				int oz = gz + sz;
				int g = gz * n + gx;
				h[g] = 0;
				v[g] = 0;
				if (ox >= 0 && ox < n && oz >= 0 && oz < n)
				{
					h[g] = m_aH[oz * n + ox];
					v[g] = m_aV[oz * n + ox];
				}
			}
		}
		for (int c = 0; c < n * n; c++)
		{
			m_aH[c] = h[c];
			m_aV[c] = v[c];
		}
		float fsx = sx;
		float fsz = sz;
		vector d = Vector(fsx * m_fStep, 0, fsz * m_fStep);
		m_fX0 += d[0];
		m_fZ0 += d[2];
		m_fMidX += d[0];
		m_fMidZ += d[2];
		foreach (MUD_WaterTile tile : m_aTiles)
		{
			if (!tile.m_Ent)
				continue;
			tile.m_Ent.SetOrigin(tile.m_Ent.GetOrigin() + d);
			tile.m_Ent.Update();
		}
		m_iShifts++;
	}

	//------------------------------------------------------------------------------------------------
	protected bool BuildAt(vector mid)
	{
		int look = m_iLook;
		BaseWorld world = GetGame().GetWorld();
		float half = TILES * SIZE * 0.5;
		float sidef = SIDE - 1;
		m_fStep = SIZE / sidef;
		m_iN = TILES * (SIDE - 1) + 1 + 2;
		m_fMidX = mid[0];
		m_fMidZ = mid[2];
		m_fX0 = mid[0] - half - m_fStep;
		m_fZ0 = mid[2] - half - m_fStep;
		float ground = GroundY(mid[0], mid[2], mid + Vector(0, 1.7, 0));
		m_fY = world.GetOceanBaseHeight() + OCEAN_OVER;
		m_mLast.Clear();
		m_fT = -1;

		int n = m_iN * m_iN;
		m_aH.Resize(n);
		m_aV.Resize(n);
		m_aDraw.Resize(n);
		m_aSponge.Resize(n);
		for (int g = 0; g < n; g++)
		{
			m_aH[g] = 0;
			m_aV[g] = 0;
			m_aDraw[g] = m_fY;
			int gz = g / m_iN;
			int gx = g - gz * m_iN;
			int d = Math.Min(Math.Min(gx, gz), Math.Min(m_iN - 1 - gx, m_iN - 1 - gz));
			m_aSponge[g] = 0;
			if (d < SPONGE)
			{
				float fd = d;
				float t = 1 - fd / SPONGE;
				m_aSponge[g] = SPONGE_K * t * t;
			}
		}
		// ambient swell: 3 waves, deep-water speed (omega = sqrt(g k)), shares 1 / 0.45 / 0.2
		m_aWave.Clear();
		AddWave(1, 0.3, 7, 1);
		AddWave(-0.4, 1, 3.1, 0.45);
		AddWave(0.8, -0.7, 1.6, 0.2);

		ResourceName prefab = PREFAB_POND;
		if (look == 1)
			prefab = PREFAB_GLASS;
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print(string.Format("MUD water probe: prefab '%1' did not load", prefab), LogLevel.ERROR);
			return false;
		}
		for (int tz = 0; tz < TILES; tz++)
		{
			for (int tx = 0; tx < TILES; tx++)
			{
				float ftx = tx;
				float ftz = tz;
				vector org = Vector(mid[0] - half + SIZE * (ftx + 0.5), m_fY, mid[2] - half + SIZE * (ftz + 0.5));
				MUD_WaterTile tile = SpawnTile(res, world, org);
				if (tile)
					m_aTiles.Insert(tile);
			}
		}
		if (m_aTiles.IsEmpty())
			return false;
		Print(string.Format("MUD water probe: at sea - %1 tiles at %2, water y %3 (ground %4, ocean base %5), step %6 m, grid %7 x %7",
			m_aTiles.Count(), mid, m_fY, ground, world.GetOceanBaseHeight(), m_fStep, m_iN), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddWave(float dx, float dz, float len, float share)
	{
		float l = Math.Sqrt(dx * dx + dz * dz);
		float k = Math.PI2 / len;
		m_aWave.Insert(dx / l * k);
		m_aWave.Insert(dz / l * k);
		m_aWave.Insert(Math.Sqrt(9.81 * k));
		m_aWave.Insert(share);
	}

	//------------------------------------------------------------------------------------------------
	protected MUD_WaterTile SpawnTile(Resource res, BaseWorld world, vector org)
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
			Print("MUD water probe: SpawnEntityPrefabLocal returned null", LogLevel.ERROR);
			return null;
		}
		Animation anim = ent.GetAnimation();
		if (!anim)
		{
			Print("MUD water probe: GetAnimation() is NULL on the water tile - no bones to drive", LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(ent);
			return null;
		}
		MUD_WaterTile tile = new MUD_WaterTile();
		tile.m_Ent = ent;
		tile.m_fY = org[1];
		int found;
		int misfit;
		for (int j = 0; j < SIDE; j++)
		{
			for (int i = 0; i < SIDE; i++)
			{
				int id = anim.GetBoneIndex(MUD_BoneProbe.BoneName(i, j));
				vector m[4];
				Math3D.MatrixIdentity4(m);
				int g = -1;
				if (id >= 0 && anim.GetBoneMatrix(id, m))
				{
					found++;
					vector p = m[3];
					float gxf = (org[0] + p[0] - m_fX0) / m_fStep;
					float gzf = (org[2] + p[2] - m_fZ0) / m_fStep;
					int gx = Math.Round(gxf);
					int gz = Math.Round(gzf);
					if (Math.AbsFloat(gxf - gx) > 0.02 || Math.AbsFloat(gzf - gz) > 0.02 || gx < 1 || gz < 1 || gx > m_iN - 2 || gz > m_iN - 2)
						misfit++;
					else
						g = gz * m_iN + gx;
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
		if (misfit > 0 || found < SIDE * SIDE)
			Print(string.Format("MUD water probe: tile at %1 | bones found %2 of %3, off-grid %4", org, found, SIDE * SIDE, misfit), LogLevel.WARNING);
		return tile;
	}

	//------------------------------------------------------------------------------------------------
	protected void DoTick(float time)
	{
		if (time >= m_fNextCheck)
		{
			m_fNextCheck = time + CHECK_EVERY;
			Follow(time);
		}
		if (m_aTiles.IsEmpty())
			return;
		if (m_fT < 0)
		{
			m_fT = time;
			m_fNextStat = time + 5;
			return;
		}
		float dt = time - m_fT;
		m_fT = time;
		if (dt <= 0)
			return;
		if (dt > DT_MAX)
			dt = DT_MAX;
		int t0 = System.GetTickCount();
		if (time >= m_fNextQuery)
		{
			m_fNextQuery = time + QUERY_EVERY;
			Query(time);
		}
		float gain = LevelGain(s_iLevel);
		for (int s = 0; s < m_aSrcP.Count(); s++)
		{
			vector v = m_aSrcV[s];
			float speed = Math.Sqrt(v[0] * v[0] + v[2] * v[2]);
			float push = (speed * m_aSrcK[s] + Math.AbsFloat(v[1]) * K_FALL) * gain;
			if (push < 0.05)
				continue;     // standing still: no ripple
			Disc(m_aSrcP[s], m_aSrcR[s], -push * dt);
		}
		Step(dt);
		if (time >= m_fNextDraw)
		{
			m_fNextDraw = time + DRAW_EVERY;
			Draw(time);
		}
		m_iMs += System.GetTickCount(t0);
		if (time >= m_fNextStat)
		{
			m_fNextStat = time + 5;
			Print(string.Format("MUD water probe: 5 s | level %1, follow steps %2, sources now %3, source scans with any %4, splashes %5, bone sets %6, clamps %7, %8 ms (WB, not perf)",
				LevelName(s_iLevel), m_iShifts, m_aSrcP.Count(), m_iSrc, m_iSplashes, m_iSets, m_iClamps, m_iMs), LogLevel.NORMAL);
			m_iShifts = 0;
			m_iSrc = 0;
			m_iSplashes = 0;
			m_iSets = 0;
			m_iClamps = 0;
			m_iMs = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddFound(IEntity e)
	{
		ChimeraCharacter ch = ChimeraCharacter.Cast(e);
		if (ch && !ch.IsInVehicle())
			m_aFound.Insert(e);
		else if (Vehicle.Cast(e))
			m_aFound.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Characters and vehicles over the block: position + speed (from the last scan) as ripple sources
	protected void Query(float time)
	{
		m_aFound.Clear();
		float w = m_fStep * (m_iN - 1);
		GetGame().GetWorld().QueryEntitiesByAABB(Vector(m_fX0 - 2, m_fY - NEAR_Y, m_fZ0 - 2), Vector(m_fX0 + w + 2, m_fY + 2, m_fZ0 + w + 2), AddFound);
		float qdt = time - m_fQueryT;
		m_fQueryT = time;
		m_aSrcP.Clear();
		m_aSrcV.Clear();
		m_aSrcR.Clear();
		m_aSrcK.Clear();
		if (m_mLast.Count() > 200)
			m_mLast.Clear();
		foreach (IEntity e : m_aFound)
		{
			vector o = e.GetOrigin();
			if (o[1] < m_fY - NEAR_Y || o[1] > m_fY + 0.6)
				continue;
			vector last;
			vector vel = vector.Zero;
			if (m_mLast.Find(e, last) && qdt > 0 && qdt < 1)
				vel = (o - last) * (1 / qdt);
			m_mLast.Set(e, o);
			float r = R_CHAR;
			float k = K_CHAR;
			if (Vehicle.Cast(e))
			{
				vector bmin;
				vector bmax;
				e.GetWorldBounds(bmin, bmax);
				r = Math.Clamp(0.5 * Math.Min(bmax[0] - bmin[0], bmax[2] - bmin[2]), 0.6, 2.5);
				k = K_VEH;
			}
			m_aSrcP.Insert(o);
			m_aSrcV.Insert(vel);
			m_aSrcR.Insert(r);
			m_aSrcK.Insert(k);
		}
		if (!m_aSrcP.IsEmpty())
			m_iSrc++;
	}

	//------------------------------------------------------------------------------------------------
	//! Ripple speed kick dv (m/s) over a soft disc of radius r at world p
	protected void Disc(vector p, float r, float dv)
	{
		int gx0 = Math.Max(1, Math.Ceil((p[0] - r - m_fX0) / m_fStep));
		int gx1 = Math.Min(m_iN - 2, Math.Floor((p[0] + r - m_fX0) / m_fStep));
		int gz0 = Math.Max(1, Math.Ceil((p[2] - r - m_fZ0) / m_fStep));
		int gz1 = Math.Min(m_iN - 2, Math.Floor((p[2] + r - m_fZ0) / m_fStep));
		for (int gz = gz0; gz <= gz1; gz++)
		{
			float fz = gz;
			float dz = m_fZ0 + fz * m_fStep - p[2];
			for (int gx = gx0; gx <= gx1; gx++)
			{
				float fx = gx;
				float dx = m_fX0 + fx * m_fStep - p[0];
				float d2 = (dx * dx + dz * dz) / (r * r);
				if (d2 >= 1)
					continue;
				int g = gz * m_iN + gx;
				m_aV[g] = m_aV[g] + dv * (1 - d2);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! numpad 4: a splash where the camera looks at the water plane (else AHEAD m in front)
	protected void DoSplash()
	{
		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		vector c = cam[3];
		vector f = cam[2];
		vector hit;
		if (f[1] < -0.05)
		{
			float t = (m_fY - c[1]) / f[1];
			hit = c + f * t;
		}
		else
		{
			f[1] = 0;
			f.Normalize();
			hit = c + f * AHEAD;
		}
		Disc(hit, SPLASH_R, -SPLASH_V * LevelGain(s_iLevel));
		m_iSplashes++;
	}

	//------------------------------------------------------------------------------------------------
	//! One wave-equation step: speed from the neighbours' pull, damped (more at the edge), then height
	protected void Step(float dt)
	{
		int n = m_iN;
		float k = C * C / (m_fStep * m_fStep) * dt;
		for (int gz = 1; gz < n - 1; gz++)
		{
			for (int gx = 1; gx < n - 1; gx++)
			{
				int g = gz * n + gx;
				float lap = m_aH[g - 1] + m_aH[g + 1] + m_aH[g - n] + m_aH[g + n] - 4 * m_aH[g];
				float keep = Math.Max(0, 1 - (DAMP + m_aSponge[g]) * dt);
				m_aV[g] = (m_aV[g] + k * lap) * keep;
			}
		}
		for (int hz = 1; hz < n - 1; hz++)
		{
			for (int hx = 1; hx < n - 1; hx++)
			{
				int h = hz * n + hx;
				m_aH[h] = Math.Clamp(m_aH[h] + m_aV[h] * dt, -1, 1);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Surface y per grid point (real ocean + swell + ripple, the edge ring sunk), then every bone that moved
	protected void Draw(float time)
	{
		BaseWorld world = GetGame().GetWorld();
		float amp = LevelAmb(s_iLevel);
		int nw = m_aWave.Count() / 4;
		for (int g = 0; g < m_iN * m_iN; g++)
		{
			int gz = g / m_iN;
			int gx = g - gz * m_iN;
			float fx = gx;
			float fz = gz;
			float x = m_fX0 + fx * m_fStep;
			float z = m_fZ0 + fz * m_fStep;
			float y = world.GetOceanHeight(x, z) + OCEAN_OVER;
			int d = Math.Min(Math.Min(gx, gz), Math.Min(m_iN - 1 - gx, m_iN - 1 - gz));
			if (d < SPONGE)
			{
				float fd = d;
				y -= (OCEAN_OVER + EDGE_SINK) * (1 - fd / SPONGE);     // edge ring under the real sea: no square border
			}
			for (int w = 0; w < nw && amp > 0; w++)
			{
				int b = w * 4;
				y += amp * m_aWave[b + 3] * Math.Sin(m_aWave[b] * x + m_aWave[b + 1] * z - m_aWave[b + 2] * time);
			}
			m_aDraw[g] = y + m_aH[g];
		}
		foreach (MUD_WaterTile tile : m_aTiles)
		{
			ApplyTile(tile);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Bones to the drawn surface, turned to its slope (as MUD_BoneProbe TILT)
	protected void ApplyTile(MUD_WaterTile tile)
	{
		if (!tile.m_Ent)
			return;
		Animation anim = tile.m_Ent.GetAnimation();
		if (!anim)
			return;
		bool any;
		for (int b = 0; b < tile.m_aId.Count(); b++)
		{
			int id = tile.m_aId[b];
			int g = tile.m_aGrid[b];
			if (id < 0 || g < 0)
				continue;
			float h = m_aDraw[g] - tile.m_fY;
			if (h > BOX_Y || h < -BOX_Y)
			{
				m_iClamps++;
				h = Math.Clamp(h, -BOX_Y, BOX_Y);
			}
			float sx = (m_aDraw[g + 1] - m_aDraw[g - 1]) / (2 * m_fStep);
			float sz = (m_aDraw[g + m_iN] - m_aDraw[g - m_iN]) / (2 * m_fStep);
			if (Math.AbsFloat(h - tile.m_aLastH[b]) < EPS && Math.AbsFloat(sx - tile.m_aLastSx[b]) < EPS && Math.AbsFloat(sz - tile.m_aLastSz[b]) < EPS)
				continue;
			tile.m_aLastH[b] = h;
			tile.m_aLastSx[b] = sx;
			tile.m_aLastSz[b] = sz;
			vector a0 = tile.m_aAx0[b];
			vector a1 = tile.m_aAx1[b];
			vector a2 = tile.m_aAx2[b];
			vector rx = Vector(1, sx, 0).Normalized();
			vector rz = Vector(0, sz, 1).Normalized();
			vector ry = MUD_BoneProbe.Cross(rz, rx).Normalized();
			rz = MUD_BoneProbe.Cross(rx, ry);
			vector m[4];
			m[0] = MUD_BoneProbe.ToBone(rx * a0[0] + ry * a0[1] + rz * a0[2], a0, a1, a2);
			m[1] = MUD_BoneProbe.ToBone(rx * a1[0] + ry * a1[1] + rz * a1[2], a0, a1, a2);
			m[2] = MUD_BoneProbe.ToBone(rx * a2[0] + ry * a2[1] + rz * a2[2], a0, a1, a2);
			m[3] = MUD_BoneProbe.ToBone(Vector(0, h, 0), a0, a1, a2);
			m_iSets++;
			any = true;
			anim.SetBoneMatrix(tile.m_Ent, id, m);
		}
		if (any)
			tile.m_Ent.Update();
	}
}
