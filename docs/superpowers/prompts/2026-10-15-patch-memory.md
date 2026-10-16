# Prompt: rut field patch memory + JIP (2026-10-15)

Paste into a local Claude session in the mod folder:

---

Task: make the rut field scale over a long match. Today every heightfield patch a rut floor touches is never freed
(MUD_FinePatch.m_bFloor: "kept, may sleep") and each MUD_FinePatch holds 15 float[1089] arrays (~64 KB) + its mesh
while asleep, so memory climbs with every metre of rut. Also every replayed rut op on a late joiner calls
MUD_WorldGroundEntity.FloorDirty -> EnsureRange, so a joiner builds patches for every rutted spot at join.
Source: review 2026-10-15 in chat; docs/features/ruts/README.md (rut field section);
docs/superpowers/plans/2026-10-15-rut-field-plan.md.

Read first: CLAUDE.md, docs/superpowers/research/mud-gotchas.md, docs/features/ruts/README.md,
docs/features/heightfield-ground/README.md, then MUD_FinePatch.c and in MUD_WorldGround.c: FreeIdle, MaySleep,
SleepPatch, Ahead, NearPlayers, FarDue, HeightsOnly, InitPatch, FloorDirty / MarkFloorDirty, RutFieldOn.
Checkpoint first: mud_backups/checkpoint.ps1 save "before patch memory".

Rules:
- RUT_FIELD false must still behave exactly as v097; with it true, ruts must look and feel as now.
- Measure before changing: every stage reports numbers from the log.
- If the code disagrees with this prompt, stop and ask; don't improvise.
- Enforce traps: no variable name declared twice in one function, no ternary, never name a variable "set",
  static arrays <= 65536, class <= 256 KB.
- Stop after each stage so I can compile and test.

Stage 0 - baseline (no code, or a log line only)
- Add (if missing) a 10 s line: `MUD patches: live N, asleep N, floor-only N (m_bFloor && !m_bDeformed), deformed N,
  heights-only N, mem KB` (System.MemoryAllocationKB).
- Run with me: (a) SP 2-3 km rutted drive, note the counts + memory every minute; (b) the rut field JIP test from
  the plan (late join after a rutted drive + a crater over a rut): joiner's join time, patches built during join,
  'MUD disp hash' lines match server vs joiner, columns equal. Write the numbers to
  docs/superpowers/results/2026-10-15-patch-memory-baseline.md.

Stage 1 - slim MUD_FinePatch
- grep every use of each MUD_FinePatch array. Report which run only in the old heightfield wheel-press paths
  (expected: m_aRutTouch, m_aBermTouch, m_aShow, m_aHideAt, m_aScrapeTouch, m_aRutFrom, m_aBermFrom, m_aPendOrig,
  m_aPendAt - CHECK, body scrape may still be live) and whether those paths can run while the voxel / field ruts are on.
- Move the press-only arrays into a holder class created on first use (null = never pressed), with the same
  default values the arrays have today (check the -1 / -100 initialisers). Readers treat null as "default".
- Test: compile, same drive as Stage 0: memory per patch down, ruts / scrape / craters unchanged.

Stage 2 - free floor-only patches, rebuild them when needed
- A patch with m_bFloor && !m_bDeformed holds nothing the rut tiles don't: allow FreeIdle to free it like an
  undeformed one (same m_fFreeAfter rule).
- Players on foot make no patches today (NearPlayers comment), so add: within the camera / player range a client
  already uses for patches (find it: FarDue / m_fPlayerRadius), a missing patch whose square has a rut tile
  (MUD_VoxelProbe.FieldInBox) becomes a build candidate, nearest first, inside the existing frame budget.
  Server: vehicles' Ahead already makes patches where wheels go (collider), so no server change expected - confirm.
- FloorDirty: mark only EXISTING patches (no EnsureRange). Missing ones read the floor when they are built
  (InitPatch -> RutFieldOn). This also stops the JIP patch storm.
- Test: drive away 300 m and back - ruts still there, look / feel same; walk (no vehicle) up to an old rut - it
  appears before you reach it (note the distance); patch count + memory flatten over a long drive; repeat the
  Stage 0 JIP test: join time and patches built during join down, hashes still match.

Stage 3 - MP gate
- Robot test >= 6 runs per setting (CLAUDE.md), before vs after: corrections not worse.
- JIP test again (as Stage 0 b).
- Update docs/features/ruts/README.md and heightfield-ground README; add proven facts to mud-gotchas.md.

After each stage: list files / functions changed, what to compile, which log lines to read.
