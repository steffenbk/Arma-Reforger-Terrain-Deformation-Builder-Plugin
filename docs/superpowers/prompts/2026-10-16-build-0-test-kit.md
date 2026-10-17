# Prompt: Build 0 - test kit (2026-10-16)

Paste everything below the line into a Claude session running in your local mod folder.

---

You are working in my local mod folder (the Arma Reforger "Terrain deformation" mud mod, a git repo). My local
scripts can be AHEAD of git. Never overwrite, revert or stash my local changes.

Task: build the Build 0 TEST KIT. No ground / dig / rut behaviour may change. Everything new sits behind switches
that are off for release. Work in stages and STOP after each stage so I can compile and test.

## Stage 0 - check the files before any work (no code changes)

### 0a. State of my folder
- Run `git status`, `git branch --show-current`, `git log --oneline -5` and `git fetch origin`.
- Report:
  - the branch;
  - uncommitted changes (list the Scripts/ files separately);
  - whether `origin/claude/stoic-clarke-uiodzl` exists.
- Is my local history already at or past commit da37c47 ("Review fixes stages 5-6")? Run
  `git merge-base --is-ancestor da37c47 HEAD`.

### 0b. Get the plan files without touching my scripts
The plan docs and one tool live on `origin/claude/stoic-clarke-uiodzl`.
- First list which of these paths differ locally: `git diff --stat HEAD origin/claude/stoic-clarke-uiodzl --
  docs Tools CLAUDE.md .gitignore`.
- If NONE of those paths has uncommitted local changes, bring in only them:
  `git checkout origin/claude/stoic-clarke-uiodzl -- docs Tools/compare_op_hashes.py
  Tools/test_compare_op_hashes.py Tools/plan_review_checks.py .gitignore`.
  - Do not check out Scripts/, Prefabs/, worlds/ or anything else.
  - Do not merge or pull.
  - Then show `git status`.
- If any of them HAS local changes: stop and ask me.

### 0c. Read
- CLAUDE.md;
- docs/superpowers/plans/2026-10-16-next-steps.md ("How to run it", "Already built", Build 0 and the session
  checklists);
- docs/superpowers/research/2026-10-16-plan-review-second-opinion.md (D7 switches, D8 the J1 test, the hash design);
- docs/superpowers/research/mud-gotchas.md;
- mud_backups/archive/test-code-2026-10-15/README.md.

### 0d. Check every code anchor this prompt relies on, in MY local scripts
Line numbers below are from git da37c47. Find each anchor by name, then report a table:
anchor | expected | found at (file:line) | same / different / missing.

| # | Anchor | Expected (da37c47) |
|---|---|---|
| 1 | MUD_WorldGround.c EOnFrame: Stats call | `if (m_bDebug)` then `Stats(timeSlice)` ~:921-922 |
| 2 | MUD_WorldGround.c PatchCounts(float fps) | ~:7653; builds the 'MUD patches: ...' line (live / asleep / floor-only / deformed / heights-only / building / lazy / total / mem / fps / moving / settle / floor free) |
| 3 | MUD_WorldGround.c m_aDirty | ~:483 `ref array<int> m_aDirty` (patch keys with a pending mesh / collider); is there a per-patch dirty time? |
| 4 | MUD_WorldGround.c CheckNew trace | runs under m_bDebug (~:3468 on f7715c3) - separate from Stats? |
| 5 | MUD_VoxelProbe.c m_aOps | ~:375 `ref array<ref MUD_VoxelOp> m_aOps` (every op, seq order) |
| 6 | MUD_VoxelProbe.c Receive held-op cap | ~:982 `if (s_mPending.Count() >= 20000) return;` |
| 7 | MUD_VoxelProbe.c Drain / AskJip | Drain ~:1003 ('waiting 10 s for op seq' warning), AskJip ~:1036 (asks once) |
| 8 | MUD_VoxelProbe.c Put | ~:1454 `protected void Put(MUD_VoxelColumn col, int li, int lk, int gi, int gj, int gk, float old, float d)` |
| 9 | MUD_VoxelProbe.c disp hash | m_iDispHash (:502, :3009, :3212), print 'MUD disp hash seq %1 kind %2: ...' ~:3822, DISP_HASH_ALWAYS ~:289, DSW_HASH / DispOn ~:3791 |
| 10 | MUD_VoxelProbe.c where an op's apply finishes | the function that prints the 'MUD disp hash' line - the field hash goes there |
| 11 | MUD_VoxelProbe.c OP_LOG | ~:279 `static const bool OP_LOG = true` |
| 12 | MUD_WorldGround.c FLOOR_SETTLE / FLOOR_FREE | ~:97 / ~:103, both true |
| 13 | Prefabs/Mud/MudWorldGround.et | `m_bDebug 1`, `m_bBandMesher 1` |
| 14 | mud_backups/archive/test-code-2026-10-15 | MUD_JoltProbe.c, MUD_StutterRobot.c, MUD_FrameLog.c (.legacy), MUD_WorldGround_snippets.c.legacy |
| 15 | Tools/compare_op_hashes.py | present after 0b; `python -m pytest Tools/test_compare_op_hashes.py` passes (if pytest is installed) |

Then report:
- whether my local scripts have anything the plan's "Already built" table does not know about;
- anything that would change a stage below.

**STOP after Stage 0 and wait for my OK.**

## Rules for every code stage
- Checkpoint before each stage: `mud_backups/checkpoint.ps1 save "<stage name>"`.
- With every new switch at its default, the game behaves exactly as now.
- Enforce traps:
  - no variable name declared twice in one function;
  - no ternary;
  - never name a variable "set";
  - static arrays <= 65536;
  - class <= 256 KB;
  - int is 32 bit: keep every product under 2^31, as m_iDispHash does.
- If the code disagrees with this prompt, stop and ask. Don't improvise.
- After each stage report:
  - the files and functions changed;
  - what to compile;
  - which log lines to read.
- Update docs/superpowers/plans/2026-10-16-next-steps.md ("Already built" table) when a stage passes.
- Proven facts go to mud-gotchas.md.

## Stage 1 - switches + the 10 s line
- New class `MUD_TestKit` (new file Scripts/Game/Mud/MUD_TestKit.c) with these consts:
  - `TEST_KIT = true`;
  - `FAKE_RUT_OPS = 0`;
  - `JIP_TEST_CAP = 0`.
- Add a row to the Lane 2 table in docs/superpowers/backlog/2026-10-15-master-list.md: "flip before release:
  MUD_TestKit.TEST_KIT false".
- Stats runs only when m_bDebug (anchor 1). Make it run when `m_bDebug || MUD_TestKit.TEST_KIT`. The per-patch
  CheckNew trace (anchor 4) stays on m_bDebug only. The PERF session runs with m_bDebug 0 and still needs the line.
- Add to the end of the 'MUD patches' line (anchor 2): ` | queue N, oldest dirty s, ops N`.
  - queue = m_aDirty.Count().
  - oldest dirty = the age of the oldest still-dirty patch. If no per-patch dirty time exists (anchor 3), add one
    write when a key enters m_aDirty.
  - ops = MUD_VoxelProbe m_aOps.Count(), read through a small static getter. On the server that is every op; on a
    client it is the ops applied.
- Test: compile, one WB drive. The line prints with m_bDebug 1 and with m_bDebug 0.

## Stage 2 - voxel field hash per op
- Where an op's apply finishes (anchor 10), print: `MUD field hash seq %1 kind %2: %3 (%4 samples)`.
  - Hash every sample the op CHANGED (Put, anchor 8, where d != old).
  - Mix the column key (ColKey), the sample index and `Math.Round(d * 100)` (the value in cm) into a mod-M hash, in
    the m_iDispHash style.
  - Count the samples.
  - Only when TEST_KIT, on the server and on clients. A joiner prints one line per replayed op.
- The existing disp hash (anchor 9) is needed too. Ask me: turn DispOn(DSW_HASH) on when TEST_KIT, or set
  DISP_HASH_ALWAYS?
- Offline compare: `python Tools/compare_op_hashes.py <server console.log> <client console.log>`. It matches the
  field and disp hash lines by seq and prints the mismatches.
- Test: SP. A crater, a trench, a tunnel and a drive each print lines. The tool on the same log twice exits 0.

## Stage 3 - fake rut ops + a low JIP cap (the J1 test case)
- When FAKE_RUT_OPS > 0, the server issues that many kind-4 rut ops, once, 30 s after its ground is ready.
  - Use short straight segments in a grid on a flat, open spot near the first player.
  - They go through the normal op path: seq, broadcast, op log.
  - At most 20 per frame.
  - Log the start and the end.
- When JIP_TEST_CAP > 0, the client's held-op cap (anchor 6, `>= 20000`) uses that number instead.
- Test (MP, session 0-MP #2): JIP_TEST_CAP 200, FAKE_RUT_OPS 500, then a late join.
  - Expected today: the joiner logs 'waiting 10 s for op seq'. That is the J1 bug.
  - Do NOT fix J1 here.

## Stage 4 - robot back
- Copy MUD_JoltProbe.c, MUD_StutterRobot.c and MUD_FrameLog.c from mud_backups/archive/test-code-2026-10-15 back
  into Scripts/Game/Mud, dropping the .legacy ending.
- Put back the two Tick lines and ProbeGround() from MUD_WorldGround_snippets.c.legacy, wrapped in
  `if (MUD_TestKit.TEST_KIT)`.
- The archive README lists the call sites. Check that they still fit my current MUD_WorldGround.c.
- Test: the CLAUDE.md MP robot setup, one run. 'MUD corr' lines appear.

## Not in the kit
- Band mesher on / off: compare in Workbench by editing m_bBandMesher in Prefabs/Mud/MudWorldGround.et (one
  machine, so no MP mismatch).
- The J1 fix, and the crater / tunnel / trench changes: those are Build 1.

## When all four stages pass
- Save a checkpoint: `save "build 0 test kit"`.
- Give me a short list of what to run: sessions 0-WB, 0-MP and 0-PERF from the plan, with their checklists.

## Amendments S1-S7 (user OK 2026-10-17, local review against da37c47) - these win over the stages above
- S1 (0b): check out only the files that differ from the plan branch (git diff --name-only), never the whole docs/
  folder: it would bring back the docs the user deleted on purpose in the working tree.
- S2 (Stage 1, oldest dirty): there is no per-patch dirty time. Add `float m_fQueuedAt` to MUD_FinePatch, written at
  the 6 places that set `m_bQueued = true` (MUD_WorldGround.c). m_aDirty also holds asleep and building patches,
  which Rebuild skips on purpose, so the age is taken over awake, not-building patches only (what the queue really
  works on). Line: ` | queue N (waiting N), oldest dirty s, ops N`.
- S3 (Stage 2, field hash): hash at the real write in Put (after its early returns, at `b.m_aD[pi] = d`). Reset next
  to `m_iDispHash = 0` in Submit; print in Submit after SmoothBox / ClearFloor (SmoothBox also writes through Put),
  NOT inside DispOpDone (that only runs with DISPLACE_RUTS). Reduce each term mod M before mixing (ColKey reaches
  ~67 million). With RUT_FIELD a kind-4 rut op changes no voxel samples: print only when samples > 0; ruts are
  compared by the disp hash.
- S4 (Stage 2, disp hash): DispOn(DSW_HASH) is true when MUD_TestKit.TEST_KIT (user pick). A per-op hash says
  equal / different per op, not a count of mismatching samples.
- S5 (Stage 3, fake ruts): the spot is the first player if one exists, else fixed consts FAKE_RUT_X / FAKE_RUT_Z (a
  DS in 0-PERF issues the ops before anyone joins). Y from the ground height. Built as a temporary
  MUD_VoxelRutTrack (m_fStep / m_fCap as a real track) + DoRutOp(kind 4) = the normal Issue path. OP_LOG prints one
  line per op: fine for 500; for the 21000-op PERF join set OP_LOG false.
- S6 (Stage 4): the snippets file's 2nd Tick line is MUD_EngineProbes (not in the copy list = compile error). Put
  back only `MUD_JoltProbe.Tick(...)` (after GroundRescue()) + ProbeGround(). Every field ProbeGround reads still
  exists. StutterRobot's modded Vehicle sits next to MUD_Wheels' modded Vehicle: watch for name clashes at compile.
- S7: MUD_WorldGround.c and MUD_VoxelProbe.c are CRLF now: every edit keeps CRLF. CLAUDE.md now has the TESTING
  RULES (no performance numbers from Workbench).
- S8 (user 2026-10-17, tests on a dedicated server): FAKE_RUT_OPS / X / Z and JIP_TEST_CAP are defaults; while TEST_KIT
  every machine reads $profile:MUD_TestKit.txt (written with defaults when missing) - no rebuild per setting.
  The robot's OnPrepareControls also needs TEST_KIT (else holding Space as a stopped driver starts it in release).
