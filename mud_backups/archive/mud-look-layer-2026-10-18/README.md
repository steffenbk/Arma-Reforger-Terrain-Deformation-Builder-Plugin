# Mud look layer - archived 2026-10-18 (user: "its really not so good, stop it here")

Findings: docs/features/mud-look/README.md. Full tree with the hooks in: checkpoint v176
("mud look layer before removal"); v173 = before Stage 2 (WorldGround / FinePatch now equal v173 again, plus the
look-ruts Reset() line).

- `MUD_LookLayer.c` - the whole layer (buffers, Compute = look_fast, Assemble, Indices). Was Scripts/Game/Mud.
- `hooks_vs_HEAD.diff` - git diff vs ebe4527 of MUD_WorldGround.c, MUD_FinePatch.c, MUD_LookRuts.c at removal time
  (also holds the uncommitted look-ruts work, which stays in the tree).

Call sites that were removed:
- MUD_WorldGround: consts LOOK_LAYER / LOOK_RADIUS / LOOK_HYST / LOOK_CHECK / LOOK_QUIET / COST_LOOK; members m_Look,
  m_fLookCheckAt, m_iLook*; setup `m_Look = new MUD_LookLayer()`; EOnFrame `LookRange()` before Rebuild; Rebuild
  (asleep look-gate builds, settle hold for m_bLookGate, COST_LOOK, clear m_bLookGate); BuildMesh top (busy rule +
  BuildMeshLook); methods BuildMeshLook, LookBaseLock, LookDiag, LookRange, LookStats; Stats -> LookStats.
- MUD_FinePatch: m_bLookNear, m_bLookGate, m_bLookOwed.
- MUD_LookRuts: per-wheel GAP (m_aPendW / m_aNow / m_aOn, GAP 0.4 then -0.05) - reverted to v174 (rearmost wheel, 0.4).
