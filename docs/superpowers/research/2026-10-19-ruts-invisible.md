# Ruts cut in the data but nothing shows (research, 2026-10-19)

Answer to `prompts/2026-10-19-research-ruts-invisible.md` (snapshot 44335db). Research only. Lines are
`Scripts/Game/Mud/MUD_WorldGround.c` unless named. Labels: PROVEN (gotchas / user), PROVEN-CODE, MEASURED (logs in
the prompt), INFERENCE.

## 1. Cause: the look lift puts the rut floor back above the heightmap, so grass grows in it again

The chain:
1. PROVEN (mud-gotchas 2026-10-08 21:50-21:59, user screenshots): grass clutter shows on our ground over a terrain hole
   only where that ground sits at least 1 mm above the heightmap; "at +0.3 m the clutter rides on top of our tile";
   mortar craters are clean (no grass in the pit).
   So until today a rut on grass was visible mainly as a grass-free stripe: its floor is below the heightmap.
2. PROVEN-CODE: the TEMP look lift adds `LOOK_LIFT_M 0.16 x p.m_aLock[v]` to every patch vertex (LiftAt 6643-6646),
   in both mesh paths after the rut floor is applied (BuildMesh 6797-6799, BandMeshIn 7465-7467), and the coarse skin's
   hole cells get the same 0.16 m (MUD_CoarseSkin.LookLift 619-642).
3. MEASURED: the rut floor was 4.4-6.4 cm deep (client trail probe 22:54:30-31). Drawn rut floor = heightmap - 0.05 +
   0.16 = ~0.11 m ABOVE the heightmap -> grass rides on top of the rut like everywhere else. A rut would need to be
   deeper than the lift (16 cm) before grass goes.
4. PROVEN-CODE: the dirt layer does not help at that depth: soil shows only from FIELD_SOIL_D 0.06 deep plus the
   ragged noise (rfS = rfF + FIELD_SOIL_D - m_fRevealDepth, 6780 / 7448; RevealT 6721-6732). 4.4-6.4 cm = at or below
   the start -> little or no dirt.
Result: a 5 cm dent under a full grass cover and no dirt = "no rut, no dent, no dirt". The data, the server cut and the
mesh rebuilds are all fine (logs: 288 cut, look meshes 25, bench differ 0).

What it is not (checked):
- The lift is added consistently: top surface, dirt layer (m_aUnder = h - gap after the lift, 6802; band Aux from the
  lifted Y, 7333) and the reveal uses depth m_aD (not absolute heights) - so no "drawn vs data" comparison breaks.
  MESH_SKIP_SMALL compares lifted vs lifted mesh Y (7644-), and at worst delays a build by MESH_SMALL_MAX_S 0.5 s.
- p.m_aLock does not fill the rut: it is 1 on open holed ground and fades to 0 only within Edge Fade (attribute default
  1 m) of kept terrain (EdgeLock 3111-3140) and near buildings. Near the runway edge both the lift and the rut press
  (m_aEdgeLock) shrink together.
- Rebuild 5: COL_MERGE is collider only (the drawn mesh never merges, 7211); the fast mesher bench compares the fast and
  the v240 mesher on the same input and found 0 differences in 94 full passes - it cannot hide what the v240 mesher drew.
- Wear: client look ruts skip worn-only samples on purpose ('worn only 644', MUD_LookRuts 462-464), mirroring the
  server; the server's 288 cut samples reached the patches (trail probe depth).

## 2. Tyres sunk "less than 16 cm"
INFERENCE: it is 16 cm, it just does not read as 16 cm. The wheel stands on the collider (rut floor); the drawn ground
is 0.16 m higher (in the rut too), and the grass now rides on that drawn ground and hides the tyre's lower edge as it
did before. On a ~1.1 m Ural tyre 16 cm is ~15 % of its height. One clean check: park with one wheel on the runway
concrete edge (lift 0) and one on grass 2 m in (lift 0.16) and compare in one screenshot.

## 3. Fix options (look only, client)
| # | fix | effect | note |
|---|---|---|---|
| 1 | Lift off (LOOK_LIFT_TEST false) | ruts as before (grass-free floor, dirt from 6 cm) | the redesign route's "0.15 m layer everywhere" loses this rut cue - see sec 4 |
| 2 | Lift pressed out by ruts: lift x (1 - clamp(cut / LIFT_M)) per vertex, i.e. a rutted vertex drops to ground + floor | a rut shows as a 16 cm + cut groove, grass-free, tyres not sunk in it | this is the mud-layer look (pressed to hard ground) without a sheet; needs a smooth edge over ~0.3 m |
| 3 | Small lift (0.02 m) | rut floors deeper than 2 cm drop below the heightmap again -> grass-free | keeps most of the old look |
| 4 | FIELD_SOIL_D lower (e.g. 0.04) | dirt shows earlier | independent of the lift |

## 4. What this changes in the redesign (research/2026-10-19-sheet-redesign.md sec 5)
My sec 5 said grass would sit partly under a lifted ground. Wrong: the gotcha says clutter rides on top once our ground
is above the heightmap. The real effect is the reverse: with a lift, grass also covers every rut, crater floor or
pressed area that stays above the heightmap. Any lift-based route (sheet, umbrella, patch mud channel) has to press the
lift out where the ground is cut (fix 2), or ruts stop showing. The prone-soldier and prop-height risks stand.

## 5. One control session (WB is fine: this is a look check, no ms)
The switches are `static const`, so a single launch needs one small TEMP change: make the lift a static float
(`s_fLookLift`, start 0.16) with a TEST_KIT key that cycles 0.16 -> 0.02 -> 0 -> 0.16 and marks every live patch mesh
dirty + rebuilds the skin chunk meshes in view. Then, one session:
1. Drive one line 3-4 times on grass until the trail probe says >= 0.06 m deep (read 'depth ahead ... deepest').
2. Lift 0.16: grass in the rut? (expected yes, invisible rut).
3. Lift 0.02: grass gone from the rut floor? (expected yes).
4. Lift 0: rut as on v234 (grass-free + dirt starting).
5. Only if step 4 still shows nothing: rebuild 5 is next - relaunch with MESHER_FAST false (then COL_MERGE false).
Log line to add for the session (client, every 10 s): 'MUD rut look: rutted patch verts N, drawn below heightmap N,
lift now L' - drawn below heightmap = d + lift < 0, i.e. where grass will be gone. It predicts what the eye sees.
Without the TEMP toggle: two launches (LOOK_LIFT_TEST false, then LOOK_LIFT_M 0.02), steps 1-4 in each.
