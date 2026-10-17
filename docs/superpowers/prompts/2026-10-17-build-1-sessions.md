Build 1 is fully built. Next: check it compiles, then run the combined test sessions and read the results.
Handoff: .remember/remember.md. Status table: docs/superpowers/plans/2026-10-16-next-steps.md "Already built" (every
Build 0 / Build 1 row, with what each still needs tested).

Never overwrite, revert or stash my local changes. No commits.

Read first:
- docs/superpowers/plans/2026-10-16-next-steps.md: "Testing rules", Session 0-WB / 0-MP / 0-PERF tables, Session
  1-WB / 1-MP / 1-PERF lists, "Already built" table (its "Still open" column).
- docs/superpowers/research/2026-10-16-deep-tunnels.md section 5 (U1 checks, now at 10 m).
- docs/superpowers/research/mud-gotchas.md (before any fix).
- Project CLAUDE.md: MP robot test setup + TESTING RULES.

State (2026-10-17):
- U1 stage 2c (TUNNEL_FLOOR 10, slope choice on X in shovel TUNNEL mode) is BUILT, NOT compiled. v126 = before it.
  Step 1: I compile; if it fails, read the newest logs/*/console.log after the last 'Compiling Game scripts took' line
  and fix it.
- Stages 2a / 2b (shovel TUNNEL mode, debug T, server checks) compile and work (user 2026-10-17).
- Deferred by me from Build 0: m_bDebug 0 check, J1 late-join run, MP robot run. Fold them into the sessions below.

What I want:
1. ONE checklist per environment (testing rule 1): a combined WB session (0-WB + 1-WB + U1 at 10 m), a combined
   MP session (0-MP + 1-MP + the deferred items), and a PERF session (0-PERF + 1-PERF, dedicated server / uploaded
   mod only - never measure performance in Workbench, rule 2). Numbered, what to do + what counts as a pass, short.
   Put them in a new docs/superpowers/plans/2026-10-17-build-1-sessions.md; hand them out one at a time.
2. U1 items to include (plus deep-tunnels.md section 5):
   - each of the 5 slopes (X): walk back out of 0.7 / 0.85 / 1.0 with no jump, note which work;
   - dig to 10 m; the 'MUD voxel probe: tunnel floor 10 m, sea guard y Y (ocean B)' line equal on server and client;
   - looking straight down digs ahead, not a shaft;
   - a crater on the surface over a deep tunnel does not lower its ceiling; RPG inside makes a chamber, no refill;
   - shovel TUNNEL mode in MP from a client (no 'server sees no entrenching tool' line: HoldsShovel(c, true) on the
     server is UNVERIFIED); no 'MUD tunnel: ... refused' lines in normal digging (T while walking / backing up too);
   - X does not clash with anything while the shovel is in TUNNEL mode;
   - once: TUNNEL_DEBUG_T false on the server -> T refused and logged.
3. After each session I tell you it is done: read the logs yourself (newest logs/*/console.log, PeerPlugin1, server;
   framelogs */profile/MUD_framelog_*.csv), run `python Tools/compare_op_hashes.py server.log client.log` for the
   hash items, and write a results file docs/superpowers/results/2026-10-17-build-1-<session>.md with the WHOLE
   per-item table (measure-before-claiming rules). Robot: >= 6 runs per setting.
4. Then list the decisions the results open (crater size = decision 2; V1 if rounds pass voxel ground; R2; T3;
   MAX_COLUMNS policy; 10 m slope defaults) with a recommendation each, and update the "Already built" table
   "Tested" column and '## Now' in .remember/remember.md (keep Now under 25 lines).

How to work:
- Save a checkpoint before any fix (mud_backups/checkpoint.ps1 save "name").
- MUD_VoxelProbe.c, MUD_WorldGround.c, MUD_FinePatch.c are CRLF: edit with a byte-safe python script from the
  scratchpad, then count line endings. MUD_Dig.c, MUD_VoxelNet.c are LF.
- New ideas go to the master list "Parked ideas" (feature freeze), not into work.
