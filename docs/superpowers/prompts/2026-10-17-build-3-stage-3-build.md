Paste everything below the line into a new chat in the mod folder.
-------------------------------------------------------------------------------------------------------------------
Build 3, Stage 3: BUILD the bulldozed ridges. The plan is written and I OK'd it 2026-10-17. Stages 0-2 are done and
WB-tested (v143 works).
Handoff: .remember/remember.md (loads by itself). Plan: docs/superpowers/plans/2026-10-17-build-3-plan.md, section
"### Stage 3 build plan (2026-10-17 ...)" items 1-10 - build exactly that (mark it OK'd by user 2026-10-17).

Never overwrite, revert or stash my local changes. No commits.

My pick (do not re-litigate): ridge GROWS each pass ("add", capped), RUT_RIDGE_B 0.4, RUT_RIDGE_GAP 0.2,
RUT_RIDGE_W 0.5, RUT_RIDGE_MAX 0.08, switch RUT_RIDGES. Ridges on PLAIN RUT FIELD ground only (no voxel column at the
sample's 4 sample columns) - so no bricks, no Displace change, no MarkBox / Row, J2 placeholder rows untouched.
Numbers: docs/superpowers/results/2026-10-17-scm-lite-proto.md "Re-run 2026-10-17" (MEASURED offline; the proto
is Tools/scm_lite_proto.py ridge() with blend "add", vol_r 0.3, rclr - match its maths in Enforce).

Read first:
- The Stage 3 build plan items 1-10 + "What the code does today" in the build-3 plan.
- docs/superpowers/research/mud-gotchas.md (before writing any Enforce).
- MUD_VoxelProbe.c: CarveFloor, RelaxFloor, FloorBase, ClearFloor, PrepareRut op box (kind 4 margin), TileText /
  ParseTilePart / LoadSnap / SnapSelfTest, DispFloorList, RutTileMake. MUD_WorldGround.c: Heights (FieldFloorAt
  block) and the 4 patch paths that call MUD_VoxelProbe.FieldFloor (mesh, collider, band mesh, band collider).
  Grep for each - line numbers in docs go stale.

Steps:
1. Checkpoint first: mud_backups/checkpoint.ps1 save "before stage 3 ridges".
2. Build items 1-10. Every new ridge reader: grep and list it; TopAt, ColTop, ColumnDepth, FloorSample / FloorMin /
   FloorAtW, Displace / DispFloor and CarveFloor's surf stay ridge-blind. Heights DOES read the ridge (hole check +
   MakeSeed); re-check every Heights / DugUndug caller is server-only, look-only or op-text (J2 safe).
3. Run the adversarial-reviewer skill (op result + net format change) and fix what it finds before calling it done.
4. Stop so I can compile and do the WB check; then read the newest logs/*/console.log after the last
   'Compiling Game scripts took' line yourself (rut floor lines, self-test mismatched, HOLE lines, script errors).
5. After my WB OK: checkpoint save, plan stage status, the "Already built" table in plans/2026-10-16-next-steps.md,
   '## Now' in .remember/remember.md (under 25 lines). Final MP list in the plan gets: floor + ridge snapshot hash on a
   late joiner, robot with ridges (belly on the crown), patch rebuilds per op (PERF).

End with a numbered WB check list: ridges both sides after 1-3 passes (light + heavy truck), none on roads / asphalt
edges, self-test 0 mismatched, hole check 0, get in / out of a truck between ridges, heavy truck drives the same rut
5 times without bellying out (the inner ridges of one axle meet in the middle).

How to work:
- MUD_VoxelProbe.c, MUD_WorldGround.c, MUD_FinePatch.c, next-steps.md and the build plans are CRLF: edit with a
  byte-safe python script (rb / wb) from the scratchpad, then count line endings. MUD_Dig.c, MUD_VoxelNet.c,
  MUD_GroundFollow.c and .remember/remember.md are LF.
- Enforce gotchas: int '%' inside a float expression fails (own int line); 'set' is reserved; string.Format max 9
  values.
- Testing: WB only. All MP / PERF checks go to the plan's "Final MP". Never measure perf in WB. T debug key stays on.
- New ideas go to the master list "Parked ideas" (feature freeze). Open user calls (D4, T3, grass, clods): ask, don't
  build.
