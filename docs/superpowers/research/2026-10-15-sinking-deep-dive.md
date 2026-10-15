# Sinking everywhere - deep dive (2026-10-15, Exp ArmaReforgerSteamDiag.exe in IDA)

Goal (user 2026-10-15): heavy vehicles sink into grass / dirt / soil anywhere off-road, the SnowRunner /
MudRunner / Spintires "feeling" - not only inside mud zones.
Labels: PROVEN-IDA (read in the exe, not run), READ (vanilla script / web), INFERENCE (not proven), PROBE (to test).

## 1. What the games do (READ, web)

| Game | How sinking works |
|---|---|
| Spintires / MudRunner | Mud layer per 16 m block, height RELATIVE to base terrain, can go below it. Artists paint mud; only blocks near the player's truck are active. Wheel stands on hard ground; mud adds drag (submerged area), lift, spin resistance, paddle push (our IDA notes 2026-10-14-mudrunner-ida.md). Mud deform = GPU MIN (press) / MAX (push) shapes. |
| SnowRunner | Ground is "extrudable" (grass, dirt, sand - each its own base viscosity) or not (asphalt, concrete, rock). Darker tint + wetness mask = stickier. Hidden painted "extrusion" = extra depth for swamps / mud roads, CAPPED on dry-looking ground so it stays predictable. Snow: sink until the ground below; stop at once on non-extrudable ground. Load: sinks deeper but more traction "if the mass is distributed properly". |

Takeaway: the feeling is (a) the truck visibly drops, (b) it slows / bogs, (c) spinning digs you in,
(d) predictable per ground type. Mostly wheel physics; the mud mesh only sells it.

## 2. Engine facts (PROVEN-IDA unless marked)

Wheel runtime state: array at VehicleWheeledSimulation + 568, stride 432 per wheel.
| Offset | Field |
|---|---|
| +292 | radius (runtime, "RadiusState"); +296 = 1/radius |
| +300 | wheel mass; +304 inertia = 0.5 x mass x radius^2 (recomputed when radius is set); +308 = 1/inertia |
| +328 | rolling drag (WheelSetRollingDrag) |

### WheelSetRadiusState (script sub_140751990 -> sub_140570DB0 -> sub_14053B710)
- Stores max(radius, 0.001), recomputes inertia, and wakes the vehicle's physics body.
- The suspension ray uses the RUNTIME radius: sub_14056FCD0 builds each wheel's ray from the hub down,
  end = hub - dir x radius - suspension, length = radius + two suspension values (+100, +104 of the wheel
  config), then one batched cast (sub_1408FA5D0). The contact point is hub - dir x radius.
  => INFERENCE (strong): a smaller radius lowers the body by the same amount, with no ground change.
- Side effects (INFERENCE from v = w x r): a 20 % smaller radius = 20 % lower top speed per gear and ~20 %
  more pull at the ground (like a shorter gear). Inertia drops with r^2 (wheel spins up faster).
- Vanilla already uses it: SCR_WheelHitZone (READ, scripts/Game/HitZone/SCR_WheelHitZone.c) - punctured
  tyre radius x0.9, destroyed x0.8, plus friction x0.4 / x0.2, roughness +1.5 / +3, rolling drag 0.1 / 1.0,
  then WakeUpPhysics. It runs from OnDamageStateChanged on EVERY machine (damage state is replicated, JIP
  included) - the vanilla MP pattern for "changed wheel": replicate a small state, apply it locally everywhere.
- Tyre MESH size: not found tied to the radius state (vanilla shows damage through the "TireDamage<i>"
  signal). INFERENCE: the visual tyre keeps its size -> with a smaller physics radius it draws INTO the ground =
  the half-buried SnowRunner tyre for free. PROBE 1.

### WheelSetRollingDrag (sub_140751A40 -> sub_140570E10) - weak, not a mud drag
- Value clamped to 0..1 by the binding.
- Only reader found: sub_14053B780 - spin-friction torque limit = |w| x inertia x 0.1 x (1 + drag), stored at
  +388 next to the brake torque (+384). So drag 1.0 only DOUBLES a small built-in spin damping.
  Example (INFERENCE): 100 kg wheel, r 0.6 m, 10 m/s -> I 18, w 16.7 rad/s -> ~30 Nm base, ~60 Nm at drag 1,
  vs engine torque at the wheel in the thousands of Nm. Not enough for bogging down.
- The tyre .conf fields RollingResistance / RollingDrag carry the editor note "NOT USED ATM!" (strings at
  0x141bc1190 / 0x141bc1240).

### ContactSurface (.vhcsurf) fields (sub_141320780)
NoiseSteer, NoiseFrequency, Roughness, FrictionNonTread, FrictionTread, RollingResistance, RollingDrag
(defaults 0, 0, 0, 1, 1, 0, 0). The surface RollingResistance / RollingDrag exist as fields and script getters;
whether the vehicle physics reads them is NOT found. PROBE 4.

### Other per-wheel knobs (API, from the 2026-10-15 api_search)
WheelTyreSet{Longitudinal,Lateral}FrictionState, WheelTyreSetRoughnessState (vanilla flat tyre uses it for
bumpiness), WheelSetMassState, WheelGetLongitudinalSlip / LateralSlip, WheelGetContactMaterial,
WheelGetPosition(i, displacement), Engine/Gearbox *State, BaseWeatherManagerEntity.GetCurrentWetness().

## 3. What this means for "sinking everywhere"

| Feeling | Knob | Notes |
|---|---|---|
| Truck drops into the ground | WheelSetRadiusState | Vanilla-proven call; tyre likely draws buried (PROBE 1) |
| Bogging / slow | Hub impulses (as MUD_MudSheet DRAG_TEST) + gearbox efficiency | Rolling drag is too weak (above) |
| Spinning digs in | sink grows with WheelGetLongitudinalSlip | |
| Squishy, unsteady | WheelTyreSetRoughnessState | as vanilla flat tyre |
| Slides sideways in deep mud | lateral friction state, mild | Low grip = vanilla MP snaps (mud-gotchas 2026-10-11) - keep small |
| Ground remembers | voxel rut depth = sink depth (server) | already synced + JIP |
| Looks muddy | sheet window only round sinking vehicles; zones = swamps | |

Sink per wheel (sketch): soft = base(surface of the ORIGINAL terrain, not our brick gamemat) x wetness x zone;
max depth per surface (grass 0.10, dirt 0.15, soil/field 0.25, zone 0.6 m - tune); grows with soft x load,
fast with slip; less in an already pressed rut; recovers slowly on hard ground or rolling gently.

MP: copy the vanilla flat-tyre pattern - server owns the sink per wheel, replicates it (1 byte per wheel,
~5 Hz, only when changed), every machine applies radius / roughness / friction locally. Driver eases toward the
server value. Measure snaps with the robot (>= 6 runs per setting).

## 4. Probes (one session, SP first)
1. Ural, all wheels WheelSetRadiusState(r - 0.2): body drops 0.2 m? tyre mesh buried or shrunk? jitter?
2. Same, one side only: does the truck lean / pull? (tells if it feels like one wheel in a hole)
3. Rolling drag 0 vs 1 at 10 m/s coasting: confirm it is tiny (stop distance).
4. A test .vhcsurf with RollingResistance / RollingDrag set high: any change in coasting? (decides if a
   "mud" surface can carry drag by itself)
5. WheelGetContactMaterial on a voxel brick: expect our dirt.gamemat everywhere -> use original surface.
6. MP robot: radius change on all machines (replicated) vs server only - snap counts.

## Sources
- SnowRunner terrain physics (Saber dev blog repost): https://www.mudrunnermods.com/?p=40604
- Mud and water of Spintires (80.lv): https://80.lv/articles/breakdown-mud-and-water-of-spintires/
- MudRunner console port (80.lv): https://80.lv/articles/mudrunner-porting-a-successful-title-to-console/
- Our notes: research/2026-10-14-mudrunner-ida.md, research/2026-10-14-mudrunner-mud-article.md
