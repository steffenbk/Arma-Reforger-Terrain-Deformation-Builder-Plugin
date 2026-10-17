# MUD_WetProbe (archived 2026-10-17, Build 3 Stage 9)

Temp probe: optional forced rain (FORCE_RAIN) + 1 s log of rain / wetness / water at the player and, in a vehicle,
per-wheel contact vhcsurf + wheel / body mud and dust (VehicleWheelEffectComponent).

Call sites: none outside the file - it ticks from its own `modded class SCR_CharacterControllerComponent`
(OnPrepareControls), gated by MUD_WetProbe.ON and MUD_TestKit.TEST_KIT. To restore: copy back to Scripts/Game/Mud/.

Results (WB log logs_2026-10-17_14-56-38): rain -> trucks muddy for free, no wheel mud spray (vanilla has none on
land), dry run after 15:32 compile: dust > 0.05 on 708/708 mud_rut, 198/218 grass, 11/11 dirt wheel rows,
0 'released BaseResourceObject' (m_RutMudRes hold fix works). Checkpoint v149.
