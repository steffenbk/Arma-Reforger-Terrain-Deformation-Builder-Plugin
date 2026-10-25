//! F2 / F3 (user 2026-10-24, feature freeze lifted): helicopters on our ground. Server only (or no Rpl), ticked by
//! MUD_WorldGroundEntity after Wheels. Plan + status: docs/features/vehicle-ground/README.md.
//! GEAR = the landing gear bones the vanilla prefabs name in their LandingGear config (pak 2026-10-24: UH-1H
//!   LandingGearSkid v_ground_contact_L01 / L02 / R01 / R02, Mi-8 LandingGearWheel v_wheel_F r 0.3, v_wheel_bL / bR
//!   r 0.45). WB 2026-10-24: GetBoneIndex finds none of them -> GEAR TRACE, then GEAR FROM BOUNDS (see below).
//! F2 HELI MARKS (round 3, LANDING OVERKILL below): heightfield cuts, not rut ops (pass 1 on grass = wear only, nothing
//!   to see). A gear point within TOUCH m of our ground touches; touching gear that slides carves; a touchdown after
//!   AIR_S stamps the gear in (lips beside it) and scuffs a broken downwash dirt ring round the body.
//! F3 HELI CRASH: a heli coming down faster than CRASH_VS that loses CRASH_DV of that in one frame with a bounds corner
//!   within CRASH_NEAR m of our ground digs an impact crater at that corner: kinetic energy (down speed + HS_SHARE of
//!   the flat speed) as TNT kg x CRASH_KG_MULT (MUD_WorldGroundEntity.ImpactCrater -> the crater path, min charge
//!   0.3 kg). Then GOUGE_S s of CRASH SCRAPE (heightfield cut + jagged berms) under the sliding body.
//! The bounce / jitter of wrecks and parked vehicles: MUD_Settle (end of this file).
class MUD_HeliGear
{
	Vehicle m_Veh;                          // weak: a deleted heli reads null
	ref array<vector> m_aPts = {};          // vehicle-local gear contact points (bone pos, wheel: minus its radius)
	ref array<float> m_aRad = {};           // per point: wheel radius (0 = skid contact)
	ref array<int> m_aWheel = {};           // per point: 1 = a wheel (set-down dent), 0 = a skid end
	ref array<vector> m_aBody = {};         // vehicle-local body box face points (crash scrape)
	float m_fScrapeAt = -100;               // m_fTime of the last crash scrape step
	float m_fSlideAt = -100;                // m_fTime of the last gear slide cut
	float m_fLength;                        // m: bounds length (downwash ring size)
	vector m_vCentre;                       // vehicle-local bounds centre (downwash ring centre)
	vector m_vBottom;                       // vehicle-local bounds bottom centre (wash height)
	VehicleHelicopterSimulation m_Sim;
	vector m_vWashAt;                       // wash centre (world x, z) the wash time belongs to
	float m_fWashT;                         // s of wash built up at m_vWashAt (scaled by height)
	float m_fWashStepAt = -100;             // m_fTime of the last wash step
	float m_fWashCarry;                     // wash points owed (fractions of a point carry over)
	bool m_bWashLogged;
	ref array<int> m_aSkidA = {};           // skid pairs: front / rear index into m_aPts
	ref array<int> m_aSkidB = {};
	bool m_bMeasured;
	bool m_bTouch;                          // any gear point touched last tick
	float m_fAirSince = -100;               // m_fTime the gear last left the ground
	float m_fLastDown;                      // down speed (m/s, > 0 = falling) last tick
	float m_fCrashAt = -100;                // m_fTime of the last impact crater
	float m_fMassAtCrash;
	vector m_aCorner[8];                    // vehicle-local bounds corners (bottom 4 first)
}

class MUD_HeliGround
{
	static const bool ENABLED = true;
	static const bool RUTS = true;                  // F2
	static const bool CRASH = true;                 // F3 impact crater
	static const bool GOUGE = true;                 // F3 crash scrape after the impact
	static const float TOUCH = 0.12;                // m: gear point this close over our ground (or under it) touches
	static const float SUNK = 1.0;                  // m: deeper than this under our ground = on something else / bad data
	static const float AIR_S = 2;                   // s in the air before a touchdown stamps the gear + dust ring
	static const float CRASH_VS = 6;                // m/s down speed before the hit
	static const float CRASH_DV = 4;                // m/s of it lost in one frame = a hit, not a soft landing
	static const float CRASH_NEAR = 1.5;            // m: lowest bounds corner this close to our ground
	static const float HS_SHARE = 0.3;              // share of the flat speed that goes into the ground
	static const float CRASH_KG_MULT = 8;           // TNT kg = kinetic energy / 4.184 MJ x this. The crater min is 0.3 kg: straight down,
	                                                // UH-1 2365 kg 10 m/s 0.23 kg (none), 12 m/s 0.33; Mi-8 9000 kg 6 m/s 0.31, 10 m/s 0.86
	// IMPACT GOUGE (user 2026-10-24: 'the crater doesnt look like something massive hit, just an explosion'): the
	// hit digs a furrow (MUD_WorldGroundEntity.ImpactGouge) instead of the blast crater (CRASH_CRATER false). Depth
	// = GOUGE_K x sqrt(kinetic energy in MJ), GOUGE_MIN_D .. GOUGE_MAX_D (UH-1 2.4 t at 33 m/s 1.3 MJ -> 0.82 m);
	// length = the flat speed x GOUGE_PER_MS, at least GOUGE_MIN_BODY x the body length (a near-vertical hit = a
	// body-shaped pit along the fuselage), at most GOUGE_MAX_L; width = the body width x GOUGE_W.
	static const bool CRASH_CRATER = false;
	static const float GOUGE_K = 0.72;
	static const float GOUGE_MIN_D = 0.35;
	static const float GOUGE_MAX_D = 1.6;
	static const float GOUGE_PER_MS = 0.3;
	static const float GOUGE_MIN_BODY = 0.5;
	static const float GOUGE_MAX_L = 16;
	static const float GOUGE_W = 1.2;
	static const float CRASH_AGAIN_S = 5;           // s before the same heli can dig another impact crater
	// CRASH SCRAPE (user WB 2026-10-24 + sketch: 'not just a crater but a scraping crash that carves a bit out', jagged
	// berms both sides). The first try cut rut ops: pass 1 on grass is wear only, so 500 gouge points left nothing. Now a
	// heightfield cut (MUD_WorldGroundEntity.CrashScrape) under every body face point touching our ground while the
	// wreck slides faster than SCRAPE_V, every SCRAPE_EVERY s for GOUGE_S s; the cut dirt piles up beside the path.
	static const float GOUGE_S = 8;                 // s of crash scrape after an impact
	static const float GOUGE_TOUCH = 0.35;          // m: a body face point this close over our ground (or under it) scrapes
	static const float SCRAPE_V = 1.0;              // m/s: slower = resting, no cut (else a parked wreck digs itself in)
	static const float SCRAPE_EVERY = 0.1;          // s between scrape steps
	static const float SCRAPE_STEP = 0.1;           // m cut per step and point
	static const float SCRAPE_SINK = 0.15;          // m: the ground is cut to this far under the body point
	static const float SCRAPE_DEPTH = 1.0;          // m: deepest scrape under the undug ground
	static const float SCRAPE_SIDE = 1.2;           // m: berms this far out from each scraping point, either side
	static const float BODY_STEP = 0.75;            // m: body face point spacing
	static const float BODY_HALF_W = 1.5;           // m: body half width with no gear
	static const float BODY_PAD = 0.3;              // m: body half width = widest gear point + this
	// GEAR TRACE (WB 2026-10-24: GetBoneIndex found none of the LandingGear bones, 'gear measured - 0 points' on both
	// helis): traces up at the heli's own collider on a TRACE_DX x TRACE_DZ grid; hits within TRACE_BAND of the lowest =
	// gear; touching hits group into skids (z span >= SKID_MIN: front + rear point) and wheels (one point).
	static const float TRACE_DX = 0.05;
	static const float TRACE_DZ = 0.25;
	static const float TRACE_BAND = 0.1;
	static const float SKID_MIN = 1.0;
	static const float LOW_OK = 0.25;               // m: lowest trace hit within this of the model bottom = gear geometry found
	static const float SKID_X = 0.8;                // GEAR FROM BOUNDS: skids / main wheels at this share of the half width
	// LANDING OVERKILL (user WB 2026-10-24 round 2: 'still dont do much visually when landing - can be more overkill,
	// they land hard and are heavy, maybe a little dirt circle'). Heightfield cuts (CrashScrape), not rut ops: anything
	// dug > 5 cm shows dirt (RING_DIRT reveal). Depths are under the UNDUG ground, so repeat landings do not dig on.
	static const float STAMP_BASE = 0.04;           // touchdown imprint depth = base + tons x PER_TON + down m/s x PER_MS
	static const float STAMP_PER_TON = 0.015;       // UH-1 2.4 t at 1.5 m/s 0.14 m, Mi-8 9 t at 1.5 m/s 0.24 m
	static const float STAMP_PER_MS = 0.04;
	static const float STAMP_MIN = 0.08;
	static const float STAMP_MAX = 0.35;
	static const float STAMP_SPACING = 0.3;         // m between imprint points along a skid
	static const float STAMP_SIDE = 0.45;           // m: lips this far out beside each imprint point
	static const float SLIDE_V = 0.5;               // m/s: touching gear moving faster carves
	static const float SLIDE_DEPTH = 0.1;           // m under the undug ground
	static const float SLIDE_STEP = 0.03;           // m per step (every SCRAPE_EVERY s)
	static const float SLIDE_SIDE = 0.45;
	static const float DUST_SHARE = 0.35;           // downwash ring radius = bounds length x this, DUST_MIN_R .. DUST_MAX_R
	static const float DUST_MIN_R = 3;              // UH-1 13 m -> 4.6 m, Mi-8 18.8 m -> 6.6 m
	static const float DUST_MAX_R = 7;
	static const float DUST_SPACING = 0.5;          // m between ring points
	static const float DUST_JIT = 0.4;              // m: radius jitter per point (scuffed, not a circle line)
	static const float DUST_GAP = 0.2;              // share of ring points left out (broken ring)
	static const float DUST_D0 = 0.06;              // m: ring depth per point, random D0 .. D1 (> 0.05 = drawn as dirt)
	static const float DUST_D1 = 0.1;
	// HOVER WASH (user 2026-10-24: 'the more time you hover or sit in one place the more it spreads around the heli').
	// Rotor wash blows down, then out along the ground; the fuselage shields the middle. While the rotor turns (RPM >=
	// WASH_RPM of its target) and the body bottom is under WASH_H over our ground, every WASH_EVERY s scuffs
	// WASH_PTS x strength dirt points (DUST_D0 .. D1 deep, under the undug ground) in a ring (GRADUAL below) round the body
	// out to a radius that grows with the wash time, up to WASH_OUT x rotor radius after WASH_FULL_S. strength =
	// 1 - height / WASH_H. Moving WASH_MOVE m off the wash centre starts a new one. In the snow zone the cut clears the
	// look lift (cut ground draws no lift) = snow blown away.
	static const bool WASH = true;
	static const float WASH_RPM = 0.6;
	static const float WASH_H = 8;
	static const float WASH_EVERY = 1;
	// GRADUAL (user 2026-10-24: 'way too fast, should be much more gradual, start in the centre of the chopper and build
	// out'; was 14 points a step, ring 0.4 .. 1.3 R in 30 s): the wash front starts at WASH_START m under the body and
	// grows to WASH_OUT x rotor radius over WASH_FULL_S of wash; each step scuffs only between WASH_BAND x the front and
	// the front, so the dirt builds outward.
	static const float WASH_PTS = 1.2;              // points per s at full strength (fractions carry over; was 4 + 1 a step)
	static const float WASH_LANDED = 0.3;           // strength x this while the gear touches (rotor on the ground, flat pitch)
	static const float WASH_START = 1;
	static const float WASH_BAND = 0.6;
	static const float WASH_OUT = 1.3;
	static const float WASH_FULL_S = 240;           // user 2026-10-24 'still too fast standing still, can be much slower' (was 90)
	static const bool DUST_RING = false;            // touchdown dust ring at once (round 3) - off: the wash builds it now
	static const float WASH_MOVE = 3;
	static const float WASH_R_NONE = 7;             // m rotor radius when the sim gives none

	// LandingGear bones (pak 2026-10-24) and their wheel radius (0 = skid contact). Skids: front / rear pairs below.
	protected static ref array<string> s_aGearBones = {"v_ground_contact_L01", "v_ground_contact_L02", "v_ground_contact_R01", "v_ground_contact_R02", "v_wheel_F", "v_wheel_bL", "v_wheel_bR"};
	protected static ref array<float> s_aGearRadius = {0, 0, 0, 0, 0.3, 0.45, 0.45};

	protected static ref MUD_HeliGround s_Inst;
	protected Vehicle m_TraceVeh;                   // TraceGear: the heli being traced (filter)
	protected ref array<ref MUD_HeliGear> m_aGears = {};
	protected int m_iRutPts;                        // 10 s log: gear points that cut a rut step
	protected int m_iSetDowns;
	protected int m_iCrashes;
	protected int m_iGouges;
	protected int m_iWashPts;                       // 10 s log: hover wash points that cut
	protected float m_fLogAt;

	//------------------------------------------------------------------------------------------------
	//! Any machine, Vehicle.EOnInit (via MUD_Wheels.Register): a heli in play
	static void Register(Vehicle v)
	{
		if (!ENABLED || !v || !v.FindComponent(VehicleHelicopterSimulation))
			return;
		if (!s_Inst)
			s_Inst = new MUD_HeliGround();
		foreach (MUD_HeliGear g : s_Inst.m_aGears)
		{
			if (g.m_Veh == v)
				return;
		}
		MUD_HeliGear ng = new MUD_HeliGear();
		ng.m_Veh = v;
		s_Inst.m_aGears.Insert(ng);
		if (MUD_TestKit.TEST_KIT)
			Print(string.Format("MUD heli: %1 registered (%2)", v.GetID(), PrefabOf(v)));
	}

	//------------------------------------------------------------------------------------------------
	//! Log text: the prefab of e
	static string PrefabOf(IEntity e)
	{
		EntityPrefabData pd = e.GetPrefabData();
		if (!pd)
			return "no prefab";
		return pd.GetPrefabName();
	}

	//------------------------------------------------------------------------------------------------
	//! Live helis (MUD_Settle reads them too)
	static void GetAll(notnull array<Vehicle> into)
	{
		if (!s_Inst)
			return;
		foreach (MUD_HeliGear g : s_Inst.m_aGears)
		{
			if (g.m_Veh)
				into.Insert(g.m_Veh);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: once per frame after the wheels. now = the world ground's m_fTime
	static void Tick(float now)
	{
		if (!ENABLED || !s_Inst)
			return;
		s_Inst.DoTick(now);
	}

	//------------------------------------------------------------------------------------------------
	protected void DoTick(float now)
	{
		for (int i = m_aGears.Count() - 1; i >= 0; i--)
		{
			MUD_HeliGear g = m_aGears[i];
			if (!g.m_Veh)
			{
				m_aGears.Remove(i);
				continue;
			}
			Physics phys = g.m_Veh.GetPhysics();
			if (!phys)
				continue;
			if (!g.m_bMeasured)
				Measure(g);
			vector vel = phys.GetVelocity();
			float down = -vel[1];
			if (CRASH)
				CrashCheck(g, phys, vel, down, now);
			if (WASH)
				Wash(g, now);
			if (RUTS && !g.m_aPts.IsEmpty())
				GearRuts(g, phys, now);
			if (GOUGE && now - g.m_fCrashAt < GOUGE_S)
				Scrape(g, vel, now);
			g.m_fLastDown = down;
			MUD_VoxelProbe.RutFlush(g.m_Veh);
		}
		if (MUD_TestKit.TEST_KIT && now - m_fLogAt >= 10)
		{
			m_fLogAt = now;
			if (m_iRutPts + m_iSetDowns + m_iCrashes + m_iGouges + m_iWashPts > 0)
				Print(string.Format("MUD heli: last 10 s - %1 gear slide points, %2 set-downs, %3 impact craters, %4 crash scrape points, %5 wash points | helis %6", m_iRutPts, m_iSetDowns, m_iCrashes, m_iGouges, m_iWashPts, m_aGears.Count()));
			m_iWashPts = 0;
			m_iRutPts = 0;
			m_iSetDowns = 0;
			m_iCrashes = 0;
			m_iGouges = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Gear points from the LandingGear bones, bounds corners for the crash / gouge checks. Once per heli.
	protected void Measure(MUD_HeliGear g)
	{
		g.m_bMeasured = true;
		g.m_bTouch = true;     // review: a heli placed on the ground at start is not a touchdown (no dent); in the air -> m_fAirSince
		Vehicle veh = g.m_Veh;
		vector mins;
		vector maxs;
		veh.GetBounds(mins, maxs);
		g.m_fLength = maxs[2] - mins[2];
		g.m_vCentre = (mins + maxs) * 0.5;
		g.m_vBottom = Vector(g.m_vCentre[0], mins[1], g.m_vCentre[2]);
		g.m_Sim = VehicleHelicopterSimulation.Cast(veh.FindComponent(VehicleHelicopterSimulation));
		Corners(g, mins, maxs);     // bounds only until the gear is known (no animation = no gear)
		Animation anim = veh.GetAnimation();
		array<int> found = {};     // per s_aGearBones entry: index into m_aPts, -1 = missing
		for (int b = 0; b < s_aGearBones.Count(); b++)
		{
			int id = -1;
			if (anim)
				id = anim.GetBoneIndex(s_aGearBones[b]);
			vector m[4];
			if (id < 0 || !anim.GetBoneMatrix(id, m))
			{
				found.Insert(-1);
				continue;
			}
			found.Insert(g.m_aPts.Count());
			g.m_aPts.Insert(m[3] - Vector(0, s_aGearRadius[b], 0));
			g.m_aRad.Insert(s_aGearRadius[b]);
			if (s_aGearRadius[b] > 0)
				g.m_aWheel.Insert(1);
			else
				g.m_aWheel.Insert(0);
		}
		// skid pairs L01-L02, R01-R02: front = larger local z
		for (int s = 0; s < 2; s++)
		{
			int a = found[s * 2];
			int c = found[s * 2 + 1];
			if (a < 0 || c < 0)
				continue;
			if (g.m_aPts[a][2] < g.m_aPts[c][2])
			{
				int sw = a;
				a = c;
				c = sw;
			}
			g.m_aSkidA.Insert(a);
			g.m_aSkidB.Insert(c);
		}
		string how = "bones";
		if (g.m_aPts.IsEmpty())
		{
			how = "traces";
			TraceGear(g, mins, maxs);
		}
		Corners(g, mins, maxs);
		BodyPoints(g, mins, maxs);
		if (MUD_TestKit.TEST_KIT || g.m_aPts.IsEmpty())
			Print(string.Format("MUD heli: %1 gear measured by %2 - %3 points (%4 skids), %5 body points, bounds %6 .. %7", veh.GetID(), how, g.m_aPts.Count(), g.m_aSkidA.Count(), g.m_aBody.Count(), mins, maxs));
	}

	//------------------------------------------------------------------------------------------------
	//! Body box corners (bottom 4 first). The model bounds hold the main rotor disk (review 2026-10-24: x +-9 m on a
	//! Mi-8), so x is cut to the gear's half width + BODY_PAD (BODY_HALF_W with no gear); z / y stay the bounds.
	protected void Corners(MUD_HeliGear g, vector mins, vector maxs)
	{
		float hw = BODY_HALF_W;
		if (!g.m_aPts.IsEmpty())
		{
			hw = 0;
			foreach (vector gp : g.m_aPts)
				hw = Math.Max(hw, Math.AbsFloat(gp[0]));
			hw += BODY_PAD;
		}
		float x0 = Math.Max(mins[0], -hw);
		float x1 = Math.Min(maxs[0], hw);
		g.m_aCorner[0] = Vector(x0, mins[1], mins[2]);
		g.m_aCorner[1] = Vector(x1, mins[1], mins[2]);
		g.m_aCorner[2] = Vector(x0, mins[1], maxs[2]);
		g.m_aCorner[3] = Vector(x1, mins[1], maxs[2]);
		g.m_aCorner[4] = Vector(x0, maxs[1], mins[2]);
		g.m_aCorner[5] = Vector(x1, maxs[1], mins[2]);
		g.m_aCorner[6] = Vector(x0, maxs[1], maxs[2]);
		g.m_aCorner[7] = Vector(x1, maxs[1], maxs[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Height of world point wp over our ground (gy = the ground there). False = not over our ground.
	protected bool OverGround(vector wp, out float over, out float gy)
	{
		if (!MUD_WorldGroundEntity.SurfaceY(wp[0], wp[2], gy))
			return false;
		over = wp[1] - gy;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! F2 (round 3, see LANDING OVERKILL): touching gear that slides carves SLIDE_DEPTH (heightfield, lips beside it); a
	//! touchdown after AIR_S stamps the gear in and scuffs a downwash dirt ring (SetDown)
	protected ref array<vector> m_aCutPts = {};
	protected void GearRuts(MUD_HeliGear g, Physics phys, float now)
	{
		Vehicle veh = g.m_Veh;
		vector vel = phys.GetVelocity();
		float hs = Math.Sqrt(vel[0] * vel[0] + vel[2] * vel[2]);
		bool slide = hs >= SLIDE_V && now - g.m_fSlideAt >= SCRAPE_EVERY;
		if (slide)
			g.m_fSlideAt = now;
		m_aCutPts.Clear();
		bool any;
		for (int i = 0; i < g.m_aPts.Count(); i++)
		{
			vector wp = veh.CoordToParent(g.m_aPts[i]);
			if (!Touches(wp))
				continue;
			any = true;
			if (slide)
				AddCut(wp, SLIDE_DEPTH);
		}
		// skids touch along their whole length: quarter points between the ends
		if (slide)
		{
			for (int k = 0; k < g.m_aSkidA.Count(); k++)
			{
				vector la = g.m_aPts[g.m_aSkidA[k]];
				vector lb = g.m_aPts[g.m_aSkidB[k]];
				for (int q = 1; q <= 3; q++)
				{
					float f = q * 0.25;
					vector sp = veh.CoordToParent(la + (lb - la) * f);
					if (Touches(sp))
						AddCut(sp, SLIDE_DEPTH);
				}
			}
		}
		if (!m_aCutPts.IsEmpty())
		{
			vector dir = Vector(vel[0] / hs, 0, vel[2] / hs);
			m_iRutPts += MUD_WorldGroundEntity.CrashScrape(m_aCutPts, dir, SLIDE_STEP, STAMP_MAX, SLIDE_SIDE);
		}
		if (any && !g.m_bTouch && now - g.m_fAirSince >= AIR_S)
			SetDown(g, phys, g.m_fLastDown);
		if (!any && g.m_bTouch)
			g.m_fAirSince = now;
		g.m_bTouch = any;
	}

	//------------------------------------------------------------------------------------------------
	//! HOVER WASH step (see the constants)
	protected void Wash(MUD_HeliGear g, float now)
	{
		if (now - g.m_fWashStepAt < WASH_EVERY)
			return;
		g.m_fWashStepAt = now;
		VehicleHelicopterSimulation sim = g.m_Sim;
		if (!sim || sim.RotorCount() < 1)
			return;
		float target = sim.RotorGetRPMTarget(0);
		if (target <= 0 || sim.RotorGetRPM(0) < target * WASH_RPM)
			return;
		vector b = g.m_Veh.CoordToParent(g.m_vBottom);
		float over;
		float gy;
		if (!OverGround(b, over, gy) || over >= WASH_H || over < -SUNK)
			return;
		float k = Math.Clamp(1 - over / WASH_H, 0, 1);
		if (g.m_bTouch)
			k = k * WASH_LANDED;
		if (vector.DistanceXZ(b, g.m_vWashAt) > WASH_MOVE)
		{
			g.m_vWashAt = b;
			g.m_fWashT = 0;
		}
		g.m_fWashT = g.m_fWashT + WASH_EVERY * k;
		float rotorR = sim.RotorGetRadius(0);
		if (rotorR <= 0)
			rotorR = WASH_R_NONE;
		float grow = Math.Min(1, g.m_fWashT / WASH_FULL_S);
		float outer = WASH_START + (rotorR * WASH_OUT - WASH_START) * grow;
		float inner = outer * WASH_BAND;
		m_aCutPts.Clear();
		g.m_fWashCarry = g.m_fWashCarry + WASH_PTS * k * WASH_EVERY;
		int n = Math.Floor(g.m_fWashCarry);
		g.m_fWashCarry = g.m_fWashCarry - n;
		if (n <= 0)
			return;
		for (int i = 0; i < n; i++)
		{
			float ang = Math.RandomFloat(0, Math.PI2);
			float rr = Math.RandomFloat(inner, outer);
			AddCut(Vector(b[0] + Math.Cos(ang) * rr, 0, b[2] + Math.Sin(ang) * rr), Math.RandomFloat(DUST_D0, DUST_D1));
		}
		int cut = MUD_WorldGroundEntity.CrashScrape(m_aCutPts, Vector(0, 0, 1), DUST_D1, DUST_D1, 0);
		m_iWashPts += cut;
		if (MUD_TestKit.TEST_KIT && !g.m_bWashLogged)
		{
			g.m_bWashLogged = true;
			Print(string.Format("MUD heli: %1 wash on | rotor r %2 m, rpm %3 of %4 | height %5 m, ring %6 .. %7 m", g.m_Veh.GetID(), rotorR.ToString(-1, 1), sim.RotorGetRPM(0).ToString(-1, 0), target.ToString(-1, 0), over.ToString(-1, 1), inner.ToString(-1, 1), outer.ToString(-1, 1)));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool Touches(vector wp)
	{
		float over;
		float gy;
		return OverGround(wp, over, gy) && over < TOUCH && over > -SUNK;
	}

	//------------------------------------------------------------------------------------------------
	//! A cut point into m_aCutPts: x, z of wp, y = the undug ground there - depth
	protected void AddCut(vector wp, float depth)
	{
		float undug;
		if (MUD_WorldGroundEntity.UndugY(wp[0], wp[2], undug))
			m_aCutPts.Insert(Vector(wp[0], undug - depth, wp[2]));
	}

	//------------------------------------------------------------------------------------------------
	//! F2 touchdown: the gear stamped in (depth by mass + down speed, lips beside it) and a broken downwash dirt ring
	protected void SetDown(MUD_HeliGear g, Physics phys, float down)
	{
		Vehicle veh = g.m_Veh;
		m_iSetDowns++;
		float depth = Math.Clamp(STAMP_BASE + phys.GetMass() * 0.001 * STAMP_PER_TON + Math.Max(down, 0) * STAMP_PER_MS, STAMP_MIN, STAMP_MAX);
		vector fwd = veh.GetTransformAxis(2);
		vector dir = Vector(fwd[0], 0, fwd[2]);
		float fl = dir.Length();
		if (fl < 0.01)
			dir = Vector(0, 0, 1);
		else
			dir = dir * (1 / fl);
		m_aCutPts.Clear();
		for (int k = 0; k < g.m_aSkidA.Count(); k++)
		{
			vector la = g.m_aPts[g.m_aSkidA[k]];
			vector lb = g.m_aPts[g.m_aSkidB[k]];
			int steps = Math.Max(1, Math.Ceil(vector.Distance(la, lb) / STAMP_SPACING));
			for (int s = 0; s <= steps; s++)
			{
				float f = s;
				f = f / steps;
				AddCut(veh.CoordToParent(la + (lb - la) * f), depth);
			}
		}
		for (int i = 0; i < g.m_aPts.Count(); i++)
		{
			if (g.m_aWheel[i] == 0)
				continue;
			for (int w = -1; w <= 1; w++)
				AddCut(veh.CoordToParent(g.m_aPts[i] + Vector(0, 0, w * 0.2)), depth);
		}
		int stamped = MUD_WorldGroundEntity.CrashScrape(m_aCutPts, dir, depth, STAMP_MAX, STAMP_SIDE);
		// downwash ring round the body centre (DUST_RING off: the hover wash builds it gradually)
		m_aCutPts.Clear();
		if (!DUST_RING)
		{
			if (MUD_TestKit.TEST_KIT)
				Print(string.Format("MUD heli: %1 set down at %2 | down %3 m/s, stamp %4 m: %5 points cut", veh.GetID(), veh.GetOrigin(), down.ToString(-1, 1), depth.ToString(-1, 2), stamped));
			return;
		}
		vector c = veh.CoordToParent(g.m_vCentre);
		float r = Math.Clamp(g.m_fLength * DUST_SHARE, DUST_MIN_R, DUST_MAX_R);
		int pts = Math.Ceil(Math.PI2 * r / DUST_SPACING);
		for (int q = 0; q < pts; q++)
		{
			if (Math.RandomFloat01() < DUST_GAP)
				continue;
			float ang = Math.PI2 * q / pts;
			float rr = r + Math.RandomFloat(-DUST_JIT, DUST_JIT);
			AddCut(Vector(c[0] + Math.Cos(ang) * rr, 0, c[2] + Math.Sin(ang) * rr), Math.RandomFloat(DUST_D0, DUST_D1));
		}
		int ring = MUD_WorldGroundEntity.CrashScrape(m_aCutPts, dir, DUST_D1, DUST_D1, 0);
		if (MUD_TestKit.TEST_KIT)
			Print(string.Format("MUD heli: %1 set down at %2 | down %3 m/s, stamp %4 m: %5 points cut | dust ring r %6 m: %7 of %8 points cut", veh.GetID(), veh.GetOrigin(), down.ToString(-1, 1), depth.ToString(-1, 2), stamped, r.ToString(-1, 1), ring, pts));
	}

	//------------------------------------------------------------------------------------------------
	protected vector GroundPoint(vector wp)
	{
		float gy;
		if (MUD_WorldGroundEntity.SurfaceY(wp[0], wp[2], gy))
			wp[1] = gy;
		return wp;
	}

	//------------------------------------------------------------------------------------------------
	//! Lowest bottom bounds corner over our ground: its height over the ground (1000 = none over our ground)
	protected float LowCorner(MUD_HeliGear g, out vector low)
	{
		float best = 1000;
		for (int c = 0; c < 8; c++)
		{
			vector wp = g.m_Veh.CoordToParent(g.m_aCorner[c]);
			float over;
			float gy;
			if (!OverGround(wp, over, gy))
				continue;
			if (over < best)
			{
				best = over;
				low = Vector(wp[0], gy, wp[2]);
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! F3: a hard hit on our ground digs an impact crater at the lowest corner
	protected void CrashCheck(MUD_HeliGear g, Physics phys, vector vel, float down, float now)
	{
		if (g.m_fLastDown < CRASH_VS || g.m_fLastDown - down < CRASH_DV || now - g.m_fCrashAt < CRASH_AGAIN_S)
			return;
		vector low;
		float near = LowCorner(g, low);
		if (near > CRASH_NEAR)
			return;
		float mass = phys.GetMass();
		float hs = Math.Sqrt(vel[0] * vel[0] + vel[2] * vel[2]);
		float v2 = g.m_fLastDown * g.m_fLastDown + HS_SHARE * HS_SHARE * hs * hs;
		float kg = 0.5 * mass * v2 / 4184000 * CRASH_KG_MULT;
		g.m_fCrashAt = now;
		g.m_fMassAtCrash = mass;
		m_iCrashes++;
		bool destroyed;
		SCR_DamageManagerComponent dm = SCR_DamageManagerComponent.GetDamageManager(g.m_Veh);
		if (dm)
			destroyed = dm.GetState() == EDamageState.DESTROYED;
		Print(string.Format("MUD heli: impact %1 | mass %2 kg | down %3 -> %4 m/s, flat %5 | %6 kg TNT | corner %7 m over the ground at %8 | destroyed %9", g.m_Veh.GetID(), mass, g.m_fLastDown.ToString(-1, 1), down.ToString(-1, 1), hs.ToString(-1, 1), kg.ToString(-1, 2), near.ToString(-1, 2), low, destroyed));
		if (CRASH_CRATER)
		{
			MUD_WorldGroundEntity.ImpactCrater(low, kg);
			return;
		}
		float mj = 0.5 * mass * v2 / 1000000;
		float depth = Math.Clamp(GOUGE_K * Math.Sqrt(mj), GOUGE_MIN_D, GOUGE_MAX_D);
		vector dir;
		if (hs > 2)
		{
			dir = Vector(vel[0] / hs, 0, vel[2] / hs);
		}
		else
		{
			vector fwd = g.m_Veh.GetTransformAxis(2);
			dir = Vector(fwd[0], 0, fwd[2]);
			float fl = dir.Length();
			if (fl < 0.01)
				dir = Vector(0, 0, 1);
			else
				dir = dir * (1 / fl);
		}
		float len = Math.Clamp(Math.Max(hs * GOUGE_PER_MS, g.m_fLength * GOUGE_MIN_BODY), 2, GOUGE_MAX_L);
		float halfW = Math.AbsFloat(g.m_aCorner[1][0] - g.m_aCorner[0][0]) * 0.5 * GOUGE_W;
		// a near-vertical hit: the pit is centred on the impact; a fast flat one starts a little behind it
		vector start = low - dir * (len * 0.15);
		if (hs <= 2)
			start = low - dir * (len * 0.5);
		int cut = MUD_WorldGroundEntity.ImpactGouge(start, dir, len, halfW, depth);
		Print(string.Format("MUD heli: impact gouge %1 | %2 MJ | depth %3 m, length %4 m, half width %5 m | %6 points cut", g.m_Veh.GetID(), mj.ToString(-1, 2), depth.ToString(-1, 2), len.ToString(-1, 1), halfW.ToString(-1, 2), cut));
	}

	//------------------------------------------------------------------------------------------------
	//! F3 crash scrape: while the wreck slides after an impact, the ground under every touching body face point is
	//! cut (heightfield, MUD_WorldGroundEntity.CrashScrape) and piled beside the path
	protected ref array<vector> m_aScrapePts = {};
	protected void Scrape(MUD_HeliGear g, vector vel, float now)
	{
		float hs = Math.Sqrt(vel[0] * vel[0] + vel[2] * vel[2]);
		if (hs < SCRAPE_V || now - g.m_fScrapeAt < SCRAPE_EVERY)
			return;
		g.m_fScrapeAt = now;
		m_aScrapePts.Clear();
		foreach (vector bp : g.m_aBody)
		{
			vector wp = g.m_Veh.CoordToParent(bp);
			float over;
			float gy;
			if (OverGround(wp, over, gy) && over < GOUGE_TOUCH && over > -SUNK)
				m_aScrapePts.Insert(Vector(wp[0], wp[1] - SCRAPE_SINK, wp[2]));
		}
		if (m_aScrapePts.IsEmpty())
			return;
		vector dir = Vector(vel[0] / hs, 0, vel[2] / hs);
		m_iGouges += MUD_WorldGroundEntity.CrashScrape(m_aScrapePts, dir, SCRAPE_STEP, SCRAPE_DEPTH, SCRAPE_SIDE);
	}

	//------------------------------------------------------------------------------------------------
	//! Body box face points (bottom, top, both sides) every BODY_STEP m, inside the gear-wide corners: whichever face
	//! the wreck slides on (side, roof) scrapes
	protected void BodyPoints(MUD_HeliGear g, vector mins, vector maxs)
	{
		g.m_aBody.Clear();
		float x0 = g.m_aCorner[0][0];
		float x1 = g.m_aCorner[1][0];
		for (float z = mins[2]; z <= maxs[2] + 0.01; z += BODY_STEP)
		{
			for (float x = x0; x <= x1 + 0.01; x += BODY_STEP)
			{
				g.m_aBody.Insert(Vector(x, mins[1], z));
				g.m_aBody.Insert(Vector(x, maxs[1], z));
			}
			for (float y = mins[1] + BODY_STEP; y < maxs[1]; y += BODY_STEP)
			{
				g.m_aBody.Insert(Vector(x0, y, z));
				g.m_aBody.Insert(Vector(x1, y, z));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! No gear geometry (INFERENCE layout): skids at +-SKID_X of the half width over the belly's z range (SKID gear), or
	//! a nose wheel at its front and two mains a quarter in from its rear (WHEEL gear), all at the model bottom
	protected void GearFromBounds(MUD_HeliGear g, array<vector> belly, vector mins, vector maxs)
	{
		float zMin = mins[2] * 0.3;
		float zMax = maxs[2] * 0.3;
		if (!belly.IsEmpty())
		{
			zMin = 1000;
			zMax = -1000;
			foreach (vector bh : belly)
			{
				zMin = Math.Min(zMin, bh[2]);
				zMax = Math.Max(zMax, bh[2]);
			}
		}
		float hw = Math.Max(Math.AbsFloat(mins[0]), Math.AbsFloat(maxs[0])) * SKID_X;
		float y = mins[1];
		bool skid = true;
		VehicleHelicopterSimulation sim = VehicleHelicopterSimulation.Cast(g.m_Veh.FindComponent(VehicleHelicopterSimulation));
		if (sim && sim.LandingGearGetType() == LandingGearType.WHEEL)
			skid = false;
		if (skid)
		{
			for (int sd = -1; sd <= 1; sd += 2)
			{
				int fi = g.m_aPts.Count();
				g.m_aPts.Insert(Vector(hw * sd, y, zMax));
				g.m_aPts.Insert(Vector(hw * sd, y, zMin));
				g.m_aRad.Insert(0);
				g.m_aRad.Insert(0);
				g.m_aWheel.Insert(0);
				g.m_aWheel.Insert(0);
				g.m_aSkidA.Insert(fi);
				g.m_aSkidB.Insert(fi + 1);
			}
			return;
		}
		float zMain = zMin + (zMax - zMin) * 0.25;
		g.m_aPts.Insert(Vector(0, y, zMax - 0.3));
		g.m_aPts.Insert(Vector(-hw, y, zMain));
		g.m_aPts.Insert(Vector(hw, y, zMain));
		for (int w = 0; w < 3; w++)
		{
			g.m_aRad.Insert(0);
			g.m_aWheel.Insert(1);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool OnlyVeh(notnull IEntity e)
	{
		return e.GetRootParent() == m_TraceVeh;
	}

	//------------------------------------------------------------------------------------------------
	//! Gear from the heli's own collider (no gear bones): see GEAR TRACE
	protected void TraceGear(MUD_HeliGear g, vector mins, vector maxs)
	{
		Vehicle veh = g.m_Veh;
		int t0 = System.GetTickCount();
		TraceParam tp = new TraceParam();
		tp.Flags = TraceFlags.ENTS;
		// WB 2026-10-24 round 2: Terrain alone hit only the belly (lowest hit local y 0.55, model bottom -0.08) - FireGeo too
		tp.LayerMask = EPhysicsLayerPresets.Terrain | EPhysicsLayerPresets.FireGeo;
		m_TraceVeh = veh;
		array<vector> hits = {};
		float lowY = 1000;
		int traces;
		for (float z = mins[2]; z <= maxs[2]; z += TRACE_DZ)
		{
			for (float x = mins[0]; x <= maxs[0]; x += TRACE_DX)
			{
				tp.Start = veh.CoordToParent(Vector(x, mins[1] - 0.5, z));
				tp.End = veh.CoordToParent(Vector(x, maxs[1], z));
				traces++;
				float frac = veh.GetWorld().TraceMove(tp, OnlyVeh);
				if (frac >= 1)
					continue;
				vector lh = veh.CoordToLocal(tp.Start + (tp.End - tp.Start) * frac);
				hits.Insert(lh);
				lowY = Math.Min(lowY, lh[1]);
			}
		}
		m_TraceVeh = null;
		array<vector> low = {};
		foreach (vector h : hits)
		{
			if (h[1] <= lowY + TRACE_BAND)
				low.Insert(h);
		}
		if (lowY > mins[1] + LOW_OK)
		{
			Print(string.Format("MUD heli: %1 gear trace found no gear geometry (lowest hit %2, model bottom %3, %4 traces, %5 ms) - gear from bounds", veh.GetID(), lowY, mins[1], traces, System.GetTickCount(t0)));
			GearFromBounds(g, low, mins, maxs);
			return;
		}
		// group neighbouring hits: lab[i] = smallest index of i's group
		int n = low.Count();
		array<int> lab = {};
		for (int i = 0; i < n; i++)
			lab.Insert(i);
		bool merged = true;
		while (merged)
		{
			merged = false;
			for (int a = 0; a < n; a++)
			{
				for (int b = a + 1; b < n; b++)
				{
					if (lab[a] == lab[b])
						continue;
					if (Math.AbsFloat(low[a][0] - low[b][0]) > TRACE_DX * 1.5 || Math.AbsFloat(low[a][2] - low[b][2]) > TRACE_DZ * 1.5)
						continue;
					int keep = Math.Min(lab[a], lab[b]);
					int drop = Math.Max(lab[a], lab[b]);
					for (int c = 0; c < n; c++)
					{
						if (lab[c] == drop)
							lab.Set(c, keep);
					}
					merged = true;
				}
			}
		}
		for (int k = 0; k < n; k++)
		{
			if (lab[k] != k)
				continue;
			float zMin = 1000;
			float zMax = -1000;
			float sx = 0;
			float sz = 0;
			int cnt = 0;
			for (int m = 0; m < n; m++)
			{
				if (lab[m] != k)
					continue;
				zMin = Math.Min(zMin, low[m][2]);
				zMax = Math.Max(zMax, low[m][2]);
				sx += low[m][0];
				sz += low[m][2];
				cnt++;
			}
			float mx = sx / cnt;
			int fi = g.m_aPts.Count();
			if (zMax - zMin >= SKID_MIN)
			{
				g.m_aPts.Insert(Vector(mx, lowY, zMax));
				g.m_aPts.Insert(Vector(mx, lowY, zMin));
				g.m_aRad.Insert(0);
				g.m_aRad.Insert(0);
				g.m_aWheel.Insert(0);
				g.m_aWheel.Insert(0);
				g.m_aSkidA.Insert(fi);
				g.m_aSkidB.Insert(fi + 1);
			}
			else
			{
				g.m_aPts.Insert(Vector(mx, lowY, sz / cnt));
				g.m_aRad.Insert(0);
				g.m_aWheel.Insert(1);
			}
		}
		Print(string.Format("MUD heli: %1 gear traced - %2 traces, %3 hits, %4 in the lowest %5 m (local y %6), %7 ms", veh.GetID(), traces, hits.Count(), n, TRACE_BAND, lowY, System.GetTickCount(t0)));
	}

	//------------------------------------------------------------------------------------------------
	//! World end (MUD_WorldGroundEntity dtor): no helis into the next session
	static void Reset()
	{
		s_Inst = null;
		MUD_Settle.Reset();
	}
}

//------------------------------------------------------------------------------------------------
//! Vehicle + wreck jitter (user 2026-10-24: wrecks and vehicles in general bounce / jitter in craters). Cause NOT
//! known yet - this is the probe + one candidate fix, A/B in one run:
//! - PROBE (TEST_KIT): every vehicle with no live pilot that moves under REST_V on our ground is watched; each
//!   PROBE_S s window it prints one line when it moved at all: mean / max speed and spin, up-down flips, how often
//!   the patch collider under it was rebuilt in the window (H1: our collider rebuild drops a resting body), the arm.
//! - SETTLE (candidate): such a vehicle gets Physics.SetDamping(SETTLE_LIN, SETTLE_ANG); a pilot getting in or a
//!   fast push (> REST_V) sets 0, 0 back (no getter: stock damping assumed 0 - KB physics-transforms.md).
//! While AB (and TEST_KIT) the arm flips every AB_S s: even windows settle on, odd off ('arm on / off' in the line).
class MUD_SettleItem
{
	Vehicle m_Veh;              // weak
	bool m_bDamped;
	float m_fSlowSince = -100;
	float m_fWinAt;
	int m_iSamples;
	float m_fSumV;
	float m_fMaxV;
	float m_fSumW;
	float m_fMaxW;
	int m_iFlips;
	float m_fLastVy;
	int m_iColBuilds;
	float m_fLastColAge = -1;
}

class MUD_Settle
{
	static const bool ON = true;
	static const bool AB = true;                // TEST_KIT only: arm flips every AB_S s
	static const float AB_S = 60;
	static const float REST_V = 1.5;            // m/s: slower than this with no pilot = resting (or jittering)
	static const float REST_AFTER = 0.5;        // s slow before the damping goes on (a falling wreck still falls)
	static const float SETTLE_LIN = 0.6;
	static const float SETTLE_ANG = 0.8;
	static const float PROBE_S = 2;
	static const float PROBE_MIN_V = 0.05;      // m/s: a window with max speed under this is quiet (no line)

	protected static ref array<ref MUD_SettleItem> s_aItems = {};
	protected static ref array<Vehicle> s_aScratch = {};

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_aItems.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static MUD_SettleItem Item(Vehicle v)
	{
		foreach (MUD_SettleItem it : s_aItems)
		{
			if (it.m_Veh == v)
				return it;
		}
		MUD_SettleItem ni = new MUD_SettleItem();
		ni.m_Veh = v;
		s_aItems.Insert(ni);
		return ni;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: once per frame after the wheels
	static void Tick(float now)
	{
		if (!ON)
			return;
		for (int i = s_aItems.Count() - 1; i >= 0; i--)
		{
			if (!s_aItems[i].m_Veh)
				s_aItems.Remove(i);
		}
		bool armOn = true;
		if (AB && MUD_TestKit.TEST_KIT)
		{
			int k = Math.Floor(now / AB_S);
			armOn = k % 2 == 0;
		}
		s_aScratch.Clear();
		s_aScratch.Copy(MUD_Wheels.GetAll());
		MUD_HeliGround.GetAll(s_aScratch);
		foreach (Vehicle veh : s_aScratch)
		{
			if (!veh)
				continue;
			Physics phys = veh.GetPhysics();
			if (!phys || !phys.IsDynamic())
				continue;
			One(veh, phys, now, armOn);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Driven(Vehicle veh)
	{
		IEntity pilot = veh.GetPilot();
		if (!pilot)
			return false;
		SCR_DamageManagerComponent dm = SCR_DamageManagerComponent.GetDamageManager(pilot);
		return !dm || dm.GetState() != EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	protected static void One(Vehicle veh, Physics phys, float now, bool armOn)
	{
		vector o = veh.GetOrigin();
		float gy;
		bool onGround = MUD_WorldGroundEntity.SurfaceY(o[0], o[2], gy);
		vector vel = phys.GetVelocity();
		float v = vel.Length();
		bool resting = onGround && !Driven(veh) && v < REST_V;
		MUD_SettleItem it = Item(veh);
		if (!resting)
		{
			it.m_fSlowSince = -100;
			if (it.m_bDamped)
			{
				phys.SetDamping(0, 0);
				it.m_bDamped = false;
			}
			it.m_iSamples = 0;
			return;
		}
		if (it.m_fSlowSince < -99)
			it.m_fSlowSince = now;
		bool want = armOn && now - it.m_fSlowSince >= REST_AFTER;
		if (want != it.m_bDamped)
		{
			if (want)
				phys.SetDamping(SETTLE_LIN, SETTLE_ANG);
			else
				phys.SetDamping(0, 0);
			it.m_bDamped = want;
		}
		if (MUD_TestKit.TEST_KIT)
			Probe(veh, phys, it, vel, v, now, armOn);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Probe(Vehicle veh, Physics phys, MUD_SettleItem it, vector vel, float v, float now, bool armOn)
	{
		if (it.m_iSamples == 0)
		{
			it.m_fWinAt = now;
			it.m_fSumV = 0;
			it.m_fMaxV = 0;
			it.m_fSumW = 0;
			it.m_fMaxW = 0;
			it.m_iFlips = 0;
			it.m_iColBuilds = 0;
			it.m_fLastVy = vel[1];
		}
		float w = phys.GetAngularVelocity().Length();
		it.m_iSamples++;
		it.m_fSumV += v;
		it.m_fMaxV = Math.Max(it.m_fMaxV, v);
		it.m_fSumW += w;
		it.m_fMaxW = Math.Max(it.m_fMaxW, w);
		if (vel[1] * it.m_fLastVy < 0 && Math.AbsFloat(vel[1]) > 0.02)
			it.m_iFlips++;
		it.m_fLastVy = vel[1];
		vector o = veh.GetOrigin();
		float age;
		if (MUD_WorldGroundEntity.ColAge(o[0], o[2], age))
		{
			if (it.m_fLastColAge >= 0 && age < it.m_fLastColAge)
				it.m_iColBuilds++;
			it.m_fLastColAge = age;
		}
		if (now - it.m_fWinAt < PROBE_S)
			return;
		if (it.m_fMaxV >= PROBE_MIN_V)
		{
			float n = it.m_iSamples;
			Print(string.Format("MUD settle: %1 | arm %2 damped %3 | speed mean %4 max %5 | spin mean %6 max %7 | up-down flips %8 | collider rebuilds under it %9",
				veh.GetID(), armOn, it.m_bDamped, (it.m_fSumV / n).ToString(-1, 3), it.m_fMaxV.ToString(-1, 3), (it.m_fSumW / n).ToString(-1, 3), it.m_fMaxW.ToString(-1, 3), it.m_iFlips, it.m_iColBuilds));
			Print(string.Format("MUD settle: %1 | mass %2 kg | active %3 | at %4 | %5", veh.GetID(), phys.GetMass(), phys.IsActive(), o, MUD_HeliGround.PrefabOf(veh)));
		}
		it.m_iSamples = 0;
	}
}
