# Prompt: fix the 2026-10-15 code review findings

Paste into a local Claude session in the mod folder (branch displaced-ruts-partial-split, pull first):

---

Task: fix every finding of the 2026-10-15 code review of displaced ruts + rut field (commits 6daaac6..08be104).
The findings are listed below with file / function names. Work in stages, measure before changing, and stop after
each stage so I can compile and test. This prompt INCLUDES docs/superpowers/prompts/2026-10-15-patch-memory.md
(Stages 0-2 there = Stages 0, 3, 4 here) - read it too.

Read first: CLAUDE.md, docs/superpowers/research/mud-gotchas.md, docs/features/ruts/README.md,
docs/superpowers/plans/2026-10-15-rut-field-plan.md, docs/superpowers/prompts/2026-10-15-patch-memory.md.
Checkpoint before every code stage: mud_backups/checkpoint.ps1 save "<stage name>".

Rules:
- DISPLACE_RUTS / RUT_FIELD false must still behave as before (v035 / v097). With both true, ruts look and feel as now.
- Every stage reports log numbers before vs after. If the code disagrees with this prompt, stop and ask.
- Enforce traps: no variable name declared twice in one function, no ternary, never name a variable "set",
  static arrays <= 65536, class <= 256 KB.
- After each stage: files / functions changed, what to compile, which log lines to read. Update
  docs/features/ruts/README.md when a stage passes; proven facts go to mud-gotchas.md.

## The findings
F1 (cost, biggest risk) Rut field patch rebuilds: ~10 mesh + ~10 collider rebuilds/s at 6-7 ms each while driving
   (ruts README Stage 4) = ~120-140 ms per second for ONE vehicle, vs ~20 ms/s for v035 ruts. Scales with vehicles.
   Split lines exist: 'MUD band: mesh split ms' / 'collider split ms' (BandStats).
F2 (memory / JIP) Floor patches never freed (~64 KB arrays + mesh each); JIP replay builds patches for every rut.
   = the patch-memory prompt.
F3 (test gap) Hole check (DispHoles) only tests carved voxel cells (DrawsAt) and its trace filter (DispOurs) only
   accepts MUD_VoxelSubEntity - field ruts on patches and the patch side of the grass -> crater seam are never
   checked. Draw floor (DispFloorList) colours every field-rut sample white (FieldSurfAt needs a voxel column).
F4 (MP, low) Patch mesh / collider / Heights scale the floor by m_aLock, which includes buildings from local
   queries (BuildingLock) - may differ between machines (destroyed building) and bricks (Displace) don't apply it
   (small step where a rut crosses patch -> brick beside a building).
F5 (small) PatchCounts comment says "always" but runs only with m_bDebug (Stats). Per-op Prints ('MUD rut floor',
   'MUD rut field', the op line) at ~4 ops/s per vehicle = DS log spam. Overflow neighbour rule not built (0
   overflows logged - leave it, just keep the log).
F6 (release) MUD_RpcAsk_PlaceTrench (MUD_TrenchPlanner.c ~518-561) takes `instant` from the CLIENT and calls
   site.DebugDigAll() with no server check - any client can instant-dig (max 20 sites, no per-player cap).
   MudWorldGround.et still has m_bDebug 1.

## Stage 0 - measure (no behaviour change)
- Patch-memory Stage 0 runs (the 'MUD patches' line is in already): 2-3 km rutted drive; late join test.
- NEW: stress run, 3-5 vehicles driving on fresh ground at once (robot or players), 2-3 minutes: record fps,
  'MUD band' build counts / ms and the mesh / collider split lines, 'MUD patches' counts. One vehicle the same way
  as the control. Write all numbers to docs/superpowers/results/2026-10-15-review-baseline.md.

## Stage 1 - F1: cheaper floor rebuilds (shaped by the Stage 0 split)
- From the split lines, report which part dominates: band mesher core, input loops, reveal / UV, MeshObject,
  per-material trimeshes, slab (FireGeo), plate, DropBody, MakeStatic.
- Then propose (don't build yet) the smallest change for floor-only rebuilds, e.g.: skip / reuse geoms that a floor
  change cannot alter (plate = safety under the floor, slab = bullets), rebuild the collider only on machines that
  need it (server + the patches near a vehicle on clients), coalesce a patch's rebuilds while a vehicle is still
  laying ruts in it (the op stream marks it dirty ~4 x / s), longer Mesh / Collider Interval for floor-only dirt.
  Wait for my OK, then build it behind a const and re-run the Stage 0 stress run. Target: well under half the ms/s.

## Stage 2 - F3: debug coverage for field ruts
- DispHoles: with RUT_FIELD, also check sample points that are NOT carved: expected = Heights() dug (it already
  includes the floor min + lock), trace filter accepts MUD_FinePatchEntity too. Keep the carved-cell path as is.
  Add a seam mode: points within 1 m of a carved / non-carved cell line get checked from both sides.
- DispFloorList: depth colour on plain ground from UndugY (as CarveFloor's plain rule), voxel only where a column is.
- Test: drive a rut grass -> crater -> grass with hole check on; 0 holes expected (or report them).

## Stage 3 + 4 - F2: patch-memory prompt Stages 1 and 2
- Do them as written there (slim MUD_FinePatch; free floor-only patches + camera-range rebuild; FloorDirty marks
  existing patches only). Re-run the Stage 0 drive, late join and stress run after each.

## Stage 5 - F4: lock determinism
- Find exactly what m_aLock is made from (EdgeLock = kept terrain border; BuildingLock = building queries) and
  whether the building part can differ between server and client (destroyed / streamed buildings, DS vs client).
- Propose one of: (a) floor ignores the building part (kept-terrain edge only, deterministic on every machine) and
  bricks apply the same edge lock, or (b) keep it and document why it is safe. Wait for my OK, then build it.

## Stage 6 - F5 + F6: small fixes and release switches
- PatchCounts: fix the comment (debug only) - or move it out of Stats if I want it always (ask).
- Per-op log lines: behind one const (e.g. MUD_VoxelProbe.OP_LOG, true in dev) so release can turn them off.
- Instant trench dig: add a server-side const in MUD_TrenchSite (e.g. ALLOW_INSTANT_DIG, true for testing) and check
  it in MUD_RpcAsk_PlaceTrench before DebugDigAll (log a refusal). Keep Ctrl+F working while it is true.
- Do NOT flip m_bDebug / ALLOW_INSTANT_DIG / OP_LOG to release values yet - add them to the Lane 2 table in
  docs/superpowers/backlog/2026-10-15-master-list.md as "flip before release".

## Stage 7 - MP gate
- Robot test >= 6 runs per setting (CLAUDE.md) before vs after the whole set; late join test (disp hash lines match,
  columns equal, join time); the stress run once more. Results to docs/superpowers/results/.
