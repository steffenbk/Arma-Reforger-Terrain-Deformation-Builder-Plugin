Continue Build 1 with U1 deep tunnels + shovel TUNNEL mode. Handoff: .remember/remember.md. Status table:
docs/superpowers/plans/2026-10-16-next-steps.md "Already built" (J1, T1, U2, crater rewrite C1-C4/C6 are built and
compile; Build 0 MP tests deferred by me).

Never overwrite, revert or stash my local changes. No commits.

Read first:
- docs/superpowers/research/2026-10-16-deep-tunnels.md sections 1-5 (section 4 = the package, 4b = shovel tunnels,
  5 = session checks).
- docs/superpowers/plans/2026-10-16-ground-improvement-plan.md "### U1" and next-steps.md "Build 1" (U1 bullets).
- docs/superpowers/research/mud-gotchas.md (Enforce gotchas).

What to build (decided by me 2026-10-16):
1. TUNNEL_FLOOR 5 m (10 m only later, after 5 m passes), the same on every machine (a const or carried in the op).
2. The floor clamp becomes "no NEW air below the floor": d = max(d, min(old, floorY - y)). Otherwise a crater inside
   a deep tunnel refills it (MEASURED: 81 of 218 air samples). Check every place that clamps to FLOOR (Carve,
   CarveSphere, CarveTunnel, CutTrench, FillTrench, ruts).
3. A sea-level guard.
4. AtGround / ColSurf read the new floor.
5. A dig slope cap so players can walk out.
6. Shovel tunnels: a new shovel mode TUNNEL (R cycles it, shovel animation, the server checks the shovel is in hand
   and allows 1 swing per second). T stays as a DEBUG key (TUNNEL_DEBUG_T true, no tool, as today) until release.

How to work:
- Save a checkpoint before each stage (mud_backups/checkpoint.ps1 save "name").
- MUD_VoxelProbe.c, MUD_WorldGround.c and MUD_FinePatch.c are CRLF. Edit them with a byte-safe python script from the
  scratchpad, then count line endings.
- Split the work into stages. Stop after each one so I can compile.
- Measure offline first where Tools/ can do it (the improve_checks.py / plan_review_checks.py models). Write each
  full results table into docs/superpowers/results/.
- After each stage, add a row to the "Already built" table and update the '## Now' section of .remember/remember.md.
- Finish by giving me the U1 test checklist from deep-tunnels.md section 5, merged into session 1-WB / 1-MP.
