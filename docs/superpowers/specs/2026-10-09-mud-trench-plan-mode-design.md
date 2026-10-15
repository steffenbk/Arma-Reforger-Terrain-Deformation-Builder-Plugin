# Trench plan mode - design spec

Date 2026-10-09. Target Experimental 192624 (same as Plan 3). No git.
Replaces Plan 3 Task 7 (round shovel trenches). Builds on the world ground of Plan 3
(`MUD_WorldGroundEntity`, fine grid 0.25 m) and copies the plan-mode pattern of the CSK sandbag
builder (`addons/CSK/docs/specs/2026-10-04-sandbag-wall-builder-design.md` section 6). CSK is a
separate mod: copy the pattern, no dependency.

## 1. Goal and why

With a shovel, a player plans a trench point to point with a ghost preview, confirms it, then digs it
piece by piece. Cross section is a box: flat floor, steep walls (user sketch 2026-10-09 22:29).

Why: the Task 7 shovel digs round bowls (crater shape); a line of bowls is not a trench. Test
2026-10-09 22:16-22:24 proved 'Square off' can flatten one 1 m square but is slow and fiddly.

Decisions (user, 2026-10-09 22:34-23:05):
- Built by shovel work after confirm, like CSK. No supplies.
- Walls from the ground grid only (no wall lining mesh).
- Plan controls: points, curves, depth, width.
- Dug soil disappears (no banks).
- Trench cut into fixed ~1 m pieces; any piece can be dug first, each keeps its own progress.
- Plain 'Dig' stays for foxholes; 'Dig fast' and 'Square off' are removed.
- Approach A: one site entity per confirmed trench.

Future features, kept in mind but not built: soil banks (parapet in front, or parapet + parados
behind); wall lining mesh (planks / wattle) for truly vertical walls; supplies speed-up.

## 2. Ground limit (why walls are ~80 degrees)

The world ground holds one height per vertex on a 0.25 m grid (`MUD_FinePatch.m_aH`). The steepest
wall it can make drops over one grid step: 1.5 m over 0.25 m is about 80 degrees. A trench edge that
is not along world X / Z (diagonal or curved) shows 0.25 m stair steps in plan view. Accepted (user
picked ground-only walls).

## 3. Pieces of code

| Piece | Runs on | Job |
|---|---|---|
| `MUD_TrenchLayout` | everywhere | Pure math: plan in, list of pieces out; `IsInside(piece, x, z)`. Same input gives the same result on every machine. |
| `MUD_TrenchPlanner` | planning client only | Plan mode: aim marker, points, curve, depth, width, undo, confirm, leave; ghost slabs. Nothing is sent before confirm. |
| `MUD_TrenchSiteEntity` | server owns, replicated | One per confirmed trench: plan + layers dug per piece; ghost slabs for unfinished pieces; deletes itself when every piece is done. |
| `MUD_WorldGroundEntity.CarveTrench` | server (and clients after Task 9) | Lowers the verts of one piece to its current depth. |
| modded `SCR_PlayerController` | client -> server RPC | 'place trench' and 'dig piece'; server re-checks shovel in hand, distance, limits. |
| `MUD_Dig` helper (existing) | digging client | Helper over an unfinished piece offers 'Dig trench', elsewhere plain 'Dig'. |

Keys: own input context in this addon (`Configs/System/chimeraInputCommon.conf`, new
`MUD_TrenchPlanContext`), flags and priority copied from CSK's `CSK_WallPlanContext` (priority 46,
flags 0xa 0). Same keys as CSK: R toggle plan mode (shovel in hand), LMB point, RMB undo point, mouse
wheel depth, X width, V curve, F confirm. Known clash: CSK and this addon both use R with the shovel;
fine while they are separate test mods, pick another key before they ship together.

## 4. Plan and layout

Plan = up to 10 points (world XZ; Y from the ground), one bend per segment (sagitta in m, 0 =
straight; the curve math is copied from CSK `CSK_SandbagWallLayout`: `CurvatureOf`, max bend 0.98 of
the half chord), one width and one depth for the whole trench.

- Width: 0.5 / 1.0 / 1.5 / 2.0 m floor width (X cycles). Default 1.0. (Changed 2026-10-09 23:50 by user after stage 1: 0.75 only dug a 0.5 m floor on the 0.25 m grid.)
- Depth: 0.5 to 1.5 m in 0.25 m steps (wheel). Default 1.5.
- Segment length max 12 m (CSK value); so at most 9 segments x 12 pieces = 108 pieces.
- Pieces: the centre line (straight or arc) of the whole chain is cut into pieces of nominal 1 m;
  per segment the count is `max(1, round(length / 1.0))` and the length is stretched so they fit
  evenly. A piece = its span along the centre line [s0, s1] on one segment.
- `IsInside(piece, x, z)`: the point's closest position on that segment's centre line lies in
  [s0, s1] and its distance to the line is <= width / 2. Shared boundary: a point at exactly s1 of
  piece i belongs to piece i + 1 (half-open span), except the last piece of the chain, which is closed.
  At a corner, the wedge outside both segment boxes is filled by a disc of radius width / 2 around
  the corner point, part of the earlier segment's last piece (so corners have no undug notch).
- Ownership: a point inside more than one piece shape (corners, tight curves) belongs to the first
  such piece in chain order. Every point inside the trench belongs to exactly one piece.

## 5. Carving

Layers: one swing digs one 0.25 m layer of the aimed piece. Layers per piece = depth / 0.25 (2 to 6).
Target for a piece with L layers dug: `t = -0.25 * L`, relative to the undug ground (`m_aH` is already
relative to `m_aBase`, so the floor follows the slope).

`CarveTrench(site, piece)` on every vertex with `IsInside`:
- skip if `m_aLock[v] <= 0` or `VertexSoft(gx, gz) <= 0` (roads, buildings, kept terrain, asphalt,
  concrete - same rules as craters and Dig);
- `y = Math.Max(t * lock, m_aFloor[v])` (map floor / depth box caps; lock fade near roads);
- `if (y < m_aH[v]) SetH(gx, gz, y)` - only lowers: a deeper crater stays, a rut or bump is cut flat;
- then `EnsureRange` first (refuse the swing if false, as `Dig` does), `MarkDirty` after.

Verts outside every piece are not touched, so the wall is one grid step. Two neighbouring pieces at
the same layer count give a continuous floor (shared verts get the same target).

Red (skipped) piece: its middle point is over a kept cell (`m_Skin.IsHoleCell` false) or
`VertexSoft` 0 at the nearest vertex. Shown red while planning; can still confirm; red pieces start
as done (never dug).

## 6. Site entity

`MUD_TrenchSiteEntity` with RplComponent. Replicated: points (max 10 vectors), bends (float per
segment), width index, depth layers, and one byte per piece = layers dug (0..max; 255 = red/skipped).
Server only: changes progress. On every machine: rebuilds the layout from the plan, shows ghosts.

Static registry of live sites (like `MUD_DepthBoxEntity.s_aAll`) for `FindPiece(wp, out site, out
piece)`. Deletes itself when every piece is done (done pieces are already carved into the ground).

## 7. Ghost

Per unfinished piece, a see-through slab lying on the ground over the piece footprint (procedural
`MeshObject`, verts at ground height + 2 cm). Green = buildable, red = skipped (plan mode only), and
fainter as layers are dug. Same ghost in plan mode and on the site. Not built on a dedicated server
(render off); the site gets a bounds mesh (`MUD_BoundsMesh`) so it has real bounds there.
Materials: own green / red `.emat` in this addon, based on the shipped preview material; read the
shipped preview `.emat` from the current pak before writing ours (CSK note 2026-10-05: shipped
Preview.emat grey / PreviewWarning.emat yellow, both Additive, AlphaMul 0.3).

## 8. Shovel and network flow

1. Plan mode confirm (F): client sends the plan to the server once (`MUD_AskPlaceTrench`).
2. Server re-checks: shovel in hand, every point within 15 m of the player, limits (points, segment
   length, width / depth values); computes red pieces itself; spawns the site.
3. Digging: the existing dig helper sits 1 m in front of the player. If `FindPiece` finds an unfinished
   piece there, the helper action reads 'Dig trench', else plain 'Dig'. Holding loops swings as today
   (`MIN_GAP_MS` 1000 on the server).
4. Each swing: server finds site + piece at its own `DigPoint`, adds one layer, calls `CarveTrench`.
5. Clients: until Plan 3 Task 9 the ground is not synced (`rpl 0`), so single player is the test
   target. Task 9: clients carve from the replicated progress (the 'sync small numbers, not heights'
   idea). Task 10 (JIP): a joining client carves every piece's current layer count.

Plain 'Dig' fix: `ShovelSpot` checks the soldier's feet against the undug surface too (proven defect
2026-10-09 22:16: from the rim, feet 49.77 vs dug surface ~48.1 refused).

## 9. Build order and tests

Each stage ends with: user compiles and plays; Claude reads the log and records the result.

1. Layout + carve. `MUD_TrenchLayout`, `CarveTrench`, script self-test of the math (piece count and
   stretch per segment, arc length, inside / outside points at edges, ends and corners, half-open
   ownership), TEMP debug spawn of a fixed test trench (straight, one corner, one curve) next to a
   flagpole. Log `MUD trench: piece N verts M depth D`. Gate: seams mismatched 0; neighbouring pieces
   join flat; a piece across a road stays locked; every vertex inside the trench belongs to exactly
   one piece (self-test count).
2. Site + shovel. Site entity, progress, 'Dig trench' on the helper; remove 'Dig fast' / 'Square off'
   (`MUD_EDigMode`, prefab actions, `Square`, `m_fDigFastMul`, `m_fSquareHalf`); plain Dig rim fix.
   Log `MUD trench dig: site S piece N layer L / M`. Gate: dig lines = swings the user made; any
   piece can be dug first; a done piece stays done; site deletes itself at the end.
3. Plan mode + ghost. Keys, aim marker, points, curve, wheel, X, red / green, undo, confirm, leave;
   ghost slabs. Log the confirmed plan. Gate: what the ghost shows = what gets dug (confirmed plan
   vs dug verts).
4. Results doc + adversarial review; back to Plan 3 Tasks 8-11 (Task 9 adds trench site sync,
   Task 10 JIP for sites).

Open item (logged, not in scope): camera dips under the ground when jumping close to a dug wall
(user 2026-10-09 22:29).

## 10. Out of scope

Soil banks, supplies, wall lining, editing a confirmed trench (delete and re-plan), saving trenches
across mission restarts.

## As built (2026-10-10, user-approved in play, Exp 192624)

Changes made during play testing; these override the text above where they differ.
- Free dig square = one trench piece, 1 x 1 m (FREE_LENGTH 1.0, width idx 1, reach 0.75, server range 2.5 m).
- Free square snaps to the 0.25 m grid, 45 deg turn steps.
- Sloped walls ~60 deg (SLOPE_RUN 0.58) for free dig and trenches; a piece never slopes into a neighbour piece of the same trench.
- R on the shovel cycles off -> Dig -> Fill -> Plan -> off (hint each step); no ghost/helper when off, except over an unfinished site piece.
- Fill: one 0.25 m layer per swing, refused over a site piece and on old ground.
- LMB in plan mode needs Configs/System/chimeraInputCommon.conf.meta to carry the vanilla GUID {795184CF9AD764DB}, then a Workbench restart.
- TEMP: MUD_TrenchSiteComponent.DEBUG_INSTANT = true (F digs the whole trench). Set false before release.

## Later ideas (not scheduled)

- Preset layouts (user 2026-10-10): in plan mode, swap between ready-made trench shapes - a mortar pit first, later a bunker and others - placed and dug like a normal planned trench.
