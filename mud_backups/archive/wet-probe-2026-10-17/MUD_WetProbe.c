//! TEMP probe, Build 3 Stage 9 (2026-10-17): what does vanilla 1.9 wet weather give on our ground for free?
//! Mud spray from the wheels, muddy trucks, puddles in ruts. Nothing here changes the ground.
//! WB host: forces RAIN_STATE (looping) once, 5 s after the player exists (only the server may - ForceWeatherTo).
//! Then every LOG_SEC for the local player: 'MUD wet' = rain / wetness / water accumulation at the player, and in a
//! vehicle 'MUD wet wheels' = per wheel the contact surface + wheel mud / dust, and the body mud / dust.
//! Archive this file when Stage 9 is done (nothing else calls it except the modded class below).
class MUD_WetProbe
{
	static const bool ON = true;    // Stage 9 WB session only; needs MUD_TestKit.TEST_KIT too
	static const bool FORCE_RAIN = false;   // false = leave the weather as it is (dry dust check, 2026-10-17)
	protected static const string RAIN_STATE = "Rainy";     // weather state name (vanilla: Clear, Cloudy, Overcast, Rainy)
	protected static const float RAIN_AFTER = 5;
	protected static const float LOG_SEC = 1;

	protected static bool s_bRained;
	protected static float s_fT;
	protected static float s_fNextLog;
	protected static ref LocalWeatherSituation s_Lws;

	//------------------------------------------------------------------------------------------------
	static void Tick(IEntity me, float dt)
	{
		if (!me)
			return;
		s_fT += dt;
		ChimeraWorld world = ChimeraWorld.CastFrom(me.GetWorld());
		if (!world)
			return;
		TimeAndWeatherManagerEntity twm = world.GetTimeAndWeatherManager();
		if (!twm)
			return;
		if (FORCE_RAIN && !s_bRained && s_fT >= RAIN_AFTER)
		{
			s_bRained = true;
			if (Replication.IsServer())
			{
				twm.ForceWeatherTo(true, RAIN_STATE, 0, 24);
				Print("MUD wet: forced weather '" + RAIN_STATE + "' (looping)");
			}
			else
			{
				Print("MUD wet: not the server - weather left as it is (set it on the server / GM)");
			}
		}
		if (s_fT < s_fNextLog)
			return;
		s_fNextLog = s_fT + LOG_SEC;

		if (!s_Lws)
			s_Lws = new LocalWeatherSituation();
		vector pos = me.GetOrigin();
		bool got = twm.TryGetCompleteLocalWeather(s_Lws, 0, pos);
		Print(string.Format("MUD wet: got %1 | rain %2 wetness %3 water %4 | at %5", got,
			s_Lws.GetRainIntensity().ToString(-1, 2), s_Lws.GetWetness().ToString(-1, 2), s_Lws.GetWaterAccumulation().ToString(-1, 2), pos));

		Vehicle veh = Vehicle.Cast(CompartmentAccessComponent.GetVehicleIn(me));
		if (!veh)
			return;
		VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
		VehicleWheelEffectComponent fx = VehicleWheelEffectComponent.Cast(veh.FindComponent(VehicleWheelEffectComponent));
		if (!sim || !sim.IsValid())
			return;
		string wheels;
		for (int w = 0; w < sim.WheelCount(); w++)
		{
			string surf = "air";
			if (sim.WheelHasContact(w))
			{
				surf = "nomat";
				GameMaterial mat = sim.WheelGetContactMaterial(w);
				if (mat)
				{
					ContactSurface cs = mat.GetVehicleInfo();
					if (cs)
						surf = FilePath.StripPath(cs.GetResourceName());
					else
						surf = "novhc";
				}
			}
			string mud = "-";
			if (fx)
				mud = fx.GetWheelMud(w).ToString(-1, 2) + "/" + fx.GetWheelDust(w).ToString(-1, 2);
			wheels += string.Format(" | w%1 %2 mud/dust %3", w, surf, mud);
		}
		string body = "no VehicleWheelEffectComponent";
		if (fx)
			body = "body mud " + fx.GetBodyMud().ToString(-1, 2) + " dust " + fx.GetBodyDust().ToString(-1, 2);
		Print(string.Format("MUD wet wheels: speed %1 | %2%3", veh.GetPhysics().GetVelocity().Length().ToString(-1, 1), body, wheels));
	}
}

//------------------------------------------------------------------------------------------------
//! TEMP (MUD_WetProbe): ticks the probe for the local player
modded class SCR_CharacterControllerComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPrepareControls(IEntity owner, ActionManager am, float dt, bool player)
	{
		if (player && MUD_WetProbe.ON && MUD_TestKit.TEST_KIT)
			MUD_WetProbe.Tick(owner, dt);
		super.OnPrepareControls(owner, am, dt, player);
	}
}
