//! Rut and wheel debugger (user 2026-10-19: 'a better debugger for debugging ruts and wheels'). Diag exe / Workbench
//! only: diag menu GameCode > Mud > Ruts and wheels (next to 'Displaced ruts'). Every switch off = nothing runs.
//! - Wheels (also TEMP magenta / violet = WheelGetPosition read as world / local, server side): sphere per wheel contact - green = leading axle cuts, orange = rear wheel off the rut cuts (TRAIL CUT),
//!   blue = rides in the rut, grey = in the air. Solid = server ruts, wireframe = client look ruts. Arrow to the probe
//!   point RUT_TRAIL_AHEAD m ahead (the point TRAIL CUT reads).
//! - Rut across: a white line across each wheel's probe point (-0.8 .. +0.8 m), red dot = deepest rut there. Red dot on
//!   the arrow = the wheel is centred on the rut; off to the side = the rut misses the wheel.
//! - Rut pieces: the segments that were cut, kept SEG_KEEP s (yellow = server op, cyan = client look cut).
//! - Depth dots: rut depth within DOT_R m of each vehicle (yellow = under the dirt line, orange = dirt, red = at least
//!   RUT_FIRST_D). 'drawn floor' = the look floor (what you see), off = the server floor (what the wheels drive on).
//! - Text table: per vehicle and side, speed, direction and per wheel role / depth ahead / where the rut centre is.
//! Hooks: MUD_VoxelProbe.LeadAxle (OnAxle), CarveFloor (OpSegs), MUD_LookRuts.Cut (Seg) and Tick (Tick), DispMenu (Menu).
class MUD_RutDebug
{
	static const float SEG_KEEP = 4.0;      // s a cut rut piece stays drawn
	static const int SEG_MAX = 600;         // newest pieces kept
	static const float DOT_R = 3.0;         // m around the vehicle for depth dots
	static const float DOT_STEP = 0.25;     // m between depth dots (= the floor sample grid)
	static const float DIRT_D = 0.09;       // m: the groove reveal is full dirt from about here (ruts README step 12)
	static const int RD_WHEELS = 0;         // switch order = the SCR_DebugMenuID MUD_RD_* order
	static const int RD_ACROSS = 1;
	static const int RD_SEGS = 2;
	static const int RD_DEPTH = 3;
	static const int RD_DEPTH_LOOK = 4;
	static const int RD_TABLE = 5;

	protected static bool s_bMenu;
	protected static ref array<vector> s_aSegA = {};
	protected static ref array<vector> s_aSegB = {};
	protected static ref array<float> s_aSegT = {};
	protected static ref array<int> s_aSegLook = {};
	protected static ref array<string> s_aRows = {};
	protected static ref array<string> s_aShow = {};

	//------------------------------------------------------------------------------------------------
	//! Registers the menu (MUD_VoxelProbe.DispMenu calls it after 'Mud' exists)
	static void Menu()
	{
#ifdef ENABLE_DIAG
		if (s_bMenu)
			return;
		if (SCR_DebugMenuID.MUD_RD_TABLE >= 512)
		{
			Print(string.Format("MUD rut debug: diag ids reach %1 (max 511) - no Ruts and wheels menu", SCR_DebugMenuID.MUD_RD_TABLE), LogLevel.ERROR);
			return;
		}
		s_bMenu = true;
		DiagMenu.RegisterMenu(SCR_DebugMenuID.MUD_RD_MENU, "Ruts and wheels", "Mud");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MUD_RD_WHEELS, "", "Wheels: who cuts", "Ruts and wheels");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MUD_RD_ACROSS, "", "Rut across each wheel", "Ruts and wheels");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MUD_RD_SEGS, "", "Rut pieces (4 s)", "Ruts and wheels");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MUD_RD_DEPTH, "", "Depth dots", "Ruts and wheels");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MUD_RD_DEPTH_LOOK, "", "Depth dots: drawn floor", "Ruts and wheels");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MUD_RD_TABLE, "", "Text table", "Ruts and wheels");
		Print("MUD rut debug: diag menu GameCode > Mud > Ruts and wheels", LogLevel.NORMAL);
#endif
	}

	//------------------------------------------------------------------------------------------------
	static bool On(int n)
	{
#ifdef ENABLE_DIAG
		if (!s_bMenu)
			return false;
		return DiagMenu.GetBool(SCR_DebugMenuID.MUD_RD_WHEELS + n);
#else
		return false;
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.LeadAxle, every frame per vehicle and side: lead[w] 1 = leading axle, 2 = TRAIL CUT, 0 = rides
	static void OnAxle(Vehicle veh, VehicleWheeledSimulation sim, vector vel, array<int> lead, bool look)
	{
		bool wheels = On(RD_WHEELS);
		bool across = On(RD_ACROSS);
		bool table = On(RD_TABLE);
		bool dots = On(RD_DEPTH) && On(RD_DEPTH_LOOK) == look;
		if (!wheels && !across && !table && !dots)
			return;
		float hs = Math.Sqrt(vel[0] * vel[0] + vel[2] * vel[2]);
		float dx = 0;
		float dz = 0;
		if (hs > 0.2)
		{
			dx = vel[0] / hs;
			dz = vel[2] / hs;
		}
		string side = "server";
		if (look)
			side = "look";
		if (table)
		{
			vector lv = veh.VectorToLocal(vel);
			string dir = "stopped";
			if (hs > 0.2 && lv[2] >= 0)
				dir = "forward";
			else if (hs > 0.2)
				dir = "reverse";
			s_aRows.Insert(string.Format("%1 | %2 | %3 m/s %4 | steering %5", side, veh.GetOrigin().ToString(), hs.ToString(-1, 1), dir, sim.GetSteering().ToString(-1, 2)));
		}
		int count = sim.WheelCount();
		for (int w = 0; w < count; w++)
		{
			if (!sim.WheelHasContact(w))
			{
				if (wheels)
					Shape.CreateSphere(ARGB(255, 128, 128, 128), Flags(look), sim.WheelGetContactPosition(w), Rad(look));
				if (table)
					s_aRows.Insert(string.Format("  wheel %1: in the air", w));
				continue;
			}
			vector cp = MUD_VoxelProbe.RutPoint(veh, sim, w);     // where the wheel cuts (RUT AT THE HUB)
			int role = 0;
			if (w < lead.Count())
				role = lead[w];
			int col = ARGB(255, 60, 140, 255);
			string roleS = "rides";
			if (role == 1)
			{
				col = ARGB(255, 40, 220, 60);
				roleS = "LEADS, cuts";
			}
			else if (role == 2)
			{
				col = ARGB(255, 255, 150, 0);
				roleS = "OFF RUT, cuts";
			}
			if (wheels)
			{
				Shape.CreateSphere(col, Flags(look), cp, Rad(look));
				if (!look)
				{
					// TEMP 2026-10-19: WheelGetPosition - magenta = read as world (wrong: it is vehicle-local, WB 00:38 log),
					// violet = read as vehicle-local (= RutPoint's x / z).
					vector wp = sim.WheelGetPosition(w);
					Shape.CreateSphere(ARGB(255, 255, 0, 255), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, wp, 0.1);
					Shape.CreateSphere(ARGB(255, 140, 60, 255), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, veh.CoordToParent(wp), 0.1);
				}
			}
			if (hs <= 0.2)
			{
				if (table)
					s_aRows.Insert(string.Format("  wheel %1: %2 (stopped)", w, roleS));
				continue;
			}
			float ax = cp[0] + dx * MUD_VoxelProbe.RUT_TRAIL_AHEAD;
			float az = cp[2] + dz * MUD_VoxelProbe.RUT_TRAIL_AHEAD;
			vector ap = Vector(ax, cp[1] + 0.05, az);
			if (wheels)
				Shape.CreateArrow(cp, ap, 0.1, col, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);
			float dep = MUD_VoxelProbe.RutDepthAt(ax, az, false);     // the real rut, as InLeadRut decides
			// across: right of the travel = (dz, -dx) (Enfusion x right, z forward)
			float best = -1;
			float bestO = 0;
			for (int s = -8; s <= 8; s++)
			{
				float o = s * 0.1;
				float d = MUD_VoxelProbe.RutDepthAt(ax + dz * o, az - dx * o, look);
				if (d > best)
				{
					best = d;
					bestO = o;
				}
			}
			if (across)
			{
				Shape.CreateLine(ARGB(255, 255, 255, 255), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, Vector(ax - dz * 0.8, ap[1], az + dx * 0.8), Vector(ax + dz * 0.8, ap[1], az - dx * 0.8));
				if (best > 0.005)
					Shape.CreateSphere(ARGB(255, 255, 30, 30), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, Vector(ax + dz * bestO, ap[1], az - dx * bestO), 0.06);
			}
			if (table)
			{
				string where = "no rut";
				if (best > 0.005)
				{
					where = "centre here";
					if (bestO > 0.05)
						where = string.Format("centre %1 cm RIGHT", Math.Round(bestO * 100));
					else if (bestO < -0.05)
						where = string.Format("centre %1 cm LEFT", Math.Round(-bestO * 100));
				}
				s_aRows.Insert(string.Format("  wheel %1 %6: %2 | ahead %3 cm deep (real rut) | deepest %4 cm, %5", w, roleS, Math.Round(dep * 100), Math.Round(best * 100), where, sim.WheelGetName(w)));
			}
		}
		if (dots)
			Dots(veh.GetOrigin(), look);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Dots(vector c, bool look)
	{
		int n = Math.Round(DOT_R / DOT_STEP);
		for (int k = -n; k <= n; k++)
		{
			for (int i = -n; i <= n; i++)
			{
				float x = c[0] + i * DOT_STEP;
				float z = c[2] + k * DOT_STEP;
				float d = MUD_VoxelProbe.RutDepthAt(x, z, look);
				if (d < 0.005)
					continue;
				float u;
				if (!MUD_WorldGroundEntity.UndugY(x, z, u))
					continue;
				int col = ARGB(255, 255, 230, 0);
				if (d >= MUD_VoxelProbe.RUT_FIRST_D - 0.005)
					col = ARGB(255, 255, 30, 30);
				else if (d >= DIRT_D)
					col = ARGB(255, 255, 140, 0);
				Shape.CreateSphere(col, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, Vector(x, u - d + 0.02, z), 0.035);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.CarveFloor: the op's segments (server rut, also replayed on clients)
	static void OpSegs(array<vector> a, array<vector> b)
	{
		if (!On(RD_SEGS))
			return;
		for (int i = 0; i < a.Count() && i < b.Count(); i++)
			Seg(a[i], b[i], false);
	}

	//------------------------------------------------------------------------------------------------
	//! One cut rut piece (look = client look cut)
	static void Seg(vector a, vector b, bool look)
	{
		if (!On(RD_SEGS))
			return;
		s_aSegA.Insert(a);
		s_aSegB.Insert(b);
		s_aSegT.Insert(System.GetTickCount() * 0.001);
		int l = 0;
		if (look)
			l = 1;
		s_aSegLook.Insert(l);
		if (s_aSegA.Count() > SEG_MAX)
		{
			s_aSegA.RemoveOrdered(0);
			s_aSegB.RemoveOrdered(0);
			s_aSegT.RemoveOrdered(0);
			s_aSegLook.RemoveOrdered(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame on rendering machines (MUD_LookRuts.Tick): rut pieces + the text table
	static void Tick()
	{
		if (On(RD_SEGS))
		{
			float now = System.GetTickCount() * 0.001;
			while (!s_aSegT.IsEmpty() && now - s_aSegT[0] > SEG_KEEP)
			{
				s_aSegA.RemoveOrdered(0);
				s_aSegB.RemoveOrdered(0);
				s_aSegT.RemoveOrdered(0);
				s_aSegLook.RemoveOrdered(0);
			}
			for (int i = 0; i < s_aSegA.Count(); i++)
			{
				int col = ARGB(255, 255, 230, 0);
				float up = 0.08;
				if (s_aSegLook[i] != 0)
				{
					col = ARGB(255, 0, 230, 255);
					up = 0.12;
				}
				Shape.CreateLine(col, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, s_aSegA[i] + Vector(0, up, 0), s_aSegB[i] + Vector(0, up, 0));
			}
		}
		else if (!s_aSegA.IsEmpty())
		{
			s_aSegA.Clear();
			s_aSegB.Clear();
			s_aSegT.Clear();
			s_aSegLook.Clear();
		}
		if (!On(RD_TABLE))
		{
			s_aRows.Clear();
			return;
		}
		if (!s_aRows.IsEmpty())
		{
			s_aShow.Clear();
			s_aShow.Copy(s_aRows);
			s_aRows.Clear();
		}
		DbgUI.Begin("MUD ruts and wheels", 20, 320);
		DbgUI.Text("green leads / orange off rut, cuts / blue rides | solid server, wire look");
		foreach (string row : s_aShow)
			DbgUI.Text(row);
		DbgUI.End();
	}

	//------------------------------------------------------------------------------------------------
	protected static ShapeFlags Flags(bool look)
	{
		if (look)
			return ShapeFlags.ONCE | ShapeFlags.NOZBUFFER | ShapeFlags.WIREFRAME;
		return ShapeFlags.ONCE | ShapeFlags.NOZBUFFER;
	}

	//------------------------------------------------------------------------------------------------
	protected static float Rad(bool look)
	{
		if (look)
			return 0.24;
		return 0.18;
	}
}

#ifdef ENABLE_DIAG
//! Ruts and wheels debug ids, after MUD_VoxelProbe's MUD_DISP_*; MUD_RD_WHEELS .. MUD_RD_TABLE in RD_* order
modded enum SCR_DebugMenuID
{
	MUD_RD_MENU,
	MUD_RD_WHEELS,
	MUD_RD_ACROSS,
	MUD_RD_SEGS,
	MUD_RD_DEPTH,
	MUD_RD_DEPTH_LOOK,
	MUD_RD_TABLE
}
#endif
