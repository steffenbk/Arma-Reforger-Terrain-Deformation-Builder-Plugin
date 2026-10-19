MUD_BlastFxProbe (TEMP 2026-10-17): logged per shell warhead the vanilla effect parts + what a trace down hits.
Result in docs/superpowers/research/mud-gotchas.md (22:52 line). Cause found: MUD_GroundFollow hid the blast ParticleEffectEntity (fixed, user-confirmed).
Call site removed: MUD_WarheadComponent.EOnInit first line "MUD_BlastFxProbe.Log(owner);".
