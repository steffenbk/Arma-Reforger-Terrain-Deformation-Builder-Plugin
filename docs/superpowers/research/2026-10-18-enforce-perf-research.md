# Fast Enforce Script + why 'simulation' ms rises (2026-10-18)

Read-only research. Exe = Experimental `ArmaReforgerSteamDiag.exe` in IDA (base 0x140000000, same build as
2026-10-18-perf-research-ida-web.md - facts there are not repeated). Labels: IDA = read in the exe (not run),
LOG = our own log lines (file + time), WEB = URL, CODE = our scripts, INFERENCE = my reading, not measured.
Note: community.bistudio.com (biki) sits behind a Cloudflare bot check today; its pages could not be read, only
search snippets. The 2026-10-18-mud-mesh-cost-research-findings.md doc already quotes the biki performance page
(static arrays fast, `sealed` + one-line-return inlining, cache Count(), foreach vs for) - not repeated.

## Summary (plain)

1. The 'simulation' number is NOT the physics step alone: it is the whole world update, and our own EOnFrame
   script runs inside it. In every log row, simulation minus 'MUD script' stays ~2.0-2.6 ms whether 0 or 15
   colliders/s are rebuilt. So the 2.2 -> 6.2 ms rise is our script, not Bullet. Rebuilding bodies costs physics ~0.
2. The physics engine is Bullet (btDbvtBroadphase, btBvhTriangleMeshShape), not Jolt. Jolt docs do not apply.
3. The script VM is a plain interpreter (no JIT): one jump per instruction, and every native call (Math.*, map.Get,
   Vector(), GetTickCount) goes through a generic bridge with several checks. Fewer calls in hot loops = faster.
4. map<int,float> is a real hash table (fast inside), but each Get is still a native call. Our collider loop does
   ~4 map lookups + ~12 small script calls per vertex (1089 vertices); a per-tile cache would remove most of them.
5. The engine script profiler (per-function time + call count) exists, but draws only on screen: use it on a Diag
   client, not on the DS. On the DS keep MUD_Prof + System.GetProfileData.

---

## Q1. Enforce VM cost model

**Finding: classic switch interpreter; native calls are the expensive unit; maps are hash tables behind a native
call; vectors are values.**

What the exe shows (IDA):
- Interpreter loop = sub_140A37570 (0x7C84 bytes; strings 'VM Recursion > 256. Infinite loop?', 'Division by zero',
  'Index out of range (%d >= %d)', 'NULL pointer to instance'). Dispatch at 0x140A37983: `opcode = word & 0x7F`,
  `jmp [table 0x140A3F098 + opcode*4]` - 128 slots, 118 distinct handlers. Per instruction the fast path also bumps
  a counter, checks a suspend flag (ctx+0x280) and a debugger byte (ctx+0x2B0); every 0x100000 instructions it
  polls a key (deadlock check). No JIT strings / code found. Confidence high.
  INFERENCE: one VM instruction ~ a few ns; a short `if (a < b) a = b;` is 3-5 instructions.
- Native bridge: Math natives are registered by sub_140E5EA60 (Round, Floor, Ceil, Min, Max, Sin ... Clamp, Lerp,
  InverseLerp, AbsFloat, Sqrt). Math.AbsFloat = sub_140E5A7A0: reads the arg through a pointer table, calls
  sub_14099D750, sub_14099B820 (x2), sub_14099D850 and type-checks the return slot (flags & 0xF0000000) before
  writing the float. So `Math.AbsFloat(x)` = call opcode + bridge + 4 helper calls + fabs. Confidence high (code);
  the ns cost is INFERENCE (~10x a plain compare).
- Containers (IDA RTTI, all in enf::):
  - `array<float>` / `array<int>` / `array<vector>` = GenericArray<float/int/Vector3>: contiguous values.
  - `map<int,float>` = `MapT<HashMap<int,float,DefaultHashingPolicy,CompareEqualPolicy>>` (RTTI 0x14243d930,
    vtable 0x141ca7478). Find/Get slot sub_140AEC0D0: FNV-1a over the 4 key bytes, 7-bit control bytes, linear
    probing (swiss-table style). Fast inside. But script `map.Get` = native sub_140AD8D90 -> virtual call
    (vtable +88) -> hash probe -> type-checked out-slot. So the cost is mostly the bridge, as for Math.*.
  - `set<int>` = `GenericSet<int, CompareGreater>` = a SORTED array (binary search; Insert shifts elements).
    Contains is O(log n), Insert is O(n). Not a hash set. Confidence high (RTTI name).
  - Map methods registered by sub_140AD9CE0 (Get, Find, Contains, Set, Insert, Begin/End/Next iterators ...).
- `vector` = Vector3 value (12 B) - stored inline in arrays (GenericArray<Vector3>), no heap object. Confidence high
  for storage; INFERENCE that `+ - *` on vectors are VM opcodes while `Vector(x,y,z)`, `.Length()`,
  `vector.Distance` are native calls.
- Relative cost ladder (INFERENCE from the code above, not timed):
  local int/float math < static-const / member fixed array `[i]` (VM index op + bounds check) ~ `array<T>[i]`
  (bounds-checked, extra indirection) < plain script method call (frame push; 'VM Recursion > 256' shows a call
  stack) < any native call (Math.*, Vector(), map.Get/Contains, Count()) < native that returns a `ref` object
  (map<int, ref X>.Get - refcount work) < `new` object / array (allocation + later GC/refcount free) <
  string.Format / string + (allocations) <<< Print on a DS (LOG: ~2.2 ms per Print line on the DS, 0.06 ms on a
  client - perf-assessment.md item 1).
- static vs instance vs protected: all are script calls; `protected` is a compile-time check only (the runtime
  string 'method is private/protected' is for ScriptModule.Call by name). Only `sealed` + single-return methods are
  inlined (WEB https://reforger.armaplatform.com/news/modding-update-scripting-1-1). INFERENCE: static vs instance
  differs by one hidden 'this' argument - negligible.
- int/float conversion, inout/out: INFERENCE: VM ops / pointer args, cheap; not worth changing code for.
- foreach vs for: foreach is quoted faster on the biki (see mesh-cost doc); for `array<T>` the bigger win is not
  calling `Count()` (a native) every iteration.

Where this bites us (CODE, MUD_WorldGround.c BuildColliderBand 7212-7270, per vertex, 33 x 33 = 1089 times):
- `MUD_VoxelProbe.FieldFloor` -> `FloorSample` -> `FloorDiv` x2 + `ColKey` + `m_mRutTiles.Get` (a
  `map<int, ref MUD_RutTile>`, MUD_VoxelProbe.c 576) - and the same chain again for `FieldRidge` -> RidgeSample.
  That is ~2 native map gets + ~10 script calls per vertex, although FSUB = 16 (MUD_VoxelProbe.c 348), so one patch
  touches at most 3 x 3 = 9 tiles.
- `Math.AbsFloat` x2, `Math.Max` x2, `p.RutTouch(v)` / `p.BermTouch(v)` (script calls), `Vector(...)` for the plate.
- The tri sort loop (7322-7358) runs once per material slot over all tris (already in the 2026-10-18 IDA doc).
- The FireGeo slab (7388-7405) builds a second full trimesh (all tris) every build: double native BVH work.

## Q2. Script profiler / per-function timing

**Finding: yes - engine per-function timing exists; on-screen only. DS: no UI, use our own timers.**

- IDA: diag menu Statistics > 'Script profiler' = off / frame / average / memory (sub_1401489D0, string 0x141c76dc8)
  and 'Script prof. external'. Display code sub_1409A42B0 prints panels 'Time per function (us)', 'Count per
  function', 'Time per class (us)', 'Class count', 'Class allocations', 'Class count * size(bytes)', padding.
  It sets ctx+0x2B2 = 1 (profiling on) while shown.
- Per-call timing in the VM: when ctx+0x2B2 is set and the function has flag 0x10, call entry stores a
  QueryPerformanceCounter stamp and bumps a call counter (0x140A35CDF), exit adds the elapsed ticks to the
  function total (sub_140A36160, 0x140A37440). So it is a true per-function inclusive timer.
- Start options (sub_140922D60): `-profiler` sets ctx+0x2B2 from launch (profiling on all the time);
  `-scriptExtProfiler` (flag for the external profiler, = 'Script prof. external'); `-checkInstance` (WEB search
  snippet of https://community.bistudio.com/wiki/Arma_Reforger:Script_Profiling: needed for the memory /
  DumpInstances view); `-debugger <host>` / `-debuggerPort`; `-VMErrorMode silent|log_only|full|fatal`.
- Steps (client, function view; perf numbers only from the game exe, not WB):
  1. Start the Diag exe client (or the peer client) with `-profiler` (optional; without it the panel turns
     profiling on only while shown).
  2. Open the diag menu (Win + Alt in WB viewports per WEB https://community.bistudio.com/wiki/Arma_Reforger:Diag_Menu
     snippet; in the Diag exe the same menu), Statistics > Script profiler = average (or frame).
  3. Drive the robot run; read the 'Time per function (us)' rows for BuildColliderBand, BuildMeshBand,
     FloorSample, RelaxFloor, Band.Build. F1 saves the menu state (diagmenu.save) for the next launch.
  4. Screenshot the panel - nothing is written to the log (no file / Print path found in sub_1409A42B0).
- DS: no window, so the panel cannot be read. INFERENCE: the per-call timer still adds cost when on, so do not
  leave `-profiler` on for perf runs. Keep MUD_Prof / GetTickCount sums on the DS (gotchas: GetTickCount sums are
  unbiased).
- Community tool: WEB https://reforger.armaplatform.com/workshop/6686460F377B64FD (Remote Script Console) has
  `#diag cq ...` commands for call-queue spike causes on live servers - not checked in code.
- Confidence: high for what the profiler measures (code); medium for the exact menu keys in the Diag exe.

## Q3. Physics: does rebuilding static colliders cost 'simulation' time?

**Finding: no measurable physics cost. 'simulation' = the whole world update incl. our EOnFrame script, and
the rise matches our script ms exactly.**

What 'simulation' is (IDA):
- `System.GetProfileData` native sub_140E6D620 copies game+248 into SimulationTimeMs (render = +184, GPU = +232,
  frame = +400). sub_14092B880 sets +248 = (+240 accumulator) x 0.1 every 10 frames (10-frame average).
- The only writer of +240 is sub_140B83300 = the native `BaseWorld.UpdateEntities` (registered in sub_140B80650,
  string 'World::InternalUpdateEntities'): it times the world's update (vtable +0xA0) and adds the ms to +240.
  That update runs entity events - our MUD_WorldGroundEntity.EOnFrame included. So SimulationTimeMs = entity /
  component / script frame work + physics. Confidence high (code); INFERENCE that the Bullet step runs inside the
  same update (strong: nothing else adds to +240).

Control (LOG, `MUD prof 5 s` lines, same robot run per pair):
| log | rows | sim ms idle -> driving | MUD script ms idle -> driving | sim - MUD |
|---|---|---|---|---|
| PeerPlugin1 logs_2026-10-18_22-31-34 (client, 75-133 fps) | 11 | 2.18 -> 6.50 | 0.22 -> 4.40 | 1.96 .. 2.23 |
| server logs_2026-10-18_22-31-32 (DS, 30 fps) | 13 (first row = load) | 1.51 -> 10.25 | 0.26 -> 7.99 | 1.17 .. 2.68 |
| PeerPlugin1 logs_2026-10-18_16-26-52 (client) | 6 | 2.23 -> 6.29 | 0.22 -> 4.25 | 2.01 .. 2.53 |
| server logs_2026-10-18_16-26-51 (DS) | 9 (first = load) | 1.70 -> 6.67 | 0.25 -> 4.29 | 1.45 .. 2.62 |
Client 22:32 rows in full (time, fps, sim, MUD, sim-MUD): 22:32:06 133 2.68 0.07 2.61 | 22:32:11 103 2.18 0.22 1.96 |
22:32:17 110 2.38 0.27 2.10 | 22:32:22 90 4.10 2.12 1.98 | 22:32:27 81 5.52 3.39 2.13 | 22:32:32 80 6.13 4.05 2.08 |
22:32:37 79 6.35 4.12 2.23 | 22:32:42 81 6.03 3.94 2.09 | 22:32:47 75 6.50 4.40 2.10 | 22:32:52 80 6.20 4.09 2.11 |
22:32:57 84 5.96 3.92 2.04. Colliders in the same log: 42 / 128 / 148 / 145 per 10 s (238 / 701 / 818 / 840 ms) =
up to ~15/s. Check: 6.50 - 4.40 = 2.10. While colliders went 0 -> ~15/s, sim - MUD moved < 0.3 ms on the client
and up to ~0.5-1 ms on the DS (DS rows 2.05-2.68 driving vs 1.17-1.50 idle - also includes vehicle physics and MUD
work outside MUD_Prof parts, so this is an upper bound for any rebuild physics cost).
Caveat: PeerPlugin1 is a WB-started peer client - fine for this RELATIVE check (same machine, same run), not as a
perf number (TESTING RULES).

What Bullet does on add/remove (for completeness; all inside our script's CreateStaticEx / Destroy calls or tiny):
- IDA: btDiscreteDynamicsWorld (stepSimulation strings 0x141e865e0), btDbvtBroadphase (RTTI 0x1423eb1f0,
  optimizePart / optimizeFinal / collide strings), btHashedOverlappingPairCache with cleanProxyFromPairs /
  removeOverlappingPairsContainingProxy (RTTI 0x14248b820 / 0x14248b8b0), btBvhTriangleMeshShape::buildOptimizedBvh
  (0x141e886d8, built in CreateTriMesh = our script time; sub_140A52060 per the 10-14 doc).
- WEB https://raw.githubusercontent.com/bulletphysics/bullet3/master/src/BulletCollision/BroadphaseCollision/btDbvtBroadphase.cpp:
  createProxy = one insert into the dynamic tree (moved to the fixed tree after a few frames); destroyProxy = one
  tree remove + a pass over the pair cache to drop its pairs; collide() per step does bounded incremental work
  (~1 % of fixed leaves, ~10 % of pairs for cleanup). New pairs (wheel/hull/character vs the new body) get a fresh
  contact manifold - one step without warm start. INFERENCE: microseconds per body at our rate.
- So these script-side levers do NOT help sim: SetActive, swapping shapes, broadphase tricks. Smaller bodies /
  fewer tris help only via OUR CreateTriMesh time (BVH build is ~0.14 us/tri: 0.28 ms per 2048 tris, gotchas
  2026-10-09).

Confidence: high (code + 4 logs agree).

## Q4. Other ways to make script faster here

- **Print**: proven ~2.2 ms per line on the DS (perf-assessment item 1). Biggest per-call cost we have.
- **One build per frame budget beats more work per frame**: already used (Rebuild one collider per frame); keep
  all new per-frame work behind ms budgets.
- **EOnFrame vs ScriptCallQueue**: CallLater/Call keep the args in memory and run from the game's queue Tick
  (API text, enfusion api_search ScriptCallQueue). INFERENCE: no speed gain over one EOnFrame that dispatches
  everything; queue per-item has a by-name/func lookup and arg copy. Keep the single EOnFrame (it is also what
  MUD_Prof measures).
- **Entity events / flags on patches**: CODE: MUD_FinePatchEntity sets no event mask (only MUD_WorldGroundEntity
  and MUD_CoarseSkin take FRAME). So no per-patch frame cost to remove. TRACEABLE only matters for traces (cost
  is in the trace, not per frame); VISIBLE off skips draw. INFERENCE: no gain from more flag clearing.
- **Spatial lookups**: since 1.1 the engine has AABGridMap / CircleGridMap / PointGridMap for manual spatial
  indexing (WEB modding-update-scripting-1-1). Candidate for FieldDue's all-patches x all-sources scan
  (perf-assessment item 8) - native grid instead of script loops. Not checked in API dump.
- **Allocations in hot paths**: `array<PhysicsGeom> geoms = {}` etc. (3 arrays per collider) are small; per-op
  `new` maps in RelaxFloor (perf-assessment item 3) matter more.

---

## Rules of thumb for this mod

1. Measure 'MUD script' (MUD_Prof), not 'simulation', when judging ground cost - simulation includes it. Physics
   cost of our rebuilds ~0; sim - MUD ~2 ms is the baseline.
2. No Print in anything that runs per op / per build on the DS; flag-gate every debug line.
3. In loops over vertices / tris: no native calls you can avoid - hand-write abs / min / max / clamp
   (`if (d < 0) d = -d;`), cache `Count()` once, build vectors as members only when needed.
4. Never look up a map per vertex: fetch the tile (map.Get) once per tile block, keep the `ref` in a local, index
   its fixed array directly. Same for ColKey / FloorDiv: compute once per row/block.
5. Use `set<int>` only for small sets (it is a sorted array); use map<int, X> or a bit array for big ones.
6. Small helpers in hot loops: make them `sealed` single-return (inlined) or inline by hand; static vs instance
   does not matter.
7. Fewer triangles into CreateTriMesh = less of OUR time (BVH build), not less physics step time.
8. Profile functions with the Diag client's Script profiler (average), read 'Time per function'; never on WB for
   numbers, never left on for a perf run.

## Try next (ranked: saving x chance / effort)

| # | What | Why / evidence | Effort | Test |
|---|---|---|---|---|
| 1 | Per-tile cache in BuildColliderBand + BuildMeshBand: walk vertices tile block by block (FSUB 16), one `m_mRutTiles.Get` per tile (9 max per patch) instead of 2 per vertex; index `tile.m_aFloor` / `m_aRidge` directly | ~2 native gets + ~10 script calls x 1089 vertices per build now (CODE 7237-7248, VoxelProbe 5148-5168); input share of collider ms was 44 of 220 (IDA doc Q2 row) | low-medium | must stay bit-identical (same floats): compare_op_hashes + collider height dump on one patch; then DS 'collider split ms' input part, >= 6 runs |
| 2 | Script profiler pass on a Diag client during the robot run: get the real per-function us table before more guessing | gives the true split of the 4 ms/frame client MUD script | low | Q2 steps; screenshot 'Time per function' |
| 3 | Inline the hot-loop natives: Math.AbsFloat / Math.Max (7246, 7257, 7262, 7265), RutTouch / BermTouch as direct `m_Press` array reads with one null check per patch | each is a native/script call per vertex | low | same as 1 |
| 4 | FireGeo slab: build it only when the floor actually moved down by more than X, or share it with the previous build when only the top changed; or one slab at lower resolution | full second trimesh per build = second BVH build (CODE 7388-7405) | medium (bullet-through-ground risk, gotchas 2026-10-10) | WB function check with grenades/bullets; then DS trimeshes ms |
| 5 | Treat 'sim - MUD script' as a tracked number in MUD_Prof's 5 s line (log it) | catches any future physics cost of rebuilds directly | very low | read in next DS run |
| 6 | Try PointGridMap for FieldDue source lookup | native spatial index vs script scan | medium | count + ms per 10 s |
| - | Not worth it: SetActive / shape swap / collision-layer tricks for sim time; ScriptCallQueue instead of EOnFrame; more EntityFlags clearing on patches | Q3 control table; patches have no event mask | - | - |

## Evidence index (IDA this pass)
- sub_140E6D620 GetProfileData (game +184 render, +232 GPU, +248 sim, +400 frame); sub_14092B880 10-frame averages.
- sub_140B83300 BaseWorld.UpdateEntities native: only writer of game +240 (sim accumulator); registered in sub_140B80650.
- sub_140A37570 VM interpreter; dispatch 0x140A37983, table 0x140A3F098 (opcode & 0x7F, 118 handlers).
- sub_140A35BD0 / 0x140A35CDF call-entry stamp; sub_140A36160 / 0x140A37440 call-exit add (ctx+0x2B2 profiler on).
- sub_1409A42B0 script profiler display ('Time per function (us)' ...); sub_1401489D0 diag entry; sub_140922D60
  start options (-profiler, -scriptExtProfiler, -checkInstance, -debugger, -VMErrorMode).
- sub_140E5EA60 Math natives; sub_140E5A7A0 Math.AbsFloat body.
- sub_140AD9CE0 array/set/map method registration; map Get sub_140AD8D90 -> vtable 0x141ca7478 +88 = sub_140AEC0D0
  (hash probe). RTTI: MapT<HashMap<int,float>> 0x14243d930, GenericSet<int,CompareGreater> 0x14243ced0.
- Bullet: btDbvtBroadphase RTTI 0x1423eb1f0, btHashedOverlappingPairCache RTTI 0x14248b7e0, stepSimulation 0x141e865e0.
