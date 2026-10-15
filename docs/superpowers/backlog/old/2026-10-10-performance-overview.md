# Mud ground - performance overview and improvement list (2026-10-10, deep pass 16:0x)

**Ranking now lives in [2026-10-10-master-list.md](2026-10-10-master-list.md) (reviewed 17:5x). Correction: N5 net
rate is ~13-15 msgs/s, 5.5-6.5 KB/s per client per driving vehicle (stats line prints only when busy), not 2.4 KB/s.**

Scope: Plan 3 deformable ground (coarse skin + 8 m fine patches + MP sync + JIP) on Arland, Exp 192624.
Numbers are quoted from results docs / the Plan 3 ledger (named per row) - nothing re-measured today.
Labels: **MEASURED** (number exists), **CODE** (read from our script 2026-10-10), **PAK** (read from the Exp 192624
pak 2026-10-10), **ONLINE** (outside source, linked), **INFERENCE** (not proven - needs the probe named).

## 1. Where the time and memory go today

### Per-patch costs (fine patch, 33 x 33 verts, 0.25 m)

| Step | Cost | Source |
|---|---|---|
| Setup (StartPatch: data, base, lock, entity) | ~2.9 ms (was ~5.1 before cell cache) | ledger 13:47 2026-10-09 |
| Index lists, cache miss | ~2.5 ms (hits 92 % making, 100 % rebuild) | ledger perf fix 1 |
| Collider (smooth + CreateTriMesh x 3-6 + body swap) | ~1.4 ms (was 4.7) | ledger 'Perf after index cache' |
| Mesh (vertex loop + MeshObject.Create + Update* + SetObject) | ~1.1 ms (was 2.4) | same |
| Skin cover / uncover / sleep swap | ~1 ms each; 156-176 ms per 10 s while driving | task3-cover.md, task5-ruts.md |
| UpdateIndices + UpdateVerts alone | 0.24 ms (bench 12 ms / 50) | ledger bench 12:54 |
| MeshObject.Create alone | ~0 ms | same |
| CreateTriMesh 2048 tris (BVH) | 0.28 ms | same |
| Live patch memory | 340-400 KB; sleep (collider drop) gives back 229-284 KB | task5-ruts.md, ledger 15:30 |

### Frame time (worst frame per 10 s, gate <= 10 ms)

| Situation | Worst frame | Source |
|---|---|---|
| Rebuild only | 5-6 ms | ledger |
| Driving fresh ground after Fix B/C | 4-10 ms | ledger 14:37, 14:46 |
| Wheel makes a patch at once (ruled case: setup + index + collider + mesh + cover in one frame) | 11-12 ms | task5-ruts.md, task6 |
| Before fixes | 21-38 ms | task4-patches.md |

### Made vs used

Task 5 drive (task5-ruts.md 15:20-15:22): made 363, deformed 110 at the end (patches 333, 30 freed).
**At least ~70 % of the patches built never got a rut** and each paid the full build (~8-9 ms over several frames)
plus ~340 KB until freed after 60 s.

### Whole map

| Item | Number | Source |
|---|---|---|
| Coarse skin build at load | heights 3.6 s -> 2.4 s with baked file | task11-baked.md |
| Coarse skin memory | +264..353 MB (1697 chunks, 2.68 M tris) | plan2-task3-skin.md |
| Net, Task 9 run | 1054 msgs / 112771 ints = ~107 ints (~430 B) per msg | ledger line 377 |
| Net latency send -> client collider | median 55 ms, p90 81, max 145 | ledger stutter probe 11:31 |
| JIP | 100000 fake keys = 400 KB RplSave, joins OK | task10-jip.md |

## 2. Deep pass per item

### O1. Lazy patches - build collider, mesh and cover only on the first dig  (TOP PICK)

How it works today (CODE, MUD_WorldGround.c): `Ahead` picks patches within 16 m (m_fAheadRadius) of any moving
wheeled vehicle, nearest first, and runs `StartPatch` (setup) then `BuildStep` per frame: index -> collider ->
mesh + `SetCovered` (skin chunk rebuild). `Press` calls `EnsureRange(..., wheel=true)` and skips the whole press
while any patch in range is missing, building or asleep (a dig next to undug skin would open a crack).

Change: Ahead stops after setup + index (a new state, e.g. `BUILD_READY`). An undeformed patch is identical to the
skin, so nothing is lost by not showing it. The first press / crater / dig on it calls `FinishNow`
(collider + mesh + cover). Undeformed READY patches are freed after 60 s as today (no cover to undo).

| | Today | Lazy |
|---|---|---|
| Work per never-dug patch | setup 2.9 + index 0-2.5 + collider 1.4 + mesh 1.1 + cover ~1 + uncover ~1 | setup 2.9 + index 0-2.5 |
| Memory per never-dug patch | ~340 KB | script arrays ~30 KB (7 float arrays x 1089) + entity (INFERENCE) |
| First press on a patch | ~0 (already live) | collider + mesh + cover ~3.5-4.5 ms in that frame (vs 11-12 ms today when the wheel meets an unmade patch) |
| Skin swaps | 156-176 ms / 10 s | only for dug patches |

Risks: (1) WHEEL_BUILDS_PER_FRAME = 1 -> 4 wheels entering 4 new patches finish one per frame; skipped presses are
lost (rut a touch shallower at patch borders) - same rule as today's capped case. (2) On a DS (no render) the cover is
a skin collider rebuild - same timing. (3) Seams: a READY patch has no mesh; `SeamCheck` / `CheckNew` must skip it.
Probe: same 70 s off-road drive; compare made / finished / deformed counts, worst frame, mem per 10 s.

### P1. MP driving stutter (small one left after the 15 cm allowance)

MEASURED: reverse jumps were client-only server corrections (0.15-0.61 m); vanilla allowance = speed/300 m (IDA).
Allowance 15 cm: "definitely better, small stutter left". Lag probe: client collider 55 ms after send, server's own
rebuild 103 ms after send - neither is far behind, but they change at DIFFERENT moments.

How a rut lands (CODE, `Press`): a vertex inside the rut radius sinks by m_fRutStep (0.08 m) at the START of a pass
(first touch after 0.6 s free), i.e. while the wheel is on it. The collider follows at the next 0.2 s rebuild.
So the ground under a wheel drops 8 cm while the wheel sits on it, at a slightly different moment on each machine.

**P1a - ground changes only behind the wheel** (INFERENCE, cheap): keep the collider at its previous height for any
vertex a wheel touched in the last ~0.15 s; take the new height at the first rebuild after the wheel left. Needs:
a `m_aColH` copy per patch (+4 KB), a wheel-touch map on clients too (clients run a touch-only wheel loop for their
local vehicles; no Press), and a re-dirty when a hold runs out. Then no machine ever moves ground under a turning
wheel; rear wheels / other vehicles still meet the finished rut. Visual mesh may use the same rule (else the wheel
looks 8 cm above its rut for a moment). Probe: jolt probe (ledger 11:4x) before / after - client-only jumps should
fall to ~0.
**P1b - driver client predicts its own ruts** (ONLINE pattern: [Chrono SynChrono](https://api.projectchrono.org/terrain_synchronization.html)
syncs deformed nodes as state; [GameDev rollback thread](https://gamedev.net/forums/topic/713082-rollbacks-and-simulation-replay-performance)
names ground changing after the client simulated over it as a misprediction source). Bigger change; only if P1a
is not enough.

### P2. White crack under ruts (intermittent, minor)

MEASURED: heights match (seams 0). INFERENCE: T-junction pixel gaps - the fine patch edge has 8 verts where the skin
edge has 1 segment. ONLINE: the standard fixes are skirts or stitching
([gamedev.net terrain LOD cracks](https://gamedev.net/forums/topic/713470-terrain-lod-and-cracks),
[Cesium skirts](https://community.cesium.com/t/skirts-tablecloth-overhangs/1864),
[T-junction pixel artifacts](https://www.gamedev.net/forums/topic/437850-t-junctions-degenerate-triangles-pixel-artifacts/3903880/));
skirts are the low-risk default and Cesium keeps them always on.
Our skirt try was a 3rd submesh and CRASHED Workbench at the first rut (cause inside it unknown).
- **P2a** skirt in submesh 0: +4 x 33 x 2 = 264 verts (edge verts copied 0.5 m down), +4 x 32 x 6 = 768 indices,
  part of the cached index layout. No new submesh, no material used twice. First try.
- **P2b** skirt as its own one-submesh entity (if P2a crashes too).
- **P2c** stitching: skin cells next to a live patch draw a fan through the patch's 9 edge verts (16 layouts, cached
  like our index lists). Cleanest, most work.

### P3. Blasts pass through crater / trench walls (backlog item 14)

PAK (Exp 192624, read 2026-10-10):
| Piece | Values |
|---|---|
| ExplosionContainer_Base.conf | `LayerPreset "Projectile"`, effects: BlastDamageEffect, BlastDamageEffect_NoTrace, Incendiary, BlastImpulse |
| BlastDamageEffect.conf | DamageValue 1000, DamageDistance 10, traced (default) |
| BlastDamageEffect_NoTrace.conf | DamageValue 333.333, **PerformTraces 0**, DamageDistance 3 |
| ExplosionContainer_HEF.conf (grenade + mortar) | NoTrace DamageDistance overridden to **1 m** |
| FragmentationDamageEffect.conf | DamageValue 16, DamageDistance 25, 300 frags; M67 400, mortar M821 **2800** |
ONLINE ([BI 1.2.1 news](https://reforger.armaplatform.com/news/modding-update-sept-10-2024)): traced effects stop on a
gamemat whose "Stop Explosion Trace" is on; damage scales with the hit zone's exposed angular size.

So in a crater only the 1 m no-trace blast should hit; blast + 2800 frags are traced. Bullets (also Projectile)
DO stop in our crater walls (Task 6, user) -> the layer is very likely fine.
Suspects left (INFERENCE): (b) our collider gamemats are the terrain surface gamemats (grass_lush_tall, dirt ...) and
those may have Stop Explosion Trace off (terrain itself may be handled another way); (c) the engine tests terrain
cover via the heightmap, which is holed under our ground.
Probe order: (1) vanilla control: crouch behind a rock / wall vs open ground, same mortar distance. (2) TEMP: on each
explosion, trace Projectile layer from the blast to each player within 25 m, log hit entity + gamemat. (3) read the
`Stop Explosion Trace` flag of grass_lush_tall / dirt gamemats (binary in pak: Workbench resource browser or
blender-bk pakread).
Fix routes: (b) -> our own copies of those few gamemats with the flag on, or one extra thin "blast shell" geom just
under the surface with one flagged gamemat (costs a geom slot: MakeStatic max 6, see O2 which frees two);
(c) -> modded explosion effect that adds our own cover trace (heavier).

**Mega dive 2026-10-10 (IDA on ArmaReforgerSteamDiag.exe as loaded + PAK Stable, Workbench not running):**
- IDA: the blast trace is class `gamecode::ExplosionDamageTrace` (vtable ref in sub_1405B4300, queued from
  sub_1405B71B0). Trace flags = 198 = ENTS 2 | WORLD 4 | ALL_CONTACTS 0x40 | 0x80 (0x80 not a script flag, meaning
  unknown). TraceFlags values read from the enum registration at sub_1409A06E0. Start fraction 1.1, keeps the NEAREST
  blocking hit.
- IDA: hit callback sub_1405B91A0 counts a hit as cover if `sub_1402A4DC0(entity)` (type-hash 0xD4C6F039 =
  **GenericTerrainEntity**, from its vtable/RTTI) OR `sub_140189500(material)` (reads the gamemat property registered
  as "StopExplosionTrace", default string **"1"**, checkbox). Any other hit is skipped and the trace goes on.
- PAK Stable: all 32 ground gamemats resolve to StopExplosionTrace 1 (grass.gamemat sets 1 explicitly, the rest
  inherit the default; only vegetation_base, wood, metal, glass, fabric... set 0). Script: gm_chain.py, scratchpad.
- => suspect (b) is **dead** and (c) is unlikely (ENTS is in the flags): our patch/skin geoms (Terrain|Foliage layer,
  terrain gamemats) should count as cover. Left: (d) the in-game probe (2) above - does the trace hit our entity at
  all (layer mask from the container, unknown flag 0x80); (e) the damage came from the 1 m NoTrace blast or from
  exposed body parts (2 m crater, see N2 below). Probe (1) + (2) decide it; no fix before that.

### P4. Skin cover rebuilds are a big hidden cost

CODE (MUD_CoarseSkin.c `SetCovered` -> `BuildGeometry`): covering 4 x 4 cells rebuilds the WHOLE 64 m chunk: vertex
loop 1089, index loop 1024 cells, MeshObject.Create, then one CreateTriMesh per material slot (up to 6) over the whole
chunk and a new static body. MEASURED ~1 ms each; 156-176 ms per 10 s while driving.
- O1 removes most of it (only dug patches cover).
- **P4b** 32 m skin chunks: each rebuild ~1/4 the work, 4x skin entities (1697 -> ~6800 with holes; INFERENCE on
  cost, measure init + memory first). Only if covers still show up after O1.

### O2. Coarse safety plate + bullet slab  (good second pick)

CODE (`BuildCollider`): per patch up to 4 surface trimeshes (all fine cells together = 2048 tris) + slab
(`m_aShell`, 2048 tris, FireGeo) + plate (`m_aPlate`, 2048 tris). **Slab + plate are 4096 of ~6144 tris = 2/3 of
the collider.** Both are flat-ish helpers: plate catches anything that falls through (base + floor - 0.5 m),
slab is the 1.5 m "exit face" that makes bullets stop in the ground (facing down).
Change: build both at 2 m (5 x 5 verts, 32 tris each), using the LOWEST fine value inside each 2 m cell, so they
never come up above the fine surface.
Expected (INFERENCE, if collider memory/time scale with triangles): collider ~1/3 the triangles -> a large part of
the ~270 KB collider per live patch and of the 1.4 ms build. Risk: slab thicker in places (min of a crater cell) -
bullets should still stop (more material, not less); redo the Task 6 bullet-in-crater-wall check.
Bonus: frees nothing in slot count (still 2 geoms), but makes a 7th geom idea (P3 blast shell) cheap if slots allow.

### O3. Reuse the mesh resource

CODE: `BuildMesh` calls MeshObject.Create + SetObject + Update on every rebuild; skin `BuildGeometry` too.
MEASURED: Create ~0 ms, Update* 0.24 ms -> the win is small (allocation churn, maybe memory). **Downgraded: only
if a memory probe shows churn.** (IDA 2026-10-09: UpdateVerts writes in place, so it is possible.)

### O4. 4 m collider pieces under an 8 m mesh (the "8 x 8 with different detail" idea)

CODE: the body is rebuilt as a whole (`Physics.CreateStaticEx` with all geoms); splitting geoms per quarter still
recreates the body, so the win is only the CreateTriMesh + loop part of 1.4 ms. INFERENCE: small. Keep 8 m patches.
0.5 m verts: rut radius 0.5 m would be one vert wide - no.

### O5. Ahead by direction

CODE: Ahead uses a circle of 16 m (direction-free). A cone of +-60 deg x 16 m plus a 6 m circle (turns, reverse)
covers about half the area (16^2 x pi/3 + 6^2 x pi ~ 381 m2 vs 804 m2). With O1 a READY patch is cheap, so this is
low value after O1. Skip unless O1's numbers say setup still hurts.

### O6-O8 (unchanged, low)
- O6 = P4b. O7 JIP skip flat runs (backlog 13): 400 KB still joined fine - low.
- O8 stamps as "type + pos" (backlog D): net ~20 B per crater instead of a rect; float drift risk, hash compare
  catches it ([SynChrono](https://api.projectchrono.org/terrain_synchronization.html) chose state sync for that reason).

Not worth it (MEASURED): BVH / CreateTriMesh (0.28 ms), MeshObject.Create (~0), script patch arrays (~30 KB of
~340 KB per live patch).

## 3. Better experience (not speed)
- Ruts dig TO a depth (Valheim, picked A) - also fewer rebuilds once a lane reached its depth.
- Depth by ground type (B), side ridges (C).
- Blast cover in craters (P3) - makes the feature matter in combat.

## 3b. Free ideas (16:1x pass) - beyond the known issues

| # | Idea | Why | Evidence / label | Size |
|---|---|---|---|---|
| F1 | **Release defaults: Debug off** | `m_bDebug` default is 1 (attribute line 173): stats every 10 s, wheel log every 1 s, and `CheckNew` traces every new patch | CODE | tiny |
| F2 | **Stress test before release** | every number we have is ONE vehicle; nothing measured with 5-10 vehicles, many craters/min, 30+ players on a DS | gap | small (peer clients / AI drivers) |
| F3 | **Ruts heal over time** | deformed patches are kept for the match: budget 2000 = ~800 MB live / ~120-340 MB asleep (ledger 15:30). Long public sessions only grow. Shallow ruts fading back to 0 after N min let patches be freed; craters / trenches slower or never | MEASURED memory, CODE (no decay exists) | medium |
| F4 | **Send changes only to players who can see them** | `RpcDo_Heights` is `RplRcver.Broadcast` - every rut message (~430 B) goes to every client on the map. Far players could get the patch later through the JIP ask path we already have (ask on approach) | CODE | medium |
| F5 | **Survive server restarts** | nothing saves the ground today. The game has a built-in PersistenceSystem (`SCR_PersistenceSystem.GetOnBeforeSave`, `TriggerSave`, API 1.9); our JIP full-copy format is already a save format | API (api_search 2026-10-10), CODE | medium |
| F6 | **Wet ground digs deeper** | `BaseWeatherManagerEntity.GetCurrentWetness()` exists -> scale rut step / depth with rain. Cheap, very visible | API | small |
| F7 | **Mud drag** (MudRunner LinResistance) | `Physics.ApplyForceAt` exists, so drag in deep ruts / soft ground is possible. Must run on the simulating machine the same way on server + driver, else MP corrections (P1) | API; untested on a wheeled sim | medium, risky |
| F8 | **DROPPED (user 2026-10-10: PvP only, no AI)** - AI knows the holes | navmesh is prebaked; AI may walk/drive into 2-4 m craters and trenches. `SCR_AIWorld.RequestNavmeshRebuildAreas` exists (used for GM building). Probe: does a rebuild see our trimesh ground, and what does it cost on the server | API; INFERENCE on AI behaviour | probe first |
| F9 | **Grass in craters** | clutter follows terrain surfaces; unknown whether grass floats over a crater / rut at the old height. Look check in-game | INFERENCE | look first |
| F10 | **Server-owner settings** | ruts / craters on-off, depths, budget, decay from the server config or mission header, not only entity attributes | product | small-medium |
| F11 | **GM "repair ground" brush** | let a Game Master flatten an area back (uses the same SetH + net path) | product | small |
| F12 | Trimesh seam bumps | known Bullet trimesh issue: contacts bump on internal triangle edges ([Bullet internal edge utility](https://www.bulletphysics.org/Bullet/phpBB3/viewtopic.php?f=9&t=7794), [jME flat-ground bumps](https://hub.jmonkeyengine.org/t/physics-shapes-collide-on-flat-surfaces-triangle-boundaries/35946)); vanilla terrain is a heightfield, ours is trimesh. SP drive had no real stutter, so low - but check with the jolt probe on flat undug skin if P1a leaves some | ONLINE + INFERENCE | probe only |
| F13 | Publish checks | packed / Workshop addon must still open `Data/Mud/mud_heights.c` (binary .c, review N1); trench `DEBUG_INSTANT` off; pak junk cleanup | ledger | small |

## 4. Mega dive findings (2026-10-10, PvP only - AI out of scope)

| # | Finding | Evidence |
|---|---|---|
| N1 | Blast cover: our ground SHOULD block (terrain gamemats all StopExplosionTrace 1, ENTS traced). Bug cause still open -> in-game probe | IDA + PAK Stable, details in P3 |
| N2 | **Crater top ring is too steep to walk out of.** Raw profile (R 2.5, D 2, rim 0.3, 0.25 m verts): 46-50 deg at 1.5-2.0 m, 54 deg at 2.0-2.25 m, **70 deg** at 2.25-2.5 m (a 0.68 m jump from -0.38 to the +0.3 rim, the inside bowl ends at 0 while the rim starts at full height). Engine soldier limits: `MaxSlope` default **50.0** ("Max slope angle of surface that character can move against"), "Move max slope angle" default **60.0**; Character_Base.et does not override either | CODE `Crater` l.2635-2647, computed; IDA property registrations; PAK Stable. INFERENCE until tried: collider smoothing may soften it; StepHeight 0.25-1.0 per stance may step the lip |
| N3 | **Every crater is the same 2 m x 5 m pit.** 13 ammo prefabs fire `SCR_ExplosionAmmoEffect` (our only crater hook): 40 mm M433 / M406 / VOG25; rockets M72A3, RPG75, PG7VM, PG22, Hydra M247 / M229, S5KO / S5MO; mortars M821 / O832DU. Hand grenades: none. One UGL round = a mortar-size foxhole | PAK Stable (prefab text scan), CODE `OnExplosion` |
| N3b | Size classes for a table: warhead containers HE (40 mm), HEAT / HEDP (rockets), HEF (mortars, Hydra HE, S5 frag) | PAK Stable, warhead .et chains |
| N4 | **Clients build ahead of every vehicle they see** - enemy / far ones too. `Ahead` runs on proxies (EOnFrame l.467, no client gate); undug fine patches are identical to the skin, so on a client this only pre-pays a possible later message | CODE |
| N5 | Net per vehicle (Task 9 run): 1054 msgs / 112771 ints in 19 net lines (10 s each) = ~5.5 msgs/s, **~2.4 KB/s payload per client**. Linear guess: 10 off-road vehicles x 64 clients = ~1.5 MB/s (~12 Mbit/s) extra server upload, all on the Reliable channel | MEASURED (ledger l.377), projection INFERENCE |
| N6 | Patch budget only WARNS (l.565-568); deformed patches are kept for the whole match -> memory has no ceiling in long PvP sessions | CODE |
| N7 | JIP ask can be re-sent once the player's queue drains (`QueueJip` only refuses while queued) -> one client can make the server resend every deformed patch again and again | CODE l.3427-3462 |
| N8 | Nothing wakes resting physics objects when the ground drops (no SetActive in Mud scripts) -> wrecks / dropped gear may hang over a fresh crater | CODE; behaviour INFERENCE (look check) |
| N9 | Ragdoll limbs stuck in trench walls / slopes (user report 02:45, open) | user report |
| N10 | Dig / trench RPCs are fine for PvP: shovel in hand, rate gaps, range, plan size are checked on the server | CODE MUD_Dig.c l.438+, MUD_TrenchPlanner.c l.506+ |

## 5. Scored list (Ease 1 = an hour, 5 = days + risk; Gain 1-5 for a PvP server)

| Rank | Change | Ease | Gain | Why / first step |
|---|---|---|---|---|
| 1 | **Crater size by ammo** (backlog 4, N3): table by warhead family; 40 mm shallow (~0.3 m, r ~1 m), rockets ~0.5 m, mortars / Hydra / S5 ~1-1.2 m | 2 | 5 | PvP balance (no instant UGL foxholes) + fewer patches / net per shot. Read the projectile prefab in `OnExplosion` (damageSource) |
| 2 | **Crater lip fix** (N2): one continuous profile (bowl rises into the rim, max wall ~40-45 deg) | 1 | 4 | Players / vehicles not trapped. First: walk out of a default crater in-game (2 min) |
| 3 | **F1 debug off + N7 JIP cap per player + F13 publish checks** | 1 | 3 | Release hygiene, anti-grief |
| 4 | **Client ahead gate** (N4): proxies run Ahead only for vehicles within ~150 m of the camera (or only READY steps after O1) | 1-2 | 4 | Client fps in busy PvP; zero risk to the server |
| 5 | **O1 lazy patches** | 3 | 5 | Biggest measured waste (~70 % of builds), server + client |
| 6 | **P3 blast probe** (vanilla control + TEMP trace log), then the fix it points to | 2 probe | 5 | Cover in craters / trenches is the point of the feature in PvP |
| 7 | **F2 stress test** (5-10 vehicles, shelling, DS + 2 peers) | 2 | 5 | Every number is one vehicle today; decides 9-10 |
| 8 | **P1a ground changes behind the wheel** | 3 | 4 | Vehicle stutter left after the 15 cm allowance |
| 9 | **F3 healing / hard budget** (N6): shallow ruts fade, oldest undug-looking patches freed | 3-4 | 4 | Long matches; needed before public servers |
| 10 | **F4 send by distance** (N5): live rects via the PlayerController Owner RPC we already use for JIP; far players marked stale and ask on approach | 4 | 4 | Server upload at 32+ players; only if 7 shows it |
| 11 | F10 server settings + **no-crater / no-dig zones** (bases, spawns, HQ) | 2-3 | 3 | PvP griefing in mains |
| 12 | Picked A (dig TO a depth) | 2 | 3 | Also fewer rebuilds once a lane is done |
| 13 | Picked B (depth per ground type) | 2 | 3 | WORLD trace gives the gamemat (proven 2026-10-08) |
| 14 | O2 coarse plate + slab (+ bullet re-check) | 2-3 | 3 | ~2/3 of collider triangles |
| 15 | F9 grass in craters (look) | 1 look | 3 | Floating clutter would hide / show players wrongly |
| 16 | N8 wake objects after a crater (look first) | 2 | 2 | Visual |
| 17 | Picked C (side ridges), F6 wetness | 2 / 1 | 2 | Looks |
| 18 | P2a skirt | 3 | 1-2 | Minor crack, one crash already in this area |
| 19 | D type+pos net, F7 mud drag, F5 persistence | 4 / 4 / 3 | 2 / 2 / 1 | PvP matches reset - persistence matters least |
| 20 | N9 ragdoll limbs, F12 trimesh bumps | ? / probe | 1 | Watch only |
| - | F8 AI | - | - | Dropped (PvP only) |

**My pick:** 1 + 2 together (same function, an hour), then 3 + 4 (small, safe), then 5 (O1), then 6 and 7.
Each step: one TEMP stats line before / after, same drive route, whole table into results/.
