# Prompt for the research AI: mud sheet cost, round 3 (after the local IDA pass, 2026-10-19)

You are doing research only on the Arma Reforger mud mod (repo steffenbk/Terrain-deformation, branch
`claude/upbeat-lovelace-q0w022`). You can read code and docs; you cannot run Workbench, the game, a DS or IDA. Do not
change anything under `Scripts/`. The mud sheet stays PARKED - this is about whether it, or a cheaper look, is worth it.

## Read first (in this order)
1. `CLAUDE.md` - project rules (TESTING RULES, FEATURE FREEZE, PvP only).
2. `docs/superpowers/results/2026-10-19-sheet-cost-probes.md` - NEW: the local IDA answers (D1-D4) and a review of
   your last prompt (`prompts/2026-10-19-local-sheet-cost-followup.md`). Its findings outrank your earlier guesses.
3. `docs/superpowers/research/2026-10-19-mud-sheet-script-cost-research.md` - your research; sec 14 is the new status.
4. `docs/superpowers/research/mud-gotchas.md` - the three new IDA 2026-10-19 lines at the end, and the Enforce limits
   (64 locals in scope, 80 KB VM stack, int[] not bool[], Math.Min returns float, '%' on its own int line, etc.).

## What changed (short)
- SetBone works on a bare entity IF the .xob is skinned and script calls `ent.GetAnimation()` once (that creates the
  skeleton instance; without it SetBone silently does nothing). Bounds stay at the static box; the MeshObject
  `BoundingBox` property is the override for animated objects.
- ProcAnim is evaluated every frame for every registered component (no visibility / change test). SetSignalValue is
  one float write. So "a ProcAnim per rut piece" does not scale to thousands of pieces.
- SpawnEntityPrefab loads nothing when you pass a held Resource; per-spawn component init was not traced.
- Instancing of many entities with one .xob: unresolved.
- IsRendered + SET_RENDERED already works on our own mesh entities (patch RENDER_GATE counts).
- The last prompt had gaps: B1/B2 need two launches (test-kit values are read once per machine at start, and A1/A2
  change synced maths, so DS and clients must match); the robot drives only one truck (B6 needs peer clients or the
  `fake_rut_ops` stand-in); the A1 mask must be built after the op's last stamp (the op box is known only then).

## Tasks
1. **Re-rank the routes** in sec 3 / 6 / 7 with the new facts. Say plainly which routes died, which got better, and
   what the top 3 local tests are now.
2. **Rut pieces without per-piece ProcAnim** (3.4 / 12): design the "pressed" look with no per-frame cost - e.g. swap
   the piece's VObject (`SetObject`) between a few premade .xob variants, a small pool of live ProcAnim pieces near
   the wheels that hand off to static pieces, or a skinned piece set once with SetBone and then left alone (an override
   persists; unchanged bones cost nothing). Count the script calls per piece per pass and the memory per 1000 pieces.
   Use the API dump to confirm each method exists (name the class + method you found).
3. **Skinned tile by bones** (3.5): with GetAnimation + BoundingBox now known, write the design: bone grid size, max
   bones per skeleton you can find evidence for (vanilla .xob / KB / wiki), how normals follow (bone rotation from
   neighbour heights), the per-tile call count vs today's rebuild (~1300-2600 natives per tile, sec 9), and the open
   risks the WB probe C1 must answer. Say whether a skinned tile can replace the live window (3.2) or only the stored
   tiles.
4. **Instancing (D3)**: search the wiki / KB copies / API dump for anything on instanced or batched drawing of
   entities sharing one mesh (render stats, "draw calls", prefab instancing). If nothing, write the exact WB count
   test (what diag menu / stat line to read, 100 vs 1000 spawned pieces) for the local AI.
5. **A1 / A2 port spec**: write a short, exact spec (prose + small Enforce-shaped pseudocode) for SLUMP_ONE and
   DRAG_STRIP that matches `Tools/mud_sheet_cost_exp.py` ("slump: 1 pass f0.6, cells within 1 of stamps" and
   "LimitDrag over stamped cells +1") and the review points in the probes doc (stamped-cell list collected in Stamp,
   mask built after the last stamp, flow only between two mask cells, `da * 0.25` cap kept, cells without a tile skipped,
   sweep order = mask list then reversed). Mark it "unreviewed plan code".
6. **Rewrite the local prompt** as `prompts/2026-10-19-local-sheet-cost-followup-v2.md`: same goals, fixed per the probes
   doc (two launches for B1/B2 or ask; B6 with peer clients or fake_rut_ops and ask; drop the A5 IsRendered print test;
   C1 calls GetAnimation and tests BoundingBox on/off; C2 = one live ProcAnim piece only; add the D3 count test).

## Rules
- Label every claim: [repo] (file + line or log), [IDA] (from the probes doc / gotchas), [doc], [code] (API dump /
  public code), [inf] (yours). Never cite a GUID, file name or line from memory - grep it first.
- Per-item tables go into the doc in full, with a checksum line where you bucket counts.
- Short plain sentences; no emojis.
- Output: update `research/2026-10-19-mud-sheet-script-cost-research.md` (new sec 15 "Round 3"), write the v2 prompt,
  commit to this branch and push. No PR. Do not decide un-parking - list the numbers the user needs to decide.
