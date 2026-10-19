# Prompt for the local AI: build the pressed mud layer (stage 1), 2026-10-19

Copy everything below the line into the local AI.

---

Build stage 1 of `docs/superpowers/plans/2026-10-19-pressed-mud-layer.md` (branch `claude/upbeat-lovelace-q0w022`):
a mud layer drawn by the patch and skin meshes, pressed out where wheels cut or wear the ground. Build + one WB look
session only (look check, no ms). Stop and report after the session.

## Read first
- `CLAUDE.md`; the plan above; `research/2026-10-19-ruts-invisible.md` (why a plain lift hides ruts);
  `mud-gotchas.md` (Enforce limits; clutter >= 1 mm above the heightmap rides on top; the IDA lines).
- Code: `MUD_WorldGround.c` LiftSetup / LiftAt (~6610-6680), BuildMesh (~6740-6810), BandMeshIn (~7410-7470),
  MESH_SKIP_SMALL / MeshSmall; `MUD_CoarseSkin.c` LookLift (~617-651, 747-751); `MUD_VoxelProbe.WearAt / WearOnly`
  (~5085-5105); `MUD_MudSheet.LOOK_LIFT_TEST` (the old TEMP test - leave it false).

## Rules
- Checkpoint first: `mud_backups/checkpoint.ps1 save "before pressed mud layer"`.
- One switch `MUD_WorldGround.PRESS_LIFT` (default false). Off = exactly today. Mesh only, clients only; colliders,
  ops, rut field, wear and the DS untouched. MUD_MudSheet.ENABLED stays false.
- For the look session the lift values must be changeable without a relaunch: make MUD_LIFT_M / PRESS_M / WEAR_PRESS
  static floats (not const) with a TEST_KIT key or console hook that cycles presets and marks every live patch mesh
  dirty + rebuilds the skin chunk meshes in view. Presets: (0, -, -), (0.08, 0.03, 0.5), (0.16, 0.03, 0.5),
  (0.16, 0.06, 0).
- Enforce limits: new per-vertex work in its own small method (BandMeshIn / BuildMesh are near the locals limit);
  no Math.Min/Max as an index; no Print per vertex / per build.
- Git: `git add --ignore-removal .`, commit, push to this branch. No PR.

## Build
1. Patch lift per vertex (both mesh paths, after d is known, replacing the LOOK_LIFT_TEST branch when PRESS_LIFT is on):
   `keep = (1 - WEAR_PRESS * min(1, wear)) * (1 - clamp(cut / PRESS_M, 0, 1))`, `cut = max(0, -rfF)` (the floor incl.
   look ruts, before the ridge), `wear = MUD_VoxelProbe.WearAt(x, z)` only when rfOn (else 0),
   `lift = MUD_LIFT_M * p.m_aLock[v] * keep`. Ridges stay on top (d as today).
2. keep = 0 at patch vertices that touch a cell the voxels carry or a dug cell (m_aH clearly below 0), so a crater /
   trench rim gets no lip. Say how you detect it and what it costs per build.
3. Coarse skin: hole cells lifted MUD_LIFT_M (reuse LookLift with the new value; mesh only, put back before the
   collider as now).
4. A wear-only pass changes no floor: mark the patch mesh dirty on clients when wear changes there (look only, through
   the normal mesh pacing). Count it.
5. Log every 10 s on the client, with PRESS_LIFT on: 'MUD pressed mud: preset P (lift L, press M, wear W) | rutted
   verts N, pressed out N, half (wear) N, drawn below heightmap N | wear mesh marks N'.

## WB look session (one launch, my eyes decide)
1. Urals on grass, drive one line 4 passes (trail probe shows the depth), plus a crater and a short trench nearby.
2. For each preset look from 2 m / 20 m / 100 m: the groove (grass gone? dirt? wall too sharp?), pass 1 (half-pressed
   track), untouched layer (tyres / boots sunk), the crater and trench rims (no lip), a road / runway edge, a building,
   holed props / rocks / trees (sunk?), the patch / skin seam, and a prone soldier (second client or placed character).
3. Screenshots per preset; note what you saw next to the log line.

## Report back
What you built (file / method per step), compile result, the log lines per preset, the screenshots, anything odd.
Do not tune the values yourself and do not turn anything on by default - I pick.
