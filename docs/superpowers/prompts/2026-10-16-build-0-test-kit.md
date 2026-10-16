# Prompt: Build 0 - test kit (2026-10-16)

Paste everything below the line into a local Claude session in the mod folder. Pull first:
`git pull origin claude/stoic-clarke-uiodzl`. That branch = your displaced-ruts-partial-split (da37c47) + the plan docs.

---

Task: build the Build 0 TEST KIT from docs/superpowers/plans/2026-10-16-next-steps.md. No ground / dig / rut
behaviour may change. Everything new sits behind switches that are off for release. Work in stages and stop after
each one so I can compile and test.

Read first:
- CLAUDE.md;
- docs/superpowers/plans/2026-10-16-next-steps.md (the "Already built" table and Build 0);
- docs/superpowers/research/2026-10-16-plan-review-second-opinion.md (D7 switches, D8 J1 test, the hash design);
- docs/superpowers/research/mud-gotchas.md;
- mud_backups/archive/test-code-2026-10-15/README.md.

Checkpoint first: `mud_backups/checkpoint.ps1 save "before build 0 test kit"`.

Rules:
- With every new switch at its default, the game must behave exactly as now.
- Enforce traps: no variable name declared twice in one function, no ternary, never name a variable "set", static
  arrays <= 65536, class <= 256 KB, int is 32 bit (keep every product under 2^31, as m_iDispHash does at
  MUD_VoxelProbe.c:3009).
- If the code disagrees with this prompt, stop and ask. Don't improvise.
- After each stage: files / functions changed, what to compile, which log lines to read.

## Stage 1 - switches + the 10 s line
- New `MUD_TestKit` class (new file Scripts/Game/Mud/MUD_TestKit.c) holding the kit's consts. Add a line to the
  master list Lane 2 table: "flip before release: TEST_KIT false".
  - `TEST_KIT = true`.
  - `FAKE_RUT_OPS = 0`.
  - `JIP_TEST_CAP = 0`.
- Stats runs only when m_bDebug (MUD_WorldGround.c:921-922). Make it run when `m_bDebug || MUD_TestKit.TEST_KIT`.
  Leave the per-patch CheckNew trace on m_bDebug only. The PERF session runs with m_bDebug 0 and still needs the line.
- Add to the end of the 'MUD patches' line (PatchCounts, :7653): ` | queue N, oldest dirty s, ops N`.
  - queue = m_aDirty.Count() (:483).
  - oldest dirty = the age of the oldest still-dirty patch. Find where a patch gets its dirty time; if there is
    none, add one write when a key enters m_aDirty.
  - ops = MUD_VoxelProbe m_aOps.Count() (:375) through a small static getter (server; on a client it is the ops
    applied).
- Test: compile, one WB drive. The line prints with m_bDebug 1 and with m_bDebug 0.

## Stage 2 - voxel field hash per op
- After an op has written its samples (where Submit / apply finishes, near the 'MUD disp hash' print at :3821),
  print: `MUD field hash seq %1 kind %2: %3 (%4 samples)`.
  - Hash every sample the op CHANGED (Put, :1454, where d != old). Mix the column key (ColKey), the sample index
    and the value as `Math.Round(d * 100)` (cm) into a mod-M hash, in the same style as m_iDispHash. Count the
    samples.
  - Only when TEST_KIT, on server and clients. A joiner prints one line per replayed op.
- Offline compare: `python Tools/compare_op_hashes.py <server console.log> <client console.log>`. It reads these
  lines and the existing 'MUD disp hash seq' lines, matches them by seq and prints mismatches. It is already
  written and tested (Tools/test_compare_op_hashes.py).
- Also turn on the existing disp hash for the kit: DispOn(DSW_HASH) true when TEST_KIT, or set DISP_HASH_ALWAYS
  (:289). Ask me which.
- Test: SP. A crater, a trench, a tunnel and a drive each print lines. Run the tool on the same log twice: exit 0.

## Stage 3 - fake rut ops + low JIP cap (the J1 test case)
- FAKE_RUT_OPS > 0: once, 30 s after the server's ground is ready, issue that many kind-4 rut ops on the server.
  Use short straight segments in a grid on a flat, open spot near the first player, through the normal op path
  (seq, broadcast, op log), at most 20 per frame. Log start and end.
- JIP_TEST_CAP > 0: the client's held-op cap (Receive, MUD_VoxelProbe.c:982, `>= 20000`) uses this number instead.
- Test (MP, session 0-MP #2): JIP_TEST_CAP 200, FAKE_RUT_OPS 500, then a late join. Expected today: the joiner
  logs 'waiting 10 s for op seq' (= the J1 bug, proven in the log). Do NOT fix J1 here.

## Stage 4 - robot back
- Copy MUD_JoltProbe.c, MUD_StutterRobot.c and MUD_FrameLog.c back from mud_backups/archive/test-code-2026-10-15
  (drop .legacy). Put back the two Tick lines + ProbeGround() from MUD_WorldGround_snippets.c.legacy, and wrap
  them in `if (MUD_TestKit.TEST_KIT)`.
- Test: CLAUDE.md MP robot setup, one run. 'MUD corr' lines appear.

## Not in the kit (decided 2026-10-16)
- Band mesher on / off: compare in Workbench by editing m_bBandMesher in Prefabs/Mud/MudWorldGround.et. It is one
  machine, so there is no MP mismatch. For PERF, compare with two uploads only if 0-WB #5 says the look is the same.
- The J1 fix itself, the crater / tunnel / trench changes: Build 1.

When all four stages compile and pass: checkpoint `save "build 0 test kit"`, then run sessions 0-WB, 0-MP and
0-PERF from the plan.
