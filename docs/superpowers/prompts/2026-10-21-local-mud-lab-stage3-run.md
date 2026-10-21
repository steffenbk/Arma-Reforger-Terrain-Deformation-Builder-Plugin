# Prompt for the local AI: mud lab stage 3 - run the plan (2026-10-21)

Copy everything below the line into Claude Desktop.

---

You have direct access to my repo. Branch: `claude/wizardly-fermi-f20uao` (pull it first; it is
`claude/upbeat-lovelace-q0w022` + the stage 3 plan). Work and push on `claude/upbeat-lovelace-q0w022` as before:
merge `claude/wizardly-fermi-f20uao` into it first (fast-forward). The task itself is
`docs/superpowers/prompts/2026-10-21-local-mud-lab-stage3.md` (three jobs: edges on turns, ruts under the sheet =
physics only, voxel + sheet optimisation, plus Part D MP checks). A cloud session already read the code for it and
wrote a plan. It could NOT read my logs, compile or launch - that is your part.

## Read first (in this order)
1. `CLAUDE.md` (rules: testing, checkpoints, git, feature freeze, RUTS STAY VOXEL, RUT WEAR).
2. `docs/superpowers/prompts/2026-10-21-local-mud-lab-stage3.md` - the task, its hard rules and parts 0 / A / B / C / D.
   Its hard rules all still hold.
3. `docs/superpowers/plans/2026-10-21-mud-lab-stage3-plan.md` - the plan: which meters are missing, the cause candidates
   for the turn edges (A-1 drape cut at cap leaves the striped vertical skirt, A-2 co-builds copy every side, A-3
   corners never matched, A-4 undrawn slots after a window shift), the under-sheet look cost map, switches A10-A12,
   B8-B10, D1, the Part C idea table and the Part D code reading. Line numbers in it are for commit b68d7b0.
4. `docs/superpowers/results/2026-10-20-mud-lab-stage2-build-report.md` (latest state), `docs/superpowers/research/
   mud-gotchas.md` (64 locals incl. params, 80 KB VM stack - BuildTileMesh is near both, string.Format max 9 values,
   '%' / '&' on their own int line, 256 KB per class, MeshObject rules), `.remember/remember.md` if you have it.

## Order of work (stop and report after each launch)
1. **Checkpoint** `mud_backups/checkpoint.ps1 save "before mud lab stage 3"`.
2. **Part 0 - measure, no behaviour change.** Read the newest WB and DS client logs yourself
   (My Games/{ArmaReforgerWorkbench,PeerPlugin1}/logs). Add only the print-only TEST_KIT meters from plan sec 0.2
   (TURN METER, B under-sheet line incl. B10 'would hold', B8 segment count, Part D note counts). Compile in WB, fix
   compile errors. Write every table you can from the existing logs + the "before" numbers in plan sec 0.3 into
   `docs/superpowers/results/2026-10-21-mud-lab-stage3-measure.md`; mark the rows the next launch must fill.
   Commit + push.
3. **Build the switches** from scratch (no code exists yet; each its own static bool, default off, new numpad * sets
   after 15; checkpoint before each part): A10 / A11 / A12 (sets 16-17), D1 (18), B8 (19), B9 (20), B10 (21). Change FixApply's B5-B7 tests to `>=` so
   later sets keep them. Server, ops, J2, rut data, wear, colliders' shape: untouched (plan says where each piece
   lives). Compile clean. Commit + push per part (`git add --ignore-removal .`).
   **Part E (tunnel darkness, plan sec 5)** rides in the same WB session if you have room: pak search for vanilla probe
   exposure settings first, then TUNNEL_DARK + DARK_MAT behind a switch and the probe exposure override
   (EnableHDRModifications / UserEVMode / UserEV_LV). Look only; no MP change. If it does not fit, say so and leave it.
4. **One combined session** (plan sec 5): WB look / function first, then the DS (ms only, >= 6 runs per row, an off
   row), Part D join with 30+ craters and two clients on one crater. Give me ONE numbered checklist for the whole
   session. Read the logs yourself afterwards; never ask me to paste them.
5. **Report**: Part A - which candidate the TURN METER proved (per bucket numbers at the moments it shows) and whether
   sets 16 / 17 removed it; Part B - under-sheet cost before / after per switch; Part D - what a join did. Then write
   `docs/superpowers/research/2026-10-21-voxel-sheet-optimise.md` (Part C, plan sec 3 table re-ranked by MEASURED ms
   saved per effort) and STOP: I pick which Part C ideas get built.
6. If a fix is accepted: checkpoint ("... works"), update `docs/features/mud-sheet/README.md` and the stage 2/3 build
   report. EDGE_AUDIT goes false only when I accept part A. New ideas outside the three jobs go to "Parked ideas" in
   `docs/superpowers/backlog/2026-10-15-master-list.md`.
