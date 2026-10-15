# Unused scripts archived (2026-10-15 ~17:12)

Moved out of `Scripts/Game/Mud` (and `Prefabs/Mud`) because nothing used them (grep of Scripts / Prefabs / worlds /
layers / .ent for every class name in them = 0 hits, 2026-10-15). Nothing deleted. Checkpoint v085 = before the move.
Review: docs/superpowers/results/2026-10-15-project-review.md.

| File | What it was | Last user |
|---|---|---|
| MUD_HoleMask.c | MUD_HoleMaskEntity, old hole-mask tool entity | none |
| MUD_RelYPins.c | MUD_RelYPins helper | MUD_HoleMask only |
| MUD_RutTrailEntity.c + RutTrail.et(.meta) | 2026-10-08 rut strip idea (parked then) | its own prefab only |
| MUD_TerrainOffProbe.c | MUD_TerrainOffProbeEntity, old terrain-off probe | `NEw map/unnamed_Layers/default.layer` (old test map - that entity will not load there now) |

To bring one back: copy it to `Scripts/Game/Mud` (prefab to `Prefabs/Mud`), drop the `.legacy` ending, recompile.
