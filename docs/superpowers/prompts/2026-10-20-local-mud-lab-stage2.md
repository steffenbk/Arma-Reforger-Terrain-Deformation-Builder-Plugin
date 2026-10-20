# Prompt for the local AI: mud lab stage 2 - edge wall, crater delay, band cost (2026-10-20)

Copy everything below the line into Claude Desktop.

---

You have direct access to my repo. Branch: `claude/upbeat-lovelace-q0w022` (pull it first). Stage 1 (sheet local,
the deposit band) is built and had its first DS run. I liked the performance, but three things need fixing:
1. a striped vertical wall along straight tile borders beside the rut;
2. a short delay before a crater on the mud ruts shows in the mud;
3. the cost: 2.3-5.3 ms MUD script per frame with all levers off.
The research is done. Build the fixes behind switches, then run one Workbench session and one DS session. Stop and
report after each launch.

## Read first (in this order)
1. `CLAUDE.md`: project rules (testing, checkpoints, git, feature freeze).
2. `docs/superpowers/research/2026-10-20-sheet-local-edges-craters.md`: THE research for this stage. Every step below
   refers to it.
3. `docs/superpowers/results/2026-10-20-mud-lab-stage1-ds.md` and `docs/superpowers/results/2026-10-20-mud-lab-stage1-build.md`.
4. `docs/superpowers/research/2026-10-20-mud-sheet-deep-dive.md` sec 2-3 (levers, band design).
5. `docs/superpowers/research/mud-gotchas.md`:
   - Enforce limits: 64 locals in scope including parameters, 80 KB VM stack, 256 KB per class
     (`MUD_MudSheetField` is at ~236 KB, so new big arrays go in their own holder class), int[] not bool[] for
     members, Math.Min/Max return float, '%' only on its own int line.
   - MeshObject rules: a fresh mesh per change, a distinct emat per section, indices before vertices, every index
     below the vertex count.

## Hard rules (unchanged from stage 1)
- **Do not touch the voxel ruts:** rut ops, the op format, the rut field, wear, look ruts, patch meshes and colliders.
  - No edits in MUD_VoxelProbe / MUD_LookRuts / MUD_WorldGround / MUD_FinePatch / MUD_CoarseSkin, except a timing
    stamp for the crater log. That stamp must be one guarded call per place, TEST_KIT only, and must not change
    behaviour.
  - If anything else seems to need a change there, STOP and ask me.
- **Client only.** Nothing new runs on a DS, except the one server crater log line (TEST_KIT, print only). Nothing
  writes J2, ops or snapshots.
- **Every fix gets its own static switch, default off,** reachable from the numpad 0 lever cycle (or a second cycle
  you name). Do not tune or turn anything on by default; I pick.
- **Checkpoints:** compile + check EDGE AUDIT first (v254 = before it). Then `mud_backups/checkpoint.ps1 save "before
  stage2 A"`, then "... B", "... C", "... D" before each part.
- **Git:** `git add --ignore-removal .`, commit per part, push to this branch. No PR.
- **Tests follow CLAUDE.md TESTING RULES.** WB = look and function only. ms only from the DS, with at least 6 runs per
  row and an "off" row.

## Part 0 - zero code
1. From the 2026-10-20 21:14 client log (PeerPlugin1/logs/logs_2026-10-20_21-14-45/console.log), copy every
   'MUD sheet builds:' 10 s line (builds by reason + would skip) into the stage 1 results doc as a table. Say what
   share of builds the would-skip column (max change < 3 mm) is.
2. Compile the EDGE AUDIT code that is already in `MUD_MudSheet.c` (EDGE_AUDIT, EdgeRecord / EdgeDrawn / EdgeAudit).
   Fix compile errors only.

## Part A - the edge wall (research sec 1.3)
1. **Profiling fix first:** `BandTile` time must not also land in the 'upload' bucket (`Upload` calls it at ~7259 and
   `BandStored` at ~7303, inside the SH_UPLOAD timer ~1047-1050). Subtract it, or time band outside, so 'upload' =
   mesh builds only. Do the same for FreezeLocal (follow) and BuildZoneTile (zones). Say how you did it.
2. **EDGE_COBUILD_PICK** (the cheap step). In `UploadPick`, a dirty slot flagged WHY_BAND or WHY_STAMP that is a
   4-neighbour of a picked wheel slot is picked like a wheel slot: SLOT_MIN_S does not hold it, and it does not use up
   UPLOAD_SLOTS. It must still be SlotTraced.
3. **EDGE_COBUILD** (the full co-build), on top of 2.
   - After `BuildTile` of tile T in sheet local, compare each side's border heights (EdgeRecord) with the neighbour's
     record.
   - If any border vert differs by more than EDGE_STEP 0.03 m, or the neighbour has no mesh there:
     - live neighbour: build it in the same Upload (ignore UPLOAD_SLOTS / SLOT_MIN_S, still needs SlotTraced);
     - stored neighbour: build it now, at most CO_MAX 4 per Upload;
     - stored T next to a live neighbour: mark the live one "must build next upload".
   - No ping-pong: a tile co-built in this Upload does not trigger co-builds itself.
   - EdgeRecord must stay on while EDGE_COBUILD is on, even if EDGE_AUDIT is off.
4. **SKIRT_APRON** (any leftover lag reads as a mud bank, not a wall).
   - In sheet local, the bottom skirt verts move outward from the tile side by about the drop (~45°) and down to
     G - BAND_UNDER - 0.02 m.
   - Their UVs come from their own world xz in the same UV frame as the top (TOP_UV64), not copied from the top vert,
     so there are no stripes.
   - This applies to both the full and the low build skirt.
5. **BAND_BORN_FRONT:** a tile outside the window that BandMarkWorld births (or re-bands) goes to the FRONT of the
   radius build list, instead of waiting for the next 0.25 s QueueNear.
6. Logs: the 'MUD sheet builds:' line gets `cobuild pick N, cobuild live N, cobuild stored N (capped N)`. The edge
   audit part stays as built.

## Part B - the crater delay (research sec 2)
1. **The log first.**
   - New file `Scripts/Game/Mud/MUD_CraterTrace.c` (static class; MUD_MudSheet is near its class limit), TEST_KIT
     only.
   - It keeps seq -> record with: t_boom (the client's own warhead init, matched by distance < 2 m and < 3 s), t_recv,
     t_submit, t_golive (batch frames, worst frame, and the go-live frame split swap / vox / sheet), BlastNow / DigNow
     tiles per class, BuildNowSlots built / skipped (untraced / unborn / no entity), and every tracked tile's build
     time and reason (BW_*) from FlushOneBuild / Upload / BandStored.
   - It prints the per-crater line from research sec 2.3 when every tracked tile is built, or after 3 s.
   - Server: one print line `MUD crater srv: seq S at P kg K | hit->issue a ms`.
   - Add the 10 s part `| craters N: recv->golive avg/max, golive frame max, now-slots skipped, taper late, eases hit`
     to the sheet local line.
   - The stamps in rut-side files are single guarded calls, TEST_KIT only, with no behaviour change. List each one.
2. **CRATER_TRACE_NOW** (O1a): in `BuildNowSlots` (~1883), a now-slot that is born with an entity but not SlotTraced
   gets its missing cells traced / filled in place (Trace / HoleGround, the same as Grounds would) and is built in the
   go-live frame.
3. **CRATER_TAPER_NOW** (O1b): right after BuildNowSlots, build every BW_HOLE entry at the front of `m_aOneBuild` in the
   same frame, capped at 12 tiles / 15 ms. Log the overflow.
4. **CRATER_EASE_DROP** (O1c): DigNow drops any running ease on every vertex it writes (`EaseDrop`), so EaseTick cannot
   write the pre-crater floor back. Do the same where FreezeLocal stores `m_fTo`.
5. Do NOT build the "hole at op arrival" (O2) or the voxel-path shortening (O4) yet. Those wait for the log numbers.

## Part C - the band cost (research sec 3)
1. **BAND_INCR** (fix b). Keep a per-cell distance field (in chamfer units) for the window, and for tile data in a new
   holder class. Do NOT add it to MUD_MudSheetField (class limit). Carry it through shift / Pack / Unpack / Freeze /
   compact far data, like m_aBand.
   - Stamps record newly touched cells (a list, or a box per upload).
   - Once per Upload: take the box of new cells + margin m, seed it from the stored distances (new cells = 0, no
     TouchAt), run the two chamfer passes inside the box only, then apply a band lookup table (rebuilt in LabSet) to
     cells whose distance changed.
   - Dirty only tiles where some band value changed (instead of "every tile in stamp box ± margin").
   - Keep `BandTile` only for tiles born outside the window and for queued stored neighbours.
   - Check: for 1000 random stamps (TEST_KIT bench), the incremental band = the full BandTile band within 1e-4. Log
     the max difference.
2. **BAND_MICRO** (fix a, for the remaining BandTile calls): read `m_F.m_aTouch` directly when tile + margin lies
   inside the window, if-compares instead of `Math.Min`, and the band lookup table instead of `Math.Cos`.
3. **BUILD_SKIP_SAME** (L5 early-out): the would-skip compare already runs before the quad loop (~7968-7980). When the
   max |vy - lastY| < 3 mm AND the bare / hole classes and traces are unchanged, keep the old mesh (no new
   MeshObject) and count it as skipped.
4. **UV64_FAST:** compute the TOP_UV64 UVs (~8504-8530) straight into a member holder array, without the Round /
   x1e6 trick through cix, and without the restore loop.
5. **BAND_STORED_HOLD:** stored tiles rebuilt only for band reasons obey SLOT_MIN_S, like live slots.
6. Turn EDGE_AUDIT off by default once Part A is checked (keep the switch).

## Part D - nothing new; tidy
- The "live built" meter: LabCounts keeps a stale `m_aTileRes` after a null build (~7268-7269). Count only slots whose
  last build returned a mesh.
- Note in the build report every switch you added and its default (all off).

## Workbench session (one launch; look and function only)
1. Compile clean, 0 MUD script errors.
2. Sheet local preset 1, every new switch OFF:
   - drive the Ural >= 20 s on grass, then STOP >= 12 s so one 10 s line falls entirely in the stop;
   - read the edge audit lines, driving vs stopped. Expected from the research: stopped = open 0 / step 0; driving =
     mostly "band 0" opens and "step, beside band lower".
   - If the STOPPED line shows open > 0, report its category; that is a path the research missed.
3. Turn on EDGE_COBUILD_PICK, then add EDGE_COBUILD, then add SKIRT_APRON; drive the same way for each. Is the wall
   gone by eye (screenshots beside the rut, the same angle as my screenshot)? How do the audit numbers move?
4. Turn on BAND_INCR + BAND_MICRO: is the look identical (screenshot the same rut), and is the bench max difference
   < 1e-4?
5. Turn on BUILD_SKIP_SAME + UV64_FAST: is the look identical?
6. Craters, with the CRATER_* switches off, then on. In each case ask me whether the crater shows late (after the
   flash), or the grass part shows before the mud part:
   - 3 mortar hits on the mud ruts with the truck stopped;
   - 3 while driving;
   - 3 within 1 s behind the moving truck;
   - 3 more than 40 m from the camera.
   Paste the per-crater lines.
7. Switch to off: all tiles gone, ground as before.

## DS session (after I OK the WB session)
DS + PeerPlugin1 as in CLAUDE.md, a fresh truck, the robot reverse circle (2 min) on grass, at least 6 runs per row:
1. off (baseline while driving: must be in this session);
2. sheet local preset 1, all levers and fixes off;
3. levers L1 + L2 + L3 + L6 + L8 on;
4. row 3 + BAND_INCR + BAND_MICRO;
5. row 4 + BUILD_SKIP_SAME + UV64_FAST + BAND_STORED_HOLD;
6. row 5 + EDGE_COBUILD_PICK + EDGE_COBUILD + SKIRT_APRON (the cost of the edge fix);
7. the 12 crater shells with CRATER_* on (timings from the per-crater lines).
Write `docs/superpowers/results/2026-10-2x-mud-lab-stage2-ds.md`:
- one table in the 10 s columns (with 'upload' now = builds only, and 'band' separate);
- builds by reason and would-skip per row;
- the crater line summary;
- your read of what each switch saved.

## Report back after each launch
- Files and methods per part, and EVERY line you touched in existing files.
- Every switch and its default.
- The compile result, the log lines, the screenshots.
- Anything you could not do as written, and why.
