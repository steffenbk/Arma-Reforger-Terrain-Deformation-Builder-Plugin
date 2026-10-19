# Ground build performance audit (2026-10-18)

Read-only audit of MUD_WorldGround.c, MUD_FinePatch.c, MUD_BandMesher.c and the helpers they call per build
(index lists, rut floor / look tile lookups) plus the MUD_Prof 'ahead' part. No code was changed.
Builds on 2026-10-18-perf-assessment.md and 2026-10-18-perf-research-ida-web.md; items already there are not repeated
(only referenced). Not audited: the new RUT_RAMP fade in MUD_VoxelProbe.c CarveFloor (RampChains / RampW / RampBehind).

## Summary

1. On a client, about 22 meshes and 14 colliders are built per second while one truck drives; each costs about 5 ms.
2. The cheapest big win is pacing floor-driven mesh rebuilds like look-only ones (0.25 s, not 0.1 s).
3. The next wins cut per-vertex map lookups in the build input loops and shrink the slab + safety plate colliders.
4. A client probably does not need colliders for patches far from any vehicle; this needs one MP test (proxy physics).
5. All savings below are inference from the 10 s log counters (DS + WB peer client), not measured A/B.

## Evidence used (counts and log splits; no Workbench fps used as evidence)

Client log: PeerPlugin1/logs/logs_2026-10-18_22-31-34 (peer client of a dedicated server, one truck driving ruts).
Per 10 s window:

| window | meshes (ms) | colliders (ms) | mesh split input / mesher / reveal+uv / mesh obj | col split input / mesher / trimesh (geoms) | look meshes (ms) | replaced | made ahead | make ms | idx hit / miss | floor spread patches / steps | render-gate waits (patch-frames) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 22:32:22 | 51 (249) | 42 (238) | 78 / 108 / 40 / 23 | 47 / 76 / 109 (194) | 39 (191) | 12 | 31 | 44 | 42 / 31 | 6 / 12 | 0 |
| 22:32:32 | 224 (1106) | 128 (701) | 338 / 452 / 200 / 113 | 145 / 231 / 303 (539) | 161 (811) | 322 | 44 | 61 | 128 / 44 | 20 / 40 | 2749 |
| 22:32:42 | 222 (1044) | 148 (818) | 312 / 450 / 174 / 105 | 165 / 278 / 362 (607) | 139 (673) | 160 | 64 | 83 | 148 / 63 | 30 / 60 | 9575 |
| 22:32:52 | 217 (1108) | 145 (840) | 309 / 500 / 192 / 106 | 156 / 303 / 362 (690) | 141 (742) | 277 | 53 | 85 | 145 / 54 | 23 / 46 | 23933 |

Checksums (evaluated): mesh split 22:32:42 = 312+450+174+105 = 1041 vs 1044 total (3 ms outside the 4 parts);
22:32:52 = 309+500+192+106 = 1107 vs 1108. Collider split 22:32:42 = 165+278+362 = 805 vs 818; 22:32:52 = 156+303+362 = 821
vs 840 (rest = DropBody / MakeStatic bookkeeping). Index hit+miss 22:32:52 = 145+54 = 199 vs 145 colliders + 54 extra
(misses are the mesh-path IdxSet jobs too, so the sum is not meant to equal one build count).

MUD_Prof 5 s lines while driving (client): fps 79-81, MUD script 3.4-4.1 ms/frame, ahead 0.83-1.18, rebuild 1.88-2.20
(rb mesh 1.19-1.36, rb collider 0.68-0.81), vox 0.34-0.50; engine simulation 5.5-6.3 ms (idle 2.2-2.7).
Rebuild queue 15-31 patches, oldest dirty about 7 s, "look covers 0".
Band mesher per build: about 1176 cells, about 2216 tris, about 266 skirt tris.

Server log: server/logs/logs_2026-10-18_22-31-32 (DS at 30 fps), 22:32:22: wheels 1.25, ahead 0.68, rebuild 1.81
(all collider), op all 0.82 ms/frame; 105 colliders (679 ms) per 10 s = about 6.5 ms each; 0 meshes.

Derived (inference from the table): mesh build about 4.7-5.1 ms (input about 1.4, mesher about 2.1, reveal+uv about 0.85,
engine about 0.5). Collider build about 5.5 ms (input about 1.1, mesher about 1.9, trimeshes about 2.45; about 4.7 geoms
per build). About 37 rut ops per 10 s give about 3.2-3.6 colliders per op on the client.
'ahead' about 920 ms per 10 s splits (by subtraction, inference): floor spread about 25 patches x (collider + mesh)
= about 260 ms, make about 85 ms, index misses about 54 x 2 ms = about 110 ms, rest about 450 ms (about 0.5 ms/frame),
mostly AheadView -> ArcHide -> MarkHide.

## Ranked table

Saving = client ms/frame while one truck drives unless noted. All savings are inference.

| # | item | where | saving (inference) | risk | effort |
|---|---|---|---|---|---|
| 1 | Pace floor-dirt meshes at 0.25 s like look-only dirt | MUD_WorldGround.c 4600-4604 | about 0.5 ms/frame client (meshes about 220 -> 130 per 10 s) | low visual (rut appears up to 0.15 s later in the mesh; collider unchanged) | small |
| 2 | Per-patch tile cache for FieldFloor / FieldRidge / MeshFloor in the build input loops | 7043-7084, 7229-7270; MUD_VoxelProbe.c 5147-5170, 5391, 5439; MUD_LookRuts.c 689-704 | about 0.35 ms/frame client; about 0.7 ms per DS collider | none if values bit-identical (same samples, same math) | medium-small |
| 3 | Coarse bullet slab + safety plate (2 m grid) and no per-build m_aPlate fill | 7268, 7388-7415 | about 1-1.3 ms per collider on every machine; about 0.15-0.2 ms/frame client, about 1.3 ms/frame DS | low-medium physics (bullets from below, wheel falling through a gap); redo Task 6 check | medium |
| 4 | Client colliders only near local vehicles / camera | 4667-4699, FarDue 8107, FarFromCamera 8061 | about 1 ms/frame per remote driving vehicle | MP: remote proxies may ride stale ground if they simulate physics (unresolved) | medium |
| 5 | Skip per-wheel ArcHide in AheadView when voxel ruts are on | 2568-2602 (ArcHide 2581) | about 0.3-0.4 ms/frame client; fewer 39 KB MUD_PatchPress allocs | low (no heightfield press happens then) | small |
| 6 | No index lists on the band path (plate only) | 3582, 4626-4639, IdxSet 6348-6431 | about 0.1-0.15 ms/frame; 1 frame less mesh latency; less memory | low | small-medium |
| 7 | Drop the +1 margin of the rut-op FloorDirty box | MUD_VoxelProbe.c 3978 | about 10-12 % fewer floor-driven rebuilds (about 0.2 ms/frame client, DS too) | low (check 'MUD world seams' line) | trivial |
| 8 | One-pass slot bucketing in BuildColliderBand | 7322-7370 | about 0.2-0.3 ms per collider | none | small (already in perf-research-ida-web.md) |
| 9 | Groove section only over revealed tris | 7118-7143, 7150-7186 | about 0.15-0.25 ms per mesh | low visual | medium-small |
| 10 | Per-patch RagNoise cache for RevealT | 7133, RevealT / RagNoise 6518-6558 | about 0.1-0.3 ms per rutted mesh | none (same values) | small |
| 11 | Small allocations and debug cost | prefab m_bDebug, 3637-3638, 3811-3812, 4639-4691 | under 0.1 ms/frame together | none | trivial |
| 12 | Not worth it / already tried | shared band, LOOK_COVERS, COL_SKIP_SMALL | - | - | - |

## Details

### 1. Pace floor-dirt meshes like look-only meshes

- Now: Rebuild (MUD_WorldGround.c 4536-4700) picks the mesh interval at 4600-4604: look-only dirt waits
  max(m_fMeshInterval, LOOK_MESH_EVERY) = 0.25 s, but any patch whose dirt includes the server floor (or m_bColDirty)
  uses m_fMeshInterval 0.1 s. Each rut op dirties about 3-4 patches, and FLOOR_SPREAD adds one more step per frame, so
  most of the 22 meshes/s are floor dirt rebuilt at 0.1 s pace.
- Change: use the 0.25 s pace for the mesh part of floor dirt too (keep the collider at m_fColliderInterval 0.2 s).
  Optional stronger form: where MUD_LookRuts already draws the rut (look tile present over the patch), skip the floor-only
  mesh rebuild until the patch is idle for 0.5 s.
- Saving (inference): meshes about 220 -> about 130 per 10 s, about 450 ms less per 10 s = about 0.5 ms/frame client.
- Risk: visual only; the floor mesh lags the collider by up to 0.25 s. Look ruts already cover the fresh rut on the
  driver's own client. No physics / MP change; floor data and ops untouched (compare_op_hashes unaffected).
- Effort: small (one condition).

### 2. Per-patch tile cache for the input loops

- Now: BuildMeshBand input loop 7043-7084 calls per vertex (1089 vertices) HideAt / Show / SetShow,
  MUD_LookRuts.MeshFloor (7058; = FieldFloor -> FloorSample, which does two FloorDiv, ColKey and m_mRutTiles.Get, plus a
  look tile map Get), MUD_VoxelProbe.FieldRidge (7071; another FloorDiv / ColKey / map Get) and LiftAt.
  BuildColliderBand input loop 7229-7270 calls FieldFloor (7237) and FieldRidge (7248) per vertex plus RutMudCells.
  That is about 3 map Gets and about 12 script calls per vertex, about 3300 map Gets per build.
- Change: before the loop, fetch the at most 3x3 MUD_RutTile entries (4 m tiles, FSUB 16) and 3x3 look tiles (TS 16)
  that overlap the 8 m patch into local refs; index their arrays directly with integer math in the loop. Same sample,
  same interpolation code -> bit-identical heights.
- Saving (inference): input about 1.4 -> 0.5 ms per mesh and about 1.1 -> 0.4 ms per collider: about 0.35 ms/frame on
  the client; on the DS about 0.7 ms per collider = about 0.7 ms/frame at 10 colliders/s.
- Risk: none if the inlined lookup is byte-for-byte the same formula. Keep FieldFloor as the one reference and assert in
  m_bDebug builds that cached == FieldFloor for a few vertices per build. Ops stay floor-blind (J2 rule) because this is
  only on the build path.
- Effort: medium-small (one helper struct, two loops).

### 3. Coarse bullet slab and safety plate

- Now: BuildColliderBand fills m_aPlate per build at 7268 (one vector per vertex), builds the bullet slab at 7388-7405
  (all surface tris copied, lowered m_fBulletSlab 1.5 m, reversed; nv + ni loops + CreateTriMesh) and the safety plate at
  7406-7415 from IdxSet m_Plate (2048 tris). About 4200 of about 6400 collider tris per patch are slab + plate; trimeshes
  are the largest collider part (about 2.45 ms).
- Change: build slab and plate on the 2 m skin grid (5x5 verts, 32 tris each), height = lowest fine value in each cell
  minus the gap. Drops the m_aPlate fill and the 2048-tri plate list. This is the old backlog item
  "O2 Coarse safety plate + bullet slab" (backlog/old/2026-10-10-performance-overview.md), never done.
- Saving (inference): about 1-1.3 ms per collider on every machine; about 0.15-0.2 ms/frame client, about 1.3 ms/frame DS
  (at 10 colliders/s). Also less physics broadphase memory.
- Risk: low-medium physics: a coarse plate sits lower than the fine floor in steep cells (bigger fall before the catch);
  bullets from below could pass in the gap. Redo the Task 6 bullet / fall-through check. Deterministic (built from the
  same floor on every machine).
- Effort: medium.

### 4. Client colliders only near vehicles it uses

- Now: a client builds mesh AND collider for every patch within m_fClientBuildRadius 300 m of the camera
  (FarFromCamera 8061, HeightsOnly 8085, FarDue 8107). Ahead (3809-3951) and AheadView (2511-2625) loop over
  MUD_Wheels.GetAll() = every wheeled vehicle, also remote ones. So a client pays about 14 colliders/s x 5.5 ms for
  ruts made by other players' trucks.
- Change: on clients, build colliders only within R (for example 50-60 m) of the local player, local vehicle or camera;
  far patches keep m_bColDirty and build when something local comes near (same pattern as FarDue). Mesh keeps 300 m.
- Saving (inference): about 1 ms/frame per remote driving vehicle; 0 for the driver's own ruts.
- Risk: MP sync. Whether remote (proxy) vehicles run wheel physics on a client is UNRESOLVED (the mod has no code that
  knows which vehicles a client simulates; IsProxy at 870 only stops wheel ops). If proxies simulate, they would drive on
  the old collider until R. Test first: second driver, log proxy wheel contact y vs floor y on the watching client.
  Bullets fired by the local player at far ground would hit the old collider (visual hit mismatch).
- Effort: medium.

### 5. Skip per-wheel ArcHide in AheadView when voxel ruts are on

- Now: AheadView (runs only when m_bRender) loops wheels at 2568-2602 (Contacts, BrakeCap, YawRate, AheadPoint) and calls
  ArcHide (2026-2043, 4 MarkHide strips) per pressing wheel at 2581. MarkHide (2670-2734) scans the box vertices, calls
  EnsurePress (allocates MUD_PatchPress 9 x 1089 floats, about 39 KB, on first touch) and writes m_aHideAt. With
  MUD_VoxelProbe.ENABLED && RUTS the server makes no heightfield press ("presses 0" in the logs), so this hide work
  prepares for presses that never come. ScrapeView (2604, 2630-2665) still needs its own ArcHide (2664): scrape / ramp run
  at 1467-1470.
- Change: when voxel ruts are on, skip the per-wheel ArcHide; keep ScrapeView.
- Saving (inference): most of the about 0.5 ms/frame 'ahead' remainder, about 0.3-0.4 ms/frame client; fewer PatchPress
  allocations.
- Risk: low; check that no hide flicker appears at the wheel (HideAt is only read where a press shows).
- Effort: small.

### 6. No index lists on the band path

- Now: BuildStep requests IdxSet at 3582 (BUILD_INDEX step); Rebuild's mesh path runs FindIdx / IdxSet at 4626-4639 and
  an idxJob blocks the collider for that frame. IdxSet (6348-6431) copies m_Mesh (6144 ints), slot lists, shell and plate
  into a 16-entry ring keyed by the 16 m_aCell values; a miss costs about 1.4-2.5 ms. With m_bBandMesher 1 (prefab line 9)
  BuildMeshBand never reads it and BuildColliderBand (7217) uses it only for the plate.
- Change: on the band path skip FindIdx / IdxSet and the idxJob frame; build only the plate list (or nothing, if item 3
  is done).
- Saving (inference): about 54 misses x about 2 ms per 10 s = 0.1-0.15 ms/frame, one frame less latency per mesh,
  and less memory (about 100-170 KB per unique IdxSet, inference).
- Risk: low (old non-band path keeps it).
- Effort: small-medium.

### 7. Drop the +1 margin on the rut-op FloorDirty box

- Now: MUD_VoxelProbe.c 3978:
  `m_iFieldDirty = MUD_WorldGroundEntity.FloorDirty(m_iFi0 - 1, m_iFi1 + 1, m_iFk0 - 1, m_iFk1 + 1, op.m_iKind == 4 && LookCoversOp());`
  The box of changed samples is grown by 1 sample each side, so an op ending on a patch edge dirties the neighbour too.
- Change: no margin (or margin only where the sample sits exactly on a shared edge, which already belongs to both
  patches). The band mesher's ghost cells read only clamped samples of their own patch (0..32), so a patch never reads
  outside its own samples.
- Saving (inference): about 10-12 % fewer floor-driven rebuilds (mesh + collider), about 0.2 ms/frame client and a
  similar share on the DS.
- Risk: low; verify with the 'MUD world seams' line and by eye at patch borders. The call at 2148 uses tile-wide
  -1 margins too; same check.
- Effort: trivial.

### 8. One-pass slot bucketing

- Now: the slot loop at 7322-7370 scans every triangle once per material slot (MAT_SLOTS) plus a mud centre / Floor test
  per triangle. Today about 2 surface slots plus mud.
- Change and saving: already described in 2026-10-18-perf-research-ida-web.md; estimate about 0.2-0.3 ms per collider
  (inference). Listed here for ranking only.

### 9. Groove section only over revealed triangles

- Now: the UV / groove loop 7118-7143 and MeshObject.Create with 2 sections (7150-7155), UpdateIndices / UpdateVerts
  (7180-7186) duplicate every surface vertex and index into the groove section, even where nothing is revealed.
- Change: put only triangles with RevealT > 0 into the groove section (often 10-30 % of the patch).
- Saving (inference): less engine normal / tangent work and upload: about 0.15-0.25 ms per mesh.
- Risk: low visual (seam between sections must use the same vertex positions; it already does).
- Effort: medium-small.

### 10. Per-patch RagNoise cache

- Now: RevealT per vertex at 7133 calls RagNoise (6518-6558) = 5 RagHash calls, each a Sin, on cell-centre values that
  repeat for neighbouring vertices.
- Change: compute RagNoise once per cell into a 33x33 (or 32x32) local array per build.
- Saving (inference): about 0.1-0.3 ms per rutted mesh. Same values -> no visual change.
- Effort: small.

### 11. Small items

- Prefab Prefabs/Mud/MudWorldGround.et has `m_bDebug 1` (line 8): BuildStep runs CheckNew per new patch (3637-3638) and
  the debug stats. Set 0 for release.
- Rebuild writes m_sFrameRebuild with string.Format per build (4639, 4655, 4676, 4691); only needed when Prof prints.
- Ahead allocates `array<int> candKey = {}` and candDist every frame (3811-3812) and uses candKey.Contains (linear).
  Make them members cleared per frame; use a set or a stamp per patch.
- The render-gated queue scan (RenderWait 4482-4494) walks about 30 patches per frame with IsRendered: negligible.
- MUD_Prof ON / CSV true: covered by the release-build item in 2026-10-18-perf-assessment.md.

### 12. Not worth it / already tried

- Shared band between mesh and collider: inputs differ (mesh = m_aH + look floor + m_aD, collider = m_aSmooth + real
  floor); measured hit 6 % (mud-gotchas 2026-10-15). Needs a schedule change, not a cache.
- LOOK_COVERS: 0 patches covered in the log ("look covers 0"), no gain.
- COL_SKIP_SMALL: 0-4 skips per 10 s.
- FieldDue scaling, op size, RelaxFloor, prints, render gate: see 2026-10-18-perf-assessment.md.

## Next measurement (to turn inference into numbers)

On a DS + standalone client (not Workbench), same robot route, >= 6 runs per setting: A/B item 1, item 2 + 7 together,
item 3. Read 'meshes' / 'colliders' ms per 10 s and the MUD_Prof rebuild / ahead parts. Item 4 needs the proxy
physics test first.
