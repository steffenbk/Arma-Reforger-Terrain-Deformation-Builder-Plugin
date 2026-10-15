# Rut layer split (2026-10-11)

Why: driver-only MP stutter. After the parked-hold fix (run B 02:11) ~1 correction per 7.6 s is left on our
ground, all on patches being rutted, collider 0.12-0.20 m off the newest heights (inference: the ground changes
under the wheels at different moments on server and driver). Idea from user brainstorm 02:22, picked 09:22.

## Design

- `MUD_FinePatch.m_aRut[1089]`: rut + berm deformation (m). Only wheels write it (`Press`).
- `m_aH` stays: craters, trenches, shovel, fill, AND wheel rim plow (user 09:22: keep the plow in physics).
- Mesh = base + H + Rut (ruts look as before, groove reveal on H + Rut).
- Physics = base + blur(H + clamp(Rut, -cap, +cap)), cap = new attribute `m_fRutPhysCap` 0.05 m (user 09:22).
  Max collider change from ruts under a wheel = cap, far under NET_DIST 0.25.
- Rut limits: Rut >= -rut depth (mass rule as before); H + Rut >= floor. Berms only where H and Rut are both
  >= -1 mm (no berm on dug ground, as before), up to Berm Height.
- Crater / trench carve / fill clear Rut on every vertex they write (the blast or shovel wipes the rut).
- Press height check uses the physics height (where the wheel actually rolls).
- Net + JIP: one int per vertex, low 16 bits H mm, high 16 bits Rut mm (was two verts per int). Live traffic ~2x.
- Hash lines hash H and Rut. Seam check compares base + H + Rut. Sleeping skin reads `m_aSmooth` (follows).
- Wheel hold stays (cheap; still covers the rim plow and the capped part).

## Cost

Wheels ride on the capped collider: on a deep rut the tyre hovers over the visual rut floor by
(rut depth - 0.05). Heaviest (0.35 m rut) ~0.30 m. Tune with the cap and Rut Max Depth.

## Not touched

`MUD_GroundEntity.c` (old single-patch test-map entity).

## Test

Same run as B (DS + PeerPlugin1, probe v2): compare 'MUD corr 10 s' ours slow/mid/fast vs run B (3/6/3 in
23/44/24 s). Then look: rut look, tyre hover, crater on ruts wipes them, JIP joiner sees ruts.

## Update 2026-10-11 16:55 - cap removed
User: wheels hovered over the visual ruts. m_fRutPhysCap attribute deleted; PhysD = H + Rut (full depth). The Rut
layer stays as storage (net packing, berm rules, crater wipe unchanged). Reason the cap is no longer needed (being
tested): the snaps came from MUD_Wheels' own SetAllowance (fixed 0.25 m limit); with the engine's limits
(MUD_Wheels.NET_LIMITS false) 4 robot runs on GM_Arland gave 0 corrections (deep-dive doc, 16:38).

## Update 2026-10-11 ~17:50 - split reverted

User asked to go back to before the layer split (ruts were more gradual). MUD_WorldGround.c and MUD_FinePatch.c restored from mud_backups/2026-10-11-0925-pre-rut-split. The split version is kept in mud_backups/2026-10-11-1750-rut-layer-split (*.c.bak).
