# Prompt for the local AI: mud sheet cost follow-up (2026-10-19)

Copy everything below the line into the local AI (Claude Code on the Workbench machine).

---

You are working on the Arma Reforger mud mod in this repo (branch `claude/upbeat-lovelace-q0w022`, based on
`displaced-ruts-partial-split`). A cloud session did research only: it could read code and docs but could not run
Workbench, the game, a dedicated server or IDA. Your job is the part that needs this machine: build a few test
switches, run ONE dedicated-server session and ONE Workbench probe session, answer a few IDA questions, and write
the results down. The mud sheet stays PARKED - this is measurement, not un-parking.

## Read first (in this order)
1. `CLAUDE.md` - all project rules (checkpoints, TESTING RULES, FEATURE FREEZE, git, MP robot setup).
2. `docs/superpowers/research/2026-10-19-mud-sheet-script-cost-research.md` - the research. Sections 0, 1, 2, 6,
   8-13 matter most. Section 0 lists premises our own logs already disproved (render cost is not the sheet;
   TrackDecal is terrain-only; LimitDrag is needed).
3. `docs/superpowers/results/2026-10-19-sheet-sync-maths-offline.md` + `Tools/mud_sheet_cost_exp.py` - offline
   numbers for the slump / drag changes below.
4. `docs/features/mud-sheet/README.md` (status, lessons) and `docs/superpowers/research/mud-gotchas.md` (Enforce
   limits: 64 locals in scope, 80 KB VM stack, int[] not bool[] for member flags, Math.Min returns float, etc.).

## Rules for this task
- Save a checkpoint before the first code change (`mud_backups/checkpoint.ps1 save "before sheet cost switches"`).
- Every change sits behind its own `static const` switch. Default = today's behaviour. Nothing changes feel or MP
  for a build with the switches off.
- `MUD_MudSheet.ENABLED` goes true ONLY in the test build for the session; ask me before leaving it true or
  committing it true.
- Perf numbers only from the DS client / DS logs, never from Workbench. Workbench = compile, function, look.
- One combined session per environment (TESTING RULES), with a written checklist before launch.
- Deterministic code (MUD_MudSync) must give the same result on every machine: same code, same inputs, same order.
  Changing the formula is allowed; check client A vs client B with `Tools/compare_op_hashes.py` (they must match each
  other; the hash baseline vs the old build will change - that is expected).
- Label every result MEASURED (with log file + time), PROVEN (I saw it), IDA, or INFERENCE.
- Ask me before: anything that touches feel (rut field, colliders), multi-vehicle setups you cannot drive with the
  robot, or more than one session per environment.
- Git: stage with `git add --ignore-removal .`, commit to this branch, push. No PR.

## Part A - switches to build (then one WB compile + function check)
A1 `MUD_MudSync.SLUMP_ONE` (offline: research sec 8). Slump = 1 pass at factor 0.6 instead of SLUMP_PER_OP 3 at
   MUD_MudSheet.SLUMP 0.25, and only over cells within 1 cell of the cells this op stamped (keep an active list or a
   bit mask filled in Stamp), not the whole op box. Ground reads (SlumpGroundRow / GroundY) only for even cells the
   mask needs. Port it so it matches the offline variant "slump: 1 pass f0.6, cells within 1 of stamps" in
   `Tools/mud_sheet_cost_exp.py` (offline: -85-87 % slump work, rms 3-5 mm). Do not change MUD_MudSheet.SLUMP itself
   (the local sheet slump uses it).
A2 `MUD_MudSync.DRAG_STRIP`: LimitDrag only over the cells the stamp actually stamped + 1 ring (same 2 sweeps, same
   neighbour clamp), instead of the stamp's axis-aligned box. Offline: -64 % drag work, heights identical, texture
   stretch p99 0.40 -> 0.48-0.60. Do NOT drop LimitDrag (offline: stretch 2.8 without it).
A3 A test-kit setting for `SERVER_LAYER` (DS keeps the sync layer only for joiner snapshots) so the DS can run with
   it off without a rebuild, if that is easy; otherwise two DS builds is fine.
A4 `BenchNatives()` from research sec 9 into the TEST_KIT, run once ~30 s after start on clients: prints
   'MUD bench natives: ...' (Math.Max vs inline if, Vector() vs copy + component set).
A5 `MUD_MudSheet.STORED_RENDERED_GATE`: set `EntityFlags.SET_RENDERED` on sheet tile entities (client); a stored
   tile rebuild waits while `!IsRendered()` and the tile is > 15 m from the camera; it builds on the first frame
   IsRendered is true. Count 'stored builds held (not rendered)' on the 'MUD sheet:' 10 s line. Never gate live
   wheel slots.
Log: put the switch states on the existing 'MUD prof 5 s' / 'MUD mud sheet sync' lines so every row says which
setting it was.
WB check (function only): compile clean, 0 MUD script errors; print IsRendered for a tile in view vs behind the
camera with and without SET_RENDERED; sheet still looks like before with A1 + A2 on (my eyes).

## Part B - ONE dedicated-server session (DS + PeerPlugin1 client, robot truck, >= 6 runs per setting)
Checklist, one launch where the switches are test-kit settings, otherwise as few builds as possible:
B1 Baseline: switches off, sheet on, one robot truck on fresh offroad, 3 runs.
B2 A1 + A2 on: same route, 3 runs (6 total with B1 as the pair). Read on the client the 'MUD mud sheet sync' COST
   split (stamps / slump / ground reads / drag / restore) and 'MUD prof' sheet sync ms; on the DS the
   'MUD mud sheet sync (server)' ms. Hash lines: client vs DS (and a second client if possible) must match each other.
B3 SERVER_LAYER off on the DS: DS MUD script ms with one truck driving, vs B1.
B4 A5 on: 'sh stored' ms and held counts, first person and third person.
B5 Bench line from A4 (client, one per run is enough).
B6 Vehicle count (only if the robot tool can drive more than one truck, else ask me): 2, then 4 trucks driving away
   from the observing client; sync ms per 5 s on that client and on the DS. Question: does sync cost grow linearly
   with remote trucks?
Write the numbers per row (time, fps, MUD ms, parts) like the 2026-10-18 docs do, with checksums.

## Part C - ONE Workbench probe session (look + function only, no perf numbers)
C1 Skinned plane + SetBone: import a 3 x 3 m plane skinned to 4 x 4 bones (Export Skinning, max 4 bones per vertex).
   Spawn it (a) bare GenericEntity with only a MeshObject, (b) with a ProcAnimComponent. Call
   `SetBone(bone, angles, trans, 1)` on one bone each frame (+0.3 m up). Does it move? Move one bone 2 m out of the
   bind-pose bounds, look away and back: is it culled wrongly? Time 10 000 SetBone calls with GetTickCount (count
   only, WB).
C2 ProcAnim piece: a 1.5 m rut piece with 2 bones, a .pap (TranslateSet on the bones) and a .siga with one input
   "pressed"; script sets it with `SignalsManagerComponent.AddOrFindSignal("pressed")` + `SetSignalValue`. Does the
   piece press down with one script call?
C3 Premade rut pieces from op segments: place the C2 piece (or a static .xob) along a driven path from the rut op
   segments (felt floor via `MUD_WorldGroundEntity.DugUndug`), tilted by two floor samples. Compare 100 spawns vs 100
   pooled `SetTransform` + `Update` (counts / rough ms only). Look at it in low sun.
C4 Parallax: one sheet tile at 0.30 m cells with a dirt emat that has HeightMap + ParallaxScale (vanilla
   Piles_01_Parallax.emat as the pattern) vs the normal 0.15 m tile. My eyes decide.

## Part D - IDA questions (Experimental ArmaReforgerSteamDiag.exe, same build as the 2026-10-18 docs)
D1 IEntity.SetBone / SetBoneMatrix native: what does it need (anim component, skeleton instance)? Does it update the
   entity / VObject bounds, or are skinned bounds fixed at bind pose?
D2 SignalsManagerComponent.SetSignalValue -> ProcAnim: per-frame cost path; is a ProcAnim evaluated every frame or
   only when a signal changes?
D3 Two entities with the same .xob MeshObject: instanced draw or one draw each?
D4 GetGame().SpawnEntityPrefab with a MeshObject component vs SpawnEntity of a script class: extra work per spawn
   (resource lookup, component init)?
Write answers like research/2026-10-18-perf-research-ida-web.md (address, what it does, confidence).

## What to write at the end
- `docs/superpowers/results/2026-10-19-sheet-cost-ds-session.md` (Part B) and
  `docs/superpowers/results/2026-10-19-sheet-cost-probes.md` (Parts C + D), measured numbers with log file + time.
- Update research/2026-10-19-mud-sheet-script-cost-research.md: mark each idea confirmed / refuted.
- Proven engine facts into `mud-gotchas.md`; status into `docs/features/mud-sheet/README.md`.
- Checkpoint after I confirm something works.
- Finish with the research doc's section 7 test: per extra remote truck the sync cost per frame, the sheet total at
  speed, and whether pieces + parallax look good enough - then say plainly whether the sheet is still worth it.
  Do not decide un-parking; I do.
