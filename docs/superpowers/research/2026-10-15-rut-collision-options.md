# Rut "collision" without (many) voxels + gradual sheet - research (2026-10-15)

Follows [2026-10-15-raised-ruts-ideas.md](2026-10-15-raised-ruts-ideas.md). Labels: USER, LOG (measured),
CODE (our script today), PROVEN-IDA (read in the exe), INFERENCE (not proven), PROBE (to test).

## Test result (user, 12:08-12:11, log logs_2026-10-15_11-54-08)
- USER: 4-5 passes on one line in a mud zone - the feel DOES grow with each pass. Confirms the cause: the sheet
  shows full depth at once, the voxel rut (what you feel) grows 8 cm per pass.
- LOG: 481 voxel rut ops, all "deepest 0.35 m"; go-live build avg 6.3 ms, worst frame 11 ms (one frame each).
- LOG: sheet PERF fps ~70 standing, 41-50 driving in the zone; sheet script cost up to 5.1 ms/frame driving.

## How the wheels "feel" ground (the facts every option rests on)
- Wheel contact = suspension RAYCAST on layer VehicleCast, which hits only Default, Static and Terrain
  (mud-gotchas 2026-10-13, measured from the gproj). The ray starts at the hub and uses the runtime radius
  (PROVEN-IDA, sinking-deep-dive). So anything on those layers under the wheel IS ground to the wheel.
- The sheet collider is on CharCollide|Foliage, which the wheel ray does not hit (CODE MudSheet COLL_LAYER) - why
  sheet ruts are not felt.
- In MP the vehicle is simulated on the server. Feel can only come from data every machine has the same: the
  voxel/rut ops (replicated + JIP). The sheet is client-only and its shape differs per machine.
- MudRunner itself has NO mud collider (PROVEN-IDA 2026-10-14-mudrunner-ida.md): wheels stand on hard ground,
  mud only adds forces (lift by depth, drag by submerged side area, spin resistance, paddle push).

## Options
| | Option | What you feel | MP | Cost | Status |
|---|---|---|---|---|---|
| 1 | **Voxel ruts (today)** | solid rut walls, suspension works | proven (v035, 0 corrections / 90 s) | 6.3 ms per op build (LOG) + surface-nets mesh + trimesh | shipped |
| 2 | **Invisible wall colliders** ("grow up" without voxels): per rut op, add thin collider-only strips (low-poly trimesh or boxes) along both sides of the track on a layer the wheel ray hits. Floor stays the real ground; walls sit INSIDE the 0.15 m sheet, so you never see them | climb-out walls, suspension moves; the truck does not sit lower | same op stream as voxel ruts (no new net data, JIP via op log), built behind the truck as RUT_BEHIND | INFERENCE: much cheaper - no render mesh, no surface nets, no brick seeding; trimesh 512 tris 0.06 ms (mud-gotchas) | PROBE |
| 3 | **Wheel radius sink**: server keeps a sink per wheel from the rut depth under it, every machine sets WheelSetRadiusState (vanilla flat-tyre pattern, replicated small state) | truck drops into ruts, tyre draws half buried; no side walls | vanilla pattern; snaps unknown | very cheap | PROBE 1 in sinking-deep-dive, never run (MUD_RadiusProbe archived, was ON=false) |
| 4 | **MudRunner forces** (lift + side drag from a depth field) | closest to MudRunner | riskiest: forces on the driver's prediction; low grip already gave vanilla snaps (mud-gotchas 2026-10-11); depth field would have to come from ops | medium | today SP-only (MudSheet.Drag returns in MP, CODE) |
| 5 | **Fewer voxel ops**: voxels only in zones, or only once a rut passes ~0.1 m; ops every 500 ms instead of 250 | as today but coarser | as today | less | easy tuning |

### Option 2 details (the main candidate)
- Shape per segment: two strips left / right of the track centre at ~HALF + BAND (sheet ridge place), height
  growing per pass up to ~THICK (0.15 m) so they stay inside the mud. Inner face steep, outer face sloped.
- One static body per 3 m tile with a single trimesh geom (rebuild the tile on change), like the sheet
  colliders. Strips from all ops in that tile merged.
- Layer: must be hit by VehicleCast (Default / Static / Terrain). Check: the sheet's own ground traces
  (GroundDown, Projectile) must NOT see the walls, or the mud sits on top of them. Pick the layer so
  Projectile misses it, or skip our bodies in the trace filter (NotMoving already filters). CHECK in gproj.
- Soldiers stand on anything VehicleCast hits (mud-gotchas) - fine, the walls are under the mud top that
  soldiers already stand on (CharCollide).
- Gives the user's "grow up" idea with zero ground editing in zones. Outside zones (no sheet to hide them)
  keep voxel ruts.

## Gradual sheet sinking (user: "now it starts on max")
CODE MudSheet.Stamp: PRESS sets `m_aD[v] = 0` under the tyre - the full 0.15 m layer is gone on the first pass.
Fix idea (client-only, no MP impact):
- New per-vertex "last pressed" time. A press only counts as a NEW pass if the cell was not pressed in the last
  ~1 s (so one truck = one pass, even with 6 wheels and 2-3 stamps per cell).
- Each pass: `m_aD -= PRESS_STEP` (e.g. 0.05 -> 3 passes to the hard ground), and the removed mud goes into the
  side ridges, so ridges grow pass by pass too.
- Option: PRESS_STEP from vehicle mass (heavier = deeper first pass), as the voxel cap.
- Best match: same pass rule as whatever gives the feel (option 1 step 0.08 or option 2 wall growth), so look =
  feel each pass.

## Suggested order (after the user picks)
1. Gradual sheet press (client-only, small, no MP risk).
2. SP probe for option 2: one zone, walls only, voxel ruts off inside zones; compare feel + fps with today.
3. If good: MP run (DS + 1 peer), count snaps vs v035.
4. Option 3 radius probe later for "truck sits lower".
