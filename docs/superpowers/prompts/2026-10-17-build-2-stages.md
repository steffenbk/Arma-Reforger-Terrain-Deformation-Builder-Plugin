Paste everything below the line into a new chat in the mod folder.
-------------------------------------------------------------------------------------------------------------------
Build 2 (J2 rut snapshot JIP + C7 buildings / objects). Stage 1 is done; continue from Stage 2.
Handoff: .remember/remember.md (loads by itself). Plan: docs/superpowers/plans/2026-10-17-build-2-plan.md (stages,
code anchors, WB checks, "Final MP" list).

Never overwrite, revert or stash my local changes. No commits.

Read first:
- docs/superpowers/plans/2026-10-17-build-2-plan.md (whole file).
- docs/superpowers/research/mud-gotchas.md (before writing any Enforce).
- Project CLAUDE.md: TESTING RULES + checkpoints.

State (2026-10-17):
- Build 1 done and tested (results/2026-10-17-build-1-wb.md, -mp.md). Stage 1 (rut floor in whole cm, CarveFloor)
  and the T-at-spawn fix compile and work in WB.
- Crater size: the OLD big preset is back (DepthPerKg 1.5, TntPower 0.6) for the server stress test. Not compiled
  since the switch back - check it compiles with Stage 2.
- Testing: WB only per stage. ALL MP checks go into one live-server session after every build is done - add each
  stage's MP checks to the plan's "Final MP" list, don't ask me for MP runs.
- Line numbers in the plan are from 2026-10-17: grep for the code before you edit.

What I want, one stage at a time (stop after each so I can compile and do the WB check):
1. Stage 2: ColumnDepth(col, li, lk, bool withFloor); FillTrench passes false (density only), TrenchCols true.
   WB check: trench, a rut across its edge, fill it -> flat, no step.
2. Stage 3: snapshot JIP. Before writing it, check whether kind 4 rut ops convert columns on any path (a joiner that
   skips them must end with the same columns). Then: first ask = voxel ops (not 4 / 6) up to seq S, then the rut tiles
   (absolute y in cm, NONE runs RLE'd), then live ops > S; later asks unchanged (J1). TEST_KIT SP self-test: build the
   snapshot, parse it into a fresh tile map, compare every sample -> 'MUD snapshot self-test: ... 0 mismatched', plus
   the snapshot size vs the rut op log size. Run the adversarial-reviewer skill before calling it done (risky net change).
3. Stage 4: C7a building lock. First a WB look at today (crater by a house, tunnel toward a house) to see whether
   the skin already keeps building footprints undug; tell me before building.
4. Stage 5: C7b wake dynamic bodies / hide floating static props. Ask me about mines first.
After each stage: update the plan's stage status, the "Already built" table in plans/2026-10-16-next-steps.md, and
'## Now' in .remember/remember.md (keep Now under 25 lines).

How to work:
- Save a checkpoint before each stage (mud_backups/checkpoint.ps1 save "name").
- MUD_VoxelProbe.c, MUD_WorldGround.c, MUD_FinePatch.c are CRLF: edit with a byte-safe python script (rb / wb) from
  the scratchpad, then count line endings. MUD_Dig.c, MUD_VoxelNet.c are LF.
- After I test: read the newest logs/*/console.log after the last 'Compiling Game scripts took' line yourself.
- New ideas go to the master list "Parked ideas" (feature freeze), not into work. Already parked 2026-10-17: crater
  size / look fine-tune, and a server-owner crater preset option (big / mix / middle / realistic).
