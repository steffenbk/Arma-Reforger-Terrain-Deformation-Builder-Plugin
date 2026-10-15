# Rut field on the heightfield - build + test plan (2026-10-15)

Goal: ruts stop converting voxel columns. The 0.25 m rut floor from displaced ruts (v097) becomes ONE sparse map-wide
field that both grounds draw: heightfield fine patches (most of the map) and voxel bricks (crater / trench / tunnel
areas, displaced as in v097). Removes the per-match ruts limit (MAX_COLUMNS 6000, research/2026-10-15-rut-alternatives-
deep-dive.md section 1) and stops ruts adding bricks.
Switch: `MUD_VoxelProbe.RUT_FIELD` (false = v097 exactly). Needs DISPLACE_RUTS true.
Labels: CODE (read in master 34d1afe - v097 code was not on GitHub, adapt names), INFERENCE, MEASURED.

Paste to the local Claude:
> Read CLAUDE.md, mud-gotchas.md, docs/superpowers/plans/2026-10-15-displaced-ruts-plan.md and this plan
> (docs/superpowers/plans/2026-10-15-rut-field-plan.md). The v097 displaced-ruts code is the base: map the names
> here (FloorSample, CarveFloor, Displace) to what was really built, and tell me where they differ before coding.
> Checkpoint first. Build in the stages below and stop after each for me to compile / test.

## Why it should work (facts it rests on)
- CODE MUD_VoxelProbe.c:242 CELL comment: voxel sample columns sit on fine heightfield vertices -> the 0.25 m floor
  grid lands exactly on the fine patch vertices (FINE 8 per 2 m skin cell = 0.25 m, PCELLS 32 = 8 m patches,
  MUD_WorldGround.c:88-91). Check once in play: floor sample (fi, fk) world x/z == patch vertex x/z.
- Since v031/v035 all ground ops are voxel; the heightfield's own deformation m_aH stays 0 except for refused ops
  (rare). So the undug ground (UndugY, same on every machine - its comment says so) is a deterministic surface for
  the floor cut where no voxel column exists.
- Patches already rebuild mesh + collider from per-vertex heights (BuildMeshBand ~5980 / BuildColliderBand ~6112) and
  exist on the server (colliders; headless = no render). Wheels drive on patch colliders today.

## Stage 1 - storage independent of voxel columns
- Today (v097) the floor lives in MUD_VoxelColumn. Move it to its own sparse map keyed by 4 m tile (same key as
  ColKey, so voxel and field agree): `map<int, ref MUD_RutTile>` with `float m_aFloor[256]` (16 x 16, NONE filled on
  create) - a tile exists without a voxel column.
- FloorSample(fi, fk) reads only this map (one accessor for everything). Displace / readers unchanged otherwise.
- With RUT_FIELD false the map is filled exactly where v097 filled the columns -> same behaviour. Compile + v097 drive
  test = must look the same.

## Stage 2 - rut ops stop converting columns (RUT_FIELD true)
- Issue (~1757) / Submit (~2112) for kind 4: no ConvCols, no fresh columns, no seed (empty), no MAX_COLUMNS check.
  The op is still numbered, sent and logged (JIP replays it in order).
- CarveFloor surface per floor sample:
  - voxel column there (converted, e.g. a crater): as v097 (field surface, min with floor);
  - else: surf = min(UndugY(x, z), floor). NOT DugUndug / m_aH: m_aH on clients comes by net and can lag ->
    machines would cut different floors. Deterministic = every machine the same.
- Carved marking: only where a voxel column exists and the cell is carved already (v097 rule). On plain ground
  NO MarkCarved - the heightfield keeps drawing it. That is the point.
- Wheel-on-surface check (RUT_ON) against that surf.
- Log per op: `MUD rut field: N samples, M patches dirty, 0 new columns` - the 0 is the headline number.

## Stage 3 - fine patches draw the floor
- Patch vertex v at fine (gx, gz) <-> floor sample: same world point (Stage "facts"). Effective deformation
  d = min(m_aH[v], floor - base) when the sample has a floor.
- Put it into the band mesher INPUTS, not into m_aH:
  - mesh (BuildMeshBand): m_Band.m_aD[v] / m_aY[v] from the effective d -> the groove reveal (soil under deep
    deformation, m_bGroove) works for ruts automatically, as the old heightfield ruts did;
  - collider (BuildColliderBand): y = base + min(m_aSmooth[v], floor - base). The floor is NOT blurred (look = feel;
    the blur was for the old jagged presses).
  - Never write the floor into m_aH: MarkDirty would then queue height rects to every client (IsNet path ~3915) and
    the patch JIP would send it a 2nd time - the op stream already carries it.
- Dirty marking: a local variant of MarkDirty (mesh + collider dirty, queued for Rebuild, NO m_aNetKeys) over the
  floor samples the op changed (+1 vertex). Patch missing there: EnsurePatch / EnsureRange like a wheel press does
  (server too - it needs the collider).
- Readers: Heights() (~4132) dug = min(undug + h, floor) so SurfaceY / AtGround / DugUndug see ruts. DugUndug feeds
  seeds: a crater that converts a rutted column later seeds the voxels with the rut (cm-rounded) and the floor stays
  on top - min of both, consistent. UndugY stays undug.
- VOX cells: the patch already skips them (FillBandOn slot -1); the bricks draw displaced ruts from the same field.

## Stage 4 - SP test (checkpoint first)
Pass / measure:
1. Long drive on fresh grass (2-3 km, no craters): rut ops log `0 new columns`; the voxel probe's column count stays
   flat (v097 would climb). Main result.
2. Look: ruts visible on the heightfield, soil in the groove (groove reveal), sharpness close to v097 bricks.
3. Seam: drive a rut from grass INTO a crater and out again - DISP_HOLE_CHECK on, no gap / step at the VOX cell edge
   (patch vertex and brick vertex read the same floor).
4. Cost: 'MUD band' stats (band mesh / collider builds x ms, m_iBandCoreMs) vs v097 rut ms/s. INFERENCE risk: the band
   path was 3-4 ms per build (freeze-test.md) - with RUT_BEHIND batching it should be 1-2 patches per 250 ms op. If
   too slow: profile band core vs slab / plate / per-material geoms before changing anything.
5. Feel: Diag Vehicles > Show raycast / contacts over field ruts; same smooth profile as v097.
6. Get in / out of a truck parked in a field rut (the 2026-10-08 / v097 door-check gotcha - patches are already hidden
   from it, check anyway).

## Stage 5 - MP (the gate)
- Robot test >= 6 runs per setting (CLAUDE.md), RUT_FIELD false (v097) vs true. PASS = corrections not worse.
- JIP: join after a long rutted drive + a crater crossing a rut. DISP_HASH lines must match server vs joiner, and the
  world HashLines (heightfield) must too. Also check the joiner's 'columns' count is the server's (no rut columns).
- Measure the JIP op count (ruts still replay as ops; compaction is later work, see below).

## Later (not in this test)
- JIP compaction: past N rut ops, send rutted tiles as floor data (bounded per tile) instead of the op log
  (Chrono-SCM style; our patch JIP already does this for heights).
- Ridges as a 2nd field value (both grounds at once).
- Grass flattening along tracks (research 6c) - client-only, can go in any time.

## Rollback
RUT_FIELD false = v097. Checkpoint before Stage 2.
