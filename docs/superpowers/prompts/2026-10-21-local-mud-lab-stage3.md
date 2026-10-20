# Prompt for the local AI: mud lab stage 3 - edges on turns, ruts under the sheet, voxel cost (2026-10-21)

Copy everything below the line into Claude Desktop.

---

You have direct access to my repo. Branch: `claude/upbeat-lovelace-q0w022` (pull it first). Stage 2 is built and
tested: sheet local (the mud sheet drawn on this client only) looks good, the tile edges are mostly gone (fix set 11),
and craters on the sheet ruts no longer freeze the game badly (fix sets 13-15). Three jobs now, in this order:
1. **Edges on turns.** I can still see tile edges sometimes, mostly while turning (the striped wedges I showed with
   screenshots in stage 2). Much rarer than before.
2. **Ruts under the sheet = physics only.** Where the sheet draws, the voxel ruts no longer need to look like anything.
   Today they still cut to dirt (soil / mud materials, dirt sections in the patch meshes), and that costs performance.
   Under the sheet it can all be one grass layer; the voxel ruts only have to do the physical part (colliders, wheels
   sinking, J2 sync).
3. **Optimise the whole voxel system together with the sheet:** ruts, sheet ruts, tunnels, mortar craters, voxel use in
   general. Find what costs the most now and how the two systems should share the work.

Research and measure first, write it up, then build behind switches. Stop and report after each launch.

## Read first (in this order)
1. `CLAUDE.md`: project rules (testing, checkpoints, git, feature freeze, RUTS STAY VOXEL, RUT WEAR).
2. `docs/superpowers/results/2026-10-20-mud-lab-stage2-build-report.md`: everything stage 2 built, every switch, every
   measured crater table. The end of it is the latest state (B5 / B6 / B7).
3. `docs/superpowers/prompts/2026-10-20-local-mud-lab-stage2.md` (what stage 2 was asked) and
   `docs/superpowers/research/2026-10-20-sheet-local-edges-craters.md` (the edge / crater research).
4. `docs/features/README.md`, then `docs/features/{ruts,mud-sheet,voxel-ground,craters,trenches-tunnels,heightfield-ground}/README.md`.
5. `docs/superpowers/research/mud-gotchas.md`: Enforce limits (64 locals per scope incl. params, string.Format max 9
   values, '%' on its own int line, 256 KB per class), MeshObject rules, J2 rules.
6. `.remember/remember.md`: the "Decisions / gotchas" list (proven facts with dates).

## Where we are (measured 2026-10-20 / 21, logs in My Games/{ArmaReforgerWorkbench,PeerPlugin1}/logs)
- Fix sets (numpad \*, TEST_KIT, sheet local): 11 = edge match (user OK), 12 = + crater B fixes, 13 = + crater
  pre-build (B5), 14 = + column cache (B6), 15 = + rut patch meshes one per frame after a crater (B7). Set 15 is the
  best so far; nothing is on by default yet.
- Crater go-live frame on the sheet ruts, DS client: set 12 69-91 ms, set 14 ~35-50 ms, set 15 ~12-25 ms
  (voxel part 5-7 ms). Still left in that frame or the one before: the ahead dig 12-21 ms (DS) / 15-28 ms (WB), the
  pre-build close 5-29 ms, the rut patch colliders 4-28 ms. User: "better but still room to improve", and "the voxels
  of one crater lag just a little bit" (the crater's voxel bricks show a bit late).
- PROVEN: a crater's go-live 'skin cells to voxel' ms is MUD_WorldGround DoVoxCells rebuilding the rut patches'
  collider + mesh, 7-13 ms per patch, 0 where no rut patch is built.
- EDGE_AUDIT is still true (const); flip it off once part A is accepted.

## Hard rules
- **Server, ops and J2 stay as they are.** Do not change the op format, rut ops, the rut field data, wear rules
  (WEAR_PASSES, cut depths), what the server simulates, or anything a snapshot / JIP joiner replays. Physics under the
  wheels must stay the same on every machine. If an idea needs that, STOP and ask me.
- **New this stage:** client-side LOOK changes in MUD_WorldGround / MUD_FinePatch / MUD_CoarseSkin / MUD_LookRuts /
  MUD_VoxelProbe are allowed (materials, dirt sections, mesh detail, mesh timing, look ruts) - each behind its own
  switch, and only where the sheet draws. Colliders: same shape as today; only their timing may move, and only behind
  a switch. Anything else in those files: STOP and ask.
- **Every change gets its own static switch, default off,** in the numpad \* fix-set cycle (new sets after 15). I pick
  what turns on.
- **Checkpoints:** `mud_backups/checkpoint.ps1 save "<name>"` before each part, and after I confirm something works.
- **Git:** `git add --ignore-removal .`, commit per part, push to this branch. No PR.
- **Testing (CLAUDE.md TESTING RULES):** one combined session per build with a checklist. WB = look and function only.
  ms only from the DS (>= 6 runs per row, an "off" row). Read the logs yourself; never ask me to paste them. End every
  test handover with a numbered checklist.
- Feature freeze: new ideas that are not these three jobs go to "Parked ideas" in
  `docs/superpowers/backlog/2026-10-15-master-list.md`.

## Part 0 - measure (no behaviour change)
1. Read the newest WB and DS client logs. Make one table per job:
   - A: 'edge match' and edge audit numbers per 10 s, split straight driving vs turning (use the wheel / heading data in
     the log, or add a TEST_KIT meter for heading change per 10 s if there is none).
   - B: for the area under the sheet, how much of the rut cost is look: patch mesh builds and ms (dirt sections vs
     grass), look rut builds and ms, voxel brick builds made only for looks. Add TEST_KIT meters where a number is
     missing (print only).
   - C: the per-part MUD prof table (vox, sheet, rebuild, look ruts, ahead, op parts, sh parts) for: driving on ruts,
     a crater on the ruts, a crater off the ruts, a tunnel dig, a trench. Name the top 5 costs.
2. Write every table, whole, into `docs/superpowers/results/2026-10-21-mud-lab-stage3-measure.md`.

## Part A - edges on turns
1. Find why edges still show while turning. Check at least: tiles whose neighbour is built a frame later while the
   camera / wheel slots move sideways, the edge match 3 cm limit (EDGE_CO_STEP), the 4-row ramp, corners (left alone
   today), drape cut at its cap ('drape cut at cap' is high on the builds line), stored vs live tiles.
2. Prove the cause in the log before fixing (edge audit lines at the moment it shows). Then fix behind a switch.

## Part B - ruts under the sheet = physics only
1. Map every look cost the voxel ruts have where the sheet draws: patch mesh dirt sections and materials
   (RUT_MAT_D / FIELD_SOIL_D look use), look ruts (MUD_LookRuts), lazy rut meshes (LAZY_RUT_MESH, Covers, CoverGen),
   rut brick meshes, mesh rebuild pace. Say which already skip under the sheet and which do not.
2. Propose the cheapest "physics only" form under the sheet: e.g. one grass material, no dirt section, coarser or no
   mesh where it can never be seen, no look-rut builds there, colliders unchanged. Watch the sheet edges and holes
   (craters, tunnels): there the ground under the sheet IS seen.
3. Build it behind switches, one per piece, so I can A/B each on the DS.

## Part C - the whole system
1. With the Part 0 tables, write `docs/superpowers/research/2026-10-21-voxel-sheet-optimise.md`: where voxels and the
   sheet do the same work twice, what can move off the frame (spread over frames, timed budgets), what can be cached,
   what is built that nobody sees. Cover ruts, sheet ruts, tunnels, mortar craters (incl. the rest of the crater
   frame: ahead dig, pre-build close, patch colliders, the voxel bricks showing a bit late), trenches / shovel.
2. Rank the ideas by measured ms saved per effort. Stop and let me pick before you build any of Part C.

## Report after each launch
What was built (switch names, fix set numbers), what the log says (tables, not just summaries), what is not done or not
measured, and a numbered checklist for my next test.
