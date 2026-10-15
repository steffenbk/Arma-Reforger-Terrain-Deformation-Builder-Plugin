# Ground ideas to test (user list 2026-10-10 10:41)

Not scheduled. Starts after Plan 3 (Task 10 re-runs, Task 11 baked heights) unless the user says otherwise.

**Ranking now lives in [2026-10-10-master-list.md](2026-10-10-master-list.md) (reviewed 17:5x).**

**PICKED (user 2026-10-10 15:41), in this order:**
- A. Ruts dig TO a target depth (Valheim, solves 7) - skip cells already at/below target, cap depth.
  ALREADY IN CODE (review 17:5x, re-checked: Press l.678-695 caps at depth x soft, skips verts at/below it).
- B. Depth per ground type (3).
- C. Side ridges (10). ALREADY IN CODE as the berm (l.149-156, 697-711) - tuning only.
- D. Send "crater type + pos" instead of heights (Valheim + 1) - test only; measure bandwidth under heavy
  shelling and keep hash compare to catch server/client drift.

## Valheim findings (ILSpy of assembly_valheim.dll, 2026-10-10 ~13:14, read from shipped code)
Classes TerrainComp, TerrainOp, TerrainModifier, Heightmap (decompiled copies were in that session's scratchpad
valheim_src/, temp). Per tile: "level" delta (cap +-8 m) + "smooth" delta (cap +-1 m), result clamped to 8 m of
original. Client sends only "tool id + position"; tile owner computes heights. Whole tile saved per edit
(changed-flag per point + 2 floats, zipped) on its net object; others reload on version change; JIP = normal save.
Dig = set target level at tool pos minus edge falloff, SKIP points already below target -> re-hitting a spot does
not dig deeper. Not copied: owner-only compute (late for others), 4-byte floats (we use 2-byte mm).
Height changes stay as they are now (user: important). Sources: GHPC + MudRunner look 2026-10-10.

1. **Stamps** - crater shape from a small hand-made depth table (rim lip, ejecta, uneven floor), 3-5 per size class,
   random turn, ADDED to current heights (stacks, crosses patch borders). MP: maybe send "stamp id, pos, angle,
   strength" instead of heights - measure bandwidth under heavy shelling first; keep hash compare.
   First step: read our current crater code to see what shape it makes now.
2. **Split cheap from real (maybe)** - thin cosmetic tracks without height/net cost, real heights for deep ruts,
   craters, trenches. Test only; unverified whether Enfusion lets a mod feed its own texture into the terrain shader.
3. **Depth per ground type** - dig depth from the gamemat under the wheel/hit (WORLD trace gives it, proven
   2026-10-08). Soft field/mud deep, grass less, dirt road a bit, asphalt/rock/concrete none
   (MudRunner does the same: IsExtrudable=false, WheelTracks=0).
4. **Depth by explosive charge** - crater size/depth scales with the shell's charge. PAK Stable 2026-10-10: 13 ammo
   prefabs make craters today, all the same 2 m pit (40 mm grenades, 8 rocket types, 2 mortars; hand grenades none).
   Ranked #1 in performance-overview section 5.
5. **Dug ground looks dug** - dark wet soil / scorched look on deep cells. Needs a material blend test first.
6. **Dust and dirt chunks** - particles + small debris when ground changes. MudRunner spray formula
   (scripts/mud.lua): amount ~ wheel slip x mud depth, little or none when the wheel is deep in water.
7. **Track ruts merge** - repeated passes deepen one lane up to a cap instead of adding noise.
8. **Crater merge** - two overlapping craters become one bigger pit; today a second crater spawns inside the first.
   User 17:36: "they just overlap". CODE (MUD_WorldGround.c `Crater` l.2635-2647, read 2026-10-10): the bowl only
   lowers (`y < h`) so a second hit in the same spot changes nothing, but the RIM is `if (rim > h) SetH(rim)` - the
   new crater's rim ring (2.5-3.75 m out) REFILLS any dug ground it crosses up to +0.3 m. Shell #2 landing 1-3 m from
   #1 puts a raised ridge through #1's bowl -> two cut circles, not one pit.
   Fix options, cheapest first:
   - M1 rim only on undug ground (`h >= -0.05`), never fill a hole. ~1 line.
   - M2 stack: a hit whose centre is already dug carves from the CURRENT surface (deeper), capped at a max total
     depth (e.g. 1.5 x one crater) and the floor -> repeated shelling deepens one pit.
   - M3 widen: centre inside an existing crater -> radius x ~1.3 -> the two bowls join into one wider pit.
   - M4 one smoothing pass over the stamped area (we already blur for the collider) to remove the seam where bowls meet.
   Dagor/Enlisted does this with combine modes per stamp (min / max / add, see session notes 12:05). Do M1 + M2 +
   item 16 (lip) in one pass - same function. Old copy in MUD_GroundEntity.c `Crater` (l.1782) only if it stays.

9. **Slippery deep ruts** (user 10:42) - deep rut cells get a muddy low-grip gamemat (per-geom gamemat is reported
   by WheelGetContactMaterial, proven 2026-10-08 15:13). Many passes -> slippery trench.
10. **Side ridges** (user 10:42) - wheel dig raises the cells beside the rut a bit, not only lowers.
11. **Wheel-spin mud spray** (user 10:42) - effects only, MudRunner formula (see 6).
12. **Tread prints per tire type** (user 10:42, low priority) - a small stamp repeated along the rut.
13. **Smaller JIP full copies: skip flat runs** (user 12:14, from Enlisted/Dagor source read 2026-10-10) -
    today PackFull ([MUD_WorldGround.c](../../../Scripts/Game/Mud/MUD_WorldGround.c) PackFull/ApplyFull) sends all
    33x33 = 1089 verts as mm = 545 ints (~2.2 KB) per patch, even for a thin rut. Dagor TerraformComponent::serialize
    sends only non-zero cells as runs {alt:8, indexDelta:16 compressed, count:8} per patch.
    First step: count non-zero verts per deformed patch from a real server log/session (is the saving worth it?).
    If yes: runs of non-zero mm values; client MUST zero the whole patch before applying runs (else old dents
    survive - Dagor seems to have that hole). Keep mm (16-bit), NOT Dagor's 8-bit: 0.5 cm error breaks hash compare.
    Live rect messages stay as they are (better than Dagor's resend-whole-layer for constant ruts).
    Source: github.com/GaijinEntertainment/DagorEngine prog/gameLibs/terraform/terraformComponent.cpp,
    prog/gameLibs/ecs/terraform/terraform.cpp.
14. **Crater/foxhole walls should block blasts** (user 15:21, seen in play) - a player down in a crater or
    trench still takes full explosion damage; blasts seem to pass through the dug ground. Cause unproven.
    Probe first: (a) does vanilla blast damage trace for cover at all (flat-ground control: hide behind a
    vanilla rock/wall vs open ground)? (b) if it does, does that trace hit our deformed-ground mesh (layer/
    collision mask) or the holed terrain? Fix depends on which: add our mesh to the layer the blast trace uses,
    or script our own cover check on explosion damage.
    IDA + PAK 2026-10-10: cover = GenericTerrainEntity OR gamemat StopExplosionTrace (default 1); all ground gamemats
    resolve to 1 -> material is NOT the cause. Details: 2026-10-10-performance-overview.md P3.

15. **TEST: explosions on asphalt / hard ground** (user 17:36, built in Task 6, never play-tested on purpose).
    What the code does (CODE 2026-10-10): `m_sHardWords` default "asphalt,concrete" -> those cells get no ruts and
    no shovel, but a crater still digs them; a hard cell that moved > 3 cm turns into `m_CraterMaterial` (dirt) and
    takes ruts from then on (`BreakHard`). A crater whose CENTRE is on kept terrain (roads not holed) is skipped
    (log 'crater skipped (kept cell)'). Test list: (a) mortar on a holed asphalt / concrete cell -> crater + dirt
    surface + ruts after; (b) mortar on a kept road -> no crater, log line; (c) mortar beside a road -> edge lock
    fades it, no step into the road; (d) cobblestone / stone / tiles_stone / gravel are NOT in the hard words (pak
    gamemats) -> they take ruts like dirt today - decide if "cobblestone,stone,tiles" join the list.
16. **Crater lip too steep** (mega dive 2026-10-10, N2 in performance-overview.md): top ring 54-70 deg vs soldier
    MaxSlope 50 deg default; the bowl ends at 0 and the rim starts at +0.3 (0.68 m jump in 0.25 m). Fix: one
    continuous profile, walls <= ~45 deg. Same function as 8 and 4.

Also raised, not picked yet: mud on vehicles. Later idea: preset trench layouts (mortar pit, bunker).

17. **Crash craters** (user 2026-10-10 21:38): a helicopter that falls, or a vehicle that hits the ground hard and
   explodes, leaves a small crater around it. Hook ideas (unchecked): vehicle destroyed event / impact speed at the
   wreck; reuse Crater() with a fixed small TNT and skip the "hit a vehicle" guard for this caller.

## Scored list (2026-10-10 17:4x) - Value 1-5 (PvP), Cost 1 = cheap (an hour) .. 5 = hard (days, unproven engine support)
Opt = also saves server / client / net work. Score = 2 x Value - Cost (+1 if Opt). Sorted by score.

| Score | # | Idea | Value | Cost | Opt | Why |
|---|---|---|---|---|---|---|
| 9 | 4 | Crater size by charge (40 mm small, rockets mid, mortars big) | 5 | 2 | yes | No mortar-size UGL foxholes; fewer patches + net per shot |
| 8 | 14 | Blasts through crater walls - probe, then fix | 5 | 2 (probe) | - | Cover is the point of craters in PvP; fix cost unknown until probed |
| 7 | 16 | Crater lip fix (walls <= 45 deg) | 4 | 1 | - | Players / vehicles not trapped |
| 6 | 8 | Crater merge (M1 rim + M2 stack; M3/M4 later) | 4 | 2 | - | Shelled areas look like one pit, not ridged circles |
| 5 | 15 | Test explosions on asphalt / hard ground | 3 | 1 | - | Built but never checked on purpose |
| 5 | 7 / A | Ruts dig TO a depth (lanes merge) | 3 | 2 | yes | No rebuilds once a lane is done |
| 4 | 3 / B | Depth per ground type | 3 | 2 | - | Gamemat already traced (proven 2026-10-08) |
| 3 | 9 | Slippery deep ruts (muddy gamemat) | 3 | 3 | - | MAT_SLOTS = 4 per patch limits it |
| 3 | 1 | Crater stamps (hand-made shapes) | 3 | 3 | - | Looks; pairs with D |
| 2 | 10 / C | Side ridges | 2 | 2 | - | Looks |
| 2 | 6 | Dust + dirt chunks when ground changes | 2 | 2 | - | Looks, small cost |
| 2 | 11 | Wheel-spin mud spray | 2 | 2 | - | Looks; same effect code as 6 |
| 2 | - | Preset trench layouts | 2 | 2 | - | Convenience |
| 2 | 5 | Dug ground looks dug (dark soil) | 3 | 4 | - | Needs a material blend test first |
| 2 | 2 | Split cheap tracks from real ruts | 3 | 5 | yes | Big saving IF the terrain shader takes our texture - unproven |
| 1 | - | Mud on vehicles | 2 | 3 | - | Looks |
| 1 | D | Crater as "type + pos" on the net | 2 | 4 | yes | Only if F4 / stress test shows net trouble |
| 0 | 13 | JIP copies skip flat runs | 1 | 3 | yes | 400 KB already joins fine |
| -1 | 12 | Tread prints per tire | 1 | 3 | - | Low |

Speed-only items (lazy patches O1, client ahead gate, healing, send by distance...) are scored in
2026-10-10-performance-overview.md section 5.

## MudRunner findings (IDA + game files, 2026-10-10 10:45-11:00, MudRunner.exe as installed)

READ FROM FILES (Media.zip/media.xml):
| Substance | SubstanceFriction | LinResistance | PushResistance | FullPushDepth |
|---|---|---|---|---|
| Mud   | 2.2 | 10.0 | 36.0 | 1.2 |
| Water | 0.4 | 1.0  | 1.0  | 2.0 |
Loader: sub_1408F0AD0 (MediaData/Mud/Substance) -> sub_140932DF0 reads exactly these 4 floats. Where the physics
uses them (combineSubstanceWheel / combineSubstanceBody Havok actions) NOT read yet.
Tire templates (_templates/trucks.xml): BodyFriction/SubstanceFriction Offroad 1.0/1.2, Highway 1.5/0.5,
Default 1.25/0.75; softness RadiusOffset 0.02-0.03, SoftForceScale 0.12-0.32. Terrain types: IsExtrudable=false
(concrete, rock_slope, asphalt overlays), WheelTracks=0 (concrete, asphalt).

READ FROM EXE: mud is its own map. Strings 'isMud' per block, 'extrudeMap'/'userExtrudeMap' per block, GPU
effect SpinTires/MudCompute with passes UpdateMudmap, RestoreMudmap, PushMud, ConvertExtrudes (driver
sub_1408F0C20 draws wheel triangles into the mud map), 'ReturnMudmap' (sub_140908210, not read).
Shaders: ShaderCache.sdc = zlib at offset 12 -> fx_2_0 effects; D3DCompiler_47 D3DDisassemble reads the ps_3_0
blobs. Mud map is 8-bit (constants 1/255, 2/255).
 - MudCompute shader 00: texkill where mud value <= wheel height -> marks cells the wheel is inside the mud.
 - MudCompute shader 05: new = cur + min(factor, target - cur, 2/255) -> mud near the wheel RISES toward a target,
   at most 2/255 of the mud range per pass.
INFERENCE (not proven): wheel presses mud down to its bottom, the pushed mud raises the cells around the footprint
a little per update -> ridges grow over several frames. Physics reads the mud map back from the GPU (ReturnMudmap).
For us: same rule in script for idea 10 (side ridges): lower cells under the wheel, add a capped share to a ring
around it. Unchecked: whether a script can add drag force to a Reforger vehicle (for LinResistance-style drag).
