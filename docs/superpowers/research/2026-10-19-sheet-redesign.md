# Whole-map mud sheet: redesign or replace (research, 2026-10-19)

Answer to `prompts/2026-10-19-research-sheet-redesign.md`. Research only; no code changed. Builds on
research/2026-10-19-whole-map-sheet-issues.md. Labels: PROVEN-CODE, MEASURED (log / offline tool), [repo], INFERENCE.

> **TESTED 2026-10-19 (DS, user eyes + research/2026-10-19-ruts-invisible.md): grass covers any lifted rut.** With the
> sec 6 look lift (0.16 m on skin + patches) the ruts vanished: a 4-6 cm rut floor drawn 0.16 m up is still ~0.11 m above
> the heightmap, and grass clutter grows on our ground wherever it sits >= 1 mm above the heightmap (mud-gotchas). Sec 5
> had this backwards: grass does not sit under a lifted ground, it rides on top of it - including every rut, crater
> floor or pressed area that stays above the heightmap. Any lift route (sheet, umbrella, patch mud channel) must press
> the lift out where the ground is cut (ruts-invisible fix 2), or ruts stop showing. LOOK_LIFT_TEST is off again.

## 0. Short answer
- **Idea A already exists: it is the coarse skin.** `MUD_CoarseSkinEntity` fills every hole cell of Arland with the
  exact 2 m heightmap triangles, in 64 m chunks, built once at load (MEASURED 2026-10-09: 1 396 378 hole cells in
  7.6 s, of which meshes 0.4 s, +262 MB incl. colliders [repo gotchas]). It already does the "build the detail first,
  then cut the piece out" rule: a fine patch covers skin cells with `CELL_COVER`, the skin mesh drops them as
  zero-area triangles in its index list, "only in the last step, after the collider exists, and in the same frame as
  the mesh: never a hole, never two surfaces showing" (PROVEN-CODE, MUD_CoarseSkin.c header + MUD_WorldGround
  BuildStep). Lifted 0.15 m on rendering machines it is the umbrella, with exact heights, no new mesh system.
- **Idea B (mud seen far away) is best done in the patch meshes, not in sheet tiles:** give the patch mesh's lift a
  per-vertex value from the synced mud layer (lift = LIFT + mud depth d) instead of a constant. Rutted patches are
  already kept for the match [repo soft-mud alternatives], so driven ground keeps its mud look at any distance with no
  extra meshes. MEASURED offline (this doc, sec 3): the 0.25 m patch grid draws the sheet's mud at 2.1-2.3 cm rms, the
  ridge kept, grooves 10-14 % shallower - good for mid / far, not under the wheels.
- **Recommended route: "one ground, three detail levels, one cut-out rule"** - lifted skin (far, static) -> lifted
  patches with the mud channel (mid, kept) -> sheet tiles only near the camera / vehicles (0.15 m, texture drag, dirt
  material). Every level already exists and two of the three hand-offs already work; the new parts are the lifts and
  the mud channel.
- **The biggest risk is not cost, it is the 0.15 m visual layer everywhere offroad**: grass rides on top of any ground
  above the heightmap, so ruts shallower than the lift keep their grass (seen in the 2026-10-19 look lift test);
  props / rocks / trees may sit at collider height, and a prone soldier lies on the collider while
  the drawn ground is 0.16 m higher - in PvP that is visual-only cover (bullets hit what you cannot see). The sheet
  had the same problem; making it whole-map makes it everyone's problem. That is a design decision before any build.

---

## 1. What the existing ground already gives (PROVEN-CODE / MEASURED)
| level | mesh | built | heights | covers / cut-out | far view |
|---|---|---|---|---|---|
| coarse skin | 64 m chunks, 2 m cells, 33 x 33 verts, one per chunk with holes (1697 chunks) | once at load (7.6 s total, meshes 0.4 s) | exact heightmap triangles + 1 mm | cells hidden by `CELL_COVER` (patch) / `CELL_VOX` (voxel tile): zero-area tris, mesh rebuilt; patch swap ~1.6 ms per free (gotchas 2026-10-09) | the whole map |
| fine patch | 8 m, 0.25 m cells, 33 x 33 verts, mesh + collider | ahead of vehicles / camera; rutted ones kept (asleep keeps the mesh) | heightmap + rut field (ruts drawn) | covers skin cells after its collider + mesh exist; hidden under a sheet with LAZY_RUT_MESH; A2 lift (SPARSE) with LIFT_BY_DRAWN | kept patches show ruts anywhere |
| sheet tile | 3 m, 0.15 m cells, ~700-1000 verts, 2 materials, no collider | radius streaming (today) | data ground + THICK + LIFT - press | patch unlifted only under drawn tiles (LIFT_BY_DRAWN) | none (dropped past the radius) |
Arland holed ground 5.60 km2 (last hole write) [repo].
One script mesh section holds at most 65 535 vertices (u16 indices and counts, gotchas 2026-10-14), and a script mesh
has one LOD (IDA 2026-10-18): one "huge" umbrella mesh is impossible (the whole skin is 1.1 M verts) and would never be
culled; 64 m chunks are already the right size (1089 verts, culled per chunk).

## 2. The options (ranked)
| # | option | how it works | memory | build cost | per-frame cost | pop-in risk | far view | determinism risk | measure first |
|---|---|---|---|---|---|---|---|---|---|
| 1 | **One ground, three levels** (recommended) | skin hole-cell verts lifted LIFT + THICK on clients (mesh only); patch mesh lift = LIFT + d(x, z) from MUD_MudSync (d = THICK where untouched); sheet tiles only within ~20-30 m of the camera / wheels, patch unlifted under drawn tiles (existing LIFT_BY_DRAWN); lift fades to 0 at kept terrain (m_aLock exists on patches) and next to voxel cells | no new meshes; sheet tiles only near (~50-150) | skin lift: inside the 0.4 s load mesh pass; patch: a sync read per vertex on rebuild (look-only rebuild when a sync tile changes) | 0 for skin / kept patches; sheet near only | skin <-> patch swap already invisible (same heights); patch <-> sheet = resample error (2-2.3 cm rms, sec 3) at the chosen hand-off distance | yes, wherever patches are kept (driven ground) | none (reads the synced layer; drawing is client-only) | WB look test sec 5 (grass / props / prone / seams) |
| 2 | SPARSE as built + the step fixed | sheet tiles only on touched ground; untouched = patch lifted constant 0.15 | as today | as today | as today | the driven / undriven step (DS 16:26) | none past the radius | none | the step (sec 4) |
| 3 | Soft layer in patch / skin only (no sheet) | option 1 without sheet tiles | least | patch rebuilds only | least | none from the sheet | yes | none | look: 0.25 m mud vs 0.15 m sheet; loses texture drag + per-quad dirt |
| 4 | Idea A as a new umbrella mesh | a separate lifted low-detail mesh over offroad | +1 mesh set (~ skin size) | extra load pass | 0 | its own cut-outs to add | yes | none | not worth it - option 1 reuses the skin |
| 5 | Premade rut pieces / decal-like prefabs on top of 1 or 3 | static .xob pieces from op segments (research 15.2) | entity per piece, shared mesh | ~0 | 0 (static) | none | yes (prefab view distance / LOD Factors) | none | look in WB (research v2 C3) |
| 6 | World.CreateDecal on settled patches | decal clipped to the target mesh; gone when the mesh is rebuilt (IDA, inference) | engine | ~0 | ~0 | n/a | engine decal distance | none | look by eye (returns null even on success) |
| 7 | Terrain material / gamemat | no runtime terrain paint or height API (gotchas / IDA); MatTerrainObject `MaterialBlendUseTerrainDistance` blends by distance to the heightmap - with a lifted ground it might tint where drawn ground is far from the heightmap; try 3 never ran | 0 | 0 | 0 | n/a | yes | none | finish try 3 (custom-shaders doc) - a tint at most, no shape |
| 8 | Full-map per-tile sheet streaming (today, SPARSE false) | radius builds | - | ~622 600 tiles if kept | 0.6-1.9 ms/frame radius builds (WB) | high (test) | none | none | dead (issues doc 1.1) |
Not new in the engine: nothing in the API dump / IDA notes we have not tried that deforms or paints ground at
runtime (TrackDecal terrain-only, DecalEntity hangs under holes, no heightmap / hole API, skinned meshes cost per drawn
frame - research 15.8).

## 3. MEASURED offline: mud drawn by the patch grid (option 1 / 3 hand-off)
Tool: the settled reference field of `Tools/mud_sheet_cost_exp.py` (game maths, 0.15 m), mud depth sampled at patch
vertices (bilinear) and drawn back with the patch's triangle interpolation; window 8 x 5 m, three scenarios.
| scenario | patch grid | rms m | max m | groove depth (ref) | ridge (ref) | width at 50 % (ref) | shape dist |
|---|---|---|---|---|---|---|---|
| base | 0.25 | 0.021 | 0.110 | 0.331 (0.387) | 0.072 (0.072) | 0.38 (0.32) | 0.058 |
| base | 0.50 | 0.037 | 0.174 | 0.288 (0.387) | 0.074 (0.072) | 0.44 (0.32) | 0.099 |
| 5 passes | 0.25 | 0.023 | 0.108 | 0.400 (0.444) | 0.079 (0.080) | 0.39 (0.37) | 0.051 |
| straight | 0.25 | 0.023 | 0.118 | 0.315 (0.367) | 0.068 (0.068) | 0.38 (0.32) | 0.033 |
Reading: at 0.25 m the ridge is kept, the groove is 10-14 % shallower and ~6 cm wider, worst cells ~0.11 m at groove
walls. That is the pop when a near sheet tile hands over to the patch: put the hand-off where a 2 cm rms change does
not show (my guess: 25-30 m; the user's eye decides). 0.5 m (far LOD patches) is too coarse for grooves.

## 4. The SPARSE driven / undriven step (option 2, and the patch <-> sheet seam in option 1)
Cause still unproven. What the code makes likely (INFERENCE): two different height functions meet at a tile side -
the sheet's untouched top is its own data ground (even cells traced / HoleGround, odd cells the mean of neighbours) +
THICK + LIFT at 0.15 m, while the lifted patch is the patch's own heights + (THICK + LIFT - LIFT_UNDER 0.01) at 0.25 m
- a built-in 1 cm offset plus grid / sampling differences of a few cm. Fix rule: one owner of the border height - the
sheet's side vertices next to a non-drawn tile take the patch's lifted height at that x, z (same function the patch
mesh uses), and with option 1 both read the same mud depth d. Log: per drawn tile, max |sheet side vertex - lifted
patch height| on the 'MUD sheet:' line.

## 5. The risk that decides it: a 0.15 m visual layer over all offroad ground
All INFERENCE until seen; each is a look / gameplay check, not a cost:
- Grass clutter: CORRECTED 2026-10-19 (research/2026-10-19-ruts-invisible.md) - grass does not get buried; it rides
  on top of our ground once that is >= 1 mm above the heightmap (gotchas 2026-10-08). The real effect: grass also
  covers every rut / pressed area that stays above the heightmap, so a lift must be pressed out where the ground is
  cut, or ruts stop showing (MEASURED in the look lift test, 2026-10-19 22:53).
- Props, rocks, trees on holed ground are placed by MUD_PinLift - if they sit at collider height they look 0.16 m sunk.
- Prone soldiers lie on the collider: their lower ~0.16 m is under the drawn ground - visible to nobody, still hit by
  bullets. In PvP that is cover that only works for the eye. The zone sheet had it in 64 m; whole-map makes it
  everywhere offroad.
- Voxel holes (craters, trenches) cut into lifted ground: the lift must fade to 0 next to voxel cells (skin: the cells
  next to `CELL_VOX`; patch: a factor like m_aLock), or every rim gets the lip from issues doc 3.
If these are not acceptable, the lift must shrink (a few cm for untouched ground, ridges still rise) - then option 3 /
5 (mud depth + pieces) carry the look and the sheet's "soft layer" becomes ridges and a groove only.

## 6. Recommended route and the smallest test
Route: option 1, with option 5 pieces for tread / wet detail, and the lift size decided by the sec 5 look test.
Smallest test that proves or kills it (one WB look session, ENABLED sheet not needed):
1. A TEMP switch that lifts only the coarse skin mesh's hole-cell vertices by 0.16 m (clients, mesh only, colliders
   untouched) and the patch meshes by the same constant (the A2 path with SPARSE logic off).
2. Look, from 2 m / 20 m / 100 m: grass, a few holed props / rocks / trees, a road edge, a building edge, the
   patch / skin seam, a fresh crater rim, a prone soldier (second client or a placed character).
3. Kill it if grass or props look buried or the prone soldier is unacceptable; otherwise repeat with the patch lift
   = LIFT + d from the sync layer on one driven track and judge the 0.25 m mud from 10-40 m.
Nothing in it needs perf numbers; perf comes later on the DS (patch look rebuilds per rut op).
