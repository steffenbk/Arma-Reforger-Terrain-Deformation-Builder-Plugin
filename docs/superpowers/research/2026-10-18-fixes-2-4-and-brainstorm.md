# Fixes 2-4 DS run + brainstorm: is the all-offroad mud sheet the right tool? (2026-10-18)

Build v201 (build ahead, speed radius, tile cheap, z split, per-frame print removed). DS run 14:59.
Logs: PeerPlugin1/logs/logs_2026-10-18_14-59-55 (client), server/logs/logs_2026-10-18_14-59-53,
CSV PeerPlugin1/profile/MUD_prof_28369.csv. 0 script errors both sides. User: "still kinda bad fps", tiles still seen
loading.

## 1. Measured (client)

5 s blocks from the CSV (rows cut at 5000 ms of dt; top parts, ms per frame):

| block | fps | MUD ms | top parts |
|---|---|---|---|
| 1-3 (standing) | 134-141 | 0.07 | - |
| 4-6 (slow) | 87-105 | 1.0-1.8 | sh zones 0.9-1.5 |
| 8 | 67 | 7.64 | sheet 4.88, sh zones 2.07, sh upload 1.54, ahead 1.19, z mesh 1.10, z ground 0.82 |
| 10 | 61 | 9.94 | sheet 6.37, sh zones 2.54, sh upload 1.67, ahead 1.52 |
| 13 | 47 | 11.97 | sheet 8.14, sh zones 3.06, sh upload 2.22, z mesh 1.68, sh stored 1.23 |
| 16 | 46 | 13.02 | sheet 8.68, sh zones 3.10, sh upload 2.37, ahead 1.39, sheet sync 1.35 |
| 19 | 50 | 12.82 | sheet 8.23, sh zones 2.91, sh upload 2.04, sheet sync 1.68 |

Fast-driving minute (CSV t = 40-100 s, 3542 frames), ms per frame: MUD 8.92 = sheet 5.72 (sh zones 2.12, sh upload
1.57, sh stored 0.82, sh wheels 0.62, sh follow 0.48, sh grounds 0.08) + ahead 1.08 + sheet sync 0.93 + rebuild 0.47 +
look ruts 0.26 + vox 0.20 + free+net 0.03. Checksum: parts 8.69 of 8.92.

Tile build split (frames with sh zones > 0, 2431 of 3542): sh zones 3.09 = z ground 1.21 + z mesh 1.67 + z data 0.10 +
z spawn 0.01. So: ground reads ~40 %, mesh ~55 %, mud data ~3 %. TILE CHEAP saved little - the data part was never big.

'MUD sheet:' 10 s lines (builds = every BuildTile call, zone + stored + live):

| time | touched | builds / 10 s | mean ms | to build | build radius m | speed m/s |
|---|---|---|---|---|---|---|
| 15:01:12 | 89 | 1302 | 1.66 | 38 | 37.7 | 8.5 |
| 15:01:22 | 193 | 1281 | 1.65 | 11 | 32.3 | 9.9 |
| 15:01:32 | 263 | 1489 | 1.66 | 34 | 28.4 | 11.3 |
| 15:01:42 | 319 | 790 | 1.70 | 14 | 24.3 | 13.2 |
| 15:01:52 | 420 | 1440 | 1.66 | 0 | 22.5 | 14.2 |
| 15:02:02 | 544 | 1555 | 1.63 | 96 | 29.5 | 10.8 |

Run 14:31 (before the fixes), driving: 1295-1567 builds / 10 s, mean 1.59-1.62 ms. Same build count -> fixes 2-4 did
not cut the work; the build list stays short (0-100), so build order matters little. fps at speed 46-69 (was 40-55
per user, not the same route - one run each).

PROVEN (this run): at 10-14 m/s on fresh offroad the mod script takes 9-13 ms a frame, about 55 % of the frame. The
tile builds are only ~3 of it; the rest is making the sheet ruts on fresh ground (live upload, stored rebuilds, sync
layer, ahead). All of it grows with speed.

## 2. How others do it (online, 2026-10-18)

- MudRunner / Spintires (developer write-up): the map is 16 x 16 m blocks; the dense mud grid exists ONLY in blocks
  that have mud, the rest is the plain low-res terrain; a mask says where mud replaces terrain and blends the edge;
  mud physics runs only in blocks near the player's truck; the shape is moved on the GPU (vertex shader reads a
  height texture). Tracks are projected decals.
  https://www.gamedeveloper.com/programming/mud-and-water-of-spintires-mudrunner
- Batman Arkham Origins snow (GDC 2014): footprints drawn into a height texture at runtime; PC uses GPU tessellation,
  consoles fake depth with relief (parallax) mapping. https://gdcvault.com/play/1020379/Deformable-Snow-Rendering-in-Batman
- Same render-target + tessellation idea in UE4 tutorials. https://www.kodeco.com/5760-creating-snow-trails-in-unreal-engine-4
- Nothing found for runtime terrain deformation in Enfusion (search 2026-10-18).

Every one of these does the shape on the GPU. We cannot (no custom shaders, no in-place vertex updates, no terrain
height API - proven earlier, see 2026-10-18-sheet-alternatives-and-perf.md sec 4). What we CAN copy: dense mud only
where it is needed, plain ground elsewhere.

## 3. "Load everything up front" - why not (estimate, not measured)

A tile mesh is 21 x 21 verts + skirt for 9 m2, ~1.7 ms to build. Arland offroad is tens of km2: 10 km2 = ~1.1 million
tiles = ~30 min of building and many GB of meshes. Not possible. A bigger ring at the loading screen (e.g. 300 m
round the spawn, ~17 k tiles, ~30 s) only helps until you drive out of it (~20 s at 15 m/s), then it is the same as
now.

## 4. Options

| # | idea | what changes | fps / pop-in | effort | look change |
|---|---|---|---|---|---|
| A | SPARSE SHEET (the MudRunner way): sheet tiles only where mud was touched (sync tile) plus round the wheels; plain ground patches everywhere else, sheet edges fade to the ground | no approach builds on untouched ground at all | removes the ~3 ms tile tax and ALL pop-in on untouched ground | medium | untouched ground is not lifted 16 cm (it looks the same otherwise) |
| B | Shape into the ground patches (old option A, 2026-10-16 doc): press + ridges on the heightfield patches, no 2nd mesh | one ground; patch rebuild ~1 ms per 64 m2 vs sheet ~1.7 ms per 9 m2 | cheapest per m2; no sheet at all | high | flat-ish rut floor at the 0.25 m grid |
| C | Engine track decals (TrackDecal, as vanilla tyres) for the wet mud look, geometry from A or B | engine draws the tread / wet look | ~0 script | low-medium | no depth by itself (parallax only) |
| D | Rut making cheaper at speed: fewer stamps / bigger op steps over 8 m/s, live upload slots capped | the other ~6 ms | fps at speed | medium | ruts coarser at speed |
| E | Coarse far LOD (0.6 m cells past 20 m) | fewer verts far | some | high | LOD switch pops |

Suggested: A first (biggest win, keeps the sheet where it matters, client-only, sync layer unchanged), then measure;
D next for the rut-making share. B is the long-term "one ground" route if the sheet still costs too much.

## 5. Deeper look per option (user 2026-10-18: "go deeper before we pick")

Baseline to beat (MEASURED, deep dive table 2.1, ~9 m/s): zone run MUD 5.40 ms / 77 fps; all-offroad 10.52 ms / 55 fps.
In the zone the sheet's rut work alone (sync 0.77 + follow 0.33 + wheels 0.49 + upload 1.27 + stored 0.68) = 3.54 ms.

### A. Sparse sheet
- How: `HasTile` / `TileInZone` for radius builds = "covered AND touched" (a sync tile under it, or an own stamp).
  Untouched tiles get no sheet; the heightfield patches already draw there (Covers answers "stored tile no mesh" ->
  not covered -> patch mesh shown, code read 2026-10-18). Sheet sides next to an untouched tile fade over EDGE 1 m
  (same code path as zone edges today).
- Gain: sh zones (~2.9 ms at 9 m/s) mostly gone; no pop-in on untouched ground (nothing to load). Expected = about the
  zone run: ~5.4 ms, ~77 fps at 9 m/s (INFERENCE: zone run is the closest measured case).
- Look risk: untouched ground today is the sheet lifted THICK 0.15 m; the patches are at ground level. So a fresh
  rut on new ground shows a sheet patch whose top is 0.15 m above the ground round it, faded over 1 m - a low mound
  appearing round the tyre. Two ways out, user picks after one look test:
  A1 accept it (reads as pushed-up mud); A2 draw the patch meshes THICK higher (colliders unchanged) so untouched
  ground matches the sheet top as now - needs a fade where patches meet real terrain (roads, buildings).
- Effort: medium; client-only; sync layer and hashes unchanged (MP-safe).

### B. Shape into the ground patches (no sheet)
- How: the rut field patches already rebuild their mesh after every rut op (rb mesh). Give that mesh the sheet's
  shape rules (tyre-width press, soft walls, ridges) instead of a 2nd mesh on top.
- Gain: removes ALL sheet parts (~3.5 ms rut work + tile tax); one ground, drawn = felt; no new per-tile memory.
- Cost: patch rebuilds get bigger; grid 0.25 m (vs sheet 0.15 m) keeps ~90 % of the ridge (offline 2026-10-16).
  Loses the "tyre sinks into soft mud" look and the mud smear (UV drag).
- Effort: high - a new look pipeline; the 2026-10-16 options doc has the design.

### C. Engine track decals
- How: vanilla `SCR_VehicleTrackDecal` way, one track per wheel, mud decal material (tread, wet, parallax).
- Gain: near zero script cost; adds the wet / tread look to A or B.
- Unknown (needs a probe): does a track decal stay on our script-built MeshObject ground? World.CreateDecal goes
  when the mesh is rebuilt (IDA 2026-10-16) - every rut op rebuilds patches, so it may vanish. One short WB probe.

### D. Cheaper rut making at speed
- Where: upload 1.27, stored 0.68, sync 0.77, wheels 0.49 ms at 9 m/s, all growing with speed.
- Ideas: fewer stamps per op over 8 m/s; stored rebuild at most every 0.3-0.5 s per tile; live upload slots capped.
- Gain: some of the ~3.5 ms (unmeasured split per idea). Ruts coarser at speed. Works with A.

### E. Far LOD
- With A, only touched tiles exist, so this is mostly moot. Skip unless A leaves big touched areas in view.

### Must-do for ANY sheet route before shipping
- Sync tiles are never freed: ~12 new tiles/s at 10 m/s = ~135 KB/s, ~0.5 GB per hour of one truck on fresh ground on
  every client and the DS (INFERENCE, deep dive 4.2). Needs a cap / heal-over-time. B does not have this problem.

### "Load everything" with A
- Untouched ground needs no load at all. Only touched tiles (where someone drove) are built on approach, and they are
  few. So "a bit more loading time" becomes: build the touched tiles near a late joiner at join.

Suggested order: A with a look test (A1 vs A2), then D; C as a side probe; B only if A + D are not enough.

## 6. Built: A + A2 (user 2026-10-18 'do A with A2'; v202 = before, NOT COMPILED)

- MUD_MudSheet: SPARSE true, PATCH_GAP 0.03; PatchLiftOn() = ENABLED && SPARSE && SYNC && ALL_OFFROAD; QueueNear
  builds only SheetWanted tiles (covered AND MUD_MudSync.AnyTileNear). HasTile (fades) unchanged = covered ground, so a
  built tile next to untouched ground does not fade (the lifted patch meets it 3 cm lower; skirts hide the step).
  LiveTile + LiftDirtyWindow (Follow: tiles that left / came rebuild their patches). Log: 'sparse true' on 'MUD sheet:'.
- MUD_WorldGroundEntity (mesh only, client only - m_bRender): LiftSetup / LiftAt in BuildMesh + BuildMeshBand,
  lift = (THICK - PATCH_GAP) x m_aLock[v] (0 at kept terrain / buildings), 0 over a sync tile or a live window tile.
  Colliders untouched. SheetLiftDirty(x, z) -> MarkLookDirty (mesh-only rebuild) for one 3 m tile.
- MUD_MudSync: HasTile; TileOf calls SheetLiftDirty when it makes a tile; snapshot load calls it per tile.
- Sheet ground traces hit colliders / data only (TraceFlags.ENTS + physics layer, HoleGround) - the lifted mesh does
  not feed back into the sheet (code read).

Risks to check (inference):
1. Patches are no longer hidden under a sheet that covers everything (lazy Covers needs every tile built) -> patch
   mesh rebuilds after rut ops come back on the client (rb mesh cost up).
2. Seam where the ground patches end and the coarse skin starts (skin not lifted): a 12 cm step far away.
3. Own predicted stamps that no op takes, on a trail tile after the window left: lifted patch could poke through.
4. A 3 cm step at a built tile's open side onto lifted ground (skirt covers the gap).

## 7. D: cheaper rut making (built 2026-10-18, NOT COMPILED, v203 = before)

Why: A2 DS 15:26 (0 errors) - sparse works (sh zones ~0.03 ms) but fps 58-72 driving, MUD 4-9 ms a frame. Biggest
part = sh upload 1.2-2.0 ms (~50 uploads / 5 s, 438-604 ms / 5 s, 346-531 slot builds put off), then ahead 0.5-1.35,
sheet sync 0.3-1.2, rebuild 0.3-1.6. The live window slots under the wheels rebuild at every 0.1 s upload.

IDA (Experimental Diag exe): script System.GetTickCount = sub_140E6F880, high-res counter but whole ms; elapsed =
floor(now exact - start ms) -> averages over many calls are unbiased, single readings coarse. No finer script clock.
Earlier IDA + measurements: MeshObject.Create ~0, UpdateIndices + UpdateVerts 0.24 ms per 1089 verts -> the cost is
our script loops, not the engine calls.

Built:
- D2 (MUD_MudSheet.BuildTileMesh, look unchanged - code read):
  - GAP_PROBE false: no 84-float array + m_mGapEdge.Set + 4 border compares per build.
  - COLLIDE false: cvv (441 verts) and cix (2400 ints) no longer filled.
  - With SMOOTH_EDGE on, 'clean' comes from bd; the 4 Changed + 1 DirtQuad calls per quad (400 quads) before it were
    thrown away -> now only run when smooth is off.
  - MUD_Prof parts t setup / t quads / t skirt / t engine (inside every tile build: live, stored, zones).
- D1 (UploadPick / Upload): SLOT_MIN_S 0.25 - a dirty zone slot that has a mesh and was built < 0.25 s ago is held
  (m_aPick 2) for a later upload, wheel slots too; slots without a mesh always build. m_aSlotBuiltMs moves with kept
  tiles on a window shift. Log: 'D1 slot passes held' on the 'MUD delay sheet 10 s' line.

Trade-off: a rut under a wheel can show up to 0.25 s later (delay meter on the same line shows it).

DS 15:37 (D, 0 errors): client fps 73-90 driving (15:26: 58-72), MUD 1.4-4.2 ms a frame (4-9); t quads 0.5, t setup
0.2, t engine 0.12 ms a frame. USER: 'a wave behind us, sheets appearing'. PROVEN by the delay meter: own wheel stamp ->
drawn tile avg 80 ms (15:26, 6 lines) -> 205-290 ms (15:37, 16 lines), 0.4 -> 0.7-2.6 m behind the wheel = D1 held the
wheel slots too (ruts came in 0.25 s chunks). FIX (NOT COMPILED): SLOT_MIN_WHEEL_FREE true - a held slot with a wheel
contact builds anyway (PickWheelSlots(heldOnly) 2 -> 3 -> 1); the hold stays for the other slots.
