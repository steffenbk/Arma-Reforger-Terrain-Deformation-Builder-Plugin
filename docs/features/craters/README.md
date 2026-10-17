# Craters

**Status: SHIPPED.** Short page - add to it when we work on this feature.

## Files (Scripts/Game/Mud)
MUD_WarheadComponent.c, MUD_VoxelProbe.c (Carve), MUD_BlastCover.c

## Docs
- [2026-10-15-master-list.md](../../superpowers/backlog/2026-10-15-master-list.md)

## Open
- Old crater items (rim, size by charge, hit-entity check) need a re-check against the voxel crater code.
  Answered 2026-10-16 ([full review](../../superpowers/research/2026-10-16-ground-options-full-review.md) N4, C1-C13):
  size by charge + vehicle skip carry over; jitter, outline lobes, stack cap and the 42 deg wall rule do NOT (voxel
  Carve gets only the depth). Untested: bullets through voxel rims, craters next to houses, objects over new craters.
