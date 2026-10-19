# Whole-map heightfield ground

**Status: SHIPPED.** Short page - add to it when we work on this feature.

## Files (Scripts/Game/Mud)
MUD_WorldGround.c, MUD_FinePatch.c, MUD_CoarseSkin.c, MUD_WorldGroundNet.c (MUD_GroundEntity.c = old test-map entity)

## Docs
- [2026-10-08-mud-whole-map-holes-design.md](../../superpowers/specs/2026-10-08-mud-whole-map-holes-design.md)
- [2026-10-15-ground-architecture-review.md](../../superpowers/research/2026-10-15-ground-architecture-review.md)

## Pin cut-out (2026-10-17, whole map written, WB whole-map check pending)
User rule: real terrain only under asphalt, concrete, buildings, real roads. Holes 1340627 -> 1420234
(results/2026-10-17-write-map.txt); pins only under Building + power poles (514); trails holed. MUD_PinLift.c lifts
every other RELATIVE_Y root on a hole by the undug y at load. Tools: mud_backups/archive/pin-cutout-2026-10-17.

## Open
- WHOLE-MAP CUT (user 2026-10-18): roads, asphalt, concrete holed too, armored until blasted -
  [plan](../../superpowers/plans/2026-10-18-whole-map-cut-plan.md). Changes the 2026-10-17 terrain rule once built.
- Pin cut-out: whole-map WB check; Final MP item in plans/2026-10-17-build-3-plan.md. Decals on holes may look wrong.
- Release switch: Prefabs/Mud/MudWorldGround.et still has m_bDebug 1 (master list Lane 2).

## Details (moved from CLAUDE.md 2026-10-15, kept word for word)
- Design: everything (ruts, berms, craters, trenches, shovel, fill, rim plow) lives in MUD_FinePatch.m_aH with full physics. The rut layer split (m_aRut + m_fRutPhysCap, spec docs/superpowers/specs/2026-10-11-rut-layer-split-design.md) was REVERTED 2026-10-11 ~17:50 on user request (restored from mud_backups/2026-10-11-0925-pre-rut-split; the split version is saved in mud_backups/2026-10-11-1750-rut-layer-split).

NOTE 2026-10-15: the 'Design: everything lives in MUD_FinePatch.m_aH' line above is history - since v031 / v035 all ops (ruts, craters, trenches, shovel, fill) go to the voxel bricks; the heightfield is the base ground the voxels are seeded from.
