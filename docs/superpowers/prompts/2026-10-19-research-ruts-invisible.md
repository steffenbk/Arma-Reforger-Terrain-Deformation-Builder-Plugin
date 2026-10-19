# Research prompt: ruts cut in the data but nothing shows (DS + client, 2026-10-19 22:53)

You are the research AI for the Arma Reforger mud mod. Find the cause from the code and the logs, and give the
smallest control test. Do not write the build.

The branch now has a snapshot of the working copy's Scripts/Game/Mud (commit with this prompt). It includes
switches that are NOT on master and were compiled together for the first time in this run:
- TEMP look lift test: MUD_MudSheet.LOOK_LIFT_TEST true, LOOK_LIFT_M 0.16 - coarse skin hole cells
  (MUD_CoarseSkin.LookLift, mesh only, put back before the collider) and every patch mesh (MUD_WorldGround.LiftSetup /
  LiftAt, times p.m_aLock) drawn 0.16 m higher. Prompt / plan: research/2026-10-19-sheet-redesign.md sec 6.
- Rebuild 5 (plans/2026-10-18-perf-plan.md last section): COL_MERGE true and the fast mesher on (MUD_WorldGround ~764-771),
  RUT_PACE / SAME_SKIP / PARTIAL_IN false.
- Rut wear (v234): MUD_VoxelProbe.WEAR_PASSES 1, FIELD_SOIL_D 0.06 (pass 1 only wears, passes 2-3 cut 3 cm each).
- MUD_MudSheet.ENABLED false (sheet off), SPARSE false.

## What the user saw (DS + PeerTool client, Urals on grass near the runway)
- Drove a lot over the same lines: NO ruts visible at all, no dent, no dirt.
- The wheels look a little sunk into the ground (screenshot: tyres sit slightly below the grass surface).
  Expected from the look lift (mesh 0.16 m up, collider at the real height) - but the depth looked smaller than 16 cm.

## What the logs say (client PeerPlugin1/logs/logs_2026-10-19_22-53-30, server server/logs/logs_2026-10-19_22-53-29)
- Rut wear: '29 samples only worn, 0 cut' then '1237 only worn, 288 cut' (both sides, 22:54:15 / 22:54:25).
- Client look ruts 22:54:29: cuts 133, samples written 171, dirty marks 78, look meshes 25 (102 ms), worn only 644.
- Client trail probe 22:54:30-31: 'depth ahead 0.044 / 0.061 m (in rut 1), deepest 0.064 m' - the rut floor data is
  4-6 cm deep under the wheel, i.e. at or past FIELD_SOIL_D (dirt should show).
- Client patches: live 7, floor-only 7, deformed 0. Server: colliders 23 / 26 rebuilt per 10 s, meshes 0 (DS).
- 'MUD rebuild 5': fast mesher bench differ 0, partial 0, full passes 94 / 10 s client, col merge on, 0 verts bad.
- No script errors.

Control known before today: rut wear passes were user-ok on the DS earlier on 2026-10-19 (v234), before the look lift
and rebuild 5 were compiled.

## Questions
1. Which code path can drop the visible rut while the floor data is 4-6 cm deep? Check at least:
   - the look lift: LiftAt is added to h in BuildMesh (~6796) and BandMeshIn (~7465) - is it also applied (or NOT
     applied) to the groove-reveal / dirt layer (m_aUnder, groove UVs, second material), the rut field MeshFloor,
     the look-ruts mesh, the hide / show heights (p.Show / SetShow), or anything that compares a drawn height with a
     data height (so a 16 cm offset makes it think "no change" or hides the dirt)?
   - does p.m_aLock fade the lift so that the rut dent is filled (lift larger inside the rut than around it)?
   - rebuild 5 fast mesher / COL_MERGE: can the fast mesher drop the second (dirt) section or the groove reveal,
     although its bench says differ 0 (what does the bench compare)?
   - wear count: is the look cut skipped for 'worn only' samples on the client even after the server cut?
2. Why do the tyres look sunk less than 16 cm (or is that what 16 cm looks like on a Ural tyre)?
3. Give ONE control test order (switch flips, one session) that splits look lift vs rebuild 5 vs wear, and the log
   line to read for each.

Report: cause PROVEN-CODE (file / line) or INFERENCE, the fix, and the control test.
