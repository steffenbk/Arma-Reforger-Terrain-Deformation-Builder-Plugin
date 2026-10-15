# MP sync, ahead system, JIP

**Status: SHIPPED.** Short page - add to it when we work on this feature.

## Files (Scripts/Game/Mud)
MUD_WorldGroundNet.c, MUD_VoxelNet.c, MUD_CameraSmooth.c

## Docs
- [2026-10-11-driver-stutter-deep-dive.md](../../superpowers/research/2026-10-11-driver-stutter-deep-dive.md)
- [2026-10-10-plan3-task10-jip.md](../../superpowers/results/2026-10-10-plan3-task10-jip.md)

## Open
- Stress test (5-10 vehicles, DS + 2 peers) not done. Test setup: project CLAUDE.md.

## Details (moved from CLAUDE.md 2026-10-15, kept word for word)
- MP "ahead" system (2026-10-11, user-tested): any ground change near a moving vehicle must be finished before it gets there. Server presses ruts a lead ahead (2 m + 0.5 s x speed, max 8) along each wheel's turn arc (PathAt/YawRate), only end axles (m_bEndAxles); clients hide the change on the mesh until Reveal Ahead (m_aShow/m_aHideAt). Body scrape: MUD_Belly underside map cuts ground ahead to underside - margin, one step per pass. Ditch escape: stuck + spinning -> dig, saved up and applied on throttle release. Camera ease: MUD_CameraSmooth.c. World entity categories "Ahead ruts", "Body scrape", "Ditch escape".
