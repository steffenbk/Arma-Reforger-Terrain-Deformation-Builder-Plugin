# Solid ruts in mud zones - code check + ideas (2026-10-15)

User question: mud-zone ruts look better than voxel ruts, but you don't feel them (no climbing out, no suspension
movement). Voxel ruts feel solid. Idea: let the voxel ground grow UP around the sheet's ruts instead of only down.
Labels: CODE (read in our script today), INFERENCE (not proven), USER.

## Code check
1. **Raising voxel ground is supported.** CODE: the crater op raises a rim above the old surface
   (MUD_VoxelProbe Carve, `bump = rimH * sin(...)`), and FillTrench raises columns ("Only raises"). The brick rows
   above the surface get made by Prepare. So a raised ridge needs no new engine trick - only a new rut profile.
2. **Voxel ruts only go down today.** CODE CarveRut: per pass the floor drops `m_fRutStep` (0.08 m, prefab
   attribute) x profile (full in the middle, half at RUT_R 0.5 m), capped at `min(base + mass x perTon, 0.35 m)`
   under the undug ground. So they ALREADY deepen gradually, pass by pass.
3. **Voxel ruts don't know about mud zones.** CODE: no zone check in WorldGround / VoxelProbe. Inside a zone the
   same 8 cm/pass rut is cut under the sheet.
4. **The sheet's rut is instant.** CODE: the sheet is 0.15 m thick (THICK) and a wheel presses it straight down
   to the hard ground, with side ridges. So after ONE pass in a zone: you SEE ~15 cm + ridges, you FEEL 8 cm
   (and only 3 m behind the truck, RUT_BEHIND). INFERENCE: this mismatch is the "no feel" the user noticed.
   Test: drive the same line 4-5 times in a zone - the feel should grow while the look stays the same.
5. **Voxel ruts do NOT clear the sheet's mud.** CODE: only voxel ops kind 0-3 (crater, tunnel, trench, fill)
   call MudSheet.GroundChanged; rut ops (kind 4) don't. The "sheet wipes mud along ruts" risk on the master
   list is not real. But the sheet's ground heights under a new rut only update by the slow re-trace (OLD_TRACES
   4 per frame), so the sheet may float over a deep voxel rut for a while (INFERENCE, not seen yet).
6. **Mud drag is single-player only.** CODE MudSheet.Drag: `if (RplSession.Mode() != RplMode.None) return;`.
   In MP the ONLY mud feel is the voxel ground. Good for MP safety; it also means the whole "feel" must come from
   the voxel ruts.
7. **Resolution.** CODE: voxel cells are 0.5 m (sheet cells 0.15 m). A rut is ~2 cells wide; a side ridge
   would be 1-2 cells wide - rounded, not crisp. The sheet has to carry the fine shape; the voxels only the
   rough solid shape.

## MP rules any solution must keep
- **Feel = server ground only.** The vehicle is simulated on the server; anything only a client has (sheet,
  sheet colliders) can't push the wheels without snaps. Voxel ops are replicated and joined (JIP), so they can.
- **Never change ground under or right beside a tyre.** v035 RUT_BEHIND (cut once the rear wheel is 3 m past)
  was the smoothest build (USER, 0 corrections / 90 s). Ridges must be made in the same behind-cut.
- **No extra net traffic.** Ruts are already 96 % of the JIP data. Ridges should be a different PROFILE inside
  the same rut op (every machine computes it the same way), not a new op.
- **Zone choice made by the server.** Zones are placed entities (all machines have them), but to be safe the
  server should put the zone values into the op fields it already sends (step = m_fDepth, cap = m_fB).

## Ideas
| | Idea | Feel | MP | Cost |
|---|---|---|---|---|
| A | **Ridges in the rut profile**: middle goes down, sides (0.5-1.0 m from the track) go up by part of what was removed, capped (e.g. 0.12 m) | climb-out walls on both sides, suspension works | same op, no extra traffic | small: one profile change in CarveRut. Old heightfield had this (berm 0.9 m / 0.08 m) |
| B | **Zone-tuned ruts**: in a zone the server sends a bigger first step (~0.15 = sheet thickness) and a deeper cap (e.g. 0.5) | feel matches the look on the first pass; deeper mud = deeper ruts | values ride in existing op fields | small |
| C | **User's "up only"**: floor stays, only the walls grow | walls yes, but the truck never sits lower (no sinking feel); the hump between the wheel tracks can hang the belly | as A | as A |
| D | **Sheet follows the voxel rut**: rut ops also tell the sheet "ground moved, move the mud down with it" (not "clear") | look sits on the real rut floor/ridges, no floating | client-only, no net | medium |
| E | **Sheet deepens per pass** to match the voxel step instead of jumping to full depth | look = feel each pass, MudRunner-like build-up | client-only | medium |
| F | Slippier when deeper (gamemat by depth on the brick colliders) | MudRunner's "deeper = slipperier" | server collider, so MP-safe in principle | big / unproven: one emat on 2 sections crashed (engine probes) |

**Recommendation:** A + B first (both inside CarveRut + the server's op values, no new op, no new net data),
with C's spirit kept by putting most of the height into the ridges and only a little into the floor. Then D so
the sheet sits on the new shape. E and F later.
Order: (1) the 4-5-pass zone test from point 4 to confirm the cause, (2) A + B in SP, (3) MP run (DS + 1 peer,
count 'MUD corr' snaps vs v035), (4) D.
Feature freeze: this is joining two existing parts (voxel ruts + sheet), listed as the first parked idea until
the user starts it.
