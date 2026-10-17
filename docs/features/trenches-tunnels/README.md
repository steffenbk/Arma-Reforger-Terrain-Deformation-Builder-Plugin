# Trenches, shovel, tunnels

**Status: SHIPPED.** Short page - add to it when we work on this feature.

## Files (Scripts/Game/Mud)
MUD_TrenchSite.c, MUD_TrenchPlanner.c, MUD_TrenchLayout.c, MUD_TrenchGhost.c, MUD_Dig.c, MUD_VoxelProbe.c (CutTrench, FillTrench, CarveTunnel)

## Docs
- [2026-10-09-mud-trench-plan-mode-design.md](../../superpowers/specs/2026-10-09-mud-trench-plan-mode-design.md)
- [2026-10-09-mud-trench-plan-mode.md](../../superpowers/plans/2026-10-09-mud-trench-plan-mode.md)

## Open
- Tunnel darkness: PARKED 2026-10-17 (user: 'put a pin in it'). MUD_VoxelProbe TUNNEL_DARK + DARK_MAT = false =
  tunnels as before darkening (no probes, DirtCover walls). Code + prefabs kept. What we learned (mud-gotchas.md):
  probes per grid cell (4 m / 2.5 m, centred, levels to 4 m) fixed day-bright pockets; view-dependent lit spots inside
  full probe cover are AMBIENT on our wall material - a DiffuseIBL / SpecularIBL 0 wall material kills them (v159) but
  all-black looked bad. Untested idea at park time: DirtMat() = MUD_TunnelDark1/2/.emat (IBL 0.5 / 0.25 / 0.1) by
  roof over the brick (1 / 2.5 / 4 m). To resume: set both flags true, WB look check.
- No per-player trench-site cap (user 2026-10-15: dropped); only MAX_SITES 20 server-wide. Keys: Ctrl+F trench, F shovel, T tunnel (debug, TUNNEL_DEBUG_T).
- U1 deep tunnels (2026-10-17, built, shovel mode works, not session-tested): tunnels down to TUNNEL_FLOOR 10 m. Shovel
  R cycles OFF -> DIG -> FILL -> TUNNEL -> PLAN; TUNNEL = one capsule per swing where you look
  (MUD_RpcAsk_ShovelTunnel, MUD_VoxelNet.c). X in TUNNEL mode picks the slope: 19 / 27 (start) / 39 / 47 / 54 deg;
  the two steepest may not walk back out (character ~50 deg). T = debug key, off before release. Shovel DIG / FILL inside a tunnel
  works on the surface above (user 2026-10-17: known, stays). Detail: superpowers/research/2026-10-16-deep-tunnels.md.
