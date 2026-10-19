# Prompt for the local AI: mud sheet cost follow-up, v2 (2026-10-19)

Replaces `prompts/2026-10-19-local-sheet-cost-followup.md`. Fixed per the review in
`results/2026-10-19-sheet-cost-probes.md` and research sec 15. Copy everything below the line into the local AI.

---

You are working on the Arma Reforger mud mod (branch `claude/upbeat-lovelace-q0w022`). A cloud session did the
research (no Workbench, game, DS or IDA there). Your job is the part that needs this machine: build a few switches,
run the DS launches and one Workbench probe session, answer four IDA questions, write the results down. The mud sheet
stays PARKED - this is measurement, not un-parking.

## Read first
1. `CLAUDE.md` - all rules (checkpoints, TESTING RULES, FEATURE FREEZE, git, MP robot setup).
2. `docs/superpowers/research/2026-10-19-mud-sheet-script-cost-research.md` - sections 0, 2, 7, 8, 11, and 15
   (round 3: re-ranked routes, piece designs, skinned tile design, instancing test, A1 / A2 port spec).
3. `docs/superpowers/results/2026-10-19-sheet-cost-probes.md` (your own IDA answers + review) and
   `docs/superpowers/results/2026-10-19-sheet-sync-maths-offline.md` + `Tools/mud_sheet_cost_exp.py`.
4. `docs/superpowers/research/mud-gotchas.md` (Enforce limits; the IDA 2026-10-19 lines at the end).
If your working tree lacks any of these files, do not switch branches with uncommitted work - copy them in with
`git checkout origin/claude/upbeat-lovelace-q0w022 -- <paths>`.

## Rules
- Checkpoint before the first code change (`mud_backups/checkpoint.ps1 save "before sheet cost switches v2"`).
- Every change behind its own `static const` switch, default = today. A build with the switches off must behave as now.
- `MUD_MudSheet.ENABLED` true ONLY in the test builds; ask me before committing it true.
- Perf numbers only from the DS / DS client logs. Workbench = compile, function, look, counts.
- Synced maths (MUD_MudSync) must be identical on the DS and every client: A1 / A2 are compile-time switches, so
  DS and clients run the same build. Re-baseline the hash compare; client A vs B (and DS) must match each other
  (`Tools/compare_op_hashes.py`).
- Label results MEASURED (log file + time), PROVEN (I saw it), IDA, or INFERENCE.
- Ask me before: more launches than listed, anything that touches feel (rut field, colliders), multi-truck setups.
- Git: `git add --ignore-removal .`, commit to this branch, push. No PR.

## Part A - switches (one WB compile + function check)
A1 `MUD_MudSync.SLUMP_ONE` - follow research sec 15.5 exactly (UNREVIEWED PLAN CODE there; review it as you port):
   stamped cells collected in Stamp as (gi, gk) lists; mask built AFTER the op's last stamp (box known then), seed
   dilated by 1 over 4 neighbours from a copy; even-grid ground only where a mask cell needs it; `m_aSok` = mask AND
   tile AND grounds ok; 1 pass, factor 0.6 (MUD_MudSheet.SLUMP untouched); flow only between two Sok cells; the
   `da * 0.25` cap and the 0.0002 floor kept; SlumpWrite / JOB_QUANT / hash / sheet restore unchanged (whole box).
A2 `MUD_MudSync.DRAG_STRIP` - sec 15.5: per stamp, mark cells that pass the geometric test (before TileOf) over the
   stamp box + 1, dilate by 1, sweep the list in row order then reversed, today's LimitDrag body per cell (moved into
   a method; cells without a tile skipped, missing neighbours read THICK / 0 drag). Do not drop LimitDrag.
A3 `SERVER_LAYER` as a DS test-kit value (DS-only; clients are unaffected), if easy; else a second DS build.
A4 `BenchNatives()` from research sec 9 in the TEST_KIT, once ~30 s after start on clients.
A5 Stored sheet tile rebuilds gated like `MUD_WorldGround.RenderWait` (SET_RENDERED + IsRendered, always build
   < 15 m, never gate live wheel slots); count 'stored builds held (not rendered)' on the 'MUD sheet:' 10 s line.
   No WB print test - RenderWait already proves IsRendered on our entities.
Put the switch states on the 'MUD prof 5 s' and 'MUD mud sheet sync' lines.
WB check: compile clean, 0 MUD script errors, sheet looks as before with A1 + A2 on (my eyes), a TEST_KIT self-test
that runs one recorded op through old and new A1 / A2 paths and prints cells changed + max diff.

## Part B - DS launches (DS + PeerPlugin1, robot truck, fresh offroad, >= 6 runs per setting)
Test-kit values are read once at start per machine, and A1 / A2 are build switches - so these are separate launches.
Write the checklist before launch 1.
B1 Launch 1: all switches off, sheet on. 6 runs. Bench line (A4) on the client.
B2 Launch 2: A1 + A2 on (DS and client same build), A5 on. 6 runs. Read the 'MUD mud sheet sync' COST split
   (stamps / slump / ground reads / drag / restore), 'sheet sync' and 'sh stored' in 'MUD prof parts', the held
   count, and the DS 'MUD mud sheet sync (server)' ms. Hash lines: DS vs client must match.
B3 SERVER_LAYER off on the DS: ask me whether to do it as a DS-only restart in launch 2 or as launch 3.
B4 Vehicle count: the robot drives only the local truck (`MUD_StutterRobot.Tick`). Ask me: peer clients with drivers,
   or the `fake_rut_ops` stand-in (per-op cost only, not "per remote truck"). Do not improvise.
Rows per 5 s (time, fps, MUD ms, parts) with checksums, as in the 2026-10-18 docs.

## Part C - ONE Workbench probe session (function, look, counts; no ms)
C1 Skinned tile (research sec 15.3): import a 1.5 x 1.5 m tile skinned to 11 x 11 bones (0.15 m), 4 influences per
   vertex, bilinear weights; if the import refuses 121 bones, a 6 x 6 version. Spawn bare (GenericEntity + MeshObject,
   no anim component), call `ent.GetAnimation()` once, then:
   - SetBone on one bone (+0.3 m) - does it show? Without GetAnimation - does it silently do nothing (IDA says so)?
   - SetBoneMatrix with a tilted frame from neighbour heights - does shading follow?
   - Move bones 0.5 m out of the bind box, look away and back: culled? Then with the MeshObject `BoundingBox` override.
   - 300 tiles, bones set once, then left alone: `System.MemoryAllocationKB()` delta (per tile); does anything look
     like per-frame work (diag counters)? ms comes later on the DS.
C2 ONE live ProcAnim piece (not a pool): 2 bones, .pap TranslateSet + .siga input "pressed", script
   `SignalsManagerComponent.AddOrFindSignal` + `SetSignalValue`. Also: does `GenericComponent.Deactivate(owner)` on the
   ProcAnimComponent stop it from moving (= the per-frame bit, D5)?
C3 Static rut pieces (sec 15.2 design A, B): a 1.5 m piece .xob with 2-3 variants (fresh / pressed / deep) placed from
   op segments on the felt floor (`MUD_WorldGroundEntity.DugUndug`). 100 `SpawnEntityPrefabLocal` with a held
   Resource vs 100 pooled `SetTransform` + `Update`; variant swap = `SetObject` + `Update`; memory per 1000 pieces.
   Look in low sun.
C4 Parallax: a 0.30 m sheet tile with a HeightMap + ParallaxScale dirt emat vs the normal 0.15 m tile. My eyes.
D3 count test (sec 15.4): fixed camera, Statistics > General = all and MeshObject stats = count / mesh, screenshots at
   0 / 100 / 1000 pieces of ONE .xob, 1000 pieces of 10 .xob, 1000 unique script MeshObjects, plus memory deltas.
   Report whether General shows a draw-call / batch line at all.

## Part D - IDA (same Experimental Diag exe)
D5 Does `GenericComponent.Deactivate` on a ProcAnimComponent clear the bit the ProcAnim job tests (`+156 & 4`,
   sub_1402A1080)? Is there any other script path to `OnFrameUpdate`?
D6 An entity with a skeleton instance whose bones did not change: any per-frame work (bone palette upload, skinning
   prep), or only after the dirty mark (`0x20000000` / MeshObject `0x100000`)?
D7 Bone limits: max bones per skeleton / per skin / per draw (import or render asserts, strings).
D8 `SetObject` on an entity that already has a skeleton instance: kept, rebuilt, or dropped (overrides lost)?
Answer like results/2026-10-19-sheet-cost-probes.md (address, finding, confidence).

## Write at the end
- `docs/superpowers/results/2026-10-19-sheet-cost-ds-session.md` (Part B) and append Parts C / D to
  `docs/superpowers/results/2026-10-19-sheet-cost-probes.md`.
- Research doc: mark each sec 15 item confirmed / refuted; proven facts into `mud-gotchas.md`; status into
  `docs/features/mud-sheet/README.md`. Checkpoint after I confirm something works.
- End with the research doc's sec 15.6 list filled in with numbers. Say plainly what they mean against sec 7; do not
  decide un-parking - I do.
