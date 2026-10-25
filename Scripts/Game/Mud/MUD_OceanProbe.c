//! OCEAN PROBE (TEST_KIT, 2026-10-24, user idea in the 10-24 handoff: 'ocean kept for physics but calm / flat, our own
//! mesh layer on top near boats - waves, bow wave, foam V wake like the boat photo, like the mud layer'). Look only,
//! local, nothing synced, never a DS. Throwaway probe: archive after the test unless the user keeps it.
//! Replaces nothing: the parked bone water probe (MUD_WaterProbe, numpad 1 entries 9 / 10) stays as it is.
//! Facts it builds on: the ocean waves come from the world entity's OceanSimulation material (GenericWorld_Default.et:
//! OceanSimulation oceanSimIsland.emat, OceanAmplitude 0.397; pak read 2026-10-24); vertex displacement is in the ocean
//! shader only (IDA 2026-10-23); vanilla has no drivable boats (asset search 2026-10-24) - hence the VIRTUAL BOAT.
//! Questions:
//!   Q1 can the real waves go to ~0?  (a) wind override 0 / 25 m/s (key End), (b) world OceanSimulation swapped to
//!      Assets/Mud/MUD_OceanCalm.emat (a WB world edit, not script). MEASURE logs the wave height numbers each 5 s.
//!   Q2 does a script MeshObject with a water material draw right on the sea (pond / ocean / glass), seam vs the real
//!      ocean, and what a 30 Hz rebuild costs (ms in WB = not perf, just a size hint).
//!   Q3 a foam V strip behind a boat (trail-based, bends with the path).
//! Keys (diag exes only):
//!   Home     : OFF -> MEASURE -> LAYER pond material -> LAYER ocean material -> LAYER glass material -> OFF
//!   End      : virtual boat speed OFF -> 4 -> 8 -> 14 m/s (drives a R_BOAT m circle that starts AHEAD m in front)
//!   Page Up  : wind override NONE -> 0 m/s -> 25 m/s
//! Layer: LAYER_M square around the virtual boat (else the local player / their vehicle) when it is over sea; heights =
//! real GetOceanHeight + LIFT + our wake field; the outer EDGE_M ring dips under the real sea (no square border).
//! Log: 'MUD ocean probe: ...'.
class MUD_OceanBuf
{
	vector m_aV[2401];          // 49 x 49 layer verts
	float m_aUV[4802];
	int m_aI[13824];            // 48 x 48 cells x 6
	vector m_aFV[360];          // foam: 3 ribbons x FOAM_PTS points x 2
	float m_aFUV[720];
	int m_aFI[1062];            // 3 x (FOAM_PTS - 1) x 6
}

class MUD_OceanProbe
{
	static const ResourceName MAT_POND = "{202B614D7E81B3C5}Terrains/Common/Water/Pond/PondSurface_01.emat";
	static const ResourceName MAT_OCEAN = "{5BD82BC098CB7A2C}Terrains/Common/Water/Ocean/ocean.emat";
	static const ResourceName MAT_GLASS = "{4D55445741544531}Assets/Mud/MUD_WaterGlass.emat";
	static const ResourceName MAT_FOAM = "{4D55444F43464F31}Assets/Mud/MUD_OceanFoam.emat";
	static const int N = 48;                // layer cells per side (49 x 49 verts, must match MUD_OceanBuf)
	static const float LAYER_M = 32;        // layer side, m
	static const float LIFT = 0.03;         // layer over the real surface
	static const float EDGE_M = 3;          // outer ring that dips under the real sea
	static const float EDGE_SINK = 0.2;
	static const float UV_SPAN = 128;       // world metres per 0..1 of UV (texture fixed to the world, not the layer)
	static const float SEA_DEPTH = 0.5;     // anchor counts as 'at sea' where our ground is this far under the ocean base
	static const float DRAW_EVERY = 0.033;  // s between layer rebuilds (flags-0 MeshObject: rebuild, never UpdateVerts)
	static const float AHEAD = 60;          // virtual boat circle centre this far in front of the camera
	static const float R_BOAT = 35;         // virtual boat circle radius
	static const float BOAT_L = 4;          // virtual boat half length / half width, m
	static const float BOAT_W = 1.3;
	static const float BOW_K = 0.08;        // wake height = BOW_K * v^2 / 2g, clamped to WAKE_MAX
	static const float WAKE_MAX = 0.5;
	static const float KELVIN = 0.354;      // tan(19.47 deg): Kelvin wake arm spread per m behind
	static const float DECAY_M = 10;        // wake halves over this many m behind the bow
	static const float WAKE_LEN = 60;       // wake gone this far behind
	static const int FOAM_PTS = 60;         // trail points per ribbon (must match MUD_OceanBuf)
	static const float FOAM_EVERY = 0.15;   // s between trail points (60 x 0.15 = 9 s of foam)
	static const float FOAM_UP = 0.05;      // foam over the layer
	static const int MEASURE_N = 7;         // MEASURE grid 7 x 7 ...
	static const float MEASURE_STEP = 8;    // ... 8 m apart around the anchor
	static const float MEASURE_EVERY = 0.25;
	static const int STAGES = 5;
	static const bool ENABLED = false;      // PARKED 2026-10-24 (user after the WB test: 'didnt look so good, park it')

	protected static int s_iStage;          // 0 off, 1 measure, 2 pond, 3 ocean, 4 glass
	protected static int s_iSpeed;          // virtual boat speed level
	protected static int s_iWind;           // 0 none, 1 = 0 m/s, 2 = 25 m/s
	protected static float s_fLastT = -1;
	protected static ref MUD_OceanBuf s_Buf;
	protected static IEntity s_Layer;
	protected static IEntity s_Foam;
	protected static ref Resource s_LayerRes;   // keep the mesh resources alive while drawn
	protected static ref Resource s_FoamRes;
	// virtual boat
	protected static vector s_vCircle;
	protected static float s_fAngle;
	protected static vector s_vBoat;
	protected static vector s_vBoatFwd = vector.Forward;
	protected static float s_fBoatV;
	protected static vector s_vLastAnchor;
	protected static float s_fLastAnchorT = -1;
	// trail (stern points, newest last)
	protected static ref array<vector> s_aTrailP = {};
	protected static ref array<vector> s_aTrailR = {};    // right vector at that point
	protected static ref array<float> s_aTrailV = {};     // speed then
	protected static ref array<float> s_aTrailT = {};     // time then
	protected static float s_fNextTrail;
	protected static float s_fNextDraw;
	protected static float s_fNextMeasure;
	protected static float s_fNextStat;
	// MEASURE window (5 s)
	protected static float s_fHMin;
	protected static float s_fHMax;
	protected static float s_fHAbs;
	protected static float s_fDispMax;
	protected static int s_iSamples;
	protected static float s_fPtMin;            // one point (the anchor) over time
	protected static float s_fPtMax;
	// layer stats (5 s)
	protected static int s_iBuilds;
	protected static int s_iMs;
	protected static int s_iFails;
	protected static bool s_bAtSea;

	//------------------------------------------------------------------------------------------------
	//! Every frame from MUD_MudLab.Tick
	static void Tick(float time)
	{
		if (!ENABLED || !MUD_TestKit.TEST_KIT || RplSession.Mode() == RplMode.Dedicated)
			return;
		if (time < s_fLastT)
		{
			s_iStage = 0;       // new play: old entities are gone with the old world
			s_iSpeed = 0;
			s_iWind = 0;
			s_Layer = null;
			s_Foam = null;
			s_aTrailP.Clear();
			s_aTrailR.Clear();
			s_aTrailV.Clear();
			s_aTrailT.Clear();
		}
		s_fLastT = time;
#ifdef ENABLE_DIAG
		if (Debug.KeyState(KeyCode.KC_HOME))
		{
			Debug.ClearKey(KeyCode.KC_HOME);
			s_iStage = (s_iStage + 1) % STAGES;
			if (s_iStage == 0)
				Clear();
			else
				DropLayer();    // new material: fresh layer next draw
			ResetWindow(time);
			Show();
		}
		if (Debug.KeyState(KeyCode.KC_END) && s_iStage > 0)
		{
			Debug.ClearKey(KeyCode.KC_END);
			s_iSpeed = (s_iSpeed + 1) % 4;
			if (s_iSpeed == 1)
				StartBoat();
			Show();
		}
		if (Debug.KeyState(KeyCode.KC_PRIOR) && s_iStage > 0)
		{
			Debug.ClearKey(KeyCode.KC_PRIOR);
			s_iWind = (s_iWind + 1) % 3;
			ApplyWind();
			ResetWindow(time);
			Show();
		}
#endif
		if (s_iStage == 0)
			return;
		vector anchorP;
		vector anchorF;
		float anchorV;
		if (!Anchor(time, anchorP, anchorF, anchorV))
			return;
		if (time >= s_fNextMeasure)
		{
			s_fNextMeasure = time + MEASURE_EVERY;
			Measure(anchorP);
		}
		if (time >= s_fNextStat)
		{
			s_fNextStat = time + 5;
			Stat();
			ResetWindow(time);
		}
		if (s_iStage < 2)
			return;
		Trail(time, anchorP, anchorF, anchorV);
		if (time >= s_fNextDraw)
		{
			s_fNextDraw = time + DRAW_EVERY;
			int t0 = System.GetTickCount();
			DrawLayer(time, anchorP, anchorF, anchorV);
			DrawFoam(time);
			s_iMs += System.GetTickCount() - t0;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Show()
	{
		string txt = StageName() + "\nboat " + SpeedName() + " (End)\nwind " + WindName() + " (Page Up)";
		Print(string.Format("MUD ocean probe: %1 | boat %2 | wind %3 | keys Home stage, End boat, Page Up wind", StageName(), SpeedName(), WindName()), LogLevel.NORMAL);
		SCR_HintManagerComponent.ShowCustomHint(txt, "Mud ocean probe", 6, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static string StageName()
	{
		if (s_iStage == 1)
			return "MEASURE (real waves only)";
		if (s_iStage == 2)
			return "LAYER pond material";
		if (s_iStage == 3)
			return "LAYER ocean material";
		if (s_iStage == 4)
			return "LAYER glass material";
		return "OFF";
	}

	//------------------------------------------------------------------------------------------------
	protected static float SpeedOf(int level)
	{
		if (level == 1)
			return 4;
		if (level == 2)
			return 8;
		if (level == 3)
			return 14;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static string SpeedName()
	{
		if (s_iSpeed == 0)
			return "OFF (layer follows you)";
		return string.Format("virtual %1 m/s", SpeedOf(s_iSpeed));
	}

	//------------------------------------------------------------------------------------------------
	protected static string WindName()
	{
		if (s_iWind == 1)
			return "override 0 m/s";
		if (s_iWind == 2)
			return "override 25 m/s";
		return "NONE (weather)";
	}

	//------------------------------------------------------------------------------------------------
	protected static BaseWeatherManagerEntity Weather()
	{
		ChimeraWorld cw = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!cw)
			return null;
		return cw.GetTimeAndWeatherManager();
	}

	//------------------------------------------------------------------------------------------------
	protected static void ApplyWind()
	{
		BaseWeatherManagerEntity w = Weather();
		if (!w)
		{
			Print("MUD ocean probe: no weather manager - wind not changed", LogLevel.WARNING);
			return;
		}
		bool ok;
		if (s_iWind == 0)
			ok = w.SetWindSpeedOverride(false, 0);
		else if (s_iWind == 1)
			ok = w.SetWindSpeedOverride(true, 0);
		else
			ok = w.SetWindSpeedOverride(true, 25);
		Print(string.Format("MUD ocean probe: wind %1 -> SetWindSpeedOverride %2, wind now %3 m/s", WindName(), ok, w.GetWindSpeed()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartBoat()
	{
		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		vector fwd = cam[2];
		fwd[1] = 0;
		if (fwd.LengthSq() < 0.0001)
			fwd = vector.Forward;
		fwd.Normalize();
		s_vCircle = cam[3] + fwd * AHEAD;
		s_fAngle = 0;
		s_aTrailP.Clear();
		s_aTrailR.Clear();
		s_aTrailV.Clear();
		s_aTrailT.Clear();
		Print(string.Format("MUD ocean probe: virtual boat circle centre %1, radius %2 m", s_vCircle, R_BOAT), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Where the layer sits and what makes the wake: the virtual boat if on, else the local player / their vehicle.
	//! False = no anchor yet (no player).
	protected static bool Anchor(float time, out vector p, out vector f, out float v)
	{
		BaseWorld world = GetGame().GetWorld();
		if (s_iSpeed > 0)
		{
			float dt = time - s_fLastAnchorT;
			if (s_fLastAnchorT < 0 || dt > 0.2)
				dt = 0;
			s_fLastAnchorT = time;
			s_fBoatV = SpeedOf(s_iSpeed);
			s_fAngle += s_fBoatV * dt / R_BOAT;
			float x = s_vCircle[0] + R_BOAT * Math.Cos(s_fAngle);
			float z = s_vCircle[2] + R_BOAT * Math.Sin(s_fAngle);
			s_vBoat = Vector(x, world.GetOceanHeight(x, z), z);
			s_vBoatFwd = Vector(-Math.Sin(s_fAngle), 0, Math.Cos(s_fAngle));     // counter-clockwise tangent
			p = s_vBoat;
			f = s_vBoatFwd;
			v = s_fBoatV;
			return true;
		}
		ChimeraCharacter ch = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!ch)
			return false;
		IEntity a = ch;
		if (ch.IsInVehicle())
		{
			IEntity veh = CompartmentAccessComponent.GetVehicleIn(ch);
			if (veh)
				a = veh;
		}
		p = a.GetOrigin();
		vector m[4];
		a.GetWorldTransform(m);
		f = m[2];
		f[1] = 0;
		if (f.LengthSq() < 0.0001)
			f = vector.Forward;
		f.Normalize();
		v = 0;
		if (s_fLastAnchorT >= 0 && time > s_fLastAnchorT)
		{
			vector d = p - s_vLastAnchor;
			d[1] = 0;
			v = d.Length() / (time - s_fLastAnchorT);
			if (v > 40)
				v = 0;      // teleport / respawn
		}
		s_vLastAnchor = p;
		s_fLastAnchorT = time;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ResetWindow(float time)
	{
		s_fHMin = 1000;
		s_fHMax = -1000;
		s_fHAbs = 0;
		s_fDispMax = 0;
		s_iSamples = 0;
		s_fPtMin = 1000;
		s_fPtMax = -1000;
		s_iBuilds = 0;
		s_iMs = 0;
		s_iFails = 0;
		s_fNextStat = time + 5;
	}

	//------------------------------------------------------------------------------------------------
	//! Real wave height (minus ocean base) on a 7 x 7 grid around p, plus p itself over time
	protected static void Measure(vector p)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world.IsOcean())
			return;
		float base = world.GetOceanBaseHeight();
		float half = (MEASURE_N - 1) * MEASURE_STEP * 0.5;
		for (int j = 0; j < MEASURE_N; j++)
		{
			float fj = j;
			for (int i = 0; i < MEASURE_N; i++)
			{
				float fi = i;
				vector hd = world.GetOceanHeightAndDisplace(p[0] - half + fi * MEASURE_STEP, p[2] - half + fj * MEASURE_STEP);
				float h = hd[1] - base;
				s_fHMin = Math.Min(s_fHMin, h);
				s_fHMax = Math.Max(s_fHMax, h);
				s_fHAbs += Math.AbsFloat(h);
				s_fDispMax = Math.Max(s_fDispMax, Math.Sqrt(hd[0] * hd[0] + hd[2] * hd[2]));
				s_iSamples++;
			}
		}
		float hp = world.GetOceanHeight(p[0], p[2]) - base;
		s_fPtMin = Math.Min(s_fPtMin, hp);
		s_fPtMax = Math.Max(s_fPtMax, hp);
		s_bAtSea = AtSea(world, p);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stat()
	{
		BaseWorld world = GetGame().GetWorld();
		float wind = -1;
		BaseWeatherManagerEntity w = Weather();
		if (w)
			wind = w.GetWindSpeed();
		float mean = 0;
		if (s_iSamples > 0)
			mean = s_fHAbs / s_iSamples;
		Print(string.Format("MUD ocean probe: 5 s | %1 | wind %2 m/s (%3) | wave h %4 .. %5 m (range %6), mean |h| %7, max sideways displace %8",
			StageName(), wind, WindName(), s_fHMin, s_fHMax, s_fHMax - s_fHMin, mean, s_fDispMax), LogLevel.NORMAL);
		Print(string.Format("MUD ocean probe: 5 s | anchor point range %1 m, samples %2, ocean %3 base %4, anchor at sea %5 | layer builds %6, fails %7, %8 ms (WB, not perf)",
			s_fPtMax - s_fPtMin, s_iSamples, world.IsOcean(), world.GetOceanBaseHeight(), s_bAtSea, s_iBuilds, s_iFails, s_iMs), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Our ground (as the patches draw it) at p is SEA_DEPTH under the ocean base; holed cell with no answer = at sea
	//! when p itself is under the base (swimmer / boat)
	protected static bool AtSea(BaseWorld world, vector p)
	{
		if (!world.IsOcean())
			return false;
		float base = world.GetOceanBaseHeight();
		float dug;
		float undug;
		if (MUD_WorldGroundEntity.DugUndug(p[0], p[2], dug, undug))
			return dug < base - SEA_DEPTH;
		float y = world.GetSurfaceY(p[0], p[2]);
		if (y > -200)
			return y < base - SEA_DEPTH;
		return p[1] < base + 0.5;
	}

	//------------------------------------------------------------------------------------------------
	//! Stern point every FOAM_EVERY s while moving; points older than the ribbon length drop off
	protected static void Trail(float time, vector p, vector f, float v)
	{
		if (time < s_fNextTrail)
			return;
		s_fNextTrail = time + FOAM_EVERY;
		float len = BOAT_L;
		if (s_iSpeed == 0)
			len = 1;
		if (v > 1)
		{
			s_aTrailP.Insert(p - f * len);
			s_aTrailR.Insert(Vector(f[2], 0, -f[0]));
			s_aTrailV.Insert(v);
			s_aTrailT.Insert(time);
		}
		while (s_aTrailP.Count() > FOAM_PTS)
		{
			s_aTrailP.RemoveOrdered(0);
			s_aTrailR.RemoveOrdered(0);
			s_aTrailV.RemoveOrdered(0);
			s_aTrailT.RemoveOrdered(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Our wake height at world x, z for a boat at p heading f with speed v (straight-line Kelvin model)
	protected static float Wake(float x, float z, vector p, vector f, float v, float hl, float hw)
	{
		if (v < 0.5)
			return 0;
		float A = Math.Min(BOW_K * v * v / 19.62, WAKE_MAX);
		float rx = x - p[0];
		float rz = z - p[2];
		float along = rx * f[0] + rz * f[2];
		float side = rx * f[2] - rz * f[0];
		float aside = Math.AbsFloat(side);
		float bw = hw + 0.5;
		float h = 0;
		float bx = along - hl * 0.7;
		h += A * Math.Pow(2.71828, -bx * bx - side * side / (bw * bw));       // bow hump
		float behind = hl * 0.7 - along;
		if (behind <= 0 || behind > WAKE_LEN)
			return h;
		float lambda = Math.Clamp(6.2832 * v * v / 9.81, 1.5, 25);
		float wave = Math.Cos(6.2832 / lambda * behind);
		float decay = (1 - behind / WAKE_LEN) / (1 + behind / DECAY_M);
		float s0 = hw * 0.8 + KELVIN * behind;
		float d = aside - s0;
		float wid = 0.5 + 0.06 * behind;
		h += A * 0.6 * decay * Math.Pow(2.71828, -d * d / (wid * wid)) * (0.6 + 0.4 * wave);    // the two V arms
		float inside = Math.Clamp((s0 - aside) / 1.5, 0, 1);
		h += A * 0.35 * decay * inside * wave;                                                 // transverse waves
		return h;
	}

	//------------------------------------------------------------------------------------------------
	protected static void DrawLayer(float time, vector ap, vector af, float av)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!AtSea(world, ap))
		{
			if (s_Layer)
				s_Layer.ClearFlags(EntityFlags.VISIBLE, false);
			return;
		}
		if (!s_Buf)
			s_Buf = new MUD_OceanBuf();
		float cell = LAYER_M / N;
		float half = LAYER_M * 0.5;
		// snap to the cell grid so the vertices do not swim over the real waves
		float ox = Math.Round(ap[0] / cell) * cell;
		float oz = Math.Round(ap[2] / cell) * cell;
		float oy = world.GetOceanBaseHeight();
		float ux0 = Math.Floor((ox - half) / UV_SPAN) * UV_SPAN;
		float uz0 = Math.Floor((oz - half) / UV_SPAN) * UV_SPAN;
		float hl = BOAT_L;
		float hw = BOAT_W;
		if (s_iSpeed == 0)
		{
			hl = 2;
			hw = 1;
		}
		for (int j = 0; j <= N; j++)
		{
			float fj = j;
			float z = oz - half + fj * cell;
			for (int i = 0; i <= N; i++)
			{
				float fi = i;
				float x = ox - half + fi * cell;
				int v = j * (N + 1) + i;
				float y = world.GetOceanHeight(x, z) + LIFT;
				float e = Math.Min(Math.Min(fi, fj), Math.Min(N - fi, N - fj)) * cell;
				float k = 1;
				if (e < EDGE_M)
				{
					k = e / EDGE_M;
					y -= (LIFT + EDGE_SINK) * (1 - k);
				}
				y += k * Wake(x, z, ap, af, av, hl, hw);
				s_Buf.m_aV[v] = Vector(x - ox, y - oy, z - oz);
				s_Buf.m_aUV[v * 2] = (x - ux0) / UV_SPAN;
				s_Buf.m_aUV[v * 2 + 1] = (z - uz0) / UV_SPAN;
			}
		}
		int n;
		for (int qj = 0; qj < N; qj++)
		{
			for (int qi = 0; qi < N; qi++)
			{
				int a = qj * (N + 1) + qi;
				int b = a + N + 1;
				int c = a + 1;
				int d = b + 1;
				s_Buf.m_aI[n] = a;
				s_Buf.m_aI[n + 1] = b;
				s_Buf.m_aI[n + 2] = c;
				s_Buf.m_aI[n + 3] = c;
				s_Buf.m_aI[n + 4] = b;
				s_Buf.m_aI[n + 5] = d;
				n += 6;
			}
		}
		ResourceName mat = MAT_POND;
		if (s_iStage == 3)
			mat = MAT_OCEAN;
		else if (s_iStage == 4)
			mat = MAT_GLASS;
		int numVerts[] = {2401};
		int numIndices[] = {13824};
		string materials[] = {mat};
		Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!res || !res.GetResource())
		{
			s_iFails++;
			if (s_iFails == 1)
				Print(string.Format("MUD ocean probe: MeshObject.Create failed - material '%1' did not load?", mat), LogLevel.ERROR);
			return;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		// indices first: UpdateVerts rebuilds normals from the triangle list
		mesh.UpdateIndices(0, s_Buf.m_aI);
		mesh.UpdateVerts(0, s_Buf.m_aV, s_Buf.m_aUV);
		vector org = Vector(ox, oy, oz);
		if (!s_Layer)
			s_Layer = Spawn(org);
		if (!s_Layer)
			return;
		s_Layer.SetOrigin(org);
		s_Layer.SetObject(mesh, "");
		s_Layer.SetFlags(EntityFlags.VISIBLE, false);
		s_Layer.Update();
		s_LayerRes = res;
		s_iBuilds++;
	}

	//------------------------------------------------------------------------------------------------
	//! Foam: 3 ribbons on the trail - centre (prop wash) and the two Kelvin arms, which spread sideways by
	//! KELVIN * v * age from where the boat was. Thin, then wider, then gone at the tail.
	protected static void DrawFoam(float time)
	{
		int cnt = s_aTrailP.Count();
		if (cnt < 3)
		{
			if (s_Foam)
				s_Foam.ClearFlags(EntityFlags.VISIBLE, false);
			return;
		}
		BaseWorld world = GetGame().GetWorld();
		vector org = s_aTrailP[cnt - 1];
		org[1] = world.GetOceanBaseHeight();
		float hw = BOAT_W;
		if (s_iSpeed == 0)
			hw = 0.6;
		float span = FOAM_PTS * FOAM_EVERY;
		for (int r = 0; r < 3; r++)
		{
			for (int k = 0; k < FOAM_PTS; k++)
			{
				int t = cnt - 1 - k;        // k = 0 newest
				int vi = (r * FOAM_PTS + k) * 2;
				if (t < 0)
				{
					// past the trail: collapse onto the last real pair (zero-area, as MUD_CoarseSkin's unused cells)
					s_Buf.m_aFV[vi] = s_Buf.m_aFV[vi - 2];
					s_Buf.m_aFV[vi + 1] = s_Buf.m_aFV[vi - 1];
					s_Buf.m_aFUV[vi * 2] = 0;
					s_Buf.m_aFUV[vi * 2 + 1] = 1;
					s_Buf.m_aFUV[vi * 2 + 2] = 1;
					s_Buf.m_aFUV[vi * 2 + 3] = 1;
					continue;
				}
				float age = time - s_aTrailT[t];
				float fade = Math.Clamp(1 - age / span, 0, 1);
				vector p = s_aTrailP[t];
				vector rv = s_aTrailR[t];
				float spread = 0;
				float w = (hw * 0.8 + 0.25 * age) * fade;           // centre: prop wash widens
				if (r > 0)
				{
					spread = hw + KELVIN * s_aTrailV[t] * age;
					if (r == 2)
						spread = -spread;
					w = (0.3 + 0.08 * age) * fade;                  // arms: thin foam lines
				}
				vector c = p + rv * spread;
				float y = world.GetOceanHeight(c[0], c[2]) + LIFT + FOAM_UP;
				vector a = c - rv * w;
				vector b = c + rv * w;
				s_Buf.m_aFV[vi] = Vector(a[0] - org[0], y - org[1], a[2] - org[2]);
				s_Buf.m_aFV[vi + 1] = Vector(b[0] - org[0], y - org[1], b[2] - org[2]);
				float fk = k;
				float vv = fk / FOAM_PTS;
				s_Buf.m_aFUV[vi * 2] = 0;
				s_Buf.m_aFUV[vi * 2 + 1] = vv;
				s_Buf.m_aFUV[vi * 2 + 2] = 1;
				s_Buf.m_aFUV[vi * 2 + 3] = vv;
			}
		}
		int n;
		for (int r2 = 0; r2 < 3; r2++)
		{
			for (int s = 0; s < FOAM_PTS - 1; s++)
			{
				int a0 = (r2 * FOAM_PTS + s) * 2;
				int b0 = a0 + 1;
				int a1 = a0 + 2;
				int b1 = a0 + 3;
				s_Buf.m_aFI[n] = a0;
				s_Buf.m_aFI[n + 1] = a1;
				s_Buf.m_aFI[n + 2] = b0;
				s_Buf.m_aFI[n + 3] = b0;
				s_Buf.m_aFI[n + 4] = a1;
				s_Buf.m_aFI[n + 5] = b1;
				n += 6;
			}
		}
		int numVerts[] = {360};
		int numIndices[] = {1062};
		string materials[] = {MAT_FOAM};
		Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!res || !res.GetResource())
		{
			s_iFails++;
			if (s_iFails == 1)
				Print(string.Format("MUD ocean probe: foam MeshObject.Create failed - material '%1' did not load?", MAT_FOAM), LogLevel.ERROR);
			return;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		mesh.UpdateIndices(0, s_Buf.m_aFI);
		mesh.UpdateVerts(0, s_Buf.m_aFV, s_Buf.m_aFUV);
		if (!s_Foam)
			s_Foam = Spawn(org);
		if (!s_Foam)
			return;
		s_Foam.SetOrigin(org);
		s_Foam.SetObject(mesh, "");
		s_Foam.SetFlags(EntityFlags.VISIBLE, false);
		s_Foam.Update();
		s_FoamRes = res;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Spawn(vector org)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = org;
		IEntity ent = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
		if (!ent)
			Print("MUD ocean probe: SpawnEntity returned null", LogLevel.ERROR);
		return ent;
	}

	//------------------------------------------------------------------------------------------------
	protected static void DropLayer()
	{
		if (s_Layer && !s_Layer.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(s_Layer);
		s_Layer = null;
		s_LayerRes = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Clear()
	{
		DropLayer();
		if (s_Foam && !s_Foam.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(s_Foam);
		s_Foam = null;
		s_FoamRes = null;
		s_iSpeed = 0;
		if (s_iWind != 0)
		{
			s_iWind = 0;
			ApplyWind();
		}
		s_aTrailP.Clear();
		s_aTrailR.Clear();
		s_aTrailV.Clear();
		s_aTrailT.Clear();
	}
}
