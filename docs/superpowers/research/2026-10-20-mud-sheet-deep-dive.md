# Mud sheet deep dive: why it cost so much, why it popped, and what to test (research, 2026-10-20)

User 2026-10-20: "the mud sheet we had worked for the function it was supposed to do. But the issues were the script
calculation and that we can't cover the whole map, so we had ugly pop-in. Do some super deep research into those issues
and issues with the mud sheet, and find alternatives we should test."

Method:
- three read-only passes over `Scripts/Game/Mud/MUD_MudSheet.c` (6624 lines), `MUD_MudZone.c`, `MUD_MudSync.c` and
  `MUD_WorldGround.c` (build cost, tile lifecycle);
- a web pass on how shipped games do deformable mud and snow;
- a check of every earlier sheet doc (cost research 2026-10-19, whole-map issues, all-offroad deep dive, mud-sheet
  README, MudRunner notes).
I re-checked the key code claims myself: `UpdateSlots` dirtying all 16 slots (1408), the `Freeze` rebuild (1243), and
`LimitDrag` over the whole stamp box (4868).

Labels: CODE (read in the code, file:line), MEASURED (our logs), DOC (a developer's own talk or article), INFERENCE
(not tested). Lines are MUD_MudSheet.c unless named.

## 0. Short answer
1. **The sheet worked, and none of its problems are about its maths.** They come from two design choices plus a lot of
   avoidable work:
   - **Choice 1: a uniformly raised 15 cm layer.** A raised layer is a second visible ground. It looks seamless only
     where it covers everything, or where the ground around it is lifted by the same amount. "Cover everything" is the
     whole-map wall (~622,600 tiles, tens of GB). "Lift the ground around it" was the A2 / look-lift path, which hid
     the voxel ruts under riding grass and gave a two-owner seam (README lesson 7). **That one choice causes the pop-in
     AND the coverage problem.**
   - **Choice 2: the sync layer.** Bit-exact mud on every client and the DS, plus quantise, hash and snapshot. That is
     ~0.5-0.7 ms/frame per client, the same work on the DS, and memory that is never freed, all to keep a look-only
     layer identical.
   - **Avoidable work.** About 35-40 % of the live tile builds rebuild tiles where nothing changed. Most of the wheel
     stamp cost is one helper (`LimitDrag`) run 3-4 times over the same cells. Several per-frame loops grow with every
     vehicle ever seen.
2. **The fix for pop-in is to change what "untouched" looks like, not to build faster.** Mud thickness should come from
   distance to the cells wheels touched: full near a track, falling to zero (and slightly under the ground mesh) 0.6-1 m
   out. Then untouched ground needs no tile at all, and a tile being born, dropped or rebuilt is invisible. MudRunner
   works this way: mud geometry exists only where the truck deformed the terrain [DOC, Spintires 2014 / MudRunner 2017].
   This is the "deposit band" design (sec 3).
3. **The fix for cost is the code levers in sec 2, plus dropping the sync layer** (sheet local). Estimate
   [INFERENCE]: ~0.6-1.2 ms/frame per live truck after the levers, against 1.3-1.8 ms for 'sheet' in the old zone plus
   0.5-0.7 ms sync.
4. **Whole map then works in the sparse sense.** Mud is kept wherever trucks went, as compact data (~1-2 KB per tile),
   and meshes are built only near the camera under a budget. No pop, because a band rises from under the ground mesh.
   Real ruts cost ~900 tiles per km of track, so a long match stays in the tens of MB.
5. **Remote trucks are a real gap.** Today another player's truck only leaves sheet mud inside YOUR 12 m live window.
   Whether a client even gets wheel contacts for other players' trucks is unknown. Fix: stamp remote trucks from the
   replicated rut-op segments, read only. That is sync-lite: the same inputs, no hash, no quantise, no snapshot
   (sec 4).

## 1. Root causes, one by one

### 1.1 Pop-in
In order of how much they explain:
| # | cause | CODE | why it pops |
|---|---|---|---|
| 1 | Untouched sheet = a raised layer (ground + THICK 0.15 + LIFT 0.01) | `BuildZoneTile` 1974-2042, `MUD_MudSync.Get` default THICK (MUD_MudSync.c 336-341) | any new tile is a 3 x 3 m slab, 16 cm high, appearing in one frame |
| 2 | `HasTile` means "offroad", not "drawn" | 1302-1305, fade only toward non-offroad tiles 5805-5808 | no fade toward an unbuilt neighbour, so a hard 16 cm ledge plus a 0.45 m skirt at every build edge |
| 3 | SPEED_RADIUS: build radius 320 / speed, 20-40 m | `RadSpeed` 1809-1826 | the build edge comes closest exactly at speed, when pop is most visible |
| 4 | LOD double build: built LOW past 20 m, rebuilt FULL at 17 m at the end of a 3 ms/frame FIFO | 5713-5728, `LodUpgrade` 5680-5699 | every approached tile is built twice; the second build is a visible detail change |
| 5 | Partial live meshes: quads with untraced corners are skipped, and the slot is then held 0.25 s | `Grounds` 5066-5091 (150 traces/frame), 6114-6118, `UploadPick` 5313 | holes / strips for at least 0.25 s after a window shift; and `BuildTile` 5746-5748 tells the patch to drop its lift on ANY mesh, holes or not |
| 6 | MESH_CAP 600 against a ring of ~470 tiles plus turning circles | `DropFar` 1831-1877 | drop + rebuild churn (MEASURED 400-900 drops per 5 s) |
| 7 | Drop vs re-lift race: the sheet is deleted after 1 s, while the patch re-lift waits in a one-per-frame queue with a render gate | 828-838; MUD_WorldGround.c 4777-4829 | an unlifted patch 16 cm low for a moment, then it pops up |
| 8 | Easing frozen mid-slide when a slot leaves the window | `EaseTick` 2349-2352, `Freeze` 1223 | a tile a few cm too high, jumping when next traced |
SPARSE in code does much less than its name. It only stops the radius builder (`SheetWanted` 1370-1377). The live
window still draws all 16 offroad slots and freezes every one of them, touched or not (`Follow` 1077-1079). So a 12 m
band of untouched raised sheet follows every drive. Its edges, ~6 m each side of the track, are the best code-backed
explanation for the "driven / undriven step" that survived LIFT_BY_DRAWN. Other contributors:
- the patch lift is multiplied by the edge lock and the sheet's is not, so the step reaches up to 15 cm near roads and
  buildings (MUD_WorldGround.c `LiftAt` 6914-6927, `EdgeLock` 3167-3197);
- sheet UVs repeat every 12 m, while patch / skin / voxel-top UVs span 64 m: a 5.3x texture scale change at every
  sheet border (5811-5812, 6154 vs MUD_WorldGround.c 7199).
Also, SPARSE needs SYNC (`PatchLiftOn` 1353-1356). With SYNC false, `SheetWanted` is true for every offroad tile, so a
"local + SPARSE" build today is the whole-map test again.

### 1.2 Coverage (whole map)
- 5.6 km² of holed ground = ~622,600 sheet tiles. At ~60-140 KB per meshed tile (6 x 441-float data = 10.6 KB, plus 761-1024
  vertices x 28 B on the CPU, the same on the GPU, a second vertex copy for two-section tiles) that is ~37-87 GB. Build
  time is 6-30 min. CODE + MEASURED; impossible.
- This only matters because of root cause 1.1 #1: the untouched layer must be drawn to be seamless. With the deposit
  band (sec 3), untouched ground needs nothing, and coverage turns into "where trucks went".
- Track maths [INFERENCE from the stamp constants]: the stamped strip is ~2.6-3.1 m wide for one truck. Add the band
  and diagonal tile crossings, and it is ~900-1040 tiles per km of track. At ~1.3 KB of compact data per tile
  (sec 3.3) that is ~1.2 MB per km kept for the whole match. Meshes exist only near the camera (40-145 MB per km if
  ALL were meshed, which they will not be).

### 1.3 Script cost
Per frame, one truck, DS client (MEASURED): 'sh upload' (live builds) 0.4-0.84 ms, 'sh wheels' (stamps) 0.1-0.46 ms,
local slump an estimated 0.3-0.5 ms, sync 0.5-0.7 ms. One tile build is 0.88 ms (setup 0.2, quad loop 0.5, skirt 0.03,
engine 0.12).
New findings (CODE):
- **~55 builds/s, of which ~35-40 % are not needed.**
  - `UpdateSlots` marks all 16 slots dirty on every window shift (1408). At 10 m/s that is every ~0.3 s, although 12
    of the 16 keep valid meshes (entities sit at world tile corners) and the 4 entering ones come with their own meshes.
  - `Follow` forces an upload at once (1195).
  - `Freeze` then rebuilds leaving slots that are "dirty" only because of that (1243).
  - The always-picked "longest waiting" slot spends its pick on those marks (5349-5352).
  - Per 0.1 s upload: ~3.2 wheel slots (needed) + 1 longest-waiting + ~1.3 Freeze + 0-0.5 nearest = ~5.5.
  - Only ~10-20 % of rebuilt vertices actually changed.
- **Stamp cost is mostly LimitDrag.** It runs over the stamp's whole axis-aligned box (~190 cells), 2 sweeps x 4
  neighbours x 2 Clamp, on every stamp (4868, 4877-4911). That is ~70 % of the stamp's natives, and consecutive stamps
  overlap ~70 %, so each cell is limited 3-4 times. Also: 5-6 map operations per written cell for pass tracking
  (4766-4777), `Grow` that nothing reads under SYNC (4864), a `Math.Sin` always thrown away (4807), and `PrunePasses`
  walking ~7,000+ map entries every 5 s (4253; likely a 1-2 ms hitch).
- **The build rebuilds identical data each time.** For an untouched, hole-free tile only the heights change. XZ, base
  UVs (16 variants), the full index list and the skirt topology are the same every build. The vertex order is a quad
  walk through tmap instead of fixed row-major, so the indices only look data-dependent. Persistent holder arrays are
  proven fine with `UpdateIndices` / `UpdateVerts` in this repo: MUD_GroundEntity.c 1342/1638, MUD_CoarseSkin.c 748,
  and gotchas 415 (the engine copies them). The comment at 5815 ("member holder crashed") predates the IDA finding
  that the crash was indices past the vertex count (gotchas 432-435). Two duplicate 441-cell scans per build:
  BuildTile 5739 and Upload 5282.
- **Natives are probably not the main cost.** The stamp rate (~180 stamps/s per truck against 9-41 ms/s 'sh wheels')
  puts one native at tens of ns. Most of a build is plain VM loop work (~5,600 iterations). So "purge the natives"
  (cost research 3.1) is a smaller lever than "do fewer builds and fewer iterations" [INFERENCE; the sec 9 bench
  decides].
- **1.5 m live tiles probably do not pay.** One wheel changes ~1.4 x 0.6-1.1 m per 0.1 s upload. Counting skirts,
  3 m tiles give ~2,660 vertices per upload; 1.5 m tiles give ~3,370 [CODE constants + INFERENCE]. This corrects the
  earlier ~-45 % estimate.
- **Things that scale badly with vehicles:**
  - `MUD_Wheels.GetAll()` includes parked vehicles;
  - `m_aVeh` is searched linearly per wheel call and never pruned (4069-4085), and `PickWheelSlots` / `UploadPick`
    loop over all vehicles too;
  - `TakeCovered` walks every own stamp on every op from any vehicle (4323-4410).
- **Smaller items:**
  - `EaseTick` runs every frame and marks slots (800, 2342-2373), not in any profile bucket;
  - `RestoreBox` writes and marks a whole op box with no compare (4537-4557);
  - `m_aOneBuild` uses `RemoveOrdered(0)` + `Contains` (O(n²), 745 / 2674);
  - each shift copies 6 x 6,561 cells and allocates stores (1093-1196, ~140 KB/s of garbage at 10 m/s).

## 2. Cost levers (ranked; all look-neutral unless noted)
| # | lever | where | saving [INFERENCE] |
|---|---|---|---|
| L1 | carry each slot's dirty flag through Follow (no blanket dirty), so Freeze builds only real changes; no forced upload unless a slot really changed | 1186 / 1408, 1071-1093, 1195, 1243 | -27-40 % of live builds, ~0.15-0.3 ms/frame |
| L2 | LimitDrag once per upload over the union of stamped cells + 1 (offline: -64 % drag work, results 2026-10-19), inline clamps | 4868, 4877-4911 | -50-75 % of 'sh wheels', ~0.05-0.3 ms/frame |
| L3 | slump only on cells that changed since the last slump + 1 ring, one pass at 0.6 (offline ≈ 3 passes at 0.25), no per-cell MarkRect | 4961-5062 | most of the local slump's est. 0.3-0.5 ms |
| L4 | build rewrite: row-major vertex order, static XZ / UV / index / skirt holders (holder class, the method is at 63 / 64 locals and ~71 KB stack), no duplicate scans, lazy eH / eV; drop the dead cvv / cix arrays (COLLIDE false) | 5802-6490, 5739, 5282 | -25-35 % per touched build, -55-65 % per untouched |
| L5 | compare against the last-built data before building (skip if max change < 2-3 mm, same bare / dirt class); RestoreBox and slump mark only what changed | 5248, 4537-4557, 5052 | -5-15 % of builds |
| L6 | EaseTick at upload rate, not every frame | 800, 2342 | 0.1-0.3 ms/frame on repeat passes |
| L7 | pass tracking as per-cell arrays in the window field instead of 3 maps (no prune hitch); skip Grow when nothing reads it; remove the dead Sin | 4766-4777, 4253, 4864, 4807 | -20-35 % of the rest of Stamp + a 1-2 ms spike every 5 s |
| L8 | vehicle loops: distance pre-check to the window, cached sim component, map by vehicle, prune m_aVeh | 4069-4085, 5404 | grows with vehicle count, 0.05-0.2 ms/frame at 50+ |
| L9 | share skirt vertices along each side (320 -> 168); O(1) build queue; reuse stores on shift | 6323-6377, 745 / 2674, 1234 | -20 % mesh memory, smoother shifts |
| L10 | drop the sync layer (sheet local) | SYNC false + couplings (mud-lab plan sec 2) | ~0.5-0.7 ms/frame per client + the same on the DS + unbounded memory |
Before coding L1-L5, add one 10 s log of builds by reason (wheel / oldest / nearest / mesh-less / Freeze /
restore / slump) plus "would skip: max change < 3 mm". It confirms the avoidable share in one DS session.

## 3. The deposit band: the design that removes pop-in
### 3.1 Rule
Thickness per vertex = THICK x band(dist), where dist = distance to the nearest cell any wheel has touched (stamped
or pressed). band = 1 within BAND_IN (start 0.4 m), then a smooth falloff (sine, like the taper) to 0 at
BAND_IN + BAND_FADE (start 0.8 m). At band = 0 the surface goes ~1 cm UNDER the drawn ground, so the sheet edge comes
out from under the patch mesh: no step, no z-fight. Quads whose four corners are all under the ground are skipped
(they are already degenerate-skipped, 5893-5900).
- Untouched ground has no tile, so there is nothing to pop.
- A tile is born when a stamp touches it. At that moment only the band around the stamp rises, under / beside the
  tyre. That reads as mud pushed up, which is what ridges are.
- The tile-side EDGE fade and `HasTile` play no part. The band is computed in world cells across tile borders, the way
  the taper field already is. So there are no staircase outlines, and no neighbour cliff when a tile is born. A tile
  that changes a neighbour's band within BAND_IN + BAND_FADE marks that neighbour, which is a few cells.
- The look: a soft mud band 1-2 m wide along each track. The groove is pressed into it (15 cm visible depth from the
  first pass, as the old sheet had), with ridges and bow wave on top. The band fades into the grass. Untouched ground
  is the patch, exactly as today, so the voxel ruts there are unchanged.
- What it gives up: the "whole area is a mud field" look of a full zone sheet. If you want a field to be mud before
  anyone drives there, give that area a pre-touched mask (the old zones could be that mask), built once.

### 3.2 Why this is not D1 / D2 again
D1 clamped the surface to the felt rut floor (no own depth), and D2 drew the layer with the patch meshes at the patch
pace. The deposit band keeps the sheet's own raised layer, its own stamp + slump simulation at 0.15 m, and its own
10 Hz live rebuild: all seven look parts (research/2026-10-20-mud-look.md sec 2). Only WHERE the layer exists changes.

### 3.3 Whole map, stored compactly
Per touched tile, keep data only (no mesh) when it is far away:
- heights as 1 byte per cell (0-0.3 m in 1.2 mm steps), drag u / v 1 byte each, bare + dirt class in 1 byte;
- 441 cells x 4 B = ~1.8 KB, against 10.6 KB of floats today.
Rebuild meshes near the camera only: a fixed 60-80 m radius, no SPEED_RADIUS, nearest-ahead first, ~2 ms/frame
budget, no LOD double build. A far ring past ~40 m can use 11 x 11 vertices, built once when it enters the ring and
never upgraded on screen. Because a band rises from under the ground mesh, a mesh appearing at 60 m is just a low berm
showing up far away, not a slab.
Cap the data at (say) 20,000 tiles = ~36 MB and forget the oldest far tiles first. The voxel ruts stay there, so the
ground keeps its shape.

### 3.4 Other pop fixes still needed
- Never build an offroad slot until all its cells are traced, and tell the patch nothing (no patch lift in this design
  at all).
- Finish eases in `Freeze`.
- Give the sheet the patch's 64 m UV frame (as voxel tops do) so the texture does not change scale at the band edge.

## 4. Several trucks and MP
- **Today's gap.** `Stamp` writes only into the live window field (4693-4718), and the window follows a vehicle within
  8 m of the local player (996-1016). So other players' trucks leave no sheet mud outside your own 12 m window. Whether
  a client gets `WheelHasContact` / contact positions for proxy vehicles is unresolved; `MUD_LookRuts` counts
  `s_iNoContact` for exactly this.
- **Option A, local stamps for every vehicle in range.** Stamp into tile data (not only the window field) for every
  vehicle within STAMP_R. This depends on proxy wheel contacts (probe first).
- **Option B, sync-lite (recommended).** Every rut op the client receives already carries the truck's path segments
  (the kind-4 rut op the voxel ruts are made from). A read-only listener on `MUD_MudSheet.FloorChanged` / the op
  stream stamps those segments into the local sheet data with the same Stamp maths:
  - no hash, no quantise, no snapshot, no DS work;
  - close on every client, not bit-identical (the look is allowed to differ a little);
  - own truck: live prediction as today, its own ops skipped by vehicle id;
  - remote trucks: mud appears at the op pace (~0.75 s behind the truck).
  - A late joiner gets the rut field in the snapshot and can stamp the far tiles from it (D1's fallback idea), or simply
    sees voxel ruts there.
  Read-only on the rut side: no change to ops, op format or Submit.

## 5. What the other games do (web pass; most page fetches were blocked, so many claims come from search summaries)
| technique | who [label] | fits us? | takeaway |
|---|---|---|---|
| mud geometry only where deformed; base terrain masked there; active blocks near the truck; physics and look data separate | Spintires 2014, MudRunner 2017 [DOC] | yes, the architecture | = the deposit band + sheet local + voxel ruts for feel |
| ridges as a MAX shape and the press as a MIN shape, no volume kept | MudRunner [DOC] | yes | our PUSH already does this; volume-kept berms were tried (v1, checkpoint v047) and came out 2-3 cm high, invisible |
| tracks as a strip of trapezes along each wheel, newer tracks drawn on top | MudRunner [DOC] | partly (no parallax pass) | ribbon chunks: only the head chunk rebuilt (R4) |
| render-from-below ortho camera into a height map, relief mapping on consoles | Batman: Arkham Origins [DOC] | maybe (N2 probe) | `$renderview` + an orthographic camera exist in the API |
| berm built from the dip itself (profile x depth) | Rise of the Tomb Raider [DOC] | yes | cheap ridge profile for ribbons / far LOD |
| store the result per zone, not the edit history; build budgets + hysteresis | Valheim patch 0.150.3 [community] | yes | sec 3.3 compact data |
| pre-authored wear states | DiRT Rally 2.0 [PRESS] | yes | premade pieces / variants (R5) are a shipped approach |
| tessellation / compute snow | God of War, RotTR, RDR2 | no | GPU only |
Sources:
- https://www.gamedeveloper.com/programming/mud-and-water-of-spintires-mudrunner
- https://www.mudrunnermods.com/the-mud-of-mudrunner/
- https://colinbarrebrisebois.com/wp-content/uploads/2022/06/gdc2014-deformable_snow_rendering.pdf
- https://www.elopezr.com/the-rendering-of-rise-of-the-tomb-raider/
- https://github.com/74oshua/BetterTerrain/blob/master/README.md
- https://www.gtplanet.net/dirt-rally-2-0-track-surfaces-will-degrade-with-every-run/
- https://www.cs.purdue.edu/homes/bbenes/papers/Benes04WSCGa.pdf

## 6. Alternatives to test, ranked (what each fixes)
| # | test | fixes | chance | cost to build |
|---|---|---|---|---|
| T1 | **sheet local + deposit band** (sec 3) + L1-L7 + compact far data | pop-in, coverage, most of the cost | high: same look code, only where / when changes | medium (MUD_MudSheet changes) |
| T2 | **sync-lite remote stamps from rut ops** (sec 4 B) | other players' trucks leave mud; close on every client | high | small (a read-only listener + Stamp into tile data) |
| T3 | **far LOD ring + compact data** (11 x 11 past 40 m, built once) | whole-map memory, far view | high | small-medium |
| T4 | the cost levers alone on the old zone sheet, measured by reason (L1-L8, the log first) | proves the cost model before T1 | high | small per lever |
| T5 | ribbon chunks with a berm profile as the FAR / cheap representation (R4, head-only rebuild) | far tracks for almost nothing | medium | medium |
| T6 | render-target paint (N2 probe) as wet / dark tracks or detail on top of T1 | fine detail without vertices | low-medium (material slot unknown) | probe only |
| T7 | wheel-attached bow-wave mesh (N1) to carry the live look if T1's live builds stay too costly | live cost | medium | small |
Dropped or demoted by this pass:
- 1.5 m live tiles: the vertex count says no gain at 10 m/s;
- S2 volume-kept berms: already tried, invisible;
- the native purge as a headline: probably 10-25 % of a build, not most of it;
- stored skinned tiles (per-frame palette).

## 7. One DS session that answers the most (after T4's log is in)
1. Old zone sheet, levers off, then on (L1, L2, L5, L6): builds by reason, 'sh upload' / 'sh wheels' / slump, at least
   6 runs each. Confirms the cost model.
2. Sheet local + deposit band, BAND_IN 0.4 / 0.8 x BAND_FADE 0.8 / 1.2. By eye: no pop on birth, no step at the band
   edge, the groove depth from pass 1, grass on the band. Numbers: MUD ms, tiles kept, data KB.
3. A second truck driven by another client: do its tracks get mud (T2), and how far behind it?
4. Drive away 200 m and come back: far ring builds, no pop, the band shows from far away.
Kill T1 if: a band birth is still visible as a pop from 5 m, or the cost while driving is over ~3 ms/frame over 'off'
after L1-L7.
