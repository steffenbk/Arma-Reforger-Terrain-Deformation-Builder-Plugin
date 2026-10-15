# MudRunner wheel-in-mud physics (IDA, 2026-10-14)

Source: G:\SteamLibrary\steamapps\common\MudRunner\MudRunner.exe (Havok 2014, own engine), read in IDA Pro;
data values from Media.zip (media.xml, _templates/trucks.xml). Read from the binary, not measured in game.
Lines marked INFERENCE are my reading of unnamed fields.

## Data (Media.zip)
| Substance | SubstanceFriction | LinResistance | FullPushDepth | PushResistance |
|---|---|---|---|---|
| Mud (media.xml) | 2.2 | 10.0 | 1.2 | 36.0 |
| Water (media.xml) | 0.4 | 1.0 | 2.0 | 1.0 (+ Water FlowImpulse 1.5) |

Wheel templates (_templates/trucks.xml): WheelSoftness Stiff/Average/Soft = SoftForceScale 0.32/0.24/0.12,
RadiusOffset 0.02/0.025/0.03; WheelFriction Offroad/Default/Highway = SubstanceFriction 1.2/0.75/0.5,
BodyFriction 1.0/1.25/1.5; TruckWheel Mass Light 50 / Medium 100 / Heavy 200 / Superheavy 600.

Parser sub_140932DF0 stores the substance as 4 floats in this order: SubstanceFriction, LinResistance,
FullPushDepth, PushResistance (+0, +4, +8, +12; inside the mud object at +40..+52, +56 = a 5th value, INFERENCE:
water FlowImpulse).

## Wheel force function sub_140930110 (per wheel, per physics step)
Wheel object offsets: radius +120, width +116, wheel SubstanceFriction +128, centre x/y/z +212/+216/+220,
forward axis +224..+232, axle axis +236..+244, spin rate +260, ground speed +264, mud depth out +272 (water +276).
1. depth = mudSurface(x,z) - (centreY - radius), clamped >= 0. Nothing happens below 0.025 m (water 0.05).
2. Velocity at the wheel; if |v| > 1 m/s it is reshaped to |v|^0.75 (drag grows slower than speed).
3. DRAG (linear, opposite motion): forward part x width * min(2r, depth) (frontal area), sideways part
   x pi * r * min(2r, depth) (side area); vector capped at 8, then x LinResistance.
4. LIFT (up): min((depth - x*0.5) / FullPushDepth, 1) * 2 * width * r * PushResistance, capped 128, minus
   128 x upward speed, never below 0. (x = a 2nd value from the mud query, INFERENCE: mud softness / water.)
5. Mud flow push (x the 5th value): only matters for water currents.
6. SPIN (wheel turning faster than it moves): torque about the axle against the spin,
   slip x min(depth/0.4, 1) x 0.1 x SubstanceFriction(mud), x clamp(mass/200, 0.25, 4), capped 8.
   With drive torque (+168 > 0) also a forward PADDLE push:
   ramp((slip-0.5)*2) x (1 - 0.8 x ramp((slip-1)/9)) x depth profile (rises to r/2, falls to 0 at 2r)
   x wheelSubstFriction x mudSubstFriction / (1 + 0.05 x rimSpeed) x tyre-width factor x (+332 / 4000) factor.
7. ROLLING (no spin, no drive): small torque spinning the wheel up to ground speed, x clamp(2 - depth/r, 0, 1).
8. Results x 60 (constant at 0x140A52180) and applied to the wheel's own Havok body (linear: vtable +184,
   torque: vtable +200). Absolute force scale is UNRESOLVED (depends on Havok body units / step).
sub_1409310D0 is the same for the truck body: underside sampled on a 0.8 m grid, 0.64 m2 per cell.

## Mud deform (sub_1408F0C20, "UpdateMudmap" / "PushMud")
GPU only: two triangle lists drawn into the mud texture with D3D blend op MIN (extrude, 20-byte verts) and MAX
(push, 36-byte verts), src/dest blend ONE - matches the article (2026-10-14-mudrunner-mud-article.md). The shapes
are built on the CPU elsewhere (not traced).

## What it means for us
- MudRunner has no mud collider. The wheel goes down to the hard ground under the mud; mud only adds forces:
  drag by submerged area, a lift by depth, spin resistance and a paddle push. Same plan as ours.
- Our sheet already sits THICK on the hard ground, so wheels look sunk with no physics change; only drag is
  missing. Lift is not needed (the hard ground carries us).
- Depth is read where the wheel is going (fresh mud), so driving in your own pressed ruts is easier.
