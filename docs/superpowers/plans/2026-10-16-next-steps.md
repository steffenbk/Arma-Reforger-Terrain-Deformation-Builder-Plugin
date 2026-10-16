# The plan now: ruts, craters, trenches, tunnels (2026-10-16)

One page that merges today's work into an order of steps. Details and evidence stay in:
- plans/2026-10-16-ground-improvement-plan.md (items J1, R0-R5, V1, C1-C7, T1-T3, U1-U3, J2, S1 - problem / change /
  why / verify each)
- research/2026-10-16-soft-mud-alternatives.md (mud look options A-K)
- research/2026-10-16-fact-check-ruts-craters-trenches-tunnels.md (verdict per claim)
- results: 2026-10-16-gap-checks.md, -improve-checks.md, -fix-checks.md (crater / ruts / trench rows; its tunnel table
  is superseded), -voxel-crater-slope.md, -mud-look-checks.md
Nothing in Scripts/ was changed. The user's local scripts are ahead of git: line numbers in the docs are from git
(f7715c3 code) - check them against the local version before building.

## The direction (what stays the same)
Heightfield base + 2D rut field + sparse voxel bricks only where something was dug. Mud gets better by adding NUMBERS to
the rut field and drawing them with the meshes we already have - never a second ground mesh again.

## Step 1 - one game test session (no feature code; three small temp log lines / switches)
| # | Test | Decides |
|---|---|---|
| 1 | Shoot into a voxel crater rim, a trench parapet, flat voxel ground | V1 bullet slab needed? |
| 2 | Walk out of an 81 mm / Hydra crater and a 1.5 m trench (no jump) | urgency of the crater rewrite, trench exits |
| 3 | Fresh vanilla truck: `MUD wheels: <id> registered` in the log | R0 (1.9 SA vehicles) needed? |
| 4 | DS + 2 peers, 3-4 trucks off-road, temp line: rebuild queue length / oldest age | how urgent the rebuild gate is |
| 5 | Temp switch: 21000 fake rut ops, then a late join | confirms the JIP stall (J1) |
| 6 | Rain + high water accumulation: do our patches / bricks get wet / puddles? | mud option E (free wet look) |
| 7 | Wet weather, drive on our ground: mud spray instead of dust? truck gets muddy? | mud option F (free spray / dirt) |
| 8 | Radius probe from the archive (MUD_RadiusProbe): tyre sinks? body drops? | mud option H (soft-soil sink) |
Also look once with the band mesher off (prefab m_bBandMesher 0) at ruts and road edges - decides R2.

## Step 2 - MP and scale blockers (small code, protects everything after it)
1. **J1** a joiner never loses an op (no drop past 20000; ask again from the missing seq; JIP sent over frames).
2. **R2** band mesher off (prefab value) if step 1 looked fine - ~3x cheaper rut rebuilds (MEASURED 2026-10-13).
3. **R1 + far meshes**: clients build rut-field patches only near the camera; far rut patches drop mesh + collider and
   keep the field data; rebuilt on approach (existing HeightsOnly / FarDue). Fixes both the per-client cost and the
   whole-map memory (rut patches are never freed today, ~206 per km - research soft-mud section 4).
4. **R0** only if test 3 failed. **V1** only if test 1 failed.
5. A field hash per voxel op (craters / trenches / tunnels are never compared between machines today).

## Step 3 - crater rewrite (one function: Carve + the server's crater call)
Cosine bowl + road-edge fade over max(1 m, 1.7 x depth) + stack cap + sphere when a roof is over the hit + the
server's jitter / lobes in the op. MEASURED together (fix-checks): no triangle over 44 deg, 0 % over 50 deg, road-edge
drop 1.40 -> 0.14 m, repeated hits stop at 2.39 m, 0 sky holes from a blast in a tunnel; ~25 % smaller footprint
(~240 instead of ~195 mortar craters in the 6000-column budget).
**Your decision first: crater SIZE** - today's big game craters (81 mm: 1.43 m deep, 6.3 m across) or a realistic preset
(81 mm: ~0.47 m deep, ~2.1 m across - measured real 81 / 120 mm craters are this order). Prefab values only.

## Step 4 - trenches and tunnels (small, then medium)
1. **T1** trench cut padded 0.25 m per side: drawn floor = plan within 0.1 m (MEASURED).
2. **U2** "look ahead" digs level (TUNNEL_MID 0.95 -> 1.25): today a level look sinks ~18 deg to the floor (MEASURED).
3. **U1** tunnel FLOOR 6 m (craters / trenches keep 3): a tunnel at 4 m gets a 2.1-2.4 m roof (MEASURED) instead of
   0.45-0.87 m.
4. Then **T2** spoil parapet, **T3** exit ramp (after test 2), **U3** roof guard + collapse rules.
**Your decision:** should T (tunnel) need a shovel like F does? Today it needs no tool (CODE).

## Step 5 - ruts and the mud look (the sheet's look, without the sheet)
1. **Rut radius by depth** (RUT_R = max(0.5, cap / 0.58)): heavy-truck walls 46 -> 40 deg p90, light vehicles unchanged.
2. **Shape channel (felt)**: side ridges + bow wave + gradual press in the rut op (option A). The 0.25 m grid keeps
   ~90 % of the sheet's ridge (MEASURED).
3. **Look channels (client-only)**: texture drag (B, the "fluid" cue), wet-mud material on fresh ruts (D), slump
   near the camera (C).
4. Whatever step 1 proved free: wet / puddles (E), mud spray + muddy trucks (F), wheel sink (H); then clods (G),
   decals (J), grass pressed along tracks (I).
5. Mud drag forces (K) last - MP risk.
**Your decision:** tyre-width ruts (sheet look, steeper walls - wheel-feel risk) or wide soft felt ruts with the narrow
groove + ridges drawn as a look layer on top (MudRunner model). Test both behind a switch.

## Step 6 - long matches
J2 rut snapshot JIP (needs: floor stored in cm on every machine, FillTrench not reading the rut floor, snapshot loaded
after the voxel ops), S1 many-vehicle test and server rebuild gate, C7 buildings + objects over new holes.

## What I need from you to start
1. Which steps go in now (feature freeze: steps 2-4 fix shipped features; step 5 is new look work).
2. Crater size: game-sized or realistic preset.
3. Tunnels: tool required or not.
4. Run step 1 (or tell me which tests to write the temp switches / log lines for).
