# Prompt for the local AI: mud lab stage 1 - lab framework + sheet local + probe field (2026-10-20, v2)

v2: deformation only (user 2026-10-20). Spray and muddy vehicles are parked. Copy everything below the line.

---

Build stage 1 of `docs/superpowers/plans/2026-10-20-mud-lab.md` (v2) on branch `claude/upbeat-lovelace-q0w022`.
Stage 1 has three parts: the lab framework, surface mode "sheet local", and the probe field (wheel track recorder,
replay, and the first rule lanes). Then run one WB launch (function and look only, no ms) and one DS session (plan
sec 4). Stop and report after each launch.

## Read first
- `CLAUDE.md`.
- The plan (all of it).
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
3. **Stamping.** `ZoneAtWorld` in `Wheels()` becomes "holed offroad ground" in local mode.
4. **Sparse tiles:**
   - BORN preset a / b;
   - DROP_S 5;
   - TILE_CAP 1500, dropping the farthest first;
   - no zone pre-build, no RADIUS sweep.
5. **Edge fade.** An open side means no sheet tile next to it; fix this for local mode only.
6. **Several vehicles:** STAMP_R 80 m, LIVE_N 2 live windows, the rest at the stored pace.
7. **Holes:** crater and dig tiles are built in the blast frame; `DigNow`'s stored path (~3446) queues its taper
   neighbours.
8. **Keep v206's base height read.**
9. **Presets:** BORN a / b x THICK 0.15 / 0.08.

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
   - S2, volume kept: per stamp, the removed volume = the volume added to the ridges and the bow wave. Log the
     imbalance.
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
