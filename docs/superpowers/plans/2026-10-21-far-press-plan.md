# Far press + MESH_CAP knob - build plan (2026-10-21)

For: research/2026-10-21-synced-sheet-server-research.md, Problem 1, option A (far press) and option B (MESH_CAP 1500).
Branch claude/upbeat-lovelace-q0w022 at 3ab5bb8 + the report file. Code read for this plan on that tree; nothing built.
Labels: PROVEN-CODE (file:line, read for this plan), MEASURED (a repo doc), INFERENCE.

## Goal

Tracks outside the 20-40 m sheet ring show as a groove (grass-free, soil-coloured once deep enough) on the ground
patch mesh, using the depth the sync layer already holds on every client. The ring still pops in its 16 cm surface
step and fine detail; the track itself is already there when the ring arrives. Look only, client only, no op /
hash / snapshot change.

## What the code has today (checked)

- Ring: `RadSpeed` MUD_MudSheet.c:5790-5795 (RADIUS 40, SPEED_RADIUS_K 320, RADIUS_MIN 20). DropFar 5801-5853: only
  meshes past RADIUS + RADIUS_HYST (46 m) and only when more than MESH_CAP (221, = 600) are stored, farthest first.
- Untouched sheet top = ground + THICK 0.15 (596) + LIFT 0.01 (115). The sync tile cell holds D = mud left
  (MUD_MudSyncTile m_aD, MUD_MudSync.c:2187; THICK = untouched); bare cells m_aB = 1 (crater / dig clear, 664).
  So a pressed track's relief under the drawn sheet is THICK - D.
- Every rut op presses the sync layer, wear-only passes included (MUD_VoxelProbe.c:4220-4221 calls RutOp for every
  kind 4 op; WEAR_PASSES 1 only skips the floor write, :283-289). So a worn-only track exists in the sync layer but
  not in the patch floor - which is why it shows nothing outside the ring today.
- A rut op's sync work ends in JOB_SHEET (StepSheet, MUD_MudSync.c:782-788) with its changed box m_iJi0..m_iJi1 x
  m_iJk0..m_iJk1 in world cells (CELL 0.15 m; cell gi sits at x = gi * CELL, Get :347). Clears end in StepHash
  (:764-772) and never reach JOB_SHEET. A joiner's layer arrives in LoadSnap (:2013; per new tile loop :2047-2056).
- Patch mesh height = base + d (+ LiftAt when LiftSetup said yes): BuildMesh 7355 / 7398-7404, BuildMeshBand 8059 /
  8338-8344. Colliders (BuildCollider 7590, BuildColliderBand 8579) never call LiftSetup. LiftSetup 7002 order:
  PRESS_LIFT (dormant D2) -> A2 lift (needs PatchLiftOn = SPARSE, false now; or LOOK_LIFT_TEST, false). With SPARSE
  false, LiftSetup returns false today: no patch has any lift.
- The dormant D2 pressed lift (PressSetup 7181, PressAt 7214, PressNearHole 7254) already has the parts we need:
  the voxel / dug cell guard, the soil reveal via m_fPressSoil, and the `PRESS_LIFT && m_fPressSoil < rfD` hook in
  both builders.
- Grass: our ground below the heightmap = no grass clutter there (research/2026-10-19-ruts-invisible.md sec 1,
  PROVEN in mud-gotchas 2026-10-08). A press below base therefore also clears grass in the groove - a strong far cue.
- Patches along a track: FieldDue 4516 makes patches from m_sFieldPatches (filled only by FloorFinish 4493 = real
  rut floor) within m_fClientBuildRadius (attribute default 300 m, 221-222) of the camera, one per frame via Ahead.
  A patch with no floor and no deformation stops lazy (IsLazyReady 3626: `!m_bDeformed && !m_bFloor`) - the coarse
  skin draws it, so a press on it would never show. StartPatch 4022 sets m_bFloor (RutFieldOn) before that stop.
- Look-only rebuild = MarkLookDirty 4647 (mesh dirty, queue, LOOK_SPREAD finishes lazy / building ones).
  RENDER_GATE (115) holds rebuilds of patches not drawn last frame past RENDER_NEAR 25 m (4917-4920).
- LAZY_RUT_MESH (MUD_MudSheet.c:260): a patch the sheet fully covers is hidden and its mesh rebuilds held (Covers 5423).

## Changes from the report (decided here)

1. **Press everywhere, not only outside drawn tiles.** Under a drawn sheet tile the pressed patch is always lower
   than the sheet (sheet >= ground + D + LIFT, patch <= ground), and LAZY_RUT_MESH hides fully covered patches
   anyway. Pressing regardless of the ring means no rebuild when the ring moves and no Covers query per patch.
   Lesson 7 (one owner per surface) holds: where the sheet draws, the patch is hidden or under it.
2. **Border fade.** A pressed vertex on a patch border whose neighbour patch draws nothing (missing, lazy, building)
   would open a hole into the skin (the skin has no skirt, MarkLookDirty LOOK SPREAD comment 4660-4663). Those border
   vertices get press 0.
3. **Joiners.** LoadSnap marks its tiles too (the report only names JOB_WRITE). JOB_SHEET is used instead of
   JOB_WRITE: it runs once per rut op, after the slump and quant, with the final box (too-big ops reach it as well).
4. **Cost estimate differs** (INFERENCE): patch mesh builds are 4-7 ms each (MEASURED, results/2026-10-19-mesh-vs-
   voxel-ruts-ds.md), a far truck at 10 m/s crosses an 8 m patch in 0.8 s, so ~1-2 new patches + ~2 rebuilds a
   second per truck in view = ~0.15-0.4 ms/frame at 60 fps per far truck, plus a collider per made patch (~1.4 ms;
   a floor patch builds one). The report said ~0.05. Measured later on the DS, never in WB.
5. **Runtime toggles**, so one WB session covers on / off and both caps (TESTING RULES: one launch, many checks).

## Build steps

Step 0. `mud_backups/checkpoint.ps1 save "before far press"`.

### Step 1 - switches (MUD_MudSheet.c, next to MESH_CAP 221)
- `static const bool FAR_PRESS = true;` (build switch; false = today, nothing new runs) and
  `static bool s_bFarPress = FAR_PRESS;` (runtime, TEST_KIT key flips it).
- `static const float FAR_PRESS_K = 1.0;` press scale (look knob), `static const float FAR_PRESS_EVERY = 2;` s
  between rebuilds of one patch.
- MESH_CAP: keep the const as the default, add `static int s_iMeshCap = MESH_CAP;` and use it in DropFar 5836-5838.
  `static const int MESH_CAP_ALT = 1500;` for the key. A new play resets both (MUD_MudLab.Tick 'new play' path).
- `static bool FarPressOn()` = ENABLED && SYNC && s_bFarPress && !PatchLiftOn() (SPARSE's A2 lift owns the patch
  height when SPARSE is on; far press stays out of that mode in this build).

### Step 2 - sync layer read (MUD_MudSync.c)
- `static int FarBox(int gi0, int gk0, int w, int h, notnull array<float> outD)` (outD = the caller's member array,
  sized once to 56 x 56 = 3136; no new array per call): fills w x h cells from gi0, gk0 with D, using THICK for no tile AND for bare cells (no press over a
  crater / dig: its ground draws itself). Walk the overlapped 3 m tiles (at most 4 x 4 for an 8 m patch + margin),
  `m_mTiles.Find` once per tile, copy its overlapped cells by index `l = (gk - tz*TQ)*TQ + gi - tx*TQ` (TileOf 845).
  Return the number of cells with D < THICK - 0.002; 0 = nothing to press (most patches: they exit here after
  <= 16 map lookups). No per-vertex Get (report: 1089 verts x 4-9 cells too slow).
- In StepSheet (782), before SyncBegin: `MUD_WorldGroundEntity.FarPressMark(m_iJi0, m_iJi1, m_iJk0, m_iJk1);`
  (cells, returns at once on a DS / with far press off).
- In LoadSnap's new-tile loop (2050-2056): `FarPressMark(tx*tq, tx*tq + tq - 1, tz*tq, tz*tq + tq - 1)`.

### Step 3 - marking and pacing (MUD_WorldGround.c)
- Members: `ref map<int, float> m_mFarDue` (patch key -> time it may rebuild), `ref set<int> m_sFarKeys` (every
  patch ever marked: for the toggle rebuild), meters (step 6).
- `static void FarPressMark(int ci0, int ci1, int ck0, int ck1)`: return unless m_bReady && m_bRender &&
  MUD_MudSheet.FarPressOn(). Cells -> fine grid as SheetLiftDirty 7080-7083 (x = ci * CELL, +-1 fine cell). For each
  patch key in PatchLo..PatchHi: insert into m_sFarKeys and m_sFieldPatches (FieldDue then makes a missing one near
  the camera / a player / a vehicle); if the patch exists set `p.m_bFloor = true` (leaves lazy, as MarkFloorDirty
  4715); if not in m_mFarDue, add it with due = now (first press shows at once) else leave the due time.
- `FarPressTick()` from the frame (next to FieldDue's call 4242): at most one key per frame whose due time passed:
  MarkLookDirty over that patch's whole box (since -1 = sweep, as SheetLiftDirty), remove it from m_mFarDue. If the
  patch is missing, leave it to FieldDue and drop the entry. A key marked again while waiting keeps its due time;
  after a rebuild a new mark sets due = now + FAR_PRESS_EVERY. RENDER_GATE and LAZY hold still apply downstream.
- StartPatch 4022: after RutFieldOn(p), `FarPressOn` + any sync tile under the patch (HasTile over its <= 4 x 4
  tiles) -> `p.m_bFloor = true`, so a patch FieldDue makes for a worn-only track builds in full.

### Step 4 - the press in the mesh (MUD_WorldGround.c, look only)
- Refactor the voxel loop of PressSetup 7188-7207 into `PressVoxSetup(p)` (fills m_aPressVox), called by PressSetup
  and FarSetup. PressNearHole is then shared as is.
- `protected bool FarSetup(MUD_FinePatch p)`: m_fLiftX0 / Z0 as PressSetup; grid origin gi0 = ceil((x0 - half) /
  CELL), gi1 = floor((x0 + 8 + half) / CELL), half = m_fFine / 2 (<= 56 cells a side: member
  `ref array<float> m_aFarD`, Resize(3136) once); FarBox; false when it returns 0. Then the border flags: for each of the 4 sides, is the
  neighbour patch drawn (exists, m_iBuild == 0 or asleep with a mesh, not IsLazyReady)? Store 4 bools.
  PressVoxSetup(p). Set `m_bFarOn = true`.
- LiftSetup 7002: after the PRESS_LIFT line and before the A2 checks, `if (!PRESS_LIFT && !MUD_MudSheet.PatchLiftOn()
  && !MUD_MudSheet.LOOK_LIFT_TEST) { m_bFarOn = false; if (!m_bRender || !MUD_MudSheet.FarPressOn()) return false;
  return FarSetup(p); }`. Keep BuildMesh / BuildMeshBand locals unchanged (lesson 8: 64 locals per method).
- LiftAt 7037: first line after PRESS_LIFT: `if (m_bFarOn) return -FarAt(p, v, d);`.
- `protected float FarAt(MUD_FinePatch p, int v, float d)`:
  - lvx, lvz from v; world x, z. Grid cells within +-half of x and z (1-2 per axis); dmin = min of m_aFarD there.
  - press = (THICK - dmin) * FAR_PRESS_K * p.m_aLock[v] (lock: 0 at kept terrain / roads / buildings, the same fade
    the A2 lift and the rut floor use); clamp >= 0.
  - press = 0 on a border vertex (lvx 0 / PVERTS-1, lvz 0 / PVERTS-1) whose side flag says the neighbour draws
    nothing; corners: either side.
  - press = 0 when PressNearHole(p, lvx, lvz) (no lip into voxel / dug cells).
  - Soil, as PressAt 7240-7247: drawn = press + max(0, -d); m_fPressSoil = 1000; if drawn > 0.005,
    m_fPressSoil = -drawn + FIELD_SOIL_D - m_fRevealDepth.
  - Meters (step 6). Return press.
- The two soil hooks: 7402 and 8342 `if (PRESS_LIFT && m_fPressSoil < rfD)` -> `if ((PRESS_LIFT || m_bFarOn) &&
  m_fPressSoil < rfD)`.
- FAR_PRESS false or SPARSE true: LiftSetup behaves exactly as today (check by reading the diff).

### Step 5 - keys (MUD_MudLab.c Tick, TEST_KIT only; numpad / and - are free: grep KC_DIVIDE / KC_SUBTRACT = 0 hits)
- Numpad `/`: flip s_bFarPress; then MarkLookDirty every key in m_sFarKeys whose patch exists (one call per key,
  normal pacing) so the press appears / goes. Hint + `MUD far press: on X (key)` line.
- Numpad `-`: s_iMeshCap 600 <-> MESH_CAP_ALT. Hint + `MUD far press: mesh cap N (key)` line.
- The label line (ShowMode) gets ` | far press X, mesh cap N`.

### Step 6 - meters (TEST_KIT, print only; numbers for the look session, NOT performance)
New 10 s line from the ground's 10 s log (9971 block): `MUD far press 10 s: on X, cap N | marks N, patches made for
press (FieldDue) N, rebuilds sent N, waiting N | built with press N, verts pressed N, soil verts N, border-faded N,
hole-guarded N | FarBox calls N, empty N | stored sheet meshes N`. The stored mesh count = DropFar's `meshes`
(keep the last value in a member).

### Step 7 - compile + review
- Compile in WB (0 script errors; watch for the 64-locals limit and int-on-its-own-line in float maths).
- `adversarial-reviewer` on the diff (session rule 5: bigger Enforce change).
- `checkpoint.ps1 save "far press built"`.
- Feature README (docs/features/mud-sheet/README.md) gets a far press line under its status.

## Risks / open

- Floating: a soldier / wheel on a far pressed track stands up to THICK (15 cm) over the drawn groove; the collider
  is unchanged. Only visible farther than the ring (20-40 m) - test item 6.
- Lazy / asleep: a patch asleep with an old mesh may not take a look rebuild (not read for this plan) - test item 7.
- MESH_CAP 1500 = up to 2.5x stored sheet meshes; DropFar walks every stored tile each RADIUS_EVERY (0.25 s) and the
  drop pick is O(drop x far) - cost check belongs to the DS session.
- Ring step 16 cm stays (report option D, slide-in, is the later answer if it still shows).
- Memory: one MUD_FinePatch per pressed patch along every track out to 300 m (FLOOR_FREE frees them after
  m_fFreeAfter out of range). Count in the 'MUD world' line; size not measured.

## Workbench look session (one launch, function / look only - no fps or ms judged here)

Setup: WB play on Arland offroad (sheet ENABLED, SYNC on as now), TEST_KIT on, a truck, free camera. Far press starts
on (FAR_PRESS true), mesh cap 600.

1. Log check right after load: 0 script errors; `MUD far press 10 s` line present, `on 1, cap 600`.
2. Drive a loop of ~150 m on grass: one stretch once, one stretch three times (same line), all at 8-10 m/s.
3. Park, free camera, look back at the loop from 50 m, 100 m, 200 m. Screenshot each.
   - 1-pass stretch: a visible line (grass-free groove)? 3-pass stretch: soil coloured?
   - Track colour far vs the sheet track near: close enough?
   - Patch borders along the track: any hole / open step into the ground (border fade working)?
   - Next to a road / building edge: no gap (lock = 0 there)?
4. Numpad `/` (far press off): the far grooves go within a few seconds; on again: they come back. Screenshot one
   pair at 100 m.
5. Drive back into the loop at ~15 m/s, camera behind the truck: does the groove run on across the ring edge? What
   is left of the pop - only the 16 cm surface step + finer detail? Short video if possible.
6. Soldier: walk onto the far track, go prone, free camera 50 m away: how much float over the groove?
7. Fire a mortar (or use the crater tool) on a far pressed stretch: crater looks as before, no lip / spike at its rim,
   no press inside it.
8. Second truck (or the robot, MUD_TestKit) driving 150-250 m away while you watch: its track appears as a groove
   behind it; any patch blinking / rebuild flicker?
9. Numpad `-` (mesh cap 1500). Drive the loop once more at 10 m/s, then drive away ~80 m and come back: are the
   revisit pops gone on your own loop? Screenshot the ring edge with cap 600 and 1500.
10. Log at the end: `MUD far press 10 s` lines (marks, made, rebuilds, verts pressed, soil verts, border-faded,
    stored meshes up to ~1500 with the alt cap), 'MUD world' patch count, 0 script errors / asserts.

Pass = 3 shows both stretches at 100 m, 5 shows the track continuing across the ring, 7 clean, no holes in 3 / 8.
Then the DS cost session from the report (Smallest tests, dedicated server part, >= 6 runs per setting) measures
far press on / off and cap 600 / 1500 for real.
