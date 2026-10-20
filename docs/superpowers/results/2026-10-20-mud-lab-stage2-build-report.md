# Mud lab stage 2 - build report (2026-10-20)

Prompt: docs/superpowers/prompts/2026-10-20-local-mud-lab-stage2.md. Branch claude/upbeat-lovelace-q0w022.
Commits: 28d518d (part 0), e94d59d (A), 4766541 (B), 1b61f33 (C), this commit (D). Checkpoints v255-v258.
**NOTHING IS COMPILED YET** (MCP wb_reload cannot recompile; the user reloads in the Script Editor).

## Switches (all in MUD_MudSheet.c, all default OFF)

| Switch | Part | What it does |
|---|---|---|
| EDGE_COBUILD_PICK | A2 | dirty band / stamp slots beside a picked wheel slot build with it |
| EDGE_COBUILD | A3 | a build whose border differs from the tile beside builds that one too (CO_MAX 4 stored per upload, rest queued) |
| SKIRT_APRON | A4 | skirt bottom verts go out ~45 deg down to under the ground, own UVs |
| EDGE_DRAPE | A7 | (added after the first DS run, user idea) the skirt curves over and down as a quarter round; wins over A4 |
| BAND_BORN_FRONT | A5 | a tile born / re-banded outside the window builds before the radius list |
| CRATER_TRACE_NOW | B2 | a now-slot not traced yet is traced / filled and built in the go-live frame |
| CRATER_TAPER_NOW | B3 | the hole's taper tiles build in the go-live frame (cap TAPER_NOW_MAX 12 / TAPER_NOW_MS 15) |
| CRATER_EASE_DROP | B4 | a dig drops the running ground ease of every vertex it writes |
| BAND_INCR | C1 | band from a kept distance field, updated only round new touched cells |
| BAND_MICRO | C2 | BandTile reads the field directly, if-compares, LUT instead of cos |
| BUILD_SKIP_SAME | C3 | a rebuild that moves no vert 3 mm with the same classes keeps the old mesh |
| UV64_FAST | C4 | TOP_UV64 UVs without the x1e6 round trip through cix and without the restore loop |
| BAND_STORED_HOLD | C5 | band-only stored rebuilds obey SLOT_MIN_S |
| EDGE_AUDIT (const) | C6 | still **true** - see deviations |

Second cycle: **numpad \*** (TEST_KIT, sheet local on) steps fix sets 0-8 (MUD_MudSheet.FixApply / LabFixStep). Log line
`MUD sheet fixes: set N: <names> | why`. Sheet local off or a new play = set 0.

| Set | Switches on |
|---|---|
| 0 | none |
| 1 | A2 |
| 2 | A2 + A3 |
| 3 | all A (A2 A3 A4 A5) |
| 4 | C1 + C2 |
| 5 | all C (C1-C5) |
| 6 | all A + all C |
| 7 | all B (B2 B3 B4) |
| 8 | everything (with the apron, not the drape) |
| 9 | A2 + A3 + A7 drape + A5 (set 3 with the drape instead of the apron) |
| 10 | set 9 + A8 open fade |

B1 (crater log, MUD_CraterTrace.c) has no switch: it is TEST_KIT only and print only.

## New log parts
- 10 s `MUD sheet builds:` line: cobuild counters (A6); `band incr: passes, box cells, slots dirtied, slot BandTiles` (C1 on);
  `skip same: kept old mesh N` (C3 on); `stored band builds held N` (C5 on); edge audit (EDGE_AUDIT).
- Per crater: `MUD crater seq S: boom ... | recv ... | golive ... | sheet: ... | taper front ... | eases in box ... | tiles ...`.
  Server: `MUD crater srv: seq S at P kg K | hit->issue a ms`. 10 s lab line: `| craters N: ...` (MUD_CraterTrace.Part10).
- Once per play with BAND_INCR on (TEST_KIT): `MUD band incr bench: 1000 stamps ... | max |incr - full| band X ... within 1e-4: true/false`.
- `MUD sheet taper now: cap hit after ...` when B3 hits its cap.

## Lines touched in existing files (current line numbers, vs 8eb5bcf)
- MUD_VoxelProbe.c: 1658, 3648, 4068, 6785 - one guarded `MUD_CraterTrace` stamp each (Receive, Issue, SubmitBody, GoLive).
- MUD_WorldGround.c: 6035 - one guarded `MUD_CraterTrace.SrvHit` before Crater().
- MUD_WarheadComponent.c: 24 - one guarded `MUD_CraterTrace.Boom`.
- MUD_MudLab.c: 206, 209-216, 223, 234-235 (numpad \* cycle, 10 s line parts).
- MUD_MudSheet.c: 351-415, 428-429, 447, 947-954, 988-989, 991, 999, 1045, 1047, 1074, 1077, 1133, 1136, 1264-1265,
  1381-1389, 1414-1417, 1484-1485, 1488-1489, 1573-1575, 1669-1672, 1679-1680, 1693-1716, 1776-1780, 1858-1860,
  1880-2114, 2117-2118, 2124-2553, 2579, 2632-2633, 2692-2707, 2711-2712, 2729-2812, 2928-3080, 3083-3084, 3086, 3088,
  3090-3153, 3155-3156, 3158-3165, 3167-3168, 3170-3224, 3235-3246, 3251-3261, 3277-3344, 4292-4295, 4737, 4744-4754,
  5170, 5173, 5213-5215, 5219-5220, 5831-5832, 6424, 6427-6428, 6449-6450, 6540-6543, 6601-6602, 6634-6635, 6706-6707,
  6709-6710, 6717-6718, 8000-8004, 8113, 8115, 8477-8482, 8494-8506, 8508, 8510-8513, 8517, 8523-8613, 9074, 9087-9096,
  9100-9101, 9138-9139, 9177-9229, 9351-9352, 9356-9363, 9509, 9784-9785, 9851-9874, 9916, 9932-9953, 9955,
  10182-10207 (the big ranges are new methods: band incr / bench / micro, crater now paths, co-builds, apron, SkipSame).
- New file: MUD_CraterTrace.c.

No rut-side behaviour changed: the 6 stamps are one `if (MUD_TestKit.TEST_KIT)` call each and only record times.
Nothing new runs on a DS except the server crater line (TEST_KIT, print only). Nothing writes J2, ops or snapshots.

## Deviations / my readings of the prompt
1. **Not compiled.** Expect a round of compile fixes on the first reload.
2. Fix switches are cycled by a **second cycle, numpad \***, not folded into the numpad 0 lever sets.
3. BAND_BORN_FRONT sits in set 3 with the other A fixes (it is an edge fix by its effect).
4. A1 also takes the band ms out of the SH_PRE and zone-data timers, not only upload / follow / zones.
5. SKIRT_APRON: the apron leaves small slits at tile corners (the two side aprons do not meet). Look check in WB.
6. CO_MAX counts stored neighbours built at once per upload; the rest go to the front of the stored queue.
7. B4: FreezeLocal is not changed. With eases dropped in DigNow there is nothing stale for it to store.
8. The crater line has an extra field `traced now n` (how many now-slots B2 traced).
9. Golive frame ms "swap" = live ms - vox ms - sheet ms (a remainder, not its own timer).
10. C1: compact far data does **not** keep distances. A tile coming back from far data gets one full BandTile (exact);
    stored (non-compact) tiles keep theirs.
11. C4: no member holder array (member holders passed to UpdateVerts crashed twice, BuildTileMesh comment). Instead the
    dirt section is uploaded first with its own UVs, then the top UVs are written over tuv in place and the top section
    goes up. Same result, no round trip, no restore loop.
12. C3: "classes unchanged" = one hash signature per tile over bare / hole, traced, band > 0, touched and untouched-top
    per vertex, plus the same LOD. UV drag is not compared, so dragged dirt UVs can lag one rebuild while C3 is on.
13. C5: a held band-only stored tile goes to the end of the stored queue (as A5 holds) and stays marked band-only.
14. C6: EDGE_AUDIT stays on until you confirm part A in WB (the WB session needs its numbers). Then I flip it to false.
15. D: LabCounts "built" now counts a live slot only when its entity is visible - a null build ("nothing to draw")
    hides the entity but used to leave the old resource counted.

## After the first DS run (2026-10-20 22:43-22:50)
- Compiled clean first try. Bench: max |incr - full| 0. Set 7/8 crater taper tiles done in the same frame to +3 frames
  (vs +5..+7 frames with set 0); sets 2+ took edge steps > 3 cm from 22 to 0 per 10 s. Left: 'open: no mesh' edges.
- Added A7 EDGE_DRAPE (set 9) for those edges. Meter: `drape cut at cap N` on the builds line.
- D1 overlay + D2 pressed lift taken out of the numpad 3 cycle and archived
  (mud_backups/archive/mud-overlay-D1-press-lift-D2-2026-10-20, README lists call sites). D2's PRESS_LIFT code stays dormant.
- WB 23:10-23:12 (set 9): user screenshots show striped dirt wedges beside tiles; edge audit 'open: no mesh' up to 92
  verts / 10 s (max 22 cm). Added A8 EDGE_OPEN_FADE (set 10): a side whose neighbour is not drawn (and not a live slot
  built in this upload) fades over EDGE (1 m) like the old open-side fade; A3 rebuilds it once the neighbour is drawn.
  Meter: `sides faded (beside not drawn) N`.
- WB 23:17-23:18 (set 10): user still sees the wedges clearly. Log: sides faded 171-790 / 10 s, steps > 3 cm 4-13,
  'open: no mesh' 114-118 (max 24 cm). A8 only moves the mismatch to the other tile until A3 rebuilds it.
- Added A9 EDGE_MATCH (set 11 = set 9 + match, no A8): a border beside a drawn tile copies that tile's drawn border when
  every vert is within EDGE_CO_STEP (3 cm), or keeps its own and co-builds the neighbour at once (no CO_MAX cap) so it
  copies ours; beside nothing drawn the raised border goes down to the band-0 height. The change ramps in over 4 vert
  rows (1, 0.75, 0.5, 0.25); corners stay. Meter: `edge match: sides copied N, moved + co-built N, faded N`.
  Checkpoint v260 = before. Not compiled.
- User 23:2x: set 11 = no more edges (checkpoint v261). Crater on the sheet still shows a little late. Set 11 has no
  B fixes. Added set 12 = set 11 + B2 B3 B4. DS log (sets 7/8, 22:47-22:49): with B on, the hole's core tiles all build
  in the go-live frame (sheet part 25-47 ms of that frame, ~2 ms per tile), taper tiles past the TAPER_NOW_MAX cap
  still come +1..+6 frames later. recv -> go-live is 50-120 ms (the voxel ahead build), the same off the sheet.
- WB 23:35 (measured, 7 craters): set 12 no faster than set 11 on the sheet (taper cap 6 tiles hit, rim +6..+8 frames,
  go-live frame sheet part 89-120 ms WB). Off the sheet that frame is 0-1 ms. recv -> go-live 110-280 ms both.
- Added B5 CRATER_PREBUILD (set 13 = set 12 + prebuild). TakeOp (Submit, before the carve) notes each crater op; next
  sheet Frame digs it ahead (QueueChange + DigNow) reading the voxel DATA uncached (PreSurf = TopAt + rut floor; the
  SurfaceAt cache holds the last go-live's tops, and traces would hit the old brick bodies that stay until SwapPending).
  Every build of a tile in the crater's reach + 1 tile is held (PreHeld / PreKeep in BuildTileMesh: no SetObject /
  visible change). Ahead frames build held now-slots + taper tiles PRE_MS (5) each. At the go-live (FlushNow) the
  crater's GroundChanged is skipped, the rest builds, then every held mesh goes on its entity in that frame. Moved tiles
  are rebuilt. Timeout PRE_MAX_MS 1 s. The shown mesh's Resource is kept until the swap (the callers drop their ref).
  Log per window: `MUD sheet prebuild: <why> after N ms (F ahead frames) | crater ops, builds held, built at close,
  shown, moved -> rebuilt, close ms`. Checkpoint v262 = before. Not compiled.
- WB 23:46 + DS 23:49 (set 13, user: "the whole game stutters when it explodes on the ruts, the vehicle stops for a split
  second"). Logs: prebuild close 5-26 ms, but the 5 s prof worst 'sheet' frame went to 92-94 ms (WB) / 60 ms (DS) =
  the untimed ahead dig (PreStep QueueChange + DigNow). Its PreSurf = TopAt (4 ColTop column scans) per sheet vert at
  0.15 m, ~11 verts per 0.5 m column, all uncached. The go-live voxel part also rose (WB 55-65 vs 15-26 ms, DS 34 vs
  17-25 ms) - cause not proven (no sheet call found in UpdateVox).
- Added B6 PRE_COL_CACHE (set 14 = set 13 + col cache): PreSurf reads each grid column's top once per frame (s_mPreTop,
  cleared every PreStep and every TakeOp), bilinear as TopAt; a corner without a cached top falls back to TopAt.
  Prebuild line now ends `| dig ahead N ms (worst frame N) | voxel top reads N, col cache 0/1 (misses -> plain read N)`.
  Checkpoint v263 = before. Not compiled.
- WB 23:56-23:57 (sets 13 / 14, user: still stutters). Set 14 cut the ahead dig 36-37 -> 15-27 ms (reads 2025-4097,
  0 cache misses). Go-live frame 50-66 ms (set 12 WB: 89-180). Correction: the go-live 'voxel part' did NOT rise with
  B5 - it is MUD_WorldGround DoVoxCells ('MUD world: voxel cells live' line) rebuilding the rut patches' collider + mesh
  in that frame, ~7-13 ms per patch, 0 ms where no rut patch is built (off the ruts). Same at set 0 (DS 21:16 / 22:44:
  10-37 ms). Table (crater, patches, colliders now, meshes now, ms):
  WB 23:46:49 4/4/4 52 | 23:46:53 4/4/4 55 | 23:47:19 1/1/1 12 | 23:47:23 3/1/3 23 | 23:47:32 4/1/1 14 | 23:48:15 2/1/2 17
  WB 23:56:56 2/2/2 34 | 23:57:04 4/3/3 44 | 23:57:15 3/1/3 26 | 23:57:23 4/1/3 24 | 23:57:30 2/0/0 0 | 23:57:36 1/1/1 10
  DS 23:49:24 2/1/2 15 | 23:49:28 4/3/3 24 | 23:49:44 4/2/2 17 | 23:49:48 3/2/2 15 | 23:49:56 4/4/4 33 | 23:50:01 2/0/0 0
  So the rest of the stutter is the rut side (WorldGround, off limits) - asked the user.
- Added B7 MUD_WorldGroundEntity.VOX_MESH_LATER (user OK 2026-10-21 for this MUD_WorldGround edit; set 15 = set 14 + it):
  with sheet local on, DoVoxCells still builds the patch colliders in the go-live frame, but the patch meshes go to a
  list built one per frame (VoxLaterStep, EOnFrame before the voxel tick; a patch gone / building goes to the normal mesh
  queue). The old patch mesh can show over the crater for 1-4 frames (under the sheet). Meters: the 'voxel cells live'
  line ends `| colliders N ms, meshes N ms, meshes later N (B7 0/1)` (split now on every set); new line
  `MUD world: voxel cells meshes later - N built over N frames, N ms (worst N ms), N to the mesh queue`.
  MudSheet.LocalOn() added. Checkpoint v264 = before. Not compiled.
