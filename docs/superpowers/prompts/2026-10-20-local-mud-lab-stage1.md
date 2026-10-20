# Prompt for the local AI: mud lab stage 1 - lab framework + sheet local + probe field (2026-10-20, v3)

v3: deformation only (user 2026-10-20); sheet local = the deposit band from research/2026-10-20-mud-sheet-deep-dive.md, plus cost levers and sync-lite remote stamps; lane S2 dropped (tried before, invisible). Copy everything below the line.

---

Build stage 1 of `docs/superpowers/plans/2026-10-20-mud-lab.md` (v3) on branch `claude/upbeat-lovelace-q0w022`.
Stage 1 has three parts: the lab framework, surface mode "sheet local", and the probe field (wheel track recorder,
replay, and the first rule lanes). Then run one WB launch (function and look only, no ms) and one DS session (plan
sec 4). Stop and report after each launch.

## Read first
- `CLAUDE.md`.
- The plan (all of it).
- `research/2026-10-20-mud-sheet-deep-dive.md`: why the sheet cost and popped, the deposit band, levers L1-L10.
- `research/2026-10-20-mud-look.md`.
- `results/2026-10-20-mud-two-designs-ds.md`.
- `research/2026-10-19-whole-map-sheet-issues.md` sec 2-3.
- `results/2026-10-19-sheet-sync-maths-offline.md`: slump one-pass maths.
- `mud-gotchas.md`:
  - Enforce limits: 64 locals including parameters, 80 KB VM stack, int[] not bool[], Math.Min/Max return float,
    '%' only on its own int line.
  - MeshObject rules: a fresh mesh per change, a distinct emat per section, indices before vertices, every index below
    the vertex count.
- Code you will reuse:
  - `MUD_MudOverlay.c:140-215`: the numpad 3 cycle and the DbgUI label;
  - `MUD_MudSheet.c`, especially `Wheels()` at 4067, `Stamp`, `Slump`, `BuildTileMesh`;
  - `MUD_MudZone.c:62`: `SheetCovers`.

## Hard rules
- **Do not touch the voxel ruts:** rut ops, the rut field, wear, look ruts, patch meshes and colliders, the op format.
- **The only allowed edits in rut-side files** are the coupling gates the plan lists for sheet local, and each one must
  keep "sheet off" byte-for-byte today's behaviour. If anything else seems to need a change in a rut file, stop and ask
  me.
- **Client only.** Nothing new runs on a DS. Nothing writes J2, ops or snapshots. Probe lanes never submit rut ops.
- **Defaults off.** New play resets everything to off.
- **Checkpoints:** `mud_backups/checkpoint.ps1 save "before mud lab"` first, then `... save "before sheet local"` and
  `... save "before probe field"` before those parts.
- **Git:** `git add --ignore-removal .`, commit per part, push to this branch. No PR.

## Part A - lab framework
1. Create a new file `Scripts/Game/Mud/MUD_MudLab.c` (plan sec 1.3).
   - Move the numpad 3 cycle and the label out of MUD_MudOverlay into it.
   - D1 and D2 become registered surface modes with their existing presets, and behave exactly as before.
2. Change the call site `MUD_WorldGround.c:1086` to `MUD_MudLab.Tick`. Keep D2's `PressLog` running.
3. Keys, TEST_KIT only:
   - numpad 3 = next surface step;
   - Left Ctrl + numpad 3 = the probe field steps (plan sec 1.3; check that `KeyCode.KC_LCONTROL` exists, or tell me what
     you used).
4. Log lines exactly as plan sec 1.3. MUD ms = the same meter as the 2026-10-20 results (say which one).

## Part B - surface mode "sheet local"
Follow plan sec 2:
1. **Switching.** `ENABLED` becomes a runtime static driven by the lab. Exit frees every tile and all data (log the
   counts). Enter starts fresh.
2. **Couplings off** (SYNC, LAZY_RUT_MESH, SHEET_LOOK, SHEET_SKIP false; the `SheetCovers` gate). Then grep every
   reference from MUD_VoxelProbe, MUD_LookRuts, MUD_WorldGround, MUD_FinePatch and MUD_CoarseSkin to `MUD_MudSheet.*` /
   `MUD_MudSync.*` / `SheetCovers`, and list each one with the gate that keeps it inert. The list goes in your report.
3. **Builds by reason, FIRST.** Add the 10 s log line before changing anything else:
   `MUD sheet builds: wheel N, oldest N, nearest N, mesh-less N, freeze N, restore N, slump N | would skip (max change < 3 mm) N`.
   Run it once on today's sheet in WB, as a function check only. The DS numbers come in the session.
4. **Stamping.** `ZoneAtWorld` in `Wheels()` becomes "holed offroad ground" in local mode.
5. **Deposit band** (plan sec 2, deep dive sec 3; this replaces the tile-side fade).
   - Per world cell, the distance to the nearest touched cell (stamped or pressed). Keep it incrementally: a stamp
     updates only the cells within BAND_IN + BAND_FADE of the cells it wrote.
   - Thickness = THICK x band(dist): 1 within BAND_IN, a sine falloff to 0 at BAND_IN + BAND_FADE, and 1 cm UNDER the
     drawn ground at 0. Quads entirely under the ground are skipped.
   - Compute it across tile borders, like the taper field. A band change within reach of a tile side marks that
     neighbour.
   - Untouched cells have no tile, and `MudAt` returns 0 for unborn cells (callers MUD_TrenchGhost 87,
     MUD_VehicleAccess 138).
   - The live window draws and freezes only born slots. Today it does all 16 (Follow 1077, Upload 5248).
   - No zone pre-build, no RADIUS sweep, no patch lift, no LIFT_BY_DRAWN, no LAZY_RUT_MESH, SPEED_RADIUS off, LOD
     double build off.
   - Never build a slot until all its cells are traced. Finish eases in `Freeze`.
   - MAT_TOP uses the 64 m chunk UV frame, as voxel tops do (MUD_VoxelProbe 576).
6. **Far data.**
   - Far touched tiles keep data only, compact: 1 byte per height / drag u / drag v / bare+dirt, ~1.8 KB per tile.
   - TILE_DATA_CAP 20,000; drop the oldest far tiles first.
   - Meshes within a fixed 70 m radius, nearest-ahead first, 2 ms/frame budget. A far ring past 40 m is built once at
     11 x 11 and never upgraded on screen.
7. **Remote trucks, sync-lite.**
   - A read-only listener on the rut ops this client receives stamps their segments into tile data with the same Stamp
     maths.
   - No hash, quantise, snapshot or DS work. Skip ops from the vehicle you drive.
   - Tell me where you hooked it. It must be a read after the rut work, with no change to ops or Submit.
8. **Cost levers** (deep dive sec 2):
   - L1: carry each slot's dirty flag through Follow; Freeze builds only real changes; no forced upload unless a slot
     changed.
   - L2: LimitDrag once per upload over the stamped cells + 1.
   - L3: slump only on cells changed since the last slump + 1 ring, one pass at 0.6.
   - L6: EaseTick at upload rate.
   - L8: vehicle loops get a distance pre-check, a cached sim component and pruned `m_aVeh`.
   Each lever gets its own static switch for the A/B.
9. **Holes:** crater and dig tiles are built in the blast frame; `DigNow`'s stored path (~3446) queues its taper
   neighbours.
10. **Keep v206's base height read.**
11. **Presets:** BAND_IN 0.4 / 0.8 x BAND_FADE 0.8 / 1.2.

## Part C - probe field
1. **Recorder** (plan sec 1.1).
   - A TEST_KIT toggle (say which input) writes every Stamp input that `Wheels()` would make to
     `$profile:MUD_wheeltrack_<date>_<n>.csv`, with the columns: t, veh, wheel, lp xyz, cp xyz, speed, half.
   - It works with the sheet off: the same contact reads, no stamping.
   - Record three files in WB:
     - the robot circle (2 min);
     - a straight line driven 1, 2 and 4 times over the same spot;
     - a crossing.
   - Commit the CSVs under `Data/Mud/wheeltracks/`.
2. **Field and replay.**
   - Lanes are 12 m wide with 2 m gaps, placed in front of the player on flat holed grass. Each lane has a DbgUI or
     world label.
   - Replay moves the track to each lane's origin and re-reads the ground height at the lane (read only).
   - Two replay speeds: 1x, and "all at once" for cost runs.
   - Lane meshes are their own entities and are freed when the field is removed.
3. **Lanes in this stage.** All of them use renderer R1: the sheet's tile build, as a separate instance per lane, not
   shared state.
   - S1, the reference.
   - S3, slow settle: per-cell age; settling cells run at 2-5 Hz for SETTLE_S (start 10 s); the walls sag and the
     ridges spread.
   - S5 wet: THICK, SOFT, SLUMP and settle as one preset (wet = deeper first pass, more flow).
   - S6, one-pass slump 0.6.
   - If running several sheet instances at once is too tangled in MUD_MudSheet, make the lane a thin copy of its stamp,
     slump and tile build in a new file `MUD_MudLabLane.c`. Copy the code, do not call into MUD_MudSheet's state. Then
     tell me.
4. **Per-lane 10 s line** as in plan sec 1.3.

## WB launch (function and look only)
1. Record the three tracks (Part C1).
2. Cycle through every surface step and check the label, logs and step count. Drive in sheet local, each preset:
   - Do tiles appear at the wheels? Does it deform and flow like the old sheet?
   - Do the edges fade? Is there grass on the band? Does the tyre sit in the groove? Is the felt rut still felt?
   - Make a crater and a trench. Switch to off: are all tiles gone?
3. Probe field: replay each track into all lanes and look from 2 / 20 / 60 m. Take one screenshot per lane per track,
   with the lane label in view.

## DS session
Run plan sec 4, rows 1-5, with at least 6 runs per row (row 5 = 6 replays per track). Write
`results/2026-10-2x-mud-lab-stage1-ds.md` with a surface table and a lane table, plus the by-eye notes.

## Report back
- The files and methods, and every line you touched in existing files.
- The coupling-gate list.
- The recorder input and the CSV paths.
- The compile result, log lines and screenshots.
- Anything odd.

Do not tune values or turn anything on by default. I compare and pick.
