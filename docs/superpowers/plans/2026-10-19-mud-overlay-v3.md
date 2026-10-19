# Mud overlay v3, option A: the sheet's look on top of the untouched voxel ruts (plan, 2026-10-19)

User 2026-10-19: the voxel ruts have colliders, so they FEEL right, but they do not LOOK as good as the old mud sheet.
v3 brings the sheet's look back as a client-only overlay, with the ruts untouched. Option A (user pick): each client
makes the mud from what it sees plus the replicated rut data - no sync layer, no hook in the rut code.
Picture: https://claude.ai/artifact/HSke4Ht8XYb19D9ioZ5nm8 (private page).
Second design to try after this one: plans/2026-10-19-pressed-lift-ruts.md (touches the rut drawing, behind its own
switch). The two never run together.

## Hard rule
No change in the rut ops, rut field, wear, look ruts, MUD_VoxelProbe Submit / op format, the patch meshes or colliders,
MUD_CoarseSkin, MUD_MudSync. The overlay only READS: `MUD_VoxelProbe.FieldFloor / FieldFloorAt / WearAt / TopAt /
ColumnAt`, `MUD_WorldGroundEntity.DugUndug`, vehicle wheel contacts (`VehicleWheeledSimulation.WheelGetContactPosition`
as MUD_MudSheet.Wheels does). MUD_MudSheet.ENABLED stays false; its hooks stay off.

## What made the sheet look good (research/2026-10-16-soft-mud-alternatives.md sec 2) - all of it comes back
Tyre-width press with a soft wall (BOX, HALF_PER_R 0.38 x radius, SOFT_WALL 0.35), side ridges + bow wave (BAND 0.32,
H0 0.05 + H_SPEED x speed, HMAX 0.28, NOSE / FRONT_LEN), gradual press per pass (PRESS_STEP 0.05, W_FIRST 0.55),
texture drag (DRAG, DRAG_MAX, LimitDrag), touched cells in a dirt material, 0.15 m cells.
Not coming back: the 0.15 m layer over untouched ground (that is what hid the ruts in the lift test).

## The height rule (per overlay vertex)
The overlay keeps the sheet's mud field d per cell (THICK = untouched, < THICK pressed, > THICK pushed up), from the
sheet's own Stamp maths (copy MUD_MudSheet.Stamp / LimitDrag into the new file; do not call MUD_MudSheet).
```
undug, dug = DugUndug(x, z)            // TopAt on voxel columns
h = undug + (d - THICK)                // the sheet's surface, measured from the original ground
h = max(h, dug)                        // never below the felt floor: a pressed groove lands ON the felt rut floor
draw the vertex if h > dug + EPS_DRAW (5 mm) or a neighbour in its quad does; quads with all four corners at the felt
floor are skipped -> the patch draws the rut floor itself (grass-free, dirt as today)
```
What this gives: untouched ground d = THICK -> h = ground -> no quads (nothing drawn, nothing streamed). Inside the
1 m voxel rut, outside the tyre's pressed width, d stays near THICK -> h = original ground > felt floor -> the overlay
fills the wide rut back up into soft walls; under the tyre d -> 0 -> the groove sits on the felt floor; ridges rise
above the ground and start at 0 at their outer edge (no step against untouched ground).
Pass 1 (wear only, no felt cut): the groove has nowhere to go (dug = undug) -> skipped; the ridges show; pressed cells
get a 5 mm dirt film (TOP_LOOK-style dirt section) so the track reads by colour.
Craters / trenches / voxel columns: where dug is more than MOVED 0.08 below undug and no stamp explains it, or a voxel
column carries the cell, d = 0 -> skipped (no mud in holes). Rebuild touched tiles in the frame the hole appears
(lesson from research/2026-10-19-whole-map-sheet-issues.md sec 2-3: no waiting for upload slots, rebuild the taper
neighbours too).

## Where d comes from (option A)
1. Live stamps: every client stamps the wheels of every vehicle it sees (MUD_Wheels.GetAll, as MUD_MudSheet.Wheels):
   full sheet look while you watch.
2. Re-centre: once the rut floor has a cut under a stamp (the felt rut lands ~1.5 m behind the truck), shift the
   stamp sideways onto the deepest floor sample within 0.3 m - fixes the ~3 cm network-smoothing offset.
3. Fallback for ground nobody here watched (far away, late joiners): synthesize stamps along the rut field's centre
   lines (deepest floor samples linked into 0.5 m segments), tyre half from the rut width, speed unknown -> H0 ridges.
   Same places as for a watcher, finer detail approximated.
Different players can see the mud a few cm different; the felt rut is the same for all. Look only.

## Tiles and cost
- 3 m tiles at 0.15 m (21 x 21), only where stamps or rut-field ruts / wear exist; built near the camera, kept (memory
  cap), dropped far behind. No streaming over fresh ground.
- Rebuild: wheel tiles at most every 0.1 s and never held (sheet lesson: holding wheel tiles makes a "wave"); other
  touched tiles at most every 0.25 s; fallback tiles once on approach. Poll the rut floor per tile near the camera every
  0.25-0.5 s for changes (no hook).
- Cost (inference from the sheet's numbers): stamps ~0.1-0.46 ms/frame (sheet 'sh wheels'), tile builds ~0.55-0.9 ms
  each near the wheels; no sync layer, nothing on the DS.

## Risks to check in the look session
- Grass on the raised mud (ridges, soft walls, film): grass rides on our ground above the heightmap and may ride on the
  overlay too. First check.
- Groove alignment with the felt rut; z-fight between overlay and patch at the groove edge (EPS_DRAW).
- Pop when a tile first appears; crater / trench clean; late joiner sees mud along every rut.

## Stage
Switch `MUD_MudOverlay.ENABLED` (new file MUD_MudOverlay.c), runtime toggle for the look session.
Build prompt: prompts/2026-10-19-local-mud-two-designs.md (part 1).
