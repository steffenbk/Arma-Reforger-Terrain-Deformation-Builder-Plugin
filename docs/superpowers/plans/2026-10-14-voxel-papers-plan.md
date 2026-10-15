# Plan: what we take from the voxel papers (all ten items)

Date 2026-10-14. Checkpoint before this plan: **v038 "pre-voxel-papers-plan"**.
Sources: research/2026-10-14-lengyel-voxel-terrain-notes.md, research/2026-10-14-voxel-papers-batch2-notes.md.
All code anchors below were grepped in MUD_VoxelProbe.c on 2026-10-14 (line numbers drift - search by name).

## Facts found while planning (change two items)

- **The seed is ALREADY delta + run-length coded.** MakeSeed / SeedRun (~1743) write "~n." for runs of 4+
  unchanged values and "!d." for big jumps; DecodeSeed (~1758) reads them. So item 3 is "measure what fills JIP",
  not "add RLE".
- **Each sample edge is meshed by exactly one brick.** Polygonize (~3317) only walks edges whose BASE sample is in
  this brick (+x / +y / +z), so overlap cells can't double quads. Item 6 is answered by reading; it keeps one cheap
  runtime check.
- Our sign is the opposite of the papers: here `solid = v > 0`.
- Vertex placement is CellPos (~3529): the mean of the crossings on a cell's 12 edges (plain surface nets). That's
  the one place item 1 changes.
- Sample grid per brick m_Gb.m_aG is 10x10x10 (index lk*100 + lj*10 + li): the 8x8x8 brick + one ring.

## Order of work

| Step | Item | Kind | Effort | Value |
|---|---|---|---|---|
| A | 2 Brick-edge lighting check | look in game | very easy | settles a question |
| A | 6 Seam double-quad check | log counter | very easy | settles a question |
| A | 3 JIP / seed size measurement | log counter | easy | decides item 3b and 4 |
| A | 7 Triplanar material search | asset search | easy | decides item 7b |
| B | 1 Dual Contouring vertex | offline Python, then in game | medium | HIGH (rut look) |
| C | 5 Smoothing pass after ops | in game, switch | medium | medium |
| D | 4 Lazy per-brick op queues (JIP) | in game, MP | medium-hard | high once matches are long |
| E | 9 Finer rut cells (0.25 m) | cost measurement only | medium | high but costly |
| F | 8 "Draw nothing" cells | in game | medium | only if bunkers come |
| F | 10 Material per voxel | in game | medium-hard | low-medium, after 7b |
| G | Engine probes after the IDA study (3 small probes) | in game | easy each | settle 3 engine questions |

Steps A run together in ONE test session (one user run). Checkpoint after each step the user confirms.

---

## Step A - four cheap checks (one session)

**A1 - Brick-edge lighting (item 2).** No code.
- SP Arland, set the time to low sun (morning / evening), blast 3-4 big craters in a row so they cross 4 m brick
  lines (brick grid = BRICK 4 m from the skin grid corner).
- Turn on the brick debug view if it exists, else note positions.
- Look for straight light/dark lines on the 4 m grid inside crater walls. Screenshot.
- Done when: yes / no with a screenshot. If yes -> item 2b below.

**A2 - Seam double-quad check (item 6).** Tiny temp code.
- In BuildSub (~3153) after Polygonize: count quads whose 4 cell indices are all in the outer ring (li/lj/lk = 0
  or 9 in cell index). Log once per build: "MUD seam check: ring-only quads N".
- Expected: 0 (by the base-sample rule). Any non-zero = real doubling bug.
- Remove the counter after.

**A3 - What fills JIP (item 3).** Log only.
- In JipBatches (~870) log per batch: op count, total chars, chars spent in the seed field, chars in the plan field,
  and the biggest single op.
- Run: SP or local DS, 20 craters + 2 min driving (ruts) + 3 trenches + 1 tunnel, then join a client.
- Write the full per-kind table into docs/superpowers/results/2026-10-14-jip-size.md (rule: whole table, sums checked).
- Decides: 3b (shrink what dominates) and how urgent 4 is.

**A4 - Triplanar material search (item 7).** No code.
- asset_search / game_browse in the pak for terrain or rock emats with world-space / triplanar projection
  (names like "Triplanar", "WorldSpace", "Projected", shader classes on rock or cliff materials).
- Done when: list of candidate emats with the parameter names, or "none shipped".

---

## Step B - Dual Contouring vertex (item 1), the big one

Goal: sharper rut walls and crater lips at the same 0.5 m cells, no change in cost class.

**B1 - Offline prototype first (Python, Tools/).**
- Copy the sample grid logic: build the 10x10x10 field from the game's real op formulas (crater, rut CarveRut,
  trench), as Tools/sdf_patch_proto.py already does for the band mesher - reuse it.
- Per cell with crossings: plane per crossing = (crossing point, normal). Normal = gradient of the trilinear field
  INSIDE that cell from its 8 corners (no extra sample ring needed - the grid has only one ring).
- Solve the 3x3 least-squares (QEF) with the mass point (today's mean) as the bias point; clamp: if the answer
  leaves the cell (or the cell's box + 10 %), use the mass point. Drop tiny singular values (SVD threshold ~0.1)
  so flat ground stays flat.
- Measure on the same shapes: max deviation from the exact SDF surface (old vs new), rut wall angle, count of
  self-intersecting triangles, time per brick in Python as a ratio to the old CellPos.
- Write results/2026-10-1x-dc-proto.md with the full per-shape table.
- Go / no-go: clearly sharper ruts and crater lips, zero or near-zero folded triangles, cost ratio <= 2x.

**B2 - In game (only on a go).**
- New switch `DC_VERTEX` (default false) and constants `DC_SVD_EPS`, `DC_CLAMP`.
- Change only CellPos: keep the mean as fallback, add the QEF solve when DC_VERTEX. Normals for UVs stay as now.
- Checkpoint before. Test SP: rut drive + craters, look; then the MP robot run (>= 6 runs) for build ms/s and
  corrections vs the current build.
- Watch: collider faces folding (prone character sinking, vehicles snagging at rut walls).

---

## Step C - Smoothing pass after ops (item 5)

**PARKED 2026-10-14:** built (SmoothBox, SMOOTH_OPS switch, now false), SP on/off test: user saw no real difference;
cost 0.55 ms per rut op, up to 7 ms per blast. Results: docs/superpowers/results/2026-10-14-smoothing.md.

- New switch `SMOOTH_OPS` + `SMOOTH_PASSES` (1) + `SMOOTH_W` (0.5).
- After an op writes its samples (Put ~1210), blur only the touched samples + 1: new = (1-w)*v + w*mean(6
  neighbours), never across MISS columns, never changing sign far from the surface (only |v| < CELL*1.2).
- Must run identically on server and client (it's part of applying the op, so JIP replays it too).
- Test SP: craters + ruts on vs off (screenshots), build ms. Then MP: 0 new corrections.

---

## Step D - Lazy per-brick op queues for JIP (item 4)

Only if A3 shows JIP replay is heavy or the user sees join hitches.
- Today: client Drain (~821) applies every received op in seq order inside DRAIN_MS / JOIN_MS.
- Change: on JIP, ops are decoded and filed per affected brick key (op box -> brick keys); bricks within R (e.g.
  150 m) of the local player are applied first, the rest are applied in the background budget or when the player
  comes near.
- Hard rule: ops touching the same brick still apply in seq order (min/max ops don't commute with seeding).
  Ops that span several bricks are applied whole the first time any of their bricks is needed.
- Collider: a brick with pending ops must not be driven on - apply on approach with a margin (> 2x max vehicle
  speed x 2 s).
- Test: local DS, 200+ ops, join a client far away and near; measure join time to first frame and hitch ms.

---

## Step E - Finer rut cells (item 9), measurement only

- Offline (Python, same proto): CELL 0.25 for a rut brick: samples 8x, surface cells ~4x; time it vs 0.5.
- In game probe (temp switch): one SP drive with rut bricks at SUB 16 / CELL 0.25, read build ms/s.
- Decide with B: if DC already makes ruts good enough, drop E.

---

## Step F - later, only when wanted

**F1 - "Draw nothing" cells (item 8)** - for a bunker / tunnel prefab entrance.
- A per-cell flag set (like the LOOK bit) by an op kind "no-draw box"; Quad skips quads whose cells are all
  flagged; the collider must skip them too or the entrance stays blocked.
- Needs a real bunker prefab to test against.

**F2 - Material per voxel (item 10)** - after A4 finds a usable material.
- A byte per sample column (or per sample), op kinds write it (crater = dug soil, deep = rock); Quad picks the UV
  kind / mesh material per cell from the min-corner rule.
- Script meshes are one material per mesh section, so each material = one more section.

**F3 - Brick-edge normal fix (item 2b)** - only if A1 shows lines.
- Engine rebuilds normals from our triangles (UpdateVerts). Idea to test: shared edge vertices get the same
  normal only if both bricks see the same faces - try adding the neighbour ring's quads as zero-area copies.
  Unproven; probe first.

**F4 - Triplanar blend (item 7b)** - only if A4 finds a material that blends by normal.

## Step G - engine probes after the IDA study (added 2026-10-14, last step)

DONE 2026-10-14 (docs/superpowers/results/2026-10-14-engine-probes.md): G1 no visible seam even without the fix (not taken);
G2 same emat on two sections CRASHES (proven, mud-gotchas); G3 patching works but costs the same as a rebuild
(not taken). MUD_EngineProbes.ENABLED = false.

From docs/superpowers/research/2026-10-14-ida-voxel-feasibility.md. Not worth trying: our own normals, vertex
colours, a second UV set (the script mesh functions have no slot for them).

**G1 - Brick-edge light fix with tiny triangles.** Add 1 mm copies of the neighbour brick's edge triangles to the
drawn mesh only (not the collider), so the engine's normals at a brick edge see the same faces from both sides.
Compare lighting at a brick edge in low sun, on and off. Note: A1 (user, 2026-10-14) saw no light seams at brick
lines, so this only matters if seams show up later.

**G2 - Same material on two sections.** A throwaway mesh with two sections using the same .emat: does it crash? A
crash confirms the old skirt crash and turns "each section needs its own emat" into a proven rule (mud-gotchas).

**G3 - Patching the collider in place.** Add a new collider piece to a body and switch the old one off instead of
rebuilding the whole collider; time it against a full rebuild (today ~1.45 ms per brick at 0.5 m).

---

## Not taken
Marching Cubes ambiguity fix (we use surface nets), LOD + transition cells (bricks are local), Aokana (needs
custom render passes, blocky, no edits), GPU geometry-shader meshing (not possible from Enforce script).
