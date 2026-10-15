# Whole-map heightfield ground

**Status: SHIPPED.** Short page - add to it when we work on this feature.

## Files (Scripts/Game/Mud)
MUD_WorldGround.c, MUD_FinePatch.c, MUD_CoarseSkin.c, MUD_WorldGroundNet.c (MUD_GroundEntity.c = old test-map entity)

## Docs
- [2026-10-08-mud-whole-map-holes-design.md](../../superpowers/specs/2026-10-08-mud-whole-map-holes-design.md)
- [2026-10-15-ground-architecture-review.md](../../superpowers/research/2026-10-15-ground-architecture-review.md)

## Open
- Release switch: Prefabs/Mud/MudWorldGround.et still has m_bDebug 1 (master list Lane 2).

## Details (moved from CLAUDE.md 2026-10-15, kept word for word)
- Design: everything (ruts, berms, craters, trenches, shovel, fill, rim plow) lives in MUD_FinePatch.m_aH with full physics. The rut layer split (m_aRut + m_fRutPhysCap, spec docs/superpowers/specs/2026-10-11-rut-layer-split-design.md) was REVERTED 2026-10-11 ~17:50 on user request (restored from mud_backups/2026-10-11-0925-pre-rut-split; the split version is saved in mud_backups/2026-10-11-1750-rut-layer-split).

NOTE 2026-10-15: the 'Design: everything lives in MUD_FinePatch.m_aH' line above is history - since v031 / v035 all ops (ruts, craters, trenches, shovel, fill) go to the voxel bricks; the heightfield is the base ground the voxels are seeded from.
