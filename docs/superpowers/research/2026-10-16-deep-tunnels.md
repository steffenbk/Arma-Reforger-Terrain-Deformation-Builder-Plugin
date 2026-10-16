# Deep tunnels: what stops them today, what to change (2026-10-16)

User 2026-10-16: "What if we want deep tunnels? I did test and 3 m was not far enough really."
Status: PLAN ONLY. Nothing in Scripts/ changed. This replaces U1's fixed "FLOOR 6" with a deep-tunnel package.
**DECIDED (user 2026-10-16): TUNNEL_FLOOR default 5 m for now** ("we don't know how it will behave at 10"). Go to
10 m only after the section 5 checks pass at 5 m. Everything below is written so 5 -> 10 is one value.
Labels: VERIFIED-CODE (git f7715c3 line numbers), MEASURED (offline, Tools/), ESTIMATE, UNVERIFIED (needs the game),
SOURCE.

## 1. What stops a tunnel at 3 m today (all VERIFIED-CODE)
| # | Where | What it does at 3 m |
|---|---|---|
| a | MUD_VoxelProbe.c:304 `FLOOR = 3` | one constant for every op kind |
| b | CarveTunnel :3261, :3272-3273; Carve :2692, :2714-2715 | `if (floorY - y > d) d = floorY - y`: nothing below undug - 3 can be air. It also turns air BACK into solid below the floor (review D2) |
| c | ColSurf :4925 `j0 = undug - FLOOR - 1` | the "top of the ground" search stops 4 m down. An open shaft deeper than that has no surface, so DIG NOW / SurfaceAt users get "no ground" |
| d | MUD_WorldGround.c:4276-4291 AtGround | on voxel cells a soldier counts as "on the ground" only within FLOOR + 0.25 m of it. Below that, the shovel (F) and trench digs are refused (MUD_Dig.c:466, :509). Tunnels (T) do not use it (MUD_VoxelNet.c:51-65) |
| e | CarveFloor :2987, FreeDig :3342, FillTrench :3404 | ruts / trenches / fills use FLOOR too. These keep 3 m; they are not tunnels |
Nothing else caps depth:
- Brick rows are made lazily per 4 m row (Put :1454-1465, keyed by row number, no limit).
- Solid rows between the surface and a deep tunnel are never made (Put skips unchanged solid samples).
- MAX_COLUMNS counts 2D columns, so depth does not use it up.

## 2. What deep tunnels cost
- **Data / bricks (ESTIMATE from the row maths):**
  - A tunnel column needs its surface rows (top +- 0.5 m) plus the rows the capsule touches (feet - 0.8 to
    feet + 2.7 m).
  - With rows 4 m tall: feet at 3 m = ~2 rows per column (they share a row), 6 m = ~3, 10 m = ~4.
  - So a 10 m tunnel costs up to ~2x the bricks of a 3 m one per metre of length.
  - The network cost does not change: the same op per click, the same JIP text.
- **The surface above still goes voxel.** MarkCarved works per 2 m skin cell (VERIFIED-CODE Put :1477-1481), so the
  terrain over any tunnel, at any depth, is holed and drawn by bricks. The same as today.

## 3. New problems that only appear when deep
| # | Problem | Status | Answer |
|---|---|---|---|
| P1 | **Sea level.** Holed terrain + a tunnel floor below the ocean height (`world.GetOceanBaseHeight()`, used in MUD_VanillaHoleFix.c:130-143). Does the ocean plane show inside, does the soldier swim? | UNVERIFIED (no source found; search 2026-10-16 found nothing) | Guard in the op: floor = max(undug - TUNNEL_FLOOR, ocean + 1 m). It is one constant per map, so it is the same on every machine. Test: dig down near the coast. |
| P2 | **Climbing back out.** At full look-down a click digs ~0.7 m lower and 1.1 m ahead. The dug ramp is ~42-55 deg (ESTIMATE: dig geometry + the 0.5 m-step sim, Tools/plan_review_checks.py trace shows 0.72 m drop per 0.5 m). The character's max slope is ~50 deg (IDA 2026-10-15). Fine for 3 m (a few steps), a trap at 10 m. | ESTIMATE; needs the game | Cap the dig slope at ~35 deg (clamp pitch so drop per click <= 0.7 x walk), or a "steps" mode. Test: dig down 6 m at full pitch and walk back up. |
| P3 | **The FLOOR clamp refills deep tunnels.** A crater (FLOOR 3) inside a tunnel deeper than 3 m turns its air back to solid (MEASURED: 81 of 218 samples, Tools/plan_review_checks.py refill). | MEASURED | All clamps become "no NEW air below the floor": `d = max(d, min(old, floorY - y))` (Carve, the C4 sphere, CarveTunnel, CutTrench, FreeDig). |
| P4 | **Shafts as traps / fall damage.** Looking straight down digs in place: ~4 m3 per click every 300 ms, no tool check (MUD_VoxelNet.c:51-65). | VERIFIED-CODE | T needs a shovel + a slower rate (decision 3). Optional: no click deeper than 1 m below the feet. |
| P5 | **Thin vs thick roofs.** Deep = thick roof: a mortar crater (3 m) can no longer break in. That is realistic (Cu Chi, WWI dugouts: SOURCE in the improvement plan U1). A tunnel just under the surface still gets the 0.45-0.87 m roof (MEASURED). | MEASURED | U3 roof guard (refuse a click that leaves < ROOF_MIN, e.g. 1.0 m, of roof) now works, because a player can go deeper instead. |
| P6 | **Bullets.** The voxel colliders are Terrain|Foliage only, with no FireGeo slab (VERIFIED-CODE MakeBody). If rounds pass voxel surfaces, they pass the ground above a tunnel AND its roof, at any depth. | UNVERIFIED | Session 0-WB #1 already tests it ("down through a tunnel roof"). If it fails, V1 must cover walls and roofs, not just the top. |
| P7 | **Shovel / fill inside a deep tunnel.** AtGround refuses below FLOOR + 0.25 on voxel cells (row d). | VERIFIED-CODE | Use TUNNEL_FLOOR + 0.25 there. |
| P8 | **"Ground top" search.** ColSurf stops at FLOOR + 1 (row c). | VERIFIED-CODE | Search down to TUNNEL_FLOOR + 1. |
| P9 | **Dark / light inside.** Is a 10 m tunnel lit like outside? Unknown how Enfusion's sky light treats runtime meshes. | UNVERIFIED (the user has seen 3 m tunnels) | A look check in the session. A torch / flashlight may be the answer if it is too bright or too dark. |
| P10 | **Dropped items, spawns, AI.** Vanilla code that asks for "the ground" at x, z gets the top surface (SafeY), not the tunnel floor. This is the same at 3 m; deeper makes it worse (items appear on the surface above). AI cannot path in tunnels (no navmesh) at any depth. | INFERENCE from MUD_VanillaHoleFix (SafeY traces down from above) | A check: drop a magazine in a deep tunnel. Not new work. |

## 4. The package (replaces U1 "FLOOR 6")
1. **TUNNEL_FLOOR.** A world-ground prefab attribute (the same prefab on every machine) or written into each tunnel
   op by the server. **Default 5 m (user 2026-10-16)**: feet down to ~5 m, a roof of up to ~2.5 m (roof ~ feet depth
   - 2.5 m, MEASURED). 10 m (Cu Chi levels 3-10 m, SOURCE in the improvement plan) is a later step, after 5 m passes.
2. **The P3 clamp fix** in every carve.
3. **The sea-level guard** (P1).
4. **AtGround and ColSurf read TUNNEL_FLOOR** (P7, P8).
5. **A slope cap on downward digs** (P2).
6. **T needs a shovel + a slower rate** (P4).
7. **U2 level digging** (with the ~3.5 cm/m sink fixed) and **the U3 roof guard**.

Optional, mission side: MUD_DepthBoxEntity already exists for the heightfield (MUD_WorldGround.c:2966). A mission
maker can place boxes where deep digging is allowed; reading it in the tunnel floor is cheap and the same on every
machine (placed entities).

## 5. Session checks (add to 1-WB / 1-MP)
All at the 5 m default (repeat at 10 m only when that step is taken):
- Dig down at full pitch to 5 m. Walk back out with no jump: pass.
- Dig level for 20 m at ~4.5 m deep. Feet start vs end within 0.2 m.
- Mortar and RPG into the ground above a 5 m tunnel: no break-in, no refill inside.
- RPG fired inside a 5 m tunnel: it makes a chamber and does not refill.
- Dig toward the coast below the sea level: the guard stops it (and look at what the water does near the guard).
- Shovel (F) and fill at 5 m deep: not refused.
- Shoot the ground above a 5 m tunnel from outside while a friend is inside: do rounds come through? (P6)
- Drop an item at 5 m. Light level at 3 / 5 m (look only).
- 1-MP: hashes after deep digging; a late joiner sees the same tunnel.
- 1-PERF: brick count / memory with a 100 m tunnel at 5 m vs 3 m.

## 6. Parked (feature freeze): keep the surface over deep tunnels
A tunnel more than ~2 m under the surface could leave the terrain cell above as it is: not holed, not voxel. Only
the tunnel rows would show. That would keep grass, trees and ruts above, and save the surface bricks.
UNVERIFIED and risky:
- It needs a character under an un-holed terrain heightfield to be safe. PROVEN only that entities can exist
  under it (mud-gotchas 2026-10-09), not that a character can walk there.
- The cell logic shows bricks per skin cell, not per row.
Parked, not in the work list.
