# Prompt for the local AI: build both mud designs, test v3 overlay first, then the pressed lift (2026-10-19)

Replaces prompts/2026-10-19-local-pressed-mud-layer.md. Copy everything below the line into the local AI.

---

Build two mud designs behind separate switches, then run ONE Workbench look session that tries design 1 first and
design 2 second (branch `claude/upbeat-lovelace-q0w022`). Look only, no ms. Stop and report after the session.

- Design 1: `docs/superpowers/plans/2026-10-19-mud-overlay-v3.md` - the mud sheet's look as a client-only overlay on top
  of the untouched voxel ruts (option A: local wheel stamps + replicated rut data, no sync layer).
- Design 2: `docs/superpowers/plans/2026-10-19-pressed-lift-ruts.md` - a soft layer drawn by the patch meshes and
  pressed out by the ruts.

## Read first
`CLAUDE.md`; both plans; `research/2026-10-19-ruts-invisible.md` (grass rides on top of our ground above the
heightmap); `research/2026-10-19-whole-map-sheet-issues.md` sec 2-3 (crater / dig rebuild lessons);
`research/2026-10-16-soft-mud-alternatives.md` sec 2 (what made the sheet look good); `mud-gotchas.md` (Enforce limits:
64 locals incl. parameters, 80 KB VM stack, int[] not bool[], Math.Min/Max return float, '%' on its own int line;
MeshObject: a fresh mesh per change, a distinct emat per section, indices before verts, index < vertex count).

## Rules
- Checkpoint before each part: `mud_backups/checkpoint.ps1 save "before mud overlay v3"`, then
  `... save "before pressed lift"`.
- Design 1 touches NO rut file: new file `Scripts/Game/Mud/MUD_MudOverlay.c`, one guarded tick call in an existing
  place (say where), reads only (list in its plan). Copy what you need from MUD_MudSheet (Stamp, LimitDrag,
  BuildTileMesh pieces); do not call into MUD_MudSheet's state; MUD_MudSheet.ENABLED stays false.
- Design 2 touches only what its plan lists (LiftSetup / LiftAt and their two call sites, a client-only wear mesh-dirty
  mark, CoarseSkin LookLift), all behind `MUD_WorldGround.PRESS_LIFT`; off must be byte-for-byte today's behaviour.
- The two are mutually exclusive: turning one on turns the other off (log it).
- Both switches and the tuning values are static (not const) so a TEST_KIT key cycles: off -> design 1 presets ->
  design 2 presets -> off, rebuilding what is in view each time. Defaults off.
- If anything seems to need a change outside these lists, stop and ask me.
- Git: `git add --ignore-removal .`, commit, push to this branch. No PR.

## Part 1 - build design 1 (overlay v3, option A)
1. Mud field per 3 m tile (d, drag u / v at 0.15 m), filled by live stamps of every vehicle this client sees (the sheet's
   Stamp maths: BOX + SOFT + GRADUAL, ridges + bow wave, drag + LimitDrag); local pass detection.
2. Height rule from the plan: h = max(undug + (d - THICK), dug); skip quads whose four corners sit on the felt floor;
   no quads on untouched ground.
3. Pass 1 (wear, no cut): ridges + a 5 mm dirt film on pressed cells (distinct emat).
4. Re-centre stamps onto the deepest felt floor sample within 0.3 m once the rut floor is cut there.
5. Fallback stamps along rut-field centre lines for ground nobody here watched (far away, late join).
6. Holes: d = 0 where a voxel column carries the cell or dug is > MOVED below undug without a stamp; rebuild those tiles
   (and their taper neighbours) in the frame the hole appears.
7. Tiles only where stamps / ruts / wear exist; wheel tiles every 0.1 s, never held; others every 0.25 s; kept with a
   memory cap. Log every 10 s: 'MUD mud overlay: tiles N (built N, kept N), stamps live N / re-centred N / fallback N,
   builds N, preset P'.

## Part 2 - build design 2 (pressed lift)
The rule in its plan, both mesh paths, the coarse skin, the wear mesh-dirty mark. Log every 10 s: 'MUD pressed lift:
preset P | rutted verts N, pressed out N, half (wear) N, drawn below heightmap N | wear marks N'.

## WB look session (one launch, design 1 first, then design 2)
Urals on grass: one line 1 / 2 / 4 passes, a crossing, a fast pass (bow wave), a crater and a short trench nearby, a
road edge, a building, a few holed props / rocks / trees, a prone soldier (second client or placed character).
For each preset of design 1, then each preset of design 2, from 2 / 20 / 60 m:
- FIRST: grass on raised mud (design 1 ridges / walls / film)?
- the rut: design 1 = groove lines up with the felt rut, tyre sits in it; design 2 = grass-free pressed groove.
- crater / trench clean, no lip; z-fight; pop when tiles appear (design 1); patch / skin seam and road edge (design 2).
- prone soldier and props (design 2 mainly).
Screenshots per preset, the log lines next to them.

## Report back
What you built (files / methods per step, the call site, every line you touched in existing files), compile result,
the log lines and screenshots per preset, anything odd. Do not tune values or enable anything by default - I compare
and pick.
