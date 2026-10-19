# Rut / rebuild perf + bug hunt (research, 2026-10-19, after DS run 11)

Prompt: `docs/superpowers/prompts/2026-10-19-research-rebuild-and-bugs.md`. Code read on branch
`displaced-ruts-partial-split` at commit 1107eec. Nothing under `Scripts/` was changed. Every code block here is
**unreviewed plan code**: the local AI builds it, compiles it and tests it on the DS.

Labels: [repo] = file:line or a results-doc line, [IDA] = from mud-gotchas / results, [doc] = plans / feature docs,
[inf] = my inference. Line numbers are from commit 1107eec.

Baseline (run 11, client, robot 2 min reverse circle) [repo results/2026-10-19-mesh-vs-voxel-ruts-ds.md run 11]:
MUD script 1.72 ms/frame driving. rebuild 0.86 (rb mesh 0.58, rb collider 0.30), ahead 0.27 (a view 0.19), vox 0.10,
look ruts 0.09. Hot 10 s lines: 107-115 band mesh builds, 55-63 small mesh skips. Collider builds per 10 s:
25, 41, 54, 42, 34, 34, 33, 33, 36, 37, 31, 36 (sum 436 over the 12 lines; the whole-log sum is 454).
Client fps in run 11 is not in the results doc; I use ~100 fps (run 5: 99.4) to turn ms/s into ms/frame [inf].

## Ranked summary (client ms/frame while driving)

| rank | item | client ms/frame saved | server | sure? | section |
|---|---|---|---|---|---|
| 1 | MESHER_BENCH still true (TEMP bench in every 25th build) - turn it off | 0.07 | ~0.06 | high (measured) | 2.1 |
| 2 | ScrapeView hide + dead per-wheel loop in AheadView ('a view') | 0.07-0.15 | 0 | medium | 5.1 |
| 3 | LOOK_PACE: no 0.1 s mesh pace for look + rut floor dirt (only height dirt keeps it) | 0.06-0.12 | 0 | low-medium | 1.3 R1 |
| 4 | Layout() cache by on-cell mask (mesher core) | 0-0.07 | 0-0.02 | low (needs the counter) | 2.2 |
| 5 | Coarse safety plate (2 tris per skin cell) | 0.01 | 0.04-0.05 | medium | 2.4 |
| 6 | Rut mud split by the mesher's cell, not a per-tri centre | 0.01-0.02 | 0.03-0.05 | medium-low | 2.4 |
| 7 | Reveal + UV loop: skip the RevealT call for flat verts | 0.015-0.03 | 0 | medium | 2.3 |
| 8 | Look ruts Cut: dense arrays instead of 2 maps | 0.02-0.04 | 0 | low-medium | 5.2 |
| 9 | LEAD along the arc in turns (look ruts) | 0-0.02 (fewer expiry rebuilds) | 0 | low | 3 P1 |

Sum if all land at the low end: ~0.25 ms/frame of the 1.72 (~15 %). High end ~0.55. The only "sure" number is rank 1.
A TEMP rebuild-reason counter (1.4) should go into the same DS session: task 1's table is from code, not data.

---

## 1. Why so many mesh rebuilds?

### 1.1 Every path that sets mesh / look dirt (client)

| path | where | sets | box | pace it gets |
|---|---|---|---|---|
| look cut, every EVERY 0.05 s per vehicle | MUD_LookRuts.c:528 -> MUD_WorldGround.c:4401 MarkLookDirty | m_bMeshDirty + m_bLookDirty, queued | bounding box of all samples this cut wrote (both lead wheels) | 0.25 s if no collider dirt, else 0.1 s (4757-4759) |
| look sample end (Sweep, every 0.5 s) | MUD_LookRuts.c:589 | same, whole 16 x 16 tile | the tile | same |
| look sample caught (<= 2 cm) | MUD_LookRuts.c:591 LookBox | box only, no dirt | tile | - |
| server rut op (kind 4) | MUD_VoxelProbe.c:4205 -> MUD_WorldGround.c:4227 FloorDirty -> FloorFinish + MarkFloorDirty (4455) | m_bMeshDirty (render), m_bColDirty, m_bRutMesh if not dirty yet | box of every floor sample the op changed: cut + relax (+2 samples) + ridges (RUT_R + GAP + W = 1.0 m sideways) | 0.1 s (collider dirt is set) |
| J2 snapshot load | MUD_VoxelProbe.c:2373 | FloorDirty per tile, +-1 sample | tile + 1 | once per join |
| heightfield change (crater, shovel, scrape) | MarkDirty 4514 (callers 1350, 1821, 2046, 2588, 5131, 5355, 5528, 6342, 8579) | mesh (own patch) + collider (blur reach) | change + blur radius | 0.1 s |
| voxel go-live | DoVoxCells 3552 | builds mesh + collider at once (3598-3613), else dirt | patch | direct |
| hide owed (ahead hide) | BuildMeshBand 7391 / BuildMesh 6855 | m_bMeshDirty = hideOwed | - | 0.1 s |
| collider wheel hold | BuildColliderBand 7944, BuildCollider 7150 | m_bColDirty again (not mesh) | - | keeps the 0.1 s mesh pace (see 1.2) |
| small collider skip | 7757-7760 | m_bColDirty stays up to COL_SMALL_MAX_S 1 s | - | keeps the 0.1 s mesh pace |
| FloorSpread / LOOK_SPREAD | 4414-4419, 4251-4258 | finishes lazy / building patches (their last step builds) | - | own build steps |

Not a source now: hideOwed from the wheel hide (AHEAD_HIDE_SKIP true [repo MUD_WorldGround.c:760]); the
ScrapeView hide still marks strips (2759) but owes a rebuild only when m_aH changed under it (7423-7428) [repo].
FLOOR_SETTLE is false (110) [repo], so no settle hold. RUT_PACE / SAME_SKIP / PARTIAL_IN are false (764-767) [repo].

### 1.2 The pace rule and three things that keep it at 0.1 s

Rebuild (4693) builds at most one mesh per frame, the patch whose last mesh is oldest among the due ones
[repo 4770-4771]. Due = `m_fTime - p.m_fMeshAt >= meshEvery` and `m_fTime >= p.m_fMeshHoldTo`.
`meshEvery` is LOOK_MESH_EVERY 0.25 only when the patch has look dirt and NO collider dirt; otherwise it is
m_fMeshInterval 0.1 (attribute default 0.1, 253-254) [repo 4757-4759].

On a patch the truck is on, collider dirt is almost always set [repo + inf]:
- every server rut op sets m_bColDirty (4483), ~4 ops / s per vehicle (RUT_EVERY_MS 250, MUD_VoxelProbe.c:237);
- COL_SKIP_SMALL keeps m_bColDirty for up to 1 s after the last real collider build (7757-7760; COL_SMALL_MAX_S 1.0, 739);
- the wheel hold re-dirties the collider while a wheel sits on changed verts (7940-7951).

So the patches next to the wheels run at the 0.1 s pace, not 0.25 [inf from the code; share of builds unproven].
What then stops a build is MESH_SKIP_SMALL (MeshSmall 7642): no vertex moved >= MESH_SMALL_M 0.035 against the last
built mesh and that mesh is < MESH_SMALL_MAX_S 0.5 s old -> skip and hold until built + 0.5 s (7277-7282).
One look / wear pass cuts at most RUT_PASS_MAX 0.03 (MUD_VoxelProbe.c:275), which is under 0.035: a single pass is
always "small" [repo]. Builds then come from (a) the 0.5 s age limit, (b) a >= 3.5 cm change inside one window -
two cuts on one sample (front + TRAIL CUT wheel in a turn, MUD_VoxelProbe.c:307), ridges / relax, a real floor
where no look sample was.

### 1.3 One wheel pass over one patch (robot circle) - table

Assumptions [inf]: ~7.5 m/s (run 6 snaps "all at ~7.5 m/s, steer 1" [repo results run 6]), circle radius ~10 m,
lap ~7.5 s, ~10-11 patches touched per lap (ring of ~20 m diameter with +-1 m ridge reach, 8 m patches).
Dwell = time the patch gets dirt in one pass: look cut lays segments LEAD 2 m ahead (LEAD_MAX, MUD_LookRuts.c:70) plus
the patch chord (~8 m) plus the wheelbase when TRAIL CUT cuts in a turn, plus ~0.3 s for the last server op
(op every 0.25 s + net) -> ~1.7-2.3 s.

| source | events on this patch per pass | mesh builds per pass it leads to | why |
|---|---|---|---|
| look cut (MarkLookDirty) | ~35-45 marks (dwell x 20 / s) | ~3-4 (age-forced, 0.5 s window) | each mark is small (<= 3 cm), MeshSmall holds |
| server rut op (MarkFloorDirty) | ~7-9 ops | ~1-3 (>= 3.5 cm changes picked at the 0.1 s pace) | collider dirt switches the pace to 0.1 s |
| tail: last ops after the last look mark | 1-2 ops | ~1-2 | the real floor lands ~0.25-0.5 s after the look |
| look sample end (Sweep REPLACE 3 s / TTL 8 s) | 0-1 per tile | 0-1 (+1 neighbour when the tile starts on a patch line, see 3 C3) | in turns the LEAD offset puts look samples where no server floor comes (3 P1) |
| look caught (LookBox) | many | 0 | box only |
| seam neighbours | a look cut box crosses a patch line ~1/3 of the time (2.6 m wide / 8 m), a server op box ~1/2 (4 m / 8 m) | same as above, on the neighbour | the wheels really touch both patches (shared edge column) |
| hide owed, settle, spread, FieldDue | 0 on a repeated circle | 0 | switches off / patches stay live |
| **small mesh skips** (not builds) | - | ~4-6 skip passes, ~0.7 ms input each | MeshSmall runs after the full input pass |

Checksum: builds per patch per pass ~ 3-4 + 1-3 + 1-2 + 0-1 = 5-10; x 10-11 patches per 7.5 s lap = 6.7-14.7 builds / s.
Measured: 10.7-11.5 / s (hot lines 107-115 per 10 s) [repo results run 11]. Skips: 4-6 x 10.5 / 7.5 s = 5.6-8.4 / s
vs measured 5.5-6.3 / s. Both inside the model's range; the split between rows is NOT measured.

### 1.4 TEMP counter to measure the table (no behaviour change)

Unreviewed plan code. One 10 s line, counts per queued band mesh build and per small skip.

```c
// MUD_FinePatch (new): which dirt reached the patch since its last mesh
bool m_bWhyLook;      // MarkLookDirty from a cut (since >= 0)
bool m_bWhySweep;     // MarkLookDirty from Sweep (since < 0)
bool m_bWhyFloor;     // MarkFloorDirty
bool m_bWhyHeight;    // MarkDirty own branch / DoVoxCells dirt branch
// set them next to the existing m_bMeshDirty = true lines (4433, 4480, 4530, 3617); clear all four where
// m_bLookDirty is cleared after a build (6859, 7272, 7395)

// MUD_WorldGroundEntity, in Rebuild right before BuildMesh(mp) (4816):
bool whyCol = mp.m_bColDirty;     // the 0.1 s pace applied
int whyKind = 0;
if (mp.m_bWhyLook)
	whyKind = whyKind | 1;
if (mp.m_bWhyFloor)
	whyKind = whyKind | 2;
if (mp.m_bWhyHeight)
	whyKind = whyKind | 4;
if (mp.m_bWhySweep)
	whyKind = whyKind | 8;
// after BuildMesh: m_bMeshSkipped -> m_aWhySkip[whyKind]++ else m_aWhyBuild[whyKind]++; whyCol -> m_iWhyCol++
// MeshSmall: count 'forced by age' (only the age test failed) vs 'big change' (a vertex >= MESH_SMALL_M) - split
// its return into two counters before returning false.
// 10 s line 'MUD rebuild why 10 s: builds N (look a, floor b, look+floor c, height d, sweep e, other f) | at 0.1 s
// pace g | skips h | MeshSmall false: age i, big j' - checksum a+b+c+d+e+f = N.
```
int arrays of 16 for the kinds (int[] not bool[], gotchas). No `%` in float expressions.

### 1.5 Ways to cut the COUNT

| id | idea | machine | expected | risk |
|---|---|---|---|---|
| R1 LOOK_PACE | meshEvery = LOOK_MESH_EVERY whenever the dirt since the last mesh has no height part (new m_bHMesh, set by MarkDirty / DoVoxCells, cleared by the mesh build). Covers look + floor, floor only (what RUT_PACE missed: RUT_PACE needed !m_bLookDirty, 4760, and m_bRutMesh is only set when the patch was clean, 4478) | client | -10..-25 % mesh builds near the wheels, fewer skip passes; 0.06-0.12 ms/frame [inf] | a server floor deeper than the look shows <= 0.15 s later. Seams: two neighbours may differ up to 0.25 s instead of 0.1 s; the 0.54 m skirt covers it (7284-7288) [repo]. Holes at the wheels (LOOK SPREAD case) are not touched. No collider change (no feel / snap risk) |
| R2 hold reset | clear m_fMeshHoldTo on height dirt (bug C1) | client | +few builds (right ones) | none |
| R3 floor-only window | a patch whose only dirt is server floor (no look, no height) uses a 1.0 s MeshSmall window (as COL_SMALL_MAX_S) instead of 0.5 | client | -the tail builds (~1 per pass) | ridge / relax / depth corrections show <= 1 s late; the look rut already showed the rut |
| R4 exact Sweep box | LookDirty only the box of the dropped samples, not the whole tile | client | -1 neighbour build per expired tile that starts on a patch line | none |
| R5 LEAD along the arc | 3 P1 | client | fewer expiry rebuilds in turns; right look | look rut moves (user check) |
| R6 longer lead, slower look pace (user call) | look builds per pass ~ chord / LEAD when the pace is LEAD / speed. LEAD_MAX 3 + LOOK_MESH_EVERY 0.4 = ~-35 % look builds | client | -0.05..-0.1 ms/frame | more dirt ahead of the tyre after a hard stop (user accepted 2 m, MUD_LookRuts.c:63-66); pop behind the tyre if lead / speed < pace |
| R7 mark-time small check | MarkFloorDirty keeps the mesh when every changed sample's drawn value (min(real, look) + ridge) moved < MESH_SMALL_M (LOOK_COVERS v2; v1 gave 0 kept because every op also moves ridge / relax samples, 117-122) | client | saves the ~0.7 ms input pass of a skip: ~6 skips / s -> ~0.04 ms/frame | needs the R3-style max wait, else a floor-only change never shows |

Not recommended: per-wheel look boxes (same patch set except diagonal headings, ~5 % [inf]); a slower rut op rate
(changes ops = J2 / feel, not a client-only fix).

---

## 2. Cheaper per build

Split per build (run 11 [repo results]): mesh input 0.6-0.8, core ~2.0, reveal + UV ~0.7 (0.76), object ~0.5 (0.53);
collider input ~0.8, core ~2.2, trimeshes ~2.0. One ms cut from every build ~ 14.7 builds / s -> ~0.15 ms/frame
client [inf].

### 2.1 MESHER_BENCH is still on

`static const bool MESHER_BENCH = true;` (MUD_WorldGround.c:773) [repo]. Every 25th band build (mesh AND collider,
BandBench 7583, called at 7289 and 7765) runs BuildRef x 3 and Build x 3 on top. Run 11 client 'bench ms' column:
21+45+82+72+70+51+68+69+98+74+67+99 = 816 ms over ~120 s = 6.8 ms/s = ~0.07 ms/frame [repo results + inf fps].
Server: old 117 + new 96 = >= 213 ms / 120 s = 1.8 ms/s = ~0.06 ms/frame at 30 fps [repo results]. The build ms
excludes it (t0 += BandBench) but MUD_Prof 'rebuild' includes it (the results doc says so). Fix: `MESHER_BENCH = false`.
The "user: KEEP 4 + 5, 1/2/3 + PARTIAL_CHECK OFF" decision (perf plan REBUILD 5) did not name the bench [doc].

### 2.2 Layout() is cached against the previous build only

MUD_BandMesher.Layout (782-862) is skipped only when m_aSkin equals the LAST build's (791-801) [repo]. One mesher
(m_Band) serves mesh builds, collider builds and benches of different patches (7208, 7692). m_aSkin is the slot per
skin cell (FillBandOn 7169: `cell & SLOT`), so two patches with other gamemat slots (grass / dirt cells) differ and
the next build redoes the whole table: 1156 cells x (10 writes + 1-9 On() calls) [repo]. My estimate of one full
Layout: ~0.4-0.7 ms [inf, from the run 9-11 rate of ~0.45 us per simple loop iteration]. How often it reruns is not
logged.

The kinds, faces and corners depend only on which skin cells are on (16 bits). Only m_aCSlot depends on the slot
values. Unreviewed plan code:

```c
// MUD_BandMesher (new members)
static bool s_bLayoutSplit = true;     // A/B switch (runtime: the test kit may flip it per robot run)
protected int m_iLastOnMask = -1;
int m_iLayoutFull;      // log: full tables made
int m_iLayoutSlots;     // log: slot refreshes only
int m_iLayoutHits;      // log: nothing changed

protected void Layout()
{
	int onMask = 0;
	bool same = m_bLayout;
	for (int k = 0; k < 16; k++)
	{
		if (m_aSkin[k] >= 0)
			onMask = onMask | (1 << k);
		if (m_aLastSkin[k] != m_aSkin[k])
			same = false;
	}
	if (same)
	{
		m_iLayoutHits++;
		return;
	}
	for (int k2 = 0; k2 < 16; k2++)
		m_aLastSkin[k2] = m_aSkin[k2];
	if (s_bLayoutSplit && m_bLayout && onMask == m_iLastOnMask)
	{
		// same on cells: kinds / faces / corners stay; only the on cells' slots change
		for (int z = 0; z < NC; z++)
		{
			int sRow = (z / SKIN_FINE) * 4;
			int cRow = (z + 1) * CW + 1;
			for (int x = 0; x < NC; x++)
				m_aCSlot[cRow + x] = m_aSkin[sRow + x / SKIN_FINE];     // on cells only: same on-mask
		}
		m_iLayoutSlots++;
		return;
	}
	m_iLastOnMask = onMask;
	m_bLayout = true;
	m_iLayoutFull++;
	// ... the existing full loop (804-861) unchanged ...
}
```
Why the slot path is as safe as today's cache hit: Verts rewrites m_aJlo / m_aJn / m_aV0 for every kind 1 / 2 cell
and m_aSingle for every kind 1 cell each build (401-470); ghost cells keep m_aSingle -1 from the last full table;
today's cache hit relies on the same [repo]. Off cells keep m_aCSlot -1. Check: a TEMP LAYOUT_CHECK every 10th slot
refresh copies m_aCSlot, forces a full table (m_bLayout = false; Layout()), compares -> 'layout bad' must be 0.
The counters alone (full / slots / hits per 10 s) tell whether this item is worth keeping, even with the switch off.
Expected: (share of full tables) x ~0.5 ms x ~14.7 builds / s. At a 50 % share: ~0.04 ms/frame client [inf].

### 2.3 Reveal + UV loop (~0.7 ms)

The loop (7317-7341) runs over every vertex incl. skirts (~1500 [repo comment 7308]). Per vertex it calls RevealT
(6719), which returns 0 at once for most verts (6722-6723) - the call itself is the cost there. Inline the test:

```c
// unreviewed plan code, BuildMeshBand loop body (replaces 7332)
float dep = -m_Band.m_Info.m_aD[i];
if (dep > m_fRevealDepth)
{
	float t = RevealT(dep, rfX0 + qx, rfZ0 + qz);
	...as today...
}
```
Expected ~0.1-0.2 ms per mesh build (one call less per vertex, ~1500 calls) -> 0.015-0.03 ms/frame [inf]. The output is
the same float (RevealT starts with the same test).

UVs: top UV = (ux + qx) * invDiv - two mults; caching them per patch does not help because the write into the
vertex-ordered buffer is the cost, and the vertex order changes with the FAST / general cells [repo 7339-7340, inf].
The groove section (Aux + under UVs + second UpdateIndices / UpdateVerts, 7322-7326, 7345-7389) is only visible where a top vertex
reveals (t > 0). A groove index list limited to cells with any revealing corner (+1 ring) would cut the engine's
section-1 normal pass (UpdateVerts rebuilds normals per section from its indices [IDA gotchas 2026-10-14]). Saving
maybe 0.1-0.2 ms per build, but it needs an index compaction pass and risks see-through where the top sinks without
a groove under it. Not in the top 5.

Small: KeepMeshY copies m_aMeshD (7676-7679) - only SAME_SKIP / PARTIAL_IN read it, both off (but see bug C2, which
would use it). Quads writes m_Slot (collider only) on mesh builds too (562-565) - 2 writes per quad, < 0.05 ms.
`m_aJ` is a second 1089 loop with a Math.Ceil call each (361-365); computing j in the input loops saves ~0.05-0.1 ms but
touches both callers. Not worth it alone.

### 2.4 Collider trimeshes (~2.0 ms)

What runs per collider build after the mesher (7782-7907) [repo]:
1. `slotUsed` pass over all tris when there is rut mud (7792-7799).
2. per slot k (p.m_aMats.Count(), up to 4): a pass over ALL tris (7813-7855); for rut mud patches each tri of the slot
   also computes its centre (3 vector reads, add, mult, 2 Floor calls, 4 clamps) (7823-7843).
3. one CreateTriMesh per slot + one for rut mud.
4. bullet slab: nv vector subtracts + ni index copies + CreateTriMesh(ni) (7878-7896).
5. safety plate: CreateTriMesh(1089 verts, ix.m_Plate, up to 2048 tris) (7897-7906), its 1089 vectors written in the
   input loop (7753).

**Coarse plate.** m_aBase is the skin triangles (InitPatch TriHeight 3305-3309, a plane per skin triangle) and
m_aFloor is one value unless a depth box holds the patch (ApplyFloor 3204-3217) [repo]. Then base + floor - gap is
exactly the skin's two planes per cell, so 2 tris per hole skin cell on its 4 corners (TLBR diagonal, the same rule
FillIndices uses for m_aIndices) give the same surface as the 8 x 8 x 2 fine tris. Unreviewed plan code:

```c
// MUD_FinePatch (new): bool m_bFlatFloor;  // ApplyFloor: every m_aFloor[v] equal (no depth box)
// MUD_FineBuffers (new): vector m_aPlateC[25]; int m_aPlateCI[96];
// BuildColliderBand / BuildCollider, in place of the plate block when COARSE_PLATE && p.m_bFlatFloor:
int np = 0;
for (int pz = 0; pz <= SKIN; pz++)
{
	for (int px = 0; px <= SKIN; px++)
	{
		int pv = (pz * FINE) * PVERTS + px * FINE;
		m_Buf.m_aPlateC[pz * (SKIN + 1) + px] = Vector(px * FINE * m_fFine, p.m_aBase[pv] + p.m_aFloor[pv] - m_fPlateGap, pz * FINE * m_fFine);
	}
}
for (int c = 0; c < SKIN * SKIN; c++)
{
	if (p.m_aCell[c] < 0)
		continue;     // kept terrain: no plate, as m_Plate (6580-6582)
	int cx = c - (c / SKIN) * SKIN;
	int cz = c / SKIN;
	int a = cz * (SKIN + 1) + cx;
	// a, a + 1, a + SKIN + 1, a + SKIN + 2 = corners 00, 10, 01, 11; split and winding: COPY FillIndices' rule for
	// (p.m_aCell[c] & TLBR) - do not guess it here
	...6 indices into m_Buf.m_aPlateCI[np..np+5]...
	np += 6;
}
if (np > 0 && !geoms.IsEmpty())
	plate = PhysicsGeom.CreateTriMesh(m_Buf.m_aPlateC, m_Buf.m_aPlateCI, 25, np);
// and skip the m_aPlate fill in the input loop when the coarse plate is used
```
Saves the 2048-tri CreateTriMesh (~0.28 ms for 2048 tris [IDA/measured gotchas 2026-10-09 12:54]) and 1089 Vector
writes (~0.05-0.1 ms): ~0.3-0.35 ms per collider build. Client ~3.7 col / s -> ~0.013 ms/frame; server ~4 / s at
30 fps -> ~0.04-0.05 ms/frame [inf]. Risk: prone / fall rescue rely on the plate (gotchas 2026-10-10 prone box) - same
plane, so same catch; check 'fall' / rescue lines and a prone crawl over a rutted patch.

**Rut mud per tri.** The mesher knows each tri's cell when it writes the slot (FAST quad: cell `ca`, 562-565; general
tris: Tri 1264). Store the skin cell index per tri next to m_Slot.m_aS (`m_aCellOf[tt] = skin cell`) and test
`mudMask & (1 << cell)` instead of the centre maths. Saves the per-tri centre on rut mud patches: ~0.3-0.5 ms per
collider build there [inf]. MUD_BandTriSlot holds 20000 ints (80 KB); a second 20000-int array in its own holder class
stays under the 256 KB member limit [IDA gotchas 2026-10-14].

**Slab.** It copies and reverses every surface tri. If Bullet trimesh traces hit both faces (Bullet's default unless
back faces are filtered [inf]), the slab could reuse m_Ib.m_aI unreversed and skip ~5400 int copies (~0.15 ms). The
gotcha only proves "a lowered, downward-facing copy stops them" (2026-10-08 09:26) - a same-winding copy is not
tested. Needs a fire test through a crater rim; not in the top 5.

**Plate / slab geom reuse.** Keeping the plate PhysicsGeom alive across body rebuilds would save its CreateTriMesh
every time, but whether a geom may go into a second CreateStaticEx after DropBody is unknown (gotchas only cover
AddGeom on a live body). Needs a WB probe first.

---

## 3. Bug hunt (rut / ground path)

### CONFIRMED (traced end to end in code)

**C1 - a small-skip hold delays every later change, not only small ones.** [repo]
MUD_WorldGround.c:7281 sets `p.m_fMeshHoldTo = p.m_fMeshBuiltAt + MESH_SMALL_MAX_S`. The queue gates on it (4739,
4770). Nothing clears it: MarkDirty (4514-4560), MarkFloorDirty (4455-4493), MarkLookDirty (4401-4448) and the
DoVoxCells dirt branch (3615-3620) leave it. Scenario: a 3 cm look cut is skipped at t; at t + 0.05 a crater edge
(MarkDirty, >= 3.5 cm) lands on the patch; the queue does not pick it before t_built + 0.5 s. Severity: low (visual
delay <= 0.5 s; colliders do not wait; DoVoxCells builds go-live patches directly, 3608-3613). Fix: in MarkDirty's
own branch and in the DoVoxCells dirt branch add `p.m_fMeshHoldTo = -100;`.

**C2 - MeshSmall ignores the groove reveal (D).** [repo]
MeshSmall (7642-7653) compares only m_Band.m_aY with p.m_aMeshY. The reveal sinks the top up to m_fRevealGap x 3 =
0.09 m and switches to soil from D (7332-7337; m_fRevealDepth / Blend / Gap 177-183), and KeepMeshY already keeps m_aMeshD (7678).
Scenario: pass 3 crosses FIELD_SOIL_D 0.06 (92) - Y moves 3 cm (small), D crosses the reveal line, the dirt shows up
to 0.5 s late. Severity: low (visual). Fix sketch (unreviewed):
```c
if (Math.AbsFloat(m_Band.m_aY[v] - p.m_aMeshY[v]) >= MESH_SMALL_M || Math.AbsFloat(m_Band.m_aD[v] - p.m_aMeshD[v]) >= MESH_SMALL_M)
	return false;
```
(m_aMeshD exists whenever m_aMeshY does: KeepMeshY makes both.)

**C3 - Sweep marks the neighbour patch for tiles that start on a patch line.** [repo]
MUD_LookRuts.c:589 dirties the whole tile `bi * TS .. bi * TS + TS - 1`. For even bi, bi * 16 is a multiple of 32 =
a patch edge column, and PatchLo (MUD_WorldGround.c:2834-2839) returns the patch before it too. That patch gets a
full mesh rebuild although only the edge column could have changed, and only if a dropped sample sits on it.
Severity: perf (small). Fix: track the min / max fi / fk of the samples dropped with a redraw in Sweep and LookDirty
that box (LookBox the same).

**C4 - TEMP bench left on.** Section 2.1. Severity: perf, ~0.07 ms/frame client, ~0.06 server.

**C5 - the scrape hide makes a 39 KB press holder on every patch the body crosses.** [repo]
AheadView -> ScrapeView (2698) -> ArcHide (2759) -> MarkHide calls `p.EnsurePress()` per vertex (2824).
MUD_PatchPress is 9 x 1089 floats (MUD_FinePatch.c:195-206) and its constructor writes 9801 values (208-222).
With voxel ruts no wheel press needs it, so this undoes patch memory Stage 1 ("~39 KB per patch saved", FinePatch.c:30-31)
for every driven patch, and adds the constructor cost on the first frame over a new patch. Severity: perf / memory,
low-medium. Fix: section 5.1.

### PLAUSIBLE (path traced, effect not measured)

**P1 - in turns the look rut is laid off the wheel path.** [repo + inf]
MUD_LookRuts.c:246-248: `lead = dir * Math.Min(LEAD_MIN + hs * LEAD_S, LEAD_MAX)` and the segment is
`last + lead .. cp + lead` - a straight shift along the current heading. On an arc of radius r the shifted chord
sits L^2 / (2 r) outside the path: L = 2 m (7.5 m/s caps at LEAD_MAX), r ~10 m -> ~0.2 m [inf], against RUT_R 0.3
(MUD_VoxelProbe.c:259). The server cuts the real path (RutWheel 1293, no lead). Effects: a wider / doubled look groove
in the robot circle; outer look samples get no server floor, so they run to REPLACE (3 s) / TTL (8 s) and rebuild
the tile when dropped (MUD_LookRuts.c:538-589); an aligned pass does not refresh m_aAt (496), so laps do not keep
them alive. Check in the run 11 client log: 'MUD look ruts' expired + replaced counts in the circle vs a straight
drive. Fix sketch (unreviewed), lead along the arc:
```c
// MUD_LookTrack (new): vector m_vPrevDir; float m_fYaw;
// Tick, after dir: yaw from the heading change since the last tick (dt as for m_vVel); sign: check against
// MUD_WorldGround.PathAt (2093) before use
float cr = t.m_vPrevDir[2] * dir[0] - t.m_vPrevDir[0] * dir[2];
float yaw = 0;
if (t.m_fPrevAt >= 0 && dt > 0.001 && t.m_vPrevDir.LengthSq() > 0.5)
	yaw = Math.Asin(Math.Clamp(cr, -1, 1)) / dt;
t.m_vPrevDir = dir;
// each endpoint P moves along its arc by L: centre C = P + (perpendicular of dir) * (hs / yaw), angle th = yaw * L / hs,
// P' = C + rotate(P - C, th); |yaw| < 0.01 -> straight shift as today
```
Simpler fallback: cap L at sqrt(2 r LEAD_TOL) with r = hs / |yaw|, LEAD_TOL 0.05 (L <= 1 m at r 10 m) - but that puts
the look rut less far ahead in turns (pop risk). Visual change: user check.

**P2 - the 0.1 s mesh pace near the wheels.** Path confirmed (1.2); its share of builds is not measured. R1 + the 1.4
counter.

**P3 - one pass of look ruts is always "small".** RUT_PASS_MAX 0.03 < MESH_SMALL_M 0.035 [repo]. A look cut on a patch
whose mesh is < 0.5 s old waits for the window end; at 7.5 m/s that is up to 3.75 m of travel, more than LEAD 2 m, so
the rut can show behind the tyre. The user judged run 8 'look the same, fine' [repo results run 8], so maybe not
visible. Not a fix on its own: lowering MESH_SMALL_M to 0.025 brings back the builds. Note for R6.

**P4 - wear-only tiles turn patches into rut field patches.** WearPass makes a tile (MUD_VoxelProbe.c:5062);
FieldInBox (5854-5871) is true for any tile, so RutFieldOn (MUD_WorldGround.c:4498-4508) sets m_bFloor and every build
of the patch runs the floor fills for grass that has no floor. Harmless for the look; small extra input cost and
patch keep / free rules for floor patches apply to merely worn grass. No fix proposed until the counter shows it matters.

### Checked, no finding

- J2 rounding: the server applies Decode(Encode(op)) (MUD_VoxelProbe.c:3596), so it uses the same cm segments and mm
  depth / cap as every client [repo]. CarveFloor rounds floors to cm (4992), WearPass wear to 0.01 (5063);
  TileText / RunText / ParseTilePart / ParseRuns round-trip floor, ridge and wear, incl. wear-only and ridge-only tiles
  (2048-2178) [repo].
- FloorBase (5235-5267) reads only op data, columns / tiles (synced by ops), the skin hole map, terrain gamemats and
  UndugY (no m_aH) - floor-blind as the J2 rule needs [repo]. LookCoversOp is machine-local but off and only chooses
  keepMesh (visual).
- RUT_RAMP: RampBehind sets m_mRampW with every 'best' (5222; CarveFloor's loop 4929), so `cf / rw` never divides by 0 [repo].
- Hash: int overflow in `rv * (fn + 1) + ColKey` wraps the same on every machine (5008) [repo, inf].
- Patch / tile borders: PatchLo / PatchHi give an edge vertex to both patches (2834-2847); FieldPatchBegin / PatchBegin
  span PVERTS - 1 = 32 covers the far edge (5756-5771, MUD_LookRuts.c:738-753); the fills index with non-negative
  offsets (FloorDiv base) [repo].
- Math.Min / Max as an array index: none in the six files (grep) [repo].
- Freed patches: m_aDirty, m_aFloorSpread, m_aSheetHid, FieldDue all keep keys and re-Get with null checks
  (4705-4716, 4670-4688, 4345-4356) [repo].
- GroundFollow runs only for ops 0-3 (MUD_VoxelProbe.c:6747), not on the rut path. Nothing found for ruts.

### Enforce limits - locals in scope (script count, approximate)

Counted with a brace-aware script (declarations of known types + foreach / for vars + params; blocks closed = freed).
It can miss locals of unusual types, so treat these as lower bounds [inf]:
MUD_LookRuts.Cut ~50 (7 params), MUD_VoxelProbe.Carve ~48, MUD_WorldGround.Crater ~48, BuildMeshBand ~38,
RelaxFloor ~38, CarveFloor ~37, BuildColliderBand ~36, Press ~36, Quad (VoxelProbe) ~36, SnapSelfTest ~35.
None near 64. Cut is the one to watch: adding the P1 arc lead there (instead of in Tick) would add ~5.

---

## 4. Review of REBUILD 5's kept code

### MergeMask / EmitMerged (collider merge, MUD_BandMesher.c:633-728)

- Block bx = cells 2bx .. 2bx + 2 = the 4 y quads at grid points 2bx + 1, 2bx + 2. Quads maps gx -> mbx = (gx - 1) / 2
  and draws the block once at gx - 1 == 2 mbx (510-527). Matches [repo].
- Skin cells: blocks with bx or bz % 4 == 3 (cells 6-8 = across a skin cell edge) never merge (642). Block 15
  (cells 30-32, cell 32 = ghost) is excluded by the same rule. Slots and rut mud stay per skin cell; the merged tris'
  centres lie in the block's skin cell, so the mud split (7823-7843) puts them right [repo].
- Untouched test: m_aTouch is 1 where y != base + smooth (7747-7750): rut, ridge, hold [repo]. All 16 samples of the
  block must be untouched (656-660). All 9 cells single-vertex means all 16 samples sit in one layer, so no x / z wall
  quad can start inside a block (inf from 404-408: a single cell has its 4 corners in one layer; neighbours share
  corners).
- Flatness: the 4 edge middles are checked against their big edge, the centre against the diagonal EmitMerged will
  use (675-686) -> every dropped vertex is within COL_MERGE_TOL 0.01 of the merged surface [repo].
- T-junctions: YES, at every edge between a merged block and an unmerged neighbour. The neighbour's quads use the
  block's edge-middle vertex, the merged quad does not. In plan view the edges meet exactly (the cell centres are
  collinear), so there is no gap to fall through; the vertical mismatch is the middle vertex's off-line distance,
  at most 1 cm. Two merged neighbours share full edges (no T). [repo + inf] Risk: a <= 1 cm ridge a tyre or a
  capsule can feel only at that line; Bullet internal-edge contacts can give a small bump there. I would not chase
  it unless the user feels flat grass buzz. Lower COL_MERGE_TOL to 0.005 if they do.
- Overflow: EmitMerged writes 6 indices after the `ii + 6 > MAX_I` check (519-525); m_aS index tt + 1 <= 19999 holds
  when ii + 6 <= 60000 [repo].
- Collider vs drawn mesh: the drawn mesh never merges (m_bMerge = false, 7208); the difference is <= 1 cm.

### Build / Verts / Quads vs BuildRef

Path by path (Build 341-380 vs BuildRef 119-339) [repo]:
- Reset of counters, Layout, m_aJ: same. Build also resets m_iMerged.
- Verts FAST cell: same floats in the same order (`(vx + 0.5) * fine` vs `(vx + 0.5) * m_fFine`, same value),
  same overflow order (cells++ before the MAX_V check).
- Verts general cell: m_iV / m_iCells are written back before VertexOn / Vertex and read after - same as BuildRef.
- Quads FAST quad: same split rule, same writes; general Quad: m_iI synced before / after each call; overflow checked
  in the same places (FAST check before writing, m_bOverflow after each grid point).
- Differences: m_iFastV / m_iFastQ are stored only at the end, so after an overflow they read 0 where BuildRef had
  partial counts. Only BandCount reads them, and it runs only after a successful build. Stats only, harmless.
- The bench (0 differ in 79 benches, run 11) agrees, but it never exercised overflow (0 overflow in run 11 is not in
  the doc; 'MUD band' overflow / fallback counters would show it).

No bug found in the kept code.

---

## 5. 'a view' 0.19 ms/frame and look ruts 0.09

### 5.1 'a view' = AheadView (MUD_WorldGround.c:2596-2722)

With AHEAD_HIDE_SKIP on, hideOn is false (2600). Per vehicle and frame AheadView still runs [repo]:
- Contacts (2652), BrakeCap (2653), YawRate (2654), and a per-wheel loop with WheelGetContactPosition,
  GetVelocityAt, AheadPoint (2659-2691). With hideOn false and m_bAheadDraw false (attribute 0, 320) the loop's only
  results are 'pressed' / 'skipped' for the debug text and `lead` for ArcHide - all unused. Dead work.
- ScrapeView (2698) because m_bScrape is on (attribute 1, 347): the belly points loop (2737-2756, CoordToParent per
  point) and ArcHide -> 4 x MarkHide (2759 -> 2765) every frame. The strip is from revealFrom to lead + 0.35 + 0.5 m
  ahead (lead = 2 + 0.5 x 7.5 = 5.75 m at 7.5 m/s, attributes 305-312) and body half width + 0.35 to each side ->
  ~13 m^2 = ~210 vertices, inside 4 bounding boxes of ~300 vertices each. Each in-strip vertex calls EnsurePress()
  (2824) [repo]. This part is inside A_VIEW but outside A_HIDE (A_HIDE only wraps the per-wheel ArcHide and HoldHide,
  2639-2642, 2672-2675) - so it is very likely most of the 0.19 [inf].

Proposals (unreviewed plan code):
```c
// AheadView: the per-wheel loop only feeds the wheel hide and the debug draw (VIEW_TRIM switch)
bool wheelLoop = !VIEW_TRIM || hideOn || m_bAheadDraw;
if (wheelLoop)
{
	Contacts(veh, sim);
	// ... existing per-wheel loop ...
}
// BrakeCap and YawRate stay: ScrapeView reads m_fLeadCap (AheadLead 2148) and yaw

// MarkHide: one EnsurePress per patch, made at the first vertex in the strip (keeps 'made on the first write')
MUD_PatchPress pr = null;
for (int lz = lz0; lz <= lz1; lz++)
{
	float rz = oz + lz - cfz;
	for (int lx = lx0; lx <= lx1; lx++)
	{
		float rx = ox + lx - cfx;
		float along = rx * dx + rz * dz;
		if (along < f0 || along > f1)
			continue;
		if (Math.AbsFloat(rx * dz - rz * dx) > rr)
			continue;
		if (!pr)
			pr = p.EnsurePress();
		pr.m_aHideAt[lz * PVERTS + lx] = m_fTime;
	}
}
if (pr)
	p.m_fHideUntil = m_fTime + HIDE_HOLD;
```
Expected: 'a view' 0.19 -> ~0.08-0.12 (-0.07..-0.11 ms/frame) [inf].

Bigger, needs a user decision: does the body scrape still do anything with voxel ruts? The server runs Scrape when
m_bAheadRuts && m_bScrape (1549); it cuts m_aH (heightfield) where the belly would touch. With 3 cm wear passes the
belly may never touch. Read the run 11 SERVER log line 'MUD scrape: last 10 s - N underside points cut ground'
(9129). If N is 0 through the whole drive, a SCRAPE_HIDE_SKIP switch (skip ScrapeView on rendering machines while
voxel ruts are on) saves most of the rest of 'a view' (~0.1-0.15 ms/frame), and also C5's press holders. If the user
wants the scrape kept as a safety net (deep craters, ditches), keep the hide - a server scrape with no client hide
shows the ground drop before the body gets there.

### 5.2 Look ruts 0.09 ms/frame (MUD_LookRuts.Tick 152 / Cut 338)

Cut runs every EVERY 0.05 s per moving vehicle (73). Each call allocates 8 arrays + 2 maps (340-348, 447-448) and does
up to 4 map operations per candidate sample (Contains, Get, Set x 2: 427-432) [repo]. A dense float array over the cut's
local box (the segments' bounding box + r, a few hundred samples) replaces both maps: index = (fk - k0) * w + (fi - i0),
FLOOR_NONE / -1 as "no entry", the box size known before the loop. Same values, same order of writes if the second
loop walks the box in (fk, fi) order instead of map order - the map order today is the map's, and the result does
not depend on it (each sample is written once from its own best) [inf]. Expected 0.09 -> 0.05-0.06 [inf]. The same
trick fits CarveFloor (MUD_VoxelProbe.c:4866) but there the WRITE order feeds RelaxFloor's sorted 'wrote' list - keep
the sort.
Cheaper knob: EVERY 0.05 -> 0.1 halves the fixed per-call cost but the look cut then lands up to 0.75 m later at
7.5 m/s (user moved it from 0.1 to 0.05 on 2026-10-18 for 'ruts not instant') - not recommended without the user.
