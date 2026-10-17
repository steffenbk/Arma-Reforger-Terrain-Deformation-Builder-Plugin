Continue Build 1: U1 STAGE 2 (tunnel input side + shovel TUNNEL mode). Handoff: .remember/remember.md. Status table:
docs/superpowers/plans/2026-10-16-next-steps.md "Already built" (U1 stage 1 = deep-tunnel ground ops is built and
COMPILES, v123 = before it).

Never overwrite, revert or stash my local changes. No commits.

Read first:
- docs/superpowers/research/2026-10-16-deep-tunnels.md section 3 (P2, P4), 4b (shovel tunnels + debug T), 5 (checks).
- docs/superpowers/results/2026-10-17-u1-deep-tunnels.md (slope table: why TUNNEL_DOWN_MAX 0.5).
- docs/superpowers/research/mud-gotchas.md (Enforce gotchas).
- Code: MUD_VoxelProbe.TunnelInput + the TUNNEL_* consts, MUD_VoxelNet.c (MUD_RpcAsk_VoxelTunnel),
  MUD_Dig.c (modes, UpdateHelper, MUD_DigAction, HoldsShovel, MIN_GAP_MS, MUD_RpcAsk_Dig).

Decided (by me, 2026-10-16 / 17):
1. Slope cap: max drop per click TUNNEL_DOWN_MAX 0.5 (MEASURED offline: ~27 deg mean, 30 deg max step; 0.7 today =
   39 / 40 deg). Pitch clamped to +-0.5 x TUNNEL_SLOPE both ways. A straight-down look (flat look ~0) digs ahead
   along the body facing, never in place (today: a 90 deg shaft in 8 clicks).
2. One shared aim helper computes mid for both T and the shovel (same as TunnelInput today + the cap).
3. Server check for BOTH tunnel RPCs: mid not lower than feet + TUNNEL_MID - TUNNEL_DOWN_MAX - 0.5 (lag tolerance);
   log refusals.
4. TUNNEL_DEBUG_T = true (const next to the tunnel consts, comment "debug key, user 2026-10-16: OFF before release").
   false: TunnelInput returns at once AND MUD_RpcAsk_VoxelTunnel refuses on the server. While true T works as today
   (no tool, 300 ms). Server logs 'MUD tunnel: debug T by player N' once per player. Add "TUNNEL_DEBUG_T false" to
   the master list release lane.
5. Shovel mode TUNNEL: R cycles OFF -> DIG -> FILL -> TUNNEL -> PLAN. Same looping action + shovel animation; each
   loop sends a NEW RPC MUD_RpcAsk_ShovelTunnel(mid). Server: on foot, HoldsShovel(c, true), mid within 3 m of the
   feet, the down check, one swing per SHOVEL_TUNNEL_MS 1000, then ServerTunnel(mid). Same tunnel op = MP / JIP /
   hashes unchanged. Hint text for the mode. No free-dig ghost in TUNNEL mode (optional capsule ghost is later).
6. Shovel DIG / FILL inside a tunnel stays as it is (it works on the surface above; known, not to fix).

How to work:
- Save a checkpoint before each stage (mud_backups/checkpoint.ps1 save "name").
- MUD_VoxelProbe.c, MUD_WorldGround.c and MUD_FinePatch.c are CRLF: edit with a byte-safe python script from the
  scratchpad, then count line endings. MUD_Dig.c and MUD_VoxelNet.c are LF.
- Split into stages (suggested: 2a aim helper + slope cap + server down check + TUNNEL_DEBUG_T; 2b shovel TUNNEL
  mode + RPC). Stop after each one so I can compile.
- Offline first where Tools/ can measure (Tools/u1_deep_tunnel_checks.py slope already covers the cap).
- Run the adversarial-reviewer skill before calling stage 2 done (net / server checks).
- After each stage, add a row to the "Already built" table and update '## Now' in .remember/remember.md (keep Now
  under 25 lines).
- Finish by giving me the U1 test checklist from deep-tunnels.md section 5, merged into session 1-WB / 1-MP, plus:
  the 'MUD voxel probe: tunnel floor 5 m, sea guard y Y (ocean B)' line must match on server and client; walk out of
  a full-pitch ramp dug to 5 m; looking straight down digs ahead, not a shaft; a crater on the surface over a 5 m
  tunnel does not lower the tunnel ceiling (stage 1 rim fix).
