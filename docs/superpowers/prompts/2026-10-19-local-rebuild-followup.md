# Prompt for the local AI: rebuild follow-up build + one DS session (2026-10-19)

Source: `docs/superpowers/research/2026-10-19-rebuild-and-bugs-research.md` (read sections 1.4, 2.1, 2.2, 2.4, 3 C1,
5.1 first). All code there is unreviewed plan code: compile it, check it against the rules below, then test.

## Rules
- CLAUDE.md TESTING RULES: perf numbers only from the DS session, never Workbench. One launch, one checklist.
- Checkpoint first: `mud_backups/checkpoint.ps1 save "before rebuild follow-up"`.
- Enforce limits (mud-gotchas): 64 locals in scope per method (params count), no `%` / `&` inside a float expression,
  `set` / `debug` / `owned` reserved, string.Format max 9 values, member flag arrays int[] not bool[], Math.Min / Max
  return float (int local before indexing).
- No op / net / J2 change in this build: every item is local build cost or a client-only pace. If anything you write
  touches CarveFloor, ops or tiles, stop and ask the user.
- Do not touch MUD_MudSheet (parked).

## Build (in this order)

0. **REBUILD_WHY (TEMP counter, always on)** - research 1.4. One 10 s line 'MUD rebuild why 10 s: ...' with the
   kinds, the 0.1 s pace count, skips, and MeshSmall false split into 'age' / 'big'. Checksum in the line: the kind
   counts must add up to the builds.

1. **BENCH_OFF** - `MUD_WorldGroundEntity.MESHER_BENCH = false` (MUD_WorldGround.c:773). No A/B: run 11 measured it
   (client 816 ms bench per 120 s). Check: 'MUD rebuild 5' line shows bench n 0.

2. **VIEW_TRIM** (runtime switch, client) - research 5.1: AheadView skips Contacts + the per-wheel loop when
   `!hideOn && !m_bAheadDraw` (keep BrakeCap + YawRate: ScrapeView reads them); MarkHide calls EnsurePress once per
   patch (first in-strip vertex), sets m_fHideUntil once. Same result, less work.

3. **LOOK_PACE + HOLD_RESET** (runtime switch, client) - research 1.5 R1 + 3 C1: new MUD_FinePatch.m_bHMesh (set where
   MarkDirty marks the own patch's mesh, 4530, and in the DoVoxCells dirt branch, 3617; cleared where m_bLookDirty is
   cleared after a build, 6859 / 7272 / 7395). Rebuild: `meshEvery = LOOK_MESH_EVERY` when `LOOK_PACE && !m_bHMesh`,
   else today's rule (4757-4759). HOLD_RESET part: the same two places set `p.m_fMeshHoldTo = -100`.

4. **LAYOUT_SPLIT** (runtime switch, both machines) - research 2.2: MUD_BandMesher.Layout keeps kinds / faces when the
   on-cell mask is unchanged and only refreshes m_aCSlot. Counters full / slots / hits per 10 s (print them in the
   'MUD band: last 10 s' line or a new 'MUD layout 10 s' line) - they show the value even when the switch is off.
   TEMP LAYOUT_CHECK: every 10th slot refresh force a full layout and compare m_aCSlot -> 'layout bad' MUST be 0.

5. **COARSE_PLATE** (const switch, both machines, on for the whole session) - research 2.4: 2 tris per hole skin cell
   for the safety plate when the patch floor is flat (new m_bFlatFloor from ApplyFloor). Copy the diagonal + winding
   rule from FillIndices (m_aIndices) - do not guess it. Both collider builders (BuildCollider 7107, BuildColliderBand
   7897). Log in the 'MUD band' collider split: plate tris per build, coarse plates N / full plates M.

Runtime A/B (items 2, 3, 4 on the client): make the three switches `static bool` (not const) and set them at the start
of every robot run from `MUD_StutterRobot.GetRun()` - odd runs ON, even runs OFF - with one line
'MUD AB: robot run N - VIEW_TRIM x LOOK_PACE x LAYOUT_SPLIT x'. The server has no robot run: its LAYOUT_SPLIT stays
on all session (judge it by its counters and core ms per collider build vs run 11's 2.18). COARSE_PLATE stays on all
session (judge it by the collider 'trimeshes' ms per build vs run 11's 2.05).

Compile in Workbench; run it once in WB only for the look / function checks the user wants (not perf).

## Not in this build - ask the user first
- C2 MeshSmall also compares D (dirt shows on time; a few more builds). Small change, visual - user call.
- P1 LEAD along the arc (look ruts in turns). Visual change - user call. Before that, read the run 11 client log:
  'MUD look ruts: 10 s' expired / replaced counts during the circle; if they are near 0, P1 is not a perf item.
- SCRAPE_HIDE_SKIP: read the run 11 SERVER log 'MUD scrape: last 10 s - N underside points cut ground'. If N is 0 in
  every driving line, ask the user whether the body scrape is still wanted with voxel ruts; if not, skipping
  ScrapeView on rendering machines saves most of 'a view'.
- R6 longer LEAD + slower look pace - user call (more dirt ahead of the tyre).

## DS session (one launch)
Setup as CLAUDE.md "MP robot test setup": DS (Workbench peer tool) + PeerPlugin1 client with -logLevel debug, mud
sheet off, test kit on, spawn a fresh truck after start. Robot: driver seat, stopped, hold Space 3 s = 2 min reverse
circle (RUN_SEC 120). 4 robot runs in a row on the same spot: run 1 ON, run 2 OFF, run 3 ON, run 4 OFF.

Numbers Claude reads afterwards (client log unless noted; windows = the robot runs, from the 'MUD AB' lines):
| item | keep if | drop if |
|---|---|---|
| VIEW_TRIM | 'MUD prof parts' a view, ON runs <= OFF runs (any gain); ruts still at the wheel | ON slower, or ruts look different |
| LOOK_PACE | hot 10 s lines: band mesh builds ON <= 0.85 x OFF and rb mesh ms/frame ON < OFF; user sees no later ruts, no seam cracks | builds not lower, or user sees later ruts / cracks |
| LAYOUT_SPLIT | 'layout bad' 0 AND full tables >= 30 % of builds with the switch off AND core ms per build ON < OFF by >= 0.1 | any 'layout bad' > 0 (drop at once), or < 30 % full tables (not worth it) |
| COARSE_PLATE | collider 'trimeshes' ms per build <= 1.8 (run 11: 2.05) on both machines, 0 new 'MUD fall: rescued' lines, prone check fine | rescue lines appear, or prone falls into the ground |
| BENCH_OFF | always (TEMP code) | - |
| REBUILD_WHY | (data) fill research 1.3's table with the real split | - |
Also: MUD script avg (driving 5 s lines) ON vs OFF, snaps ('MUD corr: ... mine 1') per run - snaps need >= 6 runs to
judge (CLAUDE.md), so only flag a big jump.

## Check list for the user (one launch)
1. Start the DS + client as usual (Start only clients first, then Play "Server localhost + PeerTool"); keep the WB
   window in front. Spawn a fresh truck.
2. Tell Claude when the client has joined (Claude checks the log for compile errors and the 'MUD AB' line).
3. Driver seat, stand still, hold Space 3 s -> robot run 1 (2 min). Watch the ruts at the wheels.
4. When it stops, hold Space 3 s again -> run 2. Then run 3, then run 4 (same spot, do not move the truck between runs).
5. Say if runs 1 / 3 and 2 / 4 looked different (ruts later, cracks at 8 m patch lines, holes at the wheels).
6. Get out, go prone and crawl across the circle's ruts for ~20 s (coarse plate check). Say if you sink or get pushed.
7. Stop. Tell Claude the times of anything odd. Claude reads the logs and fills the results doc
   (`docs/superpowers/results/2026-10-19-mesh-vs-voxel-ruts-ds.md`, new 'Run 12' section).

After the session: checkpoint save, update `docs/superpowers/plans/2026-10-18-perf-plan.md` (new section 'REBUILD 6')
and the ruts feature README with kept / dropped per item.
