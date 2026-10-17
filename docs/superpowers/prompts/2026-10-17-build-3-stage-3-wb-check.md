Paste everything below the line into a new chat in the mod folder.
-------------------------------------------------------------------------------------------------------------------
Build 3, Stage 3 (bulldozed ridges): BUILT, I compiled it and did the WB run. Read the log and wrap the stage up.
Handoff: .remember/remember.md (loads by itself). Plan: docs/superpowers/plans/2026-10-17-build-3-plan.md, section
"### Stage 3 build plan (2026-10-17, OK'd by user 2026-10-17)" (status lines under it say what was built + the review fix).

Never overwrite, revert or stash my local changes. No commits.

What was built (v144 = checkpoint before): MUD_VoxelProbe.c - MUD_RutTile.m_aRidge, RUT_RIDGES / RUT_RIDGE_* consts,
GrowRidges (after RelaxFloor in CarveFloor; ridge height from the deepest cut PER SEGMENT), RidgeGone (cut / relax /
ClearFloor clear the ridge), ColRound, FieldRidge, FieldTopAt / FieldTopW / TopSample, snapshot TileText "floor;ridge"
(RunText / ParseRuns), LoadSnap + SnapSelfTest count and compare ridges, PrepareRut kind 4 box 1.0 + CELL, DispFloorList
ridges in cyan. MUD_WorldGround.c - Heights (floor blend unchanged, then only RAISES by the ridge blend where the
heightfield delta < 0.02 m) and the 4 patch paths (mesh, collider, band mesh, band collider) draw max(.., ridge),
edge-locked.

Steps:
1. Read the newest logs/*/console.log yourself (My Games/ArmaReforgerWorkbench/logs), only after the last
   'Compiling Game scripts took' line; check 'Game destroyed' boundaries. Report in a short table:
   - script compile errors / VM exceptions (any = stop, fix, I recompile)
   - 'MUD rut floor: N samples, N relaxed, N ridged, ...' lines: how many ops, how many with ridged > 0, totals
   - 'MUD snapshot self-test' lines: mismatched (must be 0), ridge count > 0, missing / extra / bad parts
   - 'MUD disp HOLE' lines (must be 0)
   - anything new or odd since the v143 run
2. Ask me for my look result (ridges both sides after 1-3 passes light + heavy truck, none on roads / asphalt edges,
   get in / out between ridges, heavy truck same rut 5 times without bellying out). Don't guess it.
3. After my OK: checkpoint save (mud_backups/checkpoint.ps1 save "stage 3 ridges works"), plan Stage 3 status, the
   "Already built" table in plans/2026-10-16-next-steps.md, '## Now' in .remember/remember.md (under 25 lines). Final MP
   list in the build-3 plan gets: floor + ridge snapshot hash on a late joiner, robot with ridges (belly on the crown),
   patch rebuilds per op (PERF; more tiles / patches on the rut-field path now).
   If my look says change a number (B 0.4, GAP 0.2, W 0.5, MAX 0.08): change only that const, checkpoint first, and
   stop for compile again.
4. Then say what is next in Build 3 (plan stage list) and ask which stage I want; open user calls D4, T3, grass,
   clods - ask, don't build.

How to work:
- MUD_VoxelProbe.c, MUD_WorldGround.c, MUD_FinePatch.c, next-steps.md and the build plans are CRLF: edit with a byte-safe
  python script (rb / wb) from the scratchpad, then count line endings. MUD_Dig.c, MUD_VoxelNet.c, MUD_GroundFollow.c
  and .remember/remember.md are LF.
- Enforce gotchas: docs/superpowers/research/mud-gotchas.md (int '%' inside a float expression fails; 'set' reserved;
  string.Format max 9 values; no redeclared names in one function).
- Testing: WB only, never measure perf in WB. All MP / PERF checks go to the plan's "Final MP". T debug key stays on.
- New ideas go to the master list "Parked ideas" (feature freeze).
