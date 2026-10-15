# Trench plan mode - notes for writing the plan (handoff 2026-10-09 23:20)

The plan file `2026-10-09-mud-trench-plan-mode.md` is NOT written yet. These notes hold the design worked
out while reading code, so the next chat writes it without re-reading everything.
Spec: `docs/superpowers/specs/2026-10-09-mud-trench-plan-mode-design.md` (approved 23:08).

## Facts read from code (2026-10-09)

- `MUD_WorldGround.c` (LF): `ToFine(vector wp, out float fx, out float fz)` returns false off the map
  (line ~452); `At(gx, gz, out int v)` returns MUD_FinePatch or null (~462); `SetH` (~499); `BaseAt(fx, fz, out ok)`
  (~837); `VertexSoft(gx, gz)` (~961); `EnsureRange(gx0, gx1, gz0, gz1, wheel=false)` (~1438); `MarkDirty` (~1787);
  `IsProxy()` (~351); `m_bDebug` (178); `m_iDigs` (262). Patch fields: m_aBase (absolute y), m_aH, m_aLock, m_aFloor.
  Setup() ends with the "MUD world: ready" Print then `return true` (~410) - hook self-test + TEMP there.
- Dig-mode removal: diff vs `mud_backups/archive/scripts/MUD_WorldGround.c.2026-10-09-before-digmodes` =
  attrs m_fDigFastMul / m_fSquareHalf (lines 171-176), IsDebug(), DigAt mode param, Square(). Keep ShovelSpot and
  Dig(wp, step); DigAt(wp) calls Dig(wp, m_fDigStep). No .et/.layer sets those two attrs (grep 2026-10-09).
- Rim fix in ShovelSpot: accept if |feet - surf| <= 1.5 OR |feet - m_aBase[cv]| <= 1.5.
- `MUD_Dig.c` and `Prefabs/Mud/MudDigHelper.et`: restore from
  `mud_backups/archive/scripts/*.2026-10-09-before-digmodes` (single Dig action, no MUD_EDigMode), then add trench parts.
- Enforce: no ternary; same variable name twice in one function (even sibling blocks) = error, use x2 names;
  MeshObject.Create needs static arrays; UpdateIndices before UpdateVerts; keep the Resource ref.
- CSK copy sources: planner Start/Stop/Update/TraceAim/UpdateHint (`CSK_SandbagWallPlanner.c` 215-340, 680-812,
  1781-1815), MoveBend 1875, gadget R hook `CSK_BuildingGadgetTool.c`, controller `CSK_PlayerController.c` 1-60,
  site RplProp pattern `CSK_SandbagWallSiteComponent.c` 150-300 + OnDelete 939, curve math
  `CSK_SandbagWallLayout.c` 514-620, input conf `CSK/Configs/System/chimeraInputCommon.conf` (starts `ActionManager {`,
  context Priority 46, Flags 0xa 0), emat `CSK/Assets/Editor/CSK/CSK_GhostSite.emat` + .meta (EMATResourceClass PC,
  XBOX_ONE, XBOX_SERIES, PS4, PS5, HEADLESS).

## Design decisions (to ledger as rulings)

- Plan travels as int-centimetre text `"w,l;x,z,b;x,z,b;..."` (w = width index 0-3 -> 0.5/0.75/1.0/1.5, l = layers 2-6,
  b = bend of segment from this point, last point b = 0). Planner ghosts are built from Unpack(Pack()) so ghost = dug.
- Limits: 10 points, segment 0.5-12 m (+0.01 tolerance), bend <= 0.98 half chord, bends only on chords >= 3 m.
- Pieces per segment `max(1, round(arcLen / 1.0))`, stretched; half-open spans, last piece of chain closed; corner disc
  radius w/2 at the segment end belongs to that segment's last piece; owner = first piece in chain order. Piece AABB
  (5 samples, pad 0.05) for quick reject.
- Classes: MUD_TrenchPlan (Pack/Unpack/IsInt/Width), MUD_TrenchSeg (m_vMid, m_vRight, m_vFwd, m_fCurv, m_fLength,
  m_vEnd - no static-array members), MUD_TrenchPiece (seg, x0, x1, lastOfSeg), MUD_TrenchLayout (Build -> bool +
  m_sError, PieceAt, PieceBox, PieceMiddle, PieceEdge, Count), MUD_TrenchSelfTest (once, logs PASS/FAIL).
- CarveTrench(lay, piece, layers) static: -1 not ready / EnsureRange false (swing not counted), -2 nothing diggable
  (piece marked skipped), else verts moved. y = max(-0.25*layers*lock, floor), only lowers. Static SurfaceY(x, z, out y).
- Red piece = middle not over a hole cell (IsOver). Hard asphalt is found at the first swing (-2).
- Site = MUD_TrenchSiteComponent on `Prefabs/Mud/MudTrenchSite.et` (+ RplComponent), RplProp strings m_sPlan and
  m_sProgress (digit per piece, 9 = skipped), static registry, FindPiece(wp, out piece) for unfinished pieces,
  DigPiece(piece) server, deletes itself (CallLater + DeleteRplEntity) when all done. MAX_SITES 20. No bounds mesh
  (site is never traced) - deviation from spec 7.
- Server place check: shovel in hand, every point within 60 m of the player (spec said 15 m), 2 s gap.
- Ghost = one local GenericEntity per piece, 10 verts / 24 indices (5 sections), ground + 2 cm, emats green / red /
  "dug" (replaces "fainter"). Not on DS. Planner keeps placed ghost (rebuild on change) and live ghost (rebuild on aim move).
- Proposed GUID prefix `{4D55445452454E3x}` (grep for clashes first).
- Test spots: Task 2 TEMP carve `"1,6;211000,275200,0;211800,275200,0;211800,275800,150;212600,275800,0"` (23 pieces);
  Task 3 TEMP sites `"1,2;211000,274200,0;211300,274200,0"` (finish + delete test) and
  `"2,4;211000,274500,0;211800,274500,0;211800,275000,0"` (corner, any-piece-first test). Avoid depth box x 2130-2150 z 2718-2738.

## Planned tasks

1. Plan + Layout + SelfTest (self-test checks: straight 10 m = 10 pieces with edge points; stretch 10.4 m; corner
   (0,0)-(4,0)-(4,4): (4.2,-0.2) -> 3, (4,0.6) -> 4, (4.3,-0.3) -> -1, plus dense grid vs distance-to-polyline;
   curve bend 2 m over 10 m: arc 11.035, 11 pieces, (5,2) -> 5, (5,0) -> -1; pack round trip; garbage -> null;
   zero-length / 13 m / bend on 2 m chord -> Build false).
2. CarveTrench + SurfaceY + TEMP carve (retry while -1, max 30 tries) - user plays, Claude reads log.
3. Site + prefab + Dig trench (GetActionNameScript) + remove fast / square + rim fix + TEMP sites.
4. Input conf + planner + R hook + controller OnUpdate + place RPC + ghost + 3 emats (read shipped Preview.emat first);
   remove TEMP.
5. Results doc + adversarial review + ledger; back to Plan 3 Tasks 8-11.
