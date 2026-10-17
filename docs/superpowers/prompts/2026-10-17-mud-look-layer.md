# Prompt: mud look layer (the sheet / MudRunner look back) - 2026-10-17

Paste everything below the line into a new chat in the mod folder.

-------------------------------------------------------------------------------------------------------------------
Mud look layer: bring the mud sheet's MudRunner look back as a CLIENT-ONLY drawn layer on top of today's felt rut
field. User go 2026-10-17 (exception to the feature freeze; the master list "Parked ideas" entry moves to work).
Handoff: .remember/remember.md (loads by itself).

Never overwrite, revert or stash my local changes. No commits.

## Stage 0 - get the prototype files, read, check the code (no code changes)

### 0a. Get the prototype from the cloud branch
- `git fetch origin claude/stoic-clarke-uiodzl`.
- For each of these files, check it does not exist locally or has no local changes. Then take it with
  `git checkout origin/claude/stoic-clarke-uiodzl -- <path>`:
  - Tools/mud_look_proto.py;
  - docs/superpowers/results/2026-10-17-mud-look-proto.md;
  - docs/superpowers/results/img/2026-10-17-mud-look-top.png;
  - docs/superpowers/results/img/2026-10-17-mud-look-mesh.png;
  - docs/superpowers/results/img/2026-10-17-mud-look-cross.png.
- Take NOTHING else from that branch: my local docs are newer.
- Run `python Tools/mud_look_proto.py` (needs numpy + matplotlib; it imports Tools/scm_lite_proto.py). The table it
  prints must match the results file.

### 0b. Read
- The results file and its 3 images.
- docs/features/mud-sheet/README.md and its 2026-10-15-assessment.md: what made the sheet look good, and why it
  failed (second ground, client-only physics, cost).
- docs/features/ruts/README.md.
- docs/superpowers/plans/2026-10-17-build-3-plan.md (Stage 8 was dropped; D4 = felt rut only. This layer is
  look-only on top of the felt rut, so D4 stands).
- docs/superpowers/research/mud-gotchas.md (MeshObject rules: one UV set clamped 0..1, no own normals or vertex
  colours, each mesh section a DISTINCT .emat path or it crashes, DYNAMIC_VERTEXES broken -> full rebuilds).
- CLAUDE.md TESTING RULES.

### 0c. Check these code anchors in MY files
Line numbers are from git ebe4527 - grep, don't trust them. Report a table:
anchor | expected | found at | same / different.

| # | Anchor | Expected |
|---|---|---|
| 1 | MUD_WorldGround.BuildMesh | ~:6090; grid path; 33 x 33 (PVERTS); two sections when m_bGroove (m_Material on top + m_RutMaterial "under" layer, revealed by lowering the top by m_fRevealGap x 3); rut field: d = min(d, FieldFloor - base) x edge lock, then the Build 3 ridge max; UVs per skin chunk (ux, uz, uvDiv) |
| 2 | MUD_WorldGround.BuildMeshBand | ~:6544; the prefab has m_bBandMesher 1, so this path draws most patches today |
| 3 | MUD_VoxelProbe FieldFloor / FieldRidge / FieldFloorAt | ~:4848 / :4867 / :4857 (0.25 m floor samples, world y, FLOOR_NONE) |
| 4 | MeshObject.Create calls in BuildMesh | numVerts 1089, numIndices 6144 per section |
| 5 | IdxSet(p) / MUD_IdxSet | ~:6002 / MUD_FinePatch.c ~:208 - the cached 33 x 33 index lists |
| 6 | m_bRender | false on a dedicated server (~:827) - a DS must never build the look |
| 7 | RutFieldOn(p), FieldDue, m_fClientBuildRadius | the near-camera machinery from FLOOR_FREE |
| 8 | Stats / PatchCounts | where a 10 s 'MUD look' line can go (TEST_KIT) |

**STOP. Ask me:**
1. the look radius (start 20 m from the camera);
2. what to do with the felt ridge 0.5-1.0 m out: keep it (low grass hump) for now, or decide later;
3. anything that changes the stages below.

## Stage 1 - offline: make the look maths cheap enough for Enforce (Python only)
The prototype finds the track centre by brute-force nearest-centre search plus a blur. That is too slow per vertex
in Enforce. In Tools/mud_look_proto.py add a `look_fast` model and compare it with `look_run`:
- **Work per 8 m patch + a 0.75 m margin** on a 0.125 m grid. That is ~(64 + 12)^2 vertices.
- **Depth:** D from the floor samples (bilinear, as the patch).
- **Centre samples:** D > 0.02 and D >= 0.9 x the 5 x 5 local max.
- **lat + Dc:** a two-pass chamfer distance transform from the centre samples. Each sample carries its nearest
  centre's depth and position. Then a 3 x 3 box blur of lat and Dc.
- **Track direction (for texture drag):** from the structure tensor of the depth gradient, averaged over about
  0.5 m. Its main eigenvector points across the track; the tangent is perpendicular to it. No SVD, no global
  search.
- **Pass:** same table columns within about 10 % of look_run, and images that look the same. Report the numbers,
  then the operation count per patch (an estimate for Enforce).

**STOP for my OK.**

## Stage 2 - Enforce: the look mesh (mesh only, behind a switch)
- New const `MUD_WorldGroundEntity.LOOK_LAYER = true` (dev) and `LOOK_RADIUS` (from Stage 0).
- Add to the master list Lane 2: "LOOK_LAYER: decide on / off for release after PERF".
- **New BuildMeshLook(p)** for patches where RutFieldOn(p) is true AND the patch is within LOOK_RADIUS of the camera
  (2 m hysteresis so it does not flip).
  - Use a 0.125 m grid: 65 x 65 = 4225 vertices, 64 x 64 x 2 triangles, index count 24576.
  - It needs its own static index list (built once) and its own buffers. Check the class / static array limits.
  - Heights:
    - outside the tyre strip: max(look, felt);
    - inside it: the look groove, LOOK_LIFT 0.02 over the felt floor;
    - the felt value is exactly what BuildMesh would draw there (heightfield + floor + ridge + edge lock).
  - Same two sections and the same emats as BuildMesh. Dirt = the existing groove-reveal trick (lower the top
    where the look marks dirt), so no new material is needed.
  - UVs as BuildMesh at the finer step.
- Everything else is unchanged:
  - the collider (BuildCollider / band collider);
  - the band mesher for far patches;
  - ops, the snapshot, hashes.
- When a patch enters or leaves the radius, mark it mesh-dirty (mesh only).
- Never on a dedicated server (m_bRender false).
- Log (TEST_KIT / m_bDebug, 10 s): `MUD look: patches in range N, builds N, mean ms, worst ms`. This is for the PERF
  session; never judge it in Workbench.
- Run the adversarial-reviewer skill before calling Stage 2 done. Check especially:
  - a patch edge seam between a look patch and a plain one;
  - the edge lock at roads;
  - a crater over a rut;
  - crossing tracks;
  - nothing on a DS.
- **WB check (look only):**
  1. heavy truck, 3 passes: narrow groove + dirt ridge beside it, like the sheet;
  2. walk away past the radius and back: no pop that bothers you, no seam;
  3. rut at a road edge: no gap;
  4. crater over a rut;
  5. two tracks crossing;
  6. get in / out of a truck in the ruts;
  7. 0 HOLE lines;
  8. no script errors.

## Stage 3 - texture drag + tuning
- Texture drag: per-vertex UV offsets along the track direction (Stage 1), up to DRAG_MAX 0.3 m, pushed outward on
  the ridge (as the sheet). Keep UVs inside 0..1 per patch, and limit the neighbour difference (the sheet's
  DRAG_LIM 0.06) so textures do not stretch.
- Tune with me in WB: LOOK_RIDGE_K (0.35), LOOK_LIFT (0.02), LOOK_HALF (0.21), the dirt threshold, the radius.
- WB check: the drag is visible from the driver seat and from outside; no stretched texture; old ruts and craters
  unchanged.

## Final MP / PERF (add to the plan's Final MP list; do not ask me for MP runs now)
- PERF (uploaded mod, standalone client, DS -maxFPS 60):
  - fps and frame times driving 3-4 trucks in ruts, LOOK_LAYER on vs off;
  - the 'MUD look' build ms.
- MP:
  - a DS log has no 'MUD look' builds;
  - robot >= 6 runs, LOOK_LAYER on vs off (the collider is unchanged, so the snap count must not change);
  - a late joiner sees the same look (it comes from the snapshot floor).

## How to work
- Checkpoint before each code stage (`mud_backups/checkpoint.ps1 save "name"`) and after I confirm it works.
- MUD_VoxelProbe.c, MUD_WorldGround.c, MUD_FinePatch.c, next-steps.md and the build plans are CRLF. Edit them with a
  byte-safe python script (rb / wb) from the scratchpad, then count the line endings.
- Grep for the code before editing (line numbers in docs go stale).
- Enforce traps:
  - no variable name declared twice in one function;
  - no ternary;
  - never name a variable "set";
  - static arrays <= 65536;
  - class <= 256 KB;
  - 80 KB stack per method (BuildTile hit ~69 KB).
- Stop after each stage so I can compile and do the WB check. Then read the newest logs/*/console.log after the last
  'Compiling Game scripts took' line yourself.
- After each stage update:
  - the status in docs/superpowers/plans/2026-10-16-next-steps.md (the "Already built" table);
  - the master list entry;
  - '## Now' in .remember/remember.md (under 25 lines).
- Never measure performance in Workbench. New ideas go to "Parked ideas".
