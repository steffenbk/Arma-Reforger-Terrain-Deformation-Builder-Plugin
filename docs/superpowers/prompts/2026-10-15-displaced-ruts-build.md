# Prompt: build displaced ruts (Steps 1-3) + debug tools

Paste into a local Claude session in the mod folder:

---

Build displaced ruts, Steps 1-3 of docs/superpowers/plans/2026-10-15-displaced-ruts-plan.md, plus the debug tools
below. Branch: displaced-ruts-partial-split (pull it first).

Before any code:
1. Read CLAUDE.md, docs/features/ruts/README.md, docs/superpowers/research/mud-gotchas.md and the WHOLE plan,
   including the "Build guide" at the end. The plan is the spec - if the code disagrees with it, stop and tell me,
   don't improvise.
2. Save a checkpoint: mud_backups/checkpoint.ps1 save "before displaced ruts".
3. The plan's line numbers are from master 34d1afe - find each spot by name, not by number.

Rules:
- All changes behind MUD_VoxelProbe.DISPLACE_RUTS. With it false the game must behave exactly as v035
  (CarveRut, RUT_BEHIND, op text, net, JIP untouched).
- Enforce traps from the Build guide: locals are function-scoped (no name declared twice in one function), no
  ternary, never name a variable `set`, static arrays <= 65536, class <= 256 KB.
- Every debug switch below defaults to false / off. Debug draws only on rendering machines (not m_bHeadless).
- Work in this order and stop after each for me to compile in Workbench:
  A. Steps 1-3 with DISPLACE_RUTS false -> I compile + drive: must match v035.
  B0. IDA exploration (below) -> report, no code.
  B. Debug tools (shaped by B0) -> I compile.
  C. I flip DISPLACE_RUTS true and run Step 4 of the plan.

B0 - IDA exploration: what debugging does the ENGINE already give us? (no code in this stage)
Use IDA on the game exe (the Diag exe first - CLAUDE.md: ArmaReforgerWorkbenchSteamDiag.exe / the Diag game exe -
same exe and addresses as the earlier notes in docs/superpowers/research/2026-10-14-ida-voxel-feasibility.md).
Look for, and for each give: name / string, function address, how to turn it on (menu path, console command,
launch parameter, script call), and whether it works in Workbench play, the game client, a dedicated server:
1. Diag menu entries (DiagMenu registrations, their string names): physics / collision debug draw (show colliders,
   trimesh wireframe, contact points, BVH), wheel / suspension (ray casts, contact point + normal, slip), mesh
   debug (wireframe, normals, tangents, bounds), terrain / world debug. We already know Vehicles > "Vehicle
   Networking and Reconciliation" - start from how that one is registered and list its neighbours.
2. Script-side debug API: DiagMenu (RegisterMenu / RegisterBool / RegisterRange / GetBool / GetValue), DbgUI,
   Shape / DebugTextWorldSpace limits, any physics debug query reachable from script (e.g. a ray / contact query
   that returns the hit triangle or its normal). Check each is registered in the GAME exe, not only Workbench.
3. Anything that draws or logs a static trimesh body: triangle count, degenerate tris, internal-edge contacts
   (Bullet "internal edge" / btAdjustInternalEdgeContacts), per-body debug color.
4. Profiler hooks: named script profiler scopes / counters we can add to show Displace / CarveFloor ms in the
   engine profiler instead of our own GetTickCount maths.
Write the findings to docs/superpowers/research/2026-10-15-ida-debug-features.md with labels IDA (read in the exe,
not tested) / PROVEN (tried in play). Then tell me which engine features replace which of the script tools below.

B - Debug tools. Prefer an engine feature from B0 over a script tool when it does the same job.
If DiagMenu works from script in the game: put every DISP_* switch below on a "Mud > Displaced ruts" diag menu so
I can toggle it live without recompiling (debug only - DISPLACE_RUTS itself stays a const: MP machines must agree).
Otherwise keep them as static consts. Script tools (new, all in MUD_VoxelProbe unless noted, pattern as the existing Shape / DebugTextWorldSpace code in
MUD_WorldGround.c ~1648 / ~2453, ShapeFlags.ONCE drawn per frame):
1. DISP_LOG_EVERY_S (10): one summary line `MUD disp: ops N, floor written N cleared N, carved cells N, split bricks
   N, split verts N, fold drops N, overflows N, border pulls N, displace ms total N worst N` every N seconds while
   anything happened. Plus the per-op `MUD rut floor:` line and the batch-line counters from the plan.
   "border pulls" = pulled midpoints / vertices within 0.5 m of a 4 m brick line (plan Step 2 check).
2. DISP_DRAW_FLOOR (false): floor samples within 12 m of the camera as small spheres, colour by depth under the
   field surface (0-5 cm grey, 5-15 cm yellow, > 15 cm red). Shows where the rut data is vs where the mesh is.
3. DISP_DRAW_BRICKS (false): wireframe BBOX per brick built this batch within 30 m: green = split, blue = rutted but
   overflow (coarse pull only), orange = had fold drops. Keep for a few seconds (store boxes + expiry).
4. DISP_HOLE_CHECK (false): every 0.5 s on the local client, a 9 x 9 grid at 0.5 m round the player / vehicle:
   expected y = SurfaceAt (field + floor), trace down from expected + 2 m against the world (as the existing traces,
   TraceFlags.WORLD / ENTS). No hit, or hit more than 0.10 m from expected -> log `MUD disp HOLE at x z: expected
   y, hit y` and draw a red sphere there for 5 s. This catches cracks at brick lines, dropped fold triangles and
   look / feel mismatches. Rate-limit the log (max 5 lines per check).
5. DISP_HASH (false): after each op, a checksum of the floor samples it wrote / cleared (sum of
   round(value * 1000) * (sample index + 1), int wraparound is fine) logged as `MUD disp hash seq N: H` on every
   machine. Server and client logs must match line for line - the MP / JIP determinism check for Step 5.
6. Init check from the plan: DISPLACE_RUTS && RUT_LOOK -> Print error. Route the existing `if (RUT_LOOK)` checks
   through a helper `LookRuts()` returning RUT_LOOK && !DISPLACE_RUTS.

After each stage: list the files and functions you changed, what to compile, what to look for in the log. When
Step 4 passes, update docs/features/ruts/README.md (status, switches, debug tools) and add proven facts to
mud-gotchas.md. Archive throwaway probes per CLAUDE.md, never delete.
