# Prompt: synced mud sheet (the parked sheet back, look-only, same on every client) - 2026-10-17

Paste everything below the line into a new chat in the mod folder.

-------------------------------------------------------------------------------------------------------------------
Synced mud sheet: bring the PARKED mud sheet (MUD_MudSheet.c) back as a LOOK-ONLY layer. Three rules:
1. It is pressed from the replicated rut ops, so every client draws the same mud.
2. Each client predicts it at the tyres it sees, so the press, bow wave and ridges stay live.
3. The ground wheels and feet stand on stays today's felt rut field.
User go 2026-10-17 (feature-freeze exception). It replaces the "mud look layer" prompt (on hold).
Handoff: .remember/remember.md (loads by itself).

Never overwrite, revert or stash my local changes. No commits.

## Stage 0 - get the prototype files, read, check the code (no code changes)

### 0a. Get the prototype from the cloud branch
- `git fetch origin claude/stoic-clarke-uiodzl`.
- For each file below, check it does not exist locally or has no local changes. Then take it with
  `git checkout origin/claude/stoic-clarke-uiodzl -- <path>`:
  - Tools/mud_sheet_sync_proto.py;
  - Tools/mud_look_proto.py;
  - docs/superpowers/results/2026-10-17-mud-sheet-sync-proto.md;
  - docs/superpowers/results/2026-10-17-mud-look-proto.md;
  - docs/superpowers/results/img/2026-10-17-mud-sync-drive.gif;
  - docs/superpowers/results/img/2026-10-17-mud-sync-top.png;
  - docs/superpowers/results/img/2026-10-17-mud-sync-mesh.png;
  - docs/superpowers/results/img/2026-10-17-mud-sync-cross.png;
  - the 3 docs/superpowers/results/img/2026-10-17-mud-look-*.png files.
- Take NOTHING else: my local docs are newer.
- Run `python Tools/mud_sheet_sync_proto.py` (numpy + matplotlib; it imports scm_lite_proto and mud_look_proto). Its
  table must match the results file.
- The proto is the REFERENCE MATHS for Stages 2-3. Vectorised Stamp / Slump, rollback + replay and the handoff morph
  are all in it.

### 0b. Read
- The sync results file, its gif and pictures.
- docs/features/mud-sheet/README.md + 2026-10-15-assessment.md: why the sheet failed.
  - ONE_GRID made the client-only sheet the physical ground, so the DS drove on different ground (likely the DS
    stutter).
  - Zone tiles were all pre-built.
  - The stitch rules cost 5.7-9 ms per frame in WB.
  - Every op was done twice.
- docs/features/ruts/README.md; docs/superpowers/research/mud-gotchas.md (MeshObject rules; Enforce traps).
- CLAUDE.md TESTING RULES.

### 0c. Check these code anchors in MY files
Line numbers are from git ebe4527 - grep, don't trust them. Report a table:
anchor | expected | found at | same / different.

| # | Anchor | Expected |
|---|---|---|
| 1 | MUD_MudSheet consts | ENABLED false (~:25), FOLLOW true, COLLIDE true (~:106), ONE_GRID true (~:162), DRAG_TEST true (~:200), GRADUAL / PASS_GAP 4 s (~:89-91), TOP_LOOK, THICK 0.15, CELL 0.15, TILE_M 3 |
| 2 | MUD_MudSheet.Wheels (~:2539) | per vehicle, per wheel contact, every frame -> Stamp(lp, cp, speed, time, vehId, TyreHalf(radius)) |
| 3 | Stamp (~:2735), Slump (~:3024), Drag (~:2614) | as copied in the proto; Drag = forces on vehicles (SP only) |
| 4 | Pass rule in Stamp | m_mPassT / m_mPassV / m_mPassD, PassKey, by local TIME (not deterministic) |
| 5 | TakeOp (~:1369) + MUD_VoxelProbe.Submit (~:3545-3570) | ONE_GRID redirects zone ops to the sheet (TakeOp / Holds / m_bOpBoth / m_bSheetOp / CoversCell) |
| 6 | DS skip | `if (!ENABLED \|\| RplSession.Mode() == RplMode.Dedicated) return;` (~:361) |
| 7 | Zone tiles | MUD_MudZone.c boxes; all zone tiles built at load + stored (MUD_MudSheetTileData); QueueBuild / FlushOneBuild; TaperField; HideZoneGround (~:1142) |
| 8 | Ground under the sheet | GroundAt / ground traces (NEW_TRACES / OLD_TRACES), GroundChanged hook from VoxelProbe go-live (~:5774) - is there a hook when a RUT FIELD patch's floor changes? |
| 9 | Rut op | MUD_VoxelProbe.DoRutOp (~:2958): kind 4, m_fDepth = step, m_fB = cap, m_sPlan = "a,b;a,b" in cm per segment, all wheels of one vehicle in one op, no vehicle id / speed / tyre radius / server time; Encode / Decode (~:3326-3365) "seq\|kind\|x\|y\|z\|a\|b\|piece\|plan\|seed" |
| 10 | Where a kind-4 op is applied on every machine | Submit -> CarveFloor (in seq order; also JIP replay; J2 joiners get tiles, NOT ops) |
| 11 | MUD_WorldGround.Tick -> MUD_MudSheet.Tick (~:909) | the per-frame entry |

**STOP. Ask me:**
1. Where mud is: keep MUD_MudZoneEntity boxes for now (recommended for the first build), or from the terrain gamemat
   (whole map, later)?
2. Late joiners: for now they see old ruts WITHOUT sheet mud (felt rut only). Or plan Stage 5 (snapshot carries the
   mud cells)?
3. Look choice: drawn = felt ground + mud (the rut drops 1.5 m behind the truck when the felt cut comes), or the mud
   fills the felt cut (depth += felt drop, no drop, tyres look sunk deeper)?
4. Anything that changes the stages.

## Stage 1 - the sheet back as LOOK ONLY (no sync yet)
- MUD_MudSheet changes:
  - ENABLED true;
  - ONE_GRID false (ops are never redirected to the sheet - every op goes to voxels / rut field as today);
  - COLLIDE false (no sheet collider at all);
  - DRAG_TEST false (no forces).
- Check every path those switches touch:
  - TakeOp must return false;
  - Holds / m_bOpBoth / m_bSheetOp / CoversCell must not change voxel or rut results;
  - HideZoneGround must not hide our ground.
- The sheet sits ON our drawn ground (the pre-ONE_GRID behaviour): mud top = ground + mud depth, so tyres and feet
  stand on the felt ground under the mud.
- When a rut-field patch's floor changes, the sheet's ground under it must update. Find the hook or add one, next to
  GroundChanged.
- Keep the zone tile storage as it is for this stage (memory work is Stage 3).
- WB check:
  1. drive through a zone: the sheet look as before (press, ridges, bow wave, drag, slump, dirt edge);
  2. tyres sit in the mud, nothing floats;
  3. the felt rut still forms under the mud;
  4. craters / trenches in the zone still go to voxels, and the mud clears there;
  5. no script errors;
  6. 0 HOLE lines.
- Run the adversarial-reviewer skill (the old sheet had many switches - list every one still on and why).

## Stage 2 - the sync layer from rut ops + local prediction (the core)
Follow Tools/mud_sheet_sync_proto.py (class Client).

### Op fields the sheet needs
Kind 4 only; look-only:
- speed (ridge height H_SPEED), as 0.1 m/s;
- tyre half width (TyreHalf), or the radius in cm;
- a vehicle key (small int: the server's track slot);
- the server time in 0.1 s.
Pack them so old ops still parse and the J2 snapshot / disp hash are unchanged. Show me the encoding first. Option:
m_iPiece or m_vWp are unused for kind 4 - check that first. The op box / CarveFloor must ignore them.

### SYNC layer
Applied on every machine that renders (clients + WB host, never a DS), in op seq order, also during JIP replay:
- every op segment is stamped with Stamp (the op's speed / half / vehicle);
- then SLUMP_PER_OP 3 slump iterations over a FIXED box (the op box + reach, sorted fixed order). NOT the sheet's
  moving m_iBox, not on a timer.
- Pass rule: the sheet's PASS_GAP rule with the op's vehicle key + server time instead of the local clock.
- No clock, no camera, no local vehicle state may touch the SYNC layer.

### PREDICTION
Today's Wheels() stamps (every frame, every vehicle near the camera, the client's own view) go into a prediction
list, not into the sheet data:
- drawn = SYNC + the replayed prediction stamps;
- retire a prediction stamp once an applied op segment covers it (within ~0.3 m of an op segment);
- on each op arrival: rollback + replay;
- the handoff residual is morphed out (x 0.85 per frame, as the proto).
Slump animation:
- the SYNC slump per op is the lasting part;
- an optional display-only ease can animate it.

### Hash + log
- TEST_KIT: 'MUD sheet hash seq %1: %2' per kind-4 op (the cells it changed, cm-quantised, the m_iDispHash style).
- Extend Tools/compare_op_hashes.py to read 'sheet' lines too.

### Determinism traps
- Iterate maps in a fixed (sorted key) order.
- No float accumulation that depends on frame rate.
- Stamps happen in op order only.

### Run the adversarial-reviewer skill
Focus:
- every input of the SYNC layer is replicated;
- a JIP replay gives the same hashes;
- a DS does nothing;
- the prediction never writes into SYNC.

### WB check
1. Drive: the look is live at the tyre as in Stage 1.
2. No visible jump 1.5-2 m behind the tyre (the handoff), or describe it to me.
3. 'MUD sheet hash' lines print.
4. Leave and come back: same mud.
5. No script errors.

## Stage 3 - memory + cost: meshes near the camera only, sparse data
- Stored data only for TOUCHED tiles (untouched mud = the THICK constant, no storage).
- Meshes only for tiles within a radius of the camera (ask me; start 40 m).
- Drop far tiles' meshes, rebuild them on approach. Keep the data.
- Remove the zone pre-build at load.
- Remove or skip the stitch rules the assessment blamed for cost (TAPER_PAD 6 cells round every change, TaperField
  on every live build), where ONE_GRID off makes them unneeded. Keep the zone-edge taper.
- Log (TEST_KIT, 10 s): `MUD sheet: tiles touched N, meshes N, builds N, mean ms, worst ms, sync stamps N,
  replay stamps N`. PERF only - never judge it in WB.
- WB check: drive 300 m away and back: mud still there, same look, no seam at the radius edge.

## Stage 4 - zone edge, craters / trenches / tunnels
- With ONE_GRID off, check:
  - mud clears where a crater / trench / fill moves the ground (BlastKeep, MOVED, BLAST_CLEAR);
  - the sheet hides over voxel holes / tunnel mouths (DrawsAt / BARE_VOX rules);
  - the mud edge at the zone boundary.
- These come from a crater / trench / fill op, so they are the same on every client: the clear must run from the op
  (in op order), never from local timing.
- WB check: a crater, a trench and a tunnel mouth in a zone; a zone edge.

## Stage 5 (only if I said yes in Stage 0) - late joiners get the mud
- The J2 snapshot carries the touched sheet cells (mud depth + drag, cm, run-length like TileText).
- The joiner loads them after the rut tiles.
- SnapSelfTest compares them.
- ESTIMATE from the proto: 0.1-0.3 MB per km of tracks raw - measure the real size.

## Final MP / PERF (add to the plan's Final MP list; do not ask me for MP runs now)
- MP:
  - 'MUD sheet hash' lines, client A vs client B (and a WB host), via compare_op_hashes after a drive: 0 mismatched;
  - a late joiner: hashes equal (Stage 5) or no sheet on old ruts (no Stage 5);
  - robot >= 6 runs, sheet on vs off (the collider is unchanged: the snap count must not change);
  - the DS log has no sheet activity.
- PERF (uploaded mod, standalone client, DS -maxFPS 60):
  - 3-4 trucks in a mud zone, sheet on vs off: fps, frame times, the 'MUD sheet' line;
  - memory after a 2 km drive.

## How to work
- Checkpoint before each code stage (`mud_backups/checkpoint.ps1 save "name"`) and after I confirm it works.
- MUD_VoxelProbe.c, MUD_WorldGround.c, MUD_FinePatch.c, next-steps.md and the build plans are CRLF. Edit them with a
  byte-safe python script (rb / wb) from the scratchpad, then count the line endings.
- Check MUD_MudSheet.c's line endings before editing.
- Grep for the code before editing (line numbers go stale).
- Enforce traps:
  - no variable name declared twice in one function;
  - no ternary;
  - never name a variable "set";
  - static arrays <= 65536;
  - class <= 256 KB;
  - 80 KB stack per method (BuildTile was at ~69 KB);
  - member bool arrays give garbage (use int).
- Stop after each stage so I can compile and do the WB check. Then read the newest logs/*/console.log after the last
  'Compiling Game scripts took' line yourself.
- After each stage update:
  - docs/features/mud-sheet/README.md (status: un-parked as look-only);
  - the "Already built" table in docs/superpowers/plans/2026-10-16-next-steps.md;
  - the master list entry;
  - '## Now' in .remember/remember.md (under 25 lines).
- Never measure performance in Workbench. New ideas go to "Parked ideas".
