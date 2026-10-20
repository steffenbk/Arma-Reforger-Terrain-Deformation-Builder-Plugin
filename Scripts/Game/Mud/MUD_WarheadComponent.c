[ComponentEditorProps(category: "Mud", description: "Tells the MUD world ground a warhead went off (added to vanilla Warhead_Base.et by override).")]
class MUD_WarheadComponentClass : ScriptComponentClass
{
}

//! Every vanilla explosion is a child of Warhead_Base.et (pak 2026-10-10: shells, rockets, grenades,
//! mines, M112 / TNT, vehicle ammo-rack and fuel blasts, fires) and mods that inherit it. The warhead
//! is spawned where the blast happens, so its init is the blast. ExplosionDamageContainer itself can't
//! be modded (engine class); SCR_ExplosionAmmoEffect sits on only 13 ammo.
class MUD_WarheadComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (MUD_TestKit.TEST_KIT) MUD_CraterTrace.Boom(owner.GetOrigin());     // stage 2 B1 crater log
		MUD_WorldGroundEntity ground = MUD_WorldGroundEntity.GetInstance();
		if (ground)
			ground.OnWarheadBlast(owner);
	}
}
