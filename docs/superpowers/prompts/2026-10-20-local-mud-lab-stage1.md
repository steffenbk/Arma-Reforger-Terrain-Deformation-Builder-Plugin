# Prompt for the local AI: mud lab stage 1 - switcher + sheet local (deposit band) + cost levers + remote stamps (2026-10-20, v4)

v4: the probe field (recorder, replay, lanes) moved to stage 2, so stage 1 is only the sheet. Copy everything below
the line into Claude Desktop.

---

You have direct access to my repo. Branch: `claude/upbeat-lovelace-q0w022` (pull it first). The project is my Arma
Reforger mud mod ("Testing plan vech msh", Enforce Script, scripts in `Scripts/Game/Mud`).

## Goal
Bring back my old mud sheet (`MUD_MudSheet.c`, parked since 2026-10-18), which had the look I want (soft 15 cm mud,
ridges, bow wave, slump, live 10 Hz deformation under the wheel), and fix its three problems:
1. **Pop-in.** It drew a raised 15 cm layer over ALL ground, so every new tile appeared as a 3 x 3 m slab.
2. **No whole-map coverage.** That layer would need ~622,000 tiles.
3. **Script cost.** ~35-40 % of its live rebuilds change nothing, plus a bit-exact sync layer.

The fix is the **deposit band**:
- mud exists only around where wheels pressed, full thickness near the track, fading to nothing ~1 m out, ending just
  under the ground mesh;
- untouched ground has no sheet tile at all, so there is nothing to pop;
- the sheet runs client-only (no sync layer);
- remote trucks are stamped from the rut ops the client already receives;
- plus the cost fixes found in the code.

Build it behind my numpad 3 look switcher. Then run one Workbench check and one DS session. Stop and report after each
launch.

## Read first (in this order)
1. `CLAUDE.md`: project rules (testing, checkpoints, git, feature freeze).
2. `docs/superpowers/research/2026-10-20-mud-sheet-deep-dive.md`: the WHY behind every step here. Sec 1 root causes,
   sec 2 levers L1-L10, sec 3 the deposit band, sec 4 remote trucks.
3. `docs/superpowers/plans/2026-10-20-mud-lab.md` (v3): sec 1.3 the switcher, sec 2 sheet local, sec 4 the DS session.
4. `docs/superpowers/research/2026-10-20-mud-look.md`: why my last two designs (D1, D2) failed.
5. `docs/superpowers/research/2026-10-19-whole-map-sheet-issues.md` sec 2-3: crater / dig rebuild fixes.
6. `docs/superpowers/research/mud-gotchas.md`. Above all:
   - Enforce limits: 64 locals in scope including parameters, 80 KB VM stack, int[] not bool[] for members,
     Math.Min/Max return float, '%' only on its own int line.
   - MeshObject rules: a fresh mesh per change, a distinct emat per section, indices before vertices, every index
     below the vertex count.
7. `docs/features/mud-sheet/README.md`: the sheet's history and lesson 7 (two systems drawing one surface = seams).

## Hard rules
- **Do not touch the voxel ruts:** rut ops, the op format, the rut field, wear, look ruts, patch meshes and colliders.
  - The only edits allowed in rut-side files (MUD_VoxelProbe, MUD_LookRuts, MUD_WorldGround, MUD_FinePatch,
    MUD_CoarseSkin) are the sheet coupling gates listed in Part B.
  - With the sheet off, each of those files must behave byte-for-byte as today.
  - If anything else seems to need a change there, STOP and ask me.
- **Client only.** Nothing new runs on a dedicated server. Nothing writes J2, ops or snapshots.
- **Defaults off.** New play resets everything to off. Do not tune values or turn anything on by default; I compare and
  pick.
- **Every lever gets its own static switch** (not const), so I can A/B it.
- **Checkpoint before each part:** `mud_backups/checkpoint.ps1 save "before mud lab A"`, then "... B", "... C",
  "... D", "... E".
- **Git:** stage with `git add --ignore-removal .`, commit per part with a clear message, push to this branch. No PR.
- **Tests follow CLAUDE.md TESTING RULES.** Workbench is for function and look only, never ms or fps. Numbers come
  only from the DS session, with at least 6 runs per row.

## Part A - the lab switcher
1. Create a new file `Scripts/Game/Mud/MUD_MudLab.c`. Move the numpad 3 cycle and the DbgUI label out of
   `MUD_MudOverlay.c` (~140-215) into it.
   - Use a small base class (`MUD_LabMode`: `Name()`, `Presets()`, `Enter(preset)`, `Exit()`, `Tick(time)`,
     `Label()`, `Log10s()`), or a switch over mode ids if inheritance fights you.
   - D1 (overlay) and D2 (pressed lift) become registered modes with their existing presets, and behave exactly as
     before.
   - Surface cycle: off -> sheet local presets -> D2 presets -> D1 presets -> off. Only one at a time.
2. Change the call site `MUD_WorldGround.c:1086` from `MUD_MudOverlay.Tick(m_fTime)` to `MUD_MudLab.Tick(m_fTime)`.
   Keep D2's 10 s `PressLog` running.
3. Key (TEST_KIT only): numpad 3 = next step. Numpad 1-9 are all taken, so take no other key.
4. Logs:
   - on every switch: `MUD lab: surface <name> preset <p> | (key|new play)`;
   - every 10 s while a mode is on:
     `MUD lab 10s: surface <name>/<p> | fps avg <f> min <f> | MUD ms <f> | mode ms <f> | tiles <n> built <n> kept <n> | <extras>`.
     MUD ms = the same meter the 2026-10-20 results used; tell me which one.

## Part B - surface mode "sheet local" with the deposit band
1. **Switching.** `MUD_MudSheet.ENABLED` becomes a runtime `static bool` driven by the lab (today it is
   `static const`).
   - Exit deletes every sheet tile entity and frees all sheet data; log the counts.
   - Enter starts fresh.
2. **Couplings off.**
   - `SYNC`, `LAZY_RUT_MESH`, `MUD_LookRuts.SHEET_LOOK` and `MUD_LookRuts.SHEET_SKIP` = false (`ONE_GRID` is already
     false).
   - `MUD_MudZoneEntity.SheetCovers` (`MUD_MudZone.c:62`) returns false unless `ENABLED && SYNC`.
   - Then grep every reference from the rut-side files to `MUD_MudSheet.*`, `MUD_MudSync.*` and `SheetCovers`. List
     each one with the gate that keeps it inert in sheet local AND with the sheet off. That list goes in your report.
   - The deep dive found SPARSE and the patch lift depend on `PatchLiftOn()` (needs SYNC). Sheet local uses NEITHER:
     no patch lift, no LIFT_BY_DRAWN, no lazy rut mesh. The patch draws untouched ground exactly as today.
3. **Builds-by-reason log, before any other change.** Every 10 s:
   `MUD sheet builds: wheel N, oldest N, nearest N, mesh-less N, freeze N, restore N, slump N, band N | would skip (max change < 3 mm) N`.
4. **Where wheels stamp.** `ZoneAtWorld` in `Wheels()` (~4109) becomes "holed offroad ground" in local mode, so no zone
   entity is needed.
5. **Deposit band** (deep dive sec 3):
   - A cell is "touched" when any stamp wrote it, as a press or a push (ridges / bow wave count).
   - Keep a per-world-cell distance to the nearest touched cell, tile by tile with a margin of
     ceil((BAND_IN + BAND_FADE) / 0.15) cells across tile borders. A 2-pass chamfer distance over the tile + margin,
     recomputed at upload rate only for tiles whose touched set changed, is fine.
   - `band = 1` for dist <= BAND_IN, a sine falloff to 0 at BAND_IN + BAND_FADE.
   - Drawn height per vertex: `h = G + band * (m + LIFT) - (1 - band) * 0.01`, where G = the sheet's ground read as
     in v206 and m = the sheet's mud value as today (THICK at rest, pressed down, ridges up). At band 0 the surface
     sits 1 cm under the drawn ground, so the band edge comes out from under the patch: no step, no z-fight. Skip quads
     whose four corners are all band 0.
   - Untouched cells: no tile. A tile is born when a stamp first touches it. A band change within reach of a tile side
     marks that neighbour.
   - No tile-side EDGE fade in this mode. `HasTile` plays no part.
   - `MudAt` (~3885) returns 0 for cells of unborn tiles, not THICK (callers: `MUD_TrenchGhost.c:87`,
     `MUD_VehicleAccess.c:138`). With SYNC off, `MUD_MudSync.Get`'s THICK default must not leak in.
   - The live window draws and freezes ONLY born slots. Today it draws all 16 (`Upload` ~5248) and freezes all 16
     (`Follow` ~1077).
   - No zone pre-build, no RADIUS sweep of untouched tiles, `SPEED_RADIUS` off, LOD low->full double build off.
   - Never build a slot until all its cells are traced (no partial meshes).
   - Finish eases for a slot's cells in `Freeze`.
   - MAT_TOP uses the 64 m chunk UV frame, as voxel tops do (`MUD_VoxelProbe.c:576`), so the texture keeps its scale
     at the band edge.
6. **Holes.**
   - Crater and dig tiles are built in the blast frame (not via `UploadPick` / `UPLOAD_SLOTS` / `SLOT_MIN_S`).
   - `DigNow`'s stored path (~3446) also rebuilds its taper neighbours, at the front of the queue.
7. **Presets** (4 surface steps): BAND_IN 0.4 / 0.8 x BAND_FADE 0.8 / 1.2. THICK stays 0.15.
8. **Mode extras** in the 10 s line: `born <n> touched cells <n> | live <n> stored <n> data-only <n> | sh wheels / upload / stored / slump / band ms`.

## Part C - cost levers (deep dive sec 2)
Each has its own static switch, default off:
- **L1:** carry each live slot's dirty flag through `Follow` instead of `UpdateSlots` dirtying all 16 (~1408). A slot
  entering with a current full mesh starts clean. `Freeze` builds only slots with real changes (~1243). No forced
  upload on shift unless a slot really changed (~1195).
- **L2:** `LimitDrag` once per upload over the union of stamped cells + 1, not over each stamp's whole box (~4868).
  Inline the clamps.
- **L3:** slump only cells changed since the last slump, + 1 ring, one pass at factor 0.6 (offline equal to 3 passes at
  0.25, results/2026-10-19-sheet-sync-maths-offline.md). No per-cell MarkRect; mark the changed rectangle once.
- **L6:** `EaseTick` at upload rate, not every frame (~800).
- **L8:** vehicle loops get a distance pre-check to the window, a cached `VehicleWheeledSimulation` per vehicle, a map
  instead of the linear `m_aVeh` search, and dead / far entries pruned (~4069-4085, `PickWheelSlots`, `UploadPick`).
Do NOT do the build rewrite (L4), compare-to-last (L5), pass arrays (L7) or skirt sharing (L9) yet. They come in
stage 2, after the DS numbers.

## Part D - far data and mesh radius
- A touched tile far from the camera keeps data only, compact: 1 byte each for height (0-0.3 m), drag u, drag v and
  bare+dirt class, ~1.8 KB per tile.
- `TILE_DATA_CAP` 20,000; drop the oldest far tiles first. The voxel ruts stay there, so the ground keeps its shape.
- Meshes only within a fixed `MESH_R` 70 m, nearest-ahead first, `MESH_MS` 2 ms per frame.
- A far ring past `FAR_R` 40 m is built once at 11 x 11 and never upgraded on screen. Within 40 m a tile is built full
  once.
- A mesh dropped past `MESH_R` + 10 m is deleted at once. There is no patch re-lift to wait for in this mode.

## Part E - remote trucks ("sync-lite")
- `MUD_VoxelProbe.Submit` already calls `MUD_MudSync.RutOp(op)` for every kind-4 rut op (~4217) and
  `MUD_MudSync.ClearOp(op)` for craters / digs (~4220). Today both return early when SYNC is off (`OnHere`).
- In sheet local, route them from INSIDE `MUD_MudSync.c` (a sheet file, so no change in the rut file):
  - `RutOp` -> `MUD_MudSheet.OpStamps(op)`: stamp `op.m_aSegA[i] -> op.m_aSegB[i]` into the tile data with the same
    Stamp maths;
  - `ClearOp` -> the sheet's existing crater / dig handling.
- The op has no vehicle id or speed with SYNC off, because the op tag is off and stays off (it would change ops). So:
  - default speed `OP_SPEED` 5 m/s;
  - tyre half-width from `TyreHalf` of a typical radius, `OP_RADIUS` 0.5 m;
  - skip segments that lie within `COVER_R` 0.3 m of this client's own recent stamps (the old `TakeCovered` idea),
    so your own truck is not stamped twice.
- No hash, no quantise, no snapshot, no DS work.
- Mode extras: `op stamps <n> skipped own <n>`.

## Workbench check (one launch; function and look only, no ms)
1. Compile clean, with 0 MUD script errors.
2. Cycle numpad 3 through every step: the label, both log lines, and the step count are right. D1 and D2 behave as
   before.
3. In sheet local, drive a Ural on grass at each preset. Check:
   - Is there any pop when the band is born? Look from 5 m beside the truck.
   - Is there any step at the band edge?
   - Does it deform and flow like the old sheet: groove from the first pass, ridges, bow wave, slump?
   - Does the tyre sit in the groove? Is the felt rut still felt?
   - Is there grass on the band? Does the texture scale match at the edge?
4. A crater and a short trench in a mud band: are the holes clean in the blast frame, with no lip?
5. Drive 200 m away and come back: far ring builds, no pop, data-only tiles come back as meshes.
6. Switch to off: are all tiles gone, and is the ground exactly as before?
7. Take screenshots per preset, with the log lines next to them.

## DS session (after I OK the WB check)
DS + PeerPlugin1 as in CLAUDE.md, a fresh truck after start, the robot reverse circle (2 min) on grass, at least 6 runs
per row:
1. off (baseline while driving).
2. sheet local, BAND 0.4 / 0.8, levers OFF.
3. the same with L1 + L2 + L3 + L6 + L8 ON. Then each lever alone for 6 runs, if the combined run shows a clear gain.
4. D2 preset 4 (the old comparison).
5. A second truck driven by the second client ~40 m away: does it get mud (Part E), and how far behind the truck?
6. A crater + trench in a band.
Write `docs/superpowers/results/2026-10-2x-mud-lab-stage1-ds.md`:
- one table in the 10 s line's columns plus the builds-by-reason counts;
- the by-eye notes;
- your read of whether the 35-40 % avoidable-build estimate held.

## Report back after each launch
- Files and methods per part, and EVERY line you touched in existing files.
- The coupling-gate list (Part B step 2).
- The compile result, the log lines, the screenshots.
- Anything odd, or anything you could not do as written, and why.
