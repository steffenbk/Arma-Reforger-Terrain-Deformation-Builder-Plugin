//! TEMP probe v2 (2026-10-11): driver corrections and what led up to them. Delete with its one call in
//! MUD_WorldGroundEntity.EOnFrame and MUD_WorldGroundEntity.ProbeGround.
//! Engine (IDA, Exp 192624 ArmaReforgerSteamDiag.exe, NwkCarMovementComponent): the server never takes
//! the driver's position. It runs the driver's inputs through its own simulation and sends a correction
//! when the two differ by more than the SetAllowance limits; the driver then resets to the server state,
//! replays its unconfirmed moves and is meant to lerp the visual 35% a frame. So a correction = the
//! physics body jumps off its own path (body jump); the visual jump shows whether the lerp hid it.
//! Per correction: what the driver did and the ground under the wheels on this machine (newest heights
//! vs live collider, seconds since that collider last changed). Each 10 s: driving seconds and
//! corrections per surface and speed band - roads are vanilla terrain, the control.
class MUD_JoltProbe
{
	protected static ref map<Vehicle, vector> s_mBody = new map<Vehicle, vector>();
	protected static ref map<Vehicle, vector> s_mVel = new map<Vehicle, vector>();
	protected static ref map<Vehicle, vector> s_mVis = new map<Vehicle, vector>();
	protected static ref map<Vehicle, vector> s_mOff = new map<Vehicle, vector>();    // visual - body, vehicle space
	protected static ref map<Vehicle, vector> s_mBase = new map<Vehicle, vector>();   // s_mOff before the last correction
	protected static ref map<Vehicle, int> s_mTrace = new map<Vehicle, int>();        // frames left to trace
	// buckets 0-2: our ground slow (< 2 m/s) / mid (< 5) / fast, 3-5: the same on roads (vanilla terrain)
	protected static ref array<float> s_aSec = {0, 0, 0, 0, 0, 0};
	protected static ref array<int> s_aCorr = {0, 0, 0, 0, 0, 0};
	protected static float s_fPendSec;    // driving with a wheel on ground whose collider is 2+ cm off the newest heights
	protected static float s_fFreshSec;   // driving with a wheel on a collider that changed height < 0.5 s ago
	protected static float s_fDirtySec;   // driving with a wheel on a patch waiting for a rebuild
	protected static float s_fDriveSec;
	protected static float s_fSince;
	protected static int s_iFrames;
	protected static float s_fWorstDt;

	//------------------------------------------------------------------------------------------------
	static void Tick(float dt, bool proxy, MUD_WorldGroundEntity g)
	{
		if (dt <= 0)
			return;
		s_iFrames++;
		s_fWorstDt = Math.Max(s_fWorstDt, dt);
		Vehicle mine;
		IEntity me = SCR_PlayerController.GetLocalControlledEntity();
		if (me)
			mine = Vehicle.Cast(CompartmentAccessComponent.GetVehicleIn(me));
		MUD_StutterRobot.Tick(dt, mine, me);
		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh)
				continue;
			Physics ph = veh.GetPhysics();
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
			if (!ph || !sim || !sim.IsValid())
				continue;
			vector bm[4];
			ph.GetDirectWorldTransform(bm);
			vector body = bm[3];
			vector vis = veh.GetOrigin();
			vector vel = ph.GetVelocity();
			vector off = veh.VectorToLocal(vis - body);

			// wheels and the ground under them on this machine
			int ours;
			int road;
			int contacts;
			int dirtyN;
			float pend;
			float age = 1000;
			float lat;
			float lng;
			int count = sim.WheelCount();
			for (int w = 0; w < count; w++)
			{
				lat = Math.Max(lat, Math.AbsFloat(sim.WheelGetLateralSlip(w)));
				lng = Math.Max(lng, Math.AbsFloat(sim.WheelGetLongitudinalSlip(w)));
				if (!sim.WheelHasContact(w))
					continue;
				contacts++;
				float wPend;
				float wAge;
				bool wDirty;
				if (g && g.ProbeGround(sim.WheelGetContactPosition(w), wPend, wAge, wDirty))
				{
					ours++;
					pend = Math.Max(pend, wPend);
					age = Math.Min(age, wAge);
					if (wDirty)
						dirtyN++;
				}
				else
				{
					road++;
				}
			}
			float speed = vel.Length();
			int bucket = 0;
			if (speed >= 5)
				bucket = 2;
			else if (speed >= 2)
				bucket = 1;
			if (road > ours)
				bucket += 3;
			bool driving = speed > 0.3 || sim.GetThrottle() > 0.05;
			if (driving)
			{
				s_fDriveSec += dt;
				s_aSec[bucket] = s_aSec[bucket] + dt;
				if (pend > 0.02)
					s_fPendSec += dt;
				if (age < 0.5)
					s_fFreshSec += dt;
				if (dirtyN > 0)
					s_fDirtySec += dt;
			}

			vector pBody;
			vector pVel;
			vector pVis;
			vector pOff;
			if (s_mBody.Find(veh, pBody) && s_mVel.Find(veh, pVel) && s_mVis.Find(veh, pVis) && s_mOff.Find(veh, pOff))
			{
				vector step = (pVel + vel) * 0.5 * dt;
				float bodyJump = vector.Distance(body, pBody + step);
				// frame vs physics step jitter moves the body ALONG its path only (v2 run 01:58: 0.1-0.35 m
				// "jumps" every frame at speed on the server, which is never corrected): count the part off
				// the path, and the part along it only beyond one 1/30 s physics step
				vector miss = body - (pBody + step);
				float along = 0;
				if (speed > 0.1)
					along = vector.Dot(miss, vel.Normalized());
				float perp = (miss - vel.Normalized() * along).Length();
				if (speed <= 0.1)
					perp = miss.Length();
				bool corrected = perp > 0.05 || Math.AbsFloat(along) > 0.05 + speed / 30;
				MUD_StutterRobot.Count(veh == mine, driving, road > ours, corrected && bodyJump < 5, dt);
				if (veh == mine && corrected && bodyJump < 5)
					MUD_FrameLog.MarkCorrection(bodyJump);
				float visJump = vector.Distance(vis, pVis + step);
				int left;
				if (s_mTrace.Find(veh, left) && left > 0)
				{
					vector pre;
					s_mBase.Find(veh, pre);
					Print(string.Format("MUD corr trace: %1 frame %2 after, visual off its body by %3 m (pre-correction offset kept out), body jump %4, visual jump %5",
						veh.GetID(), 7 - left, vector.Distance(off, pre), bodyJump, visJump));
					s_mTrace.Set(veh, left - 1);
				}
				if (corrected && bodyJump < 5)
				{
					if (driving)
						s_aCorr[bucket] = s_aCorr[bucket] + 1;
					vector em[4];
					veh.GetTransform(em);
					float fwd = vector.Dot(vel, em[2]);
					string dir = "stop";
					if (fwd > 0.3)
						dir = "fwd";
					else if (fwd < -0.3)
						dir = "rev";
					string head = string.Format("MUD corr: %1 mine %2 proxy %3 | body jump %4 m (off path %5), visual jump %6 m | %7 %8 m/s, dt %9",
						veh.GetID(), veh == mine, proxy, bodyJump, perp, visJump, dir, speed, dt);
					string input = string.Format(" | throttle %1 brake %2 hb %3 gear %4 steer %5 | slip lat %6 long %7",
						sim.GetThrottle(), sim.GetBrake(), sim.IsHandbrakeOn(), sim.GetGear(), sim.GetSteering(), lat, lng);
					string ground = string.Format(" | wheels %1/%2 on: ours %3 road %4 | collider vs newest heights %5 m, collider changed %6 s ago, wheels on dirty patches %7 | at %8",
						contacts, count, ours, road, pend, age, dirtyN, vis);
					Print(head + input + ground);
					s_mBase.Set(veh, pOff);
					s_mTrace.Set(veh, 6);
				}
				else if (visJump > 0.1 && visJump < 5)
				{
					Print(string.Format("MUD corr visual only: %1 mine %2 proxy %3 | visual jump %4 m, body jump %5 m, %6 m/s, dt %7",
						veh.GetID(), veh == mine, proxy, visJump, bodyJump, speed, dt));
				}
			}
			s_mBody.Set(veh, body);
			s_mVel.Set(veh, vel);
			s_mVis.Set(veh, vis);
			s_mOff.Set(veh, off);
		}

		MUD_FrameLog.Tick(dt, mine);    // TEMP raw per-frame CSV, after the loop so this frame's correction is in the row

		s_fSince += dt;
		if (s_fSince >= 10)
		{
			Print(string.Format("MUD corr 10 s (proxy %1): driving %2 s | ours slow %3 s / %4 corr, mid %5 s / %6, fast %7 s / %8",
				proxy, s_fDriveSec, s_aSec[0], s_aCorr[0], s_aSec[1], s_aCorr[1], s_aSec[2], s_aCorr[2]));
			Print(string.Format("MUD corr 10 s (proxy %1): road slow %2 s / %3 corr, mid %4 s / %5, fast %6 s / %7 | worst dt %8 in %9 frames",
				proxy, s_aSec[3], s_aCorr[3], s_aSec[4], s_aCorr[4], s_aSec[5], s_aCorr[5], s_fWorstDt, s_iFrames));
			Print(string.Format("MUD corr 10 s (proxy %1): driving with a wheel on collider 2+ cm off newest heights %2 s, on collider changed < 0.5 s ago %3 s, on a dirty patch %4 s",
				proxy, s_fPendSec, s_fFreshSec, s_fDirtySec));
			s_fSince = 0;
			s_iFrames = 0;
			s_fWorstDt = 0;
			s_fDriveSec = 0;
			s_fPendSec = 0;
			s_fFreshSec = 0;
			s_fDirtySec = 0;
			for (int i = 0; i < 6; i++)
			{
				s_aSec[i] = 0;
				s_aCorr[i] = 0;
			}
		}
	}
}
