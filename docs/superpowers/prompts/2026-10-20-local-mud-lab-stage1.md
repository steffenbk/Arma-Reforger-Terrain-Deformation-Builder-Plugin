# Prompt for the local AI: mud lab stage 1 - lab framework + sheet local + wheel spray (2026-10-20)

Copy everything below the line into the local AI.

---

Build stage 1 of `docs/superpowers/plans/2026-10-20-mud-lab.md` on branch `claude/upbeat-lovelace-q0w022`.
Stage 1 has three parts: the lab framework, mode 1 "sheet local" and mode 2 "wheel mud spray". Then run one WB smoke
check (function and look only, no ms) and one DS session (plan sec 4). Stop and report after each launch.

## Read first
- `CLAUDE.md`.
- The plan (all of it).
- `research/2026-10-20-mud-look.md`: why D1 / D2 failed, and the option A details.
- `results/2026-10-20-mud-two-designs-ds.md`.
- `research/2026-10-19-whole-map-sheet-issues.md` sec 2-3: crater / dig rebuild, edge fade, upload slots.
- `mud-gotchas.md`:
  - Enforce limits: 64 locals including parameters, 80 KB VM stack, int[] not bool[], Math.Min/Max return float,
    '%' only on its own int line.
  - MeshObject rules: a fresh mesh per change, a distinct emat per section, indices go in before vertices, every index
    below the vertex count.
- Code you will reuse: `MUD_MudOverlay.c:140-215` (today's numpad 3 cycle and DbgUI label, which become the lab), and
  `MUD_MudSheet.c` (6624 lines) with `MUD_MudZone.c:62` (`SheetCovers`).

## Hard rules
- **Do not touch the voxel ruts:** rut ops, the rut field, wear, look ruts, patch meshes and colliders, the op format.
- **The only allowed edits in rut-side files** are the coupling gates the plan lists for sheet local. Each one must keep
  "sheet off" byte-for-byte today's behaviour. If anything else seems to need a change in a rut file, stop and ask me.
- **Client only.** Nothing new runs on a DS. Nothing writes J2, ops or snapshots.
- **Defaults off.** New play resets everything to off.
- **Checkpoints:** `mud_backups/checkpoint.ps1 save "before mud lab"` first, then `... save "before sheet local"` and
  `... save "before mud spray"` before those parts.
- **Git:** `git add --ignore-removal .`, commit per part, push to this branch. No PR.

## Part A - lab framework
1. Create a new file `Scripts/Game/Mud/MUD_MudLab.c` (plan sec 1).
   - The mode registry has surface modes (one at a time) and add-ons (stackable).
   - Use a base class `MUD_LabMode`, or a switch if inheritance fights you.
   - Move the numpad 3 cycle and the DbgUI label out of MUD_MudOverlay into it. D1 and D2 become registered surface
     modes with their existing presets. They must behave exactly as before.
2. Change the call site `MUD_WorldGround.c:1086` from `MUD_MudOverlay.Tick` to `MUD_MudLab.Tick`. Keep the D2 10 s log
   (`PressLog`) running from the lab.
3. Keys, TEST_KIT only:
   - numpad 3 = next surface step;
   - Left Ctrl + numpad 3 = next add-on combination (check that `KeyCode.KC_LCONTROL` exists, or use a free modifier and
     tell me which).
4. Logs (exact formats are in plan sec 1):
   - the switch line `MUD lab: ...`;
   - the common 10 s line `MUD lab 10s: ...`, where MUD ms is the same meter as the 2026-10-20 results (say which one).
   - the DbgUI label shows `step N of M`, the surface name and preset, and the add-ons.

## Part B - mode 1 "sheet local" (surface)
1. Switching:
   - Turn `MUD_MudSheet.ENABLED` into a runtime `static bool` driven by the lab.
   - Enter starts the sheet from fresh state. Exit deletes every sheet tile entity and frees all sheet data (log the
     counts).
2. Couplings off, as in plan sec 2 mode 1:
   - set `SYNC`, `LAZY_RUT_MESH`, `MUD_LookRuts.SHEET_LOOK` and `SHEET_SKIP` to false (`ONE_GRID` is false already);
   - make `SheetCovers` return false unless `ENABLED && SYNC`.
   - Then grep every reference from MUD_VoxelProbe, MUD_LookRuts, MUD_WorldGround, MUD_FinePatch and MUD_CoarseSkin to
     `MUD_MudSheet.*` / `MUD_MudSync.*` / `SheetCovers`, and list each one with the gate that keeps it inert. That list
     goes in your report.
3. Sparse tiles:
   - no zone pre-build, no RADIUS sweep;
   - tiles are born around wheels with the BORN preset: (a) on stamp, or (b) within 1 tile of a wheel;
   - unstamped tiles drop after DROP_S (5 s) with no wheel near;
   - stamped tiles are kept up to TILE_CAP (1500), dropping the farthest from the camera first.
4. Edge fade: a side with no sheet tile next to it counts as open and fades over EDGE. Fix `HasTile` / the taper input
   for local mode only.
5. Several vehicles:
   - every vehicle within STAMP_R (80 m) of the camera stamps into tile data;
   - the nearest LIVE_N (2) get the 10 Hz live window;
   - the rest rebuild at the stored pace.
6. Holes:
   - rebuild crater and dig tiles in the blast frame, not via `UploadPick`;
   - `DigNow`'s stored path (~3446) also queues its taper neighbours.
7. Keep the sheet's v206 base height read. Do not invent a new one.
8. Presets: BORN a / b x THICK 0.15 / 0.08, which is 4 surface steps.
9. Mode extras in the 10 s line: `born <n> dropped <n> kept <n> cap-drops <n> | live <n> stored-stamp <n> | sh wheels / upload / stored / slump ms`.

## Part C - mode 2 "wheel mud spray" (add-on)
1. Find a vanilla wheel dirt or mud particle (`.ptc`) with the Enfusion MCP or the resource browser. Tell me which one,
   and its GUID.
2. Emit it client-side at a wheel's contact point while the wheel is in mud: on a sheet-local tile, or on rut dirt
   (the floor below `FIELD_SOIL_D`, or the mud_rut gamemat).
   - The rate scales with wheel speed and slip;
   - cap it per wheel;
   - emit only within SPRAY_R (60 m) of the camera;
   - pool or recycle the emitters.
3. Do NOT edit gamemats in this stage (plan sec 2 mode 2).
4. Mode extras in the 10 s line: `spray emits <n> live <n>`.

## WB smoke check (one launch, function and look only)
On grass, with a Ural:
1. Cycle through every surface step and add-on. Check that the label, log lines and the step count are right.
2. In sheet local, for each preset:
   - Do tiles appear around the wheels, does the mud deform and flow like the old sheet, do the edges fade (no step)?
   - Look at 2 / 20 / 60 m.
   - Is there grass on the band?
   - Does the tyre sit in the groove? Is the felt rut still felt?
3. Make a crater and a trench in a sheet area.
4. Switch back to off. Are all tiles gone? Is the ground exactly as before?
5. Turn spray on, on top of off and on top of sheet local. Take screenshots.
6. Spawn a second vehicle and drive it about 40 m away.

## DS session
Run plan sec 4, rows 1-7, with at least 6 runs per numbered row. Write `results/2026-10-2x-mud-lab-stage1-ds.md` with one
table in the 10 s line's columns, plus the by-eye notes.

## Report back
- The files and methods per step, and every line you touched in existing files.
- The coupling-gate list from Part B step 2.
- The particle GUID.
- The compile result, log lines and screenshots per step.
- Anything odd.

Do not tune values or turn anything on by default. I compare and pick.
