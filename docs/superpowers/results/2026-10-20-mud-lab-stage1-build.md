# Mud lab stage 1 - build report (parts A-E, before the WB check)

Prompt: `docs/superpowers/prompts/2026-10-20-local-mud-lab-stage1.md`. Branch `claude/upbeat-lovelace-q0w022`.
NOT COMPILED yet (Claude cannot compile; the user reloads Workbench).

## Commits and checkpoints

| Part | Commit | Checkpoint before it |
|---|---|---|
| A switcher | 58e687d | (earlier session) |
| B sheet local + deposit band | 7d428ae | before mud lab B |
| C cost levers | f231a44 | before mud lab C |
| D far data + mesh radius | 699c795 | v252 before mud lab D |
| E remote trucks (sync-lite) | 312832b | v253 before mud lab E |
| adversarial review fixes | a519463 | - |

## Files and methods per part

- **A** - `MUD_MudLab.c` (new): numpad 3 cycle off -> sheet local -> D2 -> D1 -> off, 10 s lab line.
- **B** - `MUD_MudSheet.c`: runtime statics ENABLED / SYNC, LabSet, DropAllLab, LabCounts(I), SlumpMsAll, BandMsAll,
  BandMargin, Birth, BandMark, TouchStore, TouchAt, BandTile, BandStored, UploadWhy, CopyLastY, FreezeLocal,
  ZoneAtCellAny, BuildNowSlots, LocalLog; local branches in HasTile, TileInZone, UpdateSlots, LazyOn, MarkRect,
  RadSpeed, DropMesh, BuildZoneTile, MudAt, BlastKept, ZoneAtWorld, Grounds, BuildTile, Log10, DigNow, DoDigNow,
  GroundChanged, QueueBuild, FlushOneBuild, Freeze, Frame, Stamp, Upload, UploadPick, Follow, Pack, Unpack, FillSlot,
  BuildTileMesh (band height, skip band-0 quads, 'would skip' meter, TOP_UV64). `MUD_MudZone.c`: SheetCovers needs
  ENABLED && SYNC. `MUD_MudLab.c`: MUD_LabSheetLocal.
- **C** - `MUD_MudSheet.c`: LEV_L1_CARRY / L2_DRAG / L3_SLUMP / L6_EASE / L8_VEH (all false), LeverApply, LabLeverStep,
  LeverNames, AnySlotDirty, StoreCurrent, LimitDragCells, SlumpCells, WheelsNear, Wheels split into Wheels + WheelsSim,
  class MUD_MudSheetCells; branches in Follow, UpdateSlots, Stamp, Upload, Slump, Grow, Frame, UploadPick, PickWheelSlots.
- **D** - `MUD_MudSheet.c`: TILE_DATA_CAP 20000, MESH_R 70, MESH_DROP 10, MESH_MS 2, FAR_R 40, FAR_H 0.3; class
  MUD_MudSheetFar; TrailGet, FarPut, FarTake, FarMud, FarLog; branches in TouchStore, TouchAt, BandMark, BandStored,
  Freeze, Radius, LodUpgrade, RadSpeed, QueueNear, DropFar, DropMesh, FillSlot, BuildZoneTile, BlastNow, MudAt,
  BuildTile, LabCountsI, DropAllLab, LocalLog.
- **E** - `MUD_MudSheet.c`: OP_SPEED 5, OP_RADIUS 0.5, OP_VEH -1, COVER_R 0.3, OWN_S 20, OWN_PRUNE_S 5, OP_EDGE_M 1.5,
  BW_OP 10 (BW_N 11); OpStamps, OpClear, OpStampsI, OpInWindow, OwnMark, OwnNear, OwnKey, OpStampStore, OpCell,
  OpTile, OpSet, OpGet, OpLimitDrag, OpGround, OpLog; BandMark split into BandMark + BandMarkWorld (same maths);
  OwnMark call in WheelsSim; 'op %5' in LocalLog. `MUD_MudSync.c`: RutOp / ClearOp route to the sheet when
  ENABLED && !SYNC (not on a DS). `MUD_MudLab.c`: OpLog appended to the sheet local 10 s line.

## Coupling gates in the rut-side files (none of these files was edited)

- MUD_CoarseSkin 617 / 644 / 749 / 752: LOOK_LIFT_TEST (const false) / PRESS_LIFT.
- MUD_FinePatch 59 / 60 / 62: comments only.
- MUD_LookRuts 47 / 236 / 368 / 512 / 613: SheetCovers (now needs SYNC); LookChanged 532 / 600 only via the shX lists.
- MUD_VoxelProbe 1430 RutUnderSheet and 3396 op tag: ENABLED && SYNC; 1433 / 3971 / 3980 / 8445: SheetCovers; MudSync
  calls 1717 / 2015 / 2016 / 2276 / 2348 / 2383 / 2436 / 2559 / 4068 / 4170 / 4217 / 4220: OnHere (needs SYNC) or
  s_Inst null - except RutOp / ClearOp, which now route inside MUD_MudSync in sheet local; 3090 / 4081 / 4083 / 6558:
  ONE_GRID const false; 4086 Holds: m_aPend empty; 4212 FloorChanged: sheet-side re-ground; 6750 / 6751
  GroundChanged / FlushNow: holes (every op on this machine).
- MUD_WorldGround 1084 / 1085 ticks; 4758 LazyOn (needs SYNC); 6864 / 6935 PatchLiftOn (needs SYNC) / LOOK_LIFT_TEST.
- With the sheet off (ENABLED false), every new path returns before it does anything.

## Not done as written, and why

1. **ClearOp is only counted, not routed.** Every crater / dig op on this machine already reaches the sheet at its
   go-live (MUD_VoxelProbe.GoLive -> MUD_MudSheet.GroundChanged -> QueueChange / BlastNow). Routing ClearOp too would
   clear each crater twice. Lab line: 'clear ops <n> (taken at go-live)'.
2. **MESH_CAP is ignored in sheet local**: every mesh past MESH_R + 10 m goes (the prompt's "deleted at once").
3. **Band in the compact data**: the class byte keeps bare, touched AND the deposit band (5 bits, 32 steps), so a
   compact tile needs no band pass to come back.
4. **Numpad 0 lever cycle** added (TEST_KIT, sheet local only) so DS rows need no recompiles.
5. Op stamps outside the window do no slump (the old sync layer slumped its op box). Far ruts may look sharper.

## Known gaps / risks

- Band-born tiles have no TileCovered filter (no cut-off step), so mud can spill up to BAND_IN + BAND_FADE onto
  non-holed tiles.
- 64 m top UVs clamp oddly on a tile that straddles a 64 m chunk side.
- Grass on the band is still open.
- FillSlot / Unpack can overwrite shared side cells of kept slots (pre-existing).
- With L8 toggled, m_aVeh can hold a duplicate entry per vehicle (harmless). Under L2, drag is limited once per 0.1 s.
- Remote op stamps far away: each op brings its tiles back from compact, and DropFar compacts them again 0.25 s later
  (expand / compact churn, plus m_aFarOrder.Find over up to 20000 keys). Watch the 'op stamps ... ms' part on the DS.
- JIP: the op history replays every old rut op into OpStamps at join - possibly one long frame.
- Own-truck double stamp: own ops come back from the server after the wheels stamped (OwnMark), so they should be
  skipped. If 'op stamps' counts climb while only your own truck drives, the cover test misses them.

## Adversarial review (2026-10-20, parts B-E)

- Fixed: OpCell / OpSet took negative world cells into tile 0 at a negative index (crash near the map side).
- Fixed: OwnNear passed Math.Round floats straight into an int key.
- Checked, no change needed: no map is changed inside its own foreach (DropFar walks m_aTrailOrder by index;
  BandStored's queue is not touched by FarTake); the weak caches m_TchSt / m_TchFar / m_OpD are reset wherever their
  object can go; locals per new method stay under 64 (OpStampStore ~42, OpCell ~34); no new `%` in float maths.
- Not tested: there is no script test harness - the WB check below is the test.

## Every hunk touched since part A (git diff -U0 58e687d..HEAD, old -> new line, enclosing scope)

#### MUD_MudSheet.c

- old 29 -> new 29,3 | class MUD_MudSheet
- old 34 -> new 36 | class MUD_MudSheet
- old 275,0 -> new 278,107 | class MUD_MudSheet
- old 638,0 -> new 748,2 | class MUD_MudSheet
- old 718,0 -> new 830 | class MUD_MudSheet
- old 745,0 -> new 858,5 | class MUD_MudSheet
- old 757,0 -> new 875 | class MUD_MudSheet
- old 799 -> new 917,4 | class MUD_MudSheet
- old 800,0 -> new 922 | class MUD_MudSheet
- old 876 -> new 998 | class MUD_MudSheet
- old 878,2 -> new 1000,9 | class MUD_MudSheet
- old 894,0 -> new 1024 | class MUD_MudSheet
- old 895,0 -> new 1026 | class MUD_MudSheet
- old 896,0 -> new 1028 | class MUD_MudSheet
- old 1051,0 -> new 1184,3 | class MUD_MudSheet
- old 1057,0 -> new 1193,3 | class MUD_MudSheet
- old 1072,0 -> new 1211,3 | class MUD_MudSheet
- old 1093,2 -> new 1234,1432 | class MUD_MudSheet
- old 1096,5 -> new 2667 | class MUD_MudSheet
- old 1102 -> new 2669,26 | class MUD_MudSheet
- old 1104,14 -> new 2696,8 | class MUD_MudSheet
- old 1119,6 -> new 2705,6 | class MUD_MudSheet
- old 1125,0 -> new 2712,2 | class MUD_MudSheet
- old 1128,14 -> new 2716,14 | class MUD_MudSheet
- old 1143 -> new 2731,28 | class MUD_MudSheet
- old 1145,31 -> new 2760,4 | class MUD_MudSheet
- old 1177 -> new 2765,30 | class MUD_MudSheet
- old 1181,8 -> new 2798,11 | class MUD_MudSheet
- old 1190,2 -> new 2810,2 | class MUD_MudSheet
- old 1193,3 -> new 2813,16 | class MUD_MudSheet
- old 1226 -> new 2859 | class MUD_MudSheet
- old 1238,0 -> new 2872,2 | class MUD_MudSheet
- old 1241,0 -> new 2877,12 | class MUD_MudSheet
- old 1243 -> new 2890 | class MUD_MudSheet
- old 1244,0 -> new 2892 | class MUD_MudSheet
- old 1276,0 -> new 2925,3 | class MUD_MudSheet
- old 1295,0 -> new 2947,3 | class MUD_MudSheet
- old 1303,0 -> new 2958,2 | class MUD_MudSheet
- old 1381,0 -> new 3038,2 | class MUD_MudSheet
- old 1407,0 -> new 3066,2 | class MUD_MudSheet
- old 1408,0 -> new 3069 | class MUD_MudSheet
- old 1459 -> new 3120 | class MUD_MudSheet
- old 1554 -> new 3215,7 | class MUD_MudSheet
- old 1653 -> new 3320,4 | class MUD_MudSheet
- old 1751,0 -> new 3422,7 | class MUD_MudSheet
- old 1782,2 -> new 3459,2 | class MUD_MudSheet
- old 1790 -> new 3467 | class MUD_MudSheet
- old 1798 -> new 3475 | class MUD_MudSheet
- old 1824 -> new 3501,3 | class MUD_MudSheet
- old 1833,0 -> new 3513,2 | class MUD_MudSheet
- old 1848,3 -> new 3528,0 | class MUD_MudSheet
- old 1853,0 -> new 3532,7 | class MUD_MudSheet
- old 1860 -> new 3545,2 | class MUD_MudSheet
- old 1865 -> new 3551 | class MUD_MudSheet
- old 1886,0 -> new 3573,13 | class MUD_MudSheet
- old 1901,2 -> new 3600,2 | class MUD_MudSheet
- old 1927 -> new 3626 | class MUD_MudSheet
- old 1961,0 -> new 3661,8 | class MUD_MudSheet
- old 1979 -> new 3686 | class MUD_MudSheet
- old 2006,2 -> new 3713,2 | class MUD_MudSheet
- old 2030,0 -> new 3738,2 | class MUD_MudSheet
- old 2070,0 -> new 3780 | class MUD_MudSheet
- old 2071,0 -> new 3782 | class MUD_MudSheet
- old 2673,0 -> new 4385,10 | class MUD_MudSheet
- old 3279,0 -> new 5001 | class MUD_MudSheet
- old 3280,0 -> new 5003,3 | class MUD_MudSheet
- old 3388,2 -> new 5113,2 | class MUD_MudSheet
- old 3451,0 -> new 5177,3 | class MUD_MudSheet
- old 3455,0 -> new 5184,7 | class MUD_MudSheet
- old 3465,0 -> new 5201 | class MUD_MudSheet
- old 3517 -> new 5253 | class MUD_MudSheet
- old 3575,0 -> new 5312,5 | class MUD_MudSheet
- old 3585 -> new 5326 | class MUD_MudSheet
- old 3890,0 -> new 5632,16 | class MUD_MudSheet
- old 4085,0 -> new 5843,7 | class MUD_MudSheet
- old 4112,0 -> new 5877,2 | class MUD_MudSheet
- old 4718,0 -> new 6485 | class MUD_MudSheet
- old 4719,0 -> new 6487 | class MUD_MudSheet
- old 4767,0 -> new 6536,8 | class MUD_MudSheet
- old 4867 -> new 6643 | class MUD_MudSheet
- old 4869,0 -> new 6646,5 | class MUD_MudSheet
- old 4928,0 -> new 6710,6 | class MUD_MudSheet
- old 4948,0 -> new 6736,2 | class MUD_MudSheet
- old 4962,0 -> new 6752,5 | class MUD_MudSheet
- old 5081 -> new 6875 | class MUD_MudSheet
- old 5105 -> new 6899 | class MUD_MudSheet
- old 5217,0 -> new 7012,2 | class MUD_MudSheet
- old 5245,0 -> new 7042,6 | class MUD_MudSheet
- old 5259,0 -> new 7062,5 | class MUD_MudSheet
- old 5260,0 -> new 7068 | class MUD_MudSheet
- old 5261,0 -> new 7070,3 | class MUD_MudSheet
- old 5296,0 -> new 7108,2 | class MUD_MudSheet
- old 5308,0 -> new 7122 | class MUD_MudSheet
- old 5311,0 -> new 7126 | class MUD_MudSheet
- old 5312,0 -> new 7128,2 | class MUD_MudSheet
- old 5319,0 -> new 7137,5 | class MUD_MudSheet
- old 5324,0 -> new 7147 | class MUD_MudSheet
- old 5325,0 -> new 7149,2 | class MUD_MudSheet
- old 5338,0 -> new 7164,5 | class MUD_MudSheet
- old 5351,0 -> new 7182 | class MUD_MudSheet
- old 5366 -> new 7197,4 | class MUD_MudSheet
- old 5387,0 -> new 7222 | class MUD_MudSheet
- old 5400 -> new 7235,4 | class MUD_MudSheet
- old 5404 -> new 7242,5 | class MUD_MudSheet
- old 5682,2 -> new 7524,2 | class MUD_MudSheet
- old 5708,0 -> new 7551,2 | class MUD_MudSheet
- old 5709,0 -> new 7554 | class MUD_MudSheet
- old 5714 -> new 7559 | class MUD_MudSheet
- old 5719,0 -> new 7565,7 | class MUD_MudSheet
- old 5727,0 -> new 7580,2 | class MUD_MudSheet
- old 5729,0 -> new 7584,3 | class MUD_MudSheet
- old 5899,0 -> new 7757,8 | class MUD_MudSheet
- old 5904,0 -> new 7770,15 | class MUD_MudSheet
- old 5972,0 -> new 7853,2 | class MUD_MudSheet
- old 6118,0 -> new 8001,2 | class MUD_MudSheet
- old 6421,0 -> new 8306,19 | class MUD_MudSheet
- old 6426,0 -> new 8330,5 | class MUD_MudSheet
- old 6549,0 -> new 8458,47 | class MUD_MudSheetEase
- old 6559,0 -> new 8515 | class MUD_MudSheetVeh
- old 6590,0 -> new 8547,3 | class MUD_MudSheetTileData
- old 6595,0 -> new 8555,9 | class MUD_MudSheetTileData
- old 6615 -> new 8583 | class MUD_MudSheetTile
- old 6623,0 -> new 8592,3 | class MUD_MudSheetField

#### MUD_MudLab.c

- old 187 -> new 187,2 | class MUD_LabMode
- old 189,0 -> new 191,6 | class MUD_LabSheetLocal : MUD_LabMode
- old 191 -> new 198,42 | class MUD_LabSheetLocal : MUD_LabMode

#### MUD_MudSync.c

- old 220,0 -> new 221,7 | class MUD_MudSync
- old 252,0 -> new 260,7 | class MUD_MudSync

#### MUD_MudZone.c

- old 64,2 -> new 64,2 | class MUD_MudZoneEntity : GenericEntity
