# Prompt for the local AI: build Part A (sheet cost switches), 2026-10-19

Copy everything below the line into the local AI.

---

Start building Part A of `docs/superpowers/prompts/2026-10-19-local-sheet-cost-followup-v2.md` (branch
`claude/upbeat-lovelace-q0w022`). Build and compile only - no DS launch, no probes yet. Stop and report when Part A
compiles and passes the WB check; I start the DS launches myself.

## Read first
- `CLAUDE.md` (rules), the v2 prompt (Part A + Rules), research `2026-10-19-mud-sheet-script-cost-research.md`
  sec 15.5 (A1 / A2 spec - UNREVIEWED PLAN CODE, review it while you port) and sec 9 (BenchNatives),
  `results/2026-10-19-sheet-cost-probes.md` (review points 3-5), `mud-gotchas.md` (Enforce limits: 64 locals in scope
  incl. parameters, 80 KB VM stack, int[] not bool[] for member flags, Math.Min/Max return float, int '%' on its own
  line, no Print per op on the DS).
- If your tree lacks these files, do not switch branches with uncommitted work: `git checkout
  origin/claude/upbeat-lovelace-q0w022 -- <paths>`.

## Steps (in this order)
0. Checkpoint: `mud_backups/checkpoint.ps1 save "before sheet cost switches"`.
1. A1 `MUD_MudSync.SLUMP_ONE` (default false) per sec 15.5: stamped cells as (gi, gk) lists filled in Stamp where a
   cell is written; lists cleared in StartJob; mask built in StepStamp after the BOX_MAX test, after SlumpBegin (seed,
   then 4-neighbour dilation from the seed - never in place); even-grid ground only where a mask cell needs it;
   `m_aSok` = mask AND tile AND grounds ok; 1 pass, factor 0.6 (`MUD_MudSheet.SLUMP` untouched - the local sheet
   slump uses it); `da * 0.25` cap and 0.0002 floor kept; SlumpWrite / JOB_QUANT / hash / sheet restore unchanged.
   New helpers in their own methods (Stamp and the slump steps are near the locals limit).
2. A2 `MUD_MudSync.DRAG_STRIP` (default false) per sec 15.5: mark cells that pass the geometric test (before TileOf)
   over the stamp box + 1; dilate by 1; sweep the list in row order, then reversed; today's LimitDrag inner body moved
   into one method used by both paths (missing tiles skipped, missing neighbours THICK / 0 drag). LimitDrag stays.
3. A3 `SERVER_LAYER` as a DS test-kit value (DS only; clients unaffected) if the test kit makes that easy; otherwise
   leave the const and note it.
4. A4 `BenchNatives()` (sec 9) in the TEST_KIT, once ~30 s after start, clients only.
5. A5 `MUD_MudSheet.STORED_RENDERED_GATE` (default false): stored tile rebuilds wait like
   `MUD_WorldGround.RenderWait` (SET_RENDERED + IsRendered; always build within 15 m; never gate live wheel slots);
   'stored builds held (not rendered) N' on the 'MUD sheet:' 10 s line.
6. Switch states on the 'MUD prof 5 s' and 'MUD mud sheet sync' lines.
7. Self-test (TEST_KIT): record one rut op's input, run it through the old and the new A1 / A2 paths on copies of the
   touched tiles, print 'MUD sheet cost self-test: cells changed N, max diff M (A1) | drag cells N, max diff M (A2)'.
   Old vs new are expected to differ; the line proves the new path runs and stays in a sane range.
8. Compile in Workbench. 0 MUD script errors. Fix and recompile until clean.
9. WB function check with `MUD_MudSheet.ENABLED` true ONLY in the working copy (do not commit it true): drive the
   test zone with all switches off, then with A1 + A2 + A5 on; the self-test line prints; the sheet looks as before
   (I judge by eye if you can show me). No ms from WB.
10. Set ENABLED back to false, commit (`git add --ignore-removal .`), push to this branch. No PR.

## Report back
- What you built, file + method per switch, anything in the sec 15.5 plan code you changed and why.
- Compile result, the self-test line(s), anything odd in the WB log.
- The two DS builds I need (launch 1: all off; launch 2: A1 + A2 + A5 on, DS and clients the same build) and the
  test-kit values for each.
- Do not start Part B or C; do not decide anything about un-parking.
