# Mud lab stage 3 - Part 0 measure (2026-10-21)

Plan: `plans/2026-10-21-mud-lab-stage3-plan.md` sec 0. Checkpoint v265 = before stage 3.
Status: meters written (print only, TEST_KIT), compiled clean in WB 2026-10-21 18:17 (no new warnings), not run yet. Rows marked **NEXT** are filled by the next launch.

## 1. New meters (TEST_KIT only, print only, no behaviour change)
| meter | where | log |
|---|---|---|
| A TURN METER | MUD_MudSheet: TurnRate (every sheet frame, followed vehicle heading -> smoothed deg/s; bucket straight / turning >= 8 deg/s / stopped < 1 m/s), TurnEdges (end of Upload when a live slot was built: every drawn live slot's edge record vs the drawn neighbour's, j = 0..TQ, corners counted apart), TurnNote at drape cut (+ skirt quads left), co-build side copied over > 3 cm (A-2), born wait | appended to `MUD sheet builds:` as `TURN METER (... ms ...) \| straight: ... \| turning: ... \| stopped: ...` |
| B under sheet | MUD_WorldGround BuildMeshBand (sheet local, groove on): revealed verts vs `MUD_MudSheet.FullCoverAt`; groove section upload ms; B10 'would hold' test on queued builds (BHoldMeter, before KeepMeshY) | new line `MUD world B under sheet 10 s:` after the `MUD band: grid path` line |
| B8 | MUD_LookRuts: segments whose two lead-shifted ends pass `MUD_MudSheet.LocalStampsAt` | appended to `MUD look ruts: 10 s` as `B8 meter: segments sheet local stamps itself N` |
| D crater notes | MUD_MudSheet PreNote -> PreMeter: notes, joining (no local controlled entity), most in one sheet frame, dropped at the 32 cap | appended to `MUD sheet prebuild:` and `MUD sheet builds:` as `PART D notes ...` |

Notes on the meters (INFERENCE until a run shows them working):
- Edge records exist only with EDGE_MATCH / EDGE_AUDIT / EDGE_COBUILD on (sets 11+). With them off the TURN METER shows frames only.
- FullCoverAt counts stored tiles as NOT covered and needs the neighbour's edge record within 4 rows of a side
  (conservative: under-counts cover). NearBareTile (any bare tile in the 3 x 3 round) = not covered.
- Groove upload ms uses GetTickCount (1 ms steps): one build reads 0 or 1, the 10 s sum is right on average.
- The B meter calls FullCoverAt per revealed vert - TEST_KIT cost; do not compare its ms rows to builds without it.

## 2. Before numbers (from the repo, copied)
- Crater go-live frame on the sheet ruts, DS (stage 2 report end): set 12 69-91 ms; set 14 ~35-50; set 15 ~12-25
  (swap / voxel / sheet 1/6/9, 1/5/18, 1/7/5; off ruts 0/2/0). Set 15 split: colliders 5/4/4 ms in the go-live frame,
  meshes later 4/2/1 one per frame (12/6/2 ms total, worst 4). Ahead dig DS 12-21 ms, close 5-18 ms.
- Client rut cost without the sheet (results/2026-10-19-mesh-vs-voxel-ruts-ds.md): rebuild 1.33-2.01 ms/frame
  (rb mesh 0.85-1.33, rb collider 0.46-0.67), ahead 0.56-0.78; 86-205 patch meshes / 10 s; 4-7 ms per build.
- Sheet local cost (results/2026-10-20-mud-lab-stage1-ds.md): 2.3-5.3 ms MUD script/frame at presets 1-3; upload
  0.9-2.9, band 0.4-1.8 ms; ~600-950 tile builds / 10 s, 19-40 % would skip.

## 3. Existing log, read 2026-10-21: DS client PeerPlugin1/logs/logs_2026-10-21_00-08-31 (sheet local preset 1, set 15)
Fix sets in the lab line: 00:09:32-00:10:32 and 00:11:02-00:11:12 = set 15 (`vox_mesh_later` listed); 00:10:42 and
00:10:52 = set 14 (no `vox_mesh_later`). No turn / straight split exists in this log - that is what the TURN METER adds.

### 3.1 Edges, per 10 s (`MUD sheet builds:`; edge audit = 10 s snapshot, corners left out)
| time | drape cut | copied | moved+co-built | faded | raised | ok | step>3cm | step max cm | open | open max cm | no mesh |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 00:09:32 | 153 | 1645 | 312 | 863 | 1986 | 1986 | 0 | 0 | 0 | 0 | 0 |
| 00:09:42 | 60 | 524 | 83 | 205 | 2216 | 2216 | 0 | 0 | 0 | 0 | 0 |
| 00:09:52 | 24 | 378 | 30 | 136 | 1384 | 1384 | 0 | 0 | 0 | 0 | 0 |
| 00:10:02 | 71 | 618 | 99 | 291 | 1780 | 1780 | 0 | 0 | 0 | 0 | 0 |
| 00:10:12 | 129 | 2393 | 379 | 1188 | 2924 | 2918 | 0 | 0 | 6 | 15 | 6 |
| 00:10:22 | 197 | 2195 | 329 | 1188 | 3196 | 3162 | 0 | 0 | 34 | 22 | 34 |
| 00:10:32 | 74 | 1422 | 142 | 660 | 3064 | 3032 | 0 | 0 | 32 | 24 | 32 |
| 00:10:42 | 0 | 0 | 0 | 0 | 3064 | 3032 | 0 | 0 | 32 | 24 | 32 |
| 00:10:52 | 2 | 366 | 63 | 399 | 2818 | 2771 | 47 | 19 | 0 | 0 | 0 |
| 00:11:02 | 2 | 233 | 38 | 81 | 1602 | 1525 | 30 | 17 | 47 | 18 | 47 |
| 00:11:12 | 0 | 220 | 51 | 225 | 1532 | 1477 | 47 | 19 | 8 | 18 | 8 |
Checksum (ok + step + open = raised): 00:10:12 2918+0+6 = 2924; 00:10:52 2771+47+0 = 2818; 00:11:02 1525+30+47 = 1602;
00:11:12 1477+47+8 = 1532 - all match.
Read: drape cut at cap is high while driving (24-197 per 10 s; A-1 candidate is live). Steps > 3 cm (max 17-19 cm)
show only from 00:10:52 on; open borders are all 'no mesh' (none untraced / unborn / band 0).

### 3.2 Sheet cost and craters, per 10 s (`MUD lab 10s:`)
| time | fps avg | MUD ms | upload ms | band ms | craters | recv->golive avg/max | golive frame max |
|---|---|---|---|---|---|---|---|
| 00:09:32 | 77.1 | 4.28 | 1.59 | 0.79 | - | - | - |
| 00:09:42 | 87.4 | 1.38 | 0.26 | 0.12 | 1 | 89/89 | 35 |
| 00:09:52 | 85.4 | 1.00 | 0.06 | 0.00 | 3 | 149/158 | 24 |
| 00:10:02 | 85.7 | 1.47 | 0.35 | 0.19 | 1 | 54/54 | 8 |
| 00:10:12 | 73.5 | 4.35 | 1.48 | 0.80 | 1 | 145/145 | 47 |
| 00:10:22 | 78.0 | 3.61 | 1.32 | 0.57 | 3 | 104/146 | 31 |
| 00:10:32 | 83.9 | 2.31 | 0.78 | 0.32 | 1 | 83/83 | 42 |
| 00:10:42 | 90.2 | 0.52 | 0.00 | 0.00 | - | - | - |
| 00:10:52 | 87.5 | 1.08 | 0.01 | 0.04 | 2 | 128/132 | 52 |
| 00:11:02 | 87.9 | 0.98 | 0.00 | 0.00 | 3 | 157/160 | 40 |
| 00:11:12 | 91.7 | 0.89 | 0.00 | 0.03 | 2 | 131/154 | 24 |
Crater count checksum: 1+3+1+1+3+1+2+3+2 = 17; `MUD sheet prebuild:` lines in the log = 18 (one go-live closed in
the 00:11:12 - 00:11:15 tail after the last 10 s line).

### 3.3 Ahead dig per crater (`MUD sheet prebuild:`, all 18)
dig ahead worst frame ms: 10, 20, 15, 13, 0, 17, 18, 1, 0, 14, 15, 14, 18, 18, 18, 21, 12, 0 (max 21; 3 craters
had nothing held: off the sheet). close ms: 23, 9, 3, 2, 0, 23, 20, 1, 0, 23, 15, 8, 15, 8, 9, 18, 5, 0.

### 3.4 Look ruts on this client
`MUD look ruts: 10 s` lines show cuts (e.g. 96, 13) but `look meshes 0` in this log - no rut patch mesh rebuilt by
look cuts here (no driving over old ruts in this run). B rows below need a run that drives on ruts.

## 4. Tables the next launch fills (NEXT)
- **A** per bucket per 10 s: frames, checks, raised, step > 3 cm (max), open, corner step, drape cut (+ quads left),
  co-copy over 3 cm, born waits - sets 15 / 16 / 17, straight 20 s / tight circle 20 s / stop 12 s.
- **B** builds metered, none revealed / all covered / some uncovered, groove upload ms, B10 would hold of queued,
  B8 segments - driving on old ruts, sheet local, set 15.
- **C** `MUD prof parts` for driving on ruts, crater on ruts, crater off ruts, tunnel dig, trench (DS, >= 6 runs per
  row + an off row), top 5 costs.
- **D** join with 30+ craters: PART D notes (joining, most in one frame, cap drops), prebuild worst frame.
