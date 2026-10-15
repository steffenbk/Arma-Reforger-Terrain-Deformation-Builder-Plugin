//! Crater and trench walls stop blasts. The engine does not count our ground as cover: fragments hit
//! soldiers in a crater from a shell in the next one, 5-10 m, up to 542 dmg (probe 2026-10-10
//! 23:02-23:13, Workbench play; FireGeo surface and STATIC flag changed nothing). So fragment hits and
//! traced blast hits that cross our ground are dropped here (MUD_WorldGroundEntity.GroundCovers). The
//! blast's untraced part (NoTrace, within 1-3 m) still hurts, as behind any vanilla wall.
modded class SCR_CharacterDamageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		EDamageType type = damageContext.damageType;
		if (type == EDamageType.FRAGMENTATION && MUD_WorldGroundEntity.GroundCovers(GetOwner(), damageContext.hitPosition, damageContext.hitDirection))
			return true;
		if (type == EDamageType.EXPLOSIVE && MUD_WorldGroundEntity.GroundCovers(GetOwner(), damageContext.hitPosition, damageContext.hitDirection, 3))
			return true;
		return super.HijackDamageHandling(damageContext);
	}
}
