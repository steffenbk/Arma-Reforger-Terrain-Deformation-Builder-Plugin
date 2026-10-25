//! TEST KIT multi-vehicle load test (2026-10-23, user: 'set up a multi-vehicle load test on the dedicated server so we
//! can measure the cost per driving vehicle'). SERVER only, only while MUD_TestKit.TEST_KIT and the server's
//! $profile:MUD_TestKit.txt has load_drivers > 0.
//!  - SPAWN_WAIT s after the ground is ready it spawns load_drivers trucks (SPAWN prefab, replicated) in a grid
//!    SPACING m apart around fake_rut_x / fake_rut_z, one per frame.
//!  - Then it steps through ARMS (0, 1, 2, 4, 8, 16 driving, capped at load_drivers) for load_cycles cycles,
//!    load_phase s per arm; odd cycles count up, even cycles down (drift cancels). The first k trucks drive a circle
//!    with no driver (sim throttle / steering); the rest stand braked (asleep = the parked skip, ~free).
//!  - A truck that drives with no driver but stays under STUCK_KMH for STUCK_S is PUSHED instead (body velocity along
//!    its nose + turn rate); its wheels still roll on the ground, so the rut code sees it as driving. 'pushed' in the log.
//!  - Every 5 s: 'MUD load: ...' (arm, settled, moving, speed, pushed, ops). The 'MUD prof 5 s' lines (MUD_Prof) give the
//!    server MUD script ms; Tools/loadtest.py pairs them with the arm and prints ms per driving vehicle.
//! Runs from MUD_WorldGroundEntity.EOnFrame BEFORE its frame timer: this file's own cost is not in 'MUD script'.
//! Remove: this file, its EOnFrame call (grep MUD_LoadDrivers) and the load_* keys in MUD_TestKit.
class MUD_LoadDrivers
{
	// Ural 4320 (the one vehicle prefab this project already uses, grep 2026-10-23); heavy = the worst case
	static const ResourceName SPAWN = "{3F5D19AC3C7E20DC}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_ammo_FIA.et";
	static const float SPAWN_WAIT = 30;     // s after the ground is ready
	static const float SPACING = 60;        // m between trucks (own circles, no shared ruts)
	static const float SETTLE = 15;         // s after an arm change before its prof lines count
	static const float THROTTLE = 0.6;
	static const float STEER = 0.45;
	static const float STUCK_KMH = 2;
	static const float STUCK_S = 15;
	static const float PUSH_MS = 6;         // m/s when pushed (~22 km/h)
	static const float PUSH_TURN = 0.3;     // rad/s when pushed (6 / 0.3 = 20 m circle)
	static const float LOG_S = 5;

	protected static ref array<Vehicle> s_aVeh = {};
	protected static ref array<float> s_aSlow = {};      // s each truck has been slow while it should drive
	protected static ref array<bool> s_aPush = {};
	protected static ref array<int> s_aArms = {};
	protected static float s_fT;
	protected static float s_fPhaseT;
	protected static float s_fLogT;
	protected static int s_iSpawned;
	protected static int s_iPhase = -1;     // -1 = spawning / waiting
	protected static int s_iDrive;          // trucks driving in this phase
	protected static int s_iOps0;
	protected static bool s_bDone;
	protected static bool s_bGearLogged;

	//------------------------------------------------------------------------------------------------
	static void Tick(float dt)
	{
		if (!MUD_TestKit.TEST_KIT || s_bDone || !Replication.IsServer())
			return;
		int want = MUD_TestKit.LoadDrivers();
		if (want <= 0)
			return;
		s_fT += dt;
		if (s_fT < SPAWN_WAIT)
			return;
		if (s_iSpawned < want)
		{
			SpawnOne(s_iSpawned, want);
			s_iSpawned++;
			if (s_iSpawned == want)
			{
				MakeArms(want);
				string arms = "";
				foreach (int arm : s_aArms)
					arms += string.Format(" %1", arm);
				Print(string.Format("MUD load: %1 trucks spawned at %2 %3, arms%4, %5 s per arm, %6 cycles",
					s_aVeh.Count(), MUD_TestKit.FakeRutX(), MUD_TestKit.FakeRutZ(), arms, MUD_TestKit.LoadPhase(), MUD_TestKit.LoadCycles()), LogLevel.NORMAL);
				s_fPhaseT = -SETTLE;     // let the spawned trucks drop and fall asleep first
			}
			return;
		}
		s_fPhaseT += dt;
		int phases = s_aArms.Count() * MUD_TestKit.LoadCycles();
		float phaseLen = MUD_TestKit.LoadPhase();
		if (s_iPhase < 0)
			phaseLen = 0;
		if (s_fPhaseT >= phaseLen)
		{
			s_iPhase++;
			s_fPhaseT = 0;
			if (s_iPhase >= phases)
			{
				s_bDone = true;
				Drive(0, dt);
				Print(string.Format("MUD load: done, %1 phases - all trucks braked", phases), LogLevel.NORMAL);
				return;
			}
			s_iDrive = ArmOf(s_iPhase);
			Print(string.Format("MUD load: phase %1 of %2, cycle %3, arm %4 driving", s_iPhase + 1, phases, s_iPhase / s_aArms.Count() + 1, s_iDrive), LogLevel.NORMAL);
		}
		if (s_iPhase < 0)
			return;
		Drive(s_iDrive, dt);
		s_fLogT += dt;
		if (s_fLogT >= LOG_S)
		{
			s_fLogT = 0;
			Report();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Arm of phase p: odd cycles up the list, even cycles down
	protected static int ArmOf(int p)
	{
		int n = s_aArms.Count();
		int i = p % n;
		if ((p / n) % 2 == 1)
			i = n - 1 - i;
		return s_aArms[i];
	}

	//------------------------------------------------------------------------------------------------
	protected static void MakeArms(int want)
	{
		s_aArms.Clear();
		s_aArms.Insert(0);
		int a = 1;
		while (a < want)
		{
			s_aArms.Insert(a);
			a *= 2;
		}
		s_aArms.Insert(want);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SpawnOne(int i, int want)
	{
		int cols = Math.Ceil(Math.Sqrt(want));
		int col = i % cols;     // int '%' inside a float expression fails (mud-gotchas)
		int row = i / cols;
		float x = MUD_TestKit.FakeRutX() + (col - (cols - 1) * 0.5) * SPACING;
		float z = MUD_TestKit.FakeRutZ() + (row - (cols - 1) * 0.5) * SPACING;
		float y;
		if (!MUD_WorldGroundEntity.SurfaceY(x, z, y))
			y = GetGame().GetWorld().GetSurfaceY(x, z);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = Vector(x, y + 1, z);
		Resource res = Resource.Load(SPAWN);
		Vehicle veh;
		if (res && res.IsValid())
			veh = Vehicle.Cast(GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params));
		if (!veh)
		{
			Print(string.Format("MUD load: spawn %1 at %2 %3 failed", i, x, z), LogLevel.ERROR);
			return;
		}
		s_aVeh.Insert(veh);
		s_aSlow.Insert(0);
		s_aPush.Insert(false);
	}

	//------------------------------------------------------------------------------------------------
	//! The first n trucks drive (or are pushed), the rest brake
	protected static void Drive(int n, float dt)
	{
		for (int i = 0; i < s_aVeh.Count(); i++)
		{
			Vehicle veh = s_aVeh[i];
			if (!veh)
				continue;
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
			Physics phys = veh.GetPhysics();
			if (!sim || !phys)
				continue;
			if (i >= n)
			{
				sim.SetThrottle(0);
				sim.SetSteering(0);
				sim.SetBreak(1, true);
				s_aSlow[i] = 0;
				continue;
			}
			if (!phys.IsActive())
				phys.SetActive(ActiveState.ACTIVE);
			if (s_aPush[i])
			{
				vector v = veh.GetTransformAxis(2) * PUSH_MS;
				v[1] = phys.GetVelocity()[1];
				phys.SetVelocity(v);
				phys.SetAngularVelocity(Vector(0, PUSH_TURN, 0));
				continue;
			}
			CarControllerComponent car = CarControllerComponent.Cast(veh.FindComponent(CarControllerComponent));
			if (car)
				car.SetPersistentHandBrake(false);
			if (!sim.EngineIsOn())
				sim.EngineStart();
			if (!s_bGearLogged)
			{
				s_bGearLogged = true;
				Print(string.Format("MUD load: truck 0 gear %1 of %2 (forward %3), engine on %4", sim.GetGear(), sim.GearboxGearsCount(), sim.GearboxForwardGearsCount(), sim.EngineIsOn()), LogLevel.NORMAL);
			}
			if (sim.GetGear() == 1)     // neutral (0 = reverse, 1 = neutral: inference, see the gear line)
				sim.SetGear(2);
			sim.SetBreak(0, false);
			sim.SetThrottle(THROTTLE);
			sim.SetSteering(STEER);
			if (sim.GetSpeedKmh() < STUCK_KMH)
				s_aSlow[i] = s_aSlow[i] + dt;
			else
				s_aSlow[i] = 0;
			if (s_aSlow[i] >= STUCK_S)
			{
				s_aPush[i] = true;
				Print(string.Format("MUD load: truck %1 does not drive with no driver (%2 km/h, gear %3, engine %4) - pushed from now on",
					i, sim.GetSpeedKmh(), sim.GetGear(), sim.EngineIsOn()), LogLevel.WARNING);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Report()
	{
		int moving;
		int pushed;
		float speed;
		for (int i = 0; i < s_aVeh.Count() && i < s_iDrive; i++)
		{
			Vehicle veh = s_aVeh[i];
			if (!veh)
				continue;
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
			if (!sim)
				continue;
			float kmh = sim.GetSpeedKmh();
			if (s_aPush[i] && veh.GetPhysics())
				kmh = veh.GetPhysics().GetVelocity().Length() * Physics.MS2KMH;
			if (kmh >= STUCK_KMH)
				moving++;
			if (s_aPush[i])
				pushed++;
			speed += kmh;
		}
		if (s_iDrive > 0)
			speed /= s_iDrive;
		int ops = MUD_VoxelProbe.OpCount();
		int settled = 0;
		if (s_fPhaseT >= SETTLE)
			settled = 1;
		Print(string.Format("MUD load: arm %1, settled %2, phase %3, moving %4, avg %5 km/h, pushed %6, ops %7 (+%8 in %9 s)",
			s_iDrive, settled, s_iPhase + 1, moving, speed.ToString(-1, 1), pushed, ops, ops - s_iOps0, LOG_S), LogLevel.NORMAL);
		s_iOps0 = ops;
	}
}
