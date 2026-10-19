# Hybrid plan: mud sheet = look, server rut field = feel (2026-10-18)

User ask 2026-10-18: the mud sheet looks best (soft, flexible); voxel / rut-field ruts give real collision and the
feeling of a rut. Combine them so (1) the rut shows directly under the wheel, (2) no server/client collision desync
stutter, (3) the sheet probably has no collider. Fact check + plan. Nothing here is built yet. Checkpoint v194 = before.

## 1. What is already true (read from the code 2026-10-18, not tested unless marked)

- The sheet is a mud layer on the ground: THICK 0.15 m on the "data ground" (the rut-field floor, not the patch
  collider). A tyre pass presses it PRESS_STEP 0.05 thinner (GRADUAL); fully pressed = d 0 = the real ground and its
  rut show (MUD_MudSheet.c header, THICK, PRESS_STEP).
- The wheels stand on the hard ground under the mud, so they already look sunk THICK into it (DRAG_TEST comment).
- The synced layer (MUD_MudSync) is pressed ONLY from the server's rut ops (kind 4), in op order, on every drawing
  machine - so it already follows the same op stream the rut field cuts from. Own tyre stamps are a client guess
  (prediction) drawn at once, dropped when the op holding them arrives (SyncBegin / SyncReplay).
- When a rut op cuts the floor, the sheet re-reads its ground there and keeps its mud depth (FloorChanged / ReGround:
  "drawn = felt ground + mud, the rut deepens under the mud"). So sheet and rut field already stack the right way.
- BUT in the zones right now there is NO feel rut: TEMP RUT_SHEET_ONLY_TEST = true (MUD_VoxelProbe.c) - a rut op
  whose segments are all under the sheet cuts no floor. Off the zones the feel rut is cut RUT_BEHIND_GAP 1.5 m behind
  the rearmost wheel (no snaps), and the client look ruts (MUD_LookRuts) draw a guess on the patch mesh until it lands.

## 2. Fact check: should the sheet have a collider?

- Measured: the only sheet collider number is the 2026-10-15 WB perf A/B - COLLIDE off gave the same fps (38-48
  driving). No doc or log shows it FELT worse. "It was worse" = unverified.
- By design it cannot be MP safe: a sheet collider would be built from the drawn sheet, and the drawn sheet holds
  each client's own guesses (prediction stamps) - two machines would collide with different shapes = the same
  server/client desync stutter as cutting under the wheels. Only the synced layer is the same everywhere, and it
  only changes when ops land (same timing as the rut field).
- So: no sheet collider. The feel comes from the rut field (cut behind the truck) plus, optionally, mud drag read from
  the synced layer (section 3, H4). A WB "COLLIDE on" feel run is possible (SP only, says nothing about MP) - skip
  unless the user wants to feel it.

## 3. Plan (stages; each = one checkpoint before, one combined test session)

### H1. Feel rut back under the sheet (tiny, test first)
- RUT_SHEET_ONLY_TEST false (it is TEMP anyway). Under the sheet the feel op then goes at most every RUT_SHEET_MS
  750 ms (already in), cut 1.5 m behind the rearmost wheel - the same rule as off zone.
- TEMP log in ReGround: per rut op, the largest drop of the drawn mud surface (m) and how far behind the nearest wheel
  it happened. This tells whether you SEE the floor cut land behind the truck.
- Expected (inference): first pass, the tyre groove = 0.05 m press; then 1.5 m behind, the floor cut adds the rut
  depth under it = a visible "second drop" behind the truck. If the log + your eye say it is invisible, H2 is not needed.
- Check: DS, circles in the zone: feel (do you feel the rut on the 2nd lap?), snaps ('MUD corr ... mine 1'), fps.

### H2. Same cut rule in both: the sheet predicts the floor cut (only if H1 shows the second drop)
- Move the look-rut guess from the patch mesh INTO the sheet: at each own tyre stamp the sheet also lowers its ground
  by the rut cut the server will make (MUD_LookRuts.Cut already computes exactly this on the client - same depth
  formula, cap, RUT_R). Stored as a per-cell "look floor" offset, drawn at once under the front wheel.
- When the op lands, ReGround reads the real (now cut) floor and the offset is dropped in the same step -> the
  drawn surface does not move. Same CATCH / REPLACE / TTL rules as MUD_LookRuts (a guess no op takes goes after TTL).
- Prediction only, never in the synced layer (keeps J2 / hashes / late joiners as they are: ops stay floor-blind).
- Result: rut shows full depth under the wheel at once; the feel lands 1.5 m behind with no visible jump; nothing
  physical changes under any tyre while driving (no snaps).
- Cost: one float per drawn cell + a look in Stamp. Look ruts on the patch stay off under the sheet (SHEET_SKIP).

### H3. Sheet on all offroad -> look ruts retired
- The end goal (memory: mud sheet on all offroad). Once the sheet covers offroad, MUD_LookRuts has nothing left to
  draw and can be archived (it is ~90 % of the off-zone patch meshes, research sec 9). This is the big perf stage
  (zones -> all offroad, upload cost, stored tiles); its own plan, after H1/H2.

### H4. Optional: soft feel from the synced layer (no collider)
- MUD_MudSheet DRAG_TEST (off) already has MudRunner-style mud drag (submerged area, v^0.75, engine loss). Read from
  the SYNCED layer only (same on server and clients, ops in order) it would be safe in MP: the server sim and the
  client prediction see the same mud. Gives "it feels soft" on the first pass before any rut is cut.
- Risk: the synced layer lags ~1.5 m behind the tyre (op timing), so the drag reads mud a little behind -> read
  DRAG_LOOK ahead (fresh, not yet pressed mud = the same on all machines). Needs a DS snap check.

## 4. Order (user decides)
1. H1 (minutes of code + one DS run). 2. H2 only if H1 shows the jump. 3. H4 if the first pass feels too hard.
4. H3 = the all-offroad plan.

## 5. Not verified
- That the floor cut behind the truck is visible under the sheet (H1 measures it).
- That a sheet collider felt worse (no record either way).
- That synced-layer drag stays snap-free on a DS (H4).
