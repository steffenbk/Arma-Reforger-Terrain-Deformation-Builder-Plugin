//! TEMP raw per-frame logger (2026-10-11) for the driver stutter: while a MUD_StutterRobot run is live on
//! the local driver's machine, one CSV row per frame, no thresholds. One file per run in $profile:
//! MUD_framelog_<host|client>_run<N>_<tick>.csv (path printed in the 'MUD framelog' line).
//! Delete with its one call in MUD_JoltProbe.Tick.
//! Columns: tick ms (real clock), world time ms, dt, speed; physics body (GetDirectWorldTransform) pos +
//! yaw/pitch/roll; visual (entity transform) pos + ypr; visual - body in body space; camera
//! (BaseWorld.GetCurrentCamera) pos + yaw and camera - visual in vehicle space; inputs; per wheel:
//! contact, contact point height in body space, WheelGetPosition y (space not documented), slips, rpm.
//! probe_corr = MUD_JoltProbe body jump (m) when it counted a correction this frame, else 0.
//! No suspension-compression getter exists in VehicleWheeledSimulation; the two heights are the proxy.
//! Logged from OnPrepareControls / EOnFrame: the camera may be the previous frame's (order unproven).
class MUD_FrameLog
{
	protected static ref FileHandle s_File;
	protected static int s_iRun;
	protected static int s_iFrame;
	protected static float s_fCorr;    // MUD_JoltProbe body jump this frame (m), 0 = none

	//------------------------------------------------------------------------------------------------
	protected static string F(float v, int dec = 4)
	{
		return v.ToString(-1, dec);
	}

	//------------------------------------------------------------------------------------------------
	protected static string V(vector v, int dec = 4)
	{
		return F(v[0], dec) + "," + F(v[1], dec) + "," + F(v[2], dec);
	}

	//------------------------------------------------------------------------------------------------
	protected static vector ToLocal(vector m[4], vector worldPos)
	{
		vector d = worldPos - m[3];
		return Vector(vector.Dot(d, m[0]), vector.Dot(d, m[1]), vector.Dot(d, m[2]));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Open(VehicleWheeledSimulation sim, int run)
	{
		string role = "client";
		if (Replication.IsServer())
			role = "host";
		string path = string.Format("$profile:MUD_framelog_%1_run%2_%3.csv", role, run, System.GetTickCount());
		s_File = FileIO.OpenFile(path, FileMode.WRITE);
		s_iRun = run;
		s_iFrame = 0;
		Print(string.Format("MUD framelog: run %1 role %2 file %3 open %4", run, role, path, s_File != null));
		if (!s_File)
			return;
		string head = "frame,tick_ms,world_ms,dt,speed,body_x,body_y,body_z,body_yaw,body_pitch,body_roll,vis_x,vis_y,vis_z,vis_yaw,vis_pitch,vis_roll,off_x,off_y,off_z,cam_x,cam_y,cam_z,cam_yaw,camrel_x,camrel_y,camrel_z,throttle,brake,steer,gear,probe_corr";
		for (int w = 0; w < sim.WheelCount(); w++)
		{
			head += string.Format(",w%1_contact,w%1_cy,w%1_py,w%1_lat,w%1_long,w%1_rpm", w);
		}
		s_File.WriteLine(head);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Close()
	{
		if (!s_File)
			return;
		s_File.Close();
		s_File = null;
		Print(string.Format("MUD framelog: run %1 closed, %2 frames", s_iRun, s_iFrame));
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_JoltProbe: the local driver's body jumped off its path this frame (its "correction")
	static void MarkCorrection(float bodyJump)
	{
		s_fCorr = bodyJump;
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame, after MUD_JoltProbe's vehicle loop
	static void Tick(float dt, Vehicle mine)
	{
		float corr = s_fCorr;
		s_fCorr = 0;
		if (!MUD_StutterRobot.IsRunning() || !mine)
		{
			Close();
			return;
		}
		Physics ph = mine.GetPhysics();
		VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(mine.FindComponent(VehicleWheeledSimulation));
		if (!ph || !sim || !sim.IsValid())
			return;
		if (!s_File || s_iRun != MUD_StutterRobot.GetRun())
		{
			Close();
			Open(sim, MUD_StutterRobot.GetRun());
			if (!s_File)
				return;
		}

		vector bm[4];
		ph.GetDirectWorldTransform(bm);
		vector vm[4];
		mine.GetTransform(vm);
		vector cm[4];
		BaseWorld world = GetGame().GetWorld();
		world.GetCurrentCamera(cm);
		vector bAng = Math3D.MatrixToAngles(bm);
		vector vAng = Math3D.MatrixToAngles(vm);
		vector cAng = Math3D.MatrixToAngles(cm);

		string row = string.Format("%1,%2,%3,%4,%5", s_iFrame, System.GetTickCount(), F(world.GetWorldTime(), 1), F(dt, 5), F(ph.GetVelocity().Length()));
		row += "," + V(bm[3]) + "," + V(bAng, 3);
		row += "," + V(vm[3]) + "," + V(vAng, 3);
		row += "," + V(ToLocal(bm, vm[3]));
		row += "," + V(cm[3]) + "," + F(cAng[0], 3);
		row += "," + V(ToLocal(vm, cm[3]));
		row += string.Format(",%1,%2,%3,%4", F(sim.GetThrottle(), 2), F(sim.GetBrake(), 2), F(sim.GetSteering(), 2), sim.GetGear()) + "," + F(corr);
		for (int w = 0; w < sim.WheelCount(); w++)
		{
			bool contact = sim.WheelHasContact(w);
			float cy = 0;
			if (contact)
				cy = ToLocal(bm, sim.WheelGetContactPosition(w))[1];
			row += string.Format(",%1,%2,%3,%4,%5,%6", contact, F(cy), F(sim.WheelGetPosition(w)[1]),
				F(sim.WheelGetLateralSlip(w), 3), F(sim.WheelGetLongitudinalSlip(w), 3), F(sim.WheelGetRPM(w), 1));
		}
		s_File.WriteLine(row);
		s_iFrame++;
	}
}
