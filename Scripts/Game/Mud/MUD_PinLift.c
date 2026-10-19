//! Pin cut-out (2026-10-17): every machine, once per world, as soon as the world ground is ready (before op replay
//! and anything else that reads entity positions). The engine adds the terrain height at the origin XZ to a root
//! with EntityFlags.RELATIVE_Y when the world loads and SKIPS it when that 2 m cell is a hole, leaving the entity
//! at its raw stored offset (IDA Exp 192624: sub_140A1CC20 -> sub_140A016D0 -> sub_140A30820; 57409 buried,
//! 2026-10-09). Holes now cover trees, bushes, rocks, decals and props too (user rule 2026-10-17: real terrain only
//! under asphalt, concrete, buildings, real roads), so this adds the skipped height back: y = raw y + undug ground
//! (MUD_WorldGroundEntity.UndugY = the skin over holes, same on every machine). Roots that keep their terrain cell
//! (KeepsTerrain: buildings, power poles) are pinned by the hole tool and never lifted.
//! SetOrigin + Update keeps static collision (rock probe 2026-10-17, user: the lifted rock blocked).
class MUD_PinLift
{
	protected static MUD_WorldGroundEntity s_Ground;   // weak: null once that world's ground is deleted
	protected static ref array<IEntity> s_aFound;

	//------------------------------------------------------------------------------------------------
	protected static bool AddRelY(IEntity e)
	{
		if (!e.GetParent() && (e.GetFlags() & EntityFlags.RELATIVE_Y))
			s_aFound.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Roots that keep their real terrain cell (pin list of MUD_KeepScan; poles: cables may be built from pole
	//! heights - not checked, kept on the safe side)
	static bool KeepsTerrain(IEntity e)
	{
		return Building.Cast(e) || SCR_PowerPole.Cast(e) || SCR_JunctionPowerPole.Cast(e);
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: lift the RELATIVE_Y roots on hole cells. Once per world ground (a second call does nothing).
	static void Run(notnull MUD_WorldGroundEntity ground)
	{
		BaseWorld world = GetGame().GetWorld();
		MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
		if (!world || ground == s_Ground || !skin || !skin.IsReady())
			return;
		s_Ground = ground;
		int t0 = System.GetTickCount();
		float x0;
		float z0;
		int ncx;
		int ncz;
		skin.GetGrid(x0, z0, ncx, ncz);
		float cell = skin.GetCell();
		s_aFound = {};
		world.QueryEntitiesByAABB(Vector(x0, -500, z0), Vector(x0 + ncx * cell, 1500, z0 + ncz * cell), AddRelY);
		int lifted;
		int kept;
		int keeperOnHole;
		int miss;
		int odd;
		map<string, int> byClass = new map<string, int>();
		foreach (IEntity e : s_aFound)
		{
			vector o = e.GetOrigin();
			// same cell as the engine's lookup: (int)((x - origin) / cell)
			if (!skin.IsHoleCell(Math.Floor((o[0] - x0) / cell), Math.Floor((o[2] - z0) / cell)))
			{
				kept++;
				continue;
			}
			if (KeepsTerrain(e))
			{
				keeperOnHole++;     // its pin is missing: hole tool run without the new pin list?
				continue;
			}
			float h;
			if (!MUD_WorldGroundEntity.UndugY(o[0], o[2], h))
			{
				miss++;
				continue;
			}
			// already on the ground (nearer to it than to the raw-offset zero): a prefab spawned at runtime or streamed
			// in with the flag set (JIP client) holds an absolute y - lifting it would put it h m up in the air
			if (h > 1 && Math.AbsFloat(o[1] - h) < Math.AbsFloat(o[1]))
			{
				odd++;
				continue;
			}
			e.SetOrigin(o + Vector(0, h, 0));
			e.Update();
			lifted++;
			if (MUD_TestKit.TEST_KIT)
				byClass.Set(e.ClassName(), byClass.Get(e.ClassName()) + 1);
		}
		Print(string.Format("MUD pinlift: %1 RELATIVE_Y roots: lifted %2, on real terrain %3, keepers on a hole %4, undug unknown %5, already on the ground (left) %6 (%7 ms)",
			s_aFound.Count(), lifted, kept, keeperOnHole, miss, odd, System.GetTickCount(t0)), LogLevel.NORMAL);
		if (keeperOnHole > 0 || miss > 0)
			Print("MUD pinlift: buildings / poles on holes or unknown ground - those stay buried", LogLevel.WARNING);
		foreach (string c, int n : byClass)
			Print(string.Format("MUD pinlift: class %1 lifted %2", c, n), LogLevel.NORMAL);
		s_aFound = null;
	}
}
