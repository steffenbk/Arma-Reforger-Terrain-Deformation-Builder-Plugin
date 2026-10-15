# Project review (2026-10-15 ~17:15)

Read-only review after the mud-sheet park.
DONE 17:15 (user: docs and scripts, checkpoint v085 = before): A archived to mud_backups/archive/unused-scripts-2026-10-15/;
C1 root results/ merged into docs/superpowers/results (links fixed); C2+C3 CLAUDE.md slimmed (feature bullets moved
word for word into docs/features/*/README.md, design line marked as history); C4 old backlog -> backlog/old/.
B (root test worlds, HANDOFF.md) left alone - user decides.
"Used by" counts are grep hits in Scripts / Prefabs / worlds / layers / root .ent (2026-10-15).

## A. Scripts nothing uses (archive candidates, compile-safe)
| File | Class | Used by | Note |
|---|---|---|---|
| MUD_HoleMask.c | MUD_HoleMaskEntity | nothing | old hole-mask tool entity |
| MUD_RelYPins.c | (helpers) | MUD_HoleMask.c only | goes with HoleMask |
| MUD_RutTrailEntity.c + Prefabs/Mud/RutTrail.et | MUD_RutTrailEntity | its prefab only; prefab used by nothing | 2026-10-08 "strip idea, parked" |
| MUD_TerrainOffProbe.c | MUD_TerrainOffProbeEntity | `NEw map/unnamed_Layers/default.layer` only | master list already says archive |

Still used, keep: MUD_GroundEntity.c (old test-map entity, but CoarseSkin / Dig / CutBox / HoleMask and
Prefabs MudGround.et, MudGround_TerrainLook.et, MudWorldGround.et reference it), MUD_DepthBox.c (Arlnd_Layers +
WorldGround), MUD_VoxelHouse.c + MUD_WallPanels.c (VoxelHouse ENABLED true - Lane 2 decision still open).

## B. Old test worlds and files at the addon root (user decides)
| Item | What it is |
|---|---|
| `New test .ent` | 0 bytes - broken empty world (+ .meta, `New test _Layers`) |
| `unnamed.ent` + `unnamed_Layers` | SubScene of worlds/MP/MpTest (uses MudGround.et) |
| `Arlnd.ent` + `Arlnd_Layers` | SubScene of GM_MudArland (has a MUD_DepthBoxEntity) |
| `NEw map/` (678 KB) | old test map (TerrainOffProbe, MudGround_TerrainLook) |
| `Terrain.et` | root prefab from 2026-10-08 |
| `HANDOFF.md` | 2026-10-08 handoff (MudTileEntity era) - fully superseded by CLAUDE.md, .remember and docs/features |

## C. Docs
1. Two results folders: root `results/` (2026-10-14 papers-plan results) and `docs/superpowers/results/`.
   CLAUDE.md / master list / papers plan / gotchas link `results/2026-10-14-...`. Proposal: move root `results/`
   into `docs/superpowers/results/` and fix those links.
2. CLAUDE.md is ~9 KB and loaded every session; most of it is feature detail (voxel bricks, papers plan, mud sheet,
   ONE GRID). Proposal: move each feature bullet into its docs/features/<name>/README.md and keep CLAUDE.md to
   rules, test setup and pointers.
3. CLAUDE.md "Design: everything ... lives in MUD_FinePatch.m_aH" is out of date: since v031/v035 all ops are voxel
   (master list "What exists today").
4. Old backlog files (2026-10-10 master list, ground ideas, performance overview) - the 2026-10-15 list replaces
   the master list for ranking; could move to docs/superpowers/backlog/old/.

## D. Code switches worth a look later (no change proposed now)
- MUD_MudSheet: parked (ENABLED false); its test switches (FREEZE_TEST, OFF_TEST, DIAG lines) stay until it returns.
- MUD_VoxelHouse ENABLED true - untested wall panels live in play (Lane 2).
